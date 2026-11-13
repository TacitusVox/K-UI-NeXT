/* SPDX-License-Identifier: GPL-3.0-only */
/* The real GD service, asynchronous DMA stream and raw-sector verifier
 * against the shared card and register model. */
#include "kui/hash.h"

#define KUI_TOY_SDK_LIVENESS_MAIN toy_retained_sdk_tests
#include "test_toy_sdk_liveness.c"

#if !KUI_TOY_SUITE_VERIFIED_RESUME
#error "Toy resume integration fixture requires the verified-resume mode"
#endif

static uint8_t bcd_field(uint32_t n) { return (uint8_t)((n / 10u) * 16u + n % 10u); }

static void install_raw_sector(uint32_t lba) {
    uint8_t raw[KUI_GAME_RAW_BYTES];
    const struct kui_retail_track *track = &manifest.slots[2].track;
    const uint32_t first = (lba - track->start_lba) * KUI_GAME_RAW_BYTES;
    for(unsigned i = 0u; i < sizeof(raw); ++i) raw[i] = source(2u, first + i);
    const uint32_t fad = lba + 150u;
    raw[12] = bcd_field(fad / 4500u);
    raw[13] = bcd_field((fad / 75u) % 60u);
    raw[14] = bcd_field(fad % 75u);
    const uint32_t edc = kui_cd_edc(raw, 16u + KUI_GAME_DATA_BYTES);
    for(unsigned i = 0u; i < 4u; ++i) raw[16u + KUI_GAME_DATA_BYTES + i] = (uint8_t)(edc >> (8u * i));
    CHECK(kui_sector_edc_valid_at(raw, fad));
    for(unsigned i = 0u; i < sizeof(raw); ++i) {
        const uint32_t file_block = (first + i) / 512u;
        const struct kui_retail_extent *extent = &manifest.slots[track->first_extent].extent;
        CHECK(file_block >= extent->file_block && file_block - extent->file_block < extent->blocks);
        const uint32_t lba_on_card = extent->card_lba + file_block - extent->file_block;
        image_card[lba_on_card * 512u + (first + i) % 512u] = raw[i];
    }
}


