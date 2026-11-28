/* Review harness (build/review4 only): the UNMODIFIED src/loader/toy_cdda_player.c
 * (with src/core/retail_gd.c) and the UNMODIFIED src/loader/toy_cdda_output.c
 * in one program, against the output test's model of the sound chip, its
 * driver, the card and the bus (model.inc, generated from
 * tests/test_toy_cdda_output.c with its own model of the hold deleted), and a
 * clock that counts down on every read of TCNT0 and on every bus access.
 * The game drives the real player through its real entry points (the
 * delivery visit before each GD call, then the real disc service), as
 * tests/test_toy_cdda_drive.c does. kui_toy_cdda_player_hold is observed
 * through the linker's --wrap: the real function runs unchanged. */
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kui/vmu_native_background.h"
#include "retail_async.h"
#include "kui/native_readahead.h"
#include "kui/retail_gd.h"
#include "kui/toy_cdda_card.h"
#include "kui/toy_cdda_output.h"
#include "kui/toy_cdda_player.h"
#include "kui/toy_cdda_survey.h"

static uint32_t seed_now;
static uint32_t read_ticks = 1u;     /* what a read of TCNT0 costs */
static uint32_t player_read_ticks = 1u;   /* and one of the player's (the same, unless the run says otherwise) */
static void model_fail(int line, const char *what);
static void wrong_frame(unsigned long long k, int sl, int sr, int el, int er);
static uint32_t clock_read(void);

#include "model.inc"

static bool sounding(void) { return voice[pair[0]].active && voice[pair[1]].active; }
/* ---- violations ---- */
static unsigned long violations, seeds_violated;
static bool violated;
static char first_violation[512];
static FILE *trace;
static int verbose;
static void viol(const char *fmt, ...) {
    char text[400];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    ++violations;
    if(!violated) {
        violated = true;
        ++seeds_violated;
        snprintf(first_violation, sizeof(first_violation), "%s", text);
        printf("VIOLATION seed %u t=%llu (tcnt %08x): %s\n", seed_now, (unsigned long long)now, tcnt(), text);
        printf("   D: state %u gen %u first %u end %u passes %u done %u base_counter %08x base_offset %d frozen %u hold %u held %u held_open %u held_at %08x\n",
            D.state, D.generation, D.first, D.end, D.passes, D.done, D.base_counter, D.base_offset, D.frozen, D.hold, D.held, D.held_open, D.held_at);
        printf("   O: phase %u why %u gen %u started %u sector %u passes %u written %u played %u finished %u keys %u restarts %u underruns %u\n",
            O.phase, O.why, O.generation, O.started, O.sector, O.passes, O.written, O.played, O.finished, O.keys, O.restarts, O.underruns);
    }
}
static void model_fail(int line, const char *what) {
    printf("MODEL CHECK seed %u t=%llu model.inc:%d: %s\n", seed_now, (unsigned long long)now, line, what);
    printf("   D: state %u gen %u first %u end %u passes %u done %u base_counter %08x base_offset %d frozen %u hold %u held %u held_open %u held_at %08x\n",
        D.state, D.generation, D.first, D.end, D.passes, D.done, D.base_counter, D.base_offset, D.frozen, D.hold, D.held, D.held_open, D.held_at);
    printf("   O: phase %u why %u gen %u started %u sector %u passes %u written %u played %u finished %u keys %u restarts %u\n",
        O.phase, O.why, O.generation, O.started, O.sector, O.passes, O.written, O.played, O.finished, O.keys, O.restarts);
    fflush(stdout);
    exit(2);
}
static bool ring_dry;
static unsigned long dry_rings;
static void wrong_frame(unsigned long long k, int sl, int sr16, int el, int er) {
    /* A ring the chip plays beyond what was written of it (no heartbeat
     * came, or the card handed nothing on) plays what lay there, and its
     * decoders are then not the play's until it is keyed on again. */
    if(ring_dry)return;
    /* (Or, behind the play's last frame as the output wrote it, the
     * silence of a play that ended for the drive before its ring did; the
     * model allows that only while no new play has been asked.) */
    if(k >= O.written || (O.finished && k >= O.audio_end)) {
        ring_dry = true;
        ++dry_rings;
        return;
    }
    viol("frame %llu of the ring is not the play's: played %d %d, the play has %d %d (ring from sector %u passes %u)",
        k, sl, sr16, el, er, heard_play.base, heard_play.passes);
    ring_dry = true;
}
#define TR(...) do { if(trace) { fprintf(trace, "%10llu ", (unsigned long long)now); fprintf(trace, __VA_ARGS__); fputc('\n', trace); } } while(0)

/* ---- the clock: every read of TCNT0 costs ---- */
static bool capture_pending;
static void takeup_captured(void);
static uint32_t clock_read(void) {
    const uint32_t value = tcnt();
    now += read_ticks;
    /* The first read behind drive_now() at a take-up: O.sector is the
     * ring's first, before any of it has been read from the card. */
    if(capture_pending && O.phase == KUI_TOY_CDDA_OUT_PRIME) {
        capture_pending = false;
        takeup_captured();
    }
    return value;
}
uint32_t kui_native_readahead_aux_test_read(uint32_t address, unsigned width) {
    switch(address) {
    case UINT32_C(0xffc00000): CHECK(width == 2u); return frqcr;
    case UINT32_C(0xffd80004): CHECK(width == 1u); return tstr;
    case UINT32_C(0xffd80008): CHECK(width == 4u); return tcor;
    case UINT32_C(0xffd80010): CHECK(width == 2u); return tcr;
    case UINT32_C(0xffd8000c): {
        CHECK(width == 4u);
        const uint32_t value = tcnt();
        now += player_read_ticks;
        return value;
    }
    default: CHECK(!"a register the player does not read"); return 0u;
    }
}

/* ---- the disc service and the game's calls (tests/test_toy_cdda_drive.c) ---- */
#define BEGIN UINT32_C(0x8c010000)
#define PARAM (BEGIN + 0x000u)
#define OUT (BEGIN + 0x100u)
#define STAT (BEGIN + 0x200u)
static uint8_t guest[0x2000];
static uint8_t *gmap(void *context, uint32_t address, uint32_t bytes, int writing) {
    (void)writing; (void)context;
    if(address < BEGIN || address - BEGIN > sizeof(guest) || bytes > sizeof(guest) - (address - BEGIN))return NULL;
    return guest + (address - BEGIN);
}
static int grange(void *context, uint32_t lba, uint32_t count, uint32_t bytes) {
    (void)context; (void)bytes;
    return lba >= 45000u && lba + count <= 60000u ? 0 : -1;
}
static int greader(void *context, uint32_t lba, uint32_t count, uint32_t bytes, void *out) {
    (void)context; (void)lba;
    memset(out, 0, (size_t)count * bytes);
    return 0;
}
static void gput(uint32_t address, uint32_t value) { memcpy(gmap(0, address, 4u, 1), &value, 4u); }
static uint32_t gget(uint32_t address) { uint32_t v; memcpy(&v, gmap(0, address, 4u, 0), 4u); return v; }
/* The disc of the output's model (toc[]): audio 1000..4000, a gap,
 * 4151..7000, 7000..9000, between two data tracks. */
