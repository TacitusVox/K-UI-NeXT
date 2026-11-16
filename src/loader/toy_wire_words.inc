/* SPDX-License-Identifier: GPL-3.0-only */
/* Test 74: the card stream's block loop (toy_wire_stream.c, wire_words) by
 * hand. Every byte the stream receives goes through this loop, and with the
 * sector's EDC left to the sectors that need it, this loop is most of what
 * the reader does. The compiler's version takes 49 instructions for four
 * bytes, a third of them moves to and from R0, which every indexed load
 * and every displaced byte store of the SH-4 goes through. Written out, the
 * same steps take 41, and 34 where the receiver delivers bytes as the card
 * sent them and nothing is reversed.
 *
 *   uint32_t kui_wire_words_lsb(const uint32_t *p, uint32_t words,
 *       uint8_t *out, const uint8_t *tables, uint32_t crc);
 *   uint32_t kui_wire_words_true(the same);
 *
 * p: `words` (at least one) whole words of the ring, aligned. out: where
 * the bytes go, in card order; any alignment. tables: the stream's table
 * block as wire_tables lays it out: 256 bytes of bit reversal, 256 of the
 * CRC16 step's inverse, then the step table and the two-byte step table,
 * 256 entries of 16 bits each, at 512 and 1024. crc: the remainder so far,
 * 16 bits, the rest zero. Returns the remainder behind the words, the same
 * way. What the two compute is word for word what the C loops compute; the
 * host tests run those, and tests/test_toy_wire_linked_loops.py runs these
 * as linked against them.
 *
 * R1 one-byte step table, R2 two-byte step table, R3 remainder, R8 the
 * word, R9 and R10 table entries. A table entry is loaded sign-extended;
 * the remainder is cut back to 16 bits after each step, which is what lets
 * one 16-bit shift bring the word's upper half down into place. */
    .macro KUI_WIRE_WORDS_ENTER
    mov.l   r8,@-r15
    mov.l   r9,@-r15
    mov.l   r10,@-r15
    mov.l   @(12,r15),r3
    mov.w   9f,r0
    mov     r7,r1
    add     r0,r1
    mov     r1,r2
    add     r0,r2
    .endm
    /* The remainder over one more word: R3 with R8, by R1, R2, R9, R10. */
    .macro KUI_WIRE_WORDS_STEP
    mov.l   @r4+,r8
    xor     r8,r3
    extu.b  r3,r0
    add     r0,r0
    mov.w   @(r0,r2),r9
    mov     r3,r0
    shlr8   r0
    extu.b  r0,r0
    add     r0,r0
    mov.w   @(r0,r1),r10
    shlr16  r3
    xor     r10,r9
    extu.w  r9,r9
    xor     r9,r3
    extu.b  r3,r0
    add     r0,r0
    mov.w   @(r0,r2),r9
    mov     r3,r0
    shlr8   r0
    add     r0,r0
    mov.w   @(r0,r1),r10
    xor     r10,r9
    extu.w  r9,r3
    .endm
    .macro KUI_WIRE_WORDS_LEAVE
    mov     r3,r0
    mov.l   @r15+,r10
    mov.l   @r15+,r9
    rts
    mov.l   @r15+,r8
    .balign 4
9:  .word   512
    .endm

    .section .text.kui_wire_words_lsb,"ax"
    .balign 4
    .global C_SYMBOL(kui_wire_words_lsb)
    .type C_SYMBOL(kui_wire_words_lsb),@function
C_SYMBOL(kui_wire_words_lsb):
    KUI_WIRE_WORDS_ENTER
1:  KUI_WIRE_WORDS_STEP
    /* The four bytes, each with its bits reversed (R7: that table). */
    extu.b  r8,r0
    mov.b   @(r0,r7),r0
    mov.b   r0,@(0,r6)
    shlr8   r8
    extu.b  r8,r0
    mov.b   @(r0,r7),r0
    mov.b   r0,@(1,r6)
    shlr8   r8
    extu.b  r8,r0
    mov.b   @(r0,r7),r0
    mov.b   r0,@(2,r6)
    shlr8   r8
    mov     r8,r0
    mov.b   @(r0,r7),r0
    mov.b   r0,@(3,r6)
    dt      r5
    bf.s    1b
    add     #4,r6
    KUI_WIRE_WORDS_LEAVE
    .size C_SYMBOL(kui_wire_words_lsb),.-C_SYMBOL(kui_wire_words_lsb)

    .section .text.kui_wire_words_true,"ax"
    .balign 4
    .global C_SYMBOL(kui_wire_words_true)
    .type C_SYMBOL(kui_wire_words_true),@function
C_SYMBOL(kui_wire_words_true):
    KUI_WIRE_WORDS_ENTER
1:  KUI_WIRE_WORDS_STEP
    /* The four bytes as they are. */
    mov.b   r8,@r6
    shlr8   r8
    mov     r8,r0
    mov.b   r0,@(1,r6)
    shlr8   r0
    mov.b   r0,@(2,r6)
    shlr8   r0
    mov.b   r0,@(3,r6)
    dt      r5
    bf.s    1b
    add     #4,r6
    KUI_WIRE_WORDS_LEAVE
    .size C_SYMBOL(kui_wire_words_true),.-C_SYMBOL(kui_wire_words_true)
