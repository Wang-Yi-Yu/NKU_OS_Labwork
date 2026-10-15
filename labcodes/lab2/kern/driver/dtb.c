/**
 * @file   dtb.c
 * @brief  Flattened device tree (FDT/DTB) parser, extracting physical memory base and size.
 *
 * Reads the device tree directly according to the binary layout in the
 * devicetree specification: after the header come the memory reservation map,
 * the structure block (a stream of tokens) and the strings block. This file
 * only parses the reg property of the first memory node in the structure
 * block and does not build a full device tree data structure. The public
 * interface is in dtb.h.
 */

#include <dtb.h>
#include <stdio.h>
#include <string.h>
#include <defs.h>
#include <memlayout.h>

/**
 * @brief Flattened device tree header (FDT header).
 *
 * Located at the start of the DTB. Every field is a 32-bit big-endian integer,
 * laid out as specified by the devicetree specification. The header is
 * accessed in place through a pointer, and fdt32_to_cpu is used to convert
 * the byte order.
 */
struct fdt_header {
    uint32_t magic;             /**< Magic number, always 0xd00dfeed (big-endian) */
    uint32_t totalsize;         /**< Total size of the DTB in bytes */
    uint32_t off_dt_struct;     /**< Offset of the structure block from the start of the DTB */
    uint32_t off_dt_strings;    /**< Offset of the strings block from the start of the DTB */
    uint32_t off_mem_rsvmap;    /**< Offset of the memory reservation map */
    uint32_t version;           /**< Device tree format version */
    uint32_t last_comp_version; /**< Lowest version this tree is backward compatible with */
    uint32_t boot_cpuid_phys;   /**< Physical ID of the boot CPU */
    uint32_t size_dt_strings;   /**< Size of the strings block in bytes */
    uint32_t size_dt_struct;    /**< Size of the structure block in bytes */
};

/**
 * @brief FDT structure-block token: begin node (BEGIN_NODE).
 *
 * Followed by the NUL-terminated node name, which is padded with zeros to a
 * 4-byte boundary.
 */
#define FDT_BEGIN_NODE 0x00000001

/**
 * @brief FDT structure-block token: end node (END_NODE).
 */
#define FDT_END_NODE 0x00000002

/**
 * @brief FDT structure-block token: property (PROP).
 *
 * Followed by a 32-bit data length, a 32-bit name offset into the strings
 * block, then len bytes of property data. The data is padded to a 4-byte boundary.
 */
#define FDT_PROP 0x00000003

/**
 * @brief FDT structure-block token: no-op (NOP), skipped by the parser.
 */
#define FDT_NOP 0x00000004

/**
 * @brief FDT structure-block token: end of the structure block (END); parsing stops when it is seen.
 */
#define FDT_END 0x00000009

/**
 * @brief Swap the byte order of a 32-bit integer (big-endian <-> little-endian).
 *
 * @details All integers in an FDT are big-endian, while the RISC-V kernel is
 *          little-endian. The swap is an involution, so the same function is
 *          used in both directions.
 *
 * @param[in] x  32-bit value read from the DTB in host byte order.
 * @return The value with its bytes reversed.
 */
static uint32_t fdt32_to_cpu(uint32_t x) {
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | (((x >> 16) & 0xff) << 8) |
           ((x >> 24) & 0xff);
}

/**
 * @brief Swap the byte order of a 64-bit integer (big-endian <-> little-endian).
 *
 * @details Applies fdt32_to_cpu to the high and low 32-bit halves separately,
 *          and exchanges the two halves.
 *
 * @param[in] x  64-bit value read from the DTB in host byte order.
 * @return The value with its bytes reversed.
 */
static uint64_t fdt64_to_cpu(uint64_t x) {
    return ((uint64_t)fdt32_to_cpu(x & 0xffffffff) << 32) | fdt32_to_cpu(x >> 32);
}

/**
 * @brief Find the reg property of the first memory node in the structure block, and decode its base and size.
 *
 * @details Scans the token stream linearly. On BEGIN_NODE it checks whether the
 *          node name starts with "memory" and sets in_memory_node accordingly.
 *          On PROP, if it is inside a memory node, the property name is "reg",
 *          and the length is at least 16 bytes, the first 16 bytes are decoded
 *          as two 64-bit big-endian integers: base and size. On END_NODE,
 *          in_memory_node is cleared.
 *
 * @param[in]  dtb_vaddr  Kernel virtual address of the DTB (physical address plus PHYSICAL_MEMORY_OFFSET).
 * @param[in]  header     FDT header located at dtb_vaddr; read-only.
 * @param[out] mem_base   Output: memory base address; valid only when the function returns 0.
 * @param[out] mem_size   Output: memory size; valid only when the function returns 0.
 * @return 0 if the property was found and decoded; -1 if END was reached without finding it,
 *         or an unrecognised token was encountered.
 *
 * @note Known limitations:
 *       - #address-cells and #size-cells are not handled; reg is assumed to consist of two 64-bit values;
 *       - only the first reg range of the first memory node is used; other ranges are ignored;
 *       - in_memory_node is cleared by any END_NODE, so after a child node of a memory node ends,
 *         later reg properties of that memory node are no longer recognised;
 *       - only the first 6 characters of the node name are compared, so names such as "memory-xxx"
 *         are also treated as memory nodes;
 *       - totalsize and the offsets are not checked against the blob bounds; the DTB is assumed valid.
 * @warning reg data is read through a uint64_t pointer, but property data is only guaranteed to be
 *          4-byte aligned. On platforms that require naturally aligned accesses this may fault.
 */
