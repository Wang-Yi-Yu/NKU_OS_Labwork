/**
 * @file   sbi.h
 * @brief  Declarations of the SBI (Supervisor Binary Interface), used by the kernel to call firmware services in M-mode.
 *
 * This header only declares the interface. Of these, only the console I/O, the timer and the
 * sbi_call() wrapper (see libs/sbi.c) are implemented. The other declarations have no definition
 * in this repository, so calling them will fail at link time.
 */
#ifndef _ASM_RISCV_SBI_H
#define _ASM_RISCV_SBI_H

/**
 * @brief Description of a memory block, filled in by sbi_query_memory().
 */
typedef struct {
    unsigned long base;    /**< Start address of the memory block. */
    unsigned long size;    /**< Size of the memory block, in bytes. */
    unsigned long node_id; /**< Identifier of the node that owns the block. */
} memory_block_info;

/**
 * @brief Queries the memory block with index @p id.
 *
 * @param[in]  id  Index of the memory block.
 * @param[out] p   Structure that receives the block description.
 * @return Status code; its meaning is defined by the SBI implementation.
 * @warning Not implemented in this repository.
 */
unsigned long sbi_query_memory(unsigned long id, memory_block_info *p);

/**
 * @brief Returns the ID of the current hart (hardware thread).
 * @return The hart ID.
 * @warning Not implemented in this repository.
 */
unsigned long sbi_hart_id(void);

/**
 * @brief Returns the number of harts in the system.
 * @return The number of harts.
 * @warning Not implemented in this repository.
 */
unsigned long sbi_num_harts(void);

/**
 * @brief Returns the timebase frequency of the platform, i.e. the tick rate of the time CSR.
 * @return The timebase frequency.
 * @warning Not implemented in this repository.
 */
unsigned long sbi_timebase(void);

/**
 * @brief Schedules the next timer interrupt (SBI_SET_TIMER).
 *
 * The timer interrupt fires when the time CSR reaches @p stime_value.
 *
 * @param[in] stime_value  Absolute time, in the same units as the time CSR.
 */
void sbi_set_timer(unsigned long long stime_value);

/**
 * @brief Sends an inter-processor interrupt (IPI) to a hart.
 * @param[in] hart_id  ID of the target hart.
 * @warning Not implemented in this repository.
 */
void sbi_send_ipi(unsigned long hart_id);

/**
 * @brief Clears the pending software interrupt (IPI) of the current hart.
 * @return Status code; its meaning is defined by the SBI implementation.
 * @warning Not implemented in this repository.
 */
unsigned long sbi_clear_ipi(void);

/**
 * @brief Asks the firmware to shut the system down.
 * @warning Not implemented in this repository.
 */
void sbi_shutdown(void);

/**
 * @brief Writes one character to the SBI console (SBI_CONSOLE_PUTCHAR).
 * @param[in] ch  Character to write.
 */
void sbi_console_putchar(unsigned char ch);

/**
 * @brief Reads one character from the SBI console (SBI_CONSOLE_GETCHAR).
 *
 * Returns immediately and does not block.
 *
 * @return The character read, or -1 if no character is pending.
 */
int sbi_console_getchar(void);

/**
 * @brief Performs a remote sfence.vma for an address space (ASID) on a set of harts.
 *
 * @param[in] hart_mask_ptr  Pointer to the hart mask.
 * @param[in] asid           Address-space ID to flush.
 * @warning Not implemented in this repository.
 */
void sbi_remote_sfence_vm(unsigned long hart_mask_ptr, unsigned long asid);

/**
 * @brief Performs a remote sfence.vma over an address range on a set of harts.
 *
 * @param[in] hart_mask_ptr  Pointer to the hart mask.
 * @param[in] asid           Address-space ID to flush.
 * @param[in] start          Start virtual address of the range.
 * @param[in] size           Size of the range, in bytes.
 * @warning Not implemented in this repository.
 */
void sbi_remote_sfence_vm_range(unsigned long hart_mask_ptr, unsigned long asid,
                                unsigned long start, unsigned long size);

/**
 * @brief Performs a remote fence.i (instruction cache synchronization) on a set of harts.
 *
 * @param[in] hart_mask_ptr  Pointer to the hart mask.
 * @warning Not implemented in this repository.
 */
void sbi_remote_fence_i(unsigned long hart_mask_ptr);

/**
 * @brief Masks (disables) the given interrupt source.
 * @param[in] which  Interrupt number.
 * @return Status code; its meaning is defined by the SBI implementation.
 * @warning Not implemented in this repository.
 */
unsigned long sbi_mask_interrupt(unsigned long which);

/**
 * @brief Unmasks (enables) the given interrupt source.
 * @param[in] which  Interrupt number.
 * @return Status code; its meaning is defined by the SBI implementation.
 * @warning Not implemented in this repository.
 */
unsigned long sbi_unmask_interrupt(unsigned long which);

#endif
