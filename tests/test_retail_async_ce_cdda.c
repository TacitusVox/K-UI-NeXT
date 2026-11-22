/* SPDX-License-Identifier: GPL-3.0-only */
/* Windows CE's background reader as the CD music modules compile it
 * (Makefile.armada-audio scia: the music bus hooks of KUI_ARMADA_CE_STREAM,
 * the slow window 16 doubling twice, the fast block check and the CE resume
 * fix), run through the whole CE reader suite (test_retail_async_ce.c) with
 * the production music transport answering every bus event. Built with
 * KUI_RETAIL_CE_VRAM_DMA the suite includes KISS Psycho Circus's and Virtua
 * Cop 2's PVR pieces. Part 1: the transport detached, as before the relay.
 * VRAM builds also run part 1b, the refused pieces through the background
 * service, and part 2, the transport attached and a music read pending while
 * each PVR piece is filled. The video memory is a host array; the G1 bus,
 * PVR timing and the music worker's CE thread are not modelled. */
#define _GNU_SOURCE
#include "kui/armada_ce_stream_transport.h"
#include "kui/armada_ce_excerpt_entry.h"
#include <pthread.h>
#include <sys/mman.h>

#define finish transport_finish
#include "../src/loader/armada_ce_stream_transport.c"
#undef finish

#define main existing_ce_reader_main
#include "test_retail_async_ce.c"
#undef main

/* ---- The controller entry, as armada_ce_excerpt_entry.c routes bus events ---- */
/* Each event and how often the transport answered it with 1 (DATA_ENTER:
 * data may go on; DATA_BLOCK: the reader yields to the music; IRQ, IDLE:
 * the music owned the bus). The suite's own CHECK count stays comparable. */
#define BUS_EVENTS (KUI_ARMADA_CE_STREAM_BUS_DATA_PREFETCH + 1u)
static unsigned bus_events[BUS_EVENTS], bus_true[BUS_EVENTS], other_calls, stream_wakes;
const struct kui_armada_ce_excerpt_entry_snapshot *kui_armada_ce_bind_probe(
    uint32_t function, uint32_t command, int32_t result, uint32_t request_address) {
    (void)result;
    if(function != KUI_ARMADA_CE_STREAM_BUS_EVENT) {++other_calls; return NULL;}
    assert(command < BUS_EVENTS);
    ++bus_events[command];
    uint32_t answer = kui_armada_ce_stream_transport_event(command, request_address);
    if(answer) ++bus_true[command];
    return (const struct kui_armada_ce_excerpt_entry_snapshot *)(uintptr_t)answer;
}
static void bus_counts(const char *label) {
    static const char *const name[BUS_EVENTS] = {"attach", "enter", "block", "take", "irq", "idle", "prefetch"};
    printf("%s:", label);
    for(unsigned i = 0; i < BUS_EVENTS; ++i) printf(" %s %u/%u", name[i], bus_true[i], bus_events[i]);
    printf(" (answered 1/calls)\n");
    memset(bus_events, 0, sizeof(bus_events)); memset(bus_true, 0, sizeof(bus_true));
}
void kui_retail_ce_stream_wake(void) {++stream_wakes; take_events();}

#if KUI_RETAIL_CE_VRAM_DMA
/* ---- Part 1b: refused pieces through the background service ---- */
/* The GD service test (test_retail_gd.c) refuses these only in the
 * synchronous build. Through the background reader: each is refused at
 * DMA_TRANSFER, nothing is mapped or written, and the stream still completes
 * into an admitted view afterwards. */
