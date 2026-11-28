/* SPDX-License-Identifier: GPL-3.0-only */
/* Test 88: the CD-music output (src/loader/toy_cdda_output.c), unmodified,
 * over sound memory as an original MANATEE-lineage driver leaves it: the
 * image loaded at 0 and its own start-up run on an ARM7 model
 * (tests/test_toy_cdda_output_drivers.py, tools/manatee_read.py). Nothing of
 * the driver's state is authored here.
 *
 * usage: test-toy-cdda-output-drivers COUNT_AT SLOW SPARE CHANNEL0 CHANNEL1 < MEMORY
 *
 * MEMORY: the first 0x18000 bytes of sound memory after the start-up, on
 * stdin; they are never written anywhere. The other arguments are what the
 * caller found in the image by itself, none of it this file's or the
 * output's: the word the image's timer interrupt counts, whether it counts
 * 176 frames (1) or 176.4 (0), the first address above the highest ring the
 * image indexes, and the channels of the family's two records.
 *
 * The model: TMU0 and the sound chip on one clock; the count's word moves
 * as that image's interrupt moves it; the drive plays one long track and the
 * card always has its audio. A store may go only to [SPARE, 0x18000) and to
 * registers +0x00..+0x44 of the two channels. After every heartbeat of a
 * ring that runs, the frames the output takes the chip to have played are
 * held against the frames the chip has played: a count more or less.
 * One line of the output's own account on stdout; exit 1 with the reason
 * where the model was not kept to. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kui/toy_cdda_card.h"
#include "kui/toy_cdda_output.h"
#include "kui/toy_cdda_player.h"
#include "kui/toy_cdda_survey.h"

struct kui_toy_cdda_drive kui_toy_cdda_drive;
#define D kui_toy_cdda_drive
#define O kui_toy_cdda_output
#define SOUND UINT32_C(0xa0800000)
#define CHANNELS UINT32_C(0xa0700000)
#define KEPT 0x18000u
#define REST UINT32_C(0x40000000)
#define TCNT_START UINT32_C(0xf0000000)

static uint8_t ram[0x200000];
static uint32_t reg[64][18], sr = REST;
static uint32_t count_at, count_base, spare, pair[2];
static bool slow;
static uint64_t now, keyed_at;
static bool keyed[2];
static unsigned long ring_stores, register_stores, key_ons;

static void fail(const char *why) {
    fprintf(stderr, "test_toy_cdda_output_drivers: %s\n", why);
    exit(1);
}
static uint64_t chip_frames(uint64_t ticks) { return ticks * 882u / 15625u; }
static uint32_t count_now(void) {
    const uint64_t frames = chip_frames(now);
    return count_base + (uint32_t)(slow ? frames / 176u : frames * 5u / 882u);
}
uint32_t kui_vmu_native_passive_stack_room(void) { return 2048u; }
void kui_toy_cdda_consumer_heartbeat(void);
uint32_t kui_toy_cdda_output_test_sr(void) { return sr; }
void kui_toy_cdda_output_test_set_sr(uint32_t value) { sr = value; }
void kui_toy_cdda_output_test_even(const void *address) {
    if((uintptr_t)address & 1u)fail("samples read at an odd address");
}
uint32_t kui_toy_cdda_output_test_read(uint32_t address, unsigned width) {
    switch(address) {
    case UINT32_C(0xffc00000): return 0x0e0au;
    case UINT32_C(0xffd80004): return 1u;
    case UINT32_C(0xffd80008): return UINT32_MAX;
    case UINT32_C(0xffd80010): return 2u;
    case UINT32_C(0xffd8000c): return TCNT_START - (uint32_t)now;
    case UINT32_C(0xa05f688c): ++now; return 0u;
    case UINT32_C(0xa0702c00): return 0u;
    case UINT32_C(0xa0702040): return 0x0f0fu;
    case UINT32_C(0xa0702044): return 0x0f1fu;
    /* The chip's timers B and C, which the output only reads (Test 89). */
    case UINT32_C(0xa0702894): case UINT32_C(0xa0702898): return 0u;
    default: break;
    }
    if(address - UINT32_C(0xa05f7800) < 0x80u)return 0u;
    if(width != 4u || (address & 3u) || (sr & 0xf0u) != 0xf0u)fail("a read the output may not make");
    ++now;
    if(address == SOUND + count_at)return count_now();
    if(address - SOUND < sizeof(ram)) {
        uint32_t value;
        memcpy(&value, ram + (address - SOUND), 4u);
        return value;
    }
    const uint32_t c = (address - CHANNELS) / 0x80u, at = (address - CHANNELS) % 0x80u;
    if(address - CHANNELS >= 64u * 0x80u || at > 0x44u || (c != pair[0] && c != pair[1]))fail("a register of another channel read");
    return reg[c][at / 4u];
}
void kui_toy_cdda_output_test_write(uint32_t address, uint32_t value) {
    if((address & 3u) || (sr & 0xf0u) != 0xf0u)fail("a store the output may not make");
    ++now;
    if(address - SOUND < sizeof(ram)) {
        const uint32_t at = address - SOUND;
        if(at < spare || at >= KEPT)fail("a store outside the image's spare memory");
        memcpy(ram + at, &value, 4u);
        ++ring_stores;
        return;
    }
    const uint32_t c = (address - CHANNELS) / 0x80u, at = (address - CHANNELS) % 0x80u;
    if(address - CHANNELS >= 64u * 0x80u || at > 0x44u || (c != pair[0] && c != pair[1]))fail("a store to another channel's registers");
    reg[c][at / 4u] = value & (at ? 0xffffu : 0x7fffu);
    ++register_stores;
    if(!at && (value & 0x8000u))
        for(unsigned side = 0; side < 2u; ++side) {
            const bool on = (reg[pair[side]][0] & 0x4000u) != 0u;
            if(on && !keyed[side]) {
                keyed_at = now;
                ++key_ons;
            }
            keyed[side] = on;
        }
}
/* The player's hold (Test 89), as src/loader/toy_cdda_player.c keeps it
 * for a ring that is primed within its second, as this model's is. */
