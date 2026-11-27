/* SPDX-License-Identifier: GPL-3.0-only */
/* Test 84: CD music in the Background reader, the worker's player
 * (docs/cdda-background-test-84.md).
 *
 * The auxiliary calls kui_toy_cdda_player_visit after its guard on every visit
 * it admits, and once more with KUI_TOY_CDDA_VISIT_END at the end of a delivery
 * visit from one of the game's GD calls (include/kui/native_readahead.h). It
 * runs on the auxiliary's private stack, under its gate (SR.BL and IMASK 15),
 * and calls nothing of the auxiliary's. The cold stage finds it through the
 * worker's descriptor behind the loader timeline's
 * (src/loader/vmu_native_trace_hooks.S) and proves it lies in the worker.
 *
 * HOOK alone: the player is called and does nothing.
 *
 * DRIVE: the drive plays. A delivery visit from one of the game's GD calls
 * comes before the disc service answers that call (src/loader/retail_async.c,
 * kui_retail_async_call), so what is written into the service here is what
 * the call is answered from:
 * - A command the service accepted is seen at the game's next GD call (the
 *   EXEC the library makes right after REQUEST), by its new token. PLAY,
 *   PLAY2, PAUSE, RELEASE, STOP, SEEK and INIT move the play.
 * - While a play stands, every such visit writes the drive status: 3 while
 *   playing, 1 paused or ended. The library's own REQ_STAT after PLAY and
 *   PAUSE, and its resume, which compares their positions, read that.
 * - While a REQ_STAT, or a GETSCD of position (formats 0 and 1), is pending,
 *   the service's position is the play's sector; it goes back to the data
 *   position at the first visit that finds the command no longer pending. No
 *   read can be accepted in between (src/core/retail_gd.c, request), so the
 *   window's rule for sequential reads keeps seeing the data position.
 * - The play clock is TMU0, read only and trusted only under the Toy timer
 *   profile (include/kui/toy_pilot_clock.h): 75 sectors a second.
 * With no play the player writes nothing: every answer is today's.
 *
 * KEEP: the status stays 3 while the game reads data during a play. After the
 * visit, before the service answers, the low reader's report() publishes the
 * sectors its engine has delivered of the read pending, and the service's
 * kui_retail_gd_progress (src/core/retail_gd.c) writes status 1 whenever it
 * adds sectors: without KEEP, a call during a read in a play answers 1. At the
 * visit's end (VISIT_END) the player adds them first, word for word as that
 * function does, but with status 3, so report() finds none to add and writes
 * no status. The request's error, completion and status stay report()'s. The
 * GD call runs with interrupts blocked from the visit through the answer
 * (src/loader/retail_resident.S), so nothing is delivered in between. */
#include <stdbool.h>
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
#ifndef KUI_TOY_CDDA_DRIVE
#define KUI_TOY_CDDA_DRIVE 0
#endif
#ifndef KUI_TOY_CDDA_KEEP
#define KUI_TOY_CDDA_KEEP 0
#endif
#if KUI_TOY_CDDA_KEEP && !KUI_TOY_CDDA_DRIVE
#error "KEEP keeps the playing drive's status (KUI_TOY_CDDA_DRIVE)"
#endif

#if !KUI_TOY_CDDA_DRIVE
uint32_t kui_toy_cdda_player_visit(uint32_t event,uint32_t arg,
    kui_retail_async_region_type *r,const struct kui_native_ra_low_ops *ops) {
    (void)event;(void)arg;(void)r;(void)ops;
    return 0u;
}
#else
#include "kui/retail_gd.h"
#include "kui/toy_cdda_player.h"
#include "kui/toy_pilot_clock.h"
#if KUI_TOY_CDDA_KEEP && (defined(KUI_RETAIL_CE) || KUI_NATIVE_SESSION_CONTROLS || KUI_NATIVE_GD_LEGACY_STREAM)
#error "KEEP publishes a read's progress as the Background reader's report() does"
#endif

struct kui_toy_cdda_drive kui_toy_cdda_drive;
#define D kui_toy_cdda_drive

