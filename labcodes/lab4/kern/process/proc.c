/**
 * @file   proc.c
 * @brief  Process/thread table, PID allocation, kernel thread creation and the idle/init threads.
 *
 * Lab4 turns the single boot flow into several kernel threads. Two of them exist at the end of
 * proc_init(): idleproc (the boot context itself, PID assigned by proc_init) and initproc (a
 * kernel thread running init_main). Functions implemented by the lab4 exercises are marked with
 * `//LAB4 function2` (creation, module 2) and `//LAB4 function3` (scheduling, module 3) below,
 * and each has a Doxygen contract block placed right after its marker.
 *
 * Key global state: proc_list (all PCBs, linked via list_link), hash_list (PCBs by PID, linked
 * via hash_link) and current (the PCB of the thread that owns the CPU).
 */
#include <proc.h>
#include <kmalloc.h>
#include <string.h>
#include <sync.h>
#include <pmm.h>
#include <error.h>
#include <sched.h>
#include <elf.h>
#include <vmm.h>
#include <trap.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* ------------- process/thread mechanism design&implementation -------------
(an simplified Linux process/thread mechanism )
introduction:
  ucore implements a simple process/thread mechanism. process contains the independent memory sapce, at least one threads
for execution, the kernel data(for management), processor state (for context switch), files(in lab6), etc. ucore needs to
manage all these details efficiently. In ucore, a thread is just a special kind of process(share process's memory).
------------------------------
process state       :     meaning               -- reason
    PROC_UNINIT     :   uninitialized           -- alloc_proc
    PROC_SLEEPING   :   sleeping                -- try_free_pages, do_wait, do_sleep
    PROC_RUNNABLE   :   runnable(maybe running) -- proc_init, wakeup_proc,
    PROC_ZOMBIE     :   almost dead             -- do_exit

-----------------------------
process state changing:

  alloc_proc                                 RUNNING
      +                                   +--<----<--+
      +                                   + proc_run +
      V                                   +-->---->--+
PROC_UNINIT -- proc_init/wakeup_proc --> PROC_RUNNABLE -- try_free_pages/do_wait/do_sleep --> PROC_SLEEPING --
                                           A      +                                                           +
                                           |      +--- do_exit --> PROC_ZOMBIE                                +
                                           +                                                                  +
                                           -----------------------wakeup_proc----------------------------------
-----------------------------
process relations
parent:           proc->parent  (proc is children)
children:         proc->cptr    (proc is parent)
older sibling:    proc->optr    (proc is younger sibling)
younger sibling:  proc->yptr    (proc is older sibling)
-----------------------------
related syscall for process:
SYS_exit        : process exit,                           -->do_exit
SYS_fork        : create child process, dup mm            -->do_fork-->wakeup_proc
SYS_wait        : wait process                            -->do_wait
SYS_exec        : after fork, process execute a program   -->load a program and refresh the mm
SYS_clone       : create child thread                     -->do_fork-->wakeup_proc
SYS_yield       : process flag itself need resecheduling, -- proc->need_sched=1, then scheduler will rescheule this process
SYS_sleep       : process sleep                           -->do_sleep
SYS_kill        : kill process                            -->do_kill-->proc->flags |= PF_EXITING
                                                                 -->wakeup_proc-->do_wait-->do_exit
SYS_getpid      : get the process's pid

*/

/**
 * @brief The list of all process control blocks (linked through proc_struct::list_link).
 * @note Every PCB that is alive must be in this list; schedule() scans it for PROC_RUNNABLE.
 */
list_entry_t proc_list;

/** @brief log2 of the number of hash buckets used by hash_list. */
#define HASH_SHIFT 10
/** @brief Number of buckets in hash_list (1024). */
#define HASH_LIST_SIZE (1 << HASH_SHIFT)
/** @brief Map a PID to its bucket index in hash_list. */
#define pid_hashfn(x) (hash32(x, HASH_SHIFT))

/**
 * @brief Hash buckets indexed by pid_hashfn(pid); each process is linked via proc_struct::hash_link.
 * @note Gives find_proc() O(1) average lookup by PID. Static: only this file touches it.
 */
static list_entry_t hash_list[HASH_LIST_SIZE];

/**
 * @brief The idle process (PID assigned by proc_init()); runs cpu_idle() when nothing else is runnable.
 * @note Created from the boot context, not by do_fork(), so it has no parent.
 */
struct proc_struct *idleproc = NULL;
/**
 * @brief The first real kernel thread, created by proc_init() to run init_main().
 */
struct proc_struct *initproc = NULL;
/**
 * @brief The PCB of the thread that currently owns the CPU.
 * @note Changed only by proc_run(), with interrupts disabled. Read by trap-time code such as
 *       forkret() and by do_fork() (as the parent).
 */
struct proc_struct *current = NULL;

/**
 * @brief Number of processes currently linked into proc_list (kept by the exercise code).
 */
static int nr_process = 0;

/**
 * @brief First instruction of every kernel thread created with kernel_thread() (see process/entry.S).
 * @note Not a C function; it is reached through the epc of the new thread's trap frame.
 */
void kernel_thread_entry(void);
/**
 * @brief Exit path of a new thread: restore the trap frame at @p tf and sret into it (trapentry.S).
 * @param tf trap frame placed on the new thread's kernel stack by copy_thread()
 * @note Never returns to its caller.
 */
void forkrets(struct trapframe *tf);
/**
 * @brief Save the callee-saved context of the running thread into @p from and load @p to (switch.S).
 * @param from context to save the current thread's registers into
 * @param to   context to restore the next thread's registers from
 * @note Called only by proc_run() with interrupts disabled. Returns in the context of whichever
 *       thread is resumed.
 */
void switch_to(struct context *from, struct context *to);

/**
 * @brief Copy a name into a process's name buffer, truncated to PROC_NAME_LEN characters.
 * @param proc process whose name to set
 * @param name source string; only PROC_NAME_LEN bytes are copied, so it need not be terminated
 * @return the destination buffer proc->name
 * @warning memcpy reads PROC_NAME_LEN bytes from @p name even if the string is shorter, so a
 *          short literal is read past its terminator. This matches the original design.
 */
char *set_proc_name(struct proc_struct *proc, const char *name) {
    memset(proc->name, 0, sizeof(proc->name));
    return memcpy(proc->name, name, PROC_NAME_LEN);
}

/**
 * @brief Return a copy of a process's name in a static buffer.
 * @param proc process whose name to read
 * @return pointer to a static, zero-terminated copy of the name
 * @note The buffer is shared by all callers and overwritten by every call; copy it if it is needed
 *       across another call. Not reentrant.
 */
char *get_proc_name(struct proc_struct *proc) {
    static char name[PROC_NAME_LEN + 1];
    memset(name, 0, sizeof(name));
    return memcpy(name, proc->name, PROC_NAME_LEN);
}

/**
 * @brief Allocate a PID that is not used by any live process.
 *
 * PIDs grow monotonically from 1 up to MAX_PID and wrap around; the function skips a PID as long
 * as proc_list contains a process with it. The two static variables remember the last PID handed
 * out and the lowest used PID above it, so that the list is only rescanned when needed.
 *
 * @return a PID in the range [1, MAX_PID)
 * @note Called from do_fork() (exercise). Should run with interrupts disabled, so that the scan
 *       of proc_list and the insertion of the new PCB are not interleaved with another creator.
 */
static int get_pid(void) {
    static_assert(MAX_PID > MAX_PROCESS);
    struct proc_struct *proc;
    list_entry_t *list = &proc_list, *le;
    static int next_safe = MAX_PID, last_pid = MAX_PID;
    if (++last_pid >= MAX_PID) {
        last_pid = 1;
        goto inside;
    }
    if (last_pid >= next_safe) {
    inside:
        next_safe = MAX_PID;
    repeat:
        le = list;
        while ((le = list_next(le)) != list) {
            proc = le2proc(le, list_link);
            if (proc->pid == last_pid) {
                if (++last_pid >= next_safe) {
                    if (last_pid >= MAX_PID) {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    goto repeat;
                }
            } else if (proc->pid > last_pid && next_safe > proc->pid) {
                next_safe = proc->pid;
            }
        }
    }
    return last_pid;
}


/**
 * @brief First C function run by a new thread; hands its trap frame to forkrets().
 *
 * switch_to() "returns" into this function for a thread that has never run, because copy_thread()
 * sets the context's ra to forkret. It then calls forkrets(current->tf), which never comes back:
 * the thread leaves through __trapret and sret, starting at the frame's epc.
 *
 * @note Runs with the new thread as current, on its own kernel stack, with interrupts still
 *       disabled by proc_run(); the sret of the trap frame restores the interrupt state.
 */
static void forkret(void) {
    forkrets(current->tf);
}

/**
 * @brief Link a process into hash_list under its PID.
 * @param proc process to add; its pid must already be assigned
 */
static void hash_proc(struct proc_struct *proc) {
    list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
}

/**
 * @brief Look up a process by PID.
 * @param pid PID to search for
 * @return the PCB with that PID, or NULL if pid is out of range (not in (0, MAX_PID)) or not found
 */
struct proc_struct *find_proc(int pid) {
    if (0 < pid && pid < MAX_PID) {
        list_entry_t *list = hash_list + pid_hashfn(pid), *le = list;
        while ((le = list_next(le)) != list) {
            struct proc_struct *proc = le2proc(le, hash_link);
            if (proc->pid == pid) {
                return proc;
            }
        }
    }
    return NULL;
}


/**
 * @brief Allocate the kernel stack of a process: KSTACKPAGE contiguous physical pages.
 * @param proc PCB that receives the stack; proc->kstack is set to the stack's kernel virtual address
 * @return 0 on success, -E_NO_MEM if no block of KSTACKPAGE pages is free
 * @note The stack is KSTACKSIZE bytes and is not managed by the MM; it is freed by put_kstack().
 *       Trap frames and the initial context of a new thread live at the top of this stack.
 */
static int setup_kstack(struct proc_struct *proc) {
    struct Page *page = alloc_pages(KSTACKPAGE);
    if (page != NULL) {
        proc->kstack = (uintptr_t)page2kva(page);
        return 0;
    }
    return -E_NO_MEM;
}

/**
 * @brief Free the kernel stack allocated by setup_kstack().
 * @param proc PCB whose proc->kstack was returned by setup_kstack()
 * @note Only valid for a stack that is no longer in use (the thread has exited or was never run).
 */
static void put_kstack(struct proc_struct *proc) {
    free_pages(kva2page((void *)(proc->kstack)), KSTACKPAGE);
}


/**
 * @brief Terminate the calling process (lab4 placeholder).
 *
 * In lab4 this only panics. The real implementation (free the address space, become a zombie,
 * wake the parent, reschedule) belongs to a later lab. Because kernel_thread_entry() calls
 * do_exit() after the thread's function returns, the panic is the expected end of a lab4 run
 * once init_main() has printed its message.
 *
 * @param error_code exit status of the process (unused in lab4)
 * @return never returns
 */
int do_exit(int error_code) {
    panic("process exit!!.\n");
}

/**
 * @brief Body of initproc: print a greeting and return.
 *
 * Runs as the first kernel thread created by kernel_thread(). Its argument is the string
 * passed to kernel_thread() by proc_init().
 *
 * @param arg a `const char *` message printed as "To U: ..."
 * @return 0; returning hands control to kernel_thread_entry(), which calls do_exit()
 */
static int init_main(void *arg) {
    cprintf("this initproc, pid = %d, name = \"%s\"\n", current->pid, get_proc_name(current));
    cprintf("To U: \"%s\".\n", (const char *)arg);
    cprintf("To U: \"en.., Bye, Bye. :)\"\n");
    return 0;
}

//LAB4 function2 2411622
/**
 * @fn static struct proc_struct *alloc_proc(void)
 * @brief Allocate and minimally initialise a process control block (exercise, module 2).
 *
 * Contract:
 *  - the PCB comes from the kernel heap; on failure return NULL and leak nothing;
 *  - every field must be in a defined "empty" state before anyone reads it: state PROC_UNINIT,
 *    pid -1 (no identity yet), runs 0, kstack 0, need_resched cleared, parent and mm NULL,
 *    tf NULL, context zeroed, flags 0, name empty;
 *  - pgdir is the physical address of the kernel's boot page directory (boot_pgdir_pa), because
 *    kernel threads run in the shared kernel address space;
 *  - list_link and hash_link are not linked into any list yet (do_fork links the PCB later).
 * @return the new PCB, or NULL if the allocation failed
 * @note The returned PCB must still be PROC_UNINIT: wakeup_proc() asserts on any other state.
 */
static struct proc_struct *alloc_proc(void) {
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc == NULL) {
        return NULL;
    }

    // Every field gets a defined empty value, so that no code can read an uninitialised member.
    proc->state = PROC_UNINIT;
    proc->pid = -1;
    proc->runs = 0;
    proc->kstack = 0;
    proc->need_resched = 0;
    proc->parent = NULL;
    proc->mm = NULL;
    memset(&proc->context, 0, sizeof(proc->context));
    proc->tf = NULL;
    // Kernel threads share the boot page table.
    proc->pgdir = boot_pgdir_pa;
    proc->flags = 0;
    memset(proc->name, 0, sizeof(proc->name));
    // Self-linked nodes mean "not in any list yet"; do_fork() links the PCB later.
    list_init(&proc->list_link);
    list_init(&proc->hash_link);
    return proc;
}

//LAB4 function2 2411622
/**
 * @fn void proc_init(void)
 * @brief Turn the boot context into idleproc and create initproc (exercise, module 2).
 *
 * Contract:
 *  - proc_list and hash_list are empty and usable before any PCB is linked;
 *  - idleproc: alloc_proc() a PCB, give it a valid PID, state PROC_RUNNABLE, name "idle",
 *    need_resched set (so cpu_idle() calls schedule() at once). Its kernel stack is the boot stack
 *    that the current execution already uses, so setup_kstack() must NOT be called for it.
 *    Set current = idleproc. idleproc is not queued in proc_list: schedule() starts scanning from
 *    the list head when it is current and falls back to it when nothing else is runnable;
 *  - initproc: create it with kernel_thread(init_main, <message string>, 0), record its PCB in
 *    initproc, and leave it RUNNABLE (it is not current yet);
 *  - on return current == idleproc and initproc != NULL.
 * @note Called once from kern_init(), before clock_init() and intr_enable(), so interrupts are off.
 * @warning Failing to create initproc should panic rather than leave initproc as NULL.
 */
void proc_init(void) {
    int i;
    // The process list and every hash bucket must be valid before the first PCB is linked.
    list_init(&proc_list);
    for (i = 0; i < HASH_LIST_SIZE; i++) {
        list_init(hash_list + i);
    }

    // idleproc is the boot execution flow itself: it keeps the boot stack and gets no do_fork().
    idleproc = alloc_proc();
    if (idleproc == NULL) {
        panic("cannot allocate idle process");
    }
    idleproc->pid = 0;
    idleproc->state = PROC_RUNNABLE;
    idleproc->kstack = (uintptr_t)bootstack;
    // Ask for a reschedule at once, so that cpu_idle() hands the CPU to initproc.
    idleproc->need_resched = 1;
    set_proc_name(idleproc, "idle");
    current = idleproc;
    nr_process++;

    // The first real kernel thread; do_fork() makes it PROC_RUNNABLE with parent == idleproc.
    int pid = kernel_thread(init_main, (void *)"Hello world!!", 0);
    if (pid < 0) {
        panic("cannot create init process");
    }
    initproc = find_proc(pid);
    if (initproc == NULL) {
        panic("init process is missing");
    }
    set_proc_name(initproc, "init");
}

//LAB4 function2 2411622
/**
 * @fn int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags)
 * @brief Create a kernel thread that will run fn(arg) (exercise, module 2).
 *
 * Contract:
 *  - build a temporary trap frame on the caller's stack, zeroed first. Its s0 holds fn and its s1
 *    holds arg, which is the convention read by kernel_thread_entry() in process/entry.S;
 *  - the frame's epc is kernel_thread_entry, and its status must make the sret return to S mode
 *    with interrupts enabled afterwards: set SPP and SPIE, clear SIE, starting from the current sstatus;
 *  - create the thread with do_fork(clone_flags | CLONE_VM, 0, <frame>). A kernel thread shares
 *    the kernel address space (CLONE_VM) and has no user stack (stack argument 0).
 * @param fn          function executed by the new thread
 * @param arg         argument passed to fn
 * @param clone_flags extra clone flags, OR-ed with CLONE_VM
 * @return the PID of the new thread, or a negative error code from do_fork()
 */
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags) {
    // Temporary trap frame on this stack: do_fork() copies it into the child's kernel stack.
    struct trapframe tf;
    memset(&tf, 0, sizeof(tf));
    // kernel_thread_entry() calls s0(s1): the function and its argument travel in s0/s1.
    tf.gpr.s0 = (uintptr_t)fn;
    tf.gpr.s1 = (uintptr_t)arg;
    tf.epc = (uintptr_t)kernel_thread_entry;
    // sret returns to S mode (SPP = 1) and sets SIE from SPIE (= 1); SIE stays 0 until then.
    tf.status = (read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) & ~SSTATUS_SIE;
    return do_fork(clone_flags | CLONE_VM, 0, &tf);
}

