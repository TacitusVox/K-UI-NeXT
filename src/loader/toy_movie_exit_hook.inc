/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/toy_movie_exit.h"

#define TOY_INTRO_OPEN_POLICY (KUI_TOY_SUITE_MODE == 54 || KUI_TOY_SUITE_MODE == 56)
#define TOY_MOVIE_OUTPUT_WITNESS (KUI_TOY_SUITE_MODE >= 49 && KUI_TOY_SUITE_MODE <= 62)
#define TOY_MOVIE_PRESENT_WITNESS (KUI_TOY_SUITE_MODE >= 60 && KUI_TOY_SUITE_MODE <= 62)

/* Installed at the exact original movie-update pointer. Tail-call the
 * original for every untracked caller. Capture start and first terminal
 * results using the existing uncached auxiliary context. The intro-buffer
 * profiles consider available space only at the original start call. */
    .section .text.toy_movie_exit,"ax"
    .balign 4
    .global C_SYMBOL(kui_toy_movie_exit_hook)
    .type C_SYMBOL(kui_toy_movie_exit_hook),@function
C_SYMBOL(kui_toy_movie_exit_hook):
    movt r7
    sts pr,r1
    mov.l .Lexit_start_return,r2
    cmp/eq r2,r1
    bf .Lexit_maybe_update
    mov.w .Lexit_start_cmd,r2
    cmp/eq r2,r5
#if TOY_INTRO_OPEN_POLICY
    bf .Lexit_tail_jump
    bra .Lexit_opening_buffer
    nop
.Lexit_tail_jump:
#else
    bt .Lexit_call
#endif
    bra .Lexit_tail
    nop
.Lexit_maybe_update:
    mov.l .Lexit_update_return,r2
    cmp/eq r2,r1
    bf .Lexit_maybe_followup
    tst r5,r5
    bt .Lexit_call
    bra .Lexit_check_stop
    nop
.Lexit_maybe_followup:
    mov.l .Lexit_followup_return,r2
    cmp/eq r2,r1
#if TOY_INTRO_OPEN_POLICY
    bf .Lexit_tail_jump
#elif TOY_MOVIE_PRESENT_WITNESS
    bf .Lexit_tail_short
#else
    bf .Lexit_tail
#endif
.Lexit_check_stop:
    mov #4,r2
    cmp/eq r2,r5
#if TOY_INTRO_OPEN_POLICY
    bf .Lexit_tail_jump
#elif TOY_MOVIE_PRESENT_WITNESS
    bt .Lexit_call
.Lexit_tail_short:
    bra .Lexit_tail
    nop
#else
    bf .Lexit_tail
#endif
.Lexit_call:
#if TOY_INTRO_OPEN_POLICY
    mov #0,r2
.Lexit_call_marked:
#endif
    mov.l r4,@-r15
    mov.l r5,@-r15
    sts.l pr,@-r15
#if TOY_INTRO_OPEN_POLICY
    mov.l r2,@-r15
#else
    add #-4,r15
#endif
    mov.l .Lexit_original,r1
    jsr @r1
    shlr r7
    movt r7
#if TOY_INTRO_OPEN_POLICY
    mov.l @r15+,r2
    or r2,r7
#else
    add #4,r15
#endif
    lds.l @r15+,pr
    mov.l @r15+,r5
    mov.l @r15+,r4
    mov.w .Lexit_start_cmd,r2
    cmp/eq r2,r5
    bt/s .Lexit_name
    mov #KUI_TOY_MOVIE_EXIT_STARTED,r6
    mov #KUI_TOY_MOVIE_EXIT_TERMINAL,r6
    cmp/pz r0
    bf .Lexit_name
    mov.w .Lexit_stop_status,r2
    cmp/eq r2,r0
    bf .Lexit_done
.Lexit_name:
    mov.w .Lexit_name_offset,r1
    add r4,r1
    mov.l @r1+,r3
    mov.l .Lexit_name_ntsc,r2
    cmp/eq r2,r3
    bt .Lexit_movie
    mov.l .Lexit_name_pal,r2
    cmp/eq r2,r3
    bf .Lexit_done
