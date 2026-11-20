/**
 * @file   test_lab5.h
 * @brief  Entry points of the lab5 grading tests defined in test_lab5.c.
 */

#ifndef __LAB5_GRADE_TEST_H__
#define __LAB5_GRADE_TEST_H__

/** @brief Check idleproc and initproc after creation; called at the end of proc_init(). */
void test_lab5_process_create(void);
/**
 * @brief Check the getpid and unknown-number paths of the syscall dispatcher.
 *
 * Called at the start of init_main().
 */
void test_lab5_syscall(void);
/**
 * @brief Check the do_yield(), do_kill() and do_wait() paths.
 *
 * Called at the start of init_main().
 */
void test_lab5_process_control(void);

#endif
