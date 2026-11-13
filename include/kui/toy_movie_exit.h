/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_TOY_MOVIE_EXIT_H
#define KUI_TOY_MOVIE_EXIT_H

#define KUI_TOY_MOVIE_EXIT_OFFSET 656
#define KUI_TOY_MOVIE_EXIT_BYTES 36
#define KUI_TOY_MOVIE_EXIT_STARTED 1
#define KUI_TOY_MOVIE_EXIT_TERMINAL 2
#define KUI_TOY_MOVIE_EXIT_INPUT_STOP 4
#define KUI_TOY_MOVIE_EXIT_FOLLOWUP_STOP 8
#define KUI_TOY_MOVIE_EXIT_LARGER_BUFFER 16
#define KUI_TOY_MOVIE_EXIT_MORE_PREFILL 32

#ifndef __ASSEMBLER__
#include <stdint.h>

struct kui_toy_movie_exit_witness {
    uint32_t flags, start_status;
#if KUI_TOY_SUITE_MODE == 49 || KUI_TOY_SUITE_MODE == 50 || KUI_TOY_SUITE_MODE == 51 || KUI_TOY_SUITE_MODE == 52 || KUI_TOY_SUITE_MODE == 53 || KUI_TOY_SUITE_MODE == 54 || KUI_TOY_SUITE_MODE == 55 || KUI_TOY_SUITE_MODE == 56 || KUI_TOY_SUITE_MODE == 57 || KUI_TOY_SUITE_MODE == 58 || KUI_TOY_SUITE_MODE == 59 || KUI_TOY_SUITE_MODE == 60 || KUI_TOY_SUITE_MODE == 61 || KUI_TOY_SUITE_MODE == 62 || KUI_TOY_SUITE_MODE == 64 || KUI_TOY_SUITE_MODE == 65 || KUI_TOY_SUITE_MODE == 66
    uint32_t output_frames, skipped_frames;
#else
    uint32_t start_frame, start_samples;
#endif
    uint32_t status, frame, samples, lag, limit;
};

_Static_assert(sizeof(struct kui_toy_movie_exit_witness)==KUI_TOY_MOVIE_EXIT_BYTES,
    "Toy movie exit witness assembly stride");
#endif
#endif
