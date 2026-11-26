/**
 * @file   deadlock.c
 * @brief  Detect re-entry and circular wait on mutex semaphores (sem_init value 1).
 *
 * A mutex records the pid that holds it. down() asks this file before sleeping, and the wait is
 * refused when it would re-enter the same mutex or close a cycle. The default report prints a
 * line and enters kmonitor(); deadlock_check() swaps that hook out so the boot test can finish.
 */
#include <assert.h>
#include <deadlock.h>
#include <intr.h>
#include <kmonitor.h>
#include <proc.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <wait.h>

/** @brief Capacity of the wait-edge table. Not sized from MAX_PROCESS. */
#define DL_MAX_EDGES 32

/**
 * @brief One "waiter is blocked on the mutex held by holder" record.
 */
struct dl_edge {
    int waiter;
    int holder;
    semaphore_t *sem;
};

/** @brief Wait edges currently installed. A process sleeps on at most one mutex. */
static struct dl_edge dl_edges[DL_MAX_EDGES];
/** @brief Number of valid entries at the front of dl_edges. */
static int dl_nedges;

/** @brief Report hook. The default enters the monitor; the self-test replaces it. */
static void dl_default_report(const char *msg);
static void (*dl_report)(const char *msg) = dl_default_report;

static void dl_default_report(const char *msg) {
    cprintf("deadlock report: %s\n", msg);
    /* kmonitor reads the console; leave the detector's interrupt-off section first. */
    intr_enable();
    kmonitor(NULL);
}

/**
 * @brief Drop every edge for which @p drop returns true. The callback sees one edge.
 */
static void dl_remove_if(int (*drop)(struct dl_edge *e, void *arg), void *arg) {
    int i = 0;
    while (i < dl_nedges) {
        if (drop(&dl_edges[i], arg)) {
            dl_edges[i] = dl_edges[--dl_nedges];
        } else {
            i++;
        }
    }
}

/** @brief Match an edge that names @p pid as either endpoint. */
static int dl_related_to(struct dl_edge *e, void *arg) {
    int pid = *(int *)arg;
    return e->waiter == pid || e->holder == pid;
}

struct dl_hold_sem {
    int pid;
    semaphore_t *sem;
};

/** @brief Match an edge whose holder is releasing this semaphore. */
static int dl_held_sem(struct dl_edge *e, void *arg) {
    struct dl_hold_sem *key = arg;
    return e->holder == key->pid && e->sem == key->sem;
}

/**
 * @brief Follow waiter -> holder and report a cycle that starts at @p start.
 * @return 1 if a cycle exists; the pids on it are written to @p out
 */
static int dl_find_cycle(int start, int *out, int *out_n) {
    int path[DL_MAX_EDGES];
    int n = 0;
    int v = start;

    while (n < DL_MAX_EDGES) {
        int i;
        for (i = 0; i < n; i++) {
            if (path[i] == v) {
                int len = 0;
                for (; i < n; i++) {
                    out[len++] = path[i];
                }
                *out_n = len;
                return 1;
            }
        }
        path[n++] = v;
        {
            int next = -1;
            for (i = 0; i < dl_nedges; i++) {
                if (dl_edges[i].waiter == v) {
                    next = dl_edges[i].holder;
                    break;
                }
            }
            if (next < 0) {
                return 0;
            }
            v = next;
        }
    }
    return 0;
}

/** @brief Print the cycle and the four conditions that are present with it. */
static void dl_report_cycle(int *pids, int n) {
    int i;
    cprintf("deadlock: cycle");
    for (i = 0; i < n; i++) {
        cprintf(" pid %d ->", pids[i]);
    }
    cprintf(" pid %d\n", pids[0]);
    /* mutex: only sem_init(, 1) is tracked. hold-and-wait: the cycle is longer than one
     * process, so each waiter already holds the mutex the previous process sleeps on.
     * no-preempt: owner is cleared only by that holder's up(). circular-wait: the cycle. */
    cprintf("deadlock: mutex=1 hold-and-wait=%d no-preempt=1 circular-wait=1\n", n > 1);
    dl_report("deadlock cycle");
}

void deadlock_on_acquire(semaphore_t *sem) {
    if (sem == NULL || !sem->is_mutex || current == NULL) {
        return;
    }
    if (sem->owner == 0) {
        sem->owner = current->pid;
        return;
    }
    if (sem->owner == current->pid) {
        cprintf("deadlock: reenter pid %d\n", current->pid);
        dl_report("reenter");
        return;
    }
    cprintf("deadlock: multi-enter holder %d pid %d\n", sem->owner, current->pid);
    dl_report("multi-enter");
}