static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf);
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);

//LAB4 function2 2411622
/**
 * @fn int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)
 * @brief Create a child process or thread that copies the current one (exercise, module 2).
 *
 * Contract, in this order. Each failing step must release what the earlier steps obtained:
 *  1. alloc_proc(): new PCB, still PROC_UNINIT;
 *  2. setup_kstack(): kernel stack for the child;
 *  3. copy_mm(clone_flags, child): address space (a no-op for kernel threads in lab4);
 *  4. copy_thread(child, stack, tf): trap frame and initial context on the child's stack;
 *  5. assign a PID with get_pid(), set parent = current, and link the child into proc_list and
 *     hash_list (nr_process is updated here as well);
 *  6. wakeup_proc(child): the child becomes PROC_RUNNABLE. This is the only step that makes it runnable;
 *  7. return the child's PID.
 * Steps 5 and 6 are the critical section: interrupts must be disabled around them.
 * @param clone_flags CLONE_* flags; CLONE_VM means the child shares the parent's address space
 * @param stack       stack pointer for the child, or 0 for a kernel thread
 * @param tf          trap frame to copy into the child
 * @return the child's PID on success, or a negative error code (e.g. -E_NO_MEM)
 */
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf) {
    struct proc_struct *proc;
    int ret;

    // 0. Reject a missing template frame or a call before proc_init() has set current.
    if (tf == NULL || current == NULL) {
        return -E_INVAL;
    }
    // 1. PCB. Nothing has been acquired yet, so a failure needs no cleanup.
    if ((proc = alloc_proc()) == NULL) {
        return -E_NO_MEM;
    }
    // 2. Kernel stack. On failure only the PCB has to be released.
    if ((ret = setup_kstack(proc)) != 0) {
        kfree(proc);
        return ret;
    }
    // 3. Address space. On failure release the stack and the PCB, in reverse order of acquisition.
    if ((ret = copy_mm(clone_flags, proc)) != 0) {
        put_kstack(proc);
        kfree(proc);
        return ret;
    }

    // 4. Trap frame and initial context on the child's stack (cannot fail).
    copy_thread(proc, stack, tf);

    // 5.-6. Critical section: the PID scan, the list insertions and the wakeup must not be
    //       interleaved with an interrupt, and the child becomes visible to schedule() only at the end.
    bool intr_flag;
    local_intr_save(intr_flag);
    proc->pid = get_pid();
    proc->parent = current;
    list_add(&proc_list, &proc->list_link);
    hash_proc(proc);
    nr_process++;
    wakeup_proc(proc);
    local_intr_restore(intr_flag);
    // 7. The parent sees the child's PID; the child sees a0 == 0 (set in copy_thread()).
    return proc->pid;
}