static void refused_vram_pieces(void) {
    static const struct { uint32_t address, bytes; } refused[] = {
        {0x047fffe0u, 0x40u}, {0x04800000u, 0x20u}, {0x05800000u, 0x20u}, {0x06000000u, 0x20u},
        {0x10000000u, 0x20u}, {0x1f000000u, 0x20u}, {0x044c0370u, 0x20u}, {0xc4000000u, 0x20u}};
    setup(48, true);
    mode(2048);
    memset(pvr, 0x3c, sizeof(pvr)); memset(pvr32, 0x3c, sizeof(pvr32));
    reference_read(45000, 1);
    int32_t token = request(KUI_RETAIL_GD_DMAREAD_STREAM, 45000, 1, 0);
    CHECK(token > 0);
    for(unsigned k = 0; k < sizeof(refused) / sizeof(refused[0]); ++k) {
        put(PIECE, refused[k].address); put(PIECE + 4, refused[k].bytes);
        CHECK(gd(KUI_GD_DMA_TRANSFER, (uint32_t)token, PIECE) == -1);
        CHECK(R.shared.service.pending && !R.shared.service.xfer_left && !R.shared.service.completed_bytes);
    }
    CHECK(!hw.pvr_writes && !hw.virtual_writes && !hw.writes);
    put(PIECE, 0x05000020u); put(PIECE + 4, 2048u);
    unsigned end = ev.dma_end, woken = ev.drive;
    CHECK(gd(KUI_GD_DMA_TRANSFER, (uint32_t)token, PIECE) == 0);
    for(unsigned round = 0;; ++round) {
        CHECK(round < 1000000u);
        if(ev.drive != woken) {woken = ev.drive; (void)gd(KUI_GD_EXEC, 0, 0);}
        if(ev.dma_end != end) {
            end = ev.dma_end;
            if(gd(KUI_GD_DMA_CHECK, (uint32_t)token, STATUS) == 0) break;
            continue;
        }
        if(!interrupts(4)) elapse();
    }
    CHECK(status(token) == KUI_GD_COMPLETED && get(STATUS + 8) == 2048u);
    CHECK(!memcmp(pvr32 + 0x20u, expected, 2048u) && pvr32[0x1fu] == 0x3c && pvr32[0x20u + 2048u] == 0x3c);
    for(uint32_t i = 0; i < sizeof(pvr); i += 4093u) CHECK(pvr[i] == 0x3c);
    untouched(ram + OUTPUT - BEGIN, 4096u);
}
/* ---- Part 2: the music read pending while the PVR pieces are filled ---- */
static uint8_t music[2u * 2352u] __attribute__((aligned(32)));
static uint8_t music_expected[2u * 2352u];
static uint32_t epoch;
static unsigned music_jobs, worker_visits;
static void music_submit(void) {
    memset(music, 0x5a, sizeof(music));
    CHECK(kui_armada_ce_stream_transport_submit(++epoch, 3u, 2u, (uintptr_t)music, sizeof(music)) ==
          KUI_ARMADA_CE_STREAM_TRANSPORT_PENDING);
}
/* The music worker's visit (a CE thread, never the interrupt): each finished
 * job's bytes are checked and the next one queued at once. */
static void music_worker(void) {
    struct kui_armada_ce_stream_transport_snapshot snap;
    enum kui_armada_ce_stream_transport_status st = kui_armada_ce_stream_transport_snapshot(epoch, &snap);
    if(st == KUI_ARMADA_CE_STREAM_TRANSPORT_DONE) {
        CHECK(!memcmp(music, music_expected, sizeof(music)));
        ++music_jobs;
        music_submit();
        return;
    }
    CHECK(st == KUI_ARMADA_CE_STREAM_TRANSPORT_PENDING);
    /* As armada_ce_stream_player.c's visit: one pump, then more while ready
     * programmed receptions or a yielded token search need no interrupt. */
    for(unsigned visit = 0; visit < 8u; ++visit) {
        ++worker_visits;
        (void)kui_armada_ce_stream_transport_pump_worker();
        if(kui_armada_ce_stream_transport_snapshot(epoch, &snap) != KUI_ARMADA_CE_STREAM_TRANSPORT_PENDING ||
           !kui_armada_ce_stream_transport_worker_can_pump()) break;
    }
}
/* Pass 0 as accepted; pass 1 with the music below its emergency floor (the
 * worker receives by programmed transfers, KUI_ARMADA_CE_STREAM_EMERGENCY_PIO);
 * pass 2 with a slow card token after every CMD18, so the music's restart
 * searches in the pump that needs it (KUI_ARMADA_CE_STREAM_CHAIN slices);
 * pass 3 with overruns, CRC errors and DMA1 taken by the game (the slow
 * window, the resume fix and the retries under the shared bus). */
