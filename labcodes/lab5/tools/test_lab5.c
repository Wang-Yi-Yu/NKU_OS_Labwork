/**
 * @file   test_lab5.c
 * @brief  Lab5 grading tests: process creation, the syscall dispatcher and process control.
 *
 * These tests are not part of the student tree. tools/inject.py copies this file into kern/test/
 * of a temporary grading copy and calls the three test functions from proc_init() and init_main().
 * Each function uses assert(), so a failure panics. On success it prints a "[Test <name>] PASS"
 * line, which tools/grade.sh looks for.
 */

#include <defs.h>
#include <assert.h>
#include <error.h>
#include <proc.h>
#include <trap.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <riscv.h>

/** @brief Trap entry used by the syscall test; defined in kern/trap/trap.c. */
extern void exception_handler(struct trapframe *tf);

/**
 * @brief Print the "[Test <name>] PASS" line that grade.sh checks for.
 * @param[in] name  Name of the test that has passed.
 */
static void lab5_pass(const char *name)
{
    cprintf("[Test %s] PASS\n", name);
}

/**
 * @brief Check the state of idleproc and initproc right after they are created.
 *
 * Expects idleproc (pid 0) to be the current process, with no mm and no trap frame, and
 * initproc (pid 1) to be a runnable child of idleproc with a valid context.
 * @note Called from proc_init(). Prints "[Test Process Create] PASS" on success.
 */
void test_lab5_process_create(void)
{
    assert(current == idleproc);
    assert(idleproc != NULL && idleproc->pid == 0);
    assert(idleproc->state == PROC_RUNNABLE);
    assert(idleproc->mm == NULL && idleproc->tf == NULL);
    assert(initproc != NULL && initproc->pid == 1);
    assert(initproc->parent == idleproc);
    assert(initproc->state == PROC_RUNNABLE);
    assert(initproc->context.ra != 0 && initproc->context.sp != 0);
    lab5_pass("Process Create");
}

/**
 * @brief Send two fake ecalls through exception_handler() and check the results.
 *
 * The first call uses SYS_getpid. It must advance tf.epc by 4 (past the ecall instruction) and
 * return the PID of the current process in a0. The second call uses number 0, which has no
 * handler, and must return -E_INVAL. The original current->tf is restored afterwards.
 * @note Prints "[Test Syscall] PASS" on success.
 */
void test_lab5_syscall(void)
{
    struct trapframe tf;
    struct trapframe *old = current->tf;
    memset(&tf, 0, sizeof(tf));
    tf.cause = CAUSE_USER_ECALL;
    tf.epc = 0x1000;
    tf.gpr.a0 = SYS_getpid;
    current->tf = &tf;
    exception_handler(&tf);
    assert(tf.epc == 0x1004);
    assert((int)tf.gpr.a0 == current->pid);
    tf.gpr.a0 = 0;
    exception_handler(&tf);
    assert((int)tf.gpr.a0 == -E_INVAL);
    current->tf = old;
    lab5_pass("Syscall");
}

/**
 * @brief Check the process-control paths: yield, kill and wait error codes.
 *
 * do_yield() must return 0 and set current->need_resched. do_kill(-1) must return -E_INVAL and
 * do_wait(-1, NULL) must return -E_BAD_PROC. The need_resched flag is cleared before and after
 * the yield check.
 * @note Prints "[Test Process Control] PASS" on success.
 */
void test_lab5_process_control(void)
{
    current->need_resched = 0;
    assert(do_yield() == 0 && current->need_resched);
    current->need_resched = 0;

    assert(do_kill(-1) == -E_INVAL);
    assert(do_wait(-1, NULL) == -E_BAD_PROC);
    lab5_pass("Process Control");
}
