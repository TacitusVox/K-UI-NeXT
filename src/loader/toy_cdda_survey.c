/* SPDX-License-Identifier: GPL-3.0-only */
/* Test 84 (SURVEY): while the drive plays, the worker's heartbeat looks at the
 * sound chip (docs/cdda-background-test-84.md). It only reads: no store of
 * this file reaches the chip, its memory or the bus. What it learns is what
 * Test 85 needs before it may write there: which sound driver runs, whether
 * the game uses the two channels Test 85 would take, and, under AUDIO64,
 * whether the top 48 KiB of sound memory are in use.
 *
 * Called after the heartbeat's own work, at most once a sixtieth of a second
 * and only while the drive plays (src/loader/vmu_native_passive.c). The bus is
 * used as the console-proven pilot used it (src/loader/toy_pilot_bus.c):
 * TMU0 in the Toy profile, read again at every heartbeat (the pilot checked
 * it before every bus visit; the player only at the game's GD calls);
 * nothing with SR.BL set; nothing while any G2 DMA channel is enabled or
 * running; the interrupt mask at 15 throughout; the G2 FIFO empty before
 * every burst of at most eight reads; a word the sound CPU may be writing
 * read until two reads agree; and the whole survey within 2 ms of TMU0 and
 * 10,000 polls. A survey that runs out of either stops where it is.
 *
 * What is read, all in sound memory (0xa0800000 + the driver's address)
 * but the sound CPU's reset bit (0xa0702c00 bit 0):
 * - the driver's identity, its version bytes (0x20) and its build date's
 *   text (0x50..0x5B), at every survey: drivers of different builds share
 *   version bytes (AUDIO64 and its MIDIDA hybrid, two MANATEE pairs); and its
 *   state word (0xF8), MANATEE's running mark ('EXEC', 'SEGA') and nothing
 *   of the identity: AICADRV keeps a live voice count there;
 * - its family's two candidate channels' records: MANATEE 2 the top two
 *   stream ports (count at 0x12062, records 0x13800 + 0x40 x port, bit 0
 *   playing), MANATEE 1 the channel pool's entries 13 and 14 from 1.01.37 on
 *   and 14 and 15 before (0x12080 + 12 x entry: owner +4, proved by its
 *   register base +8), NAMIHEI stream ports 6 and 7 (0x11200 + 0x40 x port,
 *   bit 0), AUDIO64 the playing bytes of channels 62 and 63 (table at header
 *   word 0xE0, proved by the ports' base word before it; host ports 14 and
 *   15 in the hybrids);
 * - under AUDIO64 the window 0x1F4000..0x1FFFFF, 256 bytes a survey.
 * The driver facts are static readings of the drivers (docs/cd-audio-corpus.md
 * and the notes it names), not console results. */
#include <stdbool.h>
#include <stdint.h>
#include "kui/toy_cdda_player.h"
#include "kui/toy_cdda_survey.h"
#include "kui/toy_pilot_clock.h"

#if !KUI_TOY_CDDA_SURVEY
#error "The sound chip survey is Test 84's (KUI_TOY_CDDA_SURVEY)"
#endif

struct kui_toy_cdda_survey kui_toy_cdda_survey;
#define V kui_toy_cdda_survey

#define SOUND UINT32_C(0xa0800000)
#define ARM_RESET UINT32_C(0xa0702c00)
#define FIFO UINT32_C(0xa05f688c)
#define FIFO_MASK 0x31u
#define G2_DMA UINT32_C(0xa05f7800)
#define FRQCR UINT32_C(0xffc00000)
#define TSTR UINT32_C(0xffd80004)
#define TCOR0 UINT32_C(0xffd80008)
#define TCNT0 UINT32_C(0xffd8000c)
#define TCR0 UINT32_C(0xffd80010)
#define SR_BL UINT32_C(0x10000000)
#define POLL_LIMIT 10000u
#define TICK_LIMIT 1563u          /* 2 ms of TMU0 at 781,250 Hz */
#define SPACING 13021u            /* a sixtieth of a second */
#define WINDOW UINT32_C(0x1f4000)
#define WINDOW_WORDS 64u          /* 256 bytes a survey */
#define BURST 8u
#define EXEC UINT32_C(0x43455845) /* 'EXEC' */
#define SEGA UINT32_C(0x41474553) /* 'SEGA' */

#ifdef KUI_TOY_CDDA_SURVEY_HOST_TEST
extern uint32_t kui_toy_cdda_survey_test_read(uint32_t address, unsigned width);
extern uint32_t kui_toy_cdda_survey_test_sr(void);
extern void kui_toy_cdda_survey_test_set_sr(uint32_t value);
#define read32(a) kui_toy_cdda_survey_test_read((a), 4u)
#define read16(a) kui_toy_cdda_survey_test_read((a), 2u)
#define read8(a) kui_toy_cdda_survey_test_read((a), 1u)
#define sr_read kui_toy_cdda_survey_test_sr
#define sr_write kui_toy_cdda_survey_test_set_sr
#else
static uint32_t read32(uint32_t address) {
    return *(volatile const uint32_t *)(uintptr_t)address;
}
static uint32_t read16(uint32_t address) {
    return *(volatile const uint16_t *)(uintptr_t)address;
}
static uint32_t read8(uint32_t address) {
    return *(volatile const uint8_t *)(uintptr_t)address;
}
static uint32_t sr_read(void) {
    uint32_t value;
    __asm__ __volatile__("stc sr,%0" : "=r"(value) : : "memory");
    return value;
}
static void sr_write(uint32_t value) {
    __asm__ __volatile__("ldc %0,sr" : : "r"(value) : "memory", "t");
}
#endif

