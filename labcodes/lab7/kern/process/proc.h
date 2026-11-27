/**
 * @file   proc.h
 * @brief  Process control block (struct proc_struct), kernel context and the process-management API.
 *
 * Each process or kernel thread is described by one struct proc_struct, which lives in the kernel
 * heap. Its kernel stack is separate (KSTACKPAGE pages) and holds the trap frame (tf) and the
 * switch context (context) of the thread while it is not running.
 */
#ifndef __KERN_PROCESS_PROC_H__
#define __KERN_PROCESS_PROC_H__

#include <defs.h>
#include <list.h>
#include <trap.h>
#include <memlayout.h>
#include <skew_heap.h>

/**
 * @brief Life-cycle state of a process.
 *
 * PROC_UNINIT: allocated, not yet ready (alloc_proc). PROC_RUNNABLE: may run or is running.
 * PROC_SLEEPING: waiting for an event. PROC_ZOMBIE: finished, waiting for its parent (do_exit).
 */
enum proc_state {
    PROC_UNINIT = 0,  // uninitialized
    PROC_SLEEPING,    // sleeping
    PROC_RUNNABLE,    // runnable(maybe running)
    PROC_ZOMBIE,      // almost dead, and wait parent proc to reclaim his resource
};

/**
 * @brief Callee-saved register state of a thread that is switched out.
 *
 * switch_to() saves and restores exactly these fields. The field order is the layout that switch.S
 * depends on, so it must not be changed. Caller-saved registers are not stored because switch_to()
 * is an ordinary function call and the compiler saves them itself. A new thread starts with ra set
 * to forkret.
 */
struct context {
    uintptr_t ra;
    uintptr_t sp;
    uintptr_t s0;
    uintptr_t s1;
    uintptr_t s2;
    uintptr_t s3;
    uintptr_t s4;
    uintptr_t s5;
    uintptr_t s6;
    uintptr_t s7;
    uintptr_t s8;
    uintptr_t s9;
    uintptr_t s10;
    uintptr_t s11;
};

/** @brief Maximum length of a process name, without the terminating NUL. */
#define PROC_NAME_LEN 15
/** @brief Maximum number of processes that can exist at the same time. */
#define MAX_PROCESS 4096
/** @brief Upper bound (exclusive) of PIDs; PIDs are allocated in (0, MAX_PID). */
#define MAX_PID (MAX_PROCESS * 2)

/** @brief List of all process control blocks, linked via proc_struct::list_link (defined in proc.c). */
extern list_entry_t proc_list;

/**
 * @brief Process control block (PCB) of one process or kernel thread.
 *
 * Created by alloc_proc() and filled in by do_fork() / proc_init(). Lifetime: from alloc_proc()
 * until the parent frees it in do_wait() after it has become a zombie.
 * @var proc_struct::exit_code  exit status, written by do_exit() and read by the parent in do_wait()
 * @var proc_struct::wait_state why the process sleeps (WT_CHILD in do_wait(), WT_KSEM on a semaphore,
 *                              WT_TIMER in do_sleep()), 0 otherwise
 * @var proc_struct::cptr       youngest child of this process (head of the child list)
 * @var proc_struct::yptr       next younger sibling
 * @var proc_struct::optr       next older sibling
 * @var proc_struct::rq          run queue this process is currently in, NULL when it is in none (lab6)
 * @var proc_struct::run_link    node linking the process into rq->run_list (RR scheduler, lab6)
 * @var proc_struct::time_slice  remaining ticks the process may keep the CPU (lab6)
 * @var proc_struct::lab6_run_pool  node linking the process into the skew heap rq->lab6_run_pool (Stride)
 * @var proc_struct::lab6_stride    current Stride value; the runnable process with the smallest one runs next
 * @var proc_struct::lab6_priority  priority, set by lab6_set_priority(); the Stride pass is BIG_STRIDE / priority
 * @var proc_struct::rcu_read_depth nesting of rcu_read_lock(); 0 means the process is not in a read-side section
 * @var proc_struct::rcu_gp_seen    newest grace-period number for which this process has been quiescent
 * @note Do not read tf or context before copy_thread() has initialised them.
 */
struct proc_struct {
    enum proc_state state;                   // Process state
    int pid;                                 // Process ID
    int runs;                                // the running times of Proces
    uintptr_t kstack;                        // Process kernel stack
    volatile bool need_resched;              // bool value: need to be rescheduled to release CPU?
    struct proc_struct *parent;              // the parent process
    struct mm_struct *mm;                    // Process's memory management field
    struct context context;                  // Switch here to run process
    struct trapframe *tf;                    // Trap frame for current interrupt
    uintptr_t pgdir;                         // the base addr of Page Directroy Table(PDT)
    uint32_t flags;                          // Process flag
    char name[PROC_NAME_LEN + 1];            // Process name
    list_entry_t list_link;                  // Process link list
    list_entry_t hash_link;                  // Process hash list
    int exit_code;                           // exit code (be sent to parent proc)
    uint32_t wait_state;                     // waiting state
    struct proc_struct *cptr, *yptr, *optr;  // relations between processes
    struct run_queue *rq;                    // run queue that holds this process (NULL if none)
    list_entry_t run_link;                   // the entry linked in the run queue
    int time_slice;                          // time slice for occupying the CPU
    skew_heap_entry_t lab6_run_pool;         // lab6 only: the entry in the Stride run pool
    uint32_t lab6_stride;                    // lab6 only: the current stride of the process
    uint32_t lab6_priority;  // lab6 only: priority, set by lab6_set_priority(uint32_t)
    int rcu_read_depth;      // lab7 RCU: nesting of rcu_read_lock(); 0 is a quiescent reader
    unsigned rcu_gp_seen;    // lab7 RCU: newest grace period this process has passed
};