.Lexit_movie:
    mov.l @r1+,r3
    mov.l .Lexit_bumper1,r2
    cmp/eq r2,r3
    bf .Lexit_intro
    mov.l @r1+,r3
    mov.l .Lexit_bumper2,r2
    cmp/eq r2,r3
    bf .Lexit_done
    mov.l @r1+,r3
    mov.l .Lexit_bumper3,r2
    cmp/eq r2,r3
    bf .Lexit_done
    mov.l @r1+,r3
    mov.l .Lexit_bumper4,r2
    cmp/eq r2,r3
    bf .Lexit_done
    mov.b @r1,r3
    tst r3,r3
    bf .Lexit_done
    mov.l .Lexit_witness,r2
    bra .Lexit_record
    nop
.Lexit_intro:
    mov.l .Lexit_intro1,r2
    cmp/eq r2,r3
    bf .Lexit_done
    mov.l @r1+,r3
    mov.l .Lexit_intro2,r2
    cmp/eq r2,r3
    bf .Lexit_done
    mov.l @r1+,r3
    mov.l .Lexit_intro3,r2
    cmp/eq r2,r3
    bf .Lexit_done
    mov.l @r1,r3
    mov.l .Lexit_intro4,r2
    cmp/eq r2,r3
    bf .Lexit_done
    mov.l .Lexit_witness,r2
    add #KUI_TOY_MOVIE_EXIT_BYTES,r2
.Lexit_record:
    mov.l @r2,r1
    mov #KUI_TOY_MOVIE_EXIT_STARTED,r3
    cmp/eq r3,r6
    bf .Lexit_terminal
    tst r1,r1
    bf .Lexit_done
    mov.l r0,@(4,r2)
#if TOY_INTRO_OPEN_POLICY
    mov r7,r3
#if KUI_TOY_SUITE_MODE == 56
    mov #KUI_TOY_MOVIE_EXIT_MORE_PREFILL,r1
#else
    mov #KUI_TOY_MOVIE_EXIT_LARGER_BUFFER,r1
#endif
    and r1,r3
    or r3,r6
    mov #1,r3
    and r3,r7
#endif
#if !TOY_MOVIE_OUTPUT_WITNESS
    mov.w .Lexit_frame_offset,r3
    add r4,r3
    mov.l @r3,r3
    mov.l r3,@(8,r2)
    mov.w .Lexit_sample_offset,r3
    add r4,r3
    mov.l @r3,r3
    mov.l r3,@(12,r2)
#endif
#if TOY_MOVIE_PRESENT_WITNESS
    mov.l r6,@r2
    bra .Lexit_present_boundary
    nop
#else
    bra .Lexit_done
    mov.l r6,@r2
#endif
.Lexit_terminal:
    tst r3,r1
    bt .Lexit_done
    tst r6,r1
    bf .Lexit_done
    or r6,r1
    mov #KUI_TOY_MOVIE_EXIT_INPUT_STOP,r3
    cmp/eq r3,r5
    bf .Lexit_terminal_fields
    sts pr,r6
    mov.l .Lexit_followup_return,r3
    cmp/eq r3,r6
    bf/s .Lexit_publish_stop_kind
    mov #KUI_TOY_MOVIE_EXIT_INPUT_STOP,r3
    mov #KUI_TOY_MOVIE_EXIT_FOLLOWUP_STOP,r3
.Lexit_publish_stop_kind:
    or r3,r1
.Lexit_terminal_fields:
    mov.l r0,@(16,r2)
#if TOY_MOVIE_OUTPUT_WITNESS
    /* These are the owner's output-callback and no-output advance
     * counters, separate from its overall frame cursor. */
    mov.w .Lexit_output_offset,r3
    add r4,r3
    mov.l @r3,r3
    mov.l r3,@(8,r2)
    mov.w .Lexit_skip_offset,r3
    add r4,r3
    mov.l @r3,r3
    mov.l r3,@(12,r2)
