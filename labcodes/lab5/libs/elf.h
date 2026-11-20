/**
 * @file   elf.h
 * @brief  ELF executable format structures used to load the kernel and user programs.
 *
 * Declares the ELF file header (elfhdr), the program header (proghdr) and the constants
 * needed to check the magic number and to interpret segment types and permission flags.
 */

#ifndef __LIBS_ELF_H__
#define __LIBS_ELF_H__

#include <defs.h>

/** @brief First four bytes of every ELF file, "\x7FELF", read as a little-endian uint32_t. */
#define ELF_MAGIC 0x464C457FU  // "\x7FELF" in little endian

/**
 * @brief ELF file header at the start of the file.
 *
 * e_magic must equal ELF_MAGIC. e_entry is the virtual address where execution starts.
 */
/* file header */
struct elfhdr {
    uint32_t e_magic;  // must equal ELF_MAGIC
    uint8_t e_elf[12];
    uint16_t e_type;       // 1=relocatable, 2=executable, 3=shared object, 4=core image
    uint16_t e_machine;    // 3=x86, 4=68K, etc.
    uint32_t e_version;    // file version, always 1
    uint64_t e_entry;      // entry point if executable
    uint64_t e_phoff;      // file position of program header or 0
    uint64_t e_shoff;      // file position of section header or 0
    uint32_t e_flags;      // architecture-specific flags, usually 0
    uint16_t e_ehsize;     // size of this elf header
    uint16_t e_phentsize;  // size of an entry in program header
    uint16_t e_phnum;      // number of entries in program header or 0
    uint16_t e_shentsize;  // size of an entry in section header
    uint16_t e_shnum;      // number of entries in section header or 0
    uint16_t e_shstrndx;   // section number that contains section name strings
};

/**
 * @brief ELF program header: describes one segment to load into memory.
 *
 * A loader copies p_filesz bytes from p_offset to virtual address p_va and zero-fills
 * up to p_memsz (the difference is .bss).
 */
/* program section header */
struct proghdr {
    uint32_t p_type;    // loadable code or data, dynamic linking info,etc.
    uint32_t p_flags;   // read/write/execute bits
    uint64_t p_offset;  // file offset of segment
    uint64_t p_va;      // virtual address to map segment
    uint64_t p_pa;      // physical address, not used
    uint64_t p_filesz;  // size of segment in file
    uint64_t p_memsz;   // size of segment in memory (bigger if contains bss）
    uint64_t p_align;   // required alignment, invariably hardware page size
};

/* values for Proghdr::p_type */
/** @brief Segment type: loadable segment that must be mapped into memory. */
#define ELF_PT_LOAD 1

/* flag bits for Proghdr::p_flags */
/** @name Segment permission bits (Proghdr::p_flags). @{ */
/** @brief Segment is executable. */
#define ELF_PF_X 1
/** @brief Segment is writable. */
#define ELF_PF_W 2
/** @brief Segment is readable. */
#define ELF_PF_R 4
/** @} */

#endif /* !__LIBS_ELF_H__ */
