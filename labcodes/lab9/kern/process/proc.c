/**
 * @file   proc.c
 * @brief  Process/thread table, PID allocation, kernel thread creation, the idle/init threads, the
 *         lab6 scheduling fields of the PCB, the lab7 sleep call and the lab8 file table and
 *         file-system based exec.
 *
 * lab4 turned the single boot flow into several kernel threads and lab5 added the user-process
 * life cycle: fork (do_fork, copy_mm), exec (do_execve, load_icode, kernel_execve), exit, wait,
 * yield and kill. lab6 keeps all of that and only adds the scheduling state of a process (run
 * queue, time slice, Stride fields): alloc_proc() must initialise it (marker
 * `//lab6 function1-1`) and lab6_set_priority() lets a user process change its priority. Code
 * carried over from lab4/lab5 keeps its original markers (`//LAB4 ...`, `//lab5 ...`). A marker
 * with "update" means: the code below it is the baseline that has to be extended. Contracts are
 * written next to the markers. lab7 adds do_sleep(), the kernel side of the sleep system call (it is
 * built on the timers of sched.c) and lets init_main() run check_sync() from kern/sync/check_sync.c
 * (lab8 drops that call again, see init_main()).
 * lab8 gives every process a file table (proc_struct::filesp): alloc_proc() must start it as NULL
 * (marker `//lab8 function1-1`), proc_run() must switch the page table (marker `//lab8 function1-2`)
 * and do_fork() must copy the table with copy_files() (marker `//lab8 function1-3`). exec now loads
 * the program from the file system: do_execve() opens the ELF file and load_icode() (marker
 * `//lab8 function3`) reads it through the descriptor with load_icode_read().
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
#include <unistd.h>
#include <fs.h>
#include <vfs.h>
#include <sysfile.h>

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
 * @note Every PCB that is alive must be in this list. Since lab6 it is no longer used to find the
 *       next process: schedule() asks the scheduling class, which keeps its own run queue.
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
 * @note In lab5 it is also the process that adopts the children of exited processes (do_exit).
 */
struct proc_struct *initproc = NULL;
/**
 * @brief The PCB of the thread that currently owns the CPU.
 * @note Changed only by proc_run(), with interrupts disabled. Read by trap-time code such as
 *       forkret() and by do_fork() (as the parent).
 */
struct proc_struct *current = NULL;

/**
 * @brief Number of processes currently linked into proc_list (kept by set_links/remove_links).
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

//lab5 function2 2414007 update alloc_proc
//LAB4 function2 2411622
/**
 * @fn static struct proc_struct *alloc_proc(void)
 * @brief Allocate and minimally initialise a process control block (exercise, module 2; lab8 adds
 *        the file table pointer).
 *
 * Contract:
 *  - the PCB comes from the kernel heap; on failure return NULL and leak nothing;
 *  - every field must be in a defined "empty" state before anyone reads it: state PROC_UNINIT,
 *    pid -1 (no identity yet), runs 0, kstack 0, need_resched cleared, parent and mm NULL,
 *    tf NULL, context zeroed, flags 0, name empty;
 *  - pgdir is the physical address of the kernel's boot page directory (boot_pgdir_pa), because
 *    kernel threads run in the shared kernel address space;
 *  - list_link and hash_link are not linked into any list yet (do_fork links the PCB later);
 *  - (lab5) the fields added for user processes also start empty: exit_code 0, wait_state 0, and
 *    no relatives (cptr, yptr, optr NULL). kmalloc() does not zero the memory, so reading them
 *    before this initialisation, as set_links() and do_wait() do, would be a bug;
 *  - (lab6) the scheduling fields start in a defined, not-yet-queued state: rq NULL, run_link a
 *    self-linked node, time_slice 0, lab6_run_pool an initialised skew-heap node, lab6_stride 0
 *    and lab6_priority a valid starting value. The PCB must not be put into any run queue here;
 *    that is done by the scheduler class when the process becomes runnable;
 *  - (lab8) filesp starts as NULL: the new process owns no file table yet. The table is created or
 *    shared later by copy_files() in do_fork() (or by proc_init() for the idle process), never here.
 *    Because kmalloc() does not clear memory, leaving filesp uninitialised would let copy_files(),
 *    files_closeall() or put_files() work on a garbage pointer.
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
    // lab5: the exit/wait state and the process tree links start out empty as well.
    proc->exit_code = 0;
    proc->wait_state = 0;
    proc->cptr = proc->yptr = proc->optr = NULL;
    //lab6 function1-1 2414015 update alloc_proc
    proc->rq = NULL;
    list_init(&proc->run_link);
    proc->time_slice = 0;
    skew_heap_init(&proc->lab6_run_pool);
    proc->lab6_stride = 0;
    proc->lab6_priority = 1;
    //lab8 function1-1 2414007 update alloc_proc
    proc->filesp = NULL;
    return proc;
}

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
 * @brief Insert a new process into the process tree and the global list (provided by the lab framework).
 *
 * Adds proc to proc_list, makes it the youngest child of proc->parent (cptr/optr/yptr links) and
 * increments nr_process. proc->parent must already be set.
 * @param proc the new process
 * @note Call with interrupts disabled, together with the PID assignment and hash_proc().
 */
static void set_links(struct proc_struct *proc) {
    list_add(&proc_list, &(proc->list_link));
    proc->yptr = NULL;
    if ((proc->optr = proc->parent->cptr) != NULL) {
        proc->optr->yptr = proc;
    }
    proc->parent->cptr = proc;
    nr_process++;
}

/**
 * @brief Remove a process from the process tree and the global list (provided by the lab framework).
 *
 * Unlinks proc from proc_list and from its parent's child list (the inverse of set_links()) and
 * decrements nr_process.
 * @param proc the process to remove; its parent pointer must still be valid
 * @note Called by do_wait() when it reclaims a zombie child.
 */
