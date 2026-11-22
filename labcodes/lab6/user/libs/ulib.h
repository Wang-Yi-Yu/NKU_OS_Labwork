/**
 * @file   ulib.h
 * @brief  User-level library declarations: panic/warn/assert macros and system call wrappers.
 *
 * Include this header from every user program. Functions that are defined in ulib.c and panic.c
 * are declared here, together with the macros that wrap them.
 */
#ifndef __USER_LIBS_ULIB_H__
#define __USER_LIBS_ULIB_H__

#include <defs.h>

/**
 * @brief Print a warning with file and line; the process continues.
 * @param[in] file  Source file name (__FILE__).
 * @param[in] line  Line number (__LINE__).
 * @param[in] fmt  printf-style format string.
 * @note Defined in panic.c. Use the warn() macro.
 */
void __warn(const char *file, int line, const char *fmt, ...);
/**
 * @brief Print a fatal error with file and line, then exit(-E_PANIC).
 * @param[in] file  Source file name (__FILE__).
 * @param[in] line  Line number (__LINE__).
 * @param[in] fmt  printf-style format string.
 * @note Defined in panic.c. Use the panic() macro. Never returns.
 */
void __noreturn __panic(const char *file, int line, const char *fmt, ...);

/**
 * @brief Print a warning that shows the current file and line.
 * @param[in] ...  printf-style format string and its arguments.
 * @note Expands to __warn(__FILE__, __LINE__, ...).
 */
#define warn(...) __warn(__FILE__, __LINE__, __VA_ARGS__)

/**
 * @brief Print a fatal error that shows the current file and line, then exit.
 * @param[in] ...  printf-style format string and its arguments.
 * @note Expands to __panic(__FILE__, __LINE__, ...). The process exits and does not continue.
 */
#define panic(...) __panic(__FILE__, __LINE__, __VA_ARGS__)

/**
 * @brief Panic if the condition x is false.
 * @param[in] x  Condition to test; evaluated once.
 * @note The panic message contains the text of x. Compiled in all builds.
 */
#define assert(x)                              \
    do {                                       \
        if (!(x)) {                            \
            panic("assertion failed: %s", #x); \
        }                                      \
    } while (0)

/**
 * @brief Compile-time assertion: fail to compile if x is false.
 * @param[in] x  Constant expression to test; must be a compile-time constant.
 * @note Expands to switch (x) { case 0: case (x): ; }. When x is 0 the two case labels clash,
 *       so the compile fails.
 */
#define static_assert(x) \
    switch (x) {         \
        case 0:          \
        case (x):;       \
    }

/**
 * @brief Terminate the calling process with the given exit code.
 * @param[in] error_code  Exit code passed to the kernel and to the parent's waitpid().
 * @note Never returns. Defined in ulib.c.
 */
void __noreturn exit(int error_code);
/**
 * @brief Create a child process that is a copy of the caller.
 * @return In the parent, the PID of the child; in the child, 0; a negative value on error.
 */
int fork(void);
/**
 * @brief Wait for any child process to exit and reap it.
 * @return 0 if a child was reaped; a non-zero value if there is no child.
 */
int wait(void);
/**
 * @brief Wait for a specific child process to exit and reap it.
 * @param[in] pid  PID of the child to wait for.
 * @param[in] store  Where to store the child's exit code, or NULL to ignore it.
 * @return 0 on success; a non-zero value on error (for example a bad PID or pointer).
 */
int waitpid(int pid, int *store);
/**
 * @brief Voluntarily give up the CPU so that another process can run.
 */
void yield(void);
/**
 * @brief Mark a process as killed.
 * @param[in] pid  PID of the process to kill.
 * @return 0 on success; a non-zero value on error.
 */
int kill(int pid);
/**
 * @brief Return the PID of the calling process.
 * @return The PID of the caller.
 */
int getpid(void);
/**
 * @brief Ask the kernel to print the page directory and page tables of the caller.
 */
void print_pgdir(void);
/**
 * @brief Read the clock in milliseconds (lab6).
 * @return Milliseconds since boot.
 */
unsigned int gettime_msec(void);
/**
 * @brief Set the scheduling priority of the calling process (lab6).
 * @param[in] priority  New priority; a bigger value gets more CPU time under Stride scheduling.
 */
void lab6_setpriority(uint32_t priority);

#endif /* !__USER_LIBS_ULIB_H__ */
