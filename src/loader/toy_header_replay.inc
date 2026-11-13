/* SPDX-License-Identifier: GPL-3.0-only */
/* This is a one-sector replay of admitted AVI data. It owns neither SCI
 * nor the guest destination. The normal cursor and GD service still publish
 * the original read's verified bytes and both requests' terminal status. */
static void header_invalidate(void) {
    C.header.token=0u;
    C.header.valid=false;
}

static void header_write(void *unused,uint32_t offset,const uint8_t *bytes,uint32_t count) {
    (void)unused;
    C.header.write(C.header.context,offset,bytes,count);
    if(offset!=C.header.written || offset>KUI_GAME_DATA_BYTES || count>KUI_GAME_DATA_BYTES-offset) {
        C.header.written=UINT16_MAX;
        return;
    }
    if(!offset)C.header.first_chunk=(uint16_t)count;
    memcpy(C.header.data+offset,bytes,count);
    C.header.written+=(uint16_t)count;
}

static unsigned header_request(const kui_retail_async_region_type *r) {
    const struct kui_retail_gd *s=&r->shared.service;
    const struct kui_retail_async *e=&r->engine;
    const struct kui_retail_cursor *c=&e->cursor;
    /* The matched active native GD token owns the initialized cursor for
     * this exact request. Its read mode and count come from the same GD state. */
    if(!e->active || e->failed || !s->pending || !s->token || e->token!=s->token ||
       !r->shared.port.acquired || r->shared.port.fault ||
       s->count!=1u || s->sector_bytes!=KUI_GAME_DATA_BYTES)return 0u;
#if KUI_NATIVE_SESSION_CONTROLS
    if(s->disc_lid!=KUI_RETAIL_DISC_READY || s->disc_generation!=e->disc_generation)return 0u;
#endif
    const struct kui_retail_track *track=&c->manifest->slots[c->track].track;
    return kui_retail_track_sector_bytes(track)==KUI_GAME_RAW_BYTES &&
        kui_retail_track_header_bytes(track)==16u?movie_variant(s):0u;
}

static void header_publish(const kui_retail_async_region_type *r) {
    const struct kui_retail_gd *s=&r->shared.service;
    const struct kui_retail_async *e=&r->engine;
    if(s->token!=C.header.token)return;
    if(!s->token || s->status!=KUI_GD_COMPLETED || s->pending || s->error ||
       s->completed_bytes!=KUI_GAME_DATA_BYTES || e->failed || e->token!=s->token ||
       e->cursor.done!=1u || C.header.written!=KUI_GAME_DATA_BYTES) {
        header_invalidate();
        return;
    }
    C.header.last_block=e->cursor.block;
    C.header.stream_position=r->shared.stream.position;
    C.header.valid=true;
}

static bool header_replay(kui_retail_async_region_type *r,unsigned variant) {
    struct kui_retail_async *e=&r->engine;
    struct kui_retail_cursor *c=&e->cursor;
    const struct kui_retail_gd *s=&r->shared.service;
    /* A repeat keeps the parked card stream at its forward position. An
     * in-flight or uncertain reception belongs to the usual demand path. */
    if(!C.header.valid || s->token!=C.header.token+1u || s->lba!=C.header.lba ||
       s->sector_part!=C.header.sector_part ||
       s->track_type!=C.header.track_type || c->manifest!=C.header.manifest ||
       c->offset || C.spec_owned || C.quiesced ||
       r->shared.stream.state!=RA_PAUSED || r->shared.stream.position!=C.header.stream_position ||
       C.config.ops->busy())return false;
#if KUI_NATIVE_SESSION_CONTROLS
    if(s->disc_generation!=C.header.disc_generation)return false;
#endif
    C.header.token=s->token;
    C.header.valid=false;
    /* The cursor emits payload from one 512-byte card block per callback. */
    uint32_t count=C.header.first_chunk;
    for(uint32_t offset=0u;offset<KUI_GAME_DATA_BYTES;count=512u) {
        if(count>KUI_GAME_DATA_BYTES-offset)count=KUI_GAME_DATA_BYTES-offset;
        c->write(c->context,offset,C.header.data+offset,count);
        if(e->failed) {
            e->active=0u;
            header_invalidate();
            return true;
        }
        offset+=count;
    }
    c->block=C.header.last_block;c->done=1u;
    demand_complete(r);
    /* This demand consumed no card blocks. Preserve the existing physical
     * cache seed and let ordinary progress publish the new GD handle. */
    C.have_completed=1u;C.completed_token=e->token;
    ++C.playback[(variant-1u)/2u].replayed;
    return true;
}

static void header_deliver(uint32_t wait,kui_retail_async_region_type *r) {
    const struct kui_retail_gd *s=&r->shared.service;
    struct kui_retail_cursor *c=&r->engine.cursor;
    unsigned variant=header_request(r);
    if(!variant) {
        /* An accepted unrelated request advances the token. CHECK consumes
         * the command but leaves COMPLETED; INIT/RESET clears that status. */
        if(s->status!=KUI_GD_COMPLETED || s->token!=C.header.token || s->pending ||
           r->engine.failed || r->shared.port.fault)header_invalidate();
        deliver(wait,r);
        return;
    }
    if(s->token!=C.header.token) {
        if(header_replay(r,variant))return;
        C.header.valid=false;C.header.token=s->token;
        C.header.lba=s->lba;
        C.header.sector_part=s->sector_part;C.header.track_type=s->track_type;
        C.header.manifest=c->manifest;
#if KUI_NATIVE_SESSION_CONTROLS
        C.header.disc_generation=s->disc_generation;
#endif
        C.header.written=0u;
    }
    C.header.write=c->write;C.header.context=c->context;
    c->write=header_write;c->context=NULL;
    deliver(wait,r);
    c->write=C.header.write;c->context=C.header.context;
}
