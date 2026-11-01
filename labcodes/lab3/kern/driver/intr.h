/**
 * @file   intr.h
 * @brief  Interface to enable and disable supervisor-mode interrupts.
 */
#ifndef __KERN_DRIVER_INTR_H__
#define __KERN_DRIVER_INTR_H__

/** @brief Enable interrupts in S mode by setting sstatus.SIE. */
void intr_enable(void);
/** @brief Disable interrupts in S mode by clearing sstatus.SIE. */
void intr_disable(void);

#endif /* !__KERN_DRIVER_INTR_H__ */
