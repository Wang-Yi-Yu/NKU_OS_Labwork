/**
 * @file   string.c
 * @brief  Kernel-only string helpers that allocate memory: strdup() and stradd() (lab8).
 *
 * The other string functions live in libs/string.c and need no allocator, so they are shared with
 * user programs. These two call kmalloc(), so they exist only in the kernel; the file system layer
 * uses them to build path strings. Declarations are in libs/string.h.
 */
#include <string.h>
#include <kmalloc.h>

/**
 * @brief Duplicate a string into memory obtained from kmalloc().
 * @param[in] src  String to copy.
 * @return The new copy, which the caller releases with kfree(); NULL if the allocation fails.
 */
char *strdup(const char *src) {
    char *dst;
    size_t len = strlen(src);
    if ((dst = kmalloc(len + 1)) != NULL) {
        memcpy(dst, src, len);
        dst[len] = '\0';
    }
    return dst;
}

/**
 * @brief Concatenate two strings into memory obtained from kmalloc().
 * @param[in] src1  First part of the result.
 * @param[in] src2  Second part of the result.
 * @return The new string, which the caller releases with kfree(); NULL if the allocation fails.
 */
char *stradd(const char *src1, const char *src2) {
    char *ret, *dst;
    size_t len1 = strlen(src1), len2 = strlen(src2);
    if ((ret = dst = kmalloc(len1 + len2 + 1)) != NULL) {
        memcpy(dst, src1, len1), dst += len1;
        memcpy(dst, src2, len2), dst += len2;
        *dst = '\0';
    }
    return ret;
}
