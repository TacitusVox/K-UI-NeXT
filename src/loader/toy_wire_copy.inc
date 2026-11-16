/* SPDX-License-Identifier: GPL-3.0-only */
/* Test 76 (COPY): a card block into the verified window, a cache line at a
 * time.
 *
 * Every byte the stream receives is copied once more, from the stream's
 * block into its sector's place in the window (toy_ahead_write). The C
 * library's loop took it a word at a time: a load and a store by turns,
 * and the first store into each of the destination's sixteen lines made
 * the cache fetch that line from memory only to have all of it written
 * over. Where source and destination share a place in the 16 KiB cache,
 * which one block in sixteen does somewhere along its length, each turn
 * also threw the other's line out.
 *
 *   void kui_wire_copy(uint8_t *out, const uint8_t *in, uint32_t bytes);
 *
 * out, in and bytes are multiples of four (the caller checks, and uses the
 * library's copy otherwise); bytes may be zero. Words are copied singly
 * until out stands on a cache line. Then, for each whole line left: its
 * eight words are loaded first, so that nothing is read once the
 * destination's line is in hand; MOVCA.L stores the first of them, which
 * claims the line in the cache without fetching it; and the other seven
 * are stored, which leaves no byte of the line as the cache found it. What
 * is left under a line is copied word by word again.
 *
 * MOVCA.L is used only on a line all of which this call writes, at once
 * and with nothing in between (a visit runs with interrupts held off): a
 * line shared with a neighbour's bytes is never claimed. In any cache mode
 * the bytes end where a plain store would put them. tests/
 * test_toy_wire_linked_copy.py runs this as linked, with every byte of a
 * claimed line undefined until it is stored.
 *
 * R4 out, R5 in, R6 bytes left (less 32 inside the line loop); R0 to R3,
 * R7 and R8 to R10 the line's words. */
    .section .text.kui_wire_copy,"ax"
    .balign 4
    .global C_SYMBOL(kui_wire_copy)
    .type C_SYMBOL(kui_wire_copy),@function
C_SYMBOL(kui_wire_copy):
    mov.l   r8,@-r15
    mov.l   r9,@-r15
    mov.l   r10,@-r15
1:  tst     r6,r6
    bt      9f
    mov     r4,r0
    tst     #31,r0
    bf      6f
    /* On a line: whole lines while thirty-two bytes or more are left. */
    add     #-32,r6
    cmp/pz  r6
    bf      5f
3:  mov.l   @r5+,r0
    mov.l   @r5+,r1
    mov.l   @r5+,r2
    mov.l   @r5+,r3
    mov.l   @r5+,r7
    mov.l   @r5+,r8
    mov.l   @r5+,r9
    mov.l   @r5+,r10
    movca.l r0,@r4
    mov.l   r1,@(4,r4)
    mov.l   r2,@(8,r4)
    mov.l   r3,@(12,r4)
    mov.l   r7,@(16,r4)
    mov.l   r8,@(20,r4)
    mov.l   r9,@(24,r4)
    mov.l   r10,@(28,r4)
    add     #-32,r6
    cmp/pz  r6
    bt.s    3b
    add     #32,r4
5:  add     #32,r6
    tst     r6,r6
    bt      9f
    /* One word. */
6:  mov.l   @r5+,r0
    mov.l   r0,@r4
    add     #4,r4
    bra     1b
    add     #-4,r6
9:  mov.l   @r15+,r10
    mov.l   @r15+,r9
    rts
    mov.l   @r15+,r8
    .size C_SYMBOL(kui_wire_copy),.-C_SYMBOL(kui_wire_copy)
