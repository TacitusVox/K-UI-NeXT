/* SPDX-License-Identifier: GPL-3.0-only */

/* The original movie render callback can return without submitting a scene.
 * Tests 61 and later bracket the outer callback with its actual R4 owner, then
 * observes the inner scene-finish separately. */
#if KUI_TOY_SUITE_MODE >= 61
    .section .text.toy_movie_render,"ax"
    .balign 4
    .global C_SYMBOL(kui_toy_movie_render_hook)
    .type C_SYMBOL(kui_toy_movie_render_hook),@function
C_SYMBOL(kui_toy_movie_render_hook):
    add #-16,r15
    movt r1
    sts pr,r2
    mov.l r0,@r15
    mov.l r1,@(4,r15)
    mov.l r4,@(8,r15)
    mov.l r2,@(12,r15)
    mov.l .Lrender_enter,r1
    jsr @r1
    nop
    mov.l @r15,r0
    mov.l @(4,r15),r2
    mov.l @(8,r15),r4
    mov.l .Lrender_original,r1
    jsr @r1
    shlr r2

    movt r1
    mov.l r0,@r15
    mov.l r1,@(4,r15)
    mov.l .Lrender_leave,r1
    jsr @r1
    mov.l @(8,r15),r4

    mov.l @(12,r15),r2
    lds r2,pr
    mov.l @r15,r0
    mov.l @(4,r15),r1
    add #16,r15
    rts
    shlr r1

    .balign 4
.Lrender_original: .long 0x8c04c238
.Lrender_enter: .long C_SYMBOL(kui_toy_movie_render_enter)
.Lrender_leave: .long C_SYMBOL(kui_toy_movie_render_leave)
    .size C_SYMBOL(kui_toy_movie_render_hook),.-C_SYMBOL(kui_toy_movie_render_hook)
#endif

    .section .text.toy_movie_present,"ax"
    .balign 4
    .global C_SYMBOL(kui_toy_movie_present_hook)
    .type C_SYMBOL(kui_toy_movie_present_hook),@function
C_SYMBOL(kui_toy_movie_present_hook):
    movt r1
    sts pr,r3
    mov.l .Lpresent_return,r2
    cmp/eq r2,r3
    bf .Lpresent_tail

    add #-16,r15
    mov.l r3,@(12,r15)
#if KUI_TOY_SUITE_MODE == 60
    /* Test 60 derives the per-frame owner from the original render ABI. */
    mov r15,r2
    add #96,r2
    mov.l @r2,r2
    mov.l .Lpresent_render_return,r3
    cmp/eq r3,r2
    bf .Lpresent_release
#endif
    mov.l .Lpresent_engine_slot,r3
    mov.l @r3,r2
    jsr @r2
    shlr r1

    movt r1
    mov.l r0,@(4,r15)
    mov.l r1,@r15
    mov.l .Lpresent_observe,r2
    jsr @r2
#if KUI_TOY_SUITE_MODE >= 61
    nop
#else
    mov r14,r4
#endif

    mov.l @(12,r15),r1
    lds r1,pr
    mov.l @(4,r15),r0
    mov.l @r15,r1
    add #16,r15
    rts
    shlr r1
#if KUI_TOY_SUITE_MODE == 60
.Lpresent_release:
    add #16,r15
#endif
.Lpresent_tail:
    mov.l .Lpresent_engine_slot,r3
    mov.l @r3,r2
    jmp @r2
    shlr r1

    .balign 4
.Lpresent_return: .long 0x8c04c484
#if KUI_TOY_SUITE_MODE == 60
.Lpresent_render_return: .long 0x8c0977b2
#endif
.Lpresent_engine_slot: .long 0x8c0fc464
#if KUI_TOY_SUITE_MODE >= 61
.Lpresent_observe: .long C_SYMBOL(kui_toy_movie_scene_observe)
#else
.Lpresent_observe: .long C_SYMBOL(kui_toy_movie_present_observe)
#endif
    .size C_SYMBOL(kui_toy_movie_present_hook),.-C_SYMBOL(kui_toy_movie_present_hook)

    /* The original code loads through this cell and needs no opcode edits. */
    .section .rodata.toy_movie_present,"a"
    .balign 4
    .global C_SYMBOL(kui_toy_movie_present_slot)
    .type C_SYMBOL(kui_toy_movie_present_slot),@object
C_SYMBOL(kui_toy_movie_present_slot):
    .long C_SYMBOL(kui_toy_movie_present_hook)
    .size C_SYMBOL(kui_toy_movie_present_slot),4