static const union kui_retail_slot tracks[5] = {
    {.track = {.start_lba = 0u, .end_lba = 1000u, .control = 4u}},
    {.track = {.start_lba = 1000u, .end_lba = 4000u, .control = 0u}},
    {.track = {.start_lba = 4151u, .end_lba = 7000u, .control = 0u}},
    {.track = {.start_lba = 7000u, .end_lba = 9000u, .control = 0u}},
    {.track = {.start_lba = 45000u, .end_lba = 60000u, .control = 4u}},
};
static kui_retail_async_region_type R;
#define S (R.shared.service)

/* ---- the oracle of the play's position, from what was observed ---- */
#define HT KUI_TOY_CDDA_HOLD_TICKS
#define G3 KUI_TOY_CDDA_TICKS_3_SECTORS
static uint64_t sectors_of(int64_t n) {
    if(n <= 0)return 0u;
    return (uint64_t)n / G3 * 3u + (uint64_t)n % G3 * 3u / G3;
}
static uint64_t time_of(uint32_t count) { return (uint64_t)(uint32_t)(TCNT_START - count); }
static struct {
    bool live;                 /* a play is PLAYING */
    uint64_t anchor_lo, anchor_hi, anchor_abs;   /* the play stood at anchor_abs at a time in [lo, hi] */
    uint64_t stood_lo, stood_hi;                 /* ticks stood under holds that are over */
    bool standing;
    uint64_t stand_lo, stand_hi;                 /* the hold that stands was granted in [lo, hi] */
    int64_t hold_abs;                            /* where a held play was first seen (-1: not yet) */
    uint64_t last_abs, last_time;
    bool have_last;
    uint64_t win_stood;                          /* ticks stood in the window that is open (upper) */
    unsigned windows, keys;                      /* since the play began or was released */
    uint64_t began;                              /* the PLAY, for the play's length in time */
    uint64_t paused_ticks;
    uint64_t paused_at;
    unsigned key_ons;                            /* since the PLAY */
    unsigned holds_opened;                       /* since the PLAY */
    unsigned released;                           /* since the PLAY */
} P;
static uint64_t abs_of(const struct kui_toy_cdda_drive *d) {
    return (uint64_t)d->done * (d->end - d->first) + d->frozen;
}
static uint64_t total_of(const struct kui_toy_cdda_drive *d) {
    return (uint64_t)d->passes * (d->end - d->first);
}
/* The bounds of where the play stands at a time in [t0, t1]. */
static void expect(uint64_t t0, uint64_t t1, const struct kui_toy_cdda_drive *before, uint64_t *lo, uint64_t *hi) {
    uint64_t cur_lo = 0u, cur_hi = 0u;
    if(P.standing) {
        const uint64_t limit = time_of(before->held_at) + HT;
        const uint64_t e0 = t0 < limit ? t0 : limit, e1 = t1 < limit ? t1 : limit;
        cur_lo = e0 > P.stand_hi ? e0 - P.stand_hi : 0u;
        cur_hi = e1 > P.stand_lo ? e1 - P.stand_lo : 0u;
    }
    const int64_t el = (int64_t)t0 - (int64_t)P.anchor_hi - (int64_t)P.stood_hi - (int64_t)cur_hi;
    const int64_t eh = (int64_t)t1 - (int64_t)P.anchor_lo - (int64_t)P.stood_lo - (int64_t)cur_lo;
    *lo = P.anchor_abs + sectors_of(el);
    *hi = P.anchor_abs + sectors_of(eh);
}

/* ---- statistics ---- */
static struct {
    unsigned long sequences, takeups, takeups_held, takeups_refused, hold_new_window, hold_same_window;
    unsigned long dropped_at_visit, over_at_letgo, over_at_sounds, letgo_stop, letgo_keyon, hold_while_held;
    unsigned long hold_found_over, sounds_unheld, letgo_unheld, hold_not_playing;
    unsigned long plays, pauses, releases, quick_pr, stops, ends, ends_once, laps, req_stats, visits, beats, beats_bl, beats_off;
    unsigned long keys, restarts, underruns, silent0, silent_other, card_idle_spans, skips, fifo_spans, timeouts;
    unsigned long sync_rings, sync_looks, late_rings, late_looks, ended_under_hold, pause_under_hold, play_under_hold;
    unsigned long held_takeup_exact, group_between_reads, refused_edge;
    long long ahead_min_sync, ahead_max_sync, ahead_min_late, ahead_max_late;
    long long silent_remaining_max;
    unsigned long long end_late_max;       /* ticks a play played once ended behind its length */
    unsigned long long stood_max;          /* the most a window stood */
    unsigned long long sim_ticks;
} T;

/* ---- observation of the player's visits ---- */
static bool peeking;
static bool ring_sync;                    /* the ring that primes or runs began with its play, held until key-on */
static bool ring_keyed;
static uint64_t takeup_abs;               /* where the ring was taken up (the output's O.sector, O.passes) */
static uint64_t takeup_full;              /* the same on the player's own count of passes (an endless play too) */
static bool ring_unsure;                  /* taken up in a play left out of the position's checks */
static uint64_t ring_paused;              /* ticks the play stood paused since the ring was taken up */
static unsigned long ring_cut0, takeups_second_over;
static unsigned ring_pr;                  /* releases of the play since the ring was taken up */
static uint32_t takeup_sector;
static bool takeup_held;
static struct kui_toy_cdda_drive peek_after;   /* the real player, a visit on from the hold */
static bool peek_after_ok;
/* Not the hold's, and as before it (advance(), the lines the hold did not
 * touch): a base one or two sectors before a pass's start (a lap counted by
 * the part of a group) that is next looked at a pass or more later counts a
 * lap too many, and can answer a sector before the play's first. A play in
 * which a look that began with such a base counted a lap is left out of the
 * position's checks from there on (tainted), until the next PLAY. */
