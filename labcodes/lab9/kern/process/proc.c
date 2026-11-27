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

// the process set's list
list_entry_t proc_list;

#define HASH_SHIFT 10
#define HASH_LIST_SIZE (1 << HASH_SHIFT)
#define pid_hashfn(x) (hash32(x, HASH_SHIFT))

// has list for process set based on pid
static list_entry_t hash_list[HASH_LIST_SIZE];

// idle proc
struct proc_struct *idleproc = NULL;
// init proc
struct proc_struct *initproc = NULL;
// current proc
struct proc_struct *current = NULL;

static int nr_process = 0;

void kernel_thread_entry(void);
void forkrets(struct trapframe *tf);
void switch_to(struct context *from, struct context *to);

// LAB4:填写你在lab4中实现的代码

// LAB5:填写你在lab5中实现的代码

// LAB6:填写你在lab6中实现的代码

// LAB8:填写你在lab8中实现的代码



// set_proc_name - set the name of proc
char *
set_proc_name(struct proc_struct *proc, const char *name)
{
    memset(proc->name, 0, sizeof(proc->name));
    return memcpy(proc->name, name, PROC_NAME_LEN);
}

// get_proc_name - get the name of proc
char *
get_proc_name(struct proc_struct *proc)
{
    static char name[PROC_NAME_LEN + 1];
    memset(name, 0, sizeof(name));
    return memcpy(name, proc->name, PROC_NAME_LEN);
}

// set_links - set the relation links of process
static void
set_links(struct proc_struct *proc)
{
    list_add(&proc_list, &(proc->list_link));
    proc->yptr = NULL;
    if ((proc->optr = proc->parent->cptr) != NULL)
    {
        proc->optr->yptr = proc;
    }
    proc->parent->cptr = proc;
    nr_process++;
}

// remove_links - clean the relation links of process
static void
remove_links(struct proc_struct *proc)
{
    list_del(&(proc->list_link));
    if (proc->optr != NULL)
    {
        proc->optr->yptr = proc->yptr;
    }
    if (proc->yptr != NULL)
    {
        proc->yptr->optr = proc->optr;
    }
    else
    {
        proc->parent->cptr = proc->optr;
    }
    nr_process--;
}