#ifdef KUI_NATIVE_READAHEAD_AUX_HOST_TEST
extern uint32_t kui_native_readahead_aux_test_read(uint32_t,unsigned);
#define read_hw kui_native_readahead_aux_test_read
#else
static uint32_t read_hw(uint32_t address,unsigned width) {
    if(width==1u)return *(volatile const uint8_t *)(uintptr_t)address;
    if(width==2u)return *(volatile const uint16_t *)(uintptr_t)address;
    return *(volatile const uint32_t *)(uintptr_t)address;
}
#endif
/* TMU0's count, and whether the Toy timer profile holds (781,250 Hz). */
static bool clock_now(uint32_t *counter) {
    uint32_t frqcr=read_hw(UINT32_C(0xffc00000),2u);
    uint32_t tcor=read_hw(UINT32_C(0xffd80008),4u);
    uint32_t tcr=read_hw(UINT32_C(0xffd80010),2u);
    uint32_t tstr=read_hw(UINT32_C(0xffd80004),1u);
    *counter=read_hw(UINT32_C(0xffd8000c),4u);
    return kui_toy_pilot_clock_profile(frqcr,tcor,tcr,tstr);
}
static const struct kui_retail_track *track_at(const struct kui_retail_gd *s,uint32_t number) {
    return number && number<=s->track_count?&s->tracks[number-1u].track:0;
}
static bool audio(const struct kui_retail_track *t) {
    return t && !kui_retail_track_control(t) && t->start_lba<t->end_lba;
}
/* The end of the run of audio tracks from track n on, to track limit at most. */
static uint32_t run_end(const struct kui_retail_gd *s,uint32_t n,uint32_t limit) {
    while(n<limit && audio(track_at(s,n+1u)))++n;
    return track_at(s,n)->end_lba;
}
/* PLAY: tracks a to b, as far as they are audio and follow on from a. */
static bool play_range(const struct kui_retail_gd *s,uint32_t a,uint32_t b,
    uint32_t *first,uint32_t *end) {
    const struct kui_retail_track *t=track_at(s,a);
    if(!audio(t) || b<a)return false;
    *first=t->start_lba;
    *end=run_end(s,a,b<s->track_count?b:s->track_count);
    return *first<*end;
}
/* PLAY2: from a FAD inside an audio track, or in the gap before one that
 * follows another, to the end FAD or the end of that run of audio tracks. */
static bool play2_range(const struct kui_retail_gd *s,uint32_t from,uint32_t to,
    uint32_t *first,uint32_t *end,uint32_t *track) {
    if(from<150u || to<=from)return false;
    const uint32_t lba=from-150u,stop=to-150u;
    for(uint32_t n=1u;n<=s->track_count;++n) {
        const struct kui_retail_track *t=track_at(s,n);
        if(lba>=t->end_lba)continue;
        if(!audio(t) || (lba<t->start_lba && !audio(track_at(s,n-1u))))return false;
        const uint32_t limit=run_end(s,n,s->track_count);
        *first=lba;*end=stop<limit?stop:limit;*track=n;
        return *first<*end;
    }
    return false;
}
static void play_start(uint32_t first,uint32_t end,uint32_t repeat,uint32_t token,uint32_t track) {
    uint32_t now;
    D.first=first;D.end=end;D.repeat=repeat;D.track=track;D.play_token=token;
    /* 15 repeats for ever; otherwise the track plays repeat+1 times. */
    D.passes=(repeat&15u)==15u?0u:(repeat&15u)+1u;
    D.base_offset=D.frozen=D.done=0u;
    ++D.generation;
    if(clock_now(&now)) {
        D.base_counter=now;D.state=KUI_TOY_CDDA_PLAYING;
    } else {
        D.state=KUI_TOY_CDDA_LOST;++D.lost;
    }
}
/* One more pass completed: a lap, or the play's end. */
static bool lap(void) {
    ++D.done;
    if(D.passes && D.done>=D.passes) {
        D.done=D.passes-1u;D.frozen=D.end-D.first-1u;
        D.state=KUI_TOY_CDDA_ENDED;++D.ends;++D.generation;
        return false;
    }
    ++D.laps;
    return true;
}
/* Where the play stands now. The base moves on by whole groups of three
 * sectors, which are whole ticks (31,250), so nothing is lost to rounding,
 * and passes wrap by subtraction: no division by a play's length. The 0 to 2
 * sectors of a group not yet whole are added for the answer; when they cross
 * into the next pass, that lap is counted now and the base stands up to two
 * sectors before the pass's start until the group is whole. */
