/* SPDX-License-Identifier: GPL-3.0-only */
/* Test 84 (SURVEY): the sound chip's survey (src/loader/toy_cdda_survey.c) on a
 * model of the bus. The model answers only the addresses the survey may read
 * (sound memory, the sound CPU's reset bit, the G2 FIFO and DMA registers,
 * TMU0) and fails on any other; the survey's host build is given a reader and
 * no writer, so it writes nothing. Every read of the chip must come with the
 * interrupt mask at 15, SR.BL clear, every G2 DMA channel off and at most
 * eight reads since the FIFO was last seen empty. The drivers' records are
 * the static readings of docs/cd-audio-corpus.md, built here word by word:
 * no driver file is used. Host model only; nothing here ran on a console. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kui/toy_cdda_player.h"
#include "kui/toy_cdda_survey.h"

struct kui_toy_cdda_drive kui_toy_cdda_drive;
static unsigned long checks;
#define CHECK(x) do { ++checks; if(!(x)) { \
    fprintf(stderr, "test_toy_cdda_survey.c:%d: %s\n", __LINE__, #x); exit(1); } } while(0)
#define V kui_toy_cdda_survey

#define SOUND UINT32_C(0xa0800000)
#define BYTES 0x200000u
#define BL UINT32_C(0x10000000)
#define SPACING 13021u
static uint8_t ram[BYTES];
static uint32_t sr, tcnt, tick_per_read, arm_reset, dma_enable[4], dma_start[4];
/* fifo_busy: polls the FIFO still reads busy; fifo_after: sound reads after
 * which it sticks busy for good. */
static uint32_t fifo_busy, fifo_after;
/* A word the sound CPU keeps writing: reads of it differ while torn_left. */
static uint32_t torn_address, torn_left;
static unsigned long reads, sound_reads, fifo_polls, since_empty, sr_writes, window_reads;
static uint32_t lowest_window = UINT32_MAX;

static bool masked(void) { return (sr & 0xf0u) == 0xf0u && !(sr & BL); }
static bool dma_idle(void) {
    for(unsigned c = 0; c < 4u; ++c) if((dma_enable[c] | dma_start[c]) & 1u) return false;
    return true;
}
uint32_t kui_toy_cdda_survey_test_read(uint32_t address) {
    ++reads;
    if(address == UINT32_C(0xffd8000c)) { tcnt -= tick_per_read; return tcnt; }
    CHECK(masked());
    if(address == UINT32_C(0xa05f688c)) {
        ++fifo_polls;
        tcnt -= tick_per_read;
        if(fifo_busy) { --fifo_busy; return 0x11u; }
        if(fifo_after && sound_reads >= fifo_after) return 0x20u;
        since_empty = 0;
        return 0u;
    }
    if(address >= UINT32_C(0xa05f7800) && address < UINT32_C(0xa05f7880)) {
        const uint32_t channel = (address - UINT32_C(0xa05f7800)) / 0x20u, at = (address - UINT32_C(0xa05f7800)) % 0x20u;
        CHECK(at == 0x14u || at == 0x18u);
        return at == 0x14u ? dma_enable[channel] : dma_start[channel];
    }
    CHECK(dma_idle() && ++since_empty <= 8u);
    tcnt -= tick_per_read;
    if(address == UINT32_C(0xa0702c00)) return arm_reset;
    CHECK(address >= SOUND && address - SOUND < BYTES && !(address & 3u));
    ++sound_reads;
    const uint32_t at = address - SOUND;
    if(at >= UINT32_C(0x1f4000)) { ++window_reads; if(at < lowest_window) lowest_window = at; }
    uint32_t value;
    memcpy(&value, ram + at, 4u);
    if(address == torn_address && torn_left) { --torn_left; value ^= (uint32_t)sound_reads; }
    return value;
}
uint32_t kui_toy_cdda_survey_test_sr(void) { return sr; }
void kui_toy_cdda_survey_test_set_sr(uint32_t value) {
    /* Only the mask moves, and never with SR.BL set. */
    CHECK(!(sr & BL) && (value & ~UINT32_C(0xf0)) == (sr & ~UINT32_C(0xf0)));
    sr = value;
    ++sr_writes;
}