//LAB4 function2 2411622
/**
 * @fn static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf)
 * @brief Place the trap frame and the initial context of a new thread on its kernel stack (exercise).
 *
 * Contract:
 *  - reserve room for a struct trapframe at the top of proc->kstack, and make proc->tf point to it;
 *  - copy *tf into it; the child's return value (a0) must be 0 so the child sees fork() == 0;
 *  - if esp is not 0 it becomes the child's sp; for a kernel thread (esp 0) the sp must point at
 *    the frame itself;
 *  - the initial context has ra = forkret, so the first switch_to() into this thread lands in
 *    forkret(), which calls forkrets(proc->tf); its sp is the frame address;
 *  - the other context registers may start at zero.
 * @param proc PCB whose kernel stack has been set up
 * @param esp  stack pointer for the child, or 0 for a kernel thread
 * @param tf   trap frame to use as the template
 */
static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf) {
    uintptr_t stack_top = proc->kstack + KSTACKSIZE;
    struct trapframe *frame = (struct trapframe *)(stack_top - sizeof(struct trapframe));

    // The trap frame sits at the very top of the kernel stack, where __trapret will pop it from.
    memcpy(frame, tf, sizeof(struct trapframe));
    // The child's return value is 0.
    frame->gpr.a0 = 0;
    if (esp != 0) {
        frame->gpr.sp = esp;
    } else {
        frame->gpr.sp = (uintptr_t)frame;
    }
    proc->tf = frame;
    // First switch_to() into this thread: ret goes to forkret with sp at the trap frame, then
    // forkrets -> __trapret -> sret -> kernel_thread_entry.
    memset(&proc->context, 0, sizeof(proc->context));
    proc->context.ra = (uintptr_t)forkret;
    proc->context.sp = (uintptr_t)frame;
}

