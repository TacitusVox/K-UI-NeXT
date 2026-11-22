/* SPDX-License-Identifier: GPL-3.0-only */
/* The generic Windows CE background reader with read-ahead
 * (KUI_RETAIL_CE_READAHEAD): the ordinary layout with a 1 KiB guarded stack
 * from dc00, as the writable VMU reader keeps, and the pool record's entry
 * word (KUI_RETAIL_CE_POOL_ENTRY) the stage's relay follows. */
OUTPUT_FORMAT("elf32-shl")
OUTPUT_ARCH(sh)
ENTRY(_start)
PHDRS { resident PT_LOAD FLAGS(7); }
SECTIONS
{
    . = 0x8c008300;
    .entry : { KEEP(*(.entry)) } :resident
    .text : { *(.text .text.*) } :resident
    .rodata : { *(.rodata .rodata.*) } :resident
    .data : { *(.data .data.*) } :resident
    . = ALIGN(4);
    __retail_resident_binary_end = .;
    .bss (NOLOAD) : {
        __retail_resident_bss_begin = .;
        *(.bss .bss.* COMMON)
        . = ALIGN(4);
        __retail_resident_bss_end = .;
    } :resident
    __retail_hook_stack_bottom = 0x8c00dc00;
    __retail_hook_stack = 0x8c00e000;
    ASSERT(_start == 0x8c008300, "resident entry moved")
    ASSERT(_kui_retail_ce_kernel == 0x8c008324, "CE kernel slot moved")
    ASSERT(_kui_retail_ce_pool_entry == 0x8c008334, "pool record entry moved")
    ASSERT(DEFINED(__kui_ce_readahead_resident), "read-ahead reader requires its source marker")
    ASSERT(__retail_resident_bss_end <= 0x8c00dc00,
           "retail resident overlaps its guarded service stack")
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
