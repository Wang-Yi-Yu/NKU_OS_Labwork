/**
 * @file   test_lab7.c
 * @brief  Grading tests of lab7 (100 points): condition variables and the monitor philosophers.
 *
 * tools/inject.py copies this file to kern/test/ of a temporary copy of the student tree and makes
 * check_sync() call the two entry points at its start. Each test prints one result line, which
 * tools/grade.sh looks for. The tests only use the public interface and the fields of the monitor
 * and condition variable structures, so they work with any correct implementation:
 *  - test_lab7_condvar(): an empty cond_signal() changes nothing; two waiters on one condition
 *    variable are woken one per signal, and the woken thread owns the monitor (mutex.value 0,
 *    next_count 1); a waiter can wait on two condition variables one after the other;
 *  - test_lab7_philosopher(): phi_test_condvar() only lets a hungry philosopher eat when the
 *    neighbours are not eating; an uncontended take/put returns in the EATING/THINKING states;
 *    a philosopher blocked by either neighbour waits on its condition variable until that neighbour
 *    puts the forks down; put_forks wakes both neighbours; finally a stress run of all five
 *    philosophers is watched by an observer thread for two adjacent eating philosophers.
 */
#include <defs.h>
#include <stdio.h>
#include <proc.h>
#include <sem.h>
#include <monitor.h>
#include <test_lab7.h>

/** @brief Number of philosophers; the same as N in check_sync.c. */
#define TEST_LAB7_N 5
/** @brief Philosopher states; the same values as THINKING, HUNGRY and EATING in check_sync.c. */
#define TEST_LAB7_THINKING 0
#define TEST_LAB7_HUNGRY   1
#define TEST_LAB7_EATING   2
/** @brief Number of the left neighbour of philosopher i. */
#define TEST_LAB7_LEFT(i) (((i) + TEST_LAB7_N - 1) % TEST_LAB7_N)
/** @brief Number of the right neighbour of philosopher i. */
#define TEST_LAB7_RIGHT(i) (((i) + 1) % TEST_LAB7_N)

/** @brief The monitor and the state array of the student's philosopher code in check_sync.c. */
extern monitor_t mt;
extern int state_condvar[];
/** @brief Thread body of one philosopher of the monitor version (check_sync.c); arg is the number. */
extern int philosopher_using_condvar(void *arg);

/** @brief State shared by the condition variable test: its monitor, a semaphore that the waiters
 *         up when they are about to wait, the number of waiters that have woken up, and an error flag. */
static monitor_t test_cv_mt;
static semaphore_t test_cv_ready;
static volatile int test_cv_awake;
static volatile int test_cv_waiter_error;
/** @brief State of the hand-off test: its monitor, semaphores for "waiter started" and "waiter done", and an error flag. */
static monitor_t test_handoff_mt;
static semaphore_t test_handoff_started, test_handoff_done;
static volatile int test_handoff_error;
/** @brief State of the philosopher stress test: philosophers still running and a flag set by the observer on a violation. */
static volatile int test_phi_running;
static volatile int test_phi_bad;

/**
 * @brief Thread body of a waiter in the condition variable test.
 *
 * Enters the monitor, tells the main thread (test_cv_ready), and waits on cv[0]. When woken it
 * checks that it owns the monitor (mutex.value 0 and next_count 1: the signaler is suspended),
 * counts itself in test_cv_awake, and leaves the monitor through next so the signaler resumes.
 * @param arg unused
 * @return 0
 */
static int
test_cv_waiter(void *arg)
{
    (void)arg;

    down(&test_cv_mt.mutex);
    up(&test_cv_ready);
    cond_wait(&test_cv_mt.cv[0]);

    /* A correct Hoare monitor wakes this thread as the monitor owner. */
    if (test_cv_mt.mutex.value != 0 || test_cv_mt.next_count != 1) {
        test_cv_waiter_error = 1;
    }
    test_cv_awake++;

    /* Leave the monitor and let the blocked signaler resume. */
    if (test_cv_mt.next_count > 0) {
        up(&test_cv_mt.next);
    } else {
        up(&test_cv_mt.mutex);
    }
    return 0;
}

/**
 * @brief Thread body of the waiter in the hand-off test: waits on cv[0] and then on cv[1].
 *
 * After the second wake-up it checks that it owns the monitor, then ups next and the "done"
 * semaphore.
 * @param arg unused
 * @return 0
 */