static bool tainted;
static unsigned long tainted_plays, position_checks, ends_checked;
static long long ends_short_max;
static bool lap_from_before(const struct kui_toy_cdda_drive *b, const struct kui_toy_cdda_drive *a) {
    return b->state == KUI_TOY_CDDA_PLAYING && b->base_offset < 0 &&
        (a->done != b->done || (a->state != KUI_TOY_CDDA_PLAYING && a->state != KUI_TOY_CDDA_PAUSED) || a->frozen >= a->end - a->first);
}
static void taint_check(const struct kui_toy_cdda_drive *b) {
    if(!tainted && lap_from_before(b, &D)) {
        tainted = true;
        ++tainted_plays;
        TR("(a lap counted from a base before the pass's start: the play is left out of the position's checks)");
    }
}
static void check_position(const char *where, uint64_t t0, uint64_t t1, const struct kui_toy_cdda_drive *b) {
    const struct kui_toy_cdda_drive *a = &D;
    taint_check(b);
    if(tainted)return;
    ++position_checks;
    uint64_t lo, hi;
    expect(t0, t1, b, &lo, &hi);
    const uint64_t total = total_of(a);
    if(total) {
        /* The play's length in time: over no sooner, and still playing no
         * later than a second for every window a hold opened (and a sector
         * for every release, which begins at a whole one). */
        const uint64_t need = total / 3u * G3 + (total % 3u * G3 + 2u) / 3u, slack = 2u * G3 + 64u * (read_ticks + player_read_ticks);
        if(a->state == KUI_TOY_CDDA_ENDED) {
            if(t1 - P.began - P.paused_ticks + slack < need)viol("%s: a play of %llu ticks ended after %llu", where,
                (unsigned long long)need, (unsigned long long)(t1 - P.began - P.paused_ticks));
        } else {
            const uint64_t took = t0 - P.began - P.paused_ticks;
            if(took > need && took - need > T.end_late_max)T.end_late_max = took - need;
            if(took > need + (uint64_t)HT * P.holds_opened + (uint64_t)(G3 / 3u + 1u) * P.released + slack)
                viol("%s: a play of %llu ticks still plays after %llu (%u windows, %u releases)", where,
                    (unsigned long long)need, (unsigned long long)took, P.holds_opened, P.released);
        }
    }
    if(a->state == KUI_TOY_CDDA_ENDED) {
        /* Not before its length was played. */
        if(hi < total)viol("%s: the play ended early: by the observed holds it stood at %llu..%llu of %llu", where,
            (unsigned long long)lo, (unsigned long long)hi, (unsigned long long)total);
        if(a->frozen != a->end - a->first - 1u || a->done != a->passes - 1u)viol("%s: an ended play does not stand at its last sector", where);
        return;
    }
    const uint64_t at = abs_of(a);
    if(at < lo || at > hi)viol("%s: the play stands at %llu, by the observed holds %llu..%llu", where,
        (unsigned long long)at, (unsigned long long)lo, (unsigned long long)hi);
    if(total && at >= total)viol("%s: a play beyond its last pass still plays", where);
    if(P.have_last) {
        if(at < P.last_abs)viol("%s: the position went backwards: %llu after %llu", where, (unsigned long long)at, (unsigned long long)P.last_abs);
        else if(at - P.last_abs > sectors_of((int64_t)(t1 - P.last_time)) + 1u)
            viol("%s: the position moved %llu sectors in %llu ticks", where, (unsigned long long)(at - P.last_abs), (unsigned long long)(t1 - P.last_time));
    }
    P.last_abs = at;
    P.last_time = t0;
    P.have_last = true;
    if(a->hold) {
        if(P.hold_abs < 0)P.hold_abs = (int64_t)at;
        else if((uint64_t)P.hold_abs != at)viol("%s: a held play moved: %llu, it stood at %lld", where, (unsigned long long)at, (long long)P.hold_abs);
    }
}
static void stand_over(uint64_t e_lo, uint64_t e_hi) {
    const uint64_t lo = e_lo > P.stand_hi ? e_lo - P.stand_hi : 0u, hi = e_hi > P.stand_lo ? e_hi - P.stand_lo : 0u;
    P.stood_lo += lo;
    P.stood_hi += hi;
    P.win_stood += hi;
    P.standing = false;
    if(P.win_stood > T.stood_max)T.stood_max = P.win_stood;
    if(P.win_stood > HT + 64u * (read_ticks + player_read_ticks))viol("a play stood %llu ticks in one window", (unsigned long long)P.win_stood);
}
static void play_over(const char *how) {
    if(D.passes == 1u) {
        ++T.ends_once;
        /* A play played once: over between its length and its length and a
         * second for every window a hold opened. */
        (void)how;
    }
}
static void observe_visit(const struct kui_toy_cdda_drive *b, uint64_t t0) {
    const struct kui_toy_cdda_drive *a = &D;
    const uint64_t t1 = now;
    ++T.visits;
    if(a->plays + a->play2s != b->plays + b->play2s) {
        /* A PLAY was seen. */
        tainted = false;
        if(b->state == KUI_TOY_CDDA_PLAYING && b->hold)++T.play_under_hold;
        memset(&P, 0, sizeof(P));
        ring_sync = false;
        if(a->state == KUI_TOY_CDDA_PLAYING) {
            P.live = true;
            P.anchor_lo = t0;
            P.anchor_hi = t1;
            P.hold_abs = -1;
            P.began = t0;
            if(a->hold || a->held_open)viol("a new play is held");
            if(a->base_offset || a->done || a->frozen)viol("a new play does not stand at its start");
            check_position("PLAY", t0, t1, b);
        }
        return;
    }
    if(a->state == KUI_TOY_CDDA_IDLE || a->state == KUI_TOY_CDDA_LOST) {
        P.live = false;
        return;
    }
    if(b->state == KUI_TOY_CDDA_PAUSED) {
        if(a->state == KUI_TOY_CDDA_PLAYING) {
            /* RELEASE: on from where it stood. */
            const uint64_t began = P.began, paused = P.paused_ticks + (t0 - P.paused_at);
            ring_paused += t1 - P.paused_at;
            const unsigned key_ons = P.key_ons, opened = P.holds_opened, released = P.released + 1u;
            memset(&P, 0, sizeof(P));
            P.live = true;
            P.anchor_lo = t0;
            P.anchor_hi = t1;
            P.anchor_abs = abs_of(b);
            P.hold_abs = -1;
            P.began = began;
            P.paused_ticks = paused;
            P.key_ons = key_ons;
            P.holds_opened = opened;
            P.released = released;
            ++ring_pr;
            ring_sync = false;
            if(a->hold || a->held_open)viol("a released play is held");
            check_position("RELEASE", t0, t1, b);
        } else if(a->state == KUI_TOY_CDDA_PAUSED) {
            if(a->frozen != b->frozen || a->done != b->done)viol("a paused play moved");
        }
        return;
    }
    if(b->state != KUI_TOY_CDDA_PLAYING)return;
    /* The play was brought up to now in this visit. */
    if(b->hold && !a->hold) {
        /* Dropped: its second was over. */
        if(time_of(b->held_at) + HT >= t1)viol("a hold was dropped %llu ticks into its second",
            (unsigned long long)(t1 - time_of(b->held_at)));
        const uint64_t e = time_of(b->held_at) + HT;
        P.standing = true;   /* (expect() reckons the stand to the second's end) */
        check_position("visit that drops a hold", t0, t1, b);
        stand_over(e, e);
        ++T.dropped_at_visit;
        ring_sync = false;
        TR("visit: hold dropped at the second's end");
    } else {
        if(a->hold) {
            /* Still held: within its second by the count this visit read. */
            if((uint32_t)(b->held_at - (uint32_t)(TCNT_START - (uint32_t)t0)) > HT)
                viol("a play is held %u ticks after its window opened", (uint32_t)(b->held_at - (uint32_t)(TCNT_START - (uint32_t)t0)));
            if(a->held >= G3)viol("a held play keeps a whole group's ticks (%u)", a->held);
            if(!a->held_open)viol("a play is held with no window open");
        }
        check_position("visit", t0, t1, b);
    }
    if(a->state == KUI_TOY_CDDA_PAUSED) {
        if(a->hold)++T.pause_under_hold;
        if(P.standing) {
            const uint64_t limit = time_of(b->held_at) + HT;
            stand_over(t0 < limit ? t0 : limit, t1 < limit ? t1 : limit);
        }
        P.paused_at = t1;
        P.live = false;
        ring_sync = false;
    } else if(a->state == KUI_TOY_CDDA_ENDED) {
        if(a->hold)++T.ended_under_hold;
        P.live = false;
        ++T.ends;
        play_over("visit");
    }
}
static unsigned long before_first, before_first_held, before_first_standing;
static bool peek_ok;
static void visit(void) {
    const struct kui_toy_cdda_drive before = D;
    const uint64_t t0 = now;
    (void)kui_toy_cdda_player_visit(KUI_NATIVE_RA_DELIVER, 0u, &R, NULL);
    (void)kui_toy_cdda_player_visit(KUI_TOY_CDDA_VISIT_END, 0u, &R, NULL);
    if((D.state == KUI_TOY_CDDA_PLAYING || D.state == KUI_TOY_CDDA_PAUSED) && D.frozen >= D.end - D.first) {
        if(before.state == KUI_TOY_CDDA_PLAYING && before.base_offset < 0 && (D.frozen == UINT32_MAX || D.frozen == UINT32_MAX - 1u)) {
            if(!peeking)++before_first;
            if(!peeking && (before.hold || D.hold)) {
                ++before_first_held;
                if(D.hold)++before_first_standing;
            }
        } else if(before.state == KUI_TOY_CDDA_PLAYING)
            viol("%s: frozen %u outside a pass of %u (before: state %u base_counter %08x base_offset %d frozen %u done %u hold %u held %u held_open %u held_at %08x; t0 %llu)",
            peeking ? "peek" : "visit", D.frozen, D.end - D.first, before.state, before.base_counter, before.base_offset, before.frozen, before.done, before.hold, before.held, before.held_open, before.held_at, (unsigned long long)t0);
    }
    if(peeking)peek_ok = !lap_from_before(&before, &D);
    if(!peeking && verbose > 1)TR("visit t0 %llu: state %u->%u base_counter %08x base_offset %d frozen %u done %u hold %u held %u (P anchor %llu..%llu abs %llu stood %llu..%llu standing %d)",
        (unsigned long long)t0, before.state, D.state, D.base_counter, D.base_offset, D.frozen, D.done, D.hold, D.held,
        (unsigned long long)P.anchor_lo, (unsigned long long)P.anchor_hi, (unsigned long long)P.anchor_abs, (unsigned long long)P.stood_lo, (unsigned long long)P.stood_hi, (int)P.standing);
    if(!peeking)observe_visit(&before, t0);
}
static int32_t call(uint32_t function, uint32_t r4, uint32_t r5) {
    CHECK(!R.engine.in_irq);
    visit();
    return kui_retail_gd_dispatch(&S, r4, r5, 0u, function);
}
/* What the real player would answer a position query made now, without
 * moving anything: a visit on a copy of everything, taken back. A command
 * the game has asked and the player has not yet seen stays unseen. */
