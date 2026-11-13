/* SPDX-License-Identifier: GPL-3.0-only */
/* Cold installation only. Retain owned low operations and auxiliary lease
 * pointers by value; no pointer into this temporary stage survives. */
#include "kui/native_readahead.h"
#if KUI_TOY_SUITE_TABLE_CRC16
#include "kui/toy_sd_crc16.h"
#endif
#if !KUI_NATIVE_READAHEAD || !KUI_NATIVE_SDK_SCI_ROUTE || !KUI_VMU_NATIVE_BACKGROUND || !KUI_VMU_NATIVE_CACHED_CODE
#error Read-ahead installation requires its isolated cached native SDK SCI route
#endif
__asm__(".global __kui_native_readahead_stage\n"
        ".set __kui_native_readahead_stage,1\n"
        ".section .rodata.native_ra_aux,\"a\"\n.balign 32\n"
        ".global _native_ra_aux_blob_start\n_native_ra_aux_blob_start:\n.incbin \"" KUI_NATIVE_RA_AUX_BIN "\"\n"
        ".global _native_ra_aux_blob_end\n_native_ra_aux_blob_end:\n.balign 4\n"
        ".global _native_ra_aux_reloc_start\n_native_ra_aux_reloc_start:\n.incbin \"" KUI_NATIVE_RA_AUX_RELOC "\"\n"
        ".global _native_ra_aux_reloc_end\n_native_ra_aux_reloc_end:\n.previous\n");
extern const uint8_t native_ra_aux_blob_start[] __asm__("_native_ra_aux_blob_start");
extern const uint8_t native_ra_aux_blob_end[] __asm__("_native_ra_aux_blob_end");
extern const uint8_t native_ra_aux_reloc_start[] __asm__("_native_ra_aux_reloc_start");
extern const uint8_t native_ra_aux_reloc_end[] __asm__("_native_ra_aux_reloc_end");

