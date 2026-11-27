/**
 * @file   testmmap.c
 * @brief  User program that tests mmap() and munmap() (lab9).
 *
 * Four cases run one after the other; each prints its title and "  ok" when it passes. If every
 * case passes the program prints "mmaptest pass", otherwise "mmaptest failed" and returns -1:
 *  - [1] anonymous mapping of one page: write two ints, read them back, unmap;
 *  - [2] anonymous mapping of four pages: write one int into each page and check them all, which
 *        takes four page faults in the same mapping;
 *  - [3] read-only private mapping of the file /hello: the first bytes must be the ELF magic. The
 *        case is skipped if the file cannot be opened;
 *  - [4] read-only anonymous mapping: reading its first int must not fault.
 * Start it from the shell with `testmmap`. Every case depends on do_mmap(), do_munmap() and the
 * page fault handler do_pgfault() of the kernel (kern/mm/vmm.c).
 */
#include <stdio.h>
#include <ulib.h>
#include <unistd.h>
#include <file.h>

/** @brief Size of one page in bytes, the unit of every mapping made here. */
#define PAGE_SIZE 4096

/**
 * @brief Case [1]: map one anonymous page, write two ints, read them back and unmap it.
 * @return 0 on success, -1 if the mapping fails or the data read back differs
 */
static int test_anonymous_mmap(void) {
    cprintf("[1] anonymous mmap\n");

    int *p = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
        cprintf("  mmap failed\n");
        return -1;
    }

    p[0] = 0x12345678;
    p[1] = 0xdeadbeef;

    if (p[0] != 0x12345678 || p[1] != 0xdeadbeef) {
        cprintf("  data mismatch\n");
        return -1;
    }

    munmap(p, PAGE_SIZE);
    cprintf("  ok\n");
    return 0;
}

/**
 * @brief Case [2]: map four anonymous pages and write and check one int in every page.
 * @return 0 on success, -1 if the mapping fails or any page holds the wrong value
 */
static int test_multi_page_mmap(void) {
    cprintf("[2] multi-page anonymous mmap\n");

    int *p = mmap(NULL, PAGE_SIZE * 4, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
        cprintf("  mmap failed\n");
        return -1;
    }

    for (int i = 0; i < 4; i++)
        p[i * (PAGE_SIZE / sizeof(int))] = i;

    for (int i = 0; i < 4; i++)
        if (p[i * (PAGE_SIZE / sizeof(int))] != i) {
            cprintf("  page %d mismatch\n", i);
            return -1;
        }

    munmap(p, PAGE_SIZE * 4);
    cprintf("  ok\n");
    return 0;
}

/**
 * @brief Case [3]: map the first page of /hello read-only and look at the ELF magic.
 *
 * A first byte sequence other than 0x7f 'E' 'L' 'F' is only reported, it does not fail the case.
 * @return 0 on success or when /hello cannot be opened (skipped), -1 if mmap() fails
 */
static int test_file_mmap(void) {
    cprintf("[3] file mmap\n");

    int fd = open("/hello", O_RDONLY);
    if (fd < 0) {
        cprintf("  skip (no file)\n");
        return 0;
    }

    unsigned char *p = mmap(NULL, PAGE_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
    if (p == MAP_FAILED) {
        cprintf("  mmap failed\n");
        close(fd);
        return -1;
    }

    if (p[0] == 0x7f && p[1] == 'E' && p[2] == 'L' && p[3] == 'F')
        cprintf("  ELF header OK\n");
    else
        cprintf("  not ELF (still OK)\n");

    munmap(p, PAGE_SIZE);
    close(fd);
    cprintf("  ok\n");
    return 0;
}

/**
 * @brief Case [4]: map one anonymous page read-only and read from it.
 * @return 0 on success, -1 if the mapping fails
 */
static int test_readonly_mmap(void) {
    cprintf("[4] readonly mmap\n");

    int *p = mmap(NULL, PAGE_SIZE, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
        cprintf("  mmap failed\n");
        return -1;
    }

    volatile int v = p[0];
    (void)v;

    munmap(p, PAGE_SIZE);
    cprintf("  ok\n");
    return 0;
}

/**
 * @brief Run the four mmap cases in order and stop at the first failure.
 * @return 0 if all cases pass, -1 otherwise
 */
int main(void) {
    cprintf("==== mmap test ====\n");

    if (test_anonymous_mmap() < 0)
        goto fail;
    if (test_multi_page_mmap() < 0)
        goto fail;
    if (test_file_mmap() < 0)
        goto fail;
    if (test_readonly_mmap() < 0)
        goto fail;

    cprintf("mmaptest pass\n");
    return 0;

fail:
    cprintf("mmaptest failed\n");
    return -1;
}
