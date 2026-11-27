/**
 * @file   wait.c
 * @brief  Wait queues: the list operations on wait_t entries and the wake-up helpers.
 *
 * The low-level functions (wait_init, wait_queue_*) only maintain the circular list. The wake-up
 * helpers (wakeup_wait, wakeup_first, wakeup_queue) store the wake-up reason and call
 * wakeup_proc() from sched.c. wait_current_set() is the one function that changes the state of
 * the current process. See wait.h for the data structures.
 */
#include <defs.h>
#include <list.h>
#include <sync.h>
#include <wait.h>
#include <proc.h>

/**
 * @brief Initialise a wait entry for a process.
 * @param wait entry to initialise; self-linked (not in any queue) afterwards
 * @param proc process that will sleep on this entry
 * @note wakeup_flags starts as WT_INTERRUPTED, which means "not woken up normally yet".
 */
void wait_init(wait_t *wait, struct proc_struct *proc) {
    wait->proc = proc;
    wait->wakeup_flags = WT_INTERRUPTED;
    list_init(&(wait->wait_link));
}

/**
 * @brief Initialise an empty wait queue.
 * @param queue queue to initialise
 */
void wait_queue_init(wait_queue_t *queue) {
    list_init(&(queue->wait_head));
}

/**
 * @brief Add an entry at the tail of a queue.
 * @param queue queue to insert into
 * @param wait  entry to insert; it must belong to a process and must not be in a queue
 * @note list_add_before() on the head inserts at the tail of the circular list, so entries leave the
 *       queue in the order they came in. The assert catches an entry that is already queued.
 */
void wait_queue_add(wait_queue_t *queue, wait_t *wait) {
    assert(list_empty(&(wait->wait_link)) && wait->proc != NULL);
    wait->wait_queue = queue;
    list_add_before(&(queue->wait_head), &(wait->wait_link));
}

/**
 * @brief Remove an entry from the queue it is in.
 * @param queue the queue that holds the entry
 * @param wait  entry to remove
 * @note list_del_init() leaves the entry self-linked, so wait_in_queue() is false afterwards.
 */
void wait_queue_del(wait_queue_t *queue, wait_t *wait) {
    assert(!list_empty(&(wait->wait_link)) && wait->wait_queue == queue);
    list_del_init(&(wait->wait_link));
}

/**
 * @brief Get the entry behind @p wait.
 * @param queue queue that holds @p wait
 * @param wait  an entry in @p queue
 * @return the next entry, or NULL if @p wait is the last one
 */
wait_t *wait_queue_next(wait_queue_t *queue, wait_t *wait) {
    assert(!list_empty(&(wait->wait_link)) && wait->wait_queue == queue);
    list_entry_t *le = list_next(&(wait->wait_link));
    if (le != &(queue->wait_head)) {
        return le2wait(le, wait_link);
    }
    return NULL;
}

/**
 * @brief Get the entry in front of @p wait.
 * @param queue queue that holds @p wait
 * @param wait  an entry in @p queue
 * @return the previous entry, or NULL if @p wait is the first one
 */
wait_t *wait_queue_prev(wait_queue_t *queue, wait_t *wait) {
    assert(!list_empty(&(wait->wait_link)) && wait->wait_queue == queue);
    list_entry_t *le = list_prev(&(wait->wait_link));
    if (le != &(queue->wait_head)) {
        return le2wait(le, wait_link);
    }
    return NULL;
}

/**
 * @brief Get the first entry of a queue (the one that has waited longest).
 * @param queue queue to look at
 * @return the first entry, or NULL if the queue is empty
 */
wait_t *wait_queue_first(wait_queue_t *queue) {
    list_entry_t *le = list_next(&(queue->wait_head));
    if (le != &(queue->wait_head)) {
        return le2wait(le, wait_link);
    }
    return NULL;
}

/**
 * @brief Get the last entry of a queue.
 * @param queue queue to look at
 * @return the last entry, or NULL if the queue is empty
 */
wait_t *wait_queue_last(wait_queue_t *queue) {
    list_entry_t *le = list_prev(&(queue->wait_head));
    if (le != &(queue->wait_head)) {
        return le2wait(le, wait_link);
    }
    return NULL;
}

/**
 * @brief Tell whether a queue has no entries.
 * @param queue queue to look at
 * @return true if the queue is empty
 */
bool wait_queue_empty(wait_queue_t *queue) {
    return list_empty(&(queue->wait_head));
}

/**
 * @brief Tell whether an entry is currently in a queue.
 * @param wait entry to look at
 * @return true if the entry is linked into a queue (not self-linked)
 */
bool wait_in_queue(wait_t *wait) {
    return !list_empty(&(wait->wait_link));
}

/**
 * @brief Wake up the process of one entry.
 * @param queue        queue that holds @p wait
 * @param wait         entry whose process is woken up
 * @param wakeup_flags value stored in wait->wakeup_flags: the reason for the wake-up
 * @param del          if true the entry is also removed from @p queue
 * @note The process is only made runnable by wakeup_proc(); it does not run before the next schedule().
 */
void wakeup_wait(wait_queue_t *queue, wait_t *wait, uint32_t wakeup_flags, bool del) {
    if (del) {
        wait_queue_del(queue, wait);
    }
    wait->wakeup_flags = wakeup_flags;
    wakeup_proc(wait->proc);
}

/**
 * @brief Wake up the process of the first entry of a queue, if there is one.
 * @param queue        queue to take the entry from
 * @param wakeup_flags reason for the wake-up, see wakeup_wait()
 * @param del          if true the entry is removed from the queue
 */
void wakeup_first(wait_queue_t *queue, uint32_t wakeup_flags, bool del) {
    wait_t *wait;
    if ((wait = wait_queue_first(queue)) != NULL) {
        wakeup_wait(queue, wait, wakeup_flags, del);
    }
}

/**
 * @brief Wake up the processes of all entries of a queue.
 * @param queue        queue to wake up
 * @param wakeup_flags reason for the wake-up, see wakeup_wait()
 * @param del          if true every entry is removed from the queue
 * @note With del the entries are taken from the front until the queue is empty. Without del they
 *       stay in the queue, so the loop walks forward with wait_queue_next() instead.
 */
void wakeup_queue(wait_queue_t *queue, uint32_t wakeup_flags, bool del) {
    wait_t *wait;
    if ((wait = wait_queue_first(queue)) != NULL) {
        if (del) {
            do {
                wakeup_wait(queue, wait, wakeup_flags, 1);
            } while ((wait = wait_queue_first(queue)) != NULL);
        } else {
            do {
                wakeup_wait(queue, wait, wakeup_flags, 0);
            } while ((wait = wait_queue_next(queue, wait)) != NULL);
        }
    }
}

/**
 * @brief Put the current process to sleep on a queue.
 *
 * Initialises @p wait for the current process, marks the process PROC_SLEEPING with
 * wait_state @p wait_state, and adds @p wait to @p queue.
 *
 * @param queue      queue to sleep on
 * @param wait       entry to use; its storage must stay valid until the process is woken up
 * @param wait_state wait state of the process, for example WT_KSEM
 * @note Only prepares the sleep. The process keeps running until its caller calls schedule(). The
 *       caller keeps interrupts off around this call, so that no wake-up can slip in between the
 *       test of the condition and the queueing (see __down() in sem.c).
 */
void wait_current_set(wait_queue_t *queue, wait_t *wait, uint32_t wait_state) {
    assert(current != NULL);
    wait_init(wait, current);
    current->state = PROC_SLEEPING;
    current->wait_state = wait_state;
    wait_queue_add(queue, wait);
}