static void put32(uint32_t at, uint32_t value) { memcpy(ram + at, &value, 4u); }
static void put8(uint32_t at, uint8_t value) { ram[at] = value; }
static uint32_t date_hash(const char *date) {
    uint8_t text[12] = {0};
    memcpy(text, date, strlen(date) < 12u ? strlen(date) : 12u);
    uint32_t hash = UINT32_C(0x811c9dc5);
    for(unsigned i = 0; i < 12u; ++i) hash = (hash ^ text[i]) * UINT32_C(0x01000193);
    return (hash ^ hash >> 16) & 0xffffu;
}
#define EXEC UINT32_C(0x43455845)
#define SEGA UINT32_C(0x41474553)
/* A driver's header in sound memory: version bytes, date text, state word. */
static void driver(uint32_t version, uint32_t state, const char *date) {
    memset(ram, 0, 0x20000u);
    put32(0x20u, version);
    put32(0xf8u, state);
    memcpy(ram + 0x50u, date, strlen(date));
}
static void fresh(void) {
    memset(ram, 0, sizeof(ram));
    memset(&V, 0, sizeof(V));
    memset(&kui_toy_cdda_drive, 0, sizeof(kui_toy_cdda_drive));
    kui_toy_cdda_drive.state = KUI_TOY_CDDA_PLAYING;
    sr = UINT32_C(0x40000000); /* privileged, interrupts open: the survey masks them */
    tcnt = UINT32_C(0x70000000); tick_per_read = 2u;
    arm_reset = fifo_busy = fifo_after = torn_address = torn_left = 0u;
    window_reads = 0u; lowest_window = UINT32_MAX;
    memset(dma_enable, 0, sizeof(dma_enable)); memset(dma_start, 0, sizeof(dma_start));
}
/* One heartbeat a sixtieth of a second after the last: the survey runs. */
static unsigned long heartbeat(void) {
    tcnt -= SPACING;
    const uint32_t before = sr;
    const unsigned long sound = sound_reads;
    kui_toy_cdda_survey_heartbeat();
    CHECK(sr == before);
    return sound_reads - sound;
}
static unsigned scenarios;

/* No play, no work: not even the clock is read. */
static void without_play(void) {
    fresh();
    driver(UINT32_C(0x00090002), EXEC, "1999.06.16");
    for(uint32_t state = KUI_TOY_CDDA_IDLE; state <= KUI_TOY_CDDA_LOST; ++state) {
        if(state == KUI_TOY_CDDA_PLAYING) continue;
        kui_toy_cdda_drive.state = state;
        const unsigned long before = reads, writes = sr_writes;
        for(unsigned i = 0; i < 100u; ++i) heartbeat();
        CHECK(reads == before && sr_writes == writes);
    }
    struct kui_toy_cdda_survey none;
    memset(&none, 0, sizeof(none));
    CHECK(!memcmp(&none, &V, sizeof(V)));
    ++scenarios;
}

/* At most one survey a sixtieth of a second: a heartbeat sooner reads the
 * clock and nothing else. */
static void spacing(void) {
    fresh();
    driver(UINT32_C(0x00090002), EXEC, "1999.06.16");
    put32(0x12060u, 0x00080830u);
    CHECK(heartbeat() && V.proved == 1u);
    for(unsigned i = 0; i < 5u; ++i) {
        const unsigned long before = reads;
        tcnt -= 2000u;
        kui_toy_cdda_survey_heartbeat();
        CHECK(reads == before + 1u && V.proved == 1u);
    }
    tcnt -= SPACING - 10000u;
    kui_toy_cdda_survey_heartbeat();
    CHECK(V.proved == 2u);
    ++scenarios;
}

/* MANATEE 2: the top two stream ports by the split bytes; their records'
 * playing bit, as the game plays on them. */
