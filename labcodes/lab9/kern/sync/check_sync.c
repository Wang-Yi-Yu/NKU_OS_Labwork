/**
 * @file   check_sync.c
 * @brief  The dining philosophers problem (lab7): one solution with semaphores, which is provided,
 *         and one with a monitor and condition variables, which is the exercise of this file.
 *
 * Five philosophers sit around a table with five forks, one between each two of them. Each of them
 * thinks, becomes hungry, needs both neighbouring forks to eat, and then puts the forks down. The
 * state of every philosopher is kept in an array (state_sema[], state_condvar[]); a philosopher may
 * only start eating if neither neighbour is eating, so no two neighbours eat at the same time.
 * Every philosopher runs as a kernel thread, created by check_sync() and reaped with do_wait().
 *
 * check_sync() runs first the semaphore version and then the monitor version. The monitor version
 * uses the three functions phi_test_condvar(), phi_take_forks_condvar() and phi_put_forks_condvar()
 * (marker `lab7 function2`, the exercise of this file), which are built on cond_wait() and
 * cond_signal() from monitor.c. check_sync() is called from init_main() in proc.c. The grading run
 * injects the tests of tools/test_lab7.c at the start of check_sync() (see tools/inject.py).
 */
#include <stdio.h>
#include <proc.h>
#include <sem.h>
#include <monitor.h>
#include <assert.h>

/** @brief Number of philosophers (and of forks). */
#define N 5
/** @brief Number of the left neighbour of philosopher i; uses the variable `i` of the caller. */
#define LEFT (i - 1 + N) % N
/** @brief Number of the right neighbour of philosopher i; uses the variable `i` of the caller. */
#define RIGHT (i + 1) % N
/** @brief State of a philosopher who is thinking. */
#define THINKING 0
/** @brief State of a philosopher who is hungry and wants to take the forks. */
#define HUNGRY 1
/** @brief State of a philosopher who is eating (holds both forks). */
#define EATING 2
/** @brief Number of meals each philosopher eats before the thread quits. */
#define TIMES 4
/** @brief Length of one thinking or eating period in timer ticks (10 ms each), for do_sleep(). */
#define SLEEP_TIME 10

//-----------------philosopher problem using monitor ------------
/* The pseudo code that the semaphore version below follows (written by the TA).
PSEUDO CODE :philosopher problem using semaphore
system DINING_PHILOSOPHERS

VAR
me:    semaphore, initially 1;                    # for mutual exclusion 
s[5]:  semaphore s[5], initially 0;               # for synchronization 
pflag[5]: {THINK, HUNGRY, EAT}, initially THINK;  # philosopher flag 

# As before, each philosopher is an endless cycle of thinking and eating.

procedure philosopher(i)
  {
    while TRUE do
     {
       THINKING;
       take_chopsticks(i);
       EATING;
       drop_chopsticks(i);
     }
  }

# The take_chopsticks procedure involves checking the status of neighboring 
# philosophers and then declaring one's own intention to eat. This is a two-phase 
# protocol; first declaring the status HUNGRY, then going on to EAT.

procedure take_chopsticks(i)
  {
    DOWN(me);               # critical section 
    pflag[i] := HUNGRY;
    test[i];
    UP(me);                 # end critical section 
    DOWN(s[i])              # Eat if enabled 
   }

void test(i)                # Let phil[i] eat, if waiting 
  {
    if ( pflag[i] == HUNGRY
      && pflag[i-1] != EAT
      && pflag[i+1] != EAT)
       then
        {
          pflag[i] := EAT;
          UP(s[i])
         }
    }


# Once a philosopher finishes eating, all that remains is to relinquish the 
# resources---its two chopsticks---and thereby release waiting neighbors.

void drop_chopsticks(int i)
  {
    DOWN(me);                # critical section 
    test(i-1);               # Let phil. on left eat if possible 
    test(i+1);               # Let phil. on rght eat if possible 
    UP(me);                  # up critical section 
   }

*/
//---------- philosophers problem using semaphore ----------------------
/** @brief State (THINKING, HUNGRY or EATING) of each philosopher in the semaphore version. */
int state_sema[N];
/** @brief Mutex (initial value 1) that protects state_sema[]; makes test and change atomic. */
semaphore_t mutex;
/** @brief One semaphore per philosopher (initial value 0): philosopher i sleeps on s[i] to eat. */
semaphore_t s[N];

/** @brief PCBs of the philosopher threads of the semaphore version, filled in by check_sync(). */
struct proc_struct *philosopher_proc_sema[N];

/**
 * @brief Let philosopher i eat if it is hungry and neither neighbour is eating (semaphore version).
 *
 * If the condition holds the state becomes EATING and s[i] is upped, which either lets
 * philosopher i pass its own down(&s[i]) at once or wakes it up if it already sleeps there.
 * @param i number of the philosopher, 0 to N-1
 * @note Must be called with `mutex` held.
 */
