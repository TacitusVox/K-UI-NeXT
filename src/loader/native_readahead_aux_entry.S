/* SPDX-License-Identifier: GPL-3.0-only */
#define C_SYMBOL(x) _##x
    .section .native_ra_exports,"a"
    .balign 32
    .global C_SYMBOL(kui_native_readahead_aux_exports)
    .type C_SYMBOL(kui_native_readahead_aux_exports),@object
C_SYMBOL(kui_native_readahead_aux_exports):
    .long 0x4b524131,1,64
    .long C_SYMBOL(kui_native_readahead_aux_visit)
    .long C_SYMBOL(kui_native_readahead_aux_init)
    .long C_SYMBOL(kui_native_readahead_aux_context)
    .long __native_ra_image_end,__native_ra_stack_bottom,__native_ra_stack_top,__native_ra_memory_end
#if KUI_TOY_SUITE_MODE == 42 || KUI_TOY_SUITE_MODE == 44 || KUI_TOY_SUITE_MODE == 45 || KUI_TOY_SUITE_MODE == 46 || KUI_TOY_SUITE_MODE == 47 || KUI_TOY_SUITE_MODE == 48 || KUI_TOY_SUITE_MODE == 49 || KUI_TOY_SUITE_MODE == 50 || KUI_TOY_SUITE_MODE == 51 || KUI_TOY_SUITE_MODE == 52 || KUI_TOY_SUITE_MODE == 53 || KUI_TOY_SUITE_MODE == 54 || KUI_TOY_SUITE_MODE == 55 || KUI_TOY_SUITE_MODE == 56 || KUI_TOY_SUITE_MODE == 57 || KUI_TOY_SUITE_MODE == 58 || KUI_TOY_SUITE_MODE == 59 || KUI_TOY_SUITE_MODE == 60 || KUI_TOY_SUITE_MODE == 61 || KUI_TOY_SUITE_MODE == 62 || KUI_TOY_SUITE_MODE == 64 || KUI_TOY_SUITE_MODE == 65 || KUI_TOY_SUITE_MODE == 66
    .long C_SYMBOL(kui_toy_movie_exit_hook),C_SYMBOL(kui_toy_movie_chunk_hook)
#if KUI_TOY_SUITE_MODE >= 61
    .long C_SYMBOL(kui_toy_movie_refill_hook),C_SYMBOL(kui_toy_movie_present_slot)
    .long C_SYMBOL(kui_toy_movie_render_hook),0
#elif KUI_TOY_SUITE_MODE == 60
    .long C_SYMBOL(kui_toy_movie_refill_hook),C_SYMBOL(kui_toy_movie_present_slot),0,0
#elif KUI_TOY_SUITE_MODE == 57
    .long C_SYMBOL(kui_toy_movie_refill_hook),0,0,0
#elif KUI_TOY_SUITE_MODE == 58
    .long C_SYMBOL(kui_toy_sd_crc16_check),0,0,0
#else
    .long 0,0,0,0
#endif
#elif KUI_TOY_SUITE_MODE == 39 || KUI_TOY_SUITE_MODE == 40 || KUI_TOY_SUITE_MODE == 41 || KUI_TOY_SUITE_MODE == 43
    .long C_SYMBOL(kui_toy_movie_exit_hook),0,0,0,0,0
#else
    .long 0,0,0,0,0,0
#endif
    .size C_SYMBOL(kui_native_readahead_aux_exports),64
    .global __kui_native_readahead_auxiliary
    .set __kui_native_readahead_auxiliary,1

/* Standard C call: preserve SR, PR, SP and every ABI callee-saved register.
 * The low GD or VMU frame remains parked on its original stack. Reject busy
 * or damaged private storage before writing either frame or changing SP. */
    .section .text.native_ra_entry,"ax"
    .balign 4
    .global C_SYMBOL(kui_native_readahead_aux_visit)
    .type C_SYMBOL(kui_native_readahead_aux_visit),@function
