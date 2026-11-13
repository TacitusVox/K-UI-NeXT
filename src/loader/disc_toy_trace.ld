/* SPDX-License-Identifier: GPL-3.0-only */
OUTPUT_FORMAT("elf32-shl")
OUTPUT_ARCH(sh)
ENTRY(_kui_disc_toy_gd_proxy)
PHDRS { image PT_LOAD FLAGS(7); }
SECTIONS {
 . = 0xacfd0000;
 .exports : { KEEP(*(.exports)) } :image
 ASSERT(SIZEOF(.exports)==24,"physical Toy trace exports ABI")
 .text : { *(.text .text.*) } :image
 .rodata : { *(.rodata .rodata.*) } :image
 .data : { *(.data .data.*) } :image
 . = ALIGN(32);
 __disc_toy_trace_binary_end = .;
 .bss (NOLOAD) : {
  *(.bss .bss.* COMMON)
  . = ALIGN(32);
  __disc_toy_trace_bss_end = .;
 } :image
 .stack (NOLOAD) : { . += 4096; __disc_toy_trace_stack_top = .; } :image
 ASSERT(.<=0xacfe0000,"physical Toy trace overlaps game scratch RAM")
 /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