void phi_test_sema(int i) /* i：哲学家号码从0到N-1 */
{
    if (state_sema[i] == HUNGRY && state_sema[LEFT] != EATING && state_sema[RIGHT] != EATING) {
        state_sema[i] = EATING;
        up(&s[i]);
    }
}

/**
 * @brief Philosopher i tries to take both forks; returns when it may eat (semaphore version).
 *
 * Enters the critical section, declares itself HUNGRY, tests whether it can eat, leaves the
 * critical section, and then sleeps on s[i] if the test did not let it eat.
 * @param i number of the philosopher, 0 to N-1
 */
void phi_take_forks_sema(int i) /* i：哲学家号码从0到N-1 */
{
    down(&mutex);           /* 进入临界区 */
    state_sema[i] = HUNGRY; /* 记录下哲学家i饥饿的事实 */
    phi_test_sema(i);       /* 试图得到两只叉子 */
    up(&mutex);             /* 离开临界区 */
    down(&s[i]);            /* 如果得不到叉子就阻塞 */
}

/**
 * @brief Philosopher i puts both forks down (semaphore version).
 *
 * Enters the critical section, sets its state to THINKING, tests the left and then the right
 * neighbour (either may have been waiting for a fork of philosopher i), and leaves the critical
 * section.
 * @param i number of the philosopher, 0 to N-1
 */
void phi_put_forks_sema(int i) /* i：哲学家号码从0到N-1 */
{
    down(&mutex);             /* 进入临界区 */
    state_sema[i] = THINKING; /* 哲学家进餐结束 */
    phi_test_sema(LEFT);      /* 看一下左邻居现在是否能进餐 */
    phi_test_sema(RIGHT);     /* 看一下右邻居现在是否能进餐 */
    up(&mutex);               /* 离开临界区 */
}

/**
 * @brief Thread body of one philosopher in the semaphore version.
 *
 * Repeats TIMES times: think (do_sleep()), take the forks, eat (do_sleep()), put the forks
 * down. Prints one line for every step and a line when it quits.
 * @param arg the number of the philosopher, 0 to N-1, cast to a pointer
 * @return 0
 */
int philosopher_using_semaphore(void *arg) /* i：哲学家号码，从0到N-1 */
{
    int i, iter = 0;
    i = (int)arg;
    cprintf("I am No.%d philosopher_sema\n", i);
    while (iter++ < TIMES) {                                               /* 无限循环 */
        cprintf("Iter %d, No.%d philosopher_sema is thinking\n", iter, i); /* 哲学家正在思考 */
        do_sleep(SLEEP_TIME);
        phi_take_forks_sema(i);
        /* 需要两只叉子，或者阻塞 */
        cprintf("Iter %d, No.%d philosopher_sema is eating\n", iter, i); /* 进餐 */
        do_sleep(SLEEP_TIME);
        phi_put_forks_sema(i);
        /* 把两把叉子同时放回桌子 */
    }
    cprintf("No.%d philosopher_sema quit\n", i);
    return 0;
}

//-----------------philosopher problem using monitor ------------


/** @brief PCBs of the philosopher threads of the monitor version, filled in by check_sync(). */
struct proc_struct *philosopher_proc_condvar[N];  // N philosopher
/** @brief State (THINKING, HUNGRY or EATING) of each philosopher; only used inside the monitor. */
int state_condvar[N];  // the philosopher's state: EATING, HUNGARY, THINKING
/**
 * @brief The monitor of the monitor version, and a pointer to it (@c mtp).
 *
 * Its mutex makes the philosopher routines mutually exclusive, and mt.cv[i] is the condition
 * variable that philosopher i waits on until it may eat. It has N condition variables, created by
 * monitor_init() in check_sync().
 */
monitor_t mt, *mtp = &mt;  // monitor

//lab7 function2 2411622

/**
 * @brief Leave the monitor: hand it to a suspended signaler, or unlock it.
 *
 * Every routine of the monitor ends with this call. If a signaling process sleeps on mt.next
 * (next_count > 0) it is woken up and gets the monitor directly, which keeps its priority over the
 * processes that wait to enter. Otherwise mt.mutex is upped so that a new process can enter.
 * @note Must be the last step of a monitor routine, called by the process that holds the monitor.
 */
static void monitor_leave_condvar(void) {
    if (mtp->next_count > 0) {
        up(&(mtp->next));
    } else {
        up(&(mtp->mutex));
    }
}

/**
 * @brief Let philosopher i eat if it is hungry and neither neighbour is eating (monitor version).
 *
 * If the condition holds the state becomes EATING and cond_signal() is called on mt.cv[i]. That
 * wakes philosopher i if it already waits there. If it has not started to wait yet the signal is
 * lost, which is harmless because phi_take_forks_condvar() sees the state EATING and does not wait.
 * @param i number of the philosopher, 0 to N-1
 * @note Must be called inside the monitor. When a waiter is woken, the caller sleeps in
 *       cond_signal() until that philosopher leaves the monitor or waits again.
 */
