/**
 * @file   clock.h
 * @brief  Interface of the timer driver: tick counter and timer-interrupt scheduling.
 */
#ifndef __KERN_DRIVER_CLOCK_H__
#define __KERN_DRIVER_CLOCK_H__

#include <defs.h>

/** @brief Number of supervisor timer interrupts handled since clock_init(). */
extern volatile size_t ticks;

/** @brief Enable the supervisor timer interrupt, arm the first timer event and reset ::ticks. */
void clock_init(void);
/** @brief Arm the next timer event, one timebase period after the current time. */
void clock_set_next_event(void);

#endif /* !__KERN_DRIVER_CLOCK_H__ */
