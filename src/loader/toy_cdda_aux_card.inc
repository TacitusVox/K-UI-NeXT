/* SPDX-License-Identifier: GPL-3.0-only */

/* Test 85 (CARD): a CD-music track's audio from the card, between the game's
 * own reads (include/kui/toy_cdda_card.h, include/kui/toy_cdda_mail.h).
 *
 * The audio's stream is the window's own continuous card stream
 * (toy_wire_ahead.inc): the same state, ring and tables, begun as the window
 * begins it and owned through the same flag (the keep's `on`). So everything
 * that serves or ends a stream by that flag serves and ends this one
 * unchanged: the stream's timer visit, a loan's quiesce, the low resident's
 * route for the SCI's interrupt. What differs is who reads it, and when:
 *
 * - The audio takes the card only in a TICK visit, which the worker's
 *   heartbeat makes for it, and only while no read of the game's stands in
 *   any state and no window is open (Test 85's YIELD gives an idle window up
 *   for a consumer that runs low). It brings the card to the segment's
 *   next block with the low reader, one checked block by programmed
 *   transfers a heartbeat at most, the card's wait for it cut into slices
 *   that later heartbeats take up; then the stream takes over.
 * - Blocks are read out of the stream in TICK visits alone, into the
 *   worker's sink. Only a block the stream checked is handed on: every byte
 *   under its CRC16, or one lost byte rebuilt from it. Anything else ends the
 *   stream there, and that block is read again by programmed transfers,
 *   which the low reader checks.
 * - While the audio owns the card, a delivery visit is the stream's: the low
 *   reader's own delivery would take the stream's reception for one of its
 *   own. A read of the game's ends the audio at once, in the visit that
 *   finds it, and is then served as it always was (data first).
 * - No card command is ever sent from an interrupt's visit: ending the
 *   stream sends none, and the low reader ends the card's command before its
 *   own next read, as it does behind a window's stream. */
#include "kui/toy_cdda_card.h"
#include "kui/toy_cdda_mail.h"
#if !KUI_TOY_CDDA_HOOK || !KUI_TOY_WIRE_TIMER || !KUI_TOY_WIRE_CLEAN || !KUI_SCI_STREAM_TOKEN_SLICED
#error "Test 85's audio reads through Test 84's hook with Test 81's stream and the sliced low reader"
#endif

enum { TOY_CDDA_SEEK_FAILS=8u };
/* What the audio has of the card (busy): the stream that runs is its own;
 * a read by programmed transfers of its own waits for the card. */
enum { TOY_CDDA_OWNS=1u,TOY_CDDA_SEEKS=2u };
/* The auxiliary's own: the segment's cursor, the segment it belongs to, the
 * card's state for it (enum kui_toy_cdda_card), what the audio has of the
 * card, and its programmed reads failed in a row. */
static struct {
    struct kui_retail_cursor cursor;
    uint32_t generation;
    uint8_t state,busy,fails;
} toy_cdda;
#define CD toy_cdda

/* With the context, when the cold stage binds the auxiliary. */
static void toy_cdda_reset(void) {
    memset(&CD,0,sizeof(CD));
}
/* A read of the game's in any state: begun by the low reader, accepted by
 * the service, or being executed. */
static bool toy_cdda_wanted(const kui_retail_async_region_type *r) {
    const struct kui_retail_gd *s=&r->shared.service;
    return r->engine.active || s->executing ||
        (s->pending && (s->command==KUI_GD_PIOREAD || s->command==KUI_GD_DMAREAD));
}
/* The stream the audio began has been ended by the reader's own paths (a
 * loan's quiesce stops whatever stream runs by the keep's flag). */
static void toy_cdda_seen(struct kui_toy_cdda_mail *m) {
    if(!(CD.busy&TOY_CDDA_OWNS) || toy_wire_keep()->on)return;
    CD.busy=0u;
    if(CD.state==KUI_TOY_CDDA_CARD_STREAMING)CD.state=KUI_TOY_CDDA_CARD_SEEKING;
    ++m->external_halts;
}
/* End the audio's use of the card where it stands, as toy_wire_halt ends a
 * window's stream. No card command is sent, so this is allowed in an
 * interrupt's visit: the low reader is left a card inside its CMD18 at an
 * unknown byte, which it ends before its next read. A search for the block's
 * token that the audio's own read left waiting is given up the same way
 * (the card is inside the CMD18 there too), so that the read which follows
 * may have its window; a wait for the card to end or to begin a command is
 * the low reader's to go on with, as it is behind a read the game ended. */
