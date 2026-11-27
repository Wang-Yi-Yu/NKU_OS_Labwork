/**
 * @file   test_lab9.h
 * @brief  Entry points of the lab9 grading tests (implemented in test_lab9.c).
 *
 * tools/inject.py copies both files to kern/test/ of a temporary copy of the student tree and adds
 * the three calls to kern_init(), right after fs_init().
 */
#ifndef __KERN_TEST_TEST_LAB9_H__
#define __KERN_TEST_TEST_LAB9_H__

/** @brief Test 1 (40 points): the Clock swap manager. Prints one PASS or FAIL line. */
void test_lab9_clock(void);
/** @brief Test 2 (30 points): do_mmap() on VMAs. Prints one PASS or FAIL line. */
void test_lab9_mmap(void);
/** @brief Test 3 (30 points): do_munmap() on VMAs. Prints one PASS or FAIL line. */
void test_lab9_munmap(void);

#endif /* !__KERN_TEST_TEST_LAB9_H__ */
