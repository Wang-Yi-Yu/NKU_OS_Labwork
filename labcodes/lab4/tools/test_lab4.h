/**
 * @file   test_lab4.h
 * @brief  Entry points of the lab4 grading tests defined in test_lab4.c.
 */

#ifndef __KERN_TEST_TEST_LAB4_H__
#define __KERN_TEST_TEST_LAB4_H__

#include <defs.h>

/** @brief Test page allocation and page-table mapping; called at the end of pmm_init(). */
void test_lab4_pmm_basic(void);
/** @brief Check idleproc and initproc after creation; called at the end of proc_init(). */
void test_lab4_process_create(void);
/** @brief Check that two kernel threads are scheduled; called at the start of init_main(). */
void test_lab4_schedule(void);

#endif /* !__KERN_TEST_TEST_LAB4_H__ */