static void toy_cdda_halt(kui_retail_async_region_type *r,uint32_t *counted) {
    struct kui_sci_stream_state *stream=&r->shared.stream;
    const unsigned busy=CD.busy;
    if(!busy)return;
    CD.busy=0u;
    ++*counted;
    if(busy&TOY_CDDA_SEEKS) {
        if(stream->state==AHEAD_TOKEN_WAIT && stream->position==CD.cursor.block)stream->state=RA_LOST;
        return;
    }
    if(CD.state==KUI_TOY_CDDA_CARD_STREAMING)CD.state=KUI_TOY_CDDA_CARD_SEEKING;
    /* The SCI did not return to command framing: only a new acquisition
     * clears this, and no further card byte is attempted. */
    if(!kui_wire_stop(toy_wire_stream()))r->shared.port.fault=true;
    stream->state=RA_LOST;
    stream->ready[0]=stream->ready[1]=0u;
    stream->kept=0u;
    stream->arrived=0u;
    toy_wire_keep()->on=0u;
}
/* The cursor's bytes into the sink, in their order and never past its end. */
static void toy_cdda_write(void *context,uint32_t offset,const uint8_t *bytes,uint32_t count) {
    struct kui_toy_cdda_mail *m=context;
    const uint32_t at=m->sink_bytes;
    if(at>KUI_TOY_CDDA_SINK_BYTES || count>KUI_TOY_CDDA_SINK_BYTES-at || offset!=m->sink_offset+at) {
        CD.state=KUI_TOY_CDDA_CARD_REFUSED;
        return;
    }
    memcpy(m->sink+at,bytes,count);
    m->sink_bytes=at+count;
}
/* One checked block into the sink. False: the segment is complete, or it is
 * refused from here on (what the block held of it has been handed on). */
static bool toy_cdda_feed(struct kui_toy_cdda_mail *m,const uint8_t *data) {
    if(C.config.ops->feed(&CD.cursor,data)!=KUI_GAME_OK || CD.state==KUI_TOY_CDDA_CARD_REFUSED) {
        CD.state=KUI_TOY_CDDA_CARD_REFUSED;
        m->why=KUI_TOY_CDDA_WHY_FEED;
        ++m->refused;
        return false;
    }
    if(CD.cursor.done==CD.cursor.count) {
        CD.state=KUI_TOY_CDDA_CARD_DONE;
        return false;
    }
    return true;
}
/* The segment wanted: sectors of one audio track of 2,352-byte sectors (or
 * 2,448 with their subchannels, which the cursor leaves out), whose file's
 * pieces lie among the map's own. Zero and the cursor at its first block, or
 * why not. */
static uint32_t toy_cdda_segment(const kui_retail_async_region_type *r,struct kui_toy_cdda_mail *m) {
    const struct kui_retail_manifest *map=r->engine.manifest;
    const uint32_t index=m->want_track-1u,used=map->track_count+map->extent_count;
    if(index>=map->track_count || used>KUI_RETAIL_MANIFEST_SLOTS)return KUI_TOY_CDDA_WHY_TRACK;
    const struct kui_retail_track *t=&map->slots[index].track;
    const uint32_t first=kui_retail_track_first_extent(t);
    if(t->control&(4u|KUI_RETAIL_TRACK_COOKED|KUI_RETAIL_TRACK_2336|KUI_RETAIL_TRACK_MODE2))
        return KUI_TOY_CDDA_WHY_KIND;
    if(!t->extent_count)return KUI_TOY_CDDA_WHY_UNMAPPED;
    if(first<map->track_count || first>=used || t->extent_count>used-first)return KUI_TOY_CDDA_WHY_PIECES;
    if(m->want_lba<t->start_lba || m->want_end>t->end_lba || m->want_end<=m->want_lba)
        return KUI_TOY_CDDA_WHY_RANGE;
    return kui_retail_cursor_begin(&CD.cursor,map,m->want_lba,m->want_end-m->want_lba,
        KUI_GAME_SECTOR_RAW,toy_cdda_write,m)==KUI_GAME_OK?KUI_TOY_CDDA_WHY_NONE:KUI_TOY_CDDA_WHY_BEGIN;
}
/* The low reader has stopped behind the block before the segment's next one:
 * receive on from there, as toy_wire_take_over does for a window, with the
 * audio's own counters. */
