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

/** @brief Largest error code currently defined; matches the size of error_string in libs/printfmt.c. */
#define MAXERROR 6

#endif /* !__LIBS_ERROR_H__ */
