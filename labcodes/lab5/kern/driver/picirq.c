/**
 * @file   picirq.c
 * @brief  8259A interrupt controller driver stubs.
 *
 * Both functions are empty: RISC-V interrupts are routed through the CSRs and
 * OpenSBI, not through an 8259A PIC.
 */
#include <picirq.h>

/**
 * @brief Enable one interrupt line. Empty stub.
 * @param[in] irq  Interrupt line number (ignored).
 */
void pic_enable(unsigned int irq) {}

/**
 * @brief Initialise the 8259A interrupt controllers. Empty stub.
 */
void pic_init(void) {}
