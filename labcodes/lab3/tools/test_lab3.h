/**
 * @file   test_lab3.h
 * @brief  Hooks of the lab3 grading tests; tools/inject.py inserts calls to them into kern_init().
 */
#ifndef __KERN_TEST_TEST_LAB3_H__
#define __KERN_TEST_TEST_LAB3_H__

/** @brief Run the trap-context test (GRADE_LAB3_CONTEXT) or the illegal-instruction test (GRADE_LAB3_ILLEGAL); inserted after the second idt_init call. */
void test_lab3_exception_context(void);
/** @brief Check that ticks is still 0 before interrupts are enabled (GRADE_LAB3_TIMER); inserted after clock_init. */
void test_lab3_timer_is_armed(void);
/** @brief Busy-wait until ticks reaches 200 and print the PASS line (GRADE_LAB3_TIMER); inserted after intr_enable. */
void test_lab3_wait_for_timer(void);

#endif /* !__KERN_TEST_TEST_LAB3_H__ */
