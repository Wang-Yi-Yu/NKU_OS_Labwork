/**
 * @file   wait.h
 * @brief  Wait queues: the list of processes that sleep until some event happens.
 *
 * A process that cannot continue (no semaphore resource, a sleep timer, ...) puts a wait_t entry,
 * which refers to the process, into the wait_queue_t of the event and sleeps. When the event happens
 * the kernel takes the entry out of the queue and makes the process runnable again. Semaphores
 * (sem.c) and, on top of them, condition variables (monitor.c) are built from these functions.
 *
 * The functions of this file work on plain lists and do not disable interrupts themselves. The
 * callers (__up() and __down() in sem.c) do that around every use.
 */
#ifndef __KERN_SYNC_WAIT_H__
#define __KERN_SYNC_WAIT_H__

#include <list.h>

/**
 * @brief A wait queue: the head of a circular list of wait_t entries.
 * @var wait_queue_t::wait_head head node of the list; new entries are inserted just before it, so
 *                              the queue is first in, first out
 */
typedef struct {
    list_entry_t wait_head;
} wait_queue_t;

struct proc_struct;

/**
 * @brief One entry of a wait queue: the record of one sleeping process.
 * @var wait_t::proc         the sleeping process
 * @var wait_t::wakeup_flags why the process was woken up; initialised to WT_INTERRUPTED and set to
 *                           the wait state of the queue (for example WT_KSEM) by a normal wake-up
 * @var wait_t::wait_queue   the queue this entry is in
 * @var wait_t::wait_link    node linking the entry into wait_queue_t::wait_head; self-linked when the
 *                           entry is not in a queue
 * @note A wait_t usually lives on the kernel stack of the sleeping process (see __down() in sem.c).
 */
typedef struct {
    struct proc_struct *proc;
    uint32_t wakeup_flags;
    wait_queue_t *wait_queue;
    list_entry_t wait_link;
} wait_t;

/** @brief Get the wait_t that contains the list node @p le (the node member is named by @p member). */
#define le2wait(le, member) to_struct((le), wait_t, member)

/**
 * @brief Initialise a wait entry for a process.
 * @param wait entry to initialise; not in any queue afterwards
 * @param proc process that will sleep on this entry
 * @note wakeup_flags starts as WT_INTERRUPTED, which means "not woken up normally yet".
 */
void wait_init(wait_t *wait, struct proc_struct *proc);
/**
 * @brief Initialise an empty wait queue.
 * @param queue queue to initialise
 */
void wait_queue_init(wait_queue_t *queue);
/**
 * @brief Add an entry at the tail of a queue.
 * @param queue queue to insert into
 * @param wait  entry to insert; it must belong to a process and must not be in a queue
 */
void wait_queue_add(wait_queue_t *queue, wait_t *wait);
/**
 * @brief Remove an entry from the queue it is in.
 * @param queue the queue that holds the entry
 * @param wait  entry to remove; afterwards it is self-linked again
 */
void wait_queue_del(wait_queue_t *queue, wait_t *wait);

/**
 * @brief Get the entry behind @p wait.
 * @param queue queue that holds @p wait
 * @param wait  an entry in @p queue
 * @return the next entry, or NULL if @p wait is the last one
 */
wait_t *wait_queue_next(wait_queue_t *queue, wait_t *wait);
/**
 * @brief Get the entry in front of @p wait.
 * @param queue queue that holds @p wait
 * @param wait  an entry in @p queue
 * @return the previous entry, or NULL if @p wait is the first one
 */
wait_t *wait_queue_prev(wait_queue_t *queue, wait_t *wait);
/**
 * @brief Get the first entry of a queue (the one that has waited longest).
 * @param queue queue to look at
 * @return the first entry, or NULL if the queue is empty
 */
wait_t *wait_queue_first(wait_queue_t *queue);
/**
 * @brief Get the last entry of a queue.
 * @param queue queue to look at
 * @return the last entry, or NULL if the queue is empty
 */
wait_t *wait_queue_last(wait_queue_t *queue);

/**
 * @brief Tell whether a queue has no entries.
 * @param queue queue to look at
 * @return true if the queue is empty
 */
bool wait_queue_empty(wait_queue_t *queue);
/**
 * @brief Tell whether an entry is currently in a queue.
 * @param wait entry to look at
 * @return true if the entry is linked into a queue
 */
bool wait_in_queue(wait_t *wait);
/**
 * @brief Wake up the process of one entry.
 * @param queue        queue that holds @p wait
 * @param wait         entry whose process is woken up
 * @param wakeup_flags value stored in wait->wakeup_flags: the reason for the wake-up
 * @param del          if true the entry is also removed from @p queue
 * @note The process is only made runnable (wakeup_proc()); it does not run before the next schedule().
 */
void wakeup_wait(wait_queue_t *queue, wait_t *wait, uint32_t wakeup_flags, bool del);
/**
 * @brief Wake up the process of the first entry of a queue, if there is one.
 * @param queue        queue to take the entry from
 * @param wakeup_flags reason for the wake-up, see wakeup_wait()
 * @param del          if true the entry is removed from the queue
 */
void wakeup_first(wait_queue_t *queue, uint32_t wakeup_flags, bool del);
/**
 * @brief Wake up the processes of all entries of a queue.
 * @param queue        queue to wake up
 * @param wakeup_flags reason for the wake-up, see wakeup_wait()
 * @param del          if true every entry is removed from the queue
 */
void wakeup_queue(wait_queue_t *queue, uint32_t wakeup_flags, bool del);

/**
 * @brief Put the current process to sleep on a queue.
 *
 * Initialises @p wait for the current process, marks the process PROC_SLEEPING with
 * wait_state @p wait_state, and adds @p wait to @p queue. The process does not give up the CPU
 * here: the caller calls schedule() afterwards, once interrupts are enabled again. The caller keeps
 * interrupts off around this call (see __down() in sem.c).
 *
 * @param queue      queue to sleep on
 * @param wait       entry to use; its storage must stay valid until the process is woken up
 * @param wait_state wait state of the process, for example WT_KSEM
 */
void wait_current_set(wait_queue_t *queue, wait_t *wait, uint32_t wait_state);

/**
 * @brief Remove an entry from its queue if it is still in one.
 *
 * Used by a process after it has been woken up: if the waker already removed the entry (the normal
 * case) nothing happens, otherwise (for example a wake-up by do_kill) the entry is removed here.
 * @param queue the queue the entry was added to
 * @param wait  the entry of the current process
 */
#define wait_current_del(queue, wait)    \
    do {                                 \
        if (wait_in_queue(wait)) {       \
            wait_queue_del(queue, wait); \
        }                                \
    } while (0)

#endif /* !__KERN_SYNC_WAIT_H__ */