static void manatee_2(void) {
    fresh();
    driver(UINT32_C(0x00090002), EXEC, "1999.06.16");
    put32(0x12060u, 0x00080830u); /* 48 voices, 8 one-shot ports, 8 stream ports */
    for(unsigned i = 0; i < 100u; ++i) {
        put8(0x13800u + 0x40u * 6u, i % 10u < 3u ? 1u : 0u);
        put8(0x13800u + 0x40u * 7u, i % 10u == 9u ? 3u : 2u); /* bit 0 only */
        const unsigned long sound = heartbeat();
        CHECK(sound == (i ? 10u : 16u)); /* version, state, (date), split, two records: each twice */
    }
    CHECK(V.family == KUI_TOY_CDDA_FAMILY_GEN2 && V.version == UINT32_C(0x00090002) && V.state_word == EXEC);
    CHECK(V.date_hash == date_hash("1999.06.16") && V.identified && !V.changes);
    CHECK(V.proved == 100u && V.busy[0] == 30u && V.busy[1] == 10u && !V.unproved && !window_reads);
    /* The game moves the split (command 0x8D): 16 stream ports, the top two 14 and 15. */
    put32(0x12060u, 0x00101020u);
    put8(0x13800u + 0x40u * 6u, 1u); put8(0x13800u + 0x40u * 7u, 1u);
    put8(0x13800u + 0x40u * 14u, 1u); put8(0x13800u + 0x40u * 15u, 0u);
    heartbeat();
    CHECK(V.proved == 101u && V.busy[0] == 31u && V.busy[1] == 10u);
    /* Split bytes that are no split: not counted, said unproved. */
    put32(0x12060u, 0x00080831u);
    heartbeat();
    put32(0x12060u, 0x00013f00u);
    heartbeat();
    put32(0x12060u, 0x00110f20u);
    heartbeat();
    CHECK(V.proved == 101u && V.unproved == 3u);
    /* Not running (state 0xFF at start-up, or halted): no candidates. */
    put32(0x12060u, 0x00080830u);
    put32(0xf8u, 0xffu);
    heartbeat();
    CHECK(V.family == KUI_TOY_CDDA_FAMILY_UNKNOWN && V.changes == 1u && V.proved == 101u && V.unproved == 3u);
    put32(0xf8u, EXEC);
    heartbeat();
    CHECK(V.family == KUI_TOY_CDDA_FAMILY_GEN2 && V.changes == 2u && V.proved == 102u);
    ++scenarios;
}

/* MANATEE 1: the pool's entries 13 and 14 from 1.01.37 on, 14 and 15 before;
 * an entry is the record only if its register base is its channel's. */
static void manatee_1(void) {
    static const struct { uint32_t version; const char *date; uint32_t family, first; } builds[] = {
        {UINT32_C(0x003b0101), "1998.12.08", KUI_TOY_CDDA_FAMILY_GEN1, 13u},
        {UINT32_C(0x00370101), "1998.11.02", KUI_TOY_CDDA_FAMILY_GEN1, 13u},
        {UINT32_C(0x00400101), "1999.05.25", KUI_TOY_CDDA_FAMILY_GEN1, 13u},
        {UINT32_C(0x00360101), "1998.10.23", KUI_TOY_CDDA_FAMILY_GEN1_OLD, 14u},
        {UINT32_C(0x001d0101), "1998.08.31", KUI_TOY_CDDA_FAMILY_GEN1_OLD, 14u},
    };
    for(unsigned b = 0; b < sizeof(builds) / sizeof(builds[0]); ++b) {
        fresh();
        driver(builds[b].version, SEGA, builds[b].date);
        for(uint32_t e = 0; e < 16u; ++e) put32(0x12080u + 12u * e + 8u, UINT32_C(0x00801800) + 0x80u * e);
        for(unsigned i = 0; i < 40u; ++i) {
            for(uint32_t e = 0; e < 16u; ++e) put32(0x12080u + 12u * e + 4u, 0u);
            if(i < 12u) put32(0x12080u + 12u * builds[b].first + 4u, UINT32_C(0x0000c040));
            if(i >= 35u) put32(0x12080u + 12u * (builds[b].first + 1u) + 4u, 1u);
            /* The other candidate pair's owners are not ours to count. */
            put32(0x12080u + 12u * (builds[b].first == 13u ? 15u : 13u) + 4u, 7u);
            heartbeat();
        }
        CHECK(V.family == builds[b].family && V.proved == 40u && V.busy[0] == 12u && V.busy[1] == 5u);
        CHECK(V.date_hash == date_hash(builds[b].date) && !V.unproved);
        /* A pool elsewhere: the base word disproves it. */
        put32(0x12080u + 12u * (builds[b].first + 1u) + 8u, UINT32_C(0x00801800));
        heartbeat();
        CHECK(V.proved == 40u && V.unproved == 1u);
        /* Not running: no family, no candidates. */
        put32(0x12080u + 12u * (builds[b].first + 1u) + 8u, UINT32_C(0x00801800) + 0x80u * (builds[b].first + 1u));
        put32(0xf8u, 0xffffffffu);
        heartbeat();
        CHECK(V.family == KUI_TOY_CDDA_FAMILY_UNKNOWN && V.proved == 40u && V.unproved == 1u);
    }
    ++scenarios;
}