/** @brief Flag in proc_struct::flags: the process has been asked to exit (set by do_kill). */
#define PF_EXITING 0x00000001  // getting shutdown

/**
 * @brief wait_state value of a process sleeping in do_wait() for a child; WT_INTERRUPTED means
 *        that wakeup_proc() (for example from do_kill) may wake it up.
 */
#define WT_CHILD (0x00000001 | WT_INTERRUPTED)
/** @brief Bit of proc_struct::wait_state: the wait may be interrupted. */
#define WT_INTERRUPTED 0x80000000  // the wait state could be interrupted
/**
 * @brief wait_state value of a process sleeping on a kernel semaphore (lab7; set by down() through
 *        wait_current_set()). It is not interruptible: do_kill() cannot wake such a process.
 */
#define WT_KSEM 0x00000100
/**
 * @brief wait_state value of a process sleeping in do_sleep() until its timer expires (lab7).
 *        WT_INTERRUPTED means that do_kill() may cut the sleep short.
 */
#define WT_TIMER (0x00000002 | WT_INTERRUPTED)

/**
 * @brief Get the PCB that contains the list node @p le.
 * @param le     pointer to a list_entry_t field (for example proc->list_link)
 * @param member name of that list_entry_t field inside struct proc_struct
 */
#define le2proc(le, member) to_struct((le), struct proc_struct, member)

/** @brief The idle process, the init process and the process that owns the CPU (defined in proc.c). */
extern struct proc_struct *idleproc, *initproc, *current;

/**
 * @brief Set up idleproc and initproc (called once from kern_init()).
 * @note Exercise of lab4 module 2 (carried over); see proc.c for the contract.
 */
void proc_init(void);
/**
 * @brief Switch the CPU to @p proc (page table and context).
 * @param proc process to run, normally in PROC_RUNNABLE state
 * @note Exercise of lab4 module 3 (carried over); see proc.c for the contract.
 */
void proc_run(struct proc_struct *proc);
/**
 * @brief Create a kernel thread that runs fn(arg).
 * @param fn          thread function
 * @param arg         argument of fn
 * @param clone_flags extra clone flags
 * @return PID of the new thread, or a negative error code
 */
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);

/**
 * @brief Set the name of a process (truncated to PROC_NAME_LEN characters).
 * @param proc process to name
 * @param name new name
 * @return the name buffer of @p proc
 */
char *set_proc_name(struct proc_struct *proc, const char *name);
/**
 * @brief Get a copy of the name of a process.
 * @param proc process to query
 * @return pointer to a static buffer, valid until the next call
 */
char *get_proc_name(struct proc_struct *proc);
/**
 * @brief Body of idleproc: loop, calling schedule() when need_resched is set. Never returns.
 * @note Exercise of lab4 module 3 (carried over); see proc.c for the contract.
 */
void cpu_idle(void) __attribute__((noreturn));

/**
 * @brief Find a process by PID.
 * @param pid PID to look up
 * @return the PCB, or NULL if no live process has this PID
 */
struct proc_struct *find_proc(int pid);
/**
 * @brief Create a child process or thread (copy of the current one).
 * @param clone_flags CLONE_* flags (CLONE_VM shares the address space)
 * @param stack       stack pointer for the child, or 0 for a kernel thread
 * @param tf          trap frame to copy into the child
 * @return child PID on success, or a negative error code
 * @note lab4 exercise that lab5 updates (function3): the child now also needs its relations in
 *       the process tree and a copy of the parent's address space; see proc.c.
 */
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
/**
 * @brief Terminate the current process (lab5 exercise, function4; see proc.c for the contract).
 *
 * Releases the user address space, marks the process PROC_ZOMBIE with the exit code, wakes up a
 * parent that waits for a child, hands its own children to initproc and calls schedule().
 *
 * @param error_code exit status, delivered to the parent by do_wait()
 * @return never returns
 */
int do_exit(int error_code);
/**
 * @brief Ask the scheduler to run another process (lab5 exercise, function4).
 * @return 0; the current process only has need_resched set and is switched out when the trap returns
 */
int do_yield(void);
/**
 * @brief Replace the current process image by a program in memory (lab5 exercise, function2).
 * @param name   program name, copied into the process name
 * @param len    length of @p name
 * @param binary start of the ELF image in memory
 * @param size   size of the ELF image in bytes
 * @return 0 on success (the caller then returns to the new program), a negative error code on failure
 */
int do_execve(const char *name, size_t len, unsigned char *binary, size_t size);
/**
 * @brief Wait for a child of the current process to become a zombie and reclaim it (lab5 exercise).
 * @param pid        PID of the child to wait for, or 0 for any child
 * @param code_store if not NULL, user address that receives the exit code of the child
 * @return 0 on success; -E_BAD_PROC if there is no such child; -E_INVAL if code_store is not a
 *         valid user buffer; -E_KILLED if the caller was killed while waiting
 */
int do_wait(int pid, int *code_store);
/**
 * @brief Mark a process for termination (lab5 exercise, function4).
 * @param pid PID of the victim
 * @return 0 on success; -E_INVAL if the PID does not exist; -E_KILLED if it is already being killed
 */
int do_kill(int pid);
/**
 * @brief Set the scheduling priority of the current process (lab6; see proc.c).
 * @param priority new priority, a bigger value gets more CPU time under Stride scheduling; 0 is
 *        treated as 1
 */
void lab6_set_priority(uint32_t priority);
/**
 * @brief Put the current process to sleep for @p time timer ticks (lab7; see proc.c).
 * @param time number of timer ticks to sleep; 0 returns at once
 * @return 0
 */
int do_sleep(unsigned int time);

#endif /* !__KERN_PROCESS_PROC_H__ */
