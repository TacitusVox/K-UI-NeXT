/* SPDX-License-Identifier: GPL-3.0-only */
/* Boot-disc stage 1 entry. The BIOS loads 1ST_READ.BIN at 0x8c010000 and
 * jumps there, but that is also where the real bootstrap must run. So this
 * position-independent prologue first copies the whole stage, through
 * uncached (P2) aliases, to its link address high in RAM and continues there.
 * The handoff then follows KOS's own arch_exec contract: caches written back
 * and invalidated, the BIOS's stack pointer and return address restored, and
 * a jump to 0x8c010000 with SR/VBR/FPSCR as the BIOS left them. */

    .section .text.entry,"ax"
    .global start
start:
    mov.l   .Lsrc,r0
    mov.l   .Ldst,r1
    mov.l   .Lwords,r2
1:  mov.l   @r0+,r3
    mov.l   r3,@r1
    dt      r2
    bf/s    1b
    add     #4,r1
    mov.l   .Lrun,r0
    jmp     @r0
    nop
    .align  2
.Lsrc:      .long 0xac010000            /* load address, uncached */
.Ldst:      .long __stage1_start_p2     /* link address, uncached alias */
.Lwords:    .long __stage1_words
.Lrun:      .long stage1_run

    .text
    .align  2
stage1_run:
    /* Clear .bss (r0-r2 only), then keep the BIOS's r15/pr for the handoff. */
    mov.l   .Lbss,r0
    mov.l   .Lbss_end,r1
    mov     #0,r2
2:  cmp/hs  r1,r0
    bt      3f
    mov.l   r2,@r0
    bra     2b
    add     #4,r0
3:  mov.l   .Lsaved,r0
    mov.l   r15,@r0
    sts     pr,r1
    mov.l   r1,@(4,r0)
    mov.l   .Lstack,r15
    mov.l   .Lmain,r0
    jsr     @r0
    nop
    tst     r0,r0
    bf      .Lfail
    /* Cache control must run uncached: continue in this code's P2 alias. */
    mova    4f,r0
    mov.l   .Lp2,r1
    or      r1,r0
    jmp     @r0
    nop
    .align  2
4:  /* Write back and invalidate every operand-cache line through the address
     * array, as KOS's cache_write_ccr does before it changes CCR. */
    mov.l   .Loc_array,r0
    mov.l   .Loc_lines,r1
    mov     #0,r2
5:  mov.l   r2,@r0
    dt      r1
    bf/s    5b
    add     #32,r0
    /* Invalidate and disable both caches: KOS arch_exec's CCR value. The new
     * program's KOS startup enables them again (cache_write_ccr in arch_main). */
    mov.l   .Lccr,r0
    mov.l   .Lccr_exec,r1
    mov.l   r1,@r0
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    mov.l   .Lsaved,r0
    mov.l   @(4,r0),r1
    lds     r1,pr
    mov.l   @r0,r15
    mov.l   .Lentry,r0
    jmp     @r0
    nop

.Lfail:
    /* Nothing verified to start: return to the Dreamcast BIOS menu. */
    mov.l   .Lsystem,r0
    mov.l   @r0,r0
    jsr     @r0
    mov     #1,r4
6:  bra     6b
    nop

    .align  2
.Lsaved:    .long stage1_saved
.Lbss:      .long __stage1_bss
.Lbss_end:  .long __stage1_bss_end
.Lstack:    .long __stage1_stack
.Lmain:     .long _kui_stage1_main
.Lp2:       .long 0x20000000
.Loc_array: .long 0xf4000000
.Loc_lines: .long 512
.Lccr:      .long 0xff00001c
.Lccr_exec: .long 0x00000808
.Lentry:    .long 0x8c010000
.Lsystem:   .long 0x8c0000e0

    .section .bss
    .align  2
stage1_saved:
    .space  8