void kui_toy_cdda_player_hold(uint32_t hold) {
    hold = hold == KUI_TOY_CDDA_HOLD;
    if(D.state != KUI_TOY_CDDA_PLAYING || !D.hold == !hold)return;
    if(hold) {
        D.held = D.base_counter - (TCNT_START - (uint32_t)now);
        D.hold = 1u;
    } else {
        D.hold = 0u;
        D.base_counter = TCNT_START - (uint32_t)now + D.held;
    }
}
/* One track that does not end within the test, and a card that has it. */
uint32_t kui_toy_cdda_track_of(uint32_t lba, uint32_t *start, uint32_t *end) {
    if(lba < 1000u || lba >= 500000u)return 0u;
    *start = 1000u;
    *end = 500000u;
    return 2u;
}
static uint32_t card_generation, card_offset;
uint32_t kui_toy_cdda_card_heartbeat(const struct kui_toy_cdda_want *want, uint32_t room,
    kui_toy_cdda_take take, void *context) {
    static uint8_t bytes[1024];
    if(!want->generation) {
        card_generation = 0u;
        return KUI_TOY_CDDA_CARD_IDLE;
    }
    if(want->generation != card_generation) {
        card_generation = want->generation;
        card_offset = 0u;
    }
    for(uint32_t give = room / 512u * 512u > 8192u ? 8192u : room / 512u * 512u; give;) {
        const uint32_t count = give < sizeof(bytes) ? give : (uint32_t)sizeof(bytes);
        for(uint32_t i = 0; i < count; ++i)bytes[i] = (uint8_t)((card_offset + i) * 37u >> 3);
        now += 40u;
        take(context, bytes, count, card_offset);
        card_offset += count;
        give -= count;
    }
    return KUI_TOY_CDDA_CARD_STREAMING;
}

int main(int argc, char **argv) {
    if(argc != 6)fail("usage: COUNT_AT SLOW SPARE CHANNEL0 CHANNEL1 < MEMORY");
    count_at = (uint32_t)strtoul(argv[1], 0, 0);
    slow = strtoul(argv[2], 0, 0) != 0u;
    spare = (uint32_t)strtoul(argv[3], 0, 0);
    pair[0] = (uint32_t)strtoul(argv[4], 0, 0);
    pair[1] = (uint32_t)strtoul(argv[5], 0, 0);
    memset(ram, 0x5a, sizeof(ram));
    if(fread(ram, 1, KEPT, stdin) != KEPT)fail("sound memory after the start-up: 0x18000 bytes on stdin");
    memcpy(&count_base, ram + count_at, 4u);
    D.first = 1000u;
    D.end = 500000u;
    D.passes = 0u;
    D.base_counter = TCNT_START;
    D.state = KUI_TOY_CDDA_PLAYING;
    D.generation = 1u;
    uint32_t worst = 0u;
    unsigned long looks = 0u;
    /* Two minutes of a game's heartbeats, a frame of video apart. */
    for(uint64_t until = 120u * UINT64_C(781250); now < until;) {
        now += 13021u;
        const uint32_t was = O.phase;
        kui_toy_cdda_consumer_heartbeat();
        if(sr != REST)fail("SR not given back");
        if(was != KUI_TOY_CDDA_OUT_RUN || O.phase != KUI_TOY_CDDA_OUT_RUN || !keyed[0] || !keyed[1])continue;
        /* What the chip had played when the output last looked. */
        const uint64_t played = chip_frames(TCNT_START - O.played_tick) - chip_frames(keyed_at);
        const uint32_t apart = (uint32_t)(played > O.played ? played - O.played : O.played - played);
        if(apart > worst)worst = apart;
        ++looks;
    }
    printf("family=%u version=%08x hash=%04x channels=%u,%u ring=%05x frames=%u spare=%u phase=%u why=%u detail=%u "
           "keys=%u key_ons=%lu restarts=%u underruns=%u clock_faults=%u takes=%u faults=%u timeouts=%u "
           "ring_stores=%lu looks=%lu worst=%u\n", O.family, O.version, O.hash, O.channel[0], O.channel[1], O.ring, O.frames,
           O.spare, O.phase, O.why, O.why_detail, O.keys, key_ons, O.restarts, O.underruns, O.clock_faults, O.takes,
           O.faults, O.timeouts, ring_stores, looks, worst);
    return 0;
}
