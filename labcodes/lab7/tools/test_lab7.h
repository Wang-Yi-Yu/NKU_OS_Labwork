/**
 * @file   test_lab7.h
 * @brief  Entry points of the lab7 grading tests defined in test_lab7.c.
 */
#ifndef __KERN_TEST_TEST_LAB7_H__
#define __KERN_TEST_TEST_LAB7_H__

#include <defs.h>

/** @brief Type of phi_test_condvar(), phi_take_forks_condvar() and phi_put_forks_condvar(): they take a philosopher number. */
typedef void (*test_lab7_phi_fn_t)(int32_t);

/**
 * @brief Test cond_wait() and cond_signal() (50 points).
 *
 * Prints "[Test 1] Condition Variable: PASS (50/50)" or a FAIL line. Called at the start of check_sync().
 */
void test_lab7_condvar(void);
/**
 * @brief Test the three philosopher functions of the monitor version (50 points).
 *
 * Prints "[Test 2] Philosopher Monitor: PASS (50/50)" or a FAIL line. Called at the start of check_sync().
 * @param test_fn phi_test_condvar of the student
 * @param take_fn phi_take_forks_condvar of the student
 * @param put_fn  phi_put_forks_condvar of the student
 */
void test_lab7_philosopher(test_lab7_phi_fn_t test_fn,
                           test_lab7_phi_fn_t take_fn,
                           test_lab7_phi_fn_t put_fn);

#endif /* !__KERN_TEST_TEST_LAB7_H__ */