#define AHEAD (kui_native_readahead_aux_context.movie_ahead)
#define RA (kui_native_readahead_aux_context)
#define OWNER (BEGIN + 0x400u)
static uint32_t visit_count;
extern void kui_toy_movie_present_boundary(uint32_t,uintptr_t,unsigned);
extern void kui_toy_movie_ahead_update(uint32_t);
uint32_t kui_native_readahead_aux_visit(uint32_t event,uint32_t arg,
    kui_retail_async_region_type *r,uint32_t reserved) {
    CHECK(hw.sr==0xf0u);
    ++visit_count;
    return kui_native_readahead_aux_dispatch(event,arg,r,reserved);
}
static void movie_update(uint32_t ctx) {
    hw.sr=0xf0u;
    kui_toy_movie_ahead_update(ctx);
    hw.sr=0u;
}
static void fixture_ahead(void) {
    setup_movie_card(1u, 1400u);
    for(uint32_t i=0;i<140u;++i)install_raw_sector(MOVIE_LBA+i);
    struct kui_native_ra_config config=movie_configuration();
    config.movie_ranges[3]=(struct kui_toy_movie_range){MOVIE_LBA-386u,386u+140u};
    bind_movie_configuration(&config);
    fixture_movie_started(KUI_TOY_MOVIE_EXIT_STARTED,0x07dbu);
    RA.onset[1].quiet_ctx=UINT32_MAX;
    kui_toy_movie_present_boundary(OWNER,(uintptr_t)&RA.movie_exit[1],KUI_TOY_MOVIE_EXIT_STARTED);
    visit_count=0u;
}
static void status_completed(uint32_t token,uint32_t lba,uint32_t n) {
    CHECK(sdk_poll(KUI_GD_CHECK,token,movie_async_poll)==KUI_GD_COMPLETED);
    CHECK(get(STATUS+8u)==n*KUI_GAME_DATA_BYTES && !get(STATUS+4u));
    compare(lba,n,OUTPUT);
}
static void no_guest_data(unsigned old_writes,uint32_t n) {
    CHECK(hw.writes==old_writes && !R.engine.cursor.done && !R.shared.service.completed_bytes);
    for(uint32_t i=0u;i<n*KUI_GAME_DATA_BYTES;++i)CHECK(ram[OUTPUT-BEGIN+i]==0xa5u);
}
static void finish_cold(uint32_t token,uint32_t lba,uint32_t n,bool fallback) {
    unsigned visits=0;
    while(R.shared.service.pending) {
        CHECK(++visits<4000u && !R.engine.failed);
        if(kui_sci_stream_busy())independent_sdk_irq();
        else CHECK(sdk_poll(KUI_GD_EXEC,0u,movie_async_poll)==0);
        if(R.shared.service.pending && !fallback)no_guest_data(0u,n);
    }
    status_completed(token,lba,n);
}
static void fill_idle(uint32_t at) {
    unsigned visits=0;
    const unsigned writes=hw.writes;
    while(AHEAD.verified<at) {
        if(++visits==3999u)
            fprintf(stderr,"progress fail: at %u verified %u pos %u blk %u state %u recv %u busy %d hook %u cmd %u armed %u offsets %u\n",
                at,AHEAD.verified,R.shared.stream.position,AHEAD.cursor.block,R.shared.stream.state,AHEAD.receiving,
                kui_sci_stream_busy(),R.engine.hooked,R.shared.service.command,R.engine.release.armed,AHEAD.cursor.offset);
        CHECK(visits<4000u && AHEAD.variant==4u && !AHEAD.error);
        CHECK(!R.shared.service.pending && !R.engine.active && hw.writes==writes);
        if(kui_sci_stream_busy())independent_sdk_irq();
        else movie_update(OWNER);
    }
}
static void warm(uint32_t lba,uint32_t n) {
    CHECK(AHEAD.first==lba && AHEAD.verified>=lba+n);
    memset(ram+OUTPUT-BEGIN,0xa5u,64u*KUI_GAME_DATA_BYTES+1u);
    unsigned hits=RA.stats.hit_requests,cmd18=card.cmd18,cmd12=card.cmd12;
    uint32_t token=(uint32_t)request(lba,n,OUTPUT);
    if(R.shared.service.pending)CHECK(sdk_poll(KUI_GD_EXEC,0u,movie_async_poll)==0);
    status_completed(token,lba,n);
    CHECK(card.cmd18==cmd18 && card.cmd12==cmd12 && RA.stats.hit_requests==hits+1u);
    CHECK(AHEAD.first==lba+n);
}
static void run_warm(void) {
    fixture_ahead();
    uint32_t token=(uint32_t)request(MOVIE_LBA,14u,OUTPUT);
    CHECK(AHEAD.variant==4u && !R.engine.cursor.raw);
    no_guest_data(0u,14u);
    finish_cold(token,MOVIE_LBA,14u,false);
    card.nac=3u*KUI_SCI_STREAM_TOKEN_SLICE;
    unsigned visits=visit_count;
    movie_update(OWNER+4u);
    CHECK(visit_count==visits);
    fill_idle(MOVIE_LBA+90u);
    warm(MOVIE_LBA+14u,60u);
    CHECK(AHEAD.head==74u);
    warm(MOVIE_LBA+74u,14u);
    CHECK(AHEAD.head==8u);
    fill_idle(MOVIE_LBA+115u);
    warm(MOVIE_LBA+88u,14u);
    CHECK(card.cmd18==1u && !card.cmd12 && visit_count);
    printf("Ahead idle verification, 3 exact warm tokens including ring wrap, 1 CMD18, %u UPDATE entries passed\n",visit_count);
}
static void run_corrupt(void) {
    fixture_ahead();
    const struct kui_retail_track *track=&manifest.slots[2].track;
    const uint32_t first=(MOVIE_LBA-track->start_lba)*KUI_GAME_RAW_BYTES;
    const struct kui_retail_extent *extent=&manifest.slots[track->first_extent].extent;
    uint8_t *edc=image_card+extent->card_lba*512u+first+16u+KUI_GAME_DATA_BYTES;
    *edc^=0x80u;
    uint32_t token=(uint32_t)request(MOVIE_LBA,14u,OUTPUT);
    CHECK(AHEAD.variant==4u);
    for(unsigned n=0u;!RA.stats.abandoned;n++) {
        CHECK(n<400u && R.shared.service.pending && !R.engine.failed);
        no_guest_data(0u,14u);
        if(kui_sci_stream_busy())independent_sdk_irq();
        else CHECK(sdk_poll(KUI_GD_EXEC,0u,movie_async_poll)==0);
    }
    CHECK(R.shared.service.token==token && AHEAD.failed_token==token);
    CHECK(!hw.writes);
    *edc^=0x80u;
    finish_cold(token,MOVIE_LBA,14u,true);
    CHECK(!AHEAD.variant && card.cmd18>=2u && card.cmd12>=1u);
    printf("Ahead bad raw EDC quarantined with no guest writes and exact same-token fallback passed\n");
}
static void run_cancel_seek(void) {
    fixture_ahead();
    uint32_t token=(uint32_t)request(MOVIE_LBA,14u,OUTPUT);
    CHECK(AHEAD.variant==4u && kui_sci_stream_busy());
    CHECK(gd(KUI_GD_ABORT,token,0u)==0);
    CHECK(!AHEAD.variant && !kui_sci_stream_busy() && !R.shared.service.pending && !hw.writes);
    CHECK(card.cmd12==1u);
    CHECK(sdk_poll(KUI_GD_CHECK,token,movie_async_poll)==KUI_GD_FAILED);
    CHECK(get(STATUS+4u)==KUI_GD_ERROR_CANCELLED);
    token=(uint32_t)request(MOVIE_LBA+27u,14u,OUTPUT);
    finish_cold(token,MOVIE_LBA+27u,14u,false);
    CHECK(card.cmd18==2u);
    printf("Ahead canceled private in-flight DMA, no canceled writes, disjoint seek completed with original GD ownership\n");
}
static void run_repair_token_change(void) {
    fixture_ahead();
    uint32_t token=(uint32_t)request(MOVIE_LBA,14u,OUTPUT);
    CHECK(AHEAD.variant==4u);
    finish_cold(token,MOVIE_LBA,14u,false);
    fill_idle(MOVIE_LBA+28u);
    CHECK(kui_sci_stream_busy() && AHEAD.receive_token==token);
    m.overrun_after=250u;
    independent_sdk_irq();
    CHECK(!kui_sci_stream_busy() && R.shared.stream.state==4u && AHEAD.receive_token==token);
    const unsigned before=R.shared.stream.stats.toy_resumed_blocks;
    warm(MOVIE_LBA+14u,14u);
    CHECK(R.engine.token!=token && R.shared.stream.stats.toy_resumed_blocks==before);
    memset(ram+OUTPUT-BEGIN,0xa5u,64u*KUI_GAME_DATA_BYTES+1u);
    token=(uint32_t)request(MOVIE_LBA+28u,14u,OUTPUT);
    finish_cold(token,MOVIE_LBA+28u,14u,true);
    CHECK(R.shared.stream.stats.toy_resumed_blocks==before);
    printf("Ahead old-token parked suffix rejected while verified warm data and new token remain correct\n");
}

