/* SPDX-License-Identifier: GPL-3.0-only */

/* The original AVI refill at 8c0989d4 calls GDFS only after retiring the
 * previous arrival and updating the index. The other paths keep their
 * original literal. The caller's R14 is its exact AVI object. */
    .section .text.toy_movie_refill,"ax"
    .balign 4
    .global C_SYMBOL(kui_toy_movie_refill_hook)
    .type C_SYMBOL(kui_toy_movie_refill_hook),@function
C_SYMBOL(kui_toy_movie_refill_hook):
    movt r0
    sts pr,r1
    mov.l .Lrefill_return,r2
    cmp/eq r1,r2
    bf .Lrefill_original_tail
    mov.l r0,@-r15
    mov.l r4,@-r15
    mov.l r5,@-r15
    mov.l r6,@-r15
    mov.l r7,@-r15
    sts.l pr,@-r15
#if KUI_TOY_SUITE_MODE == 61
    mov r5,r7
#endif
    mov r4,r5
    mov.l .Lrefill_defer,r1
    jsr @r1
    mov r14,r4
    lds.l @r15+,pr
    mov.l @r15+,r7
    mov.l @r15+,r6
    mov.l @r15+,r5
    mov.l @r15+,r4
#if KUI_TOY_SUITE_MODE == 61
    cmp/eq #-1,r0
    mov r0,r2
    bf/s .Lrefill_selected_tail
    mov.l @r15+,r0
#else
    tst r0,r0
    bt/s .Lrefill_original_tail
    mov.l @r15+,r0
#endif
    shlr r0
    rts
    mov #0,r0
#if KUI_TOY_SUITE_MODE == 61
.Lrefill_selected_tail:
    mov r2,r6
#endif
.Lrefill_original_tail:
    mov.l .Lrefill_original,r1
    jmp @r1
    shlr r0

    .balign 4
.Lrefill_return: .long 0x8c0989d8
.Lrefill_original: .long 0x8c097186
#if KUI_TOY_SUITE_MODE == 61
.Lrefill_defer: .long C_SYMBOL(kui_toy_movie_refill_length)
#else
.Lrefill_defer: .long C_SYMBOL(kui_toy_movie_refill_defer)
#endif
    .size C_SYMBOL(kui_toy_movie_refill_hook),.-C_SYMBOL(kui_toy_movie_refill_hook)