static int extract_memory_info(uintptr_t dtb_vaddr, const struct fdt_header *header,
                               uint64_t *mem_base, uint64_t *mem_size) {
    uint32_t struct_offset = fdt32_to_cpu(header->off_dt_struct);
    uint32_t strings_offset = fdt32_to_cpu(header->off_dt_strings);

    const char *strings_base = (const char *)(dtb_vaddr + strings_offset);
    const uint32_t *struct_ptr = (const uint32_t *)(dtb_vaddr + struct_offset);

    int in_memory_node = 0;

    while (1) {
        uint32_t token = fdt32_to_cpu(*struct_ptr++);

        switch (token) {
            case FDT_BEGIN_NODE: {
                const char *name = (const char *)struct_ptr;
                int name_len = strlen(name);

                // check whether this is a memory node
                if (strncmp(name, "memory", 6) == 0) {
                    in_memory_node = 1;
                }

                // skip the node name (4-byte aligned)
                struct_ptr = (const uint32_t *)(((uintptr_t)struct_ptr + name_len + 4) & ~3);
                break;
            }

            case FDT_END_NODE:
                in_memory_node = 0;
                break;

            case FDT_PROP: {
                uint32_t prop_len = fdt32_to_cpu(*struct_ptr++);
                uint32_t prop_nameoff = fdt32_to_cpu(*struct_ptr++);
                const char *prop_name = strings_base + prop_nameoff;
                const void *prop_data = struct_ptr;

                // look for the reg property inside the memory node
                if (in_memory_node && strcmp(prop_name, "reg") == 0 && prop_len >= 16) {
                    const uint64_t *reg_data = (const uint64_t *)prop_data;
                    *mem_base = fdt64_to_cpu(reg_data[0]);
                    *mem_size = fdt64_to_cpu(reg_data[1]);
                    return 0;  // found
                }

                // skip the property data (4-byte aligned)
                struct_ptr = (const uint32_t *)(((uintptr_t)struct_ptr + prop_len + 3) & ~3);
                break;
            }

            case FDT_NOP:
                break;

            case FDT_END:
                return -1;  // not found

            default:
                return -1;  // malformed token
        }
    }
}

/**
 * @brief Parsed physical memory base address (static variable).
 *
 * Written by dtb_init and exposed through get_memory_base. Initial value is 0.
 */
static uint64_t memory_base = 0;

/**
 * @brief Parsed physical memory size (static variable, in bytes).
 *
 * Written by dtb_init and exposed through get_memory_size. Initial value is 0.
 */
static uint64_t memory_size = 0;

/**
 * @brief Parse the device tree, extract and store the base and size of the first physical memory range, and print the results.
 *
 * @details Prints the boot hartid and the DTB address first. If boot_dtb is 0,
 *          reports an error and returns. Otherwise it converts the physical
 *          address to a virtual one by adding PHYSICAL_MEMORY_OFFSET, checks the
 *          magic number, then calls extract_memory_info. On success it prints the
 *          base, size (in MB) and end address, and stores them in the static variables.
 */
void dtb_init(void) {
    cprintf("DTB Init\n");
    cprintf("HartID: %ld\n", boot_hartid);
    cprintf("DTB Address: 0x%lx\n", boot_dtb);

    if (boot_dtb == 0) {
        cprintf("Error: DTB address is null\n");
        return;
    }

    // convert to a virtual address
    uintptr_t dtb_vaddr = boot_dtb + PHYSICAL_MEMORY_OFFSET;
    const struct fdt_header *header = (const struct fdt_header *)dtb_vaddr;

    // validate the DTB
    uint32_t magic = fdt32_to_cpu(header->magic);
    if (magic != 0xd00dfeed) {
        cprintf("Error: Invalid DTB magic number: 0x%x\n", magic);
        return;
    }

    // extract the memory information
    uint64_t mem_base, mem_size;
    if (extract_memory_info(dtb_vaddr, header, &mem_base, &mem_size) == 0) {
        cprintf("Physical Memory from DTB:\n");
        cprintf("  Base: 0x%016lx\n", mem_base);
        cprintf("  Size: 0x%016lx (%ld MB)\n", mem_size, mem_size / (1024 * 1024));
        cprintf("  End:  0x%016lx\n", mem_base + mem_size - 1);
        // save to globals so the PMM can query them
        memory_base = mem_base;
        memory_size = mem_size;
    } else {
        cprintf("Warning: Could not extract memory info from DTB\n");
    }
    cprintf("DTB init completed\n");
}

/**
 * @brief Get the physical memory base address obtained by dtb_init.
 *
 * @details Returns the static variable memory_base without any checks.
 */
uint64_t get_memory_base(void) {
    return memory_base;
}

/**
 * @brief Get the physical memory size obtained by dtb_init.
 *
 * @details Returns the static variable memory_size without any checks.
 */
uint64_t get_memory_size(void) {
    return memory_size;
}
