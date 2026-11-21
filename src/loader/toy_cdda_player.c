/* SPDX-License-Identifier: GPL-3.0-only */
/* Test 84: CD music in the Background reader, the worker's player
 * (docs/cdda-background-test-84.md).
 *
 * The auxiliary calls kui_toy_cdda_player_visit after its guard on every visit
 * it admits, and once more with KUI_TOY_CDDA_KEEP at the end of a delivery
 * visit from one of the game's GD calls (include/kui/native_readahead.h). It
 * runs on the auxiliary's private stack, under its gate (SR.BL and IMASK 15),
 * and calls nothing of the auxiliary's. The cold stage finds it through the
 * worker's descriptor behind the loader timeline's
 * (src/loader/vmu_native_trace_hooks.S) and proves it lies in the worker.
 *
 * HOOK alone: the player is called and does nothing. */
#include <stdint.h>
#include "kui/vmu_native_background.h"
/* The worker's view of the reader's region, as its reader capture compiles it
 * (src/loader/vmu_native_reader_capture.inc): the layout of the background
 * reader, without its implementation. */
#ifndef KUI_RETAIL_ASYNC
#define KUI_TOY_CDDA_LAYOUT_VIEW 1
#define KUI_RETAIL_ASYNC 1
#define KUI_RETAIL_GD_ASYNC 1
#define KUI_RETAIL_TRANSPORT 1
#define KUI_SCI_SD_NO_BLOCK 1
#define KUI_GENERAL_SCI_RESUME_FIX 1
#endif
#include "retail_async.h"
#include "kui/native_readahead.h"
#ifdef KUI_TOY_CDDA_LAYOUT_VIEW
#undef KUI_RETAIL_ASYNC
#undef KUI_RETAIL_GD_ASYNC
#undef KUI_RETAIL_TRANSPORT
#undef KUI_SCI_SD_NO_BLOCK
#undef KUI_GENERAL_SCI_RESUME_FIX
#undef KUI_TOY_CDDA_LAYOUT_VIEW
#endif

#if !KUI_TOY_CDDA_HOOK
#error "The CD-music player is Test 84's (KUI_TOY_CDDA_HOOK)"
#endif

uint32_t kui_toy_cdda_player_visit(uint32_t event,uint32_t arg,
    kui_retail_async_region_type *r,const struct kui_native_ra_low_ops *ops) {
    (void)event;(void)arg;(void)r;(void)ops;
    return 0u;
}