static void attached_vram_pieces(void) {
    static const struct { uint32_t sectors, vram, body; } shapes[] = {
        {10, 0x044c0360u, 0x4800u}, {257, 0x04575600u, 0x80000u}, {10, 0x054c0360u, 0x4800u}};
    unsigned audio_blocks = 0, preemptions = 0;
    srand(4242);
    for(unsigned run = 0; run < 12; ++run) {
        const unsigned k = run % 3u, pass = run / 3u;
        kui_armada_ce_stream_transport_test_reset();
        setup(48, true);
        if(pass == 2) card.nac_first = 3u * 256u;
        manifest.storage_transport = KUI_STORAGE_SCI;
        CHECK(kui_retail_manifest_validate(&manifest) == KUI_GAME_OK);
        mode(2048);
        extern const struct kui_armada_ce_stream_bus kui_retail_ce_stream_bus;
        CHECK((uintptr_t)kui_armada_ce_bind_probe(KUI_ARMADA_CE_STREAM_BUS_EVENT,
            KUI_ARMADA_CE_STREAM_BUS_ATTACH, 0, (uint32_t)(uintptr_t)&kui_retail_ce_stream_bus) == 1u);
        CHECK(kui_armada_ce_stream_transport_attached());
        CHECK(kui_armada_ce_stream_transport_init(&manifest, (uintptr_t)music, (uintptr_t)music + sizeof(music)));
        kui_armada_ce_stream_transport_pio_floor(pass == 1 ? 0u : 1000000u, true);
        CHECK(kui_armada_ce_stream_transport_pio() == (pass == 1));
        CHECK(kui_retail_image_read(&reference, 3u, 2u, KUI_GAME_SECTOR_RAW,
                                    music_expected, sizeof(music_expected)) == KUI_GAME_OK);
        music_submit();
        memset(pvr, 0x3c, sizeof(pvr)); memset(pvr32, 0x3c, sizeof(pvr32));
        uint8_t *view = shapes[k].vram & 0x01000000u ? pvr32 : pvr;
        const uint8_t *other = view == pvr ? pvr32 : pvr;
        const uint32_t total = shapes[k].sectors * 2048u, body = shapes[k].body;
        const uint32_t offset = shapes[k].vram & 0x007fffffu;
        for(uint32_t at = 0; at < shapes[k].sectors; at += KUI_RETAIL_IMAGE_MAX_SECTORS) {
            uint32_t n = shapes[k].sectors - at;
            if(n > KUI_RETAIL_IMAGE_MAX_SECTORS) n = KUI_RETAIL_IMAGE_MAX_SECTORS;
            CHECK(kui_retail_image_read(&reference, 45000 + at, n, KUI_GAME_SECTOR_MODE1,
                big + at * 2048u, sizeof(big) - at * 2048u) == KUI_GAME_OK);
        }
        int32_t token = request(KUI_RETAIL_GD_DMAREAD_STREAM, 45000, shapes[k].sectors, 0);
        const uint32_t dest[3] = {OUTPUT & 0x1fffffffu, shapes[k].vram, (OUTPUT + 0x40u) & 0x1fffffffu};
        const uint32_t size[3] = {0x20u, body, 0x7e0u};
        for(unsigned i = 0; i < 3; ++i) {
            put(PIECE, dest[i]); put(PIECE + 4, size[i]);
            unsigned end = ev.dma_end, woken = ev.drive;
            CHECK(gd(KUI_GD_DMA_TRANSFER, (uint32_t)token, PIECE) == 0);
            for(unsigned round = 0;; ++round) {
                CHECK(round < 4000000u);
                /* The worker's visit; below its floor it also finds a
                 * music block that has arrived before its interrupt. */
                if(!(round % 7u)) {if(pass == 1) elapse(); music_worker();}
                /* Pass 3: the stress test's faults while the piece and the
                 * music share the card (overrun, CRC error, DMA1 taken by
                 * the game), each cleared a little later. */
                if(pass == 3 && round % 61u == 0u) {
                    int fault = rand() % 3;
                    if(fault == 0) {m.overrun_after = 1u + (unsigned)rand() % 512u; m.in_flight = rand() % 2;}
                    if(fault == 1) {card.corrupt_lba = 100u + (uint32_t)rand() % 1800u; card.corrupt_count = 1;}
                    if(fault == 2) m.chcr = 1;
                }
                if(pass == 3 && round % 61u == 20u) {m.chcr = 0; m.in_flight = false;}
                if(ev.drive != woken) {woken = ev.drive; (void)gd(KUI_GD_EXEC, 0, 0);}
                if(ev.dma_end != end) {
                    end = ev.dma_end;
                    if(gd(KUI_GD_DMA_CHECK, (uint32_t)token, STATUS) == 0) break;
                    continue;
                }
                if(!interrupts(4)) {elapse(); if(!(round % 3u)) (void)gd(KUI_GD_EXEC, 0, 0);}
            }
            CHECK(!R.shared.service.xfer_left);
        }
        m.chcr = 0; m.overrun_after = 0; card.corrupt_count = 0; m.in_flight = false;
        CHECK(status(token) == KUI_GD_COMPLETED && get(STATUS + 8) == total);
        struct kui_armada_ce_stream_transport_snapshot during;
        (void)kui_armada_ce_stream_transport_snapshot(epoch, &during);
        /* Music blocks were received while the game's stream was active:
         * the bus was shared, not taken in turns between requests. */
        CHECK(music_jobs >= 1u && during.data_blocks >= shapes[k].sectors * 4u);
        CHECK(!memcmp(ram + OUTPUT - BEGIN, big, 0x20u) && ram[OUTPUT - BEGIN + 0x20u] == 0xa5);
        CHECK(!memcmp(view + offset, big + 0x20u, body));
        CHECK(view[offset - 1u] == 0x3c && view[offset + body] == 0x3c);
        for(uint32_t i = 0; i < sizeof(pvr); i += 4093u) CHECK(other[i] == 0x3c);
        CHECK(!memcmp(ram + OUTPUT + 0x40u - BEGIN, big + 0x20u + body, 0x7e0u));
        CHECK(ram[OUTPUT - BEGIN + 0x40u + 0x7e0u] == 0xa5);
        CHECK(hw.pvr_writes && !hw.virtual_writes && !R.engine.spill_bytes);
        /* The music kept its share of the bus during the transfer, and its
         * finished jobs were exact. Let the last one finish too. */
        for(unsigned round = 0; round < 1000000u; ++round) {
            struct kui_armada_ce_stream_transport_snapshot snap;
            if(kui_armada_ce_stream_transport_snapshot(epoch, &snap) != KUI_ARMADA_CE_STREAM_TRANSPORT_PENDING) break;
            elapse(); (void)interrupts(4); (void)kui_armada_ce_stream_transport_pump_worker();
        }
        struct kui_armada_ce_stream_transport_snapshot snap;
        CHECK(kui_armada_ce_stream_transport_snapshot(epoch, &snap) == KUI_ARMADA_CE_STREAM_TRANSPORT_DONE);
        CHECK(!memcmp(music, music_expected, sizeof(music)));
        audio_blocks += snap.audio_blocks; preemptions += snap.preemptions;
        const struct kui_sci_stream_stats *ss = kui_sci_stream_stats();
        printf("pass %u (stream: %u polled, %u overruns, %u repaired, %u slow blocks, %u CRC errors, %u retries): ",
               pass, ss->polled, ss->overruns, ss->repaired, ss->slow_blocks, ss->crc_errors, snap.retries);
        printf("attached %u-sector PVR piece at %08x: %u music jobs done during it, %u audio blocks, "
               "%u data blocks, %u holds, %u preemptions, %u worker visits, %u PVR writes (%u in the interrupt)\n",
               shapes[k].sectors, shapes[k].vram, music_jobs, snap.audio_blocks, snap.data_blocks, snap.holds,
               snap.preemptions, worker_visits, hw.pvr_writes, hw.pvr_irq_writes);
        bus_counts("  bus events");
        fflush(stdout);
        music_jobs = 0; worker_visits = 0;
    }
    CHECK(audio_blocks);
    printf("attached transport: %u audio blocks, %u preemptions in all\n", audio_blocks, preemptions);
}
/* The reader passes the address of a stack block (DATA_TAKE) through the
 * entry's 32-bit argument, as on the console. Run with a stack below 4 GiB,
 * and keep AddressSanitizer's frames on it (no detached fake stack). */