static void remove_links(struct proc_struct *proc) {
    list_del(&(proc->list_link));
    if (proc->optr != NULL) {
        proc->optr->yptr = proc->yptr;
    }
    if (proc->yptr != NULL) {
        proc->yptr->optr = proc->optr;
    } else {
        proc->parent->cptr = proc->optr;
    }
    nr_process--;
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

//LAB4 function3 2414007
//lab8 function1-2 2414007 update proc_run
/**
 * @fn void proc_run(struct proc_struct *proc)
 * @brief Make @p proc the running process: switch the page table and the register context.
 *
 * Contract:
 *  - nothing to do if proc is already current;
 *  - disable interrupts for the whole operation (local_intr_save / local_intr_restore), so a timer
 *    interrupt cannot run the scheduler in the middle of the switch;
 *  - set current = proc, then load proc->pgdir into satp with lsatp() (Sv39 mode) and flush the
 *    TLB. Kernel threads have pgdir == boot_pgdir_pa (the value already in use). (lab8) A user
 *    process has the page directory that copy_mm() (fork) or load_icode() (exec) stored in
 *    proc->pgdir, so this write is what makes the process run in its own address space; without
 *    it a process created by load_icode() would keep executing on the previous page table;
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
        // 2. Load the new page table. A kernel thread uses boot_pgdir_pa (no change); a user
        //    process has its own pgdir. lsatp() writes MODE(Sv39) | PPN into satp; sfence.vma
        //    then drops the translations of the previous address space.
        lsatp(proc->pgdir);
        flush_tlb();
        // 3. Switch the register context. This call returns only when prev is scheduled in again.
        switch_to(&(prev->context), &(proc->context));
    }
    local_intr_restore(intr_flag);
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
 * @brief Remove a process from the PID hash table (provided by the lab framework).
 * @param proc process to remove
 * @note The inverse of hash_proc(); called by do_wait() when it reclaims a zombie child.
 */
static void unhash_proc(struct proc_struct *proc) {
    list_del(&(proc->hash_link));
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

//lab5 function2 2414007 update kernel_thread
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
 *
 * lab5 check: the same frame is returned through the extended RESTORE_ALL. SPP = 1 makes it take
 * the kernel branch (sscratch stays 0, sp is the frame), SPIE = 1 / SIE = 0 give interrupts-on
 * after sret. init_main() also uses this function to create user_main.
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
 * @brief Allocate the page directory of a new address space (provided by the lab framework).
 *
 * Allocates one page and copies the kernel part of the boot page table into it, so that the
 * kernel stays mapped in every address space (boot_pgdir_va is the template). The user part starts
 * out empty.
 * @param mm address space whose pgdir is set
 * @return 0 on success, -E_NO_MEM if no page is free
 */
static int setup_pgdir(struct mm_struct *mm) {
    struct Page *page;
    if ((page = alloc_page()) == NULL) {
        return -E_NO_MEM;
    }
    pde_t *pgdir = page2kva(page);
    memcpy(pgdir, boot_pgdir_va, PGSIZE);

    mm->pgdir = pgdir;
    return 0;
}

/**
 * @brief Free the page directory of an address space (provided by the lab framework).
 * @param mm address space whose pgdir page is freed; the pages below it are not touched
 */
static void put_pgdir(struct mm_struct *mm) {
    free_page(kva2page(mm->pgdir));
}

static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf);
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);