C_SYMBOL(kui_native_readahead_aux_visit):
    stc sr,r0
    mov.l .Lmask,r1
    or r0,r1
    ldc r1,sr
    mov.l .Lcontext,r1
    mov.l @r1,r2
    tst r2,r2
    bf .Lreject
#if KUI_TOY_WIRE_CACHED
    /* Test 70: the private stack through the cache. Everything a visit
     * runs keeps its temporaries there, and at the uncached address each
     * one costs a bus cycle. With the MMU off the same page is reached at
     * its P0 address, where writes stay in the cache under the game's
     * control setting; R3 is what turns the linked uncached address into
     * that one. With the MMU on P0 is not this memory, and the stack is
     * used as before. The guard words are read through the same address
     * as the stack, so a stack that ran into them is seen. A visit owns
     * the page for its whole length with interrupts blocked, and nothing
     * of it is needed afterwards. */
    mov.l .Lmmucr,r1
    mov.l @r1,r2
    mov.l .Lcached,r3
    mov #1,r1
    tst r1,r2
    bt .Lalias
    mov #0,r3
.Lalias:
    mov.l .Lbottom,r1
    sub r3,r1
    mov.l @r1,r2
    mov.l .Lcanary,r1
    cmp/eq r1,r2
    bf .Lreject
    mov.l .Lhigh,r1
    sub r3,r1
    mov.l @r1,r2
    mov.l .Lcanary,r1
    cmp/eq r1,r2
    bf .Lreject
    mov.l r8,@-r15
    mov.l r9,@-r15
    mov.l r10,@-r15
    mov.l r11,@-r15
    mov.l r12,@-r15
    sts.l pr,@-r15
    mov r15,r8
    mov r0,r9
    mov.l .Lcontext,r10
    mov r4,r11
    mov r3,r12
    mov #1,r0
    mov.l r0,@r10
    mov.l .Lstack,r15
    sub r12,r15
    mov.l .Ldispatch,r0
    jsr @r0
    nop
    mov.l .Lcanary,r3
    mov.l .Lbottom,r1
    sub r12,r1
    mov.l @r1,r2
    cmp/eq r2,r3
    bf .Lafter_fault
    mov.l .Lhigh,r1
    sub r12,r1
    mov.l @r1,r2
    cmp/eq r2,r3
    bt .Lrestore
#else
    mov.l .Lbottom,r1
    mov.l @r1,r2
    mov.l .Lcanary,r3
    cmp/eq r2,r3
    bf .Lreject
    mov.l .Lhigh,r1
    mov.l @r1,r2
    cmp/eq r2,r3
    bf .Lreject
    mov.l r8,@-r15
    mov.l r9,@-r15
    mov.l r10,@-r15
    mov.l r11,@-r15
    sts.l pr,@-r15
    mov r15,r8
    mov r0,r9
    mov.l .Lcontext,r10
    mov r4,r11
    mov #1,r0
    mov.l r0,@r10
    mov.l .Lstack,r15
    mov.l .Ldispatch,r0
    jsr @r0
    nop
    mov.l .Lbottom,r1
    mov.l @r1,r2
    mov.l .Lcanary,r3
    cmp/eq r2,r3
    bf .Lafter_fault
    mov.l .Lhigh,r1
    mov.l @r1,r2
    cmp/eq r2,r3
    bt .Lrestore
#endif
.Lafter_fault:
    mov r10,r1
    add #64,r1
    mov.l @r1,r2
    add #1,r2
    mov.l r2,@r1
    mov #1,r0
    mov #3,r1
    cmp/eq r11,r1
    bf .Lrestore
    mov #0,r0
.Lrestore:
    mov #0,r1
    mov.l r1,@r10
    mov r8,r15
    mov r9,r1
    lds.l @r15+,pr
#if KUI_TOY_WIRE_CACHED
    mov.l @r15+,r12
