/* SPDX-License-Identifier: GPL-3.0-only */
/* Boot-disc stage 1. The BIOS loads the flat binary at 0x8c010000; the entry
 * prologue copies it here, 15.5 MiB into RAM, out of the way of the bootstrap
 * it then loads at 0x8c010000 (at most 8 MiB, KUI_BOOT_COPIES_MAX_CHUNKS). */
OUTPUT_FORMAT("elf32-shl")
OUTPUT_ARCH(sh)
ENTRY(start)
SECTIONS
{
    . = 0x8cf80000;
    __stage1_start = .;
    .text : { KEEP(*(.text.entry)) *(.text .text.*) }
    .rodata : { *(.rodata .rodata.*) }
    .data : { KEEP(*(.data.kui_boot_copies)) *(.data .data.*) }
    . = ALIGN(4);
    __stage1_end = .;
    .bss (NOLOAD) : {
        __stage1_bss = .;
        *(.bss .bss.* COMMON)
        . = ALIGN(4);
        __stage1_bss_end = .;
    }
    __stage1_words = (__stage1_end - __stage1_start) / 4;
    __stage1_start_p2 = __stage1_start | 0x20000000;
    __stage1_stack = __stage1_start - 16;
    /DISCARD/ : { *(.comment) *(.eh_frame) *(.eh_frame_hdr) *(.note*) *(.gnu*) }
}
ASSERT(__stage1_bss_end - __stage1_start <= 0x8000, "stage 1 must stay small")
