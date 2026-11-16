/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_TOY_MOVIE_TRACE_H
#define KUI_TOY_MOVIE_TRACE_H
#include <stdbool.h>
#include <stdint.h>

/* Root ISO entries, ordered 50/60 Hz for bumper, then 50/60 Hz for intro. */
struct kui_toy_movie_range { uint32_t lba, count; };
#if defined(KUI_TOY_SUITE_MODE)
struct kui_toy_suite_seed { uint32_t base[2], count[2]; };
#endif
struct kui_toy_movie_stats {
    union {
        struct { uint32_t completed, sectors, failed, bad_clock; };
        uint32_t totals[4];
    };
    union {
        struct { uint32_t longest, longest_lba, longest_count, gap, gap_lba; };
        uint32_t timing[5];
    };
    union {
        struct { uint32_t first_error, first_error_lba, first_error_count, first_error_bytes; };
        uint32_t error[4];
    };
};
#if KUI_NATIVE_TOY_PLAYBACK_CAPTURE
struct kui_toy_playback_stats {
    union {
        struct { uint32_t sectors, span, end_lba, failed, bad_clock; };
        uint32_t progress[5];
    };
    union {
        struct { uint32_t foreground, foreground_wait, sci_irq, foreground_max, sci_irq_max; };
        uint32_t costs[5];
    };
    union {
#if KUI_NATIVE_TOY_HEADER_REPLAY
        struct { uint32_t gap, gap_lba, foreground_blocks, sci_irq_blocks, replayed; };
#else
        struct { uint32_t gap, gap_lba, foreground_blocks, sci_irq_blocks, longest; };
#endif
        uint32_t reads[5];
    };
};
#endif
#endif