static void toy_cdda_take_over(kui_retail_async_region_type *r,struct kui_toy_cdda_mail *m) {
    struct kui_wire_stream *w=toy_wire_stream();
    C.config.ops->hook();
    if(!r->engine.hooked || r->engine.release.armed!=0x10u ||
       !kui_wire_start(w,&m->wire,(uint8_t *)C.config.data+TOY_WIRE_RING_OFFSET,
           toy_resume_data()+TOY_WIRE_TABLE_OFFSET,r->shared.stream.sptr,
           (uint32_t)(uintptr_t)kui_toy_wire_timer_callback,
           r->vector600[KUI_NATIVE_SDK_SCI_VBR_INDEX],
           r->vector600[KUI_NATIVE_SDK_SCI_DEFAULT_INDEX])) {
        if(!m->first_error && r->engine.hooked && r->engine.release.armed==0x10u) {
            m->first_error=w->error;
            m->first_detail=w->detail;
        }
        ++m->declined;
        return;
    }
    /* The resident passes the SCI's interrupt on, and keeps its level,
     * exactly while its stream is in this state. */
    r->shared.stream.state=RA_DMA;
    toy_wire_keep()->on=1u;
    CD.busy=TOY_CDDA_OWNS;
    CD.state=KUI_TOY_CDDA_CARD_STREAMING;
}
/* Bring the card to the segment's next block: the stream takes over where
 * the low reader stands behind the block before it; otherwise the low reader
 * reads that block by programmed transfers, which it checks, once a
 * heartbeat at most. The card's wait is cut into the low reader's slices:
 * one that ends without the block is taken up by the next heartbeat's
 * visit. True: a block was handed on. */
static bool toy_cdda_seek(kui_retail_async_region_type *r,struct kui_toy_cdda_mail *m) {
    const struct kui_native_ra_low_ops *ops=C.config.ops;
    struct kui_sci_stream_state *stream=&r->shared.stream;
    bool fed=false;
    if(stream->state!=RA_PAUSED || stream->position!=CD.cursor.block) {
        if(!m->seek_ok)return false;
        m->seek_ok=0u;
        const uint32_t block=CD.cursor.block,tick=toy_wire_phase();
        const uint8_t bounded=stream->token_bounded,budget=stream->token_budget;
        enum kui_sci_stream_result result=KUI_SCI_STREAM_OK;
        const uint8_t *data=ops->take(block,&result);
        if(!data && result==KUI_SCI_STREAM_PENDING) {
            stream->token_bounded=1u;
            stream->token_budget=KUI_SCI_STREAM_TOKEN_SLICE;
            result=ops->fetch(block,AHEAD_TOKEN_LIMIT,true);
            if(result==KUI_SCI_STREAM_OK)data=ops->take(block,&result);
        }
        stream->token_bounded=bounded;
        stream->token_budget=budget;
        m->seek_ticks+=tick-toy_wire_phase();
        CD.busy=!data && result==KUI_SCI_STREAM_PENDING?TOY_CDDA_SEEKS:0u;
        if(!data) {
            if(CD.busy)++m->seek_waits;
            else if(++CD.fails>=TOY_CDDA_SEEK_FAILS) {
                /* The low reader starts the block afresh each time; give
                 * the segment up only when nothing gets through. */
                CD.state=KUI_TOY_CDDA_CARD_REFUSED;
                m->why=KUI_TOY_CDDA_WHY_CARD;
                ++m->refused;
            }
            return false;
        }
        CD.fails=0u;
        ++m->seek_blocks;
        fed=true;
        if(!toy_cdda_feed(m,data) || stream->state!=RA_PAUSED || stream->position!=CD.cursor.block)
            return true;
    }
    toy_cdda_take_over(r,m);
    return fed;
}
/* One TICK visit's card work for the audio, as the worker's heartbeat asks
 * for it. */