static void advance(void) {
    uint32_t now;
    if(!clock_now(&now)) {
        D.state=KUI_TOY_CDDA_LOST;++D.lost;++D.generation;return;
    }
    const int32_t length=(int32_t)(D.end-D.first);
    uint32_t ticks=D.base_counter-now;
    const uint32_t groups=ticks/KUI_TOY_CDDA_TICKS_3_SECTORS;
    D.base_counter-=groups*KUI_TOY_CDDA_TICKS_3_SECTORS;
    ticks-=groups*KUI_TOY_CDDA_TICKS_3_SECTORS;
    uint32_t step=groups*3u;
    while(step>=(uint32_t)length) {
        step-=(uint32_t)length;
        if(!lap())return;
    }
    int32_t offset=D.base_offset+(int32_t)step;
    if(offset>=length) {
        offset-=length;
        if(!lap())return;
    }
    D.base_offset=offset;
    offset+=(int32_t)(ticks*3u/KUI_TOY_CDDA_TICKS_3_SECTORS);
    if(offset>=length) {
        offset-=length;D.base_offset-=length;
        if(!lap())return;
    }
    D.frozen=(uint32_t)offset;
}
/* The sector the play stands at. */
static uint32_t play_sector(void) {
    return D.first+D.frozen;
}
/* A play that stood ends: the service's status was the player's, and is 1. */
static void stop(struct kui_retail_gd *s) {
    if(D.state==KUI_TOY_CDDA_IDLE)return;
    s->drive_status=1u;D.state=KUI_TOY_CDDA_IDLE;++D.generation;++D.stops;
}
static void command(struct kui_retail_gd *s) {
    uint32_t first,end,track;
    switch(s->command) {
    case KUI_RETAIL_GD_PLAY:
        ++D.plays;
        if(!play_range(s,s->outputs[0],s->outputs[1],&first,&end))break;
        play_start(first,end,s->outputs[2],s->token,s->outputs[0]);
        return;
    case KUI_RETAIL_GD_PLAY2:
        ++D.play2s;
        if(!play2_range(s,s->outputs[0],s->outputs[1],&first,&end,&track))break;
        play_start(first,end,s->outputs[2],s->token,track);
        return;
    case KUI_RETAIL_GD_PAUSE:
        ++D.pauses;
        if(D.state==KUI_TOY_CDDA_PLAYING) {
            advance();
            if(D.state==KUI_TOY_CDDA_PLAYING)D.state=KUI_TOY_CDDA_PAUSED;
        }
        return;
    case KUI_RETAIL_GD_RELEASE:
        ++D.releases;
        if(D.state==KUI_TOY_CDDA_PAUSED) {
            uint32_t now;
            if(clock_now(&now)) {
                D.base_counter=now;D.base_offset=(int32_t)D.frozen;D.state=KUI_TOY_CDDA_PLAYING;
            } else {
                D.state=KUI_TOY_CDDA_LOST;++D.lost;++D.generation;
            }
        }
        return;
    case KUI_GD_STOP: case KUI_RETAIL_GD_SEEK: case KUI_GD_COMMAND_INIT:
        stop(s);
        return;
    case KUI_GD_PIOREAD: case KUI_GD_DMAREAD:
        if(D.state==KUI_TOY_CDDA_PLAYING)++D.reads_in_play;
        return;
    default:
        return;
    }
    /* A play the player cannot place is refused: today's answers. */
    ++D.refused;
    stop(s);
}
static bool position_query(const struct kui_retail_gd *s) {
    return s->command==KUI_RETAIL_GD_REQ_STAT ||
        (s->command==KUI_RETAIL_GD_GETSCD && s->outputs[0]<=1u);
}
#if KUI_TOY_CDDA_KEEP
/* The service's sectors are 2,048 or 2,352 bytes (datatype); 2,352 is 16 x 147,
 * and a division by a constant needs no division routine. */
