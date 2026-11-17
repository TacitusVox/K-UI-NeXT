/* SPDX-License-Identifier: GPL-3.0-only */
/* The WinCE data probe's test reader (KUI_WCE_PROBE): the ordinary CE
 * background reader's layout with a 640-byte guarded stack from dd80, and
 * the seed entry word the stage fills (include/kui/wce_probe.h). */
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
    __retail_hook_stack_bottom = 0x8c00dd80;
    __retail_hook_stack = 0x8c00e000;
    ASSERT(_start == 0x8c008300, "resident entry moved")
    ASSERT(_kui_retail_ce_kernel == 0x8c008324, "CE kernel slot moved")
    ASSERT(_kui_wce_probe_seed_entry == 0x8c008334, "probe seed entry moved")
    ASSERT(DEFINED(__kui_wce_probe_resident), "probe reader requires its source marker")
    ASSERT(__retail_resident_bss_end <= 0x8c00dd80,
           "retail resident overlaps its guarded service stack")
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