static void __attribute__((__noinline__)) toy_cdda_fill(kui_retail_async_region_type *r) {
    struct kui_toy_cdda_mail *m=C.config.cdda_mail;
    if(!m || m->magic!=KUI_TOY_CDDA_MAIL_MAGIC)return;
    const struct kui_retail_async *e=&r->engine;
    m->card=KUI_TOY_CDDA_CARD_IDLE;
    toy_cdda_seen(m);
    if(m->want_generation!=CD.generation) {
        /* Another segment, or none: the audio leaves the card. A segment is
         * taken up whatever the card is doing; only its reading waits. */
        toy_cdda_halt(r,&m->other_halts);
        CD.generation=m->have_generation=m->want_generation;
        CD.state=KUI_TOY_CDDA_CARD_IDLE;
        CD.fails=0u;
        m->sink_offset=m->sink_bytes=0u;
        if(CD.generation) {
            m->why=toy_cdda_segment(r,m);
            if(m->why) {
                CD.state=KUI_TOY_CDDA_CARD_REFUSED;
                ++m->refused;
            } else CD.state=KUI_TOY_CDDA_CARD_SEEKING;
        }
    }
    if(CD.state!=KUI_TOY_CDDA_CARD_SEEKING && CD.state!=KUI_TOY_CDDA_CARD_STREAMING) {
        m->card=CD.state;
        return;
    }
    /* The card is the audio's to use only with no read of the game's in any
     * state, the bus the low reader's and sound, and no movie request of the
     * low reader's own path standing. */
    if(!m->want_blocks || toy_cdda_wanted(r) || e->in_irq || !e->opened || r->shared.port.fault ||
       !r->shared.port.acquired || r->shared.stream.toy_suite_active)return;
    unsigned blocks=0u;
    if(!(CD.busy&TOY_CDDA_OWNS)) {
#if KUI_TOY_CDDA_YIELD
        /* Test 85 (YIELD): a window is the game's read-ahead, and the audio
         * waits for it to end. One that stands idle, with no request of the
         * game's in any state, is given up when the consumer says that it
         * runs low: by the window's own way out, which stops its stream and
         * ends the card's command, and counts no drop of a window. */
        if(KUI_TOY_AHEAD.variant) {
            if(!m->yield_ok || r->shared.service.pending || !toy_ahead_clear(r))return;
            ++m->yields;
        }
#else
        /* A window is the game's read-ahead: the audio waits for it to end. */
        if(KUI_TOY_AHEAD.variant)return;
#endif
        /* Nor is a reception of the low reader's own, or a wait of its own
         * that a read the game ended left standing, the audio's to go on
         * with. */
        if(C.config.ops->busy() || (r->shared.stream.state>RA_RESUME && !CD.busy))return;
        if(toy_cdda_seek(r,m))++blocks;
    }
    struct kui_wire_stream *w=toy_wire_stream();
    /* The SDK may rewrite the priority register for its own devices. */
    if(CD.busy&TOY_CDDA_OWNS)C.config.ops->hook();
    while(CD.busy&TOY_CDDA_OWNS) {
        /* Between blocks: a run that ended meanwhile is answered here, and
         * room made lets a stream that stood still for it start again. */
        kui_wire_service(w);
        if(w->state==KUI_WIRE_FAULT) {
            if(!m->first_error) {
                m->first_error=w->error;
                m->first_detail=w->detail;
            }
            toy_cdda_halt(r,&m->other_halts);
            break;
        }
        if(blocks>=m->want_blocks || m->sink_bytes>KUI_TOY_CDDA_SINK_BYTES-512u)break;
        struct kui_wire_block block;
        const enum kui_wire_parse parse=kui_wire_next(w,&block);
        if(parse==KUI_WIRE_NEED)break;
        if(parse!=KUI_WIRE_BLOCK || (block.kind!=KUI_WIRE_CLEAN && block.kind!=KUI_WIRE_REBUILT)) {
            /* Not checked here, or not to be followed: never handed on.
             * The block is the cursor's next still, and the low reader
             * reads it. */
            ++m->rereads;
            toy_cdda_halt(r,&m->other_halts);
            break;
        }
        const uint32_t next=CD.cursor.block+1u;
        ++blocks;
        ++m->blocks;
        if(block.kind==KUI_WIRE_REBUILT)++m->rebuilt;
        /* The segment's end, or the file goes on elsewhere on the card. */
        if(!toy_cdda_feed(m,block.data) || CD.cursor.block!=next) {
            toy_cdda_halt(r,&m->other_halts);
            break;
        }
    }
    m->card=CD.state;
}
/* A delivery visit while the audio has the card, or waits for it. True: the
 * visit is the audio stream's, and nothing below runs. */
static bool __attribute__((__noinline__)) toy_cdda_claim(kui_retail_async_region_type *r) {
    struct kui_toy_cdda_mail *m=C.config.cdda_mail;
    toy_cdda_seen(m);
    if(toy_cdda_wanted(r)) {
        /* Data first: the read is then served as it always was. */
        toy_cdda_halt(r,&m->data_halts);
        return false;
    }
    if(!(CD.busy&TOY_CDDA_OWNS))return false;
    kui_wire_service(toy_wire_stream());
    return true;
}