//lab5 function3 2414007 update copy_mm
//LAB4 function2 2411622
/**
 * @fn static int copy_mm(uint32_t clone_flags, struct proc_struct *proc)
 * @brief Give the child an address space: share the parent's or duplicate it (lab5 version).
 *
 * Contract:
 *  - the parent has no mm (a kernel thread): the child gets none either (proc->mm stays NULL and
 *    proc->pgdir stays the boot page table) and the call succeeds;
 *  - CLONE_VM: the child shares the parent's mm_struct and page table;
 *  - otherwise (plain fork): create a new mm_struct and page directory and copy the parent's
 *    VMAs and pages into it with dup_mmap(), holding the parent's mm lock while the copy runs;
 *  - in both cases the mm's reference count is incremented, proc->mm is set, and proc->pgdir is set
 *    to the physical address of the mm's page directory (that is what proc_run() loads into satp);
 *  - on failure, what was created here is released again (page directory, mm, copied pages).
 * @param clone_flags CLONE_* flags passed by do_fork()
 * @param proc        the child PCB
 * @return 0 on success, -E_NO_MEM if memory runs out
 */
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc) {
    struct mm_struct *mm, *oldmm = current->mm;

    // Kernel threads have no user address space: nothing to share or copy.
    if (oldmm == NULL) {
        proc->mm = NULL;
        return 0;
    }

    int ret = -E_NO_MEM;
    if (clone_flags & CLONE_VM) {
        // Thread-like creation: both processes use the same address space.
        mm = oldmm;
        goto good_mm;
    }

    // Plain fork: build a private copy of the address space.
    if ((mm = mm_create()) == NULL) {
        goto bad_mm;
    }
    if (setup_pgdir(mm) != 0) {
        goto bad_pgdir_cleanup_mm;
    }
    lock_mm(oldmm);
    { ret = dup_mmap(mm, oldmm); }
    unlock_mm(oldmm);
    if (ret != 0) {
        goto bad_dup_cleanup_mmap;
    }

good_mm:
    mm_count_inc(mm);
    proc->mm = mm;
    proc->pgdir = PADDR(mm->pgdir);
    return 0;

bad_dup_cleanup_mmap:
    // mm_count is still 0 here, as exit_mmap() requires.
    exit_mmap(mm);
    put_pgdir(mm);
bad_pgdir_cleanup_mm:
    mm_destroy(mm);
bad_mm:
    return ret;
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

/**
 * @brief Give the new process a file table: share the parent's or copy it (lab8, used by do_fork()).
 *
 * With CLONE_FS the child shares the very same files_struct as the parent. Otherwise a new table is
 * created with files_create() and filled by dup_files(), which duplicates the current directory and
 * every open file. In both cases the reference count of the table is incremented and the table is
 * stored in proc->filesp.
 *
 * @param clone_flags CLONE_* flags passed by do_fork()
 * @param proc        the child PCB; proc->filesp is set on success
 * @return 0 on success, -E_NO_MEM if the new table cannot be created
 * @note The parent (current) must already own a table. A failure after files_create() destroys the
 *       new table again, so the caller has nothing to release for it.
 */
static int copy_files(uint32_t clone_flags, struct proc_struct *proc) {
    struct files_struct *filesp, *old_filesp = current->filesp;
    assert(old_filesp != NULL);

    if (clone_flags & CLONE_FS) {
        filesp = old_filesp;
        goto good_files_struct;
    }

    int ret = -E_NO_MEM;
    if ((filesp = files_create()) == NULL) {
        goto bad_files_struct;
    }

    if ((ret = dup_files(filesp, old_filesp)) != 0) {
        goto bad_dup_cleanup_fs;
    }

good_files_struct:
    files_count_inc(filesp);
    proc->filesp = filesp;
    return 0;

bad_dup_cleanup_fs:
    files_destroy(filesp);
bad_files_struct:
    return ret;
}

/**
 * @brief Drop the reference of a process to its file table; destroy the table with the last one (lab8).
 *
 * Used by do_fork() on its error path and by do_exit(). A process without a table is ignored.
 * @param proc PCB whose proc->filesp is released
 * @note files_destroy() closes the files that are still open and frees the table.
 */
static void put_files(struct proc_struct *proc) {
    struct files_struct *filesp = proc->filesp;
    if (filesp != NULL) {
        if (files_count_dec(filesp) == 0) {
            files_destroy(filesp);
        }
    }
}

/**
 * @brief Read len bytes at file offset @p offset of the open file @p fd into buf (lab8, used by load_icode()).
 *
 * Seeks to @p offset with sysfile_seek() and then reads with sysfile_read().
 * @param fd     descriptor of the ELF file
 * @param buf    kernel buffer that receives the data
 * @param len    number of bytes to read
 * @param offset position in the file
 * @return 0 if exactly len bytes were read; the negative error code of the seek or read otherwise,
 *         or -1 after a short read
 */
static int load_icode_read(int fd, void *buf, size_t len, off_t offset) {
    int ret;
    if ((ret = sysfile_seek(fd, offset, LSEEK_SET)) != 0) {
        return ret;
    }
    if ((ret = sysfile_read(fd, buf, len)) != len) {
        return (ret < 0) ? ret : -1;
    }
    return 0;
}

//lab8 function3 2414007 update load_icode
/**
 * @fn static int load_icode(int fd, int argc, char **kargv)
 * @brief Build a new user address space for the current process from an ELF file that is open as
 *        @p fd, push the program arguments and prepare the trap frame that starts the program.
 *
 * Precondition: current->mm is NULL (do_execve() released the old one) and current->tf points to
 * the trap frame at the top of the kernel stack. @p fd is open for reading and its file position is
 * not assumed: every read goes through load_icode_read(), which seeks first.
 *
 * Steps:
 *  1. create an mm_struct and a page directory (setup_pgdir() copies the kernel mappings);
 *  2. read the ELF header and check the magic number;
 *  3. for every PT_LOAD program header (read from the file at e_phoff + i * sizeof(proghdr)):
 *     create the VMA with the permissions of the segment (mm_map()), then walk the pages from
 *     ROUNDDOWN(p_va) up to p_va + p_memsz. Every page is allocated with pgdir_alloc_page() and
 *     cleared first; only the bytes that lie inside [p_va, p_va + p_filesz) are then read from the
 *     file into it, so the first page honours the offset of an unaligned p_va, the last file page
 *     is cut at p_filesz, and everything from p_filesz to p_memsz (the bss) stays zero. A
 *     segment with p_filesz > p_memsz, or whose range wraps around, is rejected;
 *  4. the descriptor is closed (everything needed is in memory now);
 *  5. create the user stack VMA of USTACKSIZE bytes below USTACKTOP (VM_STACK, readable and
 *     writable). The arguments are laid out from USTACKTOP downwards: the argument strings, then
 *     the NULL-terminated pointer array uargv[], then argc, with sp 16-byte aligned and pointing at
 *     argc (uargv = sp + 8, the usual process-entry layout). The pages that hold this block, and at
 *     least 4 pages in total, are allocated and cleared now. The rest of the stack VMA stays
 *     unmapped. Before lab9 nothing could add its pages on demand; since lab9 do_pgfault() maps a
 *     page of the stack VMA when it is first touched. fork() only has to copy the pages that are
 *     really mapped;
 *  6. make the new mm current: reference count 1, current->mm, current->pgdir, satp and a TLB
 *     flush. Only then are the arguments written, through the user addresses of the new space
 *     (sstatus.SUM is set by the kernel, so S mode may access the U pages);
 *  7. rewrite the trap frame so that sret enters user mode: epc = ELF entry point, sp = the stack
 *     pointer from step 5, a0 = argc and a1 = uargv (the arguments of umain()), SPP cleared
 *     (return to U mode) and SPIE set (interrupts on after sret); all other registers are 0.
 * @param fd    descriptor of the opened ELF file
 * @param argc  number of arguments, at least 1
 * @param kargv the argc arguments in kernel memory (kernel copies made by do_execve())
 * @return 0 on success; -E_NO_MEM if memory runs out, -E_INVAL_ELF for a bad magic number or an
 *         inconsistent segment, -E_INVAL if the arguments do not fit on the stack, or the error of
 *         a failed file read. On failure everything built here is released again and current->mm is
 *         still NULL, so the caller can exit.
 */
static int load_icode(int fd, int argc, char **kargv) {
    if (current->mm != NULL) {
        panic("load_icode: current->mm must be NULL.\n");
    }

    int ret = -E_NO_MEM;
    struct mm_struct *mm;
    // (1) a new, empty address space with a page directory that already maps the kernel
    if ((mm = mm_create()) == NULL) {
        goto bad_mm;
    }
    if (setup_pgdir(mm) != 0) {
        goto bad_pgdir_cleanup_mm;
    }

    // (2) the ELF header, read through the file descriptor
    struct elfhdr __elf, *elf = &__elf;
    if ((ret = load_icode_read(fd, elf, sizeof(struct elfhdr), 0)) != 0) {
        goto bad_elf_cleanup_pgdir;
    }
    if (elf->e_magic != ELF_MAGIC) {
        ret = -E_INVAL_ELF;
        goto bad_elf_cleanup_pgdir;
    }

    // (3) the program headers: map and fill every PT_LOAD segment
    uint32_t vm_flags, perm;
    for (int i = 0; i < elf->e_phnum; i++) {
        struct proghdr __ph, *ph = &__ph;
        off_t phoff = elf->e_phoff + (off_t)sizeof(struct proghdr) * i;
        if ((ret = load_icode_read(fd, ph, sizeof(struct proghdr), phoff)) != 0) {
            goto bad_cleanup_mmap;
        }
        if (ph->p_type != ELF_PT_LOAD) {
            continue;
        }
        if (ph->p_filesz > ph->p_memsz || ph->p_va + ph->p_memsz < ph->p_va) {
            ret = -E_INVAL_ELF;
            goto bad_cleanup_mmap;
        }
        if (ph->p_memsz == 0) {
            continue;
        }

        // Segment flags -> VMA flags and PTE permission bits (RISC-V: W needs R as well).
        vm_flags = 0;
        perm = PTE_U | PTE_V;
        if (ph->p_flags & ELF_PF_X) {
            vm_flags |= VM_EXEC;
        }
        if (ph->p_flags & ELF_PF_W) {
            vm_flags |= VM_WRITE;
        }
        if (ph->p_flags & ELF_PF_R) {
            vm_flags |= VM_READ;
        }
        if (vm_flags & VM_READ) {
            perm |= PTE_R;
        }
        if (vm_flags & VM_WRITE) {
            perm |= (PTE_W | PTE_R);
        }
        if (vm_flags & VM_EXEC) {
            perm |= PTE_X;
        }
        if ((ret = mm_map(mm, ph->p_va, ph->p_memsz, vm_flags, NULL)) != 0) {
            goto bad_cleanup_mmap;
        }

        // Fill the pages one by one. [file_start, file_end) is the part that exists in the file.
        uintptr_t file_start = ph->p_va, file_end = ph->p_va + ph->p_filesz;
        uintptr_t mem_end = ph->p_va + ph->p_memsz;
        off_t file_off = ph->p_offset;
        for (uintptr_t la = ROUNDDOWN(file_start, PGSIZE); la < mem_end; la += PGSIZE) {
            struct Page *page;
            if ((page = pgdir_alloc_page(mm->pgdir, la, perm)) == NULL) {
                ret = -E_NO_MEM;
                goto bad_cleanup_mmap;
            }
            // alloc_page() does not clear the page: zeroing it gives the bss, the bytes before an
            // unaligned p_va and the tail of the last page the right value in one step.
            void *kva = page2kva(page);
            memset(kva, 0, PGSIZE);

            uintptr_t lo = (la > file_start) ? la : file_start;
            uintptr_t hi = (la + PGSIZE < file_end) ? la + PGSIZE : file_end;
            if (lo < hi) {
                if ((ret = load_icode_read(fd, kva + (lo - la), hi - lo, file_off)) != 0) {
                    goto bad_cleanup_mmap;
                }
                file_off += hi - lo;
            }
        }
    }
    // (4) the program is in memory; the descriptor is not needed any more
    sysfile_close(fd);

    // (5) the user stack. The VMA covers USTACKSIZE bytes; only the top pages are backed (no page-fault
    //     handler exists to add the others later).
    vm_flags = VM_READ | VM_WRITE | VM_STACK;
    if ((ret = mm_map(mm, USTACKTOP - USTACKSIZE, USTACKSIZE, vm_flags, NULL)) != 0) {
        goto bad_cleanup_mmap;
    }

    // Layout, from USTACKTOP down: the strings, uargv[0..argc] (NULL at the end), argc. sp is
    // 16-byte aligned (RISC-V ABI) and uargv = sp + 8, so that argc, argv and the strings follow
    // each other as a program entry expects them.
    size_t str_size = 0;
    for (int i = 0; i < argc; i++) {
        str_size += strnlen(kargv[i], EXEC_MAX_ARG_LEN + 1) + 1;
    }
    uintptr_t str_base = ROUNDDOWN(USTACKTOP - str_size, 16);
    uintptr_t usp = ROUNDDOWN(str_base - (argc + 2) * sizeof(uintptr_t), 16);
    char **uargv = (char **)(usp + sizeof(uintptr_t));
    // The lowest page of a stack VMA is a guard page and must stay unused.
    if (usp < USTACKTOP - USTACKSIZE + PGSIZE) {
        ret = -E_INVAL;
        goto bad_cleanup_mmap;
    }

    size_t stack_pages = ROUNDUP(USTACKTOP - usp, PGSIZE) / PGSIZE;
    if (stack_pages < 4) {
        stack_pages = 4;
    }
    for (size_t i = 1; i <= stack_pages; i++) {
        struct Page *page;
        if ((page = pgdir_alloc_page(mm->pgdir, USTACKTOP - i * PGSIZE, PTE_USER)) == NULL) {
            ret = -E_NO_MEM;
            goto bad_cleanup_mmap;
        }
        memset(page2kva(page), 0, PGSIZE);
    }

    // (6) the new address space becomes the one of the current process. Nothing below can fail.
    mm_count_inc(mm);
    current->mm = mm;
    current->pgdir = PADDR(mm->pgdir);
    lsatp(PADDR(mm->pgdir));
    flush_tlb();

    // Copy the arguments into the new stack through its user addresses.
    uintptr_t str_ptr = str_base;
    for (int i = 0; i < argc; i++) {
        size_t len = strnlen(kargv[i], EXEC_MAX_ARG_LEN + 1);
        memcpy((void *)str_ptr, kargv[i], len);
        ((char *)str_ptr)[len] = '\0';
        uargv[i] = (char *)str_ptr;
        str_ptr += len + 1;
    }
    uargv[argc] = NULL;
    *(uintptr_t *)usp = argc;

    // (7) the trap frame that makes sret land at the program entry in user mode
    struct trapframe *tf = current->tf;
    uintptr_t sstatus = tf->status;
    memset(tf, 0, sizeof(struct trapframe));
    tf->gpr.sp = usp;
    tf->gpr.a0 = argc;
    tf->gpr.a1 = (uintptr_t)uargv;
    tf->epc = elf->e_entry;
    tf->status = (sstatus | SSTATUS_SPIE) & ~SSTATUS_SPP;
    return 0;

bad_cleanup_mmap:
    exit_mmap(mm);
bad_elf_cleanup_pgdir:
    put_pgdir(mm);
bad_pgdir_cleanup_mm:
    mm_destroy(mm);
bad_mm:
    return ret;
}

/**
 * @brief Free the first argc argument copies of a kernel argument vector (lab8).
 * @param argc  number of valid entries in @p kargv
 * @param kargv kernel argument vector whose strings were allocated by copy_kargv()
 */
static void put_kargv(int argc, char **kargv) {
    while (argc > 0) {
        kfree(kargv[--argc]);
    }
}

/**
 * @brief Copy an argument vector from the caller's address space into kernel memory (lab8).
 *
 * The vector itself and every string are checked with user_mem_check() / copy_string(). Every
 * string gets its own kmalloc buffer of EXEC_MAX_ARG_LEN + 1 bytes, because the user copy is
 * destroyed together with the old address space.
 * @param mm    address space of the caller (NULL for a kernel thread)
 * @param argc  number of arguments
 * @param kargv receives the argc kernel strings
 * @param argv  argument vector in the caller's address space
 * @return 0 on success; -E_INVAL if a user pointer is bad; -E_NO_MEM if a buffer cannot be allocated.
 *         On failure the strings copied so far are freed.
 */
static int copy_kargv(struct mm_struct *mm, int argc, char **kargv, const char **argv) {
    int i, ret = -E_INVAL;
    if (!user_mem_check(mm, (uintptr_t)argv, sizeof(const char *) * argc, 0)) {
        return ret;
    }
    for (i = 0; i < argc; i++) {
        char *buffer;
        if ((buffer = kmalloc(EXEC_MAX_ARG_LEN + 1)) == NULL) {
            goto failed_nomem;
        }
        if (!copy_string(mm, buffer, argv[i], EXEC_MAX_ARG_LEN + 1)) {
            kfree(buffer);
            goto failed_cleanup;
        }
        kargv[i] = buffer;
    }
    return 0;

failed_nomem:
    ret = -E_NO_MEM;
failed_cleanup:
    put_kargv(i, kargv);
    return ret;
}

//lab5 function2 2414007
/**
 * @fn int do_execve(const char *name, int argc, const char **argv)
 * @brief Replace the program of the current process by an ELF file from the file system (lab8 version).
 *
 * The lab5 version took an ELF image in memory; now the program is opened through the file system:
 *  1. check 1 <= argc <= EXEC_MAX_ARG_NUM; copy the name and the argument vector into kernel memory
 *     while the old address space is still readable (copy_string(), copy_kargv());
 *  2. close every open file of the process (files_closeall()), then open argv[0] read-only with
 *     sysfile_open();
 *  3. release the old mm: switch to the kernel page table first, drop the reference and free the
 *     pages, page tables, page directory and mm_struct when the last user is gone;
 *  4. load_icode(fd, argc, kargv) builds the new address space and the trap frame;
 *  5. free the kernel copies of the arguments and rename the process.
 *
 * @param name program name (readable in the caller's address space); NULL gives "<null> pid"
 * @param argc number of arguments
 * @param argv argument vector in the caller's address space; argv[0] is the path of the ELF file
 * @return 0 on success: the trap frame of current describes the new program, and the caller has to
 *         return through the trap path (kernel_execve() or the system call return) to start it;
 *         -E_INVAL for a bad argument count or a bad user pointer. Once the old image is gone a
 *         failure cannot be reported to the old program, so the process exits with the error code.
 */
int do_execve(const char *name, int argc, const char **argv) {
    static_assert(EXEC_MAX_ARG_LEN >= FS_MAX_FPATH_LEN);
    struct mm_struct *mm = current->mm;
    if (!(argc >= 1 && argc <= EXEC_MAX_ARG_NUM)) {
        return -E_INVAL;
    }

    char local_name[PROC_NAME_LEN + 1];
    memset(local_name, 0, sizeof(local_name));

    char *kargv[EXEC_MAX_ARG_NUM];
    const char *path;

    int ret = -E_INVAL;

    lock_mm(mm);
    if (name == NULL) {
        snprintf(local_name, sizeof(local_name), "<null> %d", current->pid);
    } else {
        if (!copy_string(mm, local_name, name, sizeof(local_name))) {
            unlock_mm(mm);
            return ret;
        }
    }
    if ((ret = copy_kargv(mm, argc, kargv, argv)) != 0) {
        unlock_mm(mm);
        return ret;
    }
    path = argv[0];
    unlock_mm(mm);
    files_closeall(current->filesp);

    // sysfile_open() validates the path against current->mm, so it needs the user-space pointer
    // argv[0]; the kernel copy kargv[0] would be rejected for a user process. The old mm is still
    // intact here, it is released only after the file has been opened.
    int fd;
    if ((ret = fd = sysfile_open(path, O_RDONLY)) < 0) {
        goto execve_exit;
    }
    if (mm != NULL) {
        // Leave the page tables that are about to be freed before freeing them.
        lsatp(boot_pgdir_pa);
        flush_tlb();
        current->pgdir = boot_pgdir_pa;
        if (mm_count_dec(mm) == 0) {
            exit_mmap(mm);
            put_pgdir(mm);
            mm_destroy(mm);
        }
        current->mm = NULL;
    }
    ret = -E_NO_MEM;
    if ((ret = load_icode(fd, argc, kargv)) != 0) {
        goto execve_exit;
    }
    put_kargv(argc, kargv);
    set_proc_name(current, local_name);
    return 0;

execve_exit:
    put_kargv(argc, kargv);
    do_exit(ret);
    panic("already exit: %e.\n", ret);
}

//lab5 function3 2414007 update do_fork
//LAB4 function2 2411622
//lab8 function1-3 2414007 update do_fork
/**
 * @fn int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)
 * @brief Create a child process or thread that copies the current one (exercise, module 2; lab8
 *        adds the file table).
 *
 * Contract, in this order. Each failing step must release what the earlier steps obtained, in the
 * reverse order of acquisition:
 *  0. refuse when MAX_PROCESS processes exist (-E_NO_FREE_PROC);
 *  1. alloc_proc(): new PCB, still PROC_UNINIT (filesp NULL); its parent is current, which must not
 *     be waiting (wait_state 0);
 *  2. setup_kstack(): kernel stack for the child;
 *  3. (lab8) copy_files(clone_flags, child): the file table. With CLONE_FS the child shares the
 *     parent's table, otherwise it gets a copy with the same open files. It either fully succeeds
 *     (child->filesp set, reference count taken) or leaves nothing behind, so a failure here only
 *     has to free the stack and the PCB;
 *  4. copy_mm(clone_flags, child): address space (a no-op for kernel threads; lab5 duplicates the mm
 *     of a user process). A failure here must also drop the file table reference taken in step 3,
 *     with put_files();
 *  5. copy_thread(child, stack, tf): trap frame and initial context on the child's stack;
 *  6. assign a PID with get_pid() and link the child into hash_list and, with set_links(), into
 *     proc_list and the process tree (parent's child list, sibling links, nr_process);
 *  7. wakeup_proc(child): the child becomes PROC_RUNNABLE. This is the only step that makes it runnable;
 *  8. return the child's PID.
 * Steps 6 and 7 are the critical section: interrupts must be disabled around them.
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
    // The process table has a fixed capacity.
    if (nr_process >= MAX_PROCESS) {
        return -E_NO_FREE_PROC;
    }
    // 1. PCB. Nothing has been acquired yet, so a failure needs no cleanup.
    if ((proc = alloc_proc()) == NULL) {
        return -E_NO_MEM;
    }
    // The new process is a child of the running one, which cannot be sleeping in do_wait().
    proc->parent = current;
    assert(current->wait_state == 0);
    // 2. Kernel stack. On failure only the PCB has to be released.
    if ((ret = setup_kstack(proc)) != 0) {
        kfree(proc);
        return ret;
    }
    // 3. File table (lab8). copy_files() cleans up after itself on failure, so no put_files() here:
    //    proc->filesp is still NULL and there is no table to release.
    if ((ret = copy_files(clone_flags, proc)) != 0) {
        put_kstack(proc);
        kfree(proc);
        return ret;
    }
    // 4. Address space. On failure release the file table reference, the stack and the PCB, in
    //    reverse order of acquisition.
    if ((ret = copy_mm(clone_flags, proc)) != 0) {
        put_files(proc);
        put_kstack(proc);
        kfree(proc);
        return ret;
    }

    // 5. Trap frame and initial context on the child's stack (cannot fail).
    copy_thread(proc, stack, tf);

    // 6.-7. Critical section: the PID scan, the list insertions and the wakeup must not be
    //       interleaved with an interrupt, and the child becomes visible to schedule() only at the end.
    bool intr_flag;
    local_intr_save(intr_flag);
    proc->pid = get_pid();
    hash_proc(proc);
    set_links(proc);
    wakeup_proc(proc);
    local_intr_restore(intr_flag);
    // 8. The parent sees the child's PID; the child sees a0 == 0 (set in copy_thread()).
    return proc->pid;
}

//lab5 function4 2414007
/**
 * @fn int do_exit(int error_code)
 * @brief Terminate the current process: free what it can free itself and become a zombie.
 *
 * The process still runs on its own kernel stack and PCB, so those two can only be freed by the
 * parent in do_wait(). Everything else is released here:
 *  - idleproc and initproc must never exit (panic);
 *  - the user address space: the CPU first switches to the kernel page table, then the mm's
 *    reference count is dropped and, if no other process shares it, the pages, page tables, page
 *    directory and mm_struct are freed;
 *  - (lab8) the reference to the file table is dropped with put_files(), which closes the open
 *    files when it was the last one;
 *  - the state becomes PROC_ZOMBIE and the exit code is recorded for the parent;
 *  - with interrupts disabled (so the process tree is not seen half-updated): a parent that sleeps
 *    in do_wait() (wait_state == WT_CHILD) is woken up, and all children are handed over to
 *    initproc, which is woken if one of them is already a zombie and it is waiting;
 *  - finally schedule() gives the CPU away. A zombie is never scheduled again.
 * @param error_code exit status, read by the parent through do_wait()
 * @return does not return
 */
int do_exit(int error_code) {
    if (current == idleproc) {
        panic("idleproc exit.\n");
    }
    if (current == initproc) {
        panic("initproc exit.\n");
    }

    struct mm_struct *mm = current->mm;
    if (mm != NULL) {
        // Stop using the page tables that are about to be freed.
        lsatp(boot_pgdir_pa);
        flush_tlb();
        current->pgdir = boot_pgdir_pa;
        if (mm_count_dec(mm) == 0) {
            exit_mmap(mm);
            put_pgdir(mm);
            mm_destroy(mm);
        }
        current->mm = NULL;
    }
    // lab8: drop the reference to the file table; the last user closes the open files.
    put_files(current);
    current->state = PROC_ZOMBIE;
    current->exit_code = error_code;

    bool intr_flag;
    struct proc_struct *proc;
    local_intr_save(intr_flag);
    {
        // Tell a parent that waits in do_wait() that a child has become a zombie.
        proc = current->parent;
        if (proc->wait_state == WT_CHILD) {
            wakeup_proc(proc);
        }
        // Give the children to initproc, which reaps them (do_wait loop of init_main()).
        while (current->cptr != NULL) {
            proc = current->cptr;
            current->cptr = proc->optr;

            proc->yptr = NULL;
            if ((proc->optr = initproc->cptr) != NULL) {
                initproc->cptr->yptr = proc;
            }
            proc->parent = initproc;
            initproc->cptr = proc;
            if (proc->state == PROC_ZOMBIE) {
                if (initproc->wait_state == WT_CHILD) {
                    wakeup_proc(initproc);
                }
            }
        }
    }
    local_intr_restore(intr_flag);

    schedule();
    panic("do_exit will not return!! pid %d.\n", current->pid);
}

//lab5 function4 2414007
/**
 * @fn int do_yield(void)
 * @brief Offer the CPU to other processes.
 *
 * Only sets current->need_resched. trap() checks the flag on the way back to user mode and calls
 * schedule(), so the process is switched out at the end of the system call, not inside it.
 * @return 0
 */
int do_yield(void) {
    current->need_resched = 1;
    return 0;
}

//lab5 function4 2414007
/**
 * @fn int do_kill(int pid)
 * @brief Ask a process to terminate.
 *
 * The victim is not stopped here: PF_EXITING is set in its flags, and trap() makes it call
 * do_exit(-E_KILLED) the next time it is about to return to user mode. If the victim sleeps in an
 * interruptible wait (WT_INTERRUPTED, for example in do_wait()) it is woken up so that it notices
 * the flag soon.
 * @param pid PID of the victim
 * @return 0 on success; -E_INVAL if no process has this PID; -E_KILLED if it was already marked
 */
int do_kill(int pid) {
    struct proc_struct *proc;
    if ((proc = find_proc(pid)) != NULL) {
        if (!(proc->flags & PF_EXITING)) {
            proc->flags |= PF_EXITING;
            if (proc->wait_state & WT_INTERRUPTED) {
                wakeup_proc(proc);
            }
            return 0;
        }
        return -E_KILLED;
    }
    return -E_INVAL;
}

/**
 * @brief Run kernel_execve() for the program file @p path of the file system (lab8).
 *
 * Builds the argument vector {path, extra arguments..., NULL} on the stack and calls
 * kernel_execve(). The program is found through the file system, not linked into the kernel image.
 * @param name program name, printed and used as the process name
 * @param path path of the ELF file; it becomes argv[0]
 * @param ...  further arguments of the program
 */
#define __KERNEL_EXECVE(name, path, ...)                                          \
    ({                                                                            \
        const char *argv[] = {path, ##__VA_ARGS__, NULL};                         \
        cprintf("kernel_execve: pid = %d, name = \"%s\".\n", current->pid, name); \
        kernel_execve(name, argv);                                                \
    })

/**
 * @brief Run program x of the file system, using its name as the path.
 * @param x   program name as a bare identifier (for example `sh`)
 * @param ... further arguments of the program, as string literals
 */
#define KERNEL_EXECVE(x, ...) __KERNEL_EXECVE(#x, #x, ##__VA_ARGS__)

/**
 * @brief Same as KERNEL_EXECVE(); kept so that the TEST macros of `make build-x` keep working.
 */
#define KERNEL_EXECVE2(x, ...) KERNEL_EXECVE(x, ##__VA_ARGS__)

/**
 * @brief Helper of KERNEL_EXECVE3(): run program x with the string s as its single argument.
 */
#define __KERNEL_EXECVE3(x, s, ...) KERNEL_EXECVE(x, #s, ##__VA_ARGS__)

/**
 * @brief Run program x with the script name s as argument (`make script-x` defines TESTSCRIPT).
 */
#define KERNEL_EXECVE3(x, s, ...) __KERNEL_EXECVE3(x, s, ##__VA_ARGS__)

//lab5 function2 2414007
/**
 * @fn static int kernel_execve(const char *name, const char **argv)
 * @brief Turn the current kernel thread into a user process running the program argv[0] (lab8 version).
 *
 * do_execve() only prepares the new image and the trap frame; the CPU is still in S mode, in the
 * kernel thread. There is no real trap to return from, so the return path is entered by hand:
 * sp is pointed at the trap frame that load_icode() wrote at the top of the kernel stack
 * (current->tf) and the code jumps to __trapret, which restores the registers from the frame, puts
 * the kernel stack top into sscratch and executes sret. sret then lands in user mode at the entry
 * point of the program. Calling do_execve() alone and returning would leave the thread in the
 * kernel.
 *
 * The lab5 version received an ELF image in memory; now the argument vector is counted up to its
 * terminating NULL pointer and do_execve() loads argv[0] from the file system.
 * @param name program name
 * @param argv NULL-terminated argument vector; argv[0] is the path of the ELF file
 * @return does not return on success; the negative error code of do_execve() otherwise
 */
static int kernel_execve(const char *name, const char **argv) {
    int64_t ret = 0;
    int argc = 0;
    while (argv[argc] != NULL) {
        argc++;
    }
    ret = do_execve(name, argc, argv);
    if (ret != 0) {
        return ret;
    }
    asm volatile("mv sp, %0\n"
                 "j __trapret\n"
                 :
                 : "r"(current->tf)
                 : "memory");
    // Not reached: __trapret ends in sret.
    return 0;
}

//lab5 function2 2414007
/**
 * @fn static int user_main(void *arg)
 * @brief Body of the kernel thread that becomes the first user process.
 *
 * Created by init_main(). It selects the program and calls kernel_execve(), which does not return
 * on success. With `make script-x` (TEST and TESTSCRIPT defined) it runs the program TEST with the
 * argument TESTSCRIPT, with `make build-x` (only TEST defined) the program TEST, and otherwise the
 * shell `sh` of the file system.
 * @param arg unused
 * @return does not return; reaching the panic means that the program could not be started
 */
static int user_main(void *arg) {
#ifdef TEST
#ifdef TESTSCRIPT
    KERNEL_EXECVE3(TEST, TESTSCRIPT);
#else
    KERNEL_EXECVE(TEST);
#endif
#else
    KERNEL_EXECVE(sh);
#endif
    panic("user_main execve failed.\n");
}

//lab5 function2 2414007
/**
 * @fn int do_wait(int pid, int *code_store)
 * @brief Wait for a child to exit, then reclaim its kernel stack and PCB.
 *
 * With pid == 0 any child counts, otherwise only that child, which must be a child of the caller
 * (else -E_BAD_PROC). If code_store is not NULL it is checked with user_mem_check() for a writable
 * int in the caller's address space (-E_INVAL) before anything is done. The caller searches its
 * children for a PROC_ZOMBIE. If there is none but a child exists, it sleeps (PROC_SLEEPING with
 * wait_state WT_CHILD, which do_exit() of a child answers with a wakeup), calls schedule() and
 * searches again after the wakeup; if it was killed meanwhile (PF_EXITING) it exits instead.
 *
 * When a zombie is found, its exit code is stored, it is removed from the hash table, the process
 * list and the process tree (with interrupts disabled), and its kernel stack and PCB are freed.
 * These two resources could not be freed by the child itself in do_exit().
 * @param pid        PID of the child, or 0 for any child
 * @param code_store user address that receives the exit code, or NULL
 * @return 0 on success; -E_BAD_PROC if there is no such child; -E_INVAL for a bad code_store;
 *         -E_KILLED is delivered through do_exit() when the caller is killed while waiting
 */
int do_wait(int pid, int *code_store) {
    struct mm_struct *mm = current->mm;
    if (code_store != NULL) {
        if (!user_mem_check(mm, (uintptr_t)code_store, sizeof(int), 1)) {
            return -E_INVAL;
        }
    }

    struct proc_struct *proc;
    bool intr_flag, haskid;
repeat:
    haskid = 0;
    if (pid != 0) {
        proc = find_proc(pid);
        if (proc != NULL && proc->parent == current) {
            haskid = 1;
            if (proc->state == PROC_ZOMBIE) {
                goto found;
            }
        }
    } else {
        proc = current->cptr;
        for (; proc != NULL; proc = proc->optr) {
            haskid = 1;
            if (proc->state == PROC_ZOMBIE) {
                goto found;
            }
        }
    }
    if (haskid) {
        // A child exists but has not exited yet: sleep until do_exit() of a child wakes us up.
        current->state = PROC_SLEEPING;
        current->wait_state = WT_CHILD;
        schedule();
        if (current->flags & PF_EXITING) {
            do_exit(-E_KILLED);
        }
        goto repeat;
    }
    return -E_BAD_PROC;

found:
    if (proc == idleproc || proc == initproc) {
        panic("wait idleproc or initproc.\n");
    }
    if (code_store != NULL) {
        *code_store = proc->exit_code;
    }
    local_intr_save(intr_flag);
    {
        unhash_proc(proc);
        remove_links(proc);
    }
    local_intr_restore(intr_flag);
    put_kstack(proc);
    kfree(proc);
    return 0;
}

//lab5 function2 2414007 update init_main
//LAB4 function2 2411622
/**
 * @brief Body of initproc (lab8 version): mount the boot file system, start the first user process
 *        and wait for all of them.
 *
 * Runs as the first kernel thread created by kernel_thread(). It makes "disk0:" the boot file
 * system with vfs_set_bootfs() (so that paths like "/sh" resolve to the SFS disk), records the
 * number of processes, creates the kernel thread user_main (which becomes the first user process),
 * then reaps children with do_wait(0, NULL), calling schedule() between calls so that the children
 * get the CPU, until no child is left. Then it flushes the file systems with fs_cleanup() and
 * checks that the process table is back in the state it started from: initproc has no relatives
 * and only idleproc and initproc remain in proc_list.
 *
 * lab7 ran the dining philosophers (check_sync()) here. lab8 no longer does: the philosopher
 * threads would print into the console while the shell is running, and they are not part of the
 * file system exercises. kern/sync/check_sync.c is still compiled.
 *
 * @param arg a `const char *` message passed by proc_init() (not used any more)
 * @return 0; returning hands control to kernel_thread_entry(), which calls do_exit()
 * @note The number of free physical pages is deliberately not compared with the value at entry:
 *       the kmalloc() heap (SLOB, kmalloc.c) takes pages from the page allocator when many
 *       PCBs, mm_structs and VMAs are alive at once (forktest) and never returns them, so the
 *       count differs by a few pages without any leak, and kallocated() always returns 0.
 */
static int init_main(void *arg) {
    int ret;
    if ((ret = vfs_set_bootfs("disk0:")) != 0) {
        panic("set boot fs failed: %e.\n", ret);
    }

    int nr_process_store = nr_process;

    int pid = kernel_thread(user_main, NULL, 0);
    if (pid <= 0) {
        panic("create user_main failed.\n");
    }

    while (do_wait(0, NULL) == 0) {
        schedule();
    }

    fs_cleanup();

    cprintf("all user-mode processes have quit.\n");
    assert(initproc->cptr == NULL && initproc->yptr == NULL && initproc->optr == NULL);
    assert(nr_process == nr_process_store);
    assert(list_next(&proc_list) == &(initproc->list_link));
    assert(list_prev(&proc_list) == &(initproc->list_link));
    cprintf("init check memory pass.\n");
    return 0;
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
 *  - (lab8) idleproc gets a file table of its own with files_create() and holds one reference to
 *    it: it is the template that do_fork() copies for initproc and, through it, for every process;
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
    // lab8: idleproc owns the first file table; initproc and its descendants copy it in do_fork().
    if ((idleproc->filesp = files_create()) == NULL) {
        panic("create filesp (idleproc) failed.\n");
    }
    files_count_inc(idleproc->filesp);
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

/**
 * @brief Set the scheduling priority of the current process (lab6; reached by the
 *        sys_lab6_set_priority system call).
 * @param priority new priority; a bigger value earns a larger share of the CPU under Stride
 *        scheduling. 0 is not a valid priority (it would divide by zero when the Stride pass is
 *        computed) and is replaced by 1.
 * @note Prints "set priority to N" with the value passed in (also when it is 0), which the
 *       priority test prints to the console.
 */
void lab6_set_priority(uint32_t priority) {
    cprintf("set priority to %d\n", priority);
    if (priority == 0) {
        current->lab6_priority = 1;
    } else {
        current->lab6_priority = priority;
    }
}

/**
 * @brief Put the current process to sleep for @p time timer ticks (kernel side of SYS_sleep).
 *
 * A timer that expires after @p time ticks is armed for the current process, which is marked
 * PROC_SLEEPING with wait_state WT_TIMER and then gives up the CPU with schedule(). When the timer
 * expires, run_timer_list() (called from the timer interrupt) wakes the process up, and the timer
 * is removed again after schedule() returns. The timer lives on the kernel stack of the caller,
 * which is safe because the caller cannot leave this function while the timer is armed.
 *
 * @param time number of timer ticks to sleep; 0 returns at once without sleeping
 * @return 0 (also if the sleep was cut short by do_kill(), which wakes the process up early)
 * @note The state change and add_timer() happen with interrupts disabled, so the timer cannot
 *       fire between them and be lost.
 */
int do_sleep(unsigned int time) {
    if (time == 0) {
        return 0;
    }
    bool intr_flag;
    local_intr_save(intr_flag);
    timer_t __timer, *timer = timer_init(&__timer, current, time);
    current->state = PROC_SLEEPING;
    current->wait_state = WT_TIMER;
    add_timer(timer);
    local_intr_restore(intr_flag);

    schedule();

    del_timer(timer);
    return 0;
}