static int
test_cv_handoff_waiter(void *arg)
{
    (void)arg;

    down(&test_handoff_mt.mutex);
    up(&test_handoff_started);
    cond_wait(&test_handoff_mt.cv[0]);
    cond_wait(&test_handoff_mt.cv[1]);

    if (test_handoff_mt.mutex.value != 0 ||
        test_handoff_mt.next_count != 1) {
        test_handoff_error = 1;
    }
    up(&test_handoff_mt.next);
    up(&test_handoff_done);
    return 0;
}

/**
 * @brief Check that a thread woken from cv[0] can wait again (on cv[1]) and then be signaled.
 *
 * The main thread signals cv[0] and expects that the woken waiter went back to wait on cv[1]
 * and handed the monitor back at once (cv[1].count 1, next_count 0, mutex.value 0), then signals
 * cv[1] and lets the waiter finish.
 * @return 1 if every check held, otherwise 0
 */
static int
test_cv_handoff(void)
{
    int pid;

    test_handoff_error = 0;
    sem_init(&test_handoff_started, 0);
    sem_init(&test_handoff_done, 0);
    monitor_init(&test_handoff_mt, 2);

    pid = kernel_thread(test_cv_handoff_waiter, NULL, 0);
    if (pid <= 0) {
        monitor_free(&test_handoff_mt, 2);
        return 0;
    }

    down(&test_handoff_started);
    down(&test_handoff_mt.mutex);
    cond_signal(&test_handoff_mt.cv[0]);

    if (test_handoff_mt.cv[1].count != 1 ||
        test_handoff_mt.next_count != 0 ||
        test_handoff_mt.mutex.value != 0) {
        test_handoff_error = 1;
    }

    cond_signal(&test_handoff_mt.cv[1]);
    up(&test_handoff_mt.mutex);
    down(&test_handoff_done);
    do_wait(pid, NULL);
    monitor_free(&test_handoff_mt, 2);
    return test_handoff_error == 0;
}

/**
 * @brief Test 1: cond_wait() and cond_signal().
 *
 * Checks, in this order: an empty cond_signal() changes no counter, semaphore value or queue;
 * two waiters give cv[0].count 2; the first signal wakes exactly one waiter and the monitor goes to
 * it (next_count back to 0 once the signaler resumes); the second signal wakes the other one; no
 * waiter saw a wrong monitor state; the hand-off test passes. Prints one result line.
 */
void
test_lab7_condvar(void)
{
    int ok = 1;
    int old_count, old_next_count, old_mutex_value;
    int old_next_value, old_cv_value, old_queue_empty;
    int pid1, pid2;

    test_cv_awake = 0;
    test_cv_waiter_error = 0;
    sem_init(&test_cv_ready, 0);
    monitor_init(&test_cv_mt, 1);

    /* Signaling an empty condition variable must be a no-op. */
    down(&test_cv_mt.mutex);
    old_count = test_cv_mt.cv[0].count;
    old_next_count = test_cv_mt.next_count;
    old_mutex_value = test_cv_mt.mutex.value;
    old_next_value = test_cv_mt.next.value;
    old_cv_value = test_cv_mt.cv[0].sem.value;
    old_queue_empty = wait_queue_empty(&test_cv_mt.cv[0].sem.wait_queue);

    cond_signal(&test_cv_mt.cv[0]);

    if (test_cv_mt.cv[0].count != old_count ||
        test_cv_mt.next_count != old_next_count ||
        test_cv_mt.mutex.value != old_mutex_value ||
        test_cv_mt.next.value != old_next_value ||
        test_cv_mt.cv[0].sem.value != old_cv_value ||
        wait_queue_empty(&test_cv_mt.cv[0].sem.wait_queue) != old_queue_empty) {
        ok = 0;
    }
    up(&test_cv_mt.mutex);

    if (!ok) {
        monitor_free(&test_cv_mt, 1);
        cprintf("[Test 1] Condition Variable: FAIL (empty signal changed state)\n");
        return;
    }

    pid1 = kernel_thread(test_cv_waiter, NULL, 0);
    pid2 = kernel_thread(test_cv_waiter, NULL, 0);
    if (pid1 <= 0 || pid2 <= 0) {
        monitor_free(&test_cv_mt, 1);
        cprintf("[Test 1] Condition Variable: FAIL (create waiter)\n");
        return;
    }

    /* Both waiters have entered cond_wait and released the monitor. */
    down(&test_cv_ready);
    down(&test_cv_ready);
    down(&test_cv_mt.mutex);
    if (test_cv_mt.cv[0].count != 2) {
        ok = 0;
    }

    /* One signal must wake exactly one waiter and transfer monitor ownership. */
    cond_signal(&test_cv_mt.cv[0]);
    if (test_cv_awake != 1 || test_cv_mt.cv[0].count != 1 ||
        test_cv_mt.next_count != 0) {
        ok = 0;
    }

    /* The second signal wakes the remaining waiter. */
    cond_signal(&test_cv_mt.cv[0]);
    if (test_cv_awake != 2 || test_cv_mt.cv[0].count != 0 ||
        test_cv_mt.next_count != 0) {
        ok = 0;
    }
    up(&test_cv_mt.mutex);

    do_wait(pid1, NULL);
    do_wait(pid2, NULL);
    if (test_cv_waiter_error) {
        ok = 0;
    }
    if (!test_cv_handoff()) {
        ok = 0;
    }
    monitor_free(&test_cv_mt, 1);

    if (ok) {
        cprintf("[Test 1] Condition Variable: PASS (50/50)\n");
    } else {
        cprintf("[Test 1] Condition Variable: FAIL (wake/count/monitor state)\n");
    }
}