#endif
    mov.w .Lexit_frame_offset,r3
    add r4,r3
    mov.l @r3,r3
    mov.l r3,@(20,r2)
    mov.w .Lexit_sample_offset,r3
    add r4,r3
    mov.l @r3,r3
    mov.l r3,@(24,r2)
    mov.w .Lexit_lag_status,r3
    cmp/eq r3,r0
    bf .Lexit_publish
    mov.l .Lexit_lag,r3
    mov.l @r3,r3
    mov.l r3,@(28,r2)
    mov.w .Lexit_limit_offset,r3
    add r4,r3
    mov.l @r3,r3
    shll r3
    mov.l r3,@(32,r2)
.Lexit_publish:
    mov.l r1,@r2
#if TOY_MOVIE_PRESENT_WITNESS
    mov #KUI_TOY_MOVIE_EXIT_TERMINAL,r6
.Lexit_present_boundary:
    /* R4 is the exact game owner and R2 the identified witness slot. */
    add #-16,r15
    sts pr,r1
    mov.l r1,@(12,r15)
    mov.l r7,@(8,r15)
    mov.l r0,@(4,r15)
    mov.l .Lexit_present_boundary_handler,r1
    jsr @r1
    mov r2,r5
    mov.l @(12,r15),r1
    lds r1,pr
    mov.l @(4,r15),r0
    mov.l @(8,r15),r7
    add #16,r15
#endif
.Lexit_done:
    rts
    shlr r7
.Lexit_tail:
    mov.l .Lexit_original,r1
    jmp @r1
    shlr r7

#if TOY_INTRO_OPEN_POLICY
.Lexit_start_cmd: .word 0x0200
.Lexit_stop_status: .word 0x07dc
.Lexit_lag_status: .word 0xf843
.Lexit_name_offset: .word 0x00bc
.Lexit_frame_offset: .word 0x01c0
.Lexit_sample_offset: .word 0x0244
.Lexit_limit_offset: .word 0x00a0
.Lexit_output_offset: .word 0x00ac
.Lexit_skip_offset: .word 0x0090

.Lexit_opening_buffer:
    /* The wrapper passes its local context at SP+4. At the recognized
     * START dispatch the owner has reserved a further 12 stack bytes. */
    mov r15,r1
    add #16,r1
    cmp/eq r1,r4
    bf .Lexit_normal_buffer
    mov.w .Lexit_opening_name_offset,r1
    add r4,r1
    mov.l @r1+,r3
    mov.l .Lexit_name_ntsc,r2
    cmp/eq r2,r3
    bt .Lexit_opening_name
    mov.l .Lexit_name_pal,r2
    cmp/eq r2,r3
    bf .Lexit_normal_buffer
.Lexit_opening_name:
    mov.l @r1+,r3
    mov.l .Lexit_intro1,r2
    cmp/eq r2,r3
    bf .Lexit_normal_buffer
    mov.l @r1+,r3
    mov.l .Lexit_intro2,r2
    cmp/eq r2,r3
    bf .Lexit_normal_buffer
    mov.l @r1+,r3
    mov.l .Lexit_intro3,r2
    cmp/eq r2,r3
    bf .Lexit_normal_buffer
    mov.l @r1,r3
    mov.l .Lexit_intro4,r2
    cmp/eq r2,r3
    bf .Lexit_normal_buffer

    /* Respect any existing caller overrides. */
    mov.w .Lexit_buffer_offset,r1
    add r4,r1
    mov.l @r1+,r3
    mov.l .Lexit_buffer_1m,r2
    cmp/eq r2,r3
    bf .Lexit_normal_buffer
    mov.l @r1,r3
    mov #-1,r2
    cmp/eq r2,r3
    bf .Lexit_normal_buffer
#if KUI_TOY_SUITE_MODE == 56
    /* 960 KiB gives 192 KiB more lead in the original 1 MiB ring. */
    mov.l .Lexit_prefill,r3
    mov #KUI_TOY_MOVIE_EXIT_MORE_PREFILL,r2
    bra .Lexit_call_marked
    mov.l r3,@r1