static void phi_test_condvar(int i) {
    if (state_condvar[i] == HUNGRY && state_condvar[LEFT] != EATING &&
        state_condvar[RIGHT] != EATING) {
        state_condvar[i] = EATING;
        cond_signal(&(mtp->cv[i]));
    }
}

/**
 * @brief Philosopher i tries to take both forks; returns when it may eat (monitor version).
 *
 * Enters the monitor (down(&mt.mutex)), declares itself HUNGRY and tests whether it can eat. If the
 * state did not become EATING it waits on mt.cv[i] with cond_wait(), which gives the monitor away
 * while it sleeps. Once it is EATING it leaves the monitor.
 * @param i number of the philosopher, 0 to N-1
 * @note `if` is enough around cond_wait(): the signaler hands the monitor over directly, so the
 *       state is EATING when cond_wait() returns.
 */
static void phi_take_forks_condvar(int i) {
    down(&(mtp->mutex));
    state_condvar[i] = HUNGRY;
    phi_test_condvar(i);
    if (state_condvar[i] != EATING) {
        cond_wait(&(mtp->cv[i]));
    }
    monitor_leave_condvar();
}

/**
 * @brief Philosopher i puts both forks down (monitor version).
 *
 * Enters the monitor, sets its state to THINKING, tests the left and then the right neighbour
 * (either may be waiting for a fork of philosopher i), which signals a neighbour that can now eat,
 * and leaves the monitor.
 * @param i number of the philosopher, 0 to N-1
 */
static void phi_put_forks_condvar(int i) {
    down(&(mtp->mutex));
    state_condvar[i] = THINKING;
    phi_test_condvar(LEFT);
    phi_test_condvar(RIGHT);
    monitor_leave_condvar();
}


//---------- philosophers using monitor (condition variable) ----------------------
/**
 * @brief Thread body of one philosopher in the monitor version.
 *
 * Repeats TIMES times: think (do_sleep()), take the forks with phi_take_forks_condvar(), eat
 * (do_sleep()), put the forks down with phi_put_forks_condvar(). Prints one line for every step
 * and a line when it quits.
 * @param arg the number of the philosopher, 0 to N-1, cast to a pointer
 * @return 0
 */
int philosopher_using_condvar(void *arg) { /* arg is the No. of philosopher 0~N-1*/

    int i, iter = 0;
    i = (int)arg;
    cprintf("I am No.%d philosopher_condvar\n", i);
    while (iter++ < TIMES) {                                                  /* iterate*/
        cprintf("Iter %d, No.%d philosopher_condvar is thinking\n", iter, i); /* thinking*/
        do_sleep(SLEEP_TIME);
        phi_take_forks_condvar(i);
        /* need two forks, maybe blocked */
        cprintf("Iter %d, No.%d philosopher_condvar is eating\n", iter, i); /* eating*/
        do_sleep(SLEEP_TIME);
        phi_put_forks_condvar(i);
        /* return two forks back*/
    }
    cprintf("No.%d philosopher_condvar quit\n", i);
    return 0;
}

/**
 * @brief Run the dining philosophers with semaphores and then with the monitor.
 *
 * Semaphore version: initialises mutex and s[], creates N philosopher threads and waits for each of
 * them. Monitor version: initialises mt with N condition variables, sets every state to THINKING,
 * creates N philosopher threads, waits for each of them, and frees the monitor. Every thread is
 * reaped, so the process table is as before when the function returns.
 * @note Called by init_main() (proc.c). The grading build adds the lab7 tests at the start of
 *       this function.
 */
void check_sync(void) {
    int i, pids[N];

    //check semaphore
    sem_init(&mutex, 1);
    for (i = 0; i < N; i++) {
        sem_init(&s[i], 0);
        int pid = kernel_thread(philosopher_using_semaphore, (void *)i, 0);
        if (pid <= 0) {
            panic("create No.%d philosopher_using_semaphore failed.\n");
        }
        pids[i] = pid;
        philosopher_proc_sema[i] = find_proc(pid);
        set_proc_name(philosopher_proc_sema[i], "philosopher_sema_proc");
    }
    for (i = 0; i < N; i++)
        assert(do_wait(pids[i], NULL) == 0);

    //check condition variable
    monitor_init(&mt, N);
    for (i = 0; i < N; i++) {
        state_condvar[i] = THINKING;
        int pid = kernel_thread(philosopher_using_condvar, (void *)i, 0);
        if (pid <= 0) {
            panic("create No.%d philosopher_using_condvar failed.\n");
        }
        pids[i] = pid;
        philosopher_proc_condvar[i] = find_proc(pid);
        set_proc_name(philosopher_proc_condvar[i], "philosopher_condvar_proc");
    }
    for (i = 0; i < N; i++)
        assert(do_wait(pids[i], NULL) == 0);
    monitor_free(&mt, N);
}
