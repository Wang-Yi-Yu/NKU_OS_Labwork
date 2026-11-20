/**
 * @file   picirq.h
 * @brief  Interface of the 8259A programmable interrupt controller driver.
 *
 * In lab4 the RISC-V kernel does not use an 8259A PIC; both functions are empty stubs kept
 * so that the common init code still compiles.
 */

#ifndef __KERN_DRIVER_PICIRQ_H__
#define __KERN_DRIVER_PICIRQ_H__

/**
 * @brief Initialise the interrupt controller. Empty stub in lab4.
 */
void pic_init(void);
/**
 * @brief Unmask (enable) an interrupt line on the controller. Empty stub in lab4.
 * @param[in] irq  Interrupt line number.
 */
void pic_enable(unsigned int irq);

#endif /* !__KERN_DRIVER_PICIRQ_H__ */
