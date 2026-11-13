/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_TOY_FMV_COMPARE_H
#define KUI_TOY_FMV_COMPARE_H
#include "kui/toy_movie_trace.h"
#include "kui/toy_suite_report_wire.h"

#define KUI_TOY_FMV_BINS 13u
#define KUI_TOY_FMV_BIN_TICKS 390625u
#define KUI_TOY_FMV_BIN_TEXT_BYTES 19u
#define KUI_TOY_FMV_PENDING 4u

struct kui_toy_fmv_bin {
    uint32_t requested, completed, longest, gap;
};
struct kui_toy_fmv_movie {
    struct kui_toy_fmv_bin bins[KUI_TOY_FMV_BINS];
    uint32_t first_tick, last_ack, last_lba, end_lba;
    uint32_t requests, acks, failed, short_bytes, unmatched;
};
struct kui_toy_fmv_pending {
    uint32_t token, tick, lba, sectors, movie;
};
struct kui_toy_fmv_trace {
    struct kui_toy_fmv_movie movie[2];
    struct kui_toy_fmv_pending pending[KUI_TOY_FMV_PENDING];
    uint32_t clock_lost;
};
void kui_toy_fmv_request(struct kui_toy_fmv_trace *, const struct kui_toy_movie_range [4],
    uint32_t token, uint32_t lba, uint32_t sectors, uint32_t tick);
void kui_toy_fmv_status(struct kui_toy_fmv_trace *, uint32_t token, uint32_t tick,
    int32_t status, uint32_t bytes);
void kui_toy_fmv_format_bin(char text[KUI_TOY_FMV_BIN_TEXT_BYTES], const struct kui_toy_fmv_bin *);
void kui_toy_fmv_format(char text[KUI_TOY_REPORT_FILE_BYTES], const struct kui_toy_fmv_trace *,
    const char *source, const char *build);
#endif