/** @brief The student's three philosopher functions, and two semaphores for the blocked-take tests. */
static test_lab7_phi_fn_t test_phi_test_fn;
static test_lab7_phi_fn_t test_phi_take_fn;
static test_lab7_phi_fn_t test_phi_put_fn;
static semaphore_t test_phi_started, test_phi_finished;

/**
 * @brief Set the state of every philosopher to @p state.
 * @param state THINKING, HUNGRY or EATING
 */
static void
test_phi_set_all(int state)
{
    int i;
    for (i = 0; i < TEST_LAB7_N; i++) {
        state_condvar[i] = state;
    }
}

/**
 * @brief Tell whether two neighbouring philosophers are eating at the same time.
 * @return 1 if such a pair exists, otherwise 0
 */
static int
test_phi_adjacent_eating(void)
{
    int i;
    for (i = 0; i < TEST_LAB7_N; i++) {
        if (state_condvar[i] == TEST_LAB7_EATING &&
            state_condvar[TEST_LAB7_RIGHT(i)] == TEST_LAB7_EATING) {
            return 1;
        }
    }
    return 0;
}

/**
 * @brief Thread body of a philosopher in the stress test: the student's philosopher loop.
 * @param arg philosopher number, cast to a pointer
 * @return 0
 */
static int
test_phi_stress_thread(void *arg)
{
    philosopher_using_condvar(arg);
    test_phi_running--;
    return 0;
}

/**
 * @brief Thread body of the observer: checks the states every tick while philosophers run.
 *
 * Sets test_phi_bad if two neighbours are ever eating at the same time.
 * @param arg unused
 * @return 0
 */
static int
test_phi_observer(void *arg)
{
    (void)arg;

    while (test_phi_running > 0) {
        if (test_phi_adjacent_eating()) {
            test_phi_bad = 1;
        }
        do_sleep(1);
    }
    return 0;
}

/**
 * @brief Run all five philosophers (philosopher_using_condvar) together with the observer.
 * @return 1 if every thread was created and no violation was seen, otherwise 0
 */
static int
test_phi_stress(void)
{
    int pids[TEST_LAB7_N];
    int observer_pid;
    int i, ok = 1;

    test_phi_running = TEST_LAB7_N;
    test_phi_bad = 0;
    test_phi_set_all(TEST_LAB7_THINKING);

    observer_pid = kernel_thread(test_phi_observer, NULL, 0);
    if (observer_pid <= 0) {
        return 0;
    }

    for (i = 0; i < TEST_LAB7_N; i++) {
        pids[i] = kernel_thread(test_phi_stress_thread, (void *)i, 0);
        if (pids[i] <= 0) {
            test_phi_running = 0;
            ok = 0;
            break;
        }
    }

    for (i = 0; i < TEST_LAB7_N && pids[i] > 0; i++) {
        do_wait(pids[i], NULL);
    }
    do_wait(observer_pid, NULL);

    if (test_phi_bad || test_phi_adjacent_eating()) {
        ok = 0;
    }
    return ok;
}

/**
 * @brief Thread body that calls take_forks for philosopher 0; it ups test_phi_started first.
 * @param arg unused
 * @return 0
 */
static int
test_phi_blocked_take(void *arg)
{
    (void)arg;
    up(&test_phi_started);
    test_phi_take_fn(0);
    up(&test_phi_finished);
    return 0;
}

/**
 * @brief Check that philosopher 0 waits while neighbour @p blocker eats, and wakes up when it stops.
 *
 * Marks @p blocker EATING and starts a thread that calls take_forks(0). The thread must be HUNGRY
 * and waiting on mt.cv[0] (count 1). Then put_forks(blocker) is called; philosopher 0 must
 * end up EATING with no two neighbours eating.
 * @param blocker the left or right neighbour of philosopher 0
 * @return 1 if every check held, otherwise 0
 */