/* One survey's use of the bus: its start (TMU0 counts down: ~TCNT0 grows),
 * its polls, and whether it may go on. */
struct bus { uint32_t start, polls; bool ok; };
static bool budget(struct bus *b) {
    if(b->ok && (b->polls >= POLL_LIMIT || ~read32(TCNT0) - b->start >= TICK_LIMIT))b->ok=false;
    return b->ok;
}
static bool drain(struct bus *b) {
    while(budget(b)) {
        ++b->polls;
        if(!(read32(FIFO) & FIFO_MASK))return true;
    }
    return false;
}
/* A word the sound CPU may be writing: two reads that agree. */
static bool stable(struct bus *b, uint32_t address, uint32_t *value) {
    while(drain(b)) {
        const uint32_t first = read32(address), second = read32(address);
        if(first == second) {
            *value = first;
            return true;
        }
    }
    return false;
}
static bool sound(struct bus *b, uint32_t offset, uint32_t *value) {
    return stable(b, SOUND + offset, value);
}

/* MANATEE 2 and NAMIHEI write their running mark when they run; MANATEE 1's
 * pool proves itself by its register bases (candidates), and its first build
 * (1.01.1d) writes no mark at all. */
static uint32_t family_of(uint32_t version, uint32_t state) {
    const uint32_t major = version & 255u, minor = version >> 8 & 255u, build = version >> 16 & 255u;
    if(major == 2u)return state == EXEC ? KUI_TOY_CDDA_FAMILY_GEN2 : KUI_TOY_CDDA_FAMILY_UNKNOWN;
    if(major != 1u)return KUI_TOY_CDDA_FAMILY_UNKNOWN;
    if(minor == 0x50u || minor == 0x5au)return KUI_TOY_CDDA_FAMILY_AUDIO64;
    if(minor == 1u)return build >= 0x37u ? KUI_TOY_CDDA_FAMILY_GEN1 : KUI_TOY_CDDA_FAMILY_GEN1_OLD;
    if(!minor && state == SEGA)return KUI_TOY_CDDA_FAMILY_NAMIHEI;
    return KUI_TOY_CDDA_FAMILY_UNKNOWN;
}
static bool manatee(uint32_t family) {
    return family >= KUI_TOY_CDDA_FAMILY_GEN2 && family <= KUI_TOY_CDDA_FAMILY_NAMIHEI;
}

/* Whether the family's two candidate channels are in use by the game, from
 * the driver's own records: 1 read, 0 a family without candidates, -1 not
 * read (the bus) or records that do not prove their layout. AUDIO64 learns
 * here whether it is a hybrid. */
static int candidates(struct bus *b, uint32_t *family, bool busy[2]) {
    uint32_t word, other;
    switch(*family) {
    case KUI_TOY_CDDA_FAMILY_GEN2: {
        /* The split bytes 0x12060..62: MIDI voices, one-shot ports, stream
         * ports, 64 channels in all; the stream ports are the top channels. */
        if(!sound(b, 0x12060u, &word))return -1;
        const uint32_t streams = word >> 16 & 255u;
        if(streams > 16u || (word & 255u) + (word >> 8 & 255u) + streams != 64u)return -1;
        /* A split the driver takes with fewer than two stream ports: no
         * stream channel to take. */
        if(streams < 2u)return 0;
        for(unsigned k = 0; k < 2u; ++k) {
            if(!sound(b, 0x13800u + 0x40u * (streams - 2u + k), &word))return -1;
            busy[k] = word & 1u;
        }
        return 1;
    }
    case KUI_TOY_CDDA_FAMILY_GEN1:
    case KUI_TOY_CDDA_FAMILY_GEN1_OLD:
        for(unsigned k = 0; k < 2u; ++k) {
            const uint32_t entry = (*family == KUI_TOY_CDDA_FAMILY_GEN1 ? 13u : 14u) + k;
            const uint32_t at = 0x12080u + 12u * entry;
            if(!sound(b, at + 8u, &word) || word != UINT32_C(0x00801800) + 0x80u * entry ||
               !sound(b, at + 4u, &other))return -1;
            busy[k] = other != 0u;
        }
        return 1;
    case KUI_TOY_CDDA_FAMILY_NAMIHEI:
        for(unsigned k = 0; k < 2u; ++k) {
            if(!sound(b, 0x11200u + 0x40u * (6u + k), &word))return -1;
            busy[k] = word & 1u;
        }
        return 1;
    case KUI_TOY_CDDA_FAMILY_AUDIO64: {
        if(!sound(b, 0xe0u, &word) || (word & 3u) || word < 0x104u || word >= 0x10000u ||
           !sound(b, word - 4u, &other))return -1;
        unsigned port;
        if(other == UINT32_C(0x00800000))port = 62u;
        else if(other == UINT32_C(0x00801800)) {
            port = 14u;
            *family = KUI_TOY_CDDA_FAMILY_HYBRID;
        } else return -1;
        /* The two ports' bytes are the upper half of one aligned word. */
        if(!sound(b, word + port - 2u, &other))return -1;
        busy[0] = (other >> 16 & 255u) != 0u;
        busy[1] = (other >> 24) != 0u;
        return 1;
    }
    default:
        return 0;
    }
}