#endif
    mov.l @r15+,r11
    mov.l @r15+,r10
    mov.l @r15+,r9
    mov.l @r15+,r8
    ldc r1,sr
    rts
    nop
.Lreject:
    mov r0,r3
    mov.l .Lcontext,r1
    add #64,r1
    mov.l @r1,r2
    add #1,r2
    mov.l r2,@r1
    mov #0,r0
    mov #1,r1
    cmp/eq r4,r1
    bf .Lreject_restore
    mov #1,r0
.Lreject_restore:
    ldc r3,sr
    rts
    nop
    .balign 4
.Lmask: .long 0x100000f0
.Lcontext: .long C_SYMBOL(kui_native_readahead_aux_context)
.Lbottom: .long __native_ra_stack_bottom
.Lhigh: .long __native_ra_stack_top-16
.Lstack: .long __native_ra_stack_top-32
.Lcanary: .long 0x4b524147
#if KUI_TOY_WIRE_CACHED
.Lmmucr: .long 0xff000010
.Lcached: .long 0xa0000000
#endif
.Ldispatch: .long C_SYMBOL(kui_native_readahead_aux_dispatch)
    .size C_SYMBOL(kui_native_readahead_aux_visit),.-C_SYMBOL(kui_native_readahead_aux_visit)

/* Test 74's hand-written block loops: on with the test unless a build
 * leaves them out (include/kui/toy_wire_report.h has the same default). */
#if defined(KUI_TOY_WIRE_LEAN) && KUI_TOY_WIRE_LEAN && (!defined(KUI_TOY_WIRE_SWIFT) || KUI_TOY_WIRE_SWIFT)
#include "toy_wire_words.inc"
    .section .text.native_ra_entry,"ax"
#endif
#if KUI_TOY_SUITE_MODE >= 39 && KUI_TOY_SUITE_MODE <= 66
#include "toy_movie_exit_hook.inc"
#endif
#if KUI_TOY_SUITE_MODE == 42 || KUI_TOY_SUITE_MODE == 44 || KUI_TOY_SUITE_MODE == 45 || KUI_TOY_SUITE_MODE == 46 || KUI_TOY_SUITE_MODE == 47 || KUI_TOY_SUITE_MODE == 48 || KUI_TOY_SUITE_MODE == 49 || KUI_TOY_SUITE_MODE == 50 || KUI_TOY_SUITE_MODE == 51 || KUI_TOY_SUITE_MODE == 52 || KUI_TOY_SUITE_MODE == 53 || KUI_TOY_SUITE_MODE == 54 || KUI_TOY_SUITE_MODE == 55 || KUI_TOY_SUITE_MODE == 56 || KUI_TOY_SUITE_MODE == 57 || KUI_TOY_SUITE_MODE == 58 || KUI_TOY_SUITE_MODE == 59 || KUI_TOY_SUITE_MODE == 60 || KUI_TOY_SUITE_MODE == 61 || KUI_TOY_SUITE_MODE == 62 || KUI_TOY_SUITE_MODE == 64 || KUI_TOY_SUITE_MODE == 65 || KUI_TOY_SUITE_MODE == 66
#include "toy_movie_chunk_hook.inc"
#endif
#if KUI_TOY_SUITE_MODE == 57 || KUI_TOY_SUITE_MODE == 60 || KUI_TOY_SUITE_MODE == 61 || KUI_TOY_SUITE_MODE == 62 || KUI_TOY_SUITE_MODE == 64 || KUI_TOY_SUITE_MODE == 65 || KUI_TOY_SUITE_MODE == 66
#include "toy_movie_refill_hook.inc"
#endif
#if KUI_TOY_SUITE_MODE == 60 || KUI_TOY_SUITE_MODE == 61 || KUI_TOY_SUITE_MODE == 62 || KUI_TOY_SUITE_MODE == 64 || KUI_TOY_SUITE_MODE == 65 || KUI_TOY_SUITE_MODE == 66
#include "toy_movie_present_hook.inc"
#endif
