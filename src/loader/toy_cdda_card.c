/* SPDX-License-Identifier: GPL-3.0-only */
/* Test 85 (CARD): a CD-music track's audio from the card, the worker's side
 * (include/kui/toy_cdda_card.h, include/kui/toy_cdda_mail.h).
 *
 * The auxiliary reads the card; the worker's heartbeat asks it to, one TICK
 * visit at a time, and hands what the visit left in the shared block's sink
 * to the consumer. A heartbeat's visits are bounded as the reader's own
 * optional work has always been, by a soft window of TMU0 time: the first
 * visit is always made; another one only for a stream that has just handed
 * bytes on, and only if the longest such visit seen still fits the window.
 * (The window is the family's share of the reader's; a sound output that
 * runs low has the whole of it.)
 * One visit a heartbeat at most reads a block by programmed transfers
 * (src/loader/toy_cdda_aux_card.inc). The consumer's take() is handed the
 * sink itself and returns before the next visit; it leaves the shared block
 * alone.
 *
 * With no sound output compiled, the consumer here follows the drive's play
 * as an output would and drops the bytes: the card's traffic and the game's
 * reads beside it are then what a sounding build has. */
#include <stdbool.h>
#include <stdint.h>
#include "kui/retail_image.h"
#include "kui/toy_cdda_card.h"
#include "kui/toy_cdda_mail.h"
#include "kui/toy_cdda_player.h"
#include "kui/toy_pilot_clock.h"

#if !KUI_TOY_CDDA_CARD || !KUI_TOY_CDDA_DRIVE
#error "The audio's card work is Test 85's, behind Test 84's drive (KUI_TOY_CDDA_CARD)"
#endif

struct kui_toy_cdda_mail kui_toy_cdda_mail __attribute__((aligned(32)));
#define M kui_toy_cdda_mail
#define D kui_toy_cdda_drive
#define TCNT0 UINT32_C(0xffd8000c)

#ifdef KUI_NATIVE_READAHEAD_AUX_HOST_TEST
extern uint32_t kui_native_readahead_aux_test_read(uint32_t,unsigned);
extern uint32_t kui_toy_cdda_card_test_sr(void);
#define read_hw kui_native_readahead_aux_test_read
#define sr_read kui_toy_cdda_card_test_sr
#else
static uint32_t read_hw(uint32_t address,unsigned width) {
    if(width==1u)return *(volatile const uint8_t *)(uintptr_t)address;
    if(width==2u)return *(volatile const uint16_t *)(uintptr_t)address;
    return *(volatile const uint32_t *)(uintptr_t)address;
}
#if !KUI_TOY_CDDA_OUTPUT
static uint32_t sr_read(void) {
    uint32_t value;
    __asm__ __volatile__("stc sr,%0":"=r"(value)::"memory");
    return value;
}
#endif
#endif
/* TMU0's count, and whether the Toy timer profile holds (781,250 Hz). */
static bool clock_now(uint32_t *counter) {
    uint32_t frqcr=read_hw(UINT32_C(0xffc00000),2u);
    uint32_t tcor=read_hw(UINT32_C(0xffd80008),4u);
    uint32_t tcr=read_hw(UINT32_C(0xffd80010),2u);
    uint32_t tstr=read_hw(UINT32_C(0xffd80004),1u);
    *counter=read_hw(TCNT0,4u);
    return kui_toy_pilot_clock_profile(frqcr,tcor,tcr,tstr);
}

uint32_t kui_toy_cdda_track_of(uint32_t lba,uint32_t *start,uint32_t *end) {
    const union kui_retail_slot *tracks=D.tracks;
    /* The table is in the disc's order. */
    for(uint32_t n=0u;tracks && n<D.track_count;++n) {
        const struct kui_retail_track *t=&tracks[n].track;
        if(lba<t->start_lba)break;
        if(lba>=t->end_lba)continue;
        if(kui_retail_track_control(t))break;
        *start=t->start_lba;*end=t->end_lba;
        return n+1u;
    }
    return 0u;
}

/* The heartbeat's own of one call, kept off the stack: the completion that
 * calls the consumer stands on what the worker's private stack has left
 * (stack_valid, src/loader/vmu_native_passive.c), and a visit's gate saves
 * six registers there before it changes stacks. */
