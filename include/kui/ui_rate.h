/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_UI_RATE_H
#define KUI_UI_RATE_H
#include "kui/options.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Should the UI thread redraw on this loop iteration? Pure, so the PC can test
 * it; main.c calls it once per loop.
 *
 * Idle, unthrottled (KUI_OPT_UI_FULL) and the first iteration after an
 * operation starts or ends always redraw. Otherwise it redraws when 1/hz
 * seconds have passed since the last draw; hz == 0 never does. It is keyed on
 * the time of the last draw, not on a deadline computed when that draw
 * happened, so a cap that changes mid-operation applies at once. (The bench
 * changes it between passes; a precomputed deadline would leave the screen
 * frozen for a whole pass after going from 'full' to a finite rate.) */
static inline bool kui_ui_redraw_due(bool busy, bool was_busy, unsigned hz,
                                     uint64_t now_ms, uint64_t last_draw_ms) {
    if(!busy || hz == KUI_OPT_UI_FULL || busy != was_busy) return true;
    return hz && now_ms - last_draw_ms >= 1000u / hz;
}
/* Every idle shell page polls independently of rendering. Fresh input and
 * published worker results render on this iteration; otherwise 30 Hz is
 * sufficient for clocks/music/status without repainting between every poll.
 * Busy capture/benchmark caps retain their original authority, including 0. */
static inline bool kui_ui_menu_redraw_due(bool busy, bool was_busy, unsigned hz,
        bool changed, uint64_t now_ms, uint64_t last_draw_ms) {
    if(busy) return kui_ui_redraw_due(busy,was_busy,hz,now_ms,last_draw_ms);
    return changed || was_busy || !last_draw_ms || now_ms-last_draw_ms>=33u;
}
/* Motion uses a start-to-start deadline. Painting and waiting for vblank
 * already consume part of that interval; adding an idle delay afterwards
 * made the short Flow animation advance in only a few slow frames. */
static inline bool kui_ui_menu_flow_redraw_due(bool motion,bool busy,bool was_busy,unsigned hz,
        bool changed,uint64_t now_ms,uint64_t last_draw_ms) {
    if(busy || !motion)return kui_ui_menu_redraw_due(busy,was_busy,hz,changed,now_ms,last_draw_ms);
    return changed || was_busy || !last_draw_ms || now_ms-last_draw_ms>=16u;
}
static inline uint64_t kui_ui_menu_frame_timestamp(bool busy,bool motion,
        uint64_t started_ms,uint64_t completed_ms) {
    return busy || motion?started_ms:completed_ms;
}
static inline unsigned kui_ui_menu_poll_delay(bool busy) {
    return busy?33u:8u;
}
/* A completed idle frame has already spent time drawing and waiting for
 * vertical blank. Yield so other threads run, then sample controls again;
 * do not add another 8 ms before observing a press that arrived during draw.
 * A non-drawing poll still sleeps, and operation pacing stays unchanged. */
static inline unsigned kui_ui_menu_wait_ms(bool busy,bool drew) {
    return !busy && drew?0u:kui_ui_menu_poll_delay(busy);
}
/* held contains only navigation bits; action buttons are never repeated.
 * An initial edge is already in pressed. Direction changes reset the hold
 * timer, while a late loop emits one step rather than a burst of catch-up. */
static inline unsigned kui_ui_navigation_repeat_configured(unsigned held,unsigned pressed,
        unsigned *previous_held,uint64_t *repeat_at,uint64_t now_ms,unsigned delay_ms,unsigned interval_ms) {
    if(held!=*previous_held) {
        *previous_held=held;*repeat_at=now_ms+delay_ms;
    } else if(held && now_ms>=*repeat_at) {
        pressed|=held;*repeat_at=now_ms+interval_ms;
    }
    return pressed;
}
static inline unsigned kui_ui_navigation_repeat(unsigned held,unsigned pressed,
        unsigned *previous_held,uint64_t *repeat_at,uint64_t now_ms) {
    return kui_ui_navigation_repeat_configured(held,pressed,previous_held,repeat_at,now_ms,250u,80u);
}
/* Games polls controls faster while idle without repainting the whole screen
 * on every poll. Input and completed listings/details appear immediately;
 * operation caps and their start/end edges keep the shared rule above. */
static inline bool kui_ui_games_redraw_due(bool games, bool busy, bool was_busy,
        unsigned hz, bool changed, uint64_t now_ms, uint64_t last_draw_ms) {
    if(!games || busy) return kui_ui_redraw_due(busy,was_busy,hz,now_ms,last_draw_ms);
    return kui_ui_menu_redraw_due(busy,was_busy,hz,changed,now_ms,last_draw_ms);
}
static inline unsigned kui_ui_poll_delay(bool games, bool busy) {
    return games && !busy ? 8u : 33u;
}
/* Idle menus check for changes at the cadence above, but only repaint when
 * the state the renderer reads has changed. The fingerprint is a cheap
 * position-sensitive checksum (not cryptographic): a pair of running sums
 * over aligned 32-bit words, then over any trailing bytes. A missed change
 * is still painted by the periodic refresh below. */
#define KUI_UI_IDLE_REFRESH_MS 1000u
struct kui_ui_fingerprint { uint32_t a,b; };
static inline void kui_ui_fingerprint_init(struct kui_ui_fingerprint *f) {
    f->a=UINT32_C(0x9e3779b9);f->b=UINT32_C(0x85ebca6b);
}
static inline void kui_ui_fingerprint_words(struct kui_ui_fingerprint *f,const uint32_t *words,size_t count) {
    uint32_t a=f->a,b=f->b;
    for(size_t i=0;i<count;i++) {a+=words[i];b+=a;}
    f->a=a;f->b=b;
}
static inline void kui_ui_fingerprint_bytes(struct kui_ui_fingerprint *f,const void *data,size_t bytes) {
    const unsigned char *p=(const unsigned char *)data;
    uint32_t a=f->a,b=f->b;
    for(size_t i=0;i<bytes;i++) {a+=p[i]+1u;b+=a;}
    f->a=a;f->b=b+(uint32_t)bytes;
}
/* NULL and "" are distinct; the terminator separates adjacent strings. */
static inline void kui_ui_fingerprint_text(struct kui_ui_fingerprint *f,const char *text) {
    uint32_t a=f->a,b=f->b;
    if(!text) {a+=UINT32_C(0x51ed27);b+=a;}
    else {
        for(const unsigned char *p=(const unsigned char *)text;*p;p++) {a+=*p+1u;b+=a;}
        a+=UINT32_C(0x2f);b+=a;
    }
    f->a=a;f->b=b;
}
static inline uint32_t kui_ui_fingerprint_value(const struct kui_ui_fingerprint *f) {
    return f->a^(f->b*UINT32_C(2654435761));
}
/* Skip a repaint only when a complete cached frame shows the same state and
 * the periodic refresh is not yet due. Input, published results, busy edges
 * and motion force a repaint through `force`. */
static inline bool kui_ui_frame_unchanged(bool force,bool frame_ready,bool have_previous,
        uint32_t previous,uint32_t current,uint64_t now_ms,uint64_t drawn_ms) {
    return !force && frame_ready && have_previous && previous==current &&
        now_ms>=drawn_ms && now_ms-drawn_ms<KUI_UI_IDLE_REFRESH_MS;
}
#endif