#if defined(__SANITIZE_ADDRESS__)
const char *__asan_default_options(void);
const char *__asan_default_options(void) { return "detect_stack_use_after_return=0"; }
#endif
static void *attached_thread(void *unused) {(void)unused; attached_vram_pieces(); return NULL;}
static void run_attached(void) {
    const size_t bytes = 8u << 20;
    void *stack = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    CHECK(stack != MAP_FAILED && (uintptr_t)stack + bytes <= UINT32_MAX);
    pthread_attr_t attr; pthread_t thread;
    CHECK(!pthread_attr_init(&attr) && !pthread_attr_setstack(&attr, stack, bytes));
    CHECK(!pthread_create(&thread, &attr, attached_thread, NULL) && !pthread_join(thread, NULL));
    pthread_attr_destroy(&attr);
    munmap(stack, bytes);
}
#endif

int main(void) {
    kui_armada_ce_stream_transport_test_reset();
    CHECK((uintptr_t)&R <= UINT32_MAX);
    (void)existing_ce_reader_main();
    /* Detached, the transport grants every data entry and prefetch and
     * claims nothing else; the controller's other functions are never called. */
    CHECK(!kui_armada_ce_stream_transport_attached() && !other_calls);
    CHECK(bus_events[KUI_ARMADA_CE_STREAM_BUS_DATA_ENTER] && bus_events[KUI_ARMADA_CE_STREAM_BUS_DATA_TAKE] &&
          bus_events[KUI_ARMADA_CE_STREAM_BUS_DATA_BLOCK] && bus_events[KUI_ARMADA_CE_STREAM_BUS_IRQ] &&
          bus_events[KUI_ARMADA_CE_STREAM_BUS_IDLE]);
    CHECK(bus_true[KUI_ARMADA_CE_STREAM_BUS_DATA_ENTER] == bus_events[KUI_ARMADA_CE_STREAM_BUS_DATA_ENTER] &&
          bus_true[KUI_ARMADA_CE_STREAM_BUS_DATA_PREFETCH] == bus_events[KUI_ARMADA_CE_STREAM_BUS_DATA_PREFETCH] &&
          !bus_true[KUI_ARMADA_CE_STREAM_BUS_DATA_TAKE] && !bus_true[KUI_ARMADA_CE_STREAM_BUS_DATA_BLOCK] &&
          !bus_true[KUI_ARMADA_CE_STREAM_BUS_IRQ] && !bus_true[KUI_ARMADA_CE_STREAM_BUS_IDLE]);
    printf("CE reader suite with the CD music reader flags: %u checks passed\n", checks);
    bus_counts("transport detached, bus events");
#if KUI_RETAIL_CE_VRAM_DMA
    unsigned before = checks;
    refused_vram_pieces();
    printf("refused PVR pieces through the background service: %u checks\n", checks - before);
    run_attached();
#endif
    printf("retail async reader (Windows CE, CD music%s): %u checks passed\n",
           KUI_RETAIL_CE_VRAM_DMA ? ", PVR stream pieces" : "", checks);
    return 0;
}
