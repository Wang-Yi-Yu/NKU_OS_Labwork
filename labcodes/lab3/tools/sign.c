/**
 * @file   sign.c
 * @brief  Host tool inherited from the x86 ucore: pads a boot sector to 512 bytes and appends 0x55AA.
 *
 * Usage: sign <input filename> <output filename>. It is not built by the lab3 Makefile (RISC-V has
 * no boot sector), the file is kept only because it is part of the original lab tree.
 */
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

/**
 * @brief Check that the input is at most 510 bytes, then write it padded to 510 bytes plus the
 *        boot signature bytes 0x55 0xAA.
 * @param argc argument count, must be 3
 * @param argv program name, input file, output file
 * @return 0 on success, -1 on error
 */
int
main(int argc, char *argv[]) {
    struct stat st;
    if (argc != 3) {
        fprintf(stderr, "Usage: <input filename> <output filename>\n");
        return -1;
    }
    if (stat(argv[1], &st) != 0) {
        fprintf(stderr, "Error opening file '%s': %s\n", argv[1], strerror(errno));
        return -1;
    }
    printf("'%s' size: %lld bytes\n", argv[1], (long long)st.st_size);
    if (st.st_size > 510) {
        fprintf(stderr, "%lld >> 510!!\n", (long long)st.st_size);
        return -1;
    }
    char buf[512];
    memset(buf, 0, sizeof(buf));
    FILE *ifp = fopen(argv[1], "rb");
    int size = fread(buf, 1, st.st_size, ifp);
    if (size != st.st_size) {
        fprintf(stderr, "read '%s' error, size is %d.\n", argv[1], size);
        return -1;
    }
    fclose(ifp);
    buf[510] = 0x55;
    buf[511] = 0xAA;
    FILE *ofp = fopen(argv[2], "wb+");
    size = fwrite(buf, 1, 512, ofp);
    if (size != 512) {
        fprintf(stderr, "write '%s' error, size is %d.\n", argv[2], size);
        return -1;
    }
    fclose(ofp);
    printf("build 512 bytes boot sector: '%s' success!\n", argv[2]);
    return 0;
}