static int
test_phi_blocked_by(int blocker)
{
    int ok = 1;
    int pid;

    test_phi_set_all(TEST_LAB7_THINKING);
    state_condvar[blocker] = TEST_LAB7_EATING;
    sem_init(&test_phi_started, 0);
    sem_init(&test_phi_finished, 0);
    pid = kernel_thread(test_phi_blocked_take, NULL, 0);
    if (pid <= 0) {
        return 0;
    }

    down(&test_phi_started);
    do_sleep(2);
    if (state_condvar[0] != TEST_LAB7_HUNGRY || mt.cv[0].count != 1) {
        ok = 0;
    }

    test_phi_put_fn(blocker);
    down(&test_phi_finished);
    do_wait(pid, NULL);
    if (state_condvar[0] != TEST_LAB7_EATING || test_phi_adjacent_eating()) {
        ok = 0;
    }
    return ok;
}

/**
 * @brief Test 2: phi_test_condvar(), phi_take_forks_condvar() and phi_put_forks_condvar().
 *
 * Checks, in this order: phi_test only promotes a HUNGRY philosopher whose neighbours do not eat;
 * an uncontended take/put gives EATING then THINKING; blocking by the left and by the right
 * neighbour (test_phi_blocked_by); put_forks wakes both hungry neighbours; the stress run.
 * Prints one result line.
 * @param test_fn phi_test_condvar of the student
 * @param take_fn phi_take_forks_condvar of the student
 * @param put_fn  phi_put_forks_condvar of the student
 */
void
test_lab7_philosopher(test_lab7_phi_fn_t test_fn,
                      test_lab7_phi_fn_t take_fn,
                      test_lab7_phi_fn_t put_fn)
{
    int ok = 1;

    test_phi_test_fn = test_fn;
    test_phi_take_fn = take_fn;
    test_phi_put_fn = put_fn;
    monitor_init(&mt, TEST_LAB7_N);
    test_phi_set_all(TEST_LAB7_THINKING);

    /* phi_test_condvar must only promote HUNGRY when both forks are free. */
    down(&mt.mutex);
    state_condvar[0] = TEST_LAB7_HUNGRY;
    state_condvar[1] = TEST_LAB7_EATING;
    test_phi_test_fn(0);
    if (state_condvar[0] != TEST_LAB7_HUNGRY) {
        ok = 0;
    }

    state_condvar[0] = TEST_LAB7_THINKING;
    state_condvar[1] = TEST_LAB7_THINKING;
    test_phi_test_fn(0);
    if (state_condvar[0] != TEST_LAB7_THINKING) {
        ok = 0;
    }

    state_condvar[0] = TEST_LAB7_HUNGRY;
    test_phi_test_fn(0);
    if (state_condvar[0] != TEST_LAB7_EATING) {
        ok = 0;
    }
    up(&mt.mutex);

    /* The uncontended take/put path must return in EATING/THINKING states. */
    test_phi_set_all(TEST_LAB7_THINKING);
    test_phi_take_fn(0);
    if (state_condvar[0] != TEST_LAB7_EATING || test_phi_adjacent_eating()) {
        ok = 0;
    }
    test_phi_put_fn(0);
    if (state_condvar[0] != TEST_LAB7_THINKING || test_phi_adjacent_eating()) {
        ok = 0;
    }

    /* Exercise both neighbour checks in phi_put_forks_condvar(). */
    if (!test_phi_blocked_by(TEST_LAB7_LEFT(0)) ||
        !test_phi_blocked_by(TEST_LAB7_RIGHT(0))) {
        ok = 0;
    }

    test_phi_set_all(TEST_LAB7_THINKING);
    state_condvar[0] = TEST_LAB7_EATING;
    state_condvar[TEST_LAB7_LEFT(0)] = TEST_LAB7_HUNGRY;
    state_condvar[TEST_LAB7_RIGHT(0)] = TEST_LAB7_HUNGRY;
    test_phi_put_fn(0);
    if (state_condvar[TEST_LAB7_LEFT(0)] != TEST_LAB7_EATING ||
        state_condvar[TEST_LAB7_RIGHT(0)] != TEST_LAB7_EATING ||
        test_phi_adjacent_eating()) {
        ok = 0;
    }

    if (!test_phi_stress()) {
        ok = 0;
    }

    test_phi_set_all(TEST_LAB7_THINKING);
    monitor_free(&mt, TEST_LAB7_N);

    if (ok) {
        cprintf("[Test 2] Philosopher Monitor: PASS (50/50)\n");
    } else {
        cprintf("[Test 2] Philosopher Monitor: FAIL (state/wait/signal)\n");
    }
}