static struct {
    uint32_t window,begun,elapsed,rounds,room,state;
    kui_toy_cdda_take take;
    void *context;
} beat;
#define B beat

uint32_t kui_toy_cdda_card_heartbeat(const struct kui_toy_cdda_want *want,uint32_t room,
    kui_toy_cdda_take take,void *context) {
    B.room=room;B.take=take;B.context=context;
    B.window=kui_toy_cdda_card_window();
    if(!B.window)return KUI_TOY_CDDA_CARD_IDLE;
#if KUI_TOY_CDDA_OUTPUT
    /* Test 87: a sound output that runs low has the reader's whole soft
     * window, whatever share of it the family's optional work has. Music is
     * not optional work once it sounds: what its ring does not get in time
     * is not heard, and the ring begins again (KOF Dream Match 1999 under
     * Test 86, on half the window: 162 times in 83 s). A ring that holds
     * enough keeps to the family's share. */
    if(want->yield_ok)B.window=KUI_TOY_CDDA_CARD_WINDOW_TICKS;
#endif
    if(!want->generation) {
        /* Nothing is wanted: the audio leaves the card, told until it has. */
        if(M.have_generation) {
            M.want_generation=0u;
            ++M.visits;
            kui_toy_cdda_card_tick();
        }
        return KUI_TOY_CDDA_CARD_IDLE;
    }
    if(!clock_now(&B.begun))return KUI_TOY_CDDA_CARD_IDLE;
    M.magic=KUI_TOY_CDDA_MAIL_MAGIC;
    M.want_generation=want->generation;
    M.want_track=want->track;
    M.want_lba=want->lba;
    M.want_end=want->end;
    M.yield_ok=want->yield_ok;
    M.seek_ok=1u;
    B.elapsed=B.rounds=0u;
    B.state=M.have_generation==M.want_generation?M.card:KUI_TOY_CDDA_CARD_IDLE;
    for(;;) {
        /* A visit hands on whole card blocks: as many as the consumer has
         * room for, and the sink. */
        uint32_t blocks=B.room/512u;
        if(blocks>KUI_TOY_CDDA_SINK_BYTES/512u)blocks=KUI_TOY_CDDA_SINK_BYTES/512u;
        if(!blocks)break;
        M.want_blocks=(uint8_t)blocks;
        kui_toy_cdda_card_tick();
        ++B.rounds;
        const uint32_t spent=B.begun-read_hw(TCNT0,4u)-B.elapsed;
        B.elapsed+=spent;
        /* (B.state: what the visit before this one answered.) */
        if(B.state==KUI_TOY_CDDA_CARD_STREAMING && spent>M.round_longest)M.round_longest=spent;
        /* A visit that did not take the segment up answered nothing. */
        B.state=KUI_TOY_CDDA_CARD_IDLE;
        if(M.have_generation!=M.want_generation || M.sink_bytes>B.room)break;
        B.state=M.card;
        if(!M.sink_bytes)break;
        B.take(B.context,M.sink,M.sink_bytes,M.sink_offset);
        B.room-=M.sink_bytes;
        M.sink_offset+=M.sink_bytes;
        M.sink_bytes=0u;
        /* Another visit only for a stream that has just handed bytes on, and
         * only if the longest such visit seen still fits the window. */
        if(B.state!=KUI_TOY_CDDA_CARD_STREAMING || B.elapsed+M.round_longest>B.window)break;
    }
    if(B.rounds) {
        M.visits+=B.rounds;
        M.visit_ticks+=B.elapsed;
        ++M.heartbeats;
        if(B.elapsed>M.longest)M.longest=B.elapsed;
    }
    return B.state;
}
#undef B

#if !KUI_TOY_CDDA_OUTPUT
/* ---- the consumer with no sound output ---- */
struct kui_toy_cdda_listener kui_toy_cdda_listener;
#define L kui_toy_cdda_listener
#ifdef KUI_NATIVE_READAHEAD_AUX_HOST_TEST
extern void kui_toy_cdda_card_test_taken(const struct kui_toy_cdda_want *segment,const uint8_t *bytes,
    uint32_t count,uint32_t offset);
#endif