/* NAMIHEI: stream ports 6 and 7, records 0x11200 + 0x40 x port. */
static void namihei(void) {
    fresh();
    driver(UINT32_C(0x00120001), SEGA, "1999.06.08");
    for(unsigned i = 0; i < 20u; ++i) {
        put8(0x11200u + 0x40u * 6u, i & 1u);
        put8(0x11200u + 0x40u * 7u, i < 4u);
        put8(0x11200u + 0x40u * 5u, 1u);
        heartbeat();
    }
    CHECK(V.family == KUI_TOY_CDDA_FAMILY_NAMIHEI && V.proved == 20u && V.busy[0] == 10u && V.busy[1] == 4u);
    ++scenarios;
}

/* AUDIO64: the playing bytes of channels 62 and 63 at the header word 0xE0,
 * the ports' base word before them; the hybrids' host ports 14 and 15. */
static void audio64(void) {
    static const struct { uint32_t version, table, base, first, family; } builds[] = {
        {UINT32_C(0x02665a01), 0xaacu, UINT32_C(0x00800000), 62u, KUI_TOY_CDDA_FAMILY_AUDIO64}, /* 1.90f.2 */
        {UINT32_C(0x02625001), 0x1454u, UINT32_C(0x00800000), 62u, KUI_TOY_CDDA_FAMILY_AUDIO64}, /* 1.80b.2 */
        {UINT32_C(0x02665a01), 0x3fccu, UINT32_C(0x00801800), 14u, KUI_TOY_CDDA_FAMILY_HYBRID},  /* MIDIDA */
        {UINT32_C(0x02685a01), 0x40d0u, UINT32_C(0x00801800), 14u, KUI_TOY_CDDA_FAMILY_HYBRID},  /* MIDIDA 1.90h.2 */
    };
    for(unsigned b = 0; b < sizeof(builds) / sizeof(builds[0]); ++b) {
        fresh();
        driver(builds[b].version, 0u, "");
        put32(0xe0u, builds[b].table);
        put32(builds[b].table - 4u, builds[b].base);
        for(unsigned i = 0; i < 30u; ++i) {
            put8(builds[b].table + builds[b].first, i < 9u ? 0xffu : 0u);
            put8(builds[b].table + builds[b].first + 1u, i >= 28u ? 0xffu : 0u);
            put8(builds[b].table + builds[b].first - 1u, 0xffu);
            heartbeat();
        }
        CHECK(V.family == builds[b].family && V.proved == 30u && V.busy[0] == 9u && V.busy[1] == 2u);
        /* No proof: a base word of neither kind, a table out of the image, misaligned. */
        put32(builds[b].table - 4u, UINT32_C(0x00801000));
        heartbeat();
        put32(builds[b].table - 4u, builds[b].base);
        /* Each with a base word that would prove it: the bounds alone refuse. */
        put32(0xe0u, 0x10000u);
        put32(0xfffcu, builds[b].base);
        heartbeat();
        put32(0xe0u, builds[b].table + 2u);
        heartbeat();
        put32(0xe0u, 0x100u);
        put32(0xfcu, builds[b].base);
        heartbeat();
        CHECK(V.proved == 30u && V.unproved == 4u);
    }
    ++scenarios;
}

