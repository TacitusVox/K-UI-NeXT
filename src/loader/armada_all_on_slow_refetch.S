/* SPDX-License-Identifier: GPL-3.0-only */
/* The background reader's slow refetch, patched into the delivered Armada
 * Virtual VMU On reader ce-armada-all-on.kui 67b0df9469cc, whose source was
 * not recovered (tools/patch_armada_all_on_slow_refetch.py). Behavior as
 * KUI_CE_SCI_SLOW_AFTER_OVERRUN (sci_stream.c), console-measured on the
 * VMU Off reader (5d92a111b0fa): after a DMA overrun at 12.5 MHz the next
 * 64 DMA receptions run at 6.25 MHz; an overrun at 6.25 MHz returns to
 * 12.5 MHz. The handoff after every reception already restores BRR=0, so
 * commands and token searches stay at 12.5 MHz. The window is counted when
 * a reception starts rather than when it completes.
 *
 * In 67b0df9's resident: the stream state s is at 0x8c00d05c (area[] +4,
 * state +12, saved[] +36, and the padding byte +75 after token_polled holds
 * the slow state: bits 0..6 receptions left, bit 7 the reception in flight
 * runs slow). */

/* start_dma(area r4, offset r5) at 0x8c009028, 152 bytes. The same register
 * writes in the same order as the original, plus the slow clock between
 * SCR=0 and CHCR=0. Returns OK (0), or RESET (7) if SPTR does not read back. */
    .section .start_dma,"ax"
    .align 2
    .global start_dma
start_dma:
    mov.l   .Lsptr,r1           /* SCSPTR1 */
    mov     #-125,r0            /* 0x83: TxD latched high */
    mov.b   r0,@r1
    mov.b   @r1,r0
    and     #138,r0
    xor     #130,r0
    tst     r0,r0
    bf      .Lreset
    mov     r1,r3
    add     #-24,r3             /* r3 = SCBRR1; SCSCR1 is 4(r3) */
    mov.l   .Ldmac,r6           /* SAR1; DAR1 +4, DMATCR1 +8, CHCR1 +12 */
    mov.l   .Ls,r1
    mov.l   @r6,r2
    mov.l   r2,@(36,r1)         /* s.saved[0..3] = SAR, DAR, TCR, CHCR */
    mov.l   @(4,r6),r2
    mov.l   r2,@(40,r1)
    mov.l   @(8,r6),r2
    mov.l   r2,@(44,r1)
    mov.l   @(12,r6),r2
    mov.l   r2,@(48,r1)
    mov     #0,r0
    mov.b   r0,@(4,r3)          /* SCR = 0 */
    mov     #75,r0              /* s.slow */
    mov.b   @(r0,r1),r2
    mov     #127,r7
    and     r7,r2               /* receptions left; the in-flight bit clears */
    tst     r2,r2
    bt/s    .Lfast
    mov.b   r2,@(r0,r1)
    add     #127,r2             /* 0x80 | (left - 1) */
    mov.b   r2,@(r0,r1)
    mov     #1,r2
    mov.b   r2,@r3              /* BRR = 1: 6.25 MHz */
    mov     #64,r2              /* more than one bit time before RE */
.Lsettle:
    dt      r2
    bf      .Lsettle
.Lfast:
    mov     #0,r2
    mov.l   r2,@(12,r6)         /* CHCR = 0 */
    mov.l   .Lrdr,r2
    mov.l   r2,@r6              /* SAR = SCRDR1 (physical) */
    mov     r4,r0
    shll2   r0
    add     r1,r0
    mov.l   @(4,r0),r2          /* s.area[area] */
    mov.l   .Lmask,r7
    and     r7,r2
    add     r5,r2
    mov.l   r2,@(4,r6)          /* DAR = physical area + offset */
    mov.w   .L513,r2
    sub     r5,r2
    mov.l   r2,@(8,r6)          /* TCR = 513 - offset */
    mov.w   .Lrx,r2
    mov.l   r2,@(12,r6)         /* CHCR = receive DMA */
    mov     #80,r0
    mov.b   r0,@(4,r3)          /* SCR = RIE|RE */
    mov     #2,r2
    mov.l   r2,@(12,r1)         /* s.state = DMA */
    rts
    mov     #0,r0
.Lreset:
    rts
    mov     #7,r0
    .align 1
.L513:  .short 513
.Lrx:   .short 0x4911
    .align 2
.Lsptr: .long 0xffe0001c
.Ldmac: .long 0xffa00010
.Ls:    .long 0x8c00d05c
.Lrdr:  .long 0x1fe00014
.Lmask: .long 0x1fffffff

/* finish(), incomplete reception, at 0x8c00965c..0x8c009682: the two late
 * diagnostic snapshots (CHCR2, DMAOR) give way to the slow window. On exit,
 * exactly as before: r1 = &s.stats word (0x8c00d0bc), r0 = status (r12),
 * r3 = 1 if RDR held a byte (status RDRF) else 0. r5, r8..r12 untouched. */
    .section .overrun,"ax"
    .align 1
    .global overrun
overrun:
    mov.l   overrun_stats,r1    /* finish's own literal (0x8c009738) */
    mov     r12,r0
    tst     #32,r0              /* ORER? */
    bt      1f
    mov     r1,r2
    add     #-21,r2             /* &s.slow (0x8c00d0a7) */
    mov.b   @r2,r0
    tst     #128,r0             /* this reception ran slow? */
    mov     #0,r0               /* yes: back to 12.5 MHz */
    bf      2f
    mov     #64,r0              /* no: the next 64 at 6.25 MHz */
2:  mov.b   r0,@r2
1:  mov     #-1,r3
    mov     r12,r0
    tst     #64,r0
    negc    r3,r3               /* held = status & RDRF ? 1 : 0 */
    nop
    nop
    nop