static void listener_take(void *context,const uint8_t *bytes,uint32_t count,uint32_t offset) {
    (void)context;
    if(offset!=L.taken)++L.wrong;
    L.taken+=count;
    L.ahead+=count;
    L.bytes+=count;
#ifdef KUI_NATIVE_READAHEAD_AUX_HOST_TEST
    kui_toy_cdda_card_test_taken(&L.want,bytes,count,offset);
#else
    (void)bytes;
#endif
}
/* value / by, the remainder left in *value; by is not zero. The worker has
 * no division routine, and needs none but here. */
static uint32_t divide(uint32_t *value,uint32_t by) {
    uint32_t rest=*value,quotient=0u,bit=1u;
    while(by<rest && !(by&UINT32_C(0x80000000))) {by<<=1;bit<<=1;}
    for(;bit;bit>>=1,by>>=1)
        if(rest>=by) {rest-=by;quotient|=bit;}
    *value=rest;
    return quotient;
}

void kui_toy_cdda_consumer_heartbeat(void) {
    uint32_t now,position=0u;
    /* The card's heartbeat is the worker's with SR.BL clear. */
    if(sr_read()&UINT32_C(0x10000000)) {++L.skipped;return;}
    bool playing=D.state==KUI_TOY_CDDA_PLAYING && clock_now(&now);
    if(playing) {
        /* Where the play stands, in sectors from its first pass's start: as
         * the player reckons it at the game's disc calls
         * (src/loader/toy_cdda_player.c), read here without moving it. */
        const uint32_t length=D.end-D.first,ticks=D.base_counter-now;
        int32_t at=D.base_offset+(int32_t)(ticks/KUI_TOY_CDDA_TICKS_3_SECTORS*3u+
            ticks%KUI_TOY_CDDA_TICKS_3_SECTORS*3u/KUI_TOY_CDDA_TICKS_3_SECTORS);
        if(at<0)at=0;
        position=D.done*length+(uint32_t)at;
        if(!L.following || L.play!=D.generation || L.releases!=D.releases) {
            /* A new play, or one released from its pause: from the sector
             * it stands at. */
            uint32_t offset=(uint32_t)at;
            L.done=D.done+divide(&offset,length);
            L.sector=D.first+offset;
            L.play=D.generation;
            L.releases=D.releases;
            L.following=1u;
            L.sync=position;
            L.ahead=0u;
            L.want.generation=0u;
            ++L.starts;
        }
    } else L.following=0u;
    /* Nothing to ask for: no play, or its last pass has been taken. */
    if(!playing || (D.passes && L.done>=D.passes)) {
        if(M.have_generation) {
            L.want.generation=0u;
            (void)kui_toy_cdda_card_heartbeat(&L.want,0u,listener_take,0);
        }
        return;
    }
    const int32_t lead=(int32_t)(L.ahead-(position-L.sync)*2352u);
    uint32_t room=lead<0?KUI_TOY_CDDA_LISTENER_LEAD:
        lead<(int32_t)KUI_TOY_CDDA_LISTENER_LEAD?KUI_TOY_CDDA_LISTENER_LEAD-(uint32_t)lead:0u;
    if(!L.want.generation) {
        /* The next segment: the track that holds the next sector, to its end
         * or the play's. */
        if(L.sector>=D.end) {
            /* A pass is taken: the next one from the play's first sector. */
            L.sector=D.first;
            if(D.passes && ++L.done>=D.passes)return;
        }
        uint32_t start,end,track;
        while(!(track=kui_toy_cdda_track_of(L.sector,&start,&end))) {
            /* No track holds it (a gap between tracks): silence, as far as
             * there is room. */
            if(room<2352u)return;
            room-=2352u;
            L.ahead+=2352u;
            ++L.silent;
            if(++L.sector>=D.end)return;
        }
        if(!++L.generation)++L.generation;
        L.want.generation=L.generation;
        L.want.track=track;
        L.want.lba=L.sector;
        L.want.end=end<D.end?end:D.end;
        L.taken=0u;
        ++L.segments;
    }
    L.want.yield_ok=lead<(int32_t)KUI_TOY_CDDA_LISTENER_LOW;
    if(kui_toy_cdda_card_heartbeat(&L.want,room,listener_take,0)>=KUI_TOY_CDDA_CARD_DONE) {
        /* Its last sector has been taken; or it is refused, and what is
         * left of it passes in silence. */
        L.ahead+=(L.want.end-L.want.lba)*2352u-L.taken;
        L.sector=L.want.end;
        L.want.generation=0u;
    }
}
#endif
