/* SPDX-License-Identifier: GPL-3.0-only */
/* Places the two replacements at their addresses in 67b0df9's resident. */
OUTPUT_FORMAT("elf32-shl")
OUTPUT_ARCH(sh)
ENTRY(start_dma)
overrun_stats = 0x8c009738;
SECTIONS {
 . = 0x8c009028;
 .start_dma : { KEEP(*(.start_dma)) }
 ASSERT(SIZEOF(.start_dma) <= 152, "start_dma replacement exceeds the original")
 . = 0x8c00965c;
 .overrun : { KEEP(*(.overrun)) }
 ASSERT(SIZEOF(.overrun) == 38, "overrun replacement must fill the snapshot span exactly")
 /DISCARD/ : { *(.comment) *(.note*) }
}