/* A driver of none of these families (AICADRV), and none at all: the identity
 * only. The sound CPU in reset: nothing but its reset bit is read. */
static void others(void) {
    fresh();
    driver(UINT32_C(0x31414349), UINT32_C(0x12345678), "2000.01.01");
    for(unsigned i = 0; i < 10u; ++i) heartbeat();
    CHECK(V.family == KUI_TOY_CDDA_FAMILY_UNKNOWN && V.version == UINT32_C(0x31414349) && !V.proved && !V.unproved);
    CHECK(!window_reads);
    fresh();
    for(unsigned i = 0; i < 10u; ++i) heartbeat();
    CHECK(V.family == KUI_TOY_CDDA_FAMILY_UNKNOWN && V.identified && !V.version && !V.proved);
    fresh();
    driver(UINT32_C(0x00090002), EXEC, "1999.06.16");
    arm_reset = 1u;
    for(unsigned i = 0; i < 10u; ++i) CHECK(!heartbeat());
    CHECK(V.family == KUI_TOY_CDDA_FAMILY_RESET && !V.identified && !V.proved);
    arm_reset = 0u;
    heartbeat(); /* MANATEE 2 without its split bytes yet: no proof */
    CHECK(V.family == KUI_TOY_CDDA_FAMILY_GEN2 && V.identified && V.unproved == 1u && !V.proved);
    ++scenarios;
}

/* A new driver mid-play (the game loads another): counted as a change, its
 * date read again. */
static void identity_change(void) {
    fresh();
    driver(UINT32_C(0x00090002), EXEC, "1999.06.16");
    put32(0x12060u, 0x00080830u);
    for(unsigned i = 0; i < 5u; ++i) heartbeat();
    driver(UINT32_C(0x00050202), EXEC, "2000.03.03");
    put32(0x12060u, 0x00080830u);
    CHECK(heartbeat() == 16u);
    CHECK(V.version == UINT32_C(0x00050202) && V.date_hash == date_hash("2000.03.03") && V.changes == 1u);
    CHECK(V.proved == 6u);
    ++scenarios;
}

/* Under AUDIO64, the window from 0x1F4000: 256 bytes a survey, a pass of 48
 * KiB in 192 surveys; whether the first pass met a non-zero word, and which
 * KiB changed in a later pass. A chunk the bus cut short is read again. */
static void window(void) {
    fresh();
    driver(UINT32_C(0x02665a01), 0u, "");
    put32(0xe0u, 0xaacu);
    put32(0xaa8u, UINT32_C(0x00800000));
    for(unsigned i = 0; i < 191u; ++i) heartbeat();
    CHECK(!V.window_passes && V.window_offset == 191u * 256u && lowest_window == UINT32_C(0x1f4000));
    heartbeat();
    CHECK(V.window_passes == 1u && !V.window_offset && !V.window_nonzero);
    CHECK(window_reads == 192u * 64u);
    /* The game writes KiB 5 and 40 after the first pass: two changed. */
    put32(0x1f4000u + 5u * 1024u + 512u, 1u);
    put32(0x1f4000u + 40u * 1024u + 1020u, UINT32_C(0x80000000));
    for(unsigned i = 0; i < 192u; ++i) heartbeat();
    CHECK(V.window_passes == 2u && V.window_changed[0] == UINT32_C(1) << 5 && V.window_changed[1] == UINT32_C(1) << 8);
    CHECK(!V.window_nonzero);
    /* Back as it was in the first pass: still marked as having changed. */
    put32(0x1f4000u + 5u * 1024u + 512u, 0u);
    for(unsigned i = 0; i < 192u; ++i) heartbeat();
    CHECK(V.window_changed[0] == UINT32_C(1) << 5 && V.window_passes == 3u);
    /* A non-zero word in the first pass. */
    fresh();
    driver(UINT32_C(0x02665a01), 0u, "");
    put32(0xe0u, 0xaacu);
    put32(0xaa8u, UINT32_C(0x00800000));
    put32(0x1fff00u, 5u);
    for(unsigned i = 0; i < 192u; ++i) heartbeat();
    CHECK(V.window_passes == 1u && V.window_nonzero && !V.window_changed[0] && !V.window_changed[1]);
    for(unsigned i = 0; i < 192u; ++i) heartbeat();
    CHECK(V.window_passes == 2u && !V.window_changed[0] && !V.window_changed[1]);
    /* Cut short in the middle of a chunk: nothing of it counted, the chunk read
     * again at the next survey, and no change seen where there was none. */
    const uint32_t offset = V.window_offset, timeouts = V.timeouts;
    fifo_after = sound_reads + 10u + 16u;
    heartbeat();
    CHECK(V.timeouts == timeouts + 1u && V.window_offset == offset);
    fifo_after = 0u;
    for(unsigned i = 0; i < 192u; ++i) heartbeat();
    CHECK(V.window_passes == 3u && !V.window_changed[0] && !V.window_changed[1]);
    ++scenarios;
}