static bool native_ra_code(uint32_t entry,uint32_t load,uint32_t bytes) {
    return !(entry&1u) && entry>=load+UINT32_C(0x1000) && entry<load+bytes;
}
static bool native_ra_stack_valid(uint32_t bottom,uint32_t top) {
    const volatile uint32_t *low=(const void *)(uintptr_t)bottom;
    const volatile uint32_t *high=(const void *)(uintptr_t)(top-16u);
    for(unsigned i=0;i<4u;i++)
        if(low[i]!=UINT32_C(0x4b524147) || high[i]!=UINT32_C(0x4b524147))return false;
    return true;
}
#if ((KUI_TOY_SUITE_MODE == 36 || KUI_TOY_SUITE_SLOW_RETRY) && KUI_TOY_SUITE_MODE < 42) || KUI_TOY_SUITE_DMA_ROUND_ROBIN
#include "toy_dma_priority_stage.inc"
#endif
#if KUI_TOY_SUITE_MOVIE_EXIT
#include "toy_movie_exit_stage.inc"
#endif
static void __attribute__((noipa)) native_readahead_bind(uint32_t payload) {
#if KUI_NATIVE_COLD_PROGRESS_STAGE
    retail_display_line("COLD 42 READ AHEAD BIND BEGIN");
#endif
    struct kui_native_ra_low_ops ops;uint32_t descriptor,sr;
    if(resident_bytes<KUI_NATIVE_RA_LOW_ENTRY_OFFSET+4u || resident_bytes<sizeof(ops))
        native_report(11u,1u);
    memcpy(&descriptor,resident_blob+KUI_NATIVE_RA_LOW_ENTRY_OFFSET,4u);
    if((descriptor&3u) || descriptor<KUI_RETAIL_RESIDENT_ADDRESS+44u ||
       descriptor-KUI_RETAIL_RESIDENT_ADDRESS>resident_bytes-sizeof(ops))
        native_report(11u,2u);
    memcpy(&ops,resident_blob+(descriptor-KUI_RETAIL_RESIDENT_ADDRESS),sizeof(ops));
    uint32_t storage=(uint32_t)(uintptr_t)ops.region_storage;
    if(ops.magic!=KUI_NATIVE_RA_OPS_MAGIC || ops.version!=KUI_NATIVE_RA_VERSION ||
       ops.bytes!=sizeof(ops) || (storage&3u) ||
       storage<KUI_RETAIL_RESIDENT_ADDRESS+resident_bytes ||
       resident_limit!=KUI_RETAIL_LOW_ASYNC_LIMIT || storage>resident_limit-4u ||
       native_word((KUI_RETAIL_RESIDENT_ADDRESS+KUI_NATIVE_RA_LOW_ENTRY_OFFSET)|UINT32_C(0x20000000))!=descriptor ||
       memcmp((const void *)(uintptr_t)(descriptor|UINT32_C(0x20000000)),&ops,sizeof(ops)))
        native_report(11u,3u);
    for(unsigned i=4u;i<sizeof(ops)/4u;i++) {
        uint32_t entry;memcpy(&entry,(const uint8_t *)&ops+4u*i,sizeof(entry));
        if((entry&1u) || entry<KUI_RETAIL_RESIDENT_ADDRESS+44u ||
           entry-KUI_RETAIL_RESIDENT_ADDRESS>=resident_bytes)
            native_report(11u,4u);
    }
    uint32_t lease=(payload&UINT32_C(0x1fffffff))|UINT32_C(0x80000000);
    uint32_t address=lease+KUI_VMU_NATIVE_BACKGROUND_REGION_OFFSET;
    if(KUI_VMU_NATIVE_BACKGROUND_LEASE_BYTES!=KUI_NATIVE_RA_LEASE_BYTES ||
       !kui_vmu_native_background_region_address(payload,address) ||
       native_word(storage)!=address || control()->status!=KUI_VMU_OBS_BOOT_INSTALLED ||
       control()->lease!=(payload&UINT32_C(0x1fffffff)))native_report(11u,5u);
    kui_retail_async_region_type *r=(void *)(uintptr_t)address;
    __asm__ volatile("stc sr,%0":"=r"(sr)::"memory");
    if(((sr>>4)&15u)!=15u || !r->shared.service.initialized ||
       r->shared.service.pending || r->shared.service.executing ||
       r->engine.active || r->engine.in_irq || r->engine.hooked ||
       r->engine.release.armed || r->engine.release.depth || r->engine.native_loan_state ||
       r->shared.stream.state>1u || r->shared.port.fault ||
       r->engine.opened!=(uint32_t)r->shared.port.acquired ||
#if KUI_TOY_SUITE_TABLE_CRC16
       r->vector600[KUI_TOY_SD_CRC16_SLOT] ||
#endif
       r->vector600[KUI_NATIVE_RA_ENTRY_INDEX] || r->vector600[KUI_NATIVE_RA_CONTEXT_INDEX] ||
       r->vector600[KUI_NATIVE_SDK_SCI_COOKIE_INDEX]!=KUI_NATIVE_SDK_SCI_COOKIE)
        native_report(11u,6u);
    uint32_t bytes=(uint32_t)(native_ra_aux_blob_end-native_ra_aux_blob_start);
    uint32_t reloc_bytes=(uint32_t)(native_ra_aux_reloc_end-native_ra_aux_reloc_start);
    const uint32_t *reloc=(const void *)native_ra_aux_reloc_start;
    if(bytes<UINT32_C(0x1002) || bytes>KUI_NATIVE_RA_AUX_IMAGE_BYTES ||
       reloc_bytes<KUI_VMU_NATIVE_RELOCATE_HEADER_BYTES ||
       reloc[5]!=KUI_NATIVE_RA_AUX_SPAN || reloc[6]!=UINT32_C(0x8c421000))
        native_report(11u,7u);
    uint32_t load=lease+KUI_NATIVE_RA_AUX_OFFSET,dest=load+UINT32_C(0x20000000);
    /* Retire stale cache lines before P2 initializes either newly owned
     * arena. Cache blocks remain invalid until a CRC-validated producer copy. */
    for(uint32_t at=load;at<load+KUI_NATIVE_RA_AUX_SPAN;at+=32u)
        __asm__ volatile("ocbp @%0"::"r"(at):"memory");
    for(uint32_t at=lease+KUI_NATIVE_RA_DATA_OFFSET;
        at<lease+KUI_NATIVE_RA_DATA_OFFSET+KUI_NATIVE_RA_DATA_BYTES;at+=32u)
        __asm__ volatile("ocbp @%0"::"r"(at):"memory");
    int relocated=kui_vmu_native_relocate(native_ra_aux_blob_start,bytes,
        native_ra_aux_reloc_start,reloc_bytes,(void *)(uintptr_t)dest,
        KUI_NATIVE_RA_AUX_SPAN,load);
    if(relocated)native_report(11u,8u);
    memset((void *)(uintptr_t)(dest+bytes),0,KUI_NATIVE_RA_AUX_SPAN-bytes);
    const struct kui_native_ra_exports *e=(const void *)(uintptr_t)dest;
    uint32_t context=(uint32_t)(uintptr_t)e->context;
    if(e->magic!=KUI_NATIVE_RA_AUX_MAGIC || e->version!=KUI_NATIVE_RA_VERSION ||
       e->bytes!=sizeof(*e) ||
       !native_ra_code((uint32_t)(uintptr_t)e->visit,load,bytes) ||
       !native_ra_code((uint32_t)(uintptr_t)e->init,load,bytes) ||
       e->image_end!=load+bytes || (context&31u) ||
       context<dest+sizeof(*e) || context>dest+UINT32_C(0x1000)-sizeof(struct kui_native_ra_public) ||
       e->stack_bottom!=lease+UINT32_C(0x20000000)+KUI_NATIVE_RA_STACK_OFFSET ||
       e->stack_top!=lease+UINT32_C(0x20000000)+KUI_NATIVE_RA_STACK_END ||
       e->memory_end!=e->stack_top)native_report(11u,9u);
#if KUI_TOY_SUITE_MOVIE_EXIT
    const uint32_t movie_exit=e->reserved[0];
    if(!native_ra_code(movie_exit,load,bytes) ||
       movie_exit==(uint32_t)(uintptr_t)e->visit || movie_exit==(uint32_t)(uintptr_t)e->init)
        native_report(11u,0x39u);
#if KUI_TOY_SUITE_MOVIE_CHUNK
    const uint32_t movie_chunk=e->reserved[1];
    if(!native_ra_code(movie_chunk,load,bytes) || movie_chunk==movie_exit ||
       movie_chunk==(uint32_t)(uintptr_t)e->visit || movie_chunk==(uint32_t)(uintptr_t)e->init)
        native_report(11u,0x42u);
#if KUI_TOY_SUITE_DECODE_WINDOW
    const uint32_t movie_refill=e->reserved[2];
    if(!native_ra_code(movie_refill,load,bytes) || movie_refill==movie_exit || movie_refill==movie_chunk ||
       movie_refill==(uint32_t)(uintptr_t)e->visit || movie_refill==(uint32_t)(uintptr_t)e->init)
        native_report(11u,0x57u);
#elif KUI_TOY_SUITE_TABLE_CRC16
    const uint32_t crc16_check=e->reserved[2];
    if(!native_ra_code(crc16_check,load,bytes) || crc16_check==movie_exit ||
       crc16_check==movie_chunk || crc16_check==(uint32_t)(uintptr_t)e->visit ||
       crc16_check==(uint32_t)(uintptr_t)e->init)
        native_report(11u,0x58u);
#endif
#if KUI_TOY_SUITE_FRAME_TIMING
    const uint32_t present_slot=e->reserved[3];
    if((present_slot&3u) || present_slot<load+0x1000u || present_slot>load+bytes-4u)
        native_report(11u,0x60u);
    const uint32_t present_entry=native_word(present_slot|UINT32_C(0x20000000));
    if(!native_ra_code(present_entry,load,bytes) || present_entry==movie_refill ||
       present_entry==movie_exit || present_entry==movie_chunk || present_entry==present_slot ||
       present_entry==(uint32_t)(uintptr_t)e->visit || present_entry==(uint32_t)(uintptr_t)e->init)
        native_report(11u,0x60u);
#endif
#if KUI_TOY_SUITE_MODE >= 61
    const uint32_t render_entry=e->reserved[4];
    if(!native_ra_code(render_entry,load,bytes) || render_entry==present_entry ||
       render_entry==movie_refill || render_entry==movie_exit || render_entry==movie_chunk ||
       render_entry==present_slot || render_entry==(uint32_t)(uintptr_t)e->visit ||
       render_entry==(uint32_t)(uintptr_t)e->init)native_report(11u,0x61u);
#endif
    for(unsigned i=KUI_TOY_SUITE_MODE>=61?5u:KUI_TOY_SUITE_FRAME_TIMING?4u:
        (KUI_TOY_SUITE_DECODE_WINDOW || KUI_TOY_SUITE_TABLE_CRC16)?3u:2u;i<6u;i++)
        if(e->reserved[i])native_report(11u,9u);
#else
    for(unsigned i=1u;i<6u;i++)if(e->reserved[i])native_report(11u,9u);
#endif
#else
    for(unsigned i=0u;i<6u;i++)if(e->reserved[i])native_report(11u,9u);
#endif
    for(unsigned i=0;i<4u;i++) {
        ((volatile uint32_t *)(uintptr_t)e->stack_bottom)[i]=UINT32_C(0x4b524147);
        ((volatile uint32_t *)(uintptr_t)(e->stack_top-16u))[i]=UINT32_C(0x4b524147);
    }
    kui_native_passive_publish(load,load+bytes);
    struct kui_native_ra_config cfg={KUI_NATIVE_RA_VERSION,sizeof(cfg),r,
        (const void *)(uintptr_t)descriptor,
        (void *)(uintptr_t)(lease+UINT32_C(0x20000000)+KUI_NATIVE_RA_DATA_OFFSET),
        KUI_NATIVE_RA_DATA_BYTES,lease&UINT32_C(0x1fffffff),KUI_NATIVE_RA_LEASE_BYTES
#if KUI_NATIVE_TOY_MOVIE_CAPTURE
        ,{{0}}
#endif
#if defined(KUI_TOY_SUITE_MODE)
        ,{{0},{0}},0u
#endif
#if KUI_TOY_SUITE_MOVIE_POLL
        ,0u
#endif
    };
#if KUI_NATIVE_TOY_MOVIE_CAPTURE
    memcpy(cfg.movie_ranges,toy_movie_ranges,sizeof(cfg.movie_ranges));
#endif
#if defined(KUI_TOY_SUITE_MODE)
    cfg.suite_seed=toy_suite_seed;
    cfg.suite_source=toy_suite_source;
#if KUI_TOY_SUITE_MOVIE_POLL
    /* The SDK call frames below are specific to this complete 1GUTH.BIN. */
    static const uint8_t toy_poll_digest[32]={
        0xee,0x68,0xa3,0x26,0xda,0x96,0x4f,0xec,0x58,0x92,0xb7,0x22,0x63,0x11,0xf2,0x29,
        0x3f,0xd3,0xa0,0xbb,0xf5,0xba,0xc4,0x96,0xec,0x82,0x99,0x19,0x64,0xd4,0x9b,0xbd};
    cfg.toy_poll_owner=cfg.suite_source==1u && exec_bytes==748444u &&
        !memcmp(native_driver.image_sha256,toy_poll_digest,sizeof(toy_poll_digest));
#if (KUI_TOY_SUITE_MODE == 36 || KUI_TOY_SUITE_SLOW_RETRY) && KUI_TOY_SUITE_MODE < 42
    if(cfg.toy_poll_owner && !toy_dma_priority_install())native_report(11u,0x36u);
#endif
#if KUI_TOY_SUITE_DMA_ROUND_ROBIN
    if(cfg.toy_poll_owner && !toy_dma_priority_install())native_report(11u,0x47u);
#endif
#endif
#if KUI_TOY_SUITE_MODE == 1 || KUI_TOY_SUITE_MODE == 2 || KUI_TOY_SUITE_MODE == 6 || KUI_TOY_SUITE_MODE == 35
    uint32_t seeded=cfg.suite_seed.count[0]+cfg.suite_seed.count[1];
#if KUI_TOY_SUITE_MODE == 1
    uint32_t unit=KUI_GAME_DATA_BYTES;
#else
    uint32_t unit=512u;
#endif
    if(seeded && seeded<=cfg.data_bytes/unit)memcpy(cfg.data,toy_suite_data,seeded*unit);
#endif
#endif
    uint32_t unused=0;
#if KUI_NATIVE_COLD_PROGRESS_STAGE
    retail_display_line("COLD 43 AUXILIARY INITIALIZE BEGIN");
#endif
    if(kui_vmu_native_worker_call((uint32_t)(uintptr_t)e->init,e->stack_top-32u,
       (const struct kui_vmu_native_worker_config *)(const void *)&cfg,
       (struct kui_vmu_native_worker_result *)(void *)&unused) ||
       !native_ra_stack_valid(e->stack_bottom,e->stack_top) ||
       ((const volatile struct kui_native_ra_public *)(uintptr_t)context)->busy)
        native_report(11u,10u);
#if KUI_NATIVE_COLD_PROGRESS_STAGE
    retail_display_line("COLD 44 AUXILIARY INITIALIZED");
#endif
#if KUI_TOY_SUITE_TABLE_CRC16
    if(cfg.toy_poll_owner) {
        const uint32_t slot=address+0x500u+4u*KUI_TOY_SD_CRC16_SLOT;
        r->vector600[KUI_TOY_SD_CRC16_SLOT]=crc16_check;
        native_publish(slot);
        if(native_word(slot|UINT32_C(0x20000000))!=crc16_check)
            native_report(11u,0x58u);
    }
#endif
    /* Publish context first. Entry zero is the disabled state throughout
     * relocation and initialization, so no partially bound call is possible. */
    r->vector600[KUI_NATIVE_RA_CONTEXT_INDEX]=context;
    native_publish(address+0x500u+4u*KUI_NATIVE_RA_CONTEXT_INDEX);
    if(native_word((address+0x500u+4u*KUI_NATIVE_RA_CONTEXT_INDEX)|UINT32_C(0x20000000))!=context)
        native_report(11u,11u);
    r->vector600[KUI_NATIVE_RA_ENTRY_INDEX]=(uint32_t)(uintptr_t)e->visit;
    native_publish(address+0x500u+4u*KUI_NATIVE_RA_ENTRY_INDEX);
    if(native_word((address+0x500u+4u*KUI_NATIVE_RA_ENTRY_INDEX)|UINT32_C(0x20000000))!=
       (uint32_t)(uintptr_t)e->visit)native_report(11u,12u);
#if KUI_TOY_SUITE_MOVIE_EXIT
#if KUI_TOY_SUITE_MOVIE_CHUNK
    if(cfg.toy_poll_owner && native_word(UINT32_C(0xac08d950))!=UINT32_C(0x8c08edbc))
        native_report(11u,0x42u);
#endif
#if KUI_TOY_SUITE_DECODE_WINDOW
    if(cfg.toy_poll_owner && native_word(UINT32_C(0xac098a88))!=UINT32_C(0x8c097186))
        native_report(11u,0x57u);
#endif
#if KUI_TOY_SUITE_FRAME_TIMING
    if(cfg.toy_poll_owner && native_word(UINT32_C(0xac04c568))!=UINT32_C(0x8c0fc464))
        native_report(11u,0x60u);
#endif
#if KUI_TOY_SUITE_MODE >= 61
    if(cfg.toy_poll_owner && native_word(UINT32_C(0xac04c774))!=UINT32_C(0x8c04c238))
        native_report(11u,0x61u);
#endif
    if(cfg.toy_poll_owner && !toy_movie_exit_install(movie_exit))native_report(11u,0x39u);
#if KUI_TOY_SUITE_MOVIE_CHUNK
    if(cfg.toy_poll_owner && !toy_movie_probe_install(UINT32_C(0x8c08d950),
            UINT32_C(0x8c08edbc),movie_chunk))native_report(11u,0x42u);
#endif
#if KUI_TOY_SUITE_DECODE_WINDOW
    if(cfg.toy_poll_owner && !toy_movie_probe_install(UINT32_C(0x8c098a88),
            UINT32_C(0x8c097186),movie_refill))native_report(11u,0x57u);
#endif
#if KUI_TOY_SUITE_FRAME_TIMING
    if(cfg.toy_poll_owner && !toy_movie_probe_install(UINT32_C(0x8c04c568),
            UINT32_C(0x8c0fc464),present_slot|UINT32_C(0x20000000)))native_report(11u,0x60u);
#endif
#if KUI_TOY_SUITE_MODE >= 61
    if(cfg.toy_poll_owner && !toy_movie_probe_install(UINT32_C(0x8c04c774),
            UINT32_C(0x8c04c238),render_entry))native_report(11u,0x61u);
#endif
#endif
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
    retail_display_line("SCI QUIET MOVIE SAMPLE");
    const volatile struct kui_native_ra_public *aux=(const void *)(uintptr_t)context;
    uint32_t opened=r->engine.opened;
    if(e->visit(KUI_NATIVE_RA_CALIBRATE,0u,r,0u)!=1u ||
       !native_ra_stack_valid(e->stack_bottom,e->stack_top) ||
       aux->busy || !aux->burst_quiet_status ||
       r->engine.active || r->engine.in_irq || r->engine.hooked ||
       r->engine.release.armed || r->engine.release.depth || r->engine.native_loan_state ||
       r->shared.service.pending || r->shared.service.executing ||
       r->shared.stream.state>1u || r->shared.port.fault ||
       r->engine.opened!=opened || (uint32_t)r->shared.port.acquired!=opened)
        native_report(11u,13u);
#endif
#if KUI_TOY_SUITE_MODE == 26 || KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
    retail_display_line("SCI BURST MOVIE READER READY");
#elif KUI_TOY_SUITE_DIRECT_PIO
    retail_display_line(cfg.toy_poll_owner?"SCI CONTINUOUS PIO READY":"SCI PIO OWNER UNVERIFIED");
#elif KUI_TOY_SUITE_FIRST_CUT_PIO
    retail_display_line(cfg.toy_poll_owner?"SCI FIRST-CUT PIO READY":"SCI PIO OWNER UNVERIFIED");
#elif KUI_TOY_SUITE_DMA_ROUND_ROBIN
    retail_display_line(cfg.toy_poll_owner?"SCI ROUND-ROBIN DMA READY":"SCI DMA OWNER UNVERIFIED");
#elif KUI_TOY_SUITE_WIDE_EDC
    retail_display_line(cfg.toy_poll_owner?"SCI FASTER EDC READY":"SCI RESUME OWNER UNVERIFIED");
#elif KUI_TOY_SUITE_MODE >= 62
    retail_display_line(cfg.toy_poll_owner?"SCI MOVIE READ AHEAD READY":"SCI RESUME OWNER UNVERIFIED");
#elif KUI_TOY_SUITE_MODE == 61
    retail_display_line(cfg.toy_poll_owner?"SCI BUMPER LATENCY READY":"SCI RESUME OWNER UNVERIFIED");
#elif KUI_TOY_SUITE_FRAME_TIMING
    retail_display_line(cfg.toy_poll_owner?"SCI FRAME TIMING READY":"SCI RESUME OWNER UNVERIFIED");
#elif KUI_TOY_SUITE_DECODE_WINDOW
    retail_display_line(cfg.toy_poll_owner?"SCI DECODE WINDOW READY":"SCI RESUME OWNER UNVERIFIED");
#elif KUI_TOY_SUITE_TABLE_CRC16
    retail_display_line(cfg.toy_poll_owner?"SCI TABLE CRC16 READY":"SCI RESUME OWNER UNVERIFIED");
#elif KUI_TOY_SUITE_FAST_COPY
    retail_display_line(cfg.toy_poll_owner?"SCI FAST COPY READY":"SCI RESUME OWNER UNVERIFIED");
#elif KUI_TOY_SUITE_MOVIE_TIMING
    retail_display_line(cfg.toy_poll_owner?"SCI MOVIE TIMING READY":"SCI RESUME OWNER UNVERIFIED");
#elif KUI_TOY_SUITE_INTRO_PREFILL
    retail_display_line(cfg.toy_poll_owner?"SCI INTRO PREFILL READY":"SCI RESUME OWNER UNVERIFIED");
#elif KUI_TOY_SUITE_FAST_CRC16
    retail_display_line(cfg.toy_poll_owner?"SCI FASTER CRC16 READY":"SCI RESUME OWNER UNVERIFIED");
#elif KUI_TOY_SUITE_INTRO_BUFFER
    retail_display_line(cfg.toy_poll_owner?"SCI INTRO BUFFER READY":"SCI RESUME OWNER UNVERIFIED");
#elif KUI_TOY_SUITE_RESUME_WAKE
    retail_display_line(cfg.toy_poll_owner?"SCI CONTROLLER RESUME READY":"SCI RESUME OWNER UNVERIFIED");
#elif KUI_TOY_SUITE_FAST_EDC
    retail_display_line(cfg.toy_poll_owner?"SCI RESUME FASTER CHECKS READY":"SCI RESUME OWNER UNVERIFIED");
#elif KUI_TOY_SUITE_RESUME_SLICE
    retail_display_line(cfg.toy_poll_owner?"SCI RESUME CADENCE READY":"SCI RESUME OWNER UNVERIFIED");
#elif KUI_TOY_SUITE_VERIFIED_RESUME
    retail_display_line(cfg.toy_poll_owner?"SCI VERIFIED STREAM RESUME READY":"SCI RESUME OWNER UNVERIFIED");
#elif KUI_TOY_SUITE_SEQUENTIAL_PIO
    retail_display_line(cfg.toy_poll_owner?"SCI SEQUENTIAL PIO READY":"SCI PIO OWNER UNVERIFIED");
#elif KUI_TOY_SUITE_BOUNDED_DMA
    retail_display_line(cfg.toy_poll_owner?"SCI BOUNDED DMA READY":"SCI DMA OWNER UNVERIFIED");
#elif KUI_TOY_SUITE_MODE == 42
    retail_display_line(cfg.toy_poll_owner?"SCI ORIGINAL GRAPHICS PRIORITY READY":"SCI PRIORITY OWNER UNVERIFIED");
#elif KUI_TOY_SUITE_MODE == 41
    retail_display_line(cfg.toy_poll_owner?"SCI PIPELINED RECOVERY READY":"SCI PIPELINED RECOVERY OWNER UNVERIFIED");
#elif KUI_TOY_SUITE_MODE == 40
    retail_display_line(cfg.toy_poll_owner?"SCI RECOVERY WINDOW READY":"SCI RECOVERY WINDOW OWNER UNVERIFIED");
#elif KUI_TOY_SUITE_MODE == 39
    retail_display_line(cfg.toy_poll_owner?"SCI MOVIE EXIT TRACE READY":"SCI MOVIE EXIT OWNER UNVERIFIED");
#elif KUI_TOY_SUITE_MODE == 38
    retail_display_line(cfg.toy_poll_owner?"SCI REFILL SLICE READY":"SCI REFILL SLICE OWNER UNVERIFIED");
#elif KUI_TOY_SUITE_MODE == 37
    retail_display_line(cfg.toy_poll_owner?"SCI SLOW RETRY READY":"SCI SLOW RETRY OWNER UNVERIFIED");
#elif KUI_TOY_SUITE_MODE == 36
    retail_display_line(cfg.toy_poll_owner?"SCI DMA PRIORITY READY":"SCI DMA PRIORITY OWNER UNVERIFIED");
#elif KUI_TOY_SUITE_MODE == 35
    retail_display_line(cfg.toy_poll_owner && cfg.suite_seed.count[0]?"SCI RAM WINDOW READY":"SCI RAM WINDOW UNAVAILABLE");
#elif KUI_TOY_SUITE_MOVIE_POLL
    retail_display_line(cfg.toy_poll_owner?"SCI MOVIE POLL READY":"SCI MOVIE POLL OWNER UNVERIFIED");
#elif KUI_TOY_SUITE_ASYNC_DEMAND
    retail_display_line("SCI ASYNC DEMAND READY");
#else
    retail_display_line("BOUNDED READ AHEAD READY / 192 KIB CACHE");
#endif
#if KUI_NATIVE_COLD_PROGRESS_STAGE
    retail_display_line("COLD 45 READ AHEAD BOUND");
#endif
}