//LAB4 function2 2411622
/**
 * @fn static int copy_mm(uint32_t clone_flags, struct proc_struct *proc)
 * @brief Give the child an address space: share the parent's or duplicate it (exercise, module 2).
 *
 * In lab4 all threads are kernel threads with current->mm == NULL, so there is nothing to copy.
 * The signature and the return convention are kept so later labs can extend the function.
 * Contract: if the parent has no mm, leave child->mm NULL and return 0; otherwise, with CLONE_VM,
 * share the parent's mm, and without it duplicate it (not needed in lab4).
 * @param clone_flags CLONE_* flags passed by do_fork()
 * @param proc        the child PCB
 * @return 0 on success, or a negative error code
 */
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc) {
    // Kernel threads have no user address space: nothing to share or copy.
    if (current->mm == NULL) {
        proc->mm = NULL;
        return 0;
    }
    // CLONE_VM: share the parent's mm. Duplicating it (fork) is left to a later lab.
    if (clone_flags & CLONE_VM) {
        proc->mm = current->mm;
        return 0;
    }
    return -E_NO_MEM;
}

//LAB4 function3 2414007
/**
 * @fn void cpu_idle(void)
 * @brief Body of idleproc: run schedule() whenever a reschedule has been requested (exercise, module 3).
 *
 * Contract: an endless loop that never returns (the prototype is noreturn). Each pass checks
 * current->need_resched and calls schedule() when it is set. The idle thread must not sleep or
 * block on its own. It starts after kern_init() has enabled interrupts.
 */