#else
    mov.l .Lexit_allocator_slot,r1
    mov.l @r1,r3
    mov.l .Lexit_default_allocator,r2
    cmp/eq r2,r3
    bf .Lexit_normal_buffer

    /* The SDK query takes pointers to total and largest free sizes.
     * Keep one further MiB available in the same contiguous free block
     * for the original player setup, plus the split block header. The
     * original allocator owns the actual allocation and all cleanup. */
    mov.l r0,@-r15
    mov.l r4,@-r15
    mov.l r5,@-r15
    mov.l r6,@-r15
    mov.l r7,@-r15
    sts.l pr,@-r15
    add #-8,r15
    mov r15,r4
    mov r15,r5
    add #4,r5
    mov.l .Lexit_query_allocator,r1
    jsr @r1
    nop
    mov.l @(4,r15),r2
    add #8,r15
    lds.l @r15+,pr
    mov.l @r15+,r7
    mov.l @r15+,r6
    mov.l @r15+,r5
    mov.l @r15+,r4
    mov.l @r15+,r0
    mov.l .Lexit_buffer_min_free,r3
    cmp/hs r3,r2
    bf .Lexit_normal_buffer
    mov.w .Lexit_buffer_offset,r1
    add r4,r1
    mov.l .Lexit_buffer_2m,r3
    mov #KUI_TOY_MOVIE_EXIT_LARGER_BUFFER,r2
    bra .Lexit_call_marked
    mov.l r3,@r1
#endif
.Lexit_normal_buffer:
    bra .Lexit_call
    nop
#endif

    .balign 4
.Lexit_original: .long 0x8c08d638
.Lexit_start_return: .long 0x8c06d3f0
.Lexit_update_return: .long 0x8c06d39a
.Lexit_followup_return: .long 0x8c06d3c0
.Lexit_witness: .long C_SYMBOL(kui_native_readahead_aux_context)+KUI_TOY_MOVIE_EXIT_OFFSET
#if TOY_MOVIE_PRESENT_WITNESS
.Lexit_present_boundary_handler: .long C_SYMBOL(kui_toy_movie_present_boundary)
#endif
.Lexit_lag: .long 0x8c1138c4
.Lexit_name_ntsc: .ascii "60hz"
.Lexit_name_pal: .ascii "50hz"
.Lexit_bumper1: .ascii "_Bum"
.Lexit_bumper2: .ascii "per_"
.Lexit_bumper3: .ascii "LRTF"
.Lexit_bumper4: .ascii ".avi"
.Lexit_intro1: .ascii "_Int"
.Lexit_intro2: .ascii "ro_L"
.Lexit_intro3: .ascii "RTF."
.Lexit_intro4: .asciz "avi"
#if !TOY_INTRO_OPEN_POLICY
.Lexit_start_cmd: .word 0x0200
.Lexit_stop_status: .word 0x07dc
.Lexit_lag_status: .word 0xf843
.Lexit_name_offset: .word 0x00bc
.Lexit_frame_offset: .word 0x01c0
.Lexit_sample_offset: .word 0x0244
.Lexit_limit_offset: .word 0x00a0
#if TOY_MOVIE_OUTPUT_WITNESS
.Lexit_output_offset: .word 0x00ac
.Lexit_skip_offset: .word 0x0090
#endif
#else
.Lexit_opening_name_offset: .word 0x00bc
.Lexit_buffer_offset: .word 0x01dc
    .balign 4
.Lexit_buffer_1m: .long 0x00100000
#if KUI_TOY_SUITE_MODE == 56
.Lexit_prefill: .long 0x000f0000
#else
.Lexit_buffer_2m: .long 0x00200000
.Lexit_buffer_min_free: .long 0x00300020
.Lexit_allocator_slot: .long 0x8c117dbc
.Lexit_default_allocator: .long 0x8c0c5ac8
.Lexit_query_allocator: .long 0x8c0b3e6e
#endif
#endif
    .size C_SYMBOL(kui_toy_movie_exit_hook),.-C_SYMBOL(kui_toy_movie_exit_hook)