// get_pid - alloc a unique pid for process
static int
get_pid(void)
{
    static_assert(MAX_PID > MAX_PROCESS);
    struct proc_struct *proc;
    list_entry_t *list = &proc_list, *le;
    static int next_safe = MAX_PID, last_pid = MAX_PID;
    if (++last_pid >= MAX_PID)
    {
        last_pid = 1;
        goto inside;
    }
    if (last_pid >= next_safe)
    {
    inside:
        next_safe = MAX_PID;
    repeat:
        le = list;
        while ((le = list_next(le)) != list)
        {
            proc = le2proc(le, list_link);
            if (proc->pid == last_pid)
            {
                if (++last_pid >= next_safe)
                {
                    if (last_pid >= MAX_PID)
                    {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    goto repeat;
                }
            }
            else if (proc->pid > last_pid && next_safe > proc->pid)
            {
                next_safe = proc->pid;
            }
        }
    }
    return last_pid;
}



// forkret -- the first kernel entry point of a new thread/process
// NOTE: the addr of forkret is setted in copy_thread function
//       after switch_to, the current proc will execute here.
static void
forkret(void)
{
    forkrets(current->tf);
}

// hash_proc - add proc into proc hash_list
static void
hash_proc(struct proc_struct *proc)
{
    list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
}

// unhash_proc - delete proc from proc hash_list
static void
unhash_proc(struct proc_struct *proc)
{
    list_del(&(proc->hash_link));
}

// find_proc - find proc frome proc hash_list according to pid
struct proc_struct *
find_proc(int pid)
{
    if (0 < pid && pid < MAX_PID)
    {
        list_entry_t *list = hash_list + pid_hashfn(pid), *le = list;
        while ((le = list_next(le)) != list)
        {
            struct proc_struct *proc = le2proc(le, hash_link);
            if (proc->pid == pid)
            {
                return proc;
            }
        }
    }
    return NULL;
}


// setup_kstack - alloc pages with size KSTACKPAGE as process kernel stack
static int
setup_kstack(struct proc_struct *proc)
{
    struct Page *page = alloc_pages(KSTACKPAGE);
    if (page != NULL)
    {
        proc->kstack = (uintptr_t)page2kva(page);
        return 0;
    }
    return -E_NO_MEM;
}

// put_kstack - free the memory space of process kernel stack
static void
put_kstack(struct proc_struct *proc)
{
    free_pages(kva2page((void *)(proc->kstack)), KSTACKPAGE);
}

// setup_pgdir - alloc one page as PDT
static int
setup_pgdir(struct mm_struct *mm)
{
    struct Page *page;
    if ((page = alloc_page()) == NULL)
    {
        return -E_NO_MEM;
    }
    pde_t *pgdir = page2kva(page);
    memcpy(pgdir, boot_pgdir_va, PGSIZE);

    mm->pgdir = pgdir;
    return 0;
}

// put_pgdir - free the memory space of PDT
static void
put_pgdir(struct mm_struct *mm)
{
    free_page(kva2page(mm->pgdir));
}


// copy_files&put_files function used by do_fork in LAB8
// copy the files_struct from current to proc
static int
copy_files(uint32_t clone_flags, struct proc_struct *proc)
{
    struct files_struct *filesp, *old_filesp = current->filesp;
    assert(old_filesp != NULL);

    if (clone_flags & CLONE_FS)
    {
        filesp = old_filesp;
        goto good_files_struct;
    }

    int ret = -E_NO_MEM;
    if ((filesp = files_create()) == NULL)
    {
        goto bad_files_struct;
    }

    if ((ret = dup_files(filesp, old_filesp)) != 0)
    {
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

// decrease the ref_count of files, and if ref_count==0, then destroy files_struct
static void
put_files(struct proc_struct *proc)
{
    struct files_struct *filesp = proc->filesp;
    if (filesp != NULL)
    {
        if (files_count_dec(filesp) == 0)
        {
            files_destroy(filesp);
        }
    }
}


// load_icode_read is used by load_icode in LAB8
static int
load_icode_read(int fd, void *buf, size_t len, off_t offset)
{
    int ret;
    if ((ret = sysfile_seek(fd, offset, LSEEK_SET)) != 0)
    {
        return ret;
    }
    if ((ret = sysfile_read(fd, buf, len)) != len)
    {
        return (ret < 0) ? ret : -1;
    }
    return 0;
}


// this function isn't very correct in LAB8
static void
put_kargv(int argc, char **kargv)
{
    while (argc > 0)
    {
        kfree(kargv[--argc]);
    }
}

static int
copy_kargv(struct mm_struct *mm, int argc, char **kargv, const char **argv)
{
    int i, ret = -E_INVAL;
    if (!user_mem_check(mm, (uintptr_t)argv, sizeof(const char *) * argc, 0))
    {
        return ret;
    }
    for (i = 0; i < argc; i++)
    {
        char *buffer;
        if ((buffer = kmalloc(EXEC_MAX_ARG_LEN + 1)) == NULL)
        {
            goto failed_nomem;
        }
        if (!copy_string(mm, buffer, argv[i], EXEC_MAX_ARG_LEN + 1))
        {
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


#define __KERNEL_EXECVE(name, path, ...) ({              \
    const char *argv[] = {path, ##__VA_ARGS__, NULL};    \
    cprintf("kernel_execve: pid = %d, name = \"%s\".\n", \
            current->pid, name);                         \
    kernel_execve(name, argv);                           \
})

#define KERNEL_EXECVE(x, ...) __KERNEL_EXECVE(#x, #x, ##__VA_ARGS__)

#define KERNEL_EXECVE2(x, ...) KERNEL_EXECVE(x, ##__VA_ARGS__)

#define __KERNEL_EXECVE3(x, s, ...) KERNEL_EXECVE(x, #s, ##__VA_ARGS__)

#define KERNEL_EXECVE3(x, s, ...) __KERNEL_EXECVE3(x, s, ##__VA_ARGS__)


// FOR LAB6, set the process's priority (bigger value will get more CPU time)
void lab6_set_priority(uint32_t priority)
{
    cprintf("set priority to %d\n", priority);
    if (priority == 0)
        current->lab6_priority = 1;
    else
        current->lab6_priority = priority;
}
// do_sleep - set current process state to sleep and add timer with "time"
//          - then call scheduler. if process run again, delete timer first.
int do_sleep(unsigned int time)
{
    if (time == 0)
    {
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