/* 256 bytes of the window, single reads in bursts. A KiB's hash is taken over
 * four surveys and kept from the first pass; a later pass marks the KiB whose
 * hash differs. A chunk the bus cut short is not counted, and read again. */
static void window(struct bus *b) {
    uint32_t hash = V.window_hash;
    bool nonzero = false;
    for(uint32_t at = 0; at < WINDOW_WORDS; at += BURST) {
        if(!drain(b))return;
        for(uint32_t i = 0; i < BURST; ++i) {
            const uint32_t word = read32(SOUND + WINDOW + V.window_offset + 4u * (at + i));
            nonzero |= word != 0u;
            hash = (hash ^ word) * UINT32_C(0x01000193);
        }
    }
    if(!V.window_passes && nonzero)V.window_nonzero = 1u;
    V.window_hash = hash;
    V.window_offset += WINDOW_WORDS * 4u;
    if(V.window_offset & 1023u)return;
    const uint32_t kib = (V.window_offset >> 10) - 1u;
    const uint16_t folded = (uint16_t)(hash ^ hash >> 16);
    if(!V.window_passes)V.window_first[kib] = folded;
    else if(V.window_first[kib] != folded)V.window_changed[kib >> 5] |= UINT32_C(1) << (kib & 31u);
    V.window_hash = 0u;
    if(kib + 1u == KUI_TOY_CDDA_WINDOW_KIB) {
        V.window_offset = 0u;
        ++V.window_passes;
    }
}

static void survey(struct bus *b) {
    uint32_t reset, version, state, date, hash = UINT32_C(0x811c9dc5);
    if(!stable(b, ARM_RESET, &reset))return;
    if(reset & 1u) {
        V.family = KUI_TOY_CDDA_FAMILY_RESET;
        return;
    }
    if(!sound(b, 0x20u, &version))return;
    for(uint32_t at = 0x50u; at < 0x5cu; at += 4u) {
        if(!sound(b, at, &date))return;
        for(unsigned i = 0; i < 4u; ++i)hash = (hash ^ (date >> 8 * i & 255u)) * UINT32_C(0x01000193);
    }
    hash = (hash ^ hash >> 16) & 0xffffu;
    if(!sound(b, 0xf8u, &state))return;
    if(V.identified && (version != V.version || hash != V.date_hash))++V.changes;
    V.version = version;
    V.date_hash = hash;
    V.identified = 1u;
    uint32_t family = family_of(version, state);
    V.state_word = manatee(family) ? state : 0u;
    bool busy[2] = {false, false};
    const int read = candidates(b, &family, busy);
    /* Cut short: the family of the last survey that finished stands. */
    if(!b->ok)return;
    V.family = family;
    if(read < 0) {
        ++V.unproved;
        return;
    }
    if(read) {
        ++V.proved;
        V.busy[0] += busy[0];
        V.busy[1] += busy[1];
    }
    if(family == KUI_TOY_CDDA_FAMILY_AUDIO64 || family == KUI_TOY_CDDA_FAMILY_HYBRID)window(b);
}

void kui_toy_cdda_survey_heartbeat(void) {
    if(kui_toy_cdda_drive.state != KUI_TOY_CDDA_PLAYING)return;
    const uint32_t frqcr = read16(FRQCR), tcor = read32(TCOR0), tcr = read16(TCR0), tstr = read8(TSTR);
    if(!kui_toy_pilot_clock_profile(frqcr, tcor, tcr, tstr)) {
        ++V.skipped_clock;
        return;
    }
    const uint32_t now = read32(TCNT0);
    if(V.started && V.last_tick - now < SPACING)return;
    V.started = 1u;
    V.last_tick = now;
    const uint32_t sr = sr_read();
    if(sr & SR_BL) {
        ++V.skipped_bl;
        return;
    }
    sr_write(sr | 0xf0u);
    bool idle = true;
    for(uint32_t channel = 0; channel < 4u; ++channel) {
        const uint32_t base = G2_DMA + 0x20u * channel;
        if((read32(base + 0x14u) | read32(base + 0x18u)) & 1u)idle = false;
    }
    if(!idle)++V.skipped_dma;
    else {
        struct bus b = {~now, 0u, true};
        survey(&b);
        if(!b.ok)++V.timeouts;
    }
    sr_write(sr);
}