/* SR.BL set: nothing is read but the clock and SR is not written. A G2 DMA
 * channel enabled or running: nothing of the chip is read. */
static void refusals(void) {
    fresh();
    driver(UINT32_C(0x00090002), EXEC, "1999.06.16");
    put32(0x12060u, 0x00080830u);
    sr |= BL;
    const unsigned long writes = sr_writes;
    for(unsigned i = 0; i < 7u; ++i) CHECK(!heartbeat());
    CHECK(V.skipped_bl == 7u && sr_writes == writes && !V.identified);
    sr &= ~BL;
    for(unsigned c = 0; c < 4u; ++c)
        for(unsigned which = 0; which < 2u; ++which) {
            (which ? dma_start : dma_enable)[c] = 1u;
            CHECK(!heartbeat());
            (which ? dma_start : dma_enable)[c] = 0u;
        }
    CHECK(V.skipped_dma == 8u && !V.identified && !V.proved);
    dma_enable[2] = 2u; /* not the enable bit */
    CHECK(heartbeat() && V.proved == 1u);
    ++scenarios;
}

/* The FIFO that does not empty, and a word that never reads the same twice:
 * the survey stops within its 10,000 polls and 2 ms, and says so. A word torn
 * a few times is read when two reads agree. */
static void budgets(void) {
    fresh();
    driver(UINT32_C(0x00090002), EXEC, "1999.06.16");
    put32(0x12060u, 0x00080830u);
    tick_per_read = 0u; /* a stopped clock: the polls bound it */
    fifo_busy = UINT32_MAX;
    unsigned long polls = fifo_polls;
    tcnt -= SPACING;
    kui_toy_cdda_survey_heartbeat();
    CHECK(fifo_polls - polls == 10000u && V.timeouts == 1u && !V.identified);
    tick_per_read = 2u; /* 2 ms of reads: the clock bounds it */
    polls = fifo_polls;
    heartbeat();
    CHECK(fifo_polls - polls < 1563u && V.timeouts == 2u);
    fifo_busy = 0u;
    torn_address = SOUND + 0x20u; torn_left = 5u;
    heartbeat();
    CHECK(V.version == UINT32_C(0x00090002) && V.proved == 1u && V.timeouts == 2u);
    torn_address = SOUND + 0x13980u; torn_left = UINT32_MAX;
    heartbeat();
    CHECK(V.timeouts == 3u && V.proved == 1u && !V.unproved);
    torn_left = 0u;
    heartbeat();
    CHECK(V.proved == 2u);
    ++scenarios;
}

int main(void) {
    without_play();
    spacing();
    manatee_2();
    manatee_1();
    namihei();
    audio64();
    others();
    identity_change();
    window();
    refusals();
    budgets();
    printf("PASS Test 84 sound chip survey: %u scenarios, %lu checks; %lu reads of the chip, none outside the "
           "admitted addresses, none unmasked, none with G2 DMA on, none more than eight after the FIFO was empty; "
           "the survey has no writer\n", scenarios, checks, sound_reads);
    return 0;
}
