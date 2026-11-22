/* SPDX-License-Identifier: GPL-3.0-only */
/* A CD music module's background reader with the 1 KiB guarded stack from
 * dc00 (KUI_ARMADA_CE_STREAM_SMALL_STACK, retail_loader_layout.h), as the
 * accepted Armada CD music and VMU reader keeps: its data may fill up to dc00;
 * its code and read-only data stay below d800, where the stream controller
 * checks the reader's entry points and bus. */
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
    ASSERT(_kui_retail_ce_stream_bus_entry == 0x8c008334, "stream bus entry moved")
    ASSERT(DEFINED(__kui_ce_cdda_small_stack_resident), "small-stack reader requires its source marker")
    ASSERT(__retail_resident_binary_end <= 0x8c00d800,
           "CD music reader code leaves the stream controller's bounds")
    ASSERT(__retail_resident_bss_end <= 0x8c00dc00,
           "retail resident overlaps its guarded service stack")
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