static void peek(struct kui_toy_cdda_drive *after) {
    const struct kui_toy_cdda_drive d = D;
    const struct kui_retail_gd s = S;
    const uint64_t t = now;
    peeking = true;
    D.seen_token = S.token;
    visit();
    *after = D;
    peeking = false;
    D = d;
    S = s;
    now = t;
}

/* ---- the hold, observed (the real function runs unchanged) ---- */
void __real_kui_toy_cdda_player_hold(uint32_t hold);
void __wrap_kui_toy_cdda_player_hold(uint32_t hold);
void __wrap_kui_toy_cdda_player_hold(uint32_t hold) {
    const struct kui_toy_cdda_drive b = D;
    const uint64_t t0 = now;
    __real_kui_toy_cdda_player_hold(hold);
    const struct kui_toy_cdda_drive *a = &D;
    const uint64_t t1 = now;
    if(hold != KUI_TOY_CDDA_LET_GO && hold != KUI_TOY_CDDA_HOLD && hold != KUI_TOY_CDDA_SOUNDS)viol("hold(%u)", hold);
    if(hold != KUI_TOY_CDDA_HOLD)capture_pending = false;
    if(b.state != KUI_TOY_CDDA_PLAYING) {
        ++T.hold_not_playing;
        if(memcmp(&b, a, sizeof(b)))viol("hold(%u) changed a drive that does not play", hold);
        return;
    }
    if(hold == KUI_TOY_CDDA_HOLD) {
        if(O.phase != KUI_TOY_CDDA_OUT_OFF)viol("a hold asked in phase %u", O.phase);
        if(b.hold) {
            ++T.hold_while_held;
            if(memcmp(&b, a, sizeof(b)))viol("a hold of a held play changed it");
            return;
        }
        if(a->state != KUI_TOY_CDDA_PLAYING) {
            /* The visit the hold begins with found the play over. */
            ++T.hold_found_over;
            if(a->hold)viol("a play that is over is held");
            check_position("hold that finds the play over", t0, t1, &b);
            if(a->state == KUI_TOY_CDDA_ENDED) {
                P.live = false;
                ++T.ends;
                play_over("hold");
            }
            TR("hold: the play is over");
            return;
        }
        if(a->hold) {
            if(!b.held_open) {
                ++T.hold_new_window;
                ++P.windows;
                ++P.holds_opened;
                P.win_stood = 0u;
                if(!a->held_open || time_of(a->held_at) < t0 || time_of(a->held_at) > t1)viol("a window opened at a count not read in the hold");
                P.stand_lo = P.stand_hi = time_of(a->held_at);
            } else {
                ++T.hold_same_window;
                if(a->held_at != b.held_at)viol("a window's opening moved");
                if(time_of(b.held_at) + HT < t0)viol("a hold granted %llu ticks after its window opened", (unsigned long long)(t0 - time_of(b.held_at)));
                P.stand_lo = t0;
                P.stand_hi = t1;
            }
            if(a->held >= G3)++T.group_between_reads;
            /* Where it stood as the visit brought it up, before it stands. */
            P.hold_abs = -1;
            check_position("hold", t0, t1, &b);
            P.standing = true;
            P.hold_abs = -1;
            takeup_held = true;
            TR("hold: granted (%s window), held %u", b.held_open ? "same" : "new", a->held);
        } else {
            ++T.takeups_refused;
            if(!b.held_open || a->held_open != 1u || a->held_at != b.held_at)viol("a hold refused with no window open");
            else if(time_of(b.held_at) + HT >= t1)viol("a hold refused %llu ticks into its second", (unsigned long long)(t1 - time_of(b.held_at)));
            check_position("hold refused", t0, t1, &b);
            takeup_held = false;
            TR("hold: refused, %llu ticks after the window opened", (unsigned long long)(t1 - time_of(b.held_at)));
        }
        /* What a position query made now would be answered. */
        peek(&peek_after);
        peek_after_ok = peek_ok && !tainted;
        capture_pending = true;
        return;
    }
    /* Let go: the ring given up, or keyed on. */
    if(b.hold) {
        const uint64_t limit = time_of(b.held_at) + HT;
        const bool over = limit < t0;
        if(a->hold)viol("a play let go is held");
        if(over) {
            if(hold == KUI_TOY_CDDA_SOUNDS)++T.over_at_sounds;
            else ++T.over_at_letgo;
            ring_sync = false;
        } else if(hold == KUI_TOY_CDDA_SOUNDS)++T.letgo_keyon;
        else ++T.letgo_stop;
        check_position(hold == KUI_TOY_CDDA_SOUNDS ? "key-on" : "let go", t0, t1, &b);
        stand_over(t0 < limit ? t0 : limit, t1 < limit ? t1 : limit);
        if(a->state == KUI_TOY_CDDA_ENDED) {
            P.live = false;
            ++T.ends;
            play_over("let go");
        }
        TR("hold: let go by %s%s", hold == KUI_TOY_CDDA_SOUNDS ? "key-on" : "stop", over ? " (its second was over)" : "");
    } else {
        if(hold == KUI_TOY_CDDA_SOUNDS)++T.sounds_unheld;
        else ++T.letgo_unheld;
        struct kui_toy_cdda_drive c = b;
        if(hold == KUI_TOY_CDDA_SOUNDS)c.held_open = 0u;
        if(memcmp(&c, a, sizeof(c)))viol("letting go a play that is not held changed it");
    }
    if(a->state == KUI_TOY_CDDA_PLAYING && hold == KUI_TOY_CDDA_SOUNDS) {
        if(a->held_open)viol("a window open behind a key-on");
        ++P.keys;
        ++P.key_ons;
        P.win_stood = 0u;
        ring_keyed = true;
        if(ring_sync && !tainted && !ring_unsure) {
            /* Held from the take-up to here: the play stands where its
             * ring begins. */
            ++T.sync_rings;
            if(abs_of(a) != takeup_full || D.first + a->frozen != takeup_sector)
                viol("a ring keyed on under its hold begins at %llu, its play stands at %llu", (unsigned long long)takeup_full, (unsigned long long)abs_of(a));
        } else ++T.late_rings;
    }
}
/* The take-up: the output's own first sector and passes, against what the
 * real player would answer at that moment. */
