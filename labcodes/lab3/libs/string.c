/**
 * @file   string.c
 * @brief  Implementation of the kernel string and memory utilities (strlen, strcpy, memcpy, ...).
 *
 * None of these functions depend on libc or check for NULL; callers must pass valid pointers.
 * Function prototypes and full documentation are in libs/string.h.
 * If the architecture provides an optimized version (signalled by __HAVE_ARCH_STRCPY and similar
 * macros), the corresponding function forwards to that version.
 */
#include <string.h>
#include <riscv.h>

/**
 * @brief Returns the length of a string, excluding the terminating '\0'.
 *
 * Full description is in libs/string.h.
 *
 * @param s  Input string; must not be NULL.
 * @return Length of the string.
 */
size_t strlen(const char *s) {
    size_t cnt = 0;
    while (*s++ != '\0') {
        cnt++;
    }
    return cnt;
}

/**
 * @brief Returns the length of a string, scanning at most @p len characters.
 *
 * The scan never goes beyond s + len. Full description is in libs/string.h.
 *
 * @param s    Input string.
 * @param len  Maximum number of characters to scan.
 * @return Length of the string, or @p len if no '\0' is found within the first @p len characters.
 */
size_t strnlen(const char *s, size_t len) {
    size_t cnt = 0;
    while (cnt < len && *s++ != '\0') {
        cnt++;
    }
    return cnt;
}

/**
 * @brief Copies a string, including its terminating '\0', to @p dst.
 *
 * Forwards to the architecture-specific __strcpy() when __HAVE_ARCH_STRCPY is defined.
 * Full description is in libs/string.h.
 *
 * @param dst  Destination buffer.
 * @param src  Source string.
 * @return @p dst.
 */
char *strcpy(char *dst, const char *src) {
#ifdef __HAVE_ARCH_STRCPY
    return __strcpy(dst, src);
#else
    char *p = dst;
    while ((*p++ = *src++) != '\0')
        /* nothing */;
    return dst;
#endif /* __HAVE_ARCH_STRCPY */
}

/**
 * @brief Copies at most @p len characters from @p src to @p dst, padding with '\0' if @p src ends early.
 *
 * Once @p src reaches '\0', zero bytes are written until @p len bytes are stored, so the result
 * is not NUL-terminated when @p src has at least @p len characters.
 * Full description is in libs/string.h.
 *
 * @param dst  Destination buffer, at least @p len bytes.
 * @param src  Source string.
 * @param len  Number of bytes to write to @p dst.
 * @return @p dst.
 */
char *strncpy(char *dst, const char *src, size_t len) {
    char *p = dst;
    while (len > 0) {
        if ((*p = *src) != '\0') {
            src++;
        }
        p++, len--;
    }
    return dst;
}

/**
 * @brief Lexicographically compares two strings, treating characters as unsigned char.
 *
 * Forwards to __strcmp() when __HAVE_ARCH_STRCMP is defined. Full description is in libs/string.h.
 *
 * @param s1  First string.
 * @param s2  Second string.
 * @return 0 if equal; a positive value if s1 is greater; a negative value if s1 is smaller.
 */
int strcmp(const char *s1, const char *s2) {
#ifdef __HAVE_ARCH_STRCMP
    return __strcmp(s1, s2);
#else
    while (*s1 != '\0' && *s1 == *s2) {
        s1++, s2++;
    }
    return (int)((unsigned char)*s1 - (unsigned char)*s2);
#endif /* __HAVE_ARCH_STRCMP */
}

/**
 * @brief Compares at most @p n characters of two strings.
 *
 * Stops at the first mismatch, at a terminating '\0', or after @p n characters.
 * Full description is in libs/string.h.
 *
 * @param s1  First string.
 * @param s2  Second string.
 * @param n   Maximum number of characters to compare.
 * @return 0 if the first @p n characters are equal; otherwise as for strcmp().
 */
int strncmp(const char *s1, const char *s2, size_t n) {
    while (n > 0 && *s1 != '\0' && *s1 == *s2) {
        n--, s1++, s2++;
    }
    return (n == 0) ? 0 : (int)((unsigned char)*s1 - (unsigned char)*s2);
}

/**
 * @brief Finds the first occurrence of a character in a string.
 *
 * Only the characters before the terminating '\0' are searched, so searching for '\0' never
 * returns the terminator. Full description is in libs/string.h.
 *
 * @param s  Input string.
 * @param c  Character to find.
 * @return Pointer to the first match, or NULL if not found.
 */
char *strchr(const char *s, char c) {
    while (*s != '\0') {
        if (*s == c) {
            return (char *)s;
        }
        s++;
    }
    return NULL;
}

/**
 * @brief Finds the first occurrence of a character, or a pointer to the terminator if absent.
 *
 * Differs from strchr() only in the not-found case. Full description is in libs/string.h.
 *
 * @param s  Input string.
 * @param c  Character to find.
 * @return Pointer to the first match, or to the terminating '\0' if not found.
 */