void cpu_idle(void) {
    while (1) {
        // need_resched is volatile, so it is re-read on every pass. It is set by proc_init() for the
        // first schedule, and by code that wants the CPU back; schedule() clears it again.
        if (current->need_resched) {
            schedule();
        }
    }
}

//LAB4 function3 2414007

/**
 * @fn void proc_run(struct proc_struct *proc)
 * @brief Make @p proc the running process: switch the page table and the register context.
 *
 * Contract:
 *  - nothing to do if proc is already current;
 *  - disable interrupts for the whole operation (local_intr_save / local_intr_restore), so a timer
 *    interrupt cannot run the scheduler in the middle of the switch;
 *  - set current = proc, then load proc->pgdir into satp (Sv39 mode) and flush the TLB. All lab4
 *    threads share the kernel page table (pgdir is boot_pgdir_pa for every PCB), so the value
 *    written is the one already in use; the write keeps the function correct for later labs;
 *  - call switch_to() with the previous thread's context and the new thread's context.
 * @param proc the process to run; it must be PROC_RUNNABLE
 * @note The call returns only when the calling thread is switched in again. A brand-new thread
 *       never returns here: it starts in forkret and leaves through sret, which restores the
 *       interrupt state from its frame. The local_intr_restore after switch_to() therefore runs only
 *       in threads that were switched out inside proc_run().
 */
void proc_run(struct proc_struct *proc) {
    if (proc == current) {
        return;
    }

    bool intr_flag;
    struct proc_struct *prev = current;

    local_intr_save(intr_flag);
    {
        // 1. current must change before the stack is switched, so that anything running in the
        //    new thread already sees itself as current.
        current = proc;
        // 2. Load the new page table. Every lab4 thread uses boot_pgdir_pa, so the value is the
        //    same today; the code is kept so that processes with their own pgdir work later.
        //    satp = MODE(Sv39) | PPN, written as in enable_paging(); sfence.vma drops stale entries.
        write_csr(satp, ((uintptr_t)SATP_MODE_SV39 << 60) | (proc->pgdir >> RISCV_PGSHIFT));
        flush_tlb();
        // 3. Switch the register context. This call returns only when prev is scheduled in again.
        switch_to(&(prev->context), &(proc->context));
    }
    local_intr_restore(intr_flag);
}
