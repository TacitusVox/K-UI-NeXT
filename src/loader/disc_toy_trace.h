/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_DISC_TOY_TRACE_H
#define KUI_DISC_TOY_TRACE_H
#include "kui/toy_movie_trace.h"
#include "kui/toy_suite_report_wire.h"
#include "retail_display.h"

#define KUI_DISC_TOY_TRACE_LEASE 0x8cfd0000u
#define KUI_DISC_TOY_TRACE_BYTES 0x00030000u
#define KUI_DISC_TOY_TRACE_LINK 0xacfd0000u
#define KUI_DISC_TOY_TRACE_GD_VECTOR 0xac0000bcu
#define KUI_DISC_TOY_TRACE_TEST 63u

struct kui_disc_toy_trace_config {
    uint32_t firmware;
    struct retail_display_state display;
    struct kui_toy_movie_range movie[4];
    struct kui_toy_report_wire report;
};

struct kui_disc_toy_trace_exports {
    uint32_t proxy, finish, config, binary_end, bss_end, heap_guard;
    uint32_t exit, render, scene_slot, scene;
};

#endif