static uint32_t sectors_in(uint32_t bytes,uint32_t sector_bytes) {
    return sector_bytes==2048u?bytes>>11:(bytes>>4)/147u;
}
/* KEEP: the engine's complete sectors of the read pending, as report() is
 * about to publish them (kui_retail_gd_progress's first branch, and its
 * observed counts), with the play's status. A failed engine, a read the
 * engine does not serve and every state but PLAYING are left to report(). */
static void keep(struct kui_retail_gd *s,const kui_retail_async_region_type *r) {
    if(D.state!=KUI_TOY_CDDA_PLAYING || !s->pending || r->engine.token!=s->token || r->engine.failed ||
       (s->command!=KUI_GD_PIOREAD && s->command!=KUI_GD_DMAREAD) ||
       (s->sector_bytes!=2048u && s->sector_bytes!=2352u))return;
    uint32_t sectors=r->engine.cursor.done;
    const uint32_t done=sectors_in(s->completed_bytes,s->sector_bytes);
    if(sectors>s->count)sectors=s->count;
    if(sectors<=done)return;
    s->completed_bytes=sectors*s->sector_bytes;
    s->diag.sectors_read+=sectors-done;++s->diag.read_steps;
    s->position_lba=s->lba+sectors-1u;s->drive_status=3u;
    ++D.keeps;
}
#endif
uint32_t kui_toy_cdda_player_visit(uint32_t event,uint32_t arg,
    kui_retail_async_region_type *r,const struct kui_native_ra_low_ops *ops) {
    (void)arg;(void)ops;
    struct kui_retail_gd *s=&r->shared.service;
    if(event==KUI_NATIVE_RA_MOVIE_COMPLETE) {
        /* After the functions INIT and RESET, and an ABORT of the play's own
         * request before it was executed. */
        if((s->status==KUI_GD_NOT_FOUND && !s->command && !s->pending) ||
           (s->error==KUI_GD_ERROR_CANCELLED && s->token==D.play_token &&
            D.state!=KUI_TOY_CDDA_IDLE))stop(s);
        return 0u;
    }
    if(event==KUI_TOY_CDDA_VISIT_END) {
#if KUI_TOY_CDDA_KEEP
        if(!r->engine.in_irq)keep(s,r);
#endif
        return 0u;
    }
    if(event!=KUI_NATIVE_RA_DELIVER || r->engine.in_irq)return 0u;
    /* A visit before the disc service answers one of the game's calls. */
#if KUI_TOY_CDDA_CARD
    /* Test 85 (CARD): the table the heartbeat's segments are taken from. */
    D.tracks=s->tracks;D.track_count=s->track_count;
#endif
    if(D.projected && (s->token!=D.projected_token || !s->pending)) {
        s->position_lba=D.saved_position;D.projected=0u;
    }
    if(s->command && s->token!=D.seen_token) {
        D.seen_token=s->token;
        command(s);
    }
    if(D.state==KUI_TOY_CDDA_IDLE)return 0u;
    if(D.state==KUI_TOY_CDDA_PLAYING)advance();
    s->drive_status=D.state==KUI_TOY_CDDA_PLAYING?3u:1u;
    if(s->pending && D.state!=KUI_TOY_CDDA_LOST && position_query(s)) {
        /* Kept from the first visit that finds it pending, and brought up to
         * the play's sector at every visit until it has been executed. */
        if(!D.projected) {
            D.saved_position=s->position_lba;D.projected=1u;D.projected_token=s->token;
            ++D.projections;
        }
        s->position_lba=play_sector();
    }
    return 0u;
}
#endif
