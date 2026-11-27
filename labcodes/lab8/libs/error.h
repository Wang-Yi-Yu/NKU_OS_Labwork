/**
 * @file   error.h
 * @brief  Kernel error code definitions.
 *
 * Error codes are positive integers; failing functions typically return their negation
 * (e.g. -E_NO_MEM). The %e conversion of vprintfmt() relies on these values, and its
 * description table is in libs/printfmt.c. When adding a code, update MAXERROR and the
 * error_string table in libs/printfmt.c together.
 */
#ifndef __LIBS_ERROR_H__
#define __LIBS_ERROR_H__

/* kernel error codes -- keep in sync with list in libs/printfmt.c */

/** @brief Unspecified or unknown error. */
#define E_UNSPECIFIED 1  // Unspecified or unknown problem

/** @brief The process does not exist or is in an unexpected state. */
#define E_BAD_PROC 2  // Process doesn't exist or otherwise

/** @brief Invalid parameter. */
#define E_INVAL 3  // Invalid parameter

/** @brief The request failed because memory is exhausted. */
#define E_NO_MEM 4  // Request failed due to memory shortage

/** @brief An attempt to create a new process was made beyond the process limit. */
#define E_NO_FREE_PROC 5  // Attempt to create a new process beyond

/** @brief Memory fault. */
#define E_FAULT 6  // Memory fault

/** @brief Swap read or write fault (unused in this lab). */
#define E_SWAP_FAULT 7  // SWAP READ/WRITE fault

/** @brief Invalid ELF file; returned by load_icode() for a bad ELF header. */
#define E_INVAL_ELF 8  // Invalid elf file

/** @brief The process was killed (PF_EXITING was set by do_kill()). */
#define E_KILLED 9  // Process is killed

/** @brief Panic failure. */
#define E_PANIC 10  // Panic Failure

/** @brief Timeout. */
#define E_TIMEOUT 11  // Timeout

/** @brief An argument is too big. */
#define E_TOO_BIG 12  // Argument is Too Big

/** @brief No such device. */
#define E_NO_DEV 13  // No such Device

/** @brief Device not available. */
#define E_NA_DEV 14  // Device Not Available

/** @brief Device or file is busy. */
#define E_BUSY 15  // Device/File is Busy

/** @brief No such file or directory. */
#define E_NOENT 16  // No Such File or Directory

/** @brief Is a directory. */
#define E_ISDIR 17  // Is a Directory

/** @brief Not a directory. */
#define E_NOTDIR 18  // Not a Directory

/** @brief Cross-device link. */
#define E_XDEV 19  // Cross Device-Link

/** @brief Unimplemented feature. */
#define E_UNIMP 20  // Unimplemented Feature

/** @brief Illegal seek. */
#define E_SEEK 21  // Illegal Seek

/** @brief Too many files are open. */
#define E_MAX_OPEN 22  // Too Many Files are Open

/** @brief The file or directory already exists. */
#define E_EXISTS 23  // File/Directory Already Exists

/** @brief The directory is not empty. */
#define E_NOTEMPTY 24  // Directory is Not Empty

/** @brief Largest error code defined (E_NOTEMPTY). printfmt.c names every code up to it; larger ones print as "error N". */
#define MAXERROR 24

#endif /* !__LIBS_ERROR_H__ */