char *strfind(const char *s, char c) {
    while (*s != '\0') {
        if (*s == c) {
            break;
        }
        s++;
    }
    return (char *)s;
}

/**
 * @brief Converts a string to a long integer (same semantics as the standard strtol).
 *
 * Full description of the parsing rules is in libs/string.h.
 *
 * @details No overflow detection is performed, and @p base is not validated. For a base outside
 * [2, 36] the digit test becomes meaningless and the result may not be what the caller expects.
 *
 * @param s       Input string.
 * @param endptr  If not NULL, receives the position of the first character not consumed.
 * @param base    Numeric base; 0 selects the base from the prefix.
 * @return The parsed value, or 0 if there are no valid digits.
 */
long strtol(const char *s, char **endptr, int base) {
    int neg = 0;
    long val = 0;

    // gobble initial whitespace
    while (*s == ' ' || *s == '\t') {
        s++;
    }

    // plus/minus sign
    if (*s == '+') {
        s++;
    } else if (*s == '-') {
        s++, neg = 1;
    }

    // hex or octal base prefix
    if ((base == 0 || base == 16) && (s[0] == '0' && s[1] == 'x')) {
        s += 2, base = 16;
    } else if (base == 0 && s[0] == '0') {
        s++, base = 8;
    } else if (base == 0) {
        base = 10;
    }

    // digits
    while (1) {
        int dig;

        if (*s >= '0' && *s <= '9') {
            dig = *s - '0';
        } else if (*s >= 'a' && *s <= 'z') {
            dig = *s - 'a' + 10;
        } else if (*s >= 'A' && *s <= 'Z') {
            dig = *s - 'A' + 10;
        } else {
            break;
        }
        if (dig >= base) {
            break;
        }
        s++, val = (val * base) + dig;
        // we don't properly detect overflow!
    }

    if (endptr) {
        *endptr = (char *)s;
    }
    return (neg ? -val : val);
}

/**
 * @brief Sets the first @p n bytes of a memory area to a value.
 *
 * Forwards to __memset() when __HAVE_ARCH_MEMSET is defined. Full description is in libs/string.h.
 *
 * @param s  Destination memory area.
 * @param c  Byte value to store.
 * @param n  Number of bytes to set.
 * @return @p s.
 */
void *memset(void *s, char c, size_t n) {
#ifdef __HAVE_ARCH_MEMSET
    return __memset(s, c, n);
#else
    char *p = s;
    while (n-- > 0) {
        *p++ = c;
    }
    return s;
#endif /* __HAVE_ARCH_MEMSET */
}

/**
 * @brief Copies @p n bytes between possibly overlapping memory areas.
 *
 * Copies backward when the source starts before the destination and the two areas overlap,
 * and forward otherwise. Forwards to __memmove() when __HAVE_ARCH_MEMMOVE is defined.
 * Full description is in libs/string.h.
 *
 * @param dst  Destination memory area.
 * @param src  Source memory area.
 * @param n    Number of bytes to copy.
 * @return @p dst.
 */
void *memmove(void *dst, const void *src, size_t n) {
#ifdef __HAVE_ARCH_MEMMOVE
    return __memmove(dst, src, n);
#else
    const char *s = src;
    char *d = dst;
    if (s < d && s + n > d) {
        s += n, d += n;
        while (n-- > 0) {
            *--d = *--s;
        }
    } else {
        while (n-- > 0) {
            *d++ = *s++;
        }
    }
    return dst;
#endif /* __HAVE_ARCH_MEMMOVE */
}

/**
 * @brief Copies @p n bytes from @p src to @p dst, without handling overlap.
 *
 * Forwards to __memcpy() when __HAVE_ARCH_MEMCPY is defined. Full description is in libs/string.h.
 *
 * @param dst  Destination memory area.
 * @param src  Source memory area.
 * @param n    Number of bytes to copy.
 * @return @p dst.
 */
void *memcpy(void *dst, const void *src, size_t n) {
#ifdef __HAVE_ARCH_MEMCPY
    return __memcpy(dst, src, n);
#else
    const char *s = src;
    char *d = dst;
    while (n-- > 0) {
        *d++ = *s++;
    }
    return dst;
#endif /* __HAVE_ARCH_MEMCPY */
}

/**
 * @brief Compares the first @p n bytes of two memory areas, treating bytes as unsigned char.
 *
 * Full description is in libs/string.h.
 *
 * @param v1  First memory area.
 * @param v2  Second memory area.
 * @param n   Number of bytes to compare.
 * @return 0 if equal; a positive value if the first differing byte in @p v1 is greater; otherwise negative.
 */
int memcmp(const void *v1, const void *v2, size_t n) {
    const char *s1 = (const char *)v1;
    const char *s2 = (const char *)v2;
    while (n-- > 0) {
        if (*s1 != *s2) {
            return (int)((unsigned char)*s1 - (unsigned char)*s2);
        }
        s1++, s2++;
    }
    return 0;
}
