/**
 * @file   string.h
 * @brief  Declarations of the kernel string and memory utilities (no libc dependency).
 *
 * Grouped as: string length, copy, compare and search (str*); string-to-integer conversion
 * (strtol); and memory operations (mem*). Implementations are in libs/string.c.
 */
#ifndef __LIBS_STRING_H__
#define __LIBS_STRING_H__

#include <defs.h>

/**
 * @brief Returns the length of a string, excluding the terminating '\0'.
 *
 * @param[in] s  Input string; must not be NULL and must be '\0'-terminated.
 * @return The length of @p s.
 */
size_t strlen(const char *s);

/**
 * @brief Returns the length of a string, examining at most @p len characters.
 *
 * The scan never goes beyond @p s + @p len, so this is safe on buffers that may lack a '\0'.
 *
 * @param[in] s    Input string; must not be NULL.
 * @param[in] len  Maximum number of characters to examine.
 * @return The length of @p s if a '\0' occurs within the first @p len characters; otherwise @p len.
 */
size_t strnlen(const char *s, size_t len);

/**
 * @brief Copies @p src, including its terminating '\0', into @p dst.
 *
 * The caller must ensure that @p dst can hold the whole of @p src, '\0' included, and that the
 * two areas do not overlap.
 *
 * @param[out] dst  Destination buffer.
 * @param[in]  src  Source string.
 * @return @p dst.
 * @warning No bounds check is performed; an undersized @p dst causes a buffer overflow.
 */
char *strcpy(char *dst, const char *src);

/**
 * @brief Copies at most @p len characters from @p src to @p dst.
 *
 * If @p src ends (reaches '\0') before @p len characters have been copied, the remainder of
 * @p dst up to @p len bytes is filled with '\0'. If @p src has @p len or more characters, the
 * result is not NUL-terminated.
 *
 * @param[out] dst  Destination buffer, at least @p len bytes.
 * @param[in]  src  Source string.
 * @param[in]  len  Number of bytes to write to @p dst.
 * @return @p dst.
 */
char *strncpy(char *dst, const char *src, size_t len);

/**
 * @brief Lexicographically compares two strings.
 *
 * Characters are compared as unsigned char, starting from the first character, until a
 * mismatch or a terminating '\0' is reached.
 *
 * @param[in] s1  First string.
 * @param[in] s2  Second string.
 * @retval 0    The strings are equal.
 * @retval >0   The first differing character in @p s1 is greater than the one in @p s2.
 * @retval <0   The first differing character in @p s1 is less than the one in @p s2.
 */
int strcmp(const char *s1, const char *s2);

/**
 * @brief Compares at most @p n characters of two strings.
 *
 * Stops at the first mismatch, at a '\0' in either string, or after @p n characters.
 * The return value has the same meaning as for strcmp().
 *
 * @param[in] s1  First string.
 * @param[in] s2  Second string.
 * @param[in] n   Maximum number of characters to compare.
 * @return 0 if the first @p n characters are equal; otherwise as for strcmp().
 */
int strncmp(const char *s1, const char *s2, size_t n);

/**
 * @brief Finds the first occurrence of a character in a string.
 *
 * Only characters before the terminating '\0' are searched, so the terminator is never
 * returned as a match.
 *
 * @param[in] s  Input string.
 * @param[in] c  Character to find.
 * @return Pointer to the first occurrence of @p c, or NULL if it does not occur.
 * @see strfind()
 */
char *strchr(const char *s, char c);

/**
 * @brief Finds the first occurrence of a character, or the terminator if it is absent.
 *
 * Same as strchr() except that when @p c is not found, a pointer to the terminating '\0'
 * of @p s is returned instead of NULL.
 *
 * @param[in] s  Input string.
 * @param[in] c  Character to find.
 * @return Pointer to the first occurrence of @p c, or to the terminating '\0' if absent.
 * @see strchr()
 */
char *strfind(const char *s, char c);

/**
 * @brief Converts the initial part of a string to a long integer (like the standard strtol).
 *
 * Parsing steps:
 *   -# skip leading spaces and tabs (' ' and '\t');
 *   -# accept one optional '+' or '-' sign;
 *   -# parse digits in the given base until an invalid character is reached.
 *
 * Base handling:
 *   - base 0: chosen from the prefix; "0x" means hexadecimal, a leading '0' means octal,
 *     anything else means decimal;
 *   - base 16: an optional "0x" prefix is accepted;
 *   - 2 to 36: digits are parsed in that base; letters are case-insensitive, with 'a'/'A' = 10
 *     through 'z'/'Z' = 35.
 *
 * @param[in]  s       Input string.
 * @param[out] endptr  If not NULL, receives the position of the first unconsumed character.
 * @param[in]  base    Numeric base, or 0 to detect it from the prefix.
 * @return The parsed value, or 0 if no digits were found.
 * @warning Overflow is not detected, and @p base is not validated against [2, 36].
 */
long strtol(const char *s, char **endptr, int base);

/**
 * @brief Sets the first @p n bytes of a memory area to a given byte value.
 *
 * @param[out] s  Destination memory area.
 * @param[in]  c  Byte value to store (passed as char).
 * @param[in]  n  Number of bytes to set.
 * @return @p s.
 */
void *memset(void *s, char c, size_t n);

/**
 * @brief Copies @p n bytes from @p src to @p dst. The two areas may overlap.
 *
 * Overlapping areas are handled correctly, which makes this safer than memcpy() at a possible
 * cost in speed.
 *
 * @param[out] dst  Destination memory area.
 * @param[in]  src  Source memory area.
 * @param[in]  n    Number of bytes to copy.
 * @return @p dst.
 */
void *memmove(void *dst, const void *src, size_t n);

/**
 * @brief Copies exactly @p n bytes from @p src to @p dst.
 *
 * The contents of @p src are not checked for '\0'; exactly @p n bytes are always copied.
 *
 * @param[out] dst  Destination memory area, at least @p n bytes.
 * @param[in]  src  Source memory area, at least @p n bytes.
 * @param[in]  n    Number of bytes to copy.
 * @return @p dst.
 * @warning If the two areas overlap the behaviour is undefined; use memmove() in that case.
 */
void *memcpy(void *dst, const void *src, size_t n);

/**
 * @brief Compares the first @p n bytes of two memory areas.
 *
 * Bytes are compared as unsigned char; the function returns at the first differing byte.
 *
 * @param[in] v1  First memory area.
 * @param[in] v2  Second memory area.
 * @param[in] n   Number of bytes to compare.
 * @retval 0    All @p n bytes are equal.
 * @retval >0   The first differing byte in @p v1 is greater than the one in @p v2.
 * @retval <0   The first differing byte in @p v1 is less than the one in @p v2.
 */
int memcmp(const void *v1, const void *v2, size_t n);

#endif /* !__LIBS_STRING_H__ */
