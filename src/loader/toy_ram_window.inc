/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/toy_ram_window.h"

static const uint8_t *toy_window_block(uint32_t lba,uint32_t count) {
    const struct kui_toy_suite_seed *seed=&C.config.suite_seed;
    uint32_t offset=0u;
    for(unsigned i=0;i<2u;i++) {
        uint32_t relative=lba-seed->base[i];
        if(relative<seed->count[i] && count<=seed->count[i]-relative)
            return C.config.data+(offset+relative)*512u;
        offset+=seed->count[i];
    }
    return NULL;
}

/* Check every physical run for this whole request before writing to the
 * guest. A partially covered request goes through the normal SCI path. */
static uint32_t toy_window_blocks(const struct kui_retail_cursor *cursor) {
    const struct kui_retail_manifest *map=cursor->manifest;
    const struct kui_retail_track *track=&map->slots[cursor->track].track;
    uint32_t byte=kui_retail_track_file_offset(track)+
        (cursor->lba-track->start_lba)*KUI_GAME_RAW_BYTES;
    uint32_t first=byte/512u;
    const uint32_t end=(byte+(cursor->count-1u)*KUI_GAME_RAW_BYTES+16u+
        KUI_GAME_DATA_BYTES+511u)/512u;
    const uint32_t total=end-first;
    const uint32_t extent=kui_retail_track_first_extent(track);
    for(unsigned i=0;i<track->extent_count && first<end;i++) {
        const struct kui_retail_extent *run=&map->slots[extent+i].extent;
        const uint32_t relative=first-run->file_block;
        if(relative>=run->blocks)continue;
        uint32_t count=run->blocks-relative;
        if(count>end-first)count=end-first;
        if(!toy_window_block(run->card_lba+relative,count))return 0u;
        first+=count;
    }
    return first==end?total:0u;
}

static bool toy_window_deliver(kui_retail_async_region_type *r,unsigned variant) {
    const struct kui_retail_gd *s=&r->shared.service;
    struct kui_retail_async *e=&r->engine;
    if(!C.config.toy_poll_owner || !C.config.suite_seed.count[0] ||
       variant!=KUI_TOY_RAM_WINDOW_VARIANT+1u ||
       s->lba-KUI_TOY_RAM_WINDOW_FIRST>=KUI_TOY_RAM_WINDOW_END-KUI_TOY_RAM_WINDOW_FIRST ||
       s->count>KUI_TOY_RAM_WINDOW_END-s->lba || s->count>KUI_TOY_RAM_WINDOW_MAX_REQUEST ||
       e->cursor.lba!=s->lba || e->cursor.count!=s->count || e->cursor.offset ||
       e->cursor.done || C.config.ops->busy())return false;
    uint32_t blocks=toy_window_blocks(&e->cursor);
    if(!blocks)return false;
    while(blocks-- && e->cursor.done<e->cursor.count && !e->failed) {
        const uint8_t *block=toy_window_block(e->cursor.block,1u);
        if(!block || C.config.ops->feed(&e->cursor,block)!=KUI_GAME_OK)
            e->failed=KUI_GD_ERROR_IO;
    }
    if(e->cursor.done!=e->cursor.count && !e->failed)e->failed=KUI_GD_ERROR_IO;
    if(!e->failed) {
        e->active=0u;
        C.window_token=s->token;
    }
    return true;
}