int deadlock_on_wait(semaphore_t *sem) {
    int idx;
    int cycle[DL_MAX_EDGES];
    int cycle_n = 0;
    int pid;

    if (sem == NULL || !sem->is_mutex || sem->owner == 0 || current == NULL) {
        return 0;
    }
    if (sem->owner == current->pid) {
        pid = current->pid;
        dl_remove_if(dl_related_to, &pid);
        cprintf("deadlock: reenter pid %d\n", pid);
        dl_report("reenter");
        return 1;
    }
    if (dl_nedges >= DL_MAX_EDGES) {
        cprintf("deadlock table full\n");
        dl_report("deadlock table full");
        return 1;
    }
    idx = dl_nedges;
    dl_edges[idx].waiter = current->pid;
    dl_edges[idx].holder = sem->owner;
    dl_edges[idx].sem = sem;
    dl_nedges++;

    if (dl_find_cycle(current->pid, cycle, &cycle_n)) {
        dl_edges[idx] = dl_edges[--dl_nedges];
        dl_report_cycle(cycle, cycle_n);
        return 1;
    }
    return 0;
}

void deadlock_on_release(semaphore_t *sem) {
    struct dl_hold_sem key;
    if (sem == NULL || !sem->is_mutex || current == NULL) {
        return;
    }
    if (sem->owner != current->pid) {
        return;
    }
    key.pid = current->pid;
    key.sem = sem;
    sem->owner = 0;
    dl_remove_if(dl_held_sem, &key);
}

/* ---------------- self-test ---------------- */

static volatile int dl_saw_reenter;
static volatile int dl_saw_cycle;
static semaphore_t dl_sem_a, dl_sem_b;
static volatile int dl_t1_has_a;
static volatile int dl_t2_has_b;
static struct proc_struct *dl_t1;

static void dl_test_report(const char *msg) {
    if (strcmp(msg, "reenter") == 0) {
        dl_saw_reenter = 1;
    } else if (strcmp(msg, "deadlock cycle") == 0) {
        dl_saw_cycle = 1;
    }
}

/**
 * @brief Thread 1 of the circular-wait test: hold A, then sleep on B until thread 2 releases it.
 */
static int dl_thread1(void *arg) {
    (void)arg;
    down(&dl_sem_a);
    dl_t1_has_a = 1;
    while (!dl_t2_has_b) {
        schedule();
    }
    down(&dl_sem_b);
    up(&dl_sem_a);
    up(&dl_sem_b);
    return 0;
}

/**
 * @brief Thread 2: hold B, wait until thread 1 sleeps on B, then down(A) must see the cycle.
 */
static int dl_thread2(void *arg) {
    (void)arg;
    while (!dl_t1_has_a) {
        schedule();
    }
    down(&dl_sem_b);
    dl_t2_has_b = 1;
    while (dl_t1->state != PROC_SLEEPING) {
        schedule();
    }
    down(&dl_sem_a);
    up(&dl_sem_b);
    return 0;
}

/** @brief Reap one child created by the test. do_wait sleeps until that child has exited. */
static void dl_reap(int pid) {
    if (do_wait(pid, NULL) != 0) {
        panic("deadlock check: wait pid %d failed\n", pid);
    }
}

void deadlock_check(void) {
    semaphore_t once;
    int pid1, pid2;

    dl_report = dl_test_report;
    dl_saw_reenter = 0;

    sem_init(&once, 1);
    down(&once);
    down(&once);
    if (!dl_saw_reenter) {
        panic("deadlock check: reenter not detected\n");
    }
    up(&once);
    if (once.owner != 0 || once.value != 1) {
        panic("deadlock check: mutex not restored after reenter\n");
    }

    dl_saw_cycle = 0;
    dl_t1_has_a = 0;
    dl_t2_has_b = 0;
    sem_init(&dl_sem_a, 1);
    sem_init(&dl_sem_b, 1);
    pid1 = kernel_thread(dl_thread1, NULL, 0);
    if (pid1 <= 0) {
        panic("deadlock check: create thread 1 failed\n");
    }
    dl_t1 = find_proc(pid1);
    pid2 = kernel_thread(dl_thread2, NULL, 0);
    if (pid2 <= 0 || dl_t1 == NULL) {
        panic("deadlock check: create thread 2 failed\n");
    }
    dl_reap(pid1);
    dl_reap(pid2);

    if (!dl_saw_cycle) {
        panic("deadlock check: cycle not detected\n");
    }
    if (dl_sem_a.owner != 0 || dl_sem_a.value != 1 || dl_sem_b.owner != 0 || dl_sem_b.value != 1) {
        panic("deadlock check: cycle mutexes not restored\n");
    }
    if (!wait_queue_empty(&dl_sem_a.wait_queue) || !wait_queue_empty(&dl_sem_b.wait_queue)) {
        panic("deadlock check: a test mutex still has a sleeper\n");
    }

    dl_report = dl_default_report;
    cprintf("deadlock check ok\n");
}