static void takeup_captured(void) {
    const struct kui_toy_cdda_drive *p = &peek_after;
    const uint32_t length = D.end - D.first;
    ++T.takeups;
    takeup_sector = O.sector;
    heard_play.first = D.first;
    heard_play.end = D.end;
    heard_play.base = O.sector;
    heard_play.passes = O.passes;
    ring_keyed = false;
    ring_dry = false;
    ring_unsure = false;
    ring_sync = takeup_held;
    if(O.sector < D.first || O.sector >= D.end) {
        viol("a ring taken up at sector %u outside the play", O.sector);
        return;
    }
    if(D.passes && (!O.passes || O.passes > D.passes)) {
        viol("a ring taken up with %u passes of the play's %u", O.passes, D.passes);
        return;
    }
    if(!D.passes && O.passes)viol("a ring of an endless play has %u passes", O.passes);
    takeup_abs = D.passes ? (uint64_t)(D.passes - O.passes) * length + (O.sector - D.first) : O.sector - D.first;
    takeup_full = takeup_abs;
    ring_pr = 0u;
    ring_paused = 0u;
    ring_cut0 = cut_frames;
    if(!peek_after_ok || ((p->state == KUI_TOY_CDDA_PLAYING || p->state == KUI_TOY_CDDA_PAUSED) && p->frozen >= length)) {
        ring_sync = false;
        ring_unsure = true;
        return;
    }
    /* (A hold granted in the last ticks of its second is over by the
     * count a query a few ticks on reads: the play has then gone on from
     * the second's end, as one that is not held.) */
    const bool second_over = takeup_held && D.hold && time_of(D.held_at) + HT < now + 8u * (uint64_t)(read_ticks + player_read_ticks);
    if(p->state != KUI_TOY_CDDA_PLAYING) {
        /* (A play a sector from its end when the hold was refused may be
         * over a few ticks on.) */
        if((takeup_held && !second_over) || takeup_abs + 2u < total_of(&D))viol("a ring taken up at %llu for a play the player finds over", (unsigned long long)takeup_abs);
        return;
    }
    const uint64_t answer = D.passes ? abs_of(p) : p->frozen;
    long long apart = (long long)takeup_abs - (long long)answer;
    if(!D.passes && apart > (long long)length / 2)apart -= length;
    if(!D.passes && apart < -(long long)length / 2)apart += length;
    takeup_full = (uint64_t)((long long)abs_of(p) + apart);
    if(second_over) {
        ++T.takeups_held;
        ++takeups_second_over;
        ring_sync = false;
        if(apart < -1 || apart > 1)viol("a ring taken up at %llu (held as its second ends); a position query is answered %llu", (unsigned long long)takeup_abs, (unsigned long long)answer);
    } else if(takeup_held) {
        ++T.takeups_held;
        if(apart)viol("a ring taken up at %llu under a hold; a position query is answered %llu", (unsigned long long)takeup_abs, (unsigned long long)answer);
        else ++T.held_takeup_exact;
    } else {
        if(apart < -1 || apart > 1)viol("a ring taken up at %llu (not held); a position query is answered %llu", (unsigned long long)takeup_abs, (unsigned long long)answer);
        if(apart)++T.refused_edge;
    }
    TR("take-up at sector %u passes %u (%s), the player answers %llu", O.sector, O.passes, takeup_held ? "held" : "not held", (unsigned long long)answer);
}

