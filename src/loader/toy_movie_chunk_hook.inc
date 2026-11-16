/* SPDX-License-Identifier: GPL-3.0-only */

/* Observe the native AVI update call at 8c08d894. Its fifth argument
 * belongs at the original callee's SP. Copy it into the same slot in
 * the wrapper's 16-byte frame, keeping the incoming stack alignment. */
    .section .text.toy_movie_chunk,"ax"
    .balign 4
    .global C_SYMBOL(kui_toy_movie_chunk_hook)
    .type C_SYMBOL(kui_toy_movie_chunk_hook),@function
C_SYMBOL(kui_toy_movie_chunk_hook):
    movt r0
    sts pr,r1
    mov.l .Lchunk_return,r2
    cmp/eq r2,r1
    bf .Lchunk_tail

    add #-16,r15
    mov.l r1,@(12,r15)
    mov.l @(16,r15),r1
    mov.l r1,@r15
    mov r6,r2
    add #-124,r2
    mov.l r2,@(8,r15)
    mov.l .Lchunk_original,r1
    jsr @r1
    shlr r0

    movt r1
    mov.l r1,@r15
    mov.l r0,@(4,r15)
    mov.l @(8,r15),r4
    mov.l .Lchunk_observe,r1
    jsr @r1
    mov r0,r5

    mov.l @(12,r15),r1
    lds r1,pr
    mov.l @(4,r15),r0
    mov.l @r15,r1
    add #16,r15
    rts
    shlr r1
.Lchunk_tail:
    mov.l .Lchunk_original,r1
    jmp @r1
    shlr r0

    .balign 4
.Lchunk_return: .long 0x8c08d898
.Lchunk_original: .long 0x8c08edbc
.Lchunk_observe: .long C_SYMBOL(kui_toy_movie_chunk_observe)
    .size C_SYMBOL(kui_toy_movie_chunk_hook),.-C_SYMBOL(kui_toy_movie_chunk_hook)