static int32_t aligned_enqueue_kick(void) {
    prepare_sdk_poll_frame(KUI_GD_EXEC,0u);
    put(poll_frame+28u,0x8c0bd55au);
    put(poll_frame+32u,0x400000f0u);
    put(poll_frame+36u,0x8c0b2b4eu);
    put(poll_frame+40u,0u);
    return sdk_poll_saved_frame(KUI_GD_EXEC,0u,poll_frame);
}
static uint32_t aligned_enqueue_request(uint32_t lba,uint32_t count) {
    hw.sr=0x400000f0u;
    kui_retail_hook_sr=hw.sr;
    uint32_t token=(uint32_t)request(lba,count,OUTPUT);
    CHECK(R.shared.stream.toy_suite_kick==1u);
    CHECK(aligned_enqueue_kick()==0);
    CHECK(!R.shared.stream.toy_suite_kick);
    hw.sr=0u;
    return token;
}
static void strict_wait_preserved(bool remove_route) {
    fixture_ahead();
    const uint32_t token=(uint32_t)request(MOVIE_LBA,14u,OUTPUT);
    CHECK(AHEAD.variant==4u && R.engine.active && R.engine.release.armed);
    if(remove_route) R.vector600[KUI_NATIVE_SDK_SCI_COOKIE_INDEX]=0u;
    else {
        CHECK(aligned_enqueue_kick()==0);
        CHECK(AHEAD.variant==4u && R.engine.active && !R.shared.stream.toy_suite_kick);
    }
    CHECK(aligned_enqueue_kick()==0);
    CHECK(!AHEAD.variant && AHEAD.failed_token==token && !RA.stats.producer_blocks);
    CHECK(card.cmd12==1u);
    if(remove_route) {
        R.vector600[KUI_NATIVE_SDK_SCI_COOKIE_INDEX]=KUI_NATIVE_SDK_SCI_COOKIE;
        CHECK(sdk_poll(KUI_GD_EXEC,0u,movie_async_poll)==0);
    }
    finish_cold(token,MOVIE_LBA,14u,false);
}
static void run_actual_cadence(bool repaired) {
    fixture_ahead();
    hw.sr=0x400000f0u;
    kui_retail_hook_sr=hw.sr;
    uint32_t token=(uint32_t)request(MOVIE_LBA,14u,OUTPUT);
    CHECK(R.shared.stream.toy_suite_kick==1u && AHEAD.variant==4u && R.engine.active);
    CHECK(!RA.stats.producer_blocks && !hw.writes);
    CHECK(aligned_enqueue_kick()==0);
    CHECK(!R.shared.stream.toy_suite_kick);
    kui_retail_hook_sr=hw.sr=0u;
    if(!repaired) {
        CHECK(!AHEAD.variant && AHEAD.failed_token==token && !RA.stats.producer_blocks);
        CHECK(RA.resume.token==token && RA.resume.state!=0u && card.cmd12==1u);
        finish_cold(token,MOVIE_LBA,14u,false);
        CHECK(!RA.stats.producer_blocks && !RA.stats.hit_requests && !RA.stats.abandoned);
        printf("Test 62 original masked 0x8c0b2b4e kick disarms ahead before EDC while same GD token succeeds\n");
        return;
    }
    CHECK(AHEAD.variant==4u && !AHEAD.failed_token && !RA.resume.token && !card.cmd12);
    finish_cold(token,MOVIE_LBA,14u,false);
    fill_idle(MOVIE_LBA+60u);
    for(unsigned offset=14u;offset<=42u;offset+=14u) {
        memset(ram+OUTPUT-BEGIN,0xa5u,64u*KUI_GAME_DATA_BYTES+1u);
        unsigned hits=RA.stats.hit_requests;
        token=aligned_enqueue_request(MOVIE_LBA+offset,14u);
        status_completed(token,MOVIE_LBA+offset,14u);
        CHECK(RA.stats.hit_requests==hits+1u && card.cmd18==1u && !card.cmd12);
    }
    printf("Kick-aware policy retains original masked enqueue/EXEC and delivers 3 verified future tokens\n");
    strict_wait_preserved(false);
    strict_wait_preserved(true);
    printf("A second blocking EXEC and a first kickoff without armed SCI retain strict fallback\n");
}
int main(void) {
    movie_chunk_classification();
    run_warm();
    run_corrupt();
    run_cancel_seek();
    run_repair_token_change();
    run_actual_cadence(KUI_TOY_SUITE_MODE==64);
    return 0;
}
