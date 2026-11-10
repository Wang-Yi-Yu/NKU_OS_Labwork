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

/**
 * @brief Life-cycle state of a process.
 *
 * PROC_UNINIT: allocated, not yet ready (alloc_proc). PROC_RUNNABLE: may run or is running.
 * PROC_SLEEPING: waiting for an event. PROC_ZOMBIE: finished, waiting for its parent (do_exit).
 */
// process's state in his life cycle
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
 * until the process is freed after it has become a zombie.
 * @note Do not read tf or context before copy_thread() has initialised them.
 */
struct proc_struct {
    enum proc_state state;         // Process state
    int pid;                       // Process ID
    int runs;                      // the running times of Proces
    uintptr_t kstack;              // Process kernel stack
    volatile bool need_resched;    // bool value: need to be rescheduled to release CPU?
    struct proc_struct *parent;    // the parent process
    struct mm_struct *mm;          // Process's memory management field
    struct context context;        // Switch here to run process
    struct trapframe *tf;          // Trap frame for current interrupt
    uintptr_t pgdir;               // the base addr of Page Directroy Table(PDT)
    uint32_t flags;                // Process flag
    char name[PROC_NAME_LEN + 1];  // Process name
    list_entry_t list_link;        // Process link list
    list_entry_t hash_link;        // Process hash list
};

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
 * @note Exercise in lab4 module 2; see proc.c for the contract.
 */
void proc_init(void);
/**
 * @brief Switch the CPU to @p proc (page table and context).
 * @param proc process to run, normally in PROC_RUNNABLE state
 * @note Exercise in lab4 module 3; see proc.c for the contract.
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
 * @note Exercise in lab4 module 3; see proc.c for the contract.
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
 * @note Exercise in lab4 module 2; see proc.c for the contract.
 */
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
/**
 * @brief Exit the current process (lab4: placeholder that panics).
 * @param error_code exit status
 * @return never returns
 */
int do_exit(int error_code);

#endif /* !__KERN_PROCESS_PROC_H__ */