/* ---- the game ---- */
static uint32_t rng;
static uint32_t rnd(void) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}
static uint32_t below(uint32_t n) { return n ? rnd() % n : 0u; }
static uint32_t between(uint32_t a, uint32_t b) { return a + below(b - a + 1u); }
/* Log-uniform ticks between a and b. */
static uint32_t span(uint32_t a, uint32_t b) {
    double x = (double)rnd() / 4294967296.0;
    double v = (double)a;
    double ratio = (double)b / (double)a;
    /* a * ratio^x without libm */
    for(int i = 0; i < 24; ++i) {
        ratio = __builtin_sqrt(ratio);
        x *= 2.0;
        if(x >= 1.0) { v *= ratio; x -= 1.0; }
    }
    return (uint32_t)v;
}
static void do_beat(void);
static unsigned split_chance;         /* in 256: a heartbeat between two calls of one request */
static int32_t request(uint32_t command, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    gput(PARAM, a); gput(PARAM + 4u, b); gput(PARAM + 8u, c); gput(PARAM + 12u, d);
    const int32_t token = call(KUI_GD_REQUEST, command, PARAM);
    CHECK(token > 0);
    if(below(256u) < split_chance) {
        now += between(1u, 3000u);
        do_beat();
    }
    (void)call(KUI_GD_EXEC, 0u, 0u);
    return token;
}
static void finish(int32_t token) {
    if(below(256u) < split_chance) {
        now += between(1u, 3000u);
        do_beat();
    }
    CHECK(call(KUI_GD_CHECK, (uint32_t)token, STAT) == KUI_GD_COMPLETED);
}
static void game_req_stat(void) {
    gput(PARAM, OUT + 16u); gput(PARAM + 4u, OUT + 20u); gput(PARAM + 8u, OUT + 24u); gput(PARAM + 12u, OUT + 28u);
    const int32_t token = call(KUI_GD_REQUEST, KUI_RETAIL_GD_REQ_STAT, PARAM);
    CHECK(token > 0);
    if(below(256u) < split_chance) {
        now += between(1u, 3000u);
        do_beat();
    }
    (void)call(KUI_GD_EXEC, 0u, 0u);
    /* The answer is the play's sector as the EXEC's visit brought it up. */
    const uint32_t state = D.state, stands = D.first + D.frozen + 150u;
    finish(token);
    ++T.req_stats;
    if(state == KUI_TOY_CDDA_PLAYING || state == KUI_TOY_CDDA_PAUSED || state == KUI_TOY_CDDA_ENDED) {
        const uint32_t fad = gget(OUT + 24u) & 0xffffffu, status = gget(OUT + 16u);
        if(fad != stands)viol("REQ_STAT answers FAD %u; the play stood at %u", fad, stands);
        if((status == 3u) != (state == KUI_TOY_CDDA_PLAYING))viol("REQ_STAT answers status %u in state %u", status, state);
    }
}
static unsigned jingles;              /* in 100: plays of a few sectors, played once */
static void game_play(void) {
    static const uint8_t repeats[] = {0u, 0u, 0u, 1u, 2u, 15u, 15u, 15u};
    uint32_t repeat = repeats[below(sizeof(repeats))];
    ++T.plays;
    if(below(100u) < jingles) {
        const uint32_t length = between(2u, 12u);
        const uint32_t first = between(1000u, 9000u - length);
        repeat = below(4u) ? 0u : 1u;
        TR("game: PLAY2 %u..%u repeat %u", first, first + length, repeat);
        finish(request(KUI_RETAIL_GD_PLAY2, first + 150u, first + length + 150u, repeat, 0u));
    } else if(below(3u)) {
        /* By FAD: a short play, 0.3 to 8 seconds, anywhere in the audio. */
        const uint32_t length = below(4u) ? between(20u, 600u) : between(3u, 40u);
        const uint32_t first = between(1000u, 9000u - length);
        TR("game: PLAY2 %u..%u repeat %u", first, first + length, repeat);
        finish(request(KUI_RETAIL_GD_PLAY2, first + 150u, first + length + 150u, repeat, 0u));
    } else {
        const uint32_t a = between(2u, 4u), b = between(a, 4u);
        TR("game: PLAY %u..%u repeat %u", a, b, repeat);
        finish(request(KUI_RETAIL_GD_PLAY, a, b, repeat, 0u));
    }
}
static unsigned chance_pause, chance_play, chance_quick;
static void game_step(void) {
    const uint32_t pick = below(1000u);
    if(D.state == KUI_TOY_CDDA_PLAYING) {
        if(pick < chance_pause) {
            ++T.pauses;
            TR("game: PAUSE");
            finish(request(KUI_RETAIL_GD_PAUSE, 0u, 0u, 0u, 0u));
        } else if(pick < chance_pause + chance_play)game_play();
        else if(pick < chance_pause + chance_play + chance_quick) {
            /* A pause and a release at once (between two heartbeats). */
            ++T.quick_pr;
            ++T.pauses;
            ++T.releases;
            TR("game: PAUSE and RELEASE at once");
            const unsigned kept = split_chance;
            split_chance = 0u;
            finish(request(KUI_RETAIL_GD_PAUSE, 0u, 0u, 0u, 0u));
            finish(request(KUI_RETAIL_GD_RELEASE, 0u, 0u, 0u, 0u));
            split_chance = kept;
        } else if(pick < chance_pause + chance_play + chance_quick + 3u) {
            ++T.stops;
            TR("game: STOP");
            finish(request(KUI_GD_STOP, 0u, 0u, 0u, 0u));
        } else if(pick < 900u)game_req_stat();
        else if(pick < 960u) {
            CHECK(call(KUI_GD_DRIVE, OUT, 0u) == 0);
        }
    } else if(D.state == KUI_TOY_CDDA_PAUSED) {
        if(pick < 250u) {
            ++T.releases;
            TR("game: RELEASE");
            finish(request(KUI_RETAIL_GD_RELEASE, 0u, 0u, 0u, 0u));
        } else if(pick < 300u)game_play();
        else if(pick < 800u)game_req_stat();
    } else {
        if(pick < 200u)game_play();
        else if(pick < 700u)game_req_stat();
        else if(pick < 800u)CHECK(call(KUI_GD_DRIVE, OUT, 0u) == 0);
    }
}

