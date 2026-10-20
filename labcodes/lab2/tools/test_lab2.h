#ifndef __KERN_TEST_TEST_LAB2_H__
#define __KERN_TEST_TEST_LAB2_H__

#include <defs.h>

/** @brief Test 1: check va_pa_offset, the device tree memory range and the Page descriptors set up by page_init(). */
void test_page_init(void);
/** @brief Test 2: check that best_fit_pmm_manager is the active physical memory manager. */
void test_best_fit(void);
/** @brief Test 3: run the Buddy System self test on a 256-page region borrowed from the active manager. */
void test_buddy_system(void);

#endif /* !__KERN_TEST_TEST_LAB2_H__ */
