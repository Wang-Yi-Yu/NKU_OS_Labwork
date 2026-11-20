/**
 * @file   test_lab6.h
 * @brief  Entry points of the lab6 grading tests defined in test_lab6.c.
 */

#ifndef __LAB6_GRADE_TEST_H__
#define __LAB6_GRADE_TEST_H__

/**
 * @brief Check the RR scheduling class (default_sched_class) with a mock run queue.
 *
 * Called at the end of sched_init().
 */
void test_lab6_rr_scheduler(void);
/**
 * @brief Check the Stride scheduling class (stride_sched_class) with a mock run queue.
 *
 * Called at the end of sched_init().
 */
void test_lab6_stride_scheduler(void);

#endif