/* ---- the worker's heartbeat ---- */
static uint64_t card_idle_until, beats_off_until, fifo_until;
static unsigned idle_chance, off_chance, fifo_chance, skip_chance;   /* in 1024, at a take-up */
static uint32_t last_phase;
static unsigned first_fifo;
static void do_beat(void) {
    if(now < beats_off_until) {
        ++T.beats_off;
        return;
    }
    ++T.beats;
    stretch_began = now;
    stretch_live = true;
    chip_run();
    card_calls_now = 0u;
    since_empty = 8u;
    card_idle = now < card_idle_until;
    fifo_stuck = now < fifo_until;
    /* (A run's own question, outside the hold: the bus does not empty at
     * the first heartbeat of the first play, and at no other.) */
    if(first_fifo == 1u && D.state == KUI_TOY_CDDA_PLAYING) {
        fifo_stuck = true;
        first_fifo = 2u;
        TR("the bus does not empty at this heartbeat, the first play's first");
    }
    const uint32_t before = O.phase, rest = sr, keys = O.keys, restarts = O.restarts, underruns = O.underruns, timeouts = O.timeouts;
    const unsigned long ups = T.takeups;
    const bool bl = !below(97u);
    if(bl) {
        sr = rest | BL;
        ++T.beats_bl;
    }
    if(O.phase == KUI_TOY_CDDA_OUT_PRIME ? below(1024u) < skip_chance : !below(700u)) {
        card_skips = true;
        ++T.skips;
    }
    kui_toy_cdda_consumer_heartbeat();
    stretch_live = false;
    CHECK(sr == (bl ? rest | BL : rest));
    sr = rest;
    card_skips = false;
    chip_run();
    T.keys += O.keys - keys;
    T.restarts += O.restarts - restarts;
    T.underruns += O.underruns - underruns;
    T.timeouts += O.timeouts - timeouts;
    if(O.keys != keys)TR("beat: keyed on (written %u)", O.written);
    if(O.restarts != restarts)TR("beat: the ring begins again (underruns %u broken %u)", O.underruns, O.broken);
    if(capture_pending) {
        /* A hold asked and no ring taken up: the output stopped. */
        if(O.phase == KUI_TOY_CDDA_OUT_PRIME || O.phase == KUI_TOY_CDDA_OUT_RUN)viol("a ring taken up that was not seen");
        capture_pending = false;
    }
    /* A hold never outlives the ring it was asked for. */
    if(D.state == KUI_TOY_CDDA_PLAYING && D.hold && O.phase != KUI_TOY_CDDA_OUT_PRIME)
        viol("a play is held with the output in phase %u", O.phase);
    if(O.phase == KUI_TOY_CDDA_OUT_SILENT && before != KUI_TOY_CDDA_OUT_SILENT) {
        if(O.why)++T.silent_other;
        else {
            /* A ring keyed on under its hold that played to its play's
             * end: all of the play from the take-up on was heard, to a
             * couple of sectors. */
            if(before == KUI_TOY_CDDA_OUT_RUN && ring_sync && ring_keyed && !ring_dry && !tainted && !ring_unsure && !ring_pr &&
               D.passes && D.generation - O.generation <= 1u) {
                const long long want = (long long)(total_of(&D) - takeup_full) * 588;
                /* (Every frame the chip played was the play's, the silence
                 * behind its end too, or silence in the play's place once
                 * the drive's play had ended: those are the cut ones.) */
                const long long played = (long long)voice[pair[0]].consumed, cuts = (long long)(cut_frames - ring_cut0);
                const long long lost = (played < want ? want - played : 0) + cuts;
                ++ends_checked;
                if(lost > ends_short_max)ends_short_max = lost;
                if(lost > 1500)viol("a ring keyed on under its hold played %lld frames (%lld of them silence in the play's place) of its play's %lld", played, cuts, want);
            }
            /* Given up as over: is it, for the game? */
            struct kui_toy_cdda_drive p;
            ++T.silent0;
            peek(&p);
            if(p.state == KUI_TOY_CDDA_PLAYING && p.frozen < p.end - p.first && peek_ok && !tainted) {
                const uint64_t total = total_of(&p);
                if(!total)viol("the output is silent (why 0) under a play that is repeated for ever");
                else {
                    const long long left = (long long)(total - abs_of(&p));
                    if(left > T.silent_remaining_max)T.silent_remaining_max = left;
                    if(left > 4)viol("the output is silent (why 0) with %lld sectors of the play to come", left);
                }
            }
            TR("beat: silent, why 0 (player state %u)", p.state);
        }
    }
    /* A ring that runs against its play: where the play stands, in frames
     * from the ring's first, less what the chip has played. */
    if(O.phase == KUI_TOY_CDDA_OUT_RUN && ring_keyed && D.state == KUI_TOY_CDDA_PLAYING && D.generation == O.generation &&
       sounding() && !(T.beats & 3u)) {
        struct kui_toy_cdda_drive p;
        peek(&p);
        if(p.state == KUI_TOY_CDDA_PLAYING && p.frozen < p.end - p.first && peek_ok && !tainted && !ring_unsure) {
            const long long sectors = (long long)abs_of(&p) - (long long)takeup_full;
            const long long played = (long long)voice[pair[0]].consumed;
            const long long ahead = sectors * 588 - played;
            if(ring_sync) {
                if(!T.sync_looks || ahead < T.ahead_min_sync)T.ahead_min_sync = ahead;
                if(!T.sync_looks || ahead > T.ahead_max_sync)T.ahead_max_sync = ahead;
                ++T.sync_looks;
                if(ahead < -1500 || ahead > 1500)viol("a ring keyed on under its hold: its play is %lld frames ahead of it", ahead);
            } else {
                if(!T.late_looks || ahead < T.ahead_min_late)T.ahead_min_late = ahead;
                if(!T.late_looks || ahead > T.ahead_max_late)T.ahead_max_late = ahead;
                ++T.late_looks;
                if(ahead < -1500 - 800 * (long long)ring_pr - (long long)(ring_paused * 882u / 15625u))
                    viol("a ring is %lld frames ahead of its play (%u releases since it began, paused %llu ticks)", -ahead, ring_pr, (unsigned long long)ring_paused);
            }
        }
    }
    if(T.takeups != ups) {
        /* A ring was taken up: now and then the card is the game's for a
         * while, no heartbeat is made, or the bus does not empty. */
        const uint32_t pick = below(1024u);
        if(pick < idle_chance) {
            card_idle_until = now + span(20000u, 3u * SECOND);
            ++T.card_idle_spans;
            TR("the card idle for %llu ticks", (unsigned long long)(card_idle_until - now));
        } else if(pick < idle_chance + off_chance) {
            beats_off_until = now + span(20000u, 3u * SECOND);
            TR("no heartbeat for %llu ticks", (unsigned long long)(beats_off_until - now));
        } else if(pick < idle_chance + off_chance + fifo_chance) {
            fifo_until = now + span(20000u, 2u * SECOND);
            ++T.fifo_spans;
            TR("the bus does not empty for %llu ticks", (unsigned long long)(fifo_until - now));
        }
    } else if(!below(900u)) {
        card_idle_until = now + span(20000u, 3u * SECOND);
        ++T.card_idle_spans;
        TR("the card idle for %llu ticks", (unsigned long long)(card_idle_until - now));
    }
    last_phase = O.phase;
}

static void sequence(uint32_t seed) {
    seed_now = seed;
    rng = seed * 2654435761u + 0x9e3779b9u;
    if(!rng)rng = 1u;
    for(int i = 0; i < 8; ++i)(void)rnd();
    fresh();
    violated = false;
    memset(&R, 0, sizeof(R));
    memset(guest, 0, sizeof(guest));
    memset(&P, 0, sizeof(P));
    capture_pending = peeking = ring_sync = ring_keyed = false;
    card_idle_until = beats_off_until = fifo_until = 0u;
    now = below(4000000u);
    const struct kui_gd_ops ops = {guest, gmap, grange, greader};
    CHECK(kui_retail_gd_init(&S, tracks, 5u, &ops, UINT32_C(0x8c008000), UINT32_C(0x8d000000)) == 0);
    S.position_lba = 45000u;
    if(!below(4u))manatee1(below(2u) != 0u);
    else manatee2();
    static const uint32_t reads[] = {1u, 1u, 2u, 3u, 5u, 11u, 37u, 113u};
    read_ticks = reads[below(8u)];
    if(getenv("READ_TICKS"))read_ticks = (uint32_t)strtoul(getenv("READ_TICKS"), 0, 0);
    player_read_ticks = getenv("PLAYER_READ_TICKS") ? (uint32_t)strtoul(getenv("PLAYER_READ_TICKS"), 0, 0) : read_ticks;
    first_fifo = getenv("FIRST_FIFO") ? 1u : 0u;
    jingles = getenv("JINGLES") ? (unsigned)strtoul(getenv("JINGLES"), 0, 0) : below(6u) ? 0u : 50u;
    access_ticks = between(1u, 2u);
    static const uint32_t mosts[] = {512u, 1024u, 2048u, 8192u, 8192u};
    card_most = mosts[below(5u)];
    static const uint32_t cards[] = {40u, 40u, 200u, 600u};
    card_ticks = cards[below(4u)];
    split_chance = below(3u) ? 0u : between(4u, 64u);
    chance_pause = between(2u, 25u);
    chance_play = between(2u, 25u);
    chance_quick = below(3u) ? 0u : between(1u, 8u);
    idle_chance = between(100u, 500u);
    off_chance = between(20u, 200u);
    fifo_chance = between(0u, 80u);
    skip_chance = between(10u, 120u);
    const unsigned beat_mode = below(4u), game_mode = below(5u);
    const uint64_t began = now, until = now + (uint64_t)between(8u, 40u) * SECOND;
    uint64_t next_beat = now + below(5000u), next_game = now + below(13000u);
    TR("---- seed %u: read %u access %u card_most %u card_ticks %u beat_mode %u game_mode %u", seed, read_ticks, access_ticks, card_most, card_ticks, beat_mode, game_mode);
    while(now < until && !violated) {
        if(next_beat <= next_game) {
            if(now < next_beat)now = next_beat;
            do_beat();
            const uint32_t pick = below(16u);
            next_beat = now + (beat_mode == 3u ? between(1700u, 2600u) :
                pick == 0u ? 300u : pick == 1u ? 9000u : 4000u + below(1024u));
        } else {
            if(now < next_game)now = next_game;
            game_step();
            const uint32_t pick = below(100u);
            if(game_mode == 4u && pick < 50u)next_game = now + between(100u, 2500u);
            else if(pick < 8u)next_game = now + between(100u, 3000u);
            else if(pick < (game_mode == 0u ? 14u : 10u))next_game = now + span(100000u, 3u * SECOND);
            else next_game = now + between(12500u, 13500u);
        }
    }
    chip_run();
    T.sim_ticks += now - began;
    ++T.sequences;
}

int main(int argc, char **argv) {
    const uint32_t from = argc > 1 ? (uint32_t)strtoul(argv[1], 0, 0) : 1u, count = argc > 2 ? (uint32_t)strtoul(argv[2], 0, 0) : 10u;
    if(argc > 3) {
        trace = stdout;
        verbose = (int)strtol(argv[3], 0, 0);
        if(!verbose)verbose = 1;
    }
    (void)sizes;
    (void)driver_of;
    (void)bank;

    (void)last_phase;
    setvbuf(stdout, 0, _IOLBF, 0);
    for(uint32_t seed = from; seed < from + count; ++seed)sequence(seed);
    printf("sequences %lu (seeds %u..%u), simulated %.0f s; heartbeats %lu (SR.BL %lu, not made %lu), visits %lu, REQ_STAT %lu\n",
        T.sequences, from, from + count - 1u, (double)T.sim_ticks / SECOND, T.beats, T.beats_bl, T.beats_off, T.visits, T.req_stats);
    printf("game: plays %lu, pauses %lu, releases %lu (at once %lu), stops %lu, plays ended %lu (played once %lu)\n",
        T.plays, T.pauses, T.releases, T.quick_pr, T.stops, T.ends, T.ends_once);
    printf("take-ups %lu: held %lu (window new %lu, same %lu; exact %lu), not held %lu (a sector apart %lu); hold found the play over %lu; hold of a held play %lu\n",
        T.takeups, T.takeups_held, T.hold_new_window, T.hold_same_window, T.held_takeup_exact, T.takeups_refused, T.refused_edge, T.hold_found_over, T.hold_while_held);
    printf("holds ended: by key-on %lu, by stop %lu, dropped at a visit at the second's end %lu, found over at key-on %lu, found over at stop %lu; a group whole between the hold's two reads %lu\n",
        T.letgo_keyon, T.letgo_stop, T.dropped_at_visit, T.over_at_sounds, T.over_at_letgo, T.group_between_reads);
    printf("under a hold: PAUSE %lu, PLAY %lu, ended %lu; let go of a play not held: key-on %lu, stop %lu; hold calls with no play %lu\n",
        T.pause_under_hold, T.play_under_hold, T.ended_under_hold, T.sounds_unheld, T.letgo_unheld, T.hold_not_playing);
    printf("output: keys %lu, restarts %lu, underruns %lu, timeouts %lu, silent why 0 %lu (most sectors left %lld), silent other %lu; card idle spans %lu, frames left out %lu, bus spans %lu\n",
        T.keys, T.restarts, T.underruns, T.timeouts, T.silent0, T.silent_remaining_max, T.silent_other, T.card_idle_spans, T.skips, T.fifo_spans);
    printf("rings keyed under their hold %lu: play ahead of ring %lld..%lld frames over %lu looks; other rings %lu: %lld..%lld over %lu looks\n",
        T.sync_rings, T.ahead_min_sync, T.ahead_max_sync, T.sync_looks, T.late_rings, T.ahead_min_late, T.ahead_max_late, T.late_looks);
    printf("most a window stood %llu ticks; a play with a count of passes was seen playing at most %llu ticks behind its length\n", T.stood_max, T.end_late_max);
    printf("not the hold's (advance() as before it): plays left out of the position's checks from a lap counted off a base before the pass's start %lu; visits that answered a sector before the play's first %lu (of a play held before the visit or after it %lu, still held after it %lu)\n", tainted_plays, before_first, before_first_held, before_first_standing);
    printf("take-ups under a hold granted as its second ends (over by a query's count) %lu\n", takeups_second_over);
    printf("checks of the position against the observed holds %lu; rings keyed on under their hold and played to the play's end %lu (short of it by at most %lld frames)\n", position_checks, ends_checked, ends_short_max);
    printf("rings the chip played beyond what was written: %lu\n", dry_rings);
    printf("%s: %lu violations in %lu sequences; model checks %lu\n", violations ? "FAIL" : "PASS", violations, seeds_violated, checks);
    return violations ? 1 : 0;
}
