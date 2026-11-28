/* SPDX-License-Identifier: GPL-3.0-only */
/* Tests 85 and 86 (OUTPUT, VOLUME, ADPCM): the CD-music output
 * (src/loader/toy_cdda_output.c) on a model of the sound chip, its bus and
 * the card.
 *
 * The model answers only the addresses the output may read and takes only
 * the stores it may make: the placed ring with its guards, at or above
 * 0x18000 and clear of every pair of the bank table as the table stands at
 * that store, and registers +0x00..+0x44 of the scripted driver's two
 * channels while the driver's records call them free. Every access of the
 * chip must come with the interrupt mask at 15, SR.BL clear, every G2 DMA
 * channel off, the sound CPU out of reset for a store, and at most eight
 * accesses since the FIFO was last seen empty.
 *
 * The model's chip plays: an execute strobe keys on the channels whose key
 * bit is set and that are off, and keys off those whose bit is clear. A
 * channel keeps its own position from its key-on: it steps a frame at a
 * time and goes back to its loop's start when it reaches the loop end
 * register as that register stands then (one scenario has it play the
 * frame there first), so a register changed under a play moves the play
 * for good. The two channels step on the chip's own clock, a set number of
 * parts in a million from TMU0. Every frame the pair plays is taken from
 * sound memory at the moment it is played and held against the play's audio
 * as the test reckons it by itself: the drive's position at the heartbeat
 * that began the ring, the disc's tracks and gaps, the card's pattern.
 *
 * With ADPCM the chip decodes: each channel's decoder begins at its key-on
 * from signal 0 and step 127 and takes one 4-bit code a frame, the low four
 * bits of a byte first, without beginning again at the loop (the format
 * with PCMS 3). A code is held against the one the test's own coder gives
 * the play's frame from the play's first frame on, and what the chip
 * decodes against the play's audio itself: it may be far from it only for
 * a few frames in a hundred.
 *
 * With SPARE the ring lies in the driver's spare memory and nowhere else:
 * 0x16000..0x17FFF under MANATEE 2, 0x15000..0x17FFF under MANATEE 1, the
 * whole of it.
 *
 * The drivers' headers and records are authored from the static readings
 * (docs/cd-audio-corpus.md): the version bytes and the date's hash are those
 * the original images and the console gave (docs/evidence/
 * cdda-background-test-84/driver-check.log); the date's text is not the
 * image's but one found here with the same hash. Host model only; nothing
 * here ran on a console. */
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
static unsigned long checks;
#define CHECK(x) do { ++checks; if(!(x)) { \
    fprintf(stderr, "test_toy_cdda_output.c:%d: %s\n", __LINE__, #x); exit(1); } } while(0)

#define SOUND UINT32_C(0xa0800000)
#define CHANNELS UINT32_C(0xa0700000)
#define BYTES 0x200000u
#define BL UINT32_C(0x10000000)
#define EXEC UINT32_C(0x43455845)
#define SEGA UINT32_C(0x41474553)
#define GEN2 KUI_TOY_CDDA_FAMILY_GEN2
#define GEN1 KUI_TOY_CDDA_FAMILY_GEN1
#define OLD KUI_TOY_CDDA_FAMILY_GEN1_OLD
#define NAMIHEI KUI_TOY_CDDA_FAMILY_NAMIHEI
#define REST UINT32_C(0x40000000) /* SR between heartbeats: the game's, mask 0 */
#define TCNT_START UINT32_C(0xf0000000)
#define SECOND 781250u
#ifndef KUI_TOY_CDDA_ADPCM
#define KUI_TOY_CDDA_ADPCM 0
#endif
#ifndef KUI_TOY_CDDA_SPARE
#define KUI_TOY_CDDA_SPARE 0
#endif
/* The driver's spare memory and the ring it holds (SPARE): from the end of
 * the family's work areas to 0x17FFF, two guard lines and two planes. */
#define SPARE_AT (family == GEN2 ? 0x16000u : 0x15000u)
#define SPARE_FRAMES (0x18000u - SPARE_AT - 64u)
/* The ring as the test reckons it: a plane's bytes, what follows a plane,
 * the format bits of a channel's first register, the sizes in their order. */
#if KUI_TOY_CDDA_ADPCM
#define PLANE(frames) ((frames) / 2u)
#define REPEAT 0u
#define FORMAT 0x0180u
static const uint32_t sizes[3] = {65472u, 32768u, 16384u};
#else
#define PLANE(frames) (2u * (frames))
#define REPEAT 4u
#define FORMAT 0u
static const uint32_t sizes[3] = {16384u, 8192u, 4096u};
#endif
#define RING(frames) (64u + 2u * (PLANE(frames) + REPEAT))   /* guards, two planes, their repeats */
#define LARGEST (sizes[0])

static uint8_t ram[BYTES];
static uint32_t reg[64][18], mixer[2], sr = REST, arm_reset, dma_enable[4], dma_start[4];
static uint32_t frqcr = 0x0e0au, tcor = UINT32_MAX, tcr = 2u, tstr = 1u;
static uint64_t now;                 /* TMU0 ticks since the model began */
static uint32_t access_ticks = 1u;   /* what one access of the bus costs */
/* When the stretch of bus work that is running began: a heartbeat's start,
 * or the card handing bytes on. No access comes later than 3 ms of TMU0 and
 * a burst behind it. */
static uint64_t stretch_began;
static bool stretch_live;
static uint32_t store_ticks;         /* what a store to sound memory costs on top */
static bool fifo_stuck;
static unsigned since_empty = 8u;
static unsigned long stores, ring_stores, register_stores, chip_reads, g2_reads, fifo_polls;
static uint64_t timer_stopped_at;    /* TMU0 stands still from here while TSTR's bit is clear */
static bool frozen;                  /* no store may be made at all */
static unsigned family, pair[2];
static bool mute_readback;           /* the CD mixers read nothing */

static uint32_t tcnt(void) { return TCNT_START - (uint32_t)(tstr & 1u ? now : timer_stopped_at); }
static bool masked(void) { return (sr & 0xf0u) == 0xf0u && !(sr & BL); }
static bool dma_idle(void) {
    for(unsigned c = 0; c < 4u; ++c)if((dma_enable[c] | dma_start[c]) & 1u)return false;
    return true;
}
static void put32(uint32_t at, uint32_t value) { memcpy(ram + at, &value, 4u); }
static uint32_t get32(uint32_t at) { uint32_t value; memcpy(&value, ram + at, 4u); return value; }

/* ---- the disc and the card ---- */
static const struct { uint32_t number, start, end; } toc[] = {
    {2u, 1000u, 4000u}, {3u, 4151u, 7000u}, {4u, 7000u, 9000u}};
static uint32_t refuse_track;        /* a track the card does not hold */
uint32_t kui_toy_cdda_track_of(uint32_t lba, uint32_t *start, uint32_t *end) {
    for(unsigned i = 0; i < sizeof(toc) / sizeof(toc[0]); ++i)
        if(lba >= toc[i].start && lba < toc[i].end) {
            *start = toc[i].start;
            *end = toc[i].end;
            return toc[i].number;
        }
    return 0u;
}
/* The disc's audio: frame i of sector lba. */
static void pattern(uint32_t lba, uint32_t i, int16_t *l, int16_t *r) {
    const uint32_t g = lba * 588u + i;
    *l = (int16_t)(uint16_t)(g * 7u + 3u);
    *r = (int16_t)(uint16_t)(g * 13u + 5u);
}
static struct { uint32_t generation, track, lba, end, offset; } card;
static uint32_t card_most = 8192u, card_chunk = 1024u, card_ticks = 40u;
static bool card_idle;               /* the card is the game's: nothing is handed on */
static bool card_skips;              /* once, the card leaves a frame out and says so by its offset */
static unsigned card_calls_now;
static unsigned long card_calls, card_nones, card_segments, card_bytes, card_low;
uint32_t kui_toy_cdda_card_heartbeat(const struct kui_toy_cdda_want *want, uint32_t room,
    kui_toy_cdda_take take, void *context) {
    CHECK(masked());
    CHECK(++card_calls_now == 1u);
    ++card_calls;
    if(!want->generation) {
        ++card_nones;
        card.generation = 0u;
        return KUI_TOY_CDDA_CARD_IDLE;
    }
    uint32_t start, end;
    CHECK(D.state == KUI_TOY_CDDA_PLAYING);       /* audio is asked for only while the drive plays */
    CHECK(want->track && kui_toy_cdda_track_of(want->lba, &start, &end) == want->track);
    CHECK(want->lba < want->end && want->end <= end && want->end <= D.end && want->lba >= D.first);
    if(want->generation != card.generation) {
        card.generation = want->generation;
        card.track = want->track;
        card.lba = want->lba;
        card.end = want->end;
        card.offset = 0u;
        ++card_segments;
    } else CHECK(want->track == card.track && want->lba == card.lba && want->end == card.end);
    card_low += want->yield_ok;
    if(want->track == refuse_track)return KUI_TOY_CDDA_CARD_REFUSED;
    if(card_idle)return KUI_TOY_CDDA_CARD_IDLE;
    /* The card hands on what whole card blocks hold of the segment, and
     * none without room for a block (src/loader/toy_cdda_card.c). */
    const uint32_t total = (card.end - card.lba) * 2352u;
    uint32_t give = total - card.offset;
    if(give > room / 512u * 512u)give = room / 512u * 512u;
    if(give > card_most)give = card_most;
    if(card_skips && card.offset + 4u < total) {
        card.offset += 4u;
        card_skips = false;
    }
    while(give) {
        uint8_t bytes[2048];
        const uint32_t count = give < card_chunk ? give : card_chunk;
        for(uint32_t i = 0; i < count; ++i) {
            const uint32_t at = card.offset + i;
            int16_t l, r;
            pattern(card.lba + at / 2352u, at % 2352u / 4u, &l, &r);
            const uint16_t sample = (uint16_t)(at & 2u ? r : l);
            bytes[i] = (uint8_t)(at & 1u ? sample >> 8 : sample);
        }
        now += card_ticks;
        stretch_began = now;
        take(context, bytes, count, card.offset);
        card.offset += count;
        card_bytes += count;
        give -= count;
    }
    return card.offset == total ? KUI_TOY_CDDA_CARD_DONE : KUI_TOY_CDDA_CARD_STREAMING;
}

/* ---- the play as the test reckons it ---- */
static struct { uint32_t first, end, base, passes; } heard_play;
/* Where the drive stands at TMU0 count tc; false: its last pass is over. */
static bool oracle(uint32_t tc, uint32_t *sector, uint32_t *passes) {
    const uint64_t ticks = (uint32_t)(D.base_counter - tc), length = D.end - D.first;
    int64_t offset = D.base_offset + (int64_t)(ticks * 3u / 31250u);
    if(offset < 0)offset = 0;
    const uint64_t laps = (uint64_t)offset / length;
    if(D.passes && D.done + laps >= D.passes)return false;
    *sector = D.first + (uint32_t)((uint64_t)offset % length);
    *passes = D.passes ? (uint32_t)(D.passes - D.done - laps) : 0u;
    return true;
}
/* Frame k of the ring that began at heard_play. */
static void expected(uint64_t k, int16_t *l, int16_t *r) {
    const uint64_t length = heard_play.end - heard_play.first, at = heard_play.base - heard_play.first + k / 588u;
    const uint32_t lba = heard_play.first + (uint32_t)(at % length);
    uint32_t start, end;
    *l = *r = 0;
    if(heard_play.passes && at / length >= heard_play.passes)return;
    const uint32_t track = kui_toy_cdda_track_of(lba, &start, &end);
    if(track && track != refuse_track)pattern(lba, (uint32_t)(k % 588u), l, r);
}

/* ---- the chip ---- */
static struct voice { bool active; uint64_t keyed_at, consumed; uint32_t pos, step; int32_t signal; } voice[64];
/* ADPCM: the play as the test's own coder gives it to a chip, a decoder a
 * side from the pair's key-on; cut: silence has taken the play's place. */
static struct coder { int32_t signal; uint32_t step; } told[2];
#if KUI_TOY_CDDA_ADPCM
/* And the codes the output has written, decoded as they are stored, and
 * where in its plane each side's next word belongs: the test's own account,
 * begun with each ring. */
static struct coder wrote[2];
static uint32_t wrote_at[2];
#endif
static bool cut;
static unsigned long far_frames;
static int ppm;                      /* the chip's clock against TMU0 */
/* The driver's count of 4 ms of the chip's time, as its timer interrupt
 * keeps it; tick_base: what it stood at when the driver last started. */
static uint32_t tick_base;
static uint32_t driver_ticks(void);
/* A count that stands still at count_frozen; a count that runs this many
 * parts in a million off the chip's own time (a driver's never does). */
static bool count_stuck;
static uint32_t count_frozen;
static int count_ppm;
/* The first builds of MANATEE 1: a count every 176 frames, none left out. */
static bool count_slow;
static bool inclusive;               /* the chip plays the frame at its loop end too */
static bool strict = true;           /* every frame played is the play's */
static unsigned long heard_frames, wrong_frames, wrong_loud, wrong_nonzero, cut_frames, loud_frames, key_ons, key_offs, lonely;
static uint64_t last_heard;
/* The frame a channel stands at, and its step on from there. */
#if KUI_TOY_CDDA_ADPCM
static unsigned code_of(unsigned c) {
    const uint32_t address = (reg[c][0] & 0x7fu) << 16 | reg[c][1], pos = voice[c].pos;
    /* (A channel the driver has taken plays what the driver gave it.) */
    CHECK(((reg[c][0] & 0x0180u) == 0x0180u || !strict) && address + pos / 2u < BYTES);
    return ram[address + pos / 2u] >> 4u * (pos & 1u) & 15u;
}
/* The chip's decoder. The accounts of the chip differ on a move of more
 * than 32,767: the play's audio never asks for one. */
static int32_t decode(int32_t *signal, uint32_t *step, unsigned code) {
    static const uint32_t scale[8] = {230u, 230u, 230u, 230u, 307u, 409u, 512u, 614u};
    int32_t move = (int32_t)(*step * (2u * (code & 7u) + 1u) / 8u);
    if(strict)CHECK(move <= 32767);
    if(move > 32767)move = 32767;
    *signal += code & 8u ? -move : move;
    if(*signal > 32767)*signal = 32767;
    if(*signal < -32768)*signal = -32768;
    *step = *step * scale[code & 7u] / 256u;
    if(*step < 127u)*step = 127u;
    if(*step > 24576u)*step = 24576u;
    return *signal;
}
/* The code the test's coder gives a sample where a decoder stands: the
 * largest of the eight sizes within the difference, by the step, its half
 * and its quarter, and none that moves more than 32,767. */
static unsigned code_for(int32_t signal, uint32_t step, int32_t sample) {
    const uint32_t far = (uint32_t)(sample < signal ? signal - sample : sample - signal);
    unsigned size = 0u;
    uint32_t reach = 0u;
    if(far >= step)size = 4u, reach = step;
    if(far - reach >= step / 2u)size += 2u, reach += step / 2u;
    if(far - reach >= step / 4u)size += 1u;
    while(step * (2u * size + 1u) / 8u > 32767u)--size;
    return size | (sample < signal ? 8u : 0u);
}
#else
static int16_t sample_of(unsigned c) {
    const uint32_t address = (reg[c][0] & 0x7fu) << 16 | reg[c][1], pos = voice[c].pos;
    CHECK(address + 2u * pos + 1u < BYTES);
    return (int16_t)(uint16_t)(ram[address + 2u * pos] | ram[address + 2u * pos + 1u] << 8);
}
#endif
static void step_on(unsigned c) {
    if(++voice[c].pos >= reg[c][3] + (inclusive ? 1u : 0u))voice[c].pos = reg[c][2];
}
static void chip_run(void) {
    struct voice *l = &voice[pair[0]], *r = &voice[pair[1]];
    if(!l->active || !r->active) {
        lonely += l->active != r->active;
        return;
    }
    CHECK(l->keyed_at == r->keyed_at);
    const uint64_t frames = (now - l->keyed_at) * 882u * (uint64_t)(1000000 + ppm) / (15625u * UINT64_C(1000000));
    for(; l->consumed < frames; ++l->consumed) {
        const uint64_t k = l->consumed;
        int16_t el, er;
        const bool loud = ((reg[pair[0]][9] | reg[pair[1]][9]) & 0x0f00u) != 0u;
        /* A chip that plays the frame at the loop end hears the plane's
         * first frame twice a lap. */
        expected(inclusive ? k - k / (reg[pair[0]][3] + 1u) : k, &el, &er);
#if KUI_TOY_CDDA_ADPCM
        /* The codes the chip takes; the codes of the play's frame; the
         * codes of silence where the chip's decoders stand; the codes of a
         * ring that was cleared and not written. */
        const unsigned cl = code_of(pair[0]), cr = code_of(pair[1]);
        const unsigned tl = code_for(told[0].signal, told[0].step, el), tr = code_for(told[1].signal, told[1].step, er);
        const bool same = !cut && cl == tl && cr == tr;
        const bool quiet = cl == code_for(l->signal, l->step, 0) && cr == code_for(r->signal, r->step, 0);
        const bool blank = cl == (l->pos & 1u ? 8u : 0u) && cr == (r->pos & 1u ? 8u : 0u);
        (void)decode(&told[0].signal, &told[0].step, tl);
        (void)decode(&told[1].signal, &told[1].step, tr);
        const int32_t sl = decode(&l->signal, &l->step, cl), sr16 = decode(&r->signal, &r->step, cr);
        if(same)far_frames += labs(sl - el) > 2048 || labs(sr16 - er) > 2048;
#else
        const int16_t sl = sample_of(pair[0]), sr16 = sample_of(pair[1]);
        const bool same = sl == el && sr16 == er, quiet = !sl && !sr16, blank = quiet;
#endif
        step_on(pair[0]);
        step_on(pair[1]);
        ++heard_frames;
        loud_frames += loud;
        last_heard = k;
        if(same)continue;
        /* Once the drive's play has ended, silence in the place of what
         * the output had not yet read is the play's end, a little early. */
        if(D.state != KUI_TOY_CDDA_PLAYING && quiet) {
            ++cut_frames;
            cut = true;
            continue;
        }
        ++wrong_frames;
        wrong_loud += loud;
        wrong_nonzero += !blank;
        if(strict) {
            fprintf(stderr, "frame %llu of the ring: played %d %d, the play has %d %d\n",
                (unsigned long long)k, (int)sl, (int)sr16, el, er);
            CHECK(!"a frame that is not the play's");
        }
    }
}
static void strobe(void) {
    for(unsigned c = 0; c < 64u; ++c) {
        const bool on = (reg[c][0] & 0x4000u) != 0u;
        if(on && !voice[c].active) {
            voice[c] = (struct voice){true, now, 0u, 0u, 127u, 0};
            if(c == pair[0] || c == pair[1]) {
                told[0].signal = told[1].signal = 0;
                told[0].step = told[1].step = 127u;
                cut = false;
            }
            ++key_ons;
        } else if(!on && voice[c].active) {
            voice[c].active = false;
            ++key_offs;
        }
    }
}
static uint32_t driver_ticks(void) {
    if(count_stuck)return count_frozen;
    const uint64_t chip = now * (uint64_t)(1000000 + ppm) / UINT64_C(1000000);
    if(count_slow)return (uint32_t)(chip * 882u / (15625u * UINT64_C(176))) - tick_base;
    return (uint32_t)(chip * (uint64_t)(1000000 + count_ppm) / (3125u * UINT64_C(1000000))) - tick_base;
}
static bool record_busy(unsigned side) {
    if(family == GEN2)return ram[0x13800u + 0x40u * (ram[0x12062u] - 2u + side)] & 1u;
    if(family == NAMIHEI)return ram[0x11200u + 0x40u * (6u + side)] & 1u;
    return get32(0x12080u + 12u * (pair[side] - 48u) + 4u) != 0u;
}
static void access(void) {
    CHECK(masked() && dma_idle());
    CHECK(since_empty < 8u);
    if(stretch_live && tstr)CHECK(now - stretch_began <= 2344u + 10u * access_ticks + 8u * store_ticks);
    ++since_empty;
    now += access_ticks;
    chip_run();
}
/* The worker's private stack below the consumer's caller. */
static uint32_t stack_room = 2048u;
uint32_t kui_vmu_native_passive_stack_room(void) { return stack_room; }
void kui_toy_cdda_consumer_heartbeat(void);
uint32_t kui_toy_cdda_output_test_sr(void) { return sr; }
void kui_toy_cdda_output_test_set_sr(uint32_t value) { sr = value; }
/* A word the sound CPU keeps writing: reads of it differ while torn_left. */
static uint32_t torn_address, torn_left;
uint32_t kui_toy_cdda_output_test_read(uint32_t address, unsigned width) {
    switch(address) {
    case UINT32_C(0xffc00000): CHECK(width == 2u); return frqcr;
    case UINT32_C(0xffd80004): CHECK(width == 1u); return tstr;
    case UINT32_C(0xffd80008): CHECK(width == 4u); return tcor;
    case UINT32_C(0xffd80010): CHECK(width == 2u); return tcr;
    case UINT32_C(0xffd8000c): CHECK(width == 4u); return tcnt();
    default: break;
    }
    CHECK(width == 4u && masked());
    if(address == UINT32_C(0xa05f688c)) {
        now += access_ticks;
        ++g2_reads;
        ++fifo_polls;
        if(fifo_stuck)return 0x20u;
        since_empty = 0u;
        return 0u;
    }
    if(address - UINT32_C(0xa05f7800) < 0x80u) {
        const uint32_t channel = (address - UINT32_C(0xa05f7800)) / 0x20u, at = (address - UINT32_C(0xa05f7800)) % 0x20u;
        CHECK(at == 0x14u || at == 0x18u);
        ++g2_reads;
        return at == 0x14u ? dma_enable[channel] : dma_start[channel];
    }
    access();
    ++chip_reads;
    if(address == torn_address && torn_left) {
        --torn_left;
        return UINT32_C(0xdead0000) + torn_left;
    }
    if(address == UINT32_C(0xa0702c00))return arm_reset;
    if(address == UINT32_C(0xa0702040) || address == UINT32_C(0xa0702044))
        return mute_readback ? 0u : mixer[(address - UINT32_C(0xa0702040)) / 4u];
    if(family && address == SOUND + (family == GEN2 ? 0x12418u : 0x13418u))return driver_ticks();
    if(address - SOUND < BYTES) {
        CHECK(!(address & 3u));
        return get32(address - SOUND);
    }
    const uint32_t c = (address - CHANNELS) / 0x80u, at = (address - CHANNELS) % 0x80u;
    CHECK(address - CHANNELS < 64u * 0x80u && at <= 0x44u && !(at & 3u));
    CHECK(family && (c == pair[0] || c == pair[1]));
    return reg[c][at / 4u];
}
#if KUI_TOY_CDDA_ADPCM
/* The coder reads its samples as 16-bit words: at an odd address the SH-4
 * faults. */
void kui_toy_cdda_output_test_even(const void *address);
void kui_toy_cdda_output_test_even(const void *address) {
    CHECK(!((uintptr_t)address & 1u));
}
#endif
void kui_toy_cdda_output_test_write(uint32_t address, uint32_t value) {
    CHECK(!frozen);
    CHECK(!(arm_reset & 1u));
    access();
    ++stores;
    if(address - SOUND < BYTES) {
        const uint32_t at = address - SOUND;
        now += store_ticks;
        CHECK(!(at & 3u) && O.placed && at - O.ring < RING(O.frames));
        if(KUI_TOY_CDDA_SPARE)CHECK(O.spare && O.ring == SPARE_AT && O.frames == SPARE_FRAMES && O.ring + RING(O.frames) == 0x18000u);
        else CHECK(at >= 0x18000u && !O.spare);
        for(unsigned i = 0; i < 84u; ++i) {
            const uint32_t start = get32(0x14000u + 8u * i), size = get32(0x14000u + 8u * i + 4u);
            if(size && at + 4u > start && at < start + size)CHECK(!"a store into a pair of the bank table");
        }
#if KUI_TOY_CDDA_ADPCM
        /* A word of codes into a plane: each plane's words follow on from
         * its first, round the plane, one after the other and never one
         * twice; a ring's first word is the one the output stores with
         * nothing written yet. The codes are decoded as a chip would decode
         * them behind every code before: where the output's own coders
         * must stand when its heartbeat is over (beat()). */
        for(unsigned side = 0; side < 2u && O.phase != KUI_TOY_CDDA_OUT_OFF; ++side) {
            const uint32_t begin = O.ring + 32u + side * PLANE(O.frames);
            if(at - begin >= PLANE(O.frames))continue;
            if(at == begin && !O.written) {
                wrote[side].signal = 0;
                wrote[side].step = 127u;
                wrote_at[side] = 0u;
            }
            CHECK(at - begin == wrote_at[side]);
            wrote_at[side] = wrote_at[side] + 4u < PLANE(O.frames) ? wrote_at[side] + 4u : 0u;
            for(unsigned shift = 0; shift < 32u; shift += 4u)(void)decode(&wrote[side].signal, &wrote[side].step, value >> shift & 15u);
        }
#endif
        put32(at, value);
        ++ring_stores;
        return;
    }
    const uint32_t c = (address - CHANNELS) / 0x80u, at = (address - CHANNELS) % 0x80u;
    CHECK(address - CHANNELS < 64u * 0x80u && at <= 0x44u && !(at & 3u));
    CHECK(family && (c == pair[0] || c == pair[1]));
    CHECK(!record_busy(c == pair[1]));
    reg[c][at / 4u] = value & (at ? 0xffffu : 0x7fffu);   /* the execute bit reads back clear */
    ++register_stores;
    if(!at && (value & 0x8000u))strobe();
}

/* ---- the drivers ---- */
static uint32_t hash12(const uint8_t *text) {
    uint32_t hash = UINT32_C(0x811c9dc5);
    for(unsigned i = 0; i < 12u; ++i)hash = (hash ^ text[i]) * UINT32_C(0x01000193);
    return (hash ^ hash >> 16) & 0xffffu;
}
/* A driver's header: version bytes, a date text of the given hash (the date
 * with three more characters), the state word. */
static void header(uint32_t version, uint32_t state, const char *date, uint32_t hash) {
    uint8_t text[12];
    memset(ram, 0, 0x18000u);
    put32(0x20u, version);
    put32(0xf8u, state);
    memcpy(text, date, 9u);
    for(uint32_t tail = 0; ; ++tail) {
        CHECK(tail < UINT32_C(0x1000000));
        text[9] = (uint8_t)tail;
        text[10] = (uint8_t)(tail >> 8);
        text[11] = (uint8_t)(tail >> 16);
        if(hash12(text) == hash)break;
    }
    memcpy(ram + 0x50u, text, 12u);
}
/* A MANATEE 2 build as its start-up leaves it: the default split, the
 * stream ports' register base. */
static void manatee2_build(uint32_t version, const char *date, uint32_t hash) {
    header(version, EXEC, date, hash);
    ram[0x12060u] = 48u;
    ram[0x12061u] = 8u;
    ram[0x12062u] = 8u;
    put32(0x12068u, UINT32_C(0x00801c00));
    family = GEN2;
    pair[0] = 62u;
    pair[1] = 63u;
}
static void manatee2(void) {
    manatee2_build(UINT32_C(0x00090002), "1999.06.1", 0xc46du);
}
/* A MANATEE 1 build from 1.01.37 on: the pool of 16 with its register bases. */
static void manatee1_build(uint32_t version, const char *date, uint32_t hash) {
    header(version, SEGA, date, hash);
    for(uint32_t entry = 0; entry < 16u; ++entry)put32(0x12080u + 12u * entry + 8u, UINT32_C(0x00801800) + 0x80u * entry);
    family = GEN1;
    pair[0] = 61u;
    pair[1] = 62u;
}
static void manatee1(bool pod) {
    if(pod)manatee1_build(UINT32_C(0x00370101), "1998.11.0", 0xe50fu);
    else manatee1_build(UINT32_C(0x003b0101), "1998.12.0", 0x1c1fu);
}
/* A MANATEE 1 build before 1.01.37: the same pool, its last two entries;
 * before 1.01.32 the slower count; the first build leaves no mark. */
static void manatee1_old(uint32_t version, const char *date, uint32_t hash) {
    const uint32_t build = version >> 16 & 255u;
    manatee1_build(version, date, hash);
    if(build < 0x28u)put32(0xf8u, 0u);
    count_slow = build < 0x32u;
    family = OLD;
    pair[0] = 62u;
    pair[1] = 63u;
}
/* A NAMIHEI build: sixteen port records of 0x40 bytes from 0x11000, the
 * last eight the stream ports', each with its channel's register base. */
static void namihei(uint32_t version, const char *date, uint32_t hash) {
    header(version, SEGA, date, hash);
    for(uint32_t port = 0; port < 16u; ++port) {
        put32(0x11000u + 0x40u * port + 4u, 0x13a00u + 0x40u * port);
        put32(0x11000u + 0x40u * port + 8u, UINT32_C(0x00801800) + 0x80u * port);
    }
    family = NAMIHEI;
    pair[0] = 62u;
    pair[1] = 63u;
}
/* The builds of the CD-audio corpus beside the three run on the console:
 * version bytes, the date as far as its day's tens, the date's hash
 * (docs/evidence/cdda-background-test-88/manatee-read.log). */
static const struct build { uint32_t version; const char *date; uint16_t hash; uint8_t family; } builds[] = {
    {UINT32_C(0x00060002), "1999.05.2", 0x2cf9u, GEN2}, {UINT32_C(0x00080002), "1999.06.0", 0x4c07u, GEN2},
    {UINT32_C(0x000b0002), "1999.08.2", 0x85ecu, GEN2}, {UINT32_C(0x00000102), "1999.07.1", 0x5a6bu, GEN2},
    {UINT32_C(0x00020102), "1999.08.2", 0x140du, GEN2}, {UINT32_C(0x00020102), "1999.09.2", 0x6aa1u, GEN2},
    {UINT32_C(0x00020202), "1999.11.2", 0x8af6u, GEN2}, {UINT32_C(0x00040202), "2000.02.1", 0xa4c5u, GEN2},
    {UINT32_C(0x00050202), "2000.03.0", 0x77afu, GEN2}, {UINT32_C(0x00060202), "2000.05.2", 0x4a40u, GEN2},
    {UINT32_C(0x00070202), "2000.05.2", 0xfca8u, GEN2}, {UINT32_C(0x00080202), "2000.07.1", 0xf73eu, GEN2},
    {UINT32_C(0x00015002), "2000.03.1", 0xcb0cu, GEN2}, {UINT32_C(0x00045002), "2000.07.1", 0xd369u, GEN2},
    {UINT32_C(0x00380101), "1998.11.0", 0x6edeu, GEN1}, {UINT32_C(0x00380101), "1998.11.2", 0xa0c1u, GEN1},
    {UINT32_C(0x003f0101), "1999.03.1", 0xfe7fu, GEN1}, {UINT32_C(0x00400101), "1999.05.2", 0x2cf9u, GEN1},
    {UINT32_C(0x001d0101), "1998.08.3", 0xd25du, OLD}, {UINT32_C(0x00280101), "1998.09.2", 0x768du, OLD},
    {UINT32_C(0x00320101), "1998.10.1", 0x1c18u, OLD}, {UINT32_C(0x00360101), "1998.10.2", 0xbd17u, OLD},
    {UINT32_C(0x000d0001), "1998.12.1", 0x0378u, NAMIHEI}, {UINT32_C(0x00120001), "1999.06.0", 0x4902u, NAMIHEI}};
static void driver_of(const struct build *b) {
    if(b->family == GEN2)manatee2_build(b->version, b->date, b->hash);
    else if(b->family == OLD)manatee1_old(b->version, b->date, b->hash);
    else if(b->family == NAMIHEI)namihei(b->version, b->date, b->hash);
    else manatee1_build(b->version, b->date, b->hash);
}
static void bank(unsigned index, uint32_t address, uint32_t size) {
    put32(0x14000u + 8u * index, address);
    put32(0x14000u + 8u * index + 4u, size);
}
static unsigned long starts;
static void fresh(void) {
    starts = 0u;
    memset(ram, 0x5a, sizeof(ram));   /* what lay in sound memory: not silence */
    memset(ram, 0, 0x18000u);
    memset(reg, 0, sizeof(reg));
    memset(voice, 0, sizeof(voice));
    memset(&O, 0, sizeof(O));
    memset(&D, 0, sizeof(D));
    memset(&card, 0, sizeof(card));
    memset(dma_enable, 0, sizeof(dma_enable));
    memset(dma_start, 0, sizeof(dma_start));
    sr = REST;
    arm_reset = 0u;
    frqcr = 0x0e0au;
    tcor = UINT32_MAX;
    tcr = 2u;
    tstr = 1u;
    access_ticks = 1u;
    store_ticks = 0u;
    fifo_stuck = frozen = mute_readback = card_idle = card_skips = inclusive = false;
    strict = true;
    ppm = 0;
    refuse_track = 0u;
    card_most = 8192u;
    card_chunk = 1024u;
    card_ticks = 40u;
    mixer[0] = 0x0f1fu;
    mixer[1] = 0x0f0fu;
    stores = ring_stores = register_stores = chip_reads = g2_reads = fifo_polls = 0u;
    card_calls = card_nones = card_segments = card_bytes = card_low = 0u;
    heard_frames = wrong_frames = wrong_loud = wrong_nonzero = cut_frames = loud_frames = key_ons = key_offs = lonely = 0u;
    far_frames = 0u;
    cut = false;
    tick_base = 0u;
    count_stuck = count_slow = false;
    count_ppm = 0;
    torn_left = 0u;
    family = 0u;
}
/* One heartbeat of the worker, `ticks` after the last ended. */
static uint64_t beat_began;
static void beat(uint32_t ticks) {
    now += ticks;
    beat_began = stretch_began = now;
    stretch_live = true;
    chip_run();
    card_calls_now = 0u;
    since_empty = 8u;
    const uint32_t before = O.phase, rest = sr;
    uint32_t sector = 0u, passes = 0u;
    const bool live = D.state == KUI_TOY_CDDA_PLAYING && oracle(tcnt(), &sector, &passes);
    /* As the worker's heartbeat calls it. */
    kui_toy_cdda_consumer_heartbeat();
    stretch_live = false;
    CHECK(sr == rest);
#if KUI_TOY_CDDA_ADPCM
    /* What the output holds of its writing is what it stored: both planes
     * as far as each other, the write position behind the last unit, and
     * each plane's coder where a chip's decoder stands behind the codes. */
    if((O.phase == KUI_TOY_CDDA_OUT_PRIME || O.phase == KUI_TOY_CDDA_OUT_RUN) && O.written) {
        CHECK(wrote_at[0] == wrote_at[1] && O.at / 2u == wrote_at[0]);
        for(unsigned side = 0; side < 2u; ++side)
            CHECK(O.coder[side].signal == wrote[side].signal && O.coder[side].step == wrote[side].step);
    }
#endif
    if(before == KUI_TOY_CDDA_OUT_OFF && (O.phase == KUI_TOY_CDDA_OUT_PRIME || O.phase == KUI_TOY_CDDA_OUT_RUN)) {
        CHECK(live);
        heard_play.first = D.first;
        heard_play.end = D.end;
        heard_play.base = sector;
        heard_play.passes = passes;
        ++starts;
    }
}
/* The game's heartbeats for a while: 5.5 ms apart, now and then closer or
 * later, as a game's polls come. */
static void run(uint32_t milliseconds) {
    static uint32_t seed = 12345u;
    for(uint64_t until = now + (uint64_t)milliseconds * SECOND / 1000u; now < until;) {
        seed = seed * 1103515245u + 12345u;
        const uint32_t pick = seed >> 16 & 15u;
        beat(pick == 0u ? 300u : pick == 1u ? 9000u : 4000u + (seed >> 20 & 1023u));
    }
}
static void play(uint32_t first, uint32_t end, uint32_t passes, int32_t offset) {
    D.first = first;
    D.end = end;
    D.passes = passes;
    D.done = 0u;
    D.base_counter = tcnt();
    D.base_offset = offset;
    D.state = KUI_TOY_CDDA_PLAYING;
    ++D.generation;
}
static bool sounding(void) { return voice[pair[0]].active && voice[pair[1]].active; }
static bool silent(void) { return !voice[pair[0]].active && !voice[pair[1]].active; }

/* ---- the scenarios ---- */
/* A play from its start to its end on each of the three builds. */
static void plays_through(unsigned build, bool full) {
    fresh();
    if(build == 0u)manatee2();
    else manatee1(build == 2u);
    if(full) {
        /* The game's data fills sound memory. */
        bank(0u, 0x18000u, 0x1e8000u);
    } else {
        /* The game's banks from the driver's end up, and one in the middle. */
        bank(0u, 0x18000u, 0x100000u);
        bank(17u, 0x150000u, 0x20000u);
    }
    bank(40u, 0x4000u, 0x800u);       /* a default table inside the driver */
    beat(1000u);
    CHECK(!stores && !chip_reads && !g2_reads);    /* no play: nothing is touched */
    card_most = 1024u;
    play(1000u, 4000u, 1u, 0);
    /* The ring is being primed and wants all it can get: all the same no
     * two heartbeats use the bus that begin less than 2 ms apart. */
    for(unsigned beats = 0; O.phase != KUI_TOY_CDDA_OUT_PRIME; ++beats) {
        CHECK(beats < 100u);
        beat(5000u);
    }
    beat(5000u);
    const unsigned long first = stores + chip_reads + g2_reads;
    const uint32_t spent = (uint32_t)(now - beat_began);
    CHECK(O.phase == KUI_TOY_CDDA_OUT_PRIME && spent < 1000u);
    beat(1500u - spent);
    CHECK(stores + chip_reads + g2_reads == first);
    beat(200u);
    CHECK(stores + chip_reads + g2_reads > first);
    card_most = 8192u;
    run(2000u);
    CHECK(O.phase == KUI_TOY_CDDA_OUT_RUN && sounding() && O.keys == 1u && key_ons == 2u && starts == 1u);
    CHECK(O.family == family && O.channel[0] == pair[0] && O.channel[1] == pair[1]);
    /* The largest ring below CRI's slots, clear of the banks; or, with
     * SPARE, the driver's spare memory whatever the banks leave. */
    if(KUI_TOY_CDDA_SPARE)CHECK(O.spare && O.frames == SPARE_FRAMES && O.ring == SPARE_AT);
    else CHECK(!full && !O.spare && O.frames == LARGEST && O.ring + RING(LARGEST) <= 0x1dfe00u && O.ring >= 0x170000u && !(O.ring & 31u));
    CHECK(get32(O.ring) == UINT32_C(0x4b434452) && get32(O.ring + RING(O.frames) - 4u) == UINT32_C(0x4b434452));
    CHECK(heard_frames > 80000u && !wrong_frames && !O.underruns && !O.restarts && !O.faults && !O.timeouts);
    /* (ADPCM: what the chip decodes is the play's audio, but for the frames
     * behind a leap of the test's pattern.) */
    CHECK(far_frames * 50u < heard_frames);
    /* Two heartbeats less than 2 ms apart: the second does nothing. */
    beat(14000u);
    const unsigned long work = stores + chip_reads + g2_reads;
    CHECK(work);
    beat(300u);
    CHECK(stores + chip_reads + g2_reads == work);
    /* The channels as the adapters' driver keyed them. */
    for(unsigned side = 0; side < 2u; ++side) {
        const uint32_t *r = reg[pair[side]], plane = O.ring + 32u + side * (PLANE(O.frames) + REPEAT);
        static const uint32_t rest[] = {0x1fu, 0x1fu, 0u, 0x4210u, 0u};
        CHECK(r[0] == (0x4200u | FORMAT | plane >> 16) && r[1] == (plane & 0xffffu) && !r[2] && r[3] == O.frames);
#if !KUI_TOY_CDDA_ADPCM
        /* Behind the plane, its first two frames once more. */
        CHECK(!memcmp(ram + plane, ram + plane + 2u * O.frames, 4u));
#endif
        CHECK(!memcmp(r + 4, rest, sizeof(rest)) && r[10] == 4u && !r[16] && !r[17]);
        for(unsigned i = 11; i < 16u; ++i)CHECK(r[i] == 0x1fffu);
        CHECK(r[9] == (side ? 0x0f0fu : 0x0f1fu));
    }
    /* Paused: keyed off, and the card is told so. Released: on from where
     * the drive stands. */
    D.state = KUI_TOY_CDDA_PAUSED;
    run(100u);
    CHECK(silent() && O.phase == KUI_TOY_CDDA_OUT_OFF && card_nones == 1u && !card.generation);
    const unsigned long quiet_stores = stores, quiet_reads = chip_reads;
    run(300u);
    CHECK(stores == quiet_stores && chip_reads == quiet_reads);
    D.base_counter = tcnt();
    D.base_offset = 400;
    D.state = KUI_TOY_CDDA_PLAYING;
    run(1000u);
    CHECK(sounding() && starts == 2u && heard_play.base == 1400u && !wrong_frames);
    /* To the end of its one pass: the last frames are played, then nothing. */
    run(40000u);
    CHECK(silent() && O.phase == KUI_TOY_CDDA_OUT_SILENT && !O.why && !wrong_frames && !O.underruns);
    CHECK(last_heard >= 2600u * 588u && last_heard < 2600u * 588u + 4096u);
    D.state = KUI_TOY_CDDA_ENDED;
    ++D.generation;
    run(100u);
    CHECK(O.phase == KUI_TOY_CDDA_OUT_OFF && silent() && !O.asked);
    printf("PASS a play on %s: placed at 0x%06x, %lu frames played, every one the play's; pause, release and the end\n",
        build == 0u ? "MANATEE 2.00.09" : build == 1u ? "MANATEE 1.01.3b" : "MANATEE 1.01.37", (unsigned)O.ring, heard_frames);
}
/* Tracks, a gap between them, a track the card does not hold, laps. */
static void pieces(void) {
    fresh();
    manatee2();
    play(3900u, 4300u, 0u, 0);        /* the end of track 2, the gap, the start of track 3, for ever */
    run(20000u);
    CHECK(sounding() && !wrong_frames && O.silence_frames >= 151u * 588u && card_segments >= 4u && !O.underruns);
    D.state = KUI_TOY_CDDA_IDLE;
    ++D.generation;
    run(100u);
    CHECK(silent());
    refuse_track = 4u;
    play(6900u, 7200u, 2u, 0);        /* into a track the card does not hold: silence in its place */
    run(9000u);
    CHECK(O.phase == KUI_TOY_CDDA_OUT_SILENT && !O.why && !wrong_frames && O.refusals >= 2u && silent());
    printf("PASS tracks, a gap, a track the card refuses and laps: %lu frames, every one the play's or silence\n", heard_frames);
}
/* The chip's clock off TMU0's: the console's quarter of a percent (Test 86)
 * and more, each way. The driver's count is the chip's own time, and the
 * ring's clock. */
static void drifting(int parts) {
    fresh();
    manatee1(false);
    ppm = parts;
    play(1000u, 9000u, 0u, 0);
    run(300000u);
    /* The two clocks as the output read them, for the report: the driver's
     * count of 4 ms of the chip's time, and TMU0's ticks beside it, each
     * true to one of the driver's ticks at either end. */
    CHECK(O.chip_ticks > 70000u && llabs((long long)O.tmu_ticks -
        (long long)O.chip_ticks * 3125 * 1000000 / (1000000 + parts)) < 2 * 3125 + 200);
    /* Five minutes, and the ring never begun again. */
    CHECK(sounding() && !wrong_frames && !O.underruns && !O.clock_faults && !O.restarts && starts == 1u);
    /* For the report: the least a look found the ring ahead of the chip
     * (never less than the margin: it did not run dry; and well below the
     * full ring it ends with: the least, not the last), and the longest
     * wait between two looks, which is how far the game's calls came
     * apart. */
    CHECK(O.lead_least >= 241u && O.lead_least < O.frames * 2u / 3u && O.gap_longest >= 9000u && O.gap_longest < 20000u);
#if KUI_TOY_CDDA_ADPCM
#if KUI_TOY_CDDA_SPARE
    /* The same in MANATEE 2's spare memory, the smallest ring there is. */
    fresh();
    manatee2();
    bank(0u, 0x18000u, 0x1e8000u);
    ppm = parts;
    play(1000u, 9000u, 0u, 0);
    run(300000u);
    CHECK(sounding() && O.spare && !wrong_frames && !O.underruns && !O.clock_faults && !O.restarts && starts == 1u);
#else
    /* And in the smallest ring of the table's, for eight minutes. */
    fresh();
    manatee1(false);
    bank(0u, 0x18000u, 0x200000u - 0x18000u - 20000u);
    ppm = parts;
    play(1000u, 9000u, 0u, 0);
    run(480000u);
    CHECK(sounding() && O.frames == sizes[2] && !wrong_frames && !O.underruns && !O.clock_faults && !O.restarts && starts == 1u);
#endif
#endif
    /* A chip that plays the frame at its loop end as well: a frame twice a
     * lap, and never one that is not the play's, for a minute. (16-bit
     * samples only: an ADPCM decoder would take a code twice, and the
     * console's chip plays the compressed ring as reckoned, Test 86. Such
     * a chip falls a frame a lap behind the count: the margin holds it for
     * about two minutes of the largest ring.) */
    if(parts > 0 && !KUI_TOY_CDDA_ADPCM) {
        fresh();
        manatee2();
        inclusive = true;
        play(1000u, 9000u, 0u, 0);
        run(60000u);
        CHECK(sounding() && !wrong_frames && !O.underruns && heard_frames > 2600000u);
    }
    /* A play of nearly two minutes to its end: its last frame is played
     * before the key-off. (A chip faster than the drive's clock, which is
     * TMU0's, comes to the play's end ahead of the drive. One slower by
     * more than what the ring holds would have its end cut by the drive's:
     * 180 parts in a million is held here.) */
    fresh();
    manatee2();
    ppm = parts < -180 ? -180 : parts;
    play(1000u, 9000u, 1u, 0);
    run(109000u);
    CHECK(silent() && !wrong_frames && last_heard + 1u >= (9000u - heard_play.base) * 588u && !O.restarts);
    printf("PASS the chip's clock %+d ppm from TMU0 over five minutes: every frame the play's and the ring never begun again; a play's last frame at %+d ppm\n",
        parts, parts < -180 ? -180 : parts);
}
/* The ring's clock is the driver's count, and TMU0 its witness. */
static void clocked(void) {
    /* A count that stands still is seen at the next look or the one after,
     * before the ring has run out: the ring begins again. Still standing,
     * each new ring is given up some ten milliseconds in, and the play at
     * the ninth. Nothing but the play is heard. */
    fresh();
    manatee2();
    bank(0u, 0x18000u, 0x100000u);
    ppm = 2450;
    play(1000u, 9000u, 0u, 0);
    run(20000u);
    CHECK(sounding() && !O.clock_faults && !O.restarts);
    count_frozen = driver_ticks();
    count_stuck = true;
    beat(4500u);
    beat(4500u);
    beat(4500u);
    CHECK(O.clock_faults == 1u && O.restarts == 1u && !O.underruns && !wrong_frames);
    run(3000u);
    CHECK(silent() && O.phase == KUI_TOY_CDDA_OUT_SILENT && O.why == KUI_TOY_CDDA_OUT_WHY_CLOCK && O.why_detail == 2u);
    CHECK(O.clock_faults == 9u && O.restarts == 8u && !O.underruns && !wrong_frames);
    /* The next play, with the count running again, plays. */
    count_stuck = false;
    play(2000u, 9000u, 0u, 0);
    run(5000u);
    CHECK(sounding() && O.clock_faults == 9u && !wrong_frames);
    /* A count that fell under a play (the driver began it again, and
     * everything else of the driver is as it was): the ring begins again,
     * once, and plays on. */
    const unsigned long begun = starts;
    tick_base += driver_ticks() / 2u;
    run(5000u);
    CHECK(sounding() && O.clock_faults == 10u && starts == begun + 1u && !O.underruns && !wrong_frames);
    /* A count that jumped ahead: the same. */
    tick_base -= 1000u;
    run(5000u);
    CHECK(sounding() && O.clock_faults == 11u && starts == begun + 2u && !O.underruns && !wrong_frames);
    /* And one that jumps two seconds ahead five minutes into a ring, where
     * that is less than a sixty-fourth of the ring's time: it is the time
     * since the count last moved that does not bear it out. */
    run(310000u);
    CHECK(sounding() && O.clock_faults == 11u && starts == begun + 2u && !O.underruns && !wrong_frames);
    tick_base -= 500u;
    run(5000u);
    CHECK(sounding() && O.clock_faults == 12u && starts == begun + 3u && !O.underruns && !wrong_frames);
    /* A count that is no count of the chip's time, a twentieth fast or
     * slow: TMU0 does not bear it out, and the play is given up. (What is
     * heard on the way is not held to the play.) */
    for(int way = -1; way <= 1; way += 2) {
        fresh();
        manatee1(false);
        strict = false;
        count_ppm = 50000 * way;
        play(1000u, 9000u, 0u, 0);
        run(30000u);
        CHECK(silent() && O.phase == KUI_TOY_CDDA_OUT_SILENT && O.why == KUI_TOY_CDDA_OUT_WHY_CLOCK && O.why_detail == 2u &&
            O.clock_faults == 9u);
    }
    printf("PASS the ring's clock: a count that stands still, fell or jumped begins the ring again before anything else is heard; one that is no clock is given up\n");
}
/* A game that calls once a frame of video, as the console's games do: 13,000
 * of TMU0's ticks from one call to the next, the chip a quarter of a percent
 * ahead of TMU0. The chip plays 2,947 bytes of the card's a frame. */
static void frames_of_video(unsigned count) {
    for(unsigned frame = 0; frame < count; ++frame)beat(13000u - (uint32_t)(now - beat_began));
}
static void framed(void) {
    /* The card hands on three kilobytes a heartbeat, and four: the ring
     * holds for a minute, every frame of video a heartbeat that uses the
     * bus, and never less ahead than the margin. The minute takes the play
     * out of one track, through the two seconds between tracks and into
     * the next: silence comes as fast as the chip plays it. (As ADPCM. The
     * build of 16-bit samples, which is not the one delivered, keeps its
     * slower silence and is held within the first track here.) */
    for(uint32_t most = 3072u; most <= 4096u; most += 1024u) {
        fresh();
        manatee1(false);
        ppm = 2450;
        card_most = most;
        play(1000u, 9000u, 0u, 0);
        frames_of_video(KUI_TOY_CDDA_ADPCM ? 3600u : 2000u);
        CHECK(sounding() && !wrong_frames && !O.underruns && !O.restarts && starts == 1u);
        CHECK(O.heartbeats >= (KUI_TOY_CDDA_ADPCM ? 3590u : 1990u) && O.lead_least >= 241u && O.gap_longest < 13100u);
    }
    /* Two kilobytes a heartbeat, which was all KOF Dream Match 1999's
     * window let through under Test 86: the ring runs dry twice a second,
     * as it did on the console. */
    fresh();
    manatee1(false);
    ppm = 2450;
    card_most = 2048u;
    strict = false;
    play(1000u, 9000u, 0u, 0);
    frames_of_video(3600u);
    CHECK(O.underruns >= 100u && O.underruns <= 140u && O.restarts == O.underruns && O.lead_least < 241u);
    printf("PASS a game that calls once a frame: three kilobytes a heartbeat hold the ring for a minute, two run it dry %u times\n",
        (unsigned)O.underruns);
}
/* A family is every build of it: each build of the corpus plays, on its
 * family's channels and in its family's spare memory, known by its version
 * bytes and its date. */
static void families(void) {
    for(unsigned i = 0; i < sizeof(builds) / sizeof(builds[0]); ++i) {
        const struct build *b = &builds[i];
        fresh();
        driver_of(b);
        if(!KUI_TOY_CDDA_SPARE)bank(0u, 0x18000u, 0x100000u);
        play(1000u, 9000u, 0u, 0);
        /* (A count taken by the wrong measure is a quarter of a percent
         * out. Taken for more frames than it is, the chip and the writer
         * meet within seconds; taken for fewer, as a later build's by the
         * first builds' measure, the chip gains on the writer by a ring
         * in two minutes.) */
        run(b->family == NAMIHEI ? 150000u : b->family == OLD ? 30000u : 1500u);
        CHECK(O.phase == KUI_TOY_CDDA_OUT_RUN && sounding() && O.keys == 1u && starts == 1u);
        CHECK(O.family == b->family && O.version == b->version && O.hash == b->hash);
        CHECK(O.channel[0] == pair[0] && O.channel[1] == pair[1]);
        if(KUI_TOY_CDDA_SPARE)CHECK(O.spare && O.frames == SPARE_FRAMES && O.ring == SPARE_AT);
        CHECK(heard_frames > 50000u && !wrong_frames && !O.underruns && !O.restarts && !O.faults && !O.timeouts && !O.clock_faults);
    }
    /* A build of the later measure is not taken by the first builds'. */
    fresh();
    manatee1(false);
    play(1000u, 9000u, 0u, 0);
    run(150000u);
    CHECK(sounding() && O.keys == 1u && !wrong_frames && !O.restarts);
    /* MANATEE 1 before 1.01.37 by its last two entries: one in use, the
     * play silent; the entry before them is the game's to use. */
    for(unsigned kind = 0; kind < 4u; ++kind) {
        fresh();
        driver_of(&builds[21]);
        if(kind < 2u)put32(0x12080u + 12u * (14u + kind) + 4u, 0x1234u);
        if(kind == 2u)put32(0x12080u + 12u * 15u + 8u, UINT32_C(0x00801f00));
        if(kind == 3u)put32(0x12080u + 12u * 13u + 4u, 0x1234u);
        play(1000u, 4000u, 1u, 0);
        run(500u);
        if(kind < 2u)CHECK(!stores && O.why == KUI_TOY_CDDA_OUT_WHY_TAKE && O.why_detail == 1u << kind);
        if(kind == 2u)CHECK(!stores && O.why == KUI_TOY_CDDA_OUT_WHY_PROOF);
        if(kind == 3u)CHECK(sounding() && !wrong_frames && O.channel[0] == 62u && O.channel[1] == 63u);
    }
    /* NAMIHEI by its stream ports 6 and 7: one playing, the play silent;
     * a record that does not name its channel proves nothing; port 5 is
     * the game's to use; and MANATEE 1's pool is not asked. */
    for(unsigned kind = 0; kind < 5u; ++kind) {
        fresh();
        driver_of(&builds[23]);
        if(kind < 2u)ram[0x11200u + 0x40u * (6u + kind)] = 1u;
        if(kind == 2u)put32(0x11200u + 0x40u * 7u + 8u, UINT32_C(0x00801f00));
        if(kind == 3u)ram[0x11200u + 0x40u * 5u] = 1u;
        if(kind == 4u)put32(0x12080u + 12u * 14u + 4u, 0x1234u), put32(0x12080u + 12u * 15u + 4u, 0x1234u);
        play(1000u, 4000u, 1u, 0);
        run(500u);
        if(kind < 2u)CHECK(!stores && O.why == KUI_TOY_CDDA_OUT_WHY_TAKE && O.why_detail == 1u << kind);
        if(kind == 2u)CHECK(!stores && O.why == KUI_TOY_CDDA_OUT_WHY_PROOF);
        if(kind > 2u)CHECK(sounding() && !wrong_frames && O.channel[0] == 62u && O.channel[1] == 63u && (!KUI_TOY_CDDA_SPARE || O.ring == 0x15000u));
    }
    printf("PASS every build of the four families plays: %u builds beside the three of the console, each known by its version bytes and its date; the first builds of MANATEE 1 by their count of 176 frames\n",
        (unsigned)(sizeof(builds) / sizeof(builds[0])));
}
/* Whatever is not proved leaves the chip alone. */
static void refused(void) {
    /* A driver that is not running; one whose date cannot be read as it
     * runs is another build of its family, and admitted (families()). */
    for(unsigned kind = 0; kind < 2u; ++kind) {
        fresh();
        manatee2();
        if(kind == 0u)put32(0xf8u, 0u);
        if(kind == 1u)arm_reset = 1u;
        play(1000u, 4000u, 1u, 0);
        run(500u);
        CHECK(!stores && O.phase == KUI_TOY_CDDA_OUT_SILENT && !card_calls &&
            O.why == (kind == 1u ? KUI_TOY_CDDA_OUT_WHY_ARM : KUI_TOY_CDDA_OUT_WHY_DRIVER));
    }
    /* Version bytes of no family given output, and a family's under
     * another's running mark or none. */
    static const struct { uint32_t version, state; } none[] = {
        {UINT32_C(0x00090003), EXEC},     /* 3.00.09 */
        {UINT32_C(0x003b0103), SEGA},     /* 3.01.3b and 0.01.3b: MANATEE 1's but for the first byte */
        {UINT32_C(0x003b0100), SEGA},
        {UINT32_C(0x00120003), SEGA},     /* and NAMIHEI's */
        {UINT32_C(0x01090002), EXEC},     /* a fourth byte */
        {UINT32_C(0x00090002), SEGA},     /* MANATEE 2 under MANATEE 1's mark */
        {UINT32_C(0x003b0101), EXEC},     /* and the other way */
        {UINT32_C(0x003b0101), 0u},
        {UINT32_C(0x003b0201), SEGA},     /* 1.02 */
        {UINT32_C(0x00365001), SEGA},     /* AUDIO64's 1.50 */
        {UINT32_C(0x002d0101), SEGA},     /* between 1.01.28 and 1.01.32: no such build was read */
        {UINT32_C(0x00310101), SEGA},
        {UINT32_C(0x00290101), SEGA},
        {UINT32_C(0x00280101), 0u},       /* from 1.01.28 on a driver that runs has its mark */
        {UINT32_C(0x00360101), 0u},
        {UINT32_C(0x00120001), 0u},       /* NAMIHEI without its mark, and under MANATEE 2's */
        {UINT32_C(0x00120001), EXEC}};
    for(unsigned kind = 0; kind < sizeof(none) / sizeof(none[0]); ++kind) {
        fresh();
        if(none[kind].version & 1u)manatee1(false);
        else manatee2();
        put32(0x20u, none[kind].version);
        put32(0xf8u, none[kind].state);
        play(1000u, 4000u, 1u, 0);
        run(500u);
        CHECK(!stores && O.phase == KUI_TOY_CDDA_OUT_SILENT && !card_calls && !O.family &&
            O.why == KUI_TOY_CDDA_OUT_WHY_DRIVER && O.seen_version == none[kind].version);
    }
    /* Records that do not prove the two channels; and a split of
     * MANATEE 2's that is proved and leaves no pair: one stream port, or
     * none (Bangai-O gives all 64 channels to one-shots; the stream ports'
     * base word is then not asked). */
    for(unsigned kind = 0; kind < 7u; ++kind) {
        fresh();
        if(kind != 3u)manatee2();
        else manatee1(false);
        if(kind == 0u)ram[0x12062u] = 1u, ram[0x12061u] = 15u, put32(0x12068u, UINT32_C(0x00801f80));
        if(kind == 1u)ram[0x12060u] = 47u;
        if(kind == 2u)put32(0x12068u, UINT32_C(0x00801800));
        if(kind == 3u)put32(0x12080u + 12u * 14u + 8u, UINT32_C(0x00801f80));
        if(kind == 4u)ram[0x12060u] = 0u, ram[0x12061u] = 64u, ram[0x12062u] = 0u, put32(0x12068u, 0u);
        if(kind == 5u)ram[0x12060u] = 0u, ram[0x12061u] = 63u, ram[0x12062u] = 0u;
        if(kind == 6u)ram[0x12060u] = 31u, ram[0x12061u] = 16u, ram[0x12062u] = 17u, put32(0x12068u, UINT32_C(0x00801780));
        play(1000u, 4000u, 1u, 0);
        run(500u);
        CHECK(!stores && O.phase == KUI_TOY_CDDA_OUT_SILENT && O.why == KUI_TOY_CDDA_OUT_WHY_PROOF);
        CHECK(O.why_detail == (kind == 0u || kind == 4u ? KUI_TOY_CDDA_OUT_PROOF_SPLIT : 0u));
    }
    /* A split with two stream ports is a pair: the top two channels. */
    fresh();
    manatee2();
    ram[0x12060u] = 0u, ram[0x12061u] = 62u, ram[0x12062u] = 2u;
    put32(0x12068u, UINT32_C(0x00801f00));
    play(1000u, 4000u, 1u, 0);
    run(500u);
    CHECK(sounding() && !wrong_frames && O.channel[0] == 62u && O.channel[1] == 63u);
    /* A channel the game uses at the start. */
    for(unsigned side = 0; side < 2u; ++side) {
        fresh();
        manatee1(true);
        put32(0x12080u + 12u * (13u + side) + 4u, 0x1234u);
        play(1000u, 4000u, 1u, 0);
        run(500u);
        CHECK(!stores && O.why == KUI_TOY_CDDA_OUT_WHY_TAKE && O.why_detail == 1u << side);
    }
    /* No room: the banks leave less than the smallest ring anywhere. */
    fresh();
    manatee2();
    bank(0u, 0x18000u, 0x1e8000u - 0x3000u);
    play(1000u, 4000u, 1u, 0);
    run(500u);
#if KUI_TOY_CDDA_SPARE
    /* (With SPARE that is no refusal: the driver's spare memory.) */
    CHECK(sounding() && O.spare && O.ring == SPARE_AT && !wrong_frames);
#else
    CHECK(!stores && O.why == KUI_TOY_CDDA_OUT_WHY_RING && O.why_detail == KUI_TOY_CDDA_OUT_RING_NONE);
#endif
    /* A pair the reading does not explain. */
    fresh();
    manatee2();
    bank(33u, 0x00800000u, 16u);
    play(1000u, 4000u, 1u, 0);
    run(500u);
#if KUI_TOY_CDDA_SPARE
    CHECK(sounding() && O.spare && O.ring == SPARE_AT && !wrong_frames);
#else
    CHECK(!stores && O.why == KUI_TOY_CDDA_OUT_WHY_RING && O.why_detail == KUI_TOY_CDDA_OUT_RING_PAIR + 33u);
#endif
    /* The timer out of its profile; SR.BL; a G2 DMA channel. */
    fresh();
    manatee2();
    tcr = 3u;
    play(1000u, 4000u, 1u, 0);
    run(300u);
    CHECK(!stores && !chip_reads && O.why == KUI_TOY_CDDA_OUT_WHY_CLOCK);
    for(unsigned kind = 0; kind < 2u; ++kind) {
        fresh();
        manatee2();
        play(1000u, 4000u, 1u, 0);
        if(kind)dma_start[3] = 1u;
        else dma_enable[1] = 1u;
        run(300u);
        CHECK(!stores && !chip_reads && O.skipped_dma > 10u);
    }
    fresh();
    manatee2();
    play(1000u, 4000u, 1u, 0);
    sr = REST | BL;
    run(300u);
    CHECK(!stores && !chip_reads && !g2_reads && O.skipped_bl > 10u);
    printf("PASS nothing is written for another driver, one not running, a held sound CPU, unproved records, a used channel, no room, no clock, a G2 transfer, SR.BL\n");
}
/* Where the ring goes. */
static void placing(void) {
#if KUI_TOY_CDDA_SPARE
    /* The driver's spare memory, whatever the table says. The game has the
     * driver start again, which clears its work memory: what lies there is
     * no longer the ring. The play takes the place again and begins there;
     * the ninth time in one play it is given up. */
    for(unsigned kind = 0; kind < 2u; ++kind) {
        fresh();
        if(kind)manatee2();
        else manatee1(false);
        bank(48u + 15u, 0x1fbfc0u, 0x4000u);
        play(1000u, 4000u, 1u, 0);
        run(1000u);
        CHECK(sounding() && O.spare && O.ring == SPARE_AT && O.frames == SPARE_FRAMES && !wrong_frames);
        /* (MANATEE 1 clears 0x13000..0x17FFF, MANATEE 2 0x12000..0x16FFF
         * and with it its split, which the game sets again.) */
        memset(ram + (kind ? 0x12000u : 0x13000u), 0, 0x5000u);
        if(kind)manatee2();
        tick_base += driver_ticks();
        strict = false;
        run(40u);
        CHECK(O.lost_guard == 1u && O.restarts == 1u);
        strict = true;
        const unsigned long wrong_spare = wrong_frames;
        run(1000u);
        CHECK(sounding() && O.spare && O.ring == SPARE_AT && O.places == 2u && starts == 2u && wrong_frames == wrong_spare);
        /* (The driver's count began again with it, and the output's look
         * at the two clocks with the count.) */
        CHECK(O.chip_ticks > 200u && O.chip_ticks < 400u && llabs((long long)O.tmu_ticks - (long long)O.chip_ticks * 3125) < 2 * 3125 + 200);
        for(unsigned times = 2; times <= 9u; ++times) {
            CHECK(sounding());
            put32(O.ring + (times & 1u ? 0u : RING(O.frames) - 4u), 0u);
            strict = false;
            run(40u);
            strict = true;
            run(1000u);
            CHECK(O.lost_guard == times);
        }
        CHECK(silent() && O.why == KUI_TOY_CDDA_OUT_WHY_RING && O.why_detail == KUI_TOY_CDDA_OUT_RING_GUARD && O.restarts == 8u &&
            !O.placed);
        /* The next play takes the place again. */
        D.state = KUI_TOY_CDDA_IDLE;
        ++D.generation;
        run(100u);
        play(1000u, 4000u, 1u, 0);
        run(1000u);
        CHECK(sounding() && O.spare && O.ring == SPARE_AT && O.places == 10u);
    }
    printf("PASS the ring's place: the driver's spare memory; given up when a guard says so and taken again in the same play, eight times\n");
#else
    /* Banks up to CRI's slots: the ring in the slots' place while the table
     * names none of them. */
    fresh();
    manatee2();
    bank(0u, 0x18000u, 0x1dfe00u - 0x18000u);
    play(1000u, 4000u, 1u, 0);
    run(1000u);
    CHECK(sounding() && O.frames == LARGEST && O.ring == ((0x200000u - RING(LARGEST)) & ~31u) && !wrong_frames);
    /* CRI opens its first slot, which lies in it: no store there after the
     * table names it (the model refuses one), and the play begins again in
     * a place below the slot. */
    bank(48u + 15u, 0x1fbfc0u, 0x4000u);
    strict = false;
    run(40u);
    CHECK(O.lost_table == 1u && O.restarts == 1u);
    strict = true;
    const unsigned long wrong_slot = wrong_frames;
    run(1000u);
    CHECK(sounding() && O.frames == LARGEST && O.ring == ((0x1fbfc0u - RING(LARGEST)) & ~31u) && starts == 2u &&
        O.places == 2u && wrong_frames == wrong_slot);
    /* Less room: the smaller rings. */
    for(unsigned size = 1; size < 3u; ++size) {
        fresh();
        manatee1(false);
        const uint32_t room = size == 1u ? 40000u : 20000u;
        bank(0u, 0x18000u, 0x200000u - 0x18000u - room);
        play(1000u, 4000u, 1u, 0);
        run(3000u);
        CHECK(sounding() && O.frames == sizes[size] && O.ring >= 0x200000u - room && !wrong_frames && !O.underruns);
    }
    /* A guard line overwritten: what lies there is no longer ours. */
    fresh();
    manatee2();
    play(1000u, 4000u, 1u, 0);
    run(1000u);
    CHECK(sounding());
    put32(O.ring + 28u, 0u);
    strict = false;
    run(40u);
    CHECK(O.lost_guard == 1u && O.restarts == 1u);
    strict = true;
    const unsigned long wrong_guard = wrong_frames;
    run(1000u);
    CHECK(sounding() && O.places == 2u && get32(O.ring + 28u) == UINT32_C(0x4b434452) && wrong_frames == wrong_guard);
    /* The ninth time in one play it is given up. */
    for(unsigned times = 2; times <= 9u; ++times) {
        CHECK(sounding());
        put32(O.ring + (times & 1u ? 0u : RING(O.frames) - 4u), 0u);
        strict = false;
        run(40u);
        strict = true;
        run(1000u);
        CHECK(O.lost_guard == times);
    }
    CHECK(silent() && O.why == KUI_TOY_CDDA_OUT_WHY_RING && O.why_detail == KUI_TOY_CDDA_OUT_RING_GUARD && O.restarts == 8u && !O.placed);
    printf("PASS the ring's place: below CRI's slots first, among them when the table names none, smaller where room is short; given up when a pair or a guard says so\n");
#endif
}
/* The driver takes a channel, keys one off, changes its registers, changes itself. */
static void contested(void) {
    for(unsigned side = 0; side < 2u; ++side) {
        fresh();
        manatee2();
        play(1000u, 4000u, 1u, 0);
        run(1000u);
        CHECK(sounding());
        /* The game starts a stream on the port: its record says so first. */
        ram[0x13800u + 0x40u * (6u + side)] |= 1u;
        const unsigned long before = ring_stores;
        run(100u);
        /* Nothing more to that channel (the model refuses it), the other
         * keyed off, no more audio written. */
        CHECK(!voice[pair[!side]].active && ring_stores == before && O.takes == 1u &&
            O.phase == KUI_TOY_CDDA_OUT_SILENT && O.why == KUI_TOY_CDDA_OUT_WHY_TAKE && O.why_detail == 1u << side);
        CHECK(reg[pair[side]][0] & 0x4000u);
        /* A silent play is looked at every sixtieth of a second, no more. */
        beat(14000u);
        const unsigned long looked = chip_reads;
        beat(6000u);
        CHECK(chip_reads == looked);
        beat(7100u);
        CHECK(chip_reads > looked);
    }
    /* Another address on a channel with its record still free: the driver's. */
    fresh();
    manatee1(false);
    play(1000u, 4000u, 1u, 0);
    run(1000u);
    reg[pair[1]][0] = 0x4203u;
    strict = false;
    run(100u);
    CHECK(O.why == KUI_TOY_CDDA_OUT_WHY_TAKE && O.why_detail == 8u && !voice[pair[0]].active && reg[pair[1]][0] == 0x4203u);
    /* A stop-all keys our channel off: the ring begins again; nine times in
     * one play is given up. */
    fresh();
    manatee2();
    play(1000u, 9000u, 0u, 0);
    run(1000u);
    for(unsigned times = 1; times <= 9u; ++times) {
        CHECK(sounding());
        reg[63][0] &= ~0x4000u;
        strict = false;                /* the right plane has stopped: the pair is no longer whole */
        strobe();
        run(60u);
        strict = true;
        run(1000u);
        CHECK(O.drops == times && !wrong_loud);
    }
    CHECK(silent() && O.why == KUI_TOY_CDDA_OUT_WHY_FLAP && O.restarts == 8u && starts == 9u);
    /* A command for the idle port rewrites its level: put back, and the
     * play goes on. */
    fresh();
    manatee2();
    play(1000u, 9000u, 0u, 0);
    run(1000u);
    reg[62][9] = 0x0000u;
    reg[63][8] = 0x1234u;
    run(40u);
    CHECK(reg[62][9] == 0x0f1fu && !reg[63][8] && O.fixes == 2u && sounding() && !O.restarts && !wrong_frames);
    /* Its pitch, or its loop's end: put back too, but the chip has moved
     * by then (here the right plane went back to its start early), so the
     * ring begins again; nine times in one play is given up. */
    for(unsigned times = 1; times <= 9u; ++times) {
        CHECK(sounding());
        if(times & 1u)reg[63][3] = 0x1000u;
        else reg[62][6] = 0x0800u;
        strict = false;
        run(40u);
        strict = true;
        CHECK(!reg[62][6] && reg[63][3] == O.frames && O.fixes == 2u + times && O.restarts == (times < 9u ? times : 8u));
        run(1000u);
    }
    /* (What the chip played between the driver's write and that heartbeat
     * was not the play's, and was heard.) */
    CHECK(silent() && O.why == KUI_TOY_CDDA_OUT_WHY_FLAP && O.restarts == 8u && starts == 9u);
    /* The driver is loaded again under the play: not one store more. */
    fresh();
    manatee2();
    play(1000u, 9000u, 0u, 0);
    run(1000u);
    CHECK(sounding());
    put32(0xf8u, 0u);
    frozen = true;
    strict = false;
    run(500u);
    CHECK(O.phase == KUI_TOY_CDDA_OUT_SILENT && O.why == KUI_TOY_CDDA_OUT_WHY_ID && !O.family && !O.placed);
    /* Another admitted build in its place is another driver all the same. */
    fresh();
    manatee1(false);
    play(1000u, 9000u, 0u, 0);
    run(1000u);
    CHECK(sounding());
    manatee1(true);
    frozen = true;
    strict = false;
    run(500u);
    CHECK(O.phase == KUI_TOY_CDDA_OUT_SILENT && O.why == KUI_TOY_CDDA_OUT_WHY_ID);
    /* And so is a build of the same version bytes and another date. */
    fresh();
    driver_of(&builds[14]);
    play(1000u, 9000u, 0u, 0);
    run(1000u);
    CHECK(sounding() && O.version == builds[14].version && O.hash == builds[14].hash);
    CHECK(builds[15].version == builds[14].version && builds[15].hash != builds[14].hash);
    driver_of(&builds[15]);
    frozen = true;
    strict = false;
    run(500u);
    CHECK(O.phase == KUI_TOY_CDDA_OUT_SILENT && O.why == KUI_TOY_CDDA_OUT_WHY_ID && !O.family && !O.placed);
    CHECK(O.seen_hash == builds[15].hash);
    /* And one of the same date and other version bytes (no two builds of
     * the corpus are so: the date is authored). */
    fresh();
    manatee1(false);
    play(1000u, 9000u, 0u, 0);
    run(1000u);
    CHECK(sounding() && O.hash == 0x1c1fu);
    manatee1_build(UINT32_C(0x003f0101), "1998.12.0", 0x1c1fu);
    frozen = true;
    strict = false;
    run(500u);
    CHECK(O.phase == KUI_TOY_CDDA_OUT_SILENT && O.why == KUI_TOY_CDDA_OUT_WHY_ID && O.seen_hash == 0x1c1fu);
    /* A channel the driver left keyed on, its port free: keyed off before
     * the ring's key-on, so that both planes start together. */
    fresh();
    manatee2();
    reg[63][0] = 0x4012u;
    voice[63] = (struct voice){true, now, 0u, 0u, 127u, 0};
    play(1000u, 9000u, 0u, 0);
    run(1000u);
    CHECK(sounding() && O.rekeys == 1u && !wrong_frames && O.keys == 1u);
    printf("PASS the driver's part: a taken channel is left alone, a dropped key begins the ring again (given up at the ninth), changed registers are put back and the ring begun again where the play was moved, a new driver ends every store\n");
}
/* The card does not keep up; the bus does not answer. */
static void starved(void) {
    fresh();
    manatee2();
    play(1000u, 9000u, 0u, 0);
    run(1000u);
    CHECK(sounding() && !O.underruns);
    /* The game reads: the card hands nothing on for longer than the ring
     * holds (the largest of 16-bit samples a third of a second, of ADPCM a
     * second and a half). */
    card_idle = true;
    strict = false;
    run(KUI_TOY_CDDA_ADPCM ? 2500u : 500u);
    CHECK(O.underruns >= 1u && !sounding());
    /* What was played past the audio was the ring's last lap or silence,
     * and the play begins again when the card is back. */
    card_idle = false;
    run(300u);
    strict = true;
    const unsigned long wrong_before = wrong_frames;
    run(2000u);
    CHECK(sounding() && wrong_frames == wrong_before && O.restarts >= 1u);
    /* Bytes that do not follow on from the last: not one of them is
     * played, and the ring begins again. */
    fresh();
    manatee1(false);
    play(1000u, 9000u, 0u, 0);
    run(1000u);
    card_skips = true;
    run(1000u);
    CHECK(sounding() && !card_skips && O.restarts == 1u && !O.underruns && !wrong_frames);
    /* Dry in the ring's first lap: what is played past the audio is the
     * cleared ring, not what lay in sound memory. */
    fresh();
    manatee2();
    card_most = 2048u;
    play(1000u, 9000u, 0u, 0);
    for(unsigned beats = 0; O.phase != KUI_TOY_CDDA_OUT_RUN; ++beats) {
        CHECK(beats < 200u);
        beat(4500u);
    }
    /* The card hands nothing on, and the next heartbeat comes a fifth of a
     * second later (in the driver's spare memory, where the ring holds
     * less than that, a seventh): the chip has run past the audio by then. */
    card_idle = true;
    strict = false;
    beat((KUI_TOY_CDDA_SPARE ? 150u : 200u) * (SECOND / 1000u));
    run(200u);
    CHECK(O.underruns == 1u && wrong_frames > 1000u && !wrong_nonzero && silent());
    /* A card slower than the music: begun again and again, never wrong
     * audio at the start of a ring. */
    fresh();
    manatee1(true);
    card_most = 512u;
    strict = false;
    play(1000u, 9000u, 0u, 0);
    run(5000u);
    CHECK(O.underruns >= 3u && card_low > 100u);
    /* A card that hands its bytes on in odd counts, a frame and a unit cut
     * anywhere and the next bytes at an odd address: every frame is the
     * play's all the same. */
    fresh();
    manatee1(false);
    card_chunk = 999u;
    play(1000u, 9000u, 0u, 0);
    run(20000u);
    CHECK(sounding() && !wrong_frames && !O.restarts && heard_frames > 800000u);
    /* And three bytes at a time, so that a unit is made whole by a count's
     * last byte. */
    card_chunk = 3u;
    card_ticks = 0u;
    run(3000u);
    CHECK(sounding() && !wrong_frames && !O.restarts && heard_frames > 930000u);
    /* Sound memory turns so slow under a play that a stretch's 3 ms run
     * out in the middle of a kilobyte's stores: the stores end there (no
     * access of the bus comes later than a burst behind the stretch's
     * time), the bytes that did not reach the ring start the play over,
     * and it is not keyed on again. The ring that began again could not
     * key its channels off in the same heartbeat, the stretch being over:
     * the next one does, before it primes anything. */
    fresh();
    manatee2();
    play(1000u, 9000u, 0u, 0);
    run(1000u);
    CHECK(sounding() && !O.timeouts && O.keys == 1u);
    store_ticks = 300u;
    strict = false;
    beat(4500u);
    CHECK(O.timeouts == 1u && O.restarts == 1u && O.keyed && sounding());
    beat(4500u);
    CHECK(!O.keyed && silent());
    run(2000u);
    CHECK(O.timeouts >= 3u && O.restarts >= 3u && O.keys == 1u && silent());
    /* The FIFO never empties: nothing is read or written, and it ends. */
    fresh();
    manatee2();
    fifo_stuck = true;
    play(1000u, 4000u, 1u, 0);
    run(200u);
    CHECK(!stores && !chip_reads && O.timeouts > 5u);
    /* One heartbeat waits 3 ms of TMU0 for it and no longer; with TMU0
     * standing still, 40,000 polls. */
    uint64_t began = now;
    beat(14000u);
    CHECK(now - began - 14000u <= 2344u + 8u && now - began - 14000u >= 2344u);
    fresh();
    manatee2();
    play(1000u, 9000u, 0u, 0);
    run(1000u);
    CHECK(sounding());
    fifo_stuck = true;
    timer_stopped_at = now;
    tstr = 0u;
    const unsigned long polled = fifo_polls;
    beat(14000u);
    CHECK(fifo_polls - polled == 40000u && O.why == KUI_TOY_CDDA_OUT_WHY_CLOCK);
    tstr = 1u;
    /* A torn word is read until two reads agree. */
    fresh();
    manatee2();
    torn_address = SOUND + 0x20u;
    torn_left = 5u;
    play(1000u, 4000u, 1u, 0);
    run(1000u);
    CHECK(sounding() && !torn_left && !wrong_frames);
    torn_address = 0u;
    printf("PASS a card that hands nothing on, a slow card, a bus that does not answer, a word being written\n");
}
/* VOLUME: the levels follow the CD mixers; without it, full level. */
static void volume(void) {
    fresh();
    manatee2();
    mixer[0] = 0x0a1fu;
    mixer[1] = 0x0a0fu;
    play(1000u, 9000u, 0u, 0);
    run(1000u);
#if KUI_TOY_CDDA_VOLUME
    CHECK(reg[62][9] == 0x0a1fu && reg[63][9] == 0x0a0fu);
    /* One level for both planes, the larger of the two; the registers'
     * pans and other bits are not the planes'. As POD SpeedZone had them
     * on the console: */
    mixer[0] = 0x0a00u;
    mixer[1] = 0u;
    run(100u);
    CHECK(reg[62][9] == 0x0a1fu && reg[63][9] == 0x0a0fu && !O.fixes);
    mixer[0] = 0xf005u;
    mixer[1] = 0x030fu;
    run(100u);
    CHECK(reg[62][9] == 0x031fu && reg[63][9] == 0x030fu && !O.fixes && O.mixer[0] == 0xf005u);
    /* No level in either: silence. */
    mixer[0] = 0x001fu;
    mixer[1] = 0x000fu;
    run(100u);
    CHECK(!reg[62][9] && !reg[63][9]);
    /* Mixers that read nothing: MANATEE 2's own level bytes after eight
     * heartbeats, silence until then and where they are zero. */
    mute_readback = true;
    put32(0x12470u, UINT32_C(0x00400040));
    beat(14000u);
    CHECK(!reg[62][9] && !reg[63][9]);
    run(300u);
    CHECK(reg[62][9] == 0x0f1fu && reg[63][9] == 0x0f0fu && O.fallbacks && O.shadow == UINT32_C(0x00400040));
    /* One of the two bytes is enough; none is silence. */
    put32(0x12470u, UINT32_C(0x00400000));
    run(100u);
    CHECK(reg[62][9] == 0x0f1fu && reg[63][9] == 0x0f0fu);
    put32(0x12470u, UINT32_C(0x00000040));
    run(100u);
    CHECK(reg[62][9] == 0x0f1fu && reg[63][9] == 0x0f0fu);
    put32(0x12470u, UINT32_C(0x1f001f00));
    run(100u);
    CHECK(!reg[62][9] && !reg[63][9]);
    /* MANATEE 1 keeps no such bytes: silence. */
    fresh();
    manatee1(false);
    put32(0x12470u, UINT32_C(0x00400040));
    mute_readback = true;
    play(1000u, 9000u, 0u, 0);
    run(1000u);
    CHECK(sounding() && !reg[61][9] && !reg[62][9] && !loud_frames);
    printf("PASS VOLUME: both planes at the larger of the CD mixers' two levels, each on its side; MANATEE 2's level bytes where the mixers read nothing\n");
#else
    CHECK(reg[62][9] == 0x0f1fu && reg[63][9] == 0x0f0fu);
    mixer[0] = mixer[1] = 0u;
    run(100u);
    CHECK(reg[62][9] == 0x0f1fu && reg[63][9] == 0x0f0fu);
    printf("PASS without VOLUME: full level, hard left and right, whatever the mixers say\n");
#endif
}
/* The terminal report, another play, the clock lost in play. */
static void endings(void) {
    fresh();
    manatee2();
    play(1000u, 9000u, 0u, 0);
    run(1000u);
    CHECK(sounding());
    const unsigned long calls = card_calls;
    since_empty = 8u;
    kui_toy_cdda_output_terminal();
    CHECK(silent() && !O.keyed && sr == REST && card_calls == calls && !reg[62][9] && !reg[63][9]);
    /* (The levels the planes played at are kept for the report.) */
    CHECK(O.level_last[0] == 0x0f1fu && O.level_last[1] == 0x0f0fu && !O.level_set[0] && !O.level_set[1]);
    /* A channel that names another address by then is the driver's: it is
     * left as it is, and the other keyed off. */
    fresh();
    manatee2();
    play(1000u, 9000u, 0u, 0);
    run(1000u);
    reg[63][0] = 0x4203u;
    reg[63][9] = 0x0a05u;
    strict = false;
    since_empty = 8u;
    kui_toy_cdda_output_terminal();
    CHECK(!voice[62].active && voice[63].active && reg[63][0] == 0x4203u && reg[63][9] == 0x0a05u && !reg[62][9] && !O.keyed);
    /* Less of the worker's stack below the caller than the output needs:
     * nothing is done, at a heartbeat or at the terminal, and it is counted. */
    fresh();
    manatee2();
    play(1000u, 9000u, 0u, 0);
    run(1000u);
    CHECK(sounding() && !O.skipped_stack);
    stack_room = KUI_TOY_CDDA_OUTPUT_STACK - 4u;
    const unsigned long held_stores = stores, held_reads = chip_reads + g2_reads;
    beat(14000u);
    since_empty = 8u;
    kui_toy_cdda_output_terminal();
    CHECK(sounding() && stores == held_stores && chip_reads + g2_reads == held_reads && O.skipped_stack == 1u);
    stack_room = KUI_TOY_CDDA_OUTPUT_STACK;
    beat(14000u);
    CHECK(chip_reads + g2_reads > held_reads && O.skipped_stack == 1u);
    stack_room = 2048u;
    /* With SR.BL set there, nothing is touched. */
    fresh();
    manatee2();
    play(1000u, 9000u, 0u, 0);
    run(1000u);
    sr = REST | BL;
    const unsigned long before = stores, reads = chip_reads, beats = O.heartbeats;
    kui_toy_cdda_output_terminal();
    CHECK(sounding() && stores == before && chip_reads == reads && O.skipped_bl == 1u && O.heartbeats == beats);
    sr = REST;
    /* Another play begins: the first one's ring ends, the new one's begins
     * at its own place. */
    play(4151u, 7000u, 1u, 20);
    run(1000u);
    CHECK(sounding() && starts >= 2u && heard_play.base >= 4171u && heard_play.base < 4260u && !wrong_frames);
    /* The timer leaves its profile: keyed off, and no play followed. */
    tstr = 0u;
    run(100u);
    CHECK(silent() && O.why == KUI_TOY_CDDA_OUT_WHY_CLOCK);
    tstr = 1u;
    /* The drive's play ends (the game asks, and the player sees its last
     * sector passed) while the ring still holds its last frames: they are
     * played to the end, then nothing. */
    fresh();
    manatee2();
    play(1000u, 1300u, 1u, 0);
    run(3950u);
    CHECK(sounding() && O.finished);
    D.state = KUI_TOY_CDDA_ENDED;
    ++D.generation;
    run(1000u);
    CHECK(silent() && !wrong_frames && !cut_frames && last_heard + 1u >= (1300u - heard_play.base) * 588u &&
        heard_play.base < 1020u && O.phase == KUI_TOY_CDDA_OUT_OFF);
    /* The same where the ring began late and is small, so that the drive
     * ends before the last sectors are asked for: what is in the ring is
     * the play's last, and the card is not asked again. */
    fresh();
    manatee2();
    /* (A ring of less than a fifth of a second of 16-bit samples, begun a
     * quarter of a second late; of ADPCM, a third of a second and four
     * fifths.) */
    bank(0u, 0x18000u, 0x200000u - 0x18000u - (KUI_TOY_CDDA_ADPCM ? 20000u : 40000u));
    card_most = KUI_TOY_CDDA_ADPCM ? 100u : 300u;
    play(1000u, 1300u, 1u, 0);
    for(unsigned beats = 0; O.phase != KUI_TOY_CDDA_OUT_RUN; ++beats) {
        CHECK(beats < 400u);
        beat(4500u);
    }
    card_most = 8192u;
    CHECK(O.frames == (KUI_TOY_CDDA_SPARE ? SPARE_FRAMES : sizes[KUI_TOY_CDDA_ADPCM ? 2 : 1]) && heard_play.base < 1010u);
    while(tcnt() != D.base_counter - 4010u * (SECOND / 1000u) && (uint32_t)(D.base_counter - tcnt()) < 4010u * (SECOND / 1000u))beat(4500u);
    CHECK(sounding() && !O.finished && !O.underruns);
    D.state = KUI_TOY_CDDA_ENDED;
    ++D.generation;
    const unsigned long asked = card_calls - card_nones;
    run(1000u);
    CHECK(silent() && !wrong_frames && cut_frames && card_calls - card_nones == asked && O.phase == KUI_TOY_CDDA_OUT_OFF);
    /* A play that is over by the drive's clock before the output looks. */
    fresh();
    manatee2();
    play(1000u, 1100u, 1u, 0);
    now += 3u * SECOND;
    run(300u);
    CHECK(!key_ons && O.phase == KUI_TOY_CDDA_OUT_SILENT && !O.why && !ring_stores);
    printf("PASS the terminal key-off, a new play over an old one, a lost clock, a play already over\n");
}
/* A play's last frames, whatever the driver's count stands at when the
 * channels are keyed on and when the play ends. The count is true to one
 * count either way, so the chip may be a count short of what it says: a
 * play is keyed off only when the count stands a count and the edge beyond
 * its last frame. Short plays, each begun a third of a millisecond later
 * than the one before: every phase of a count at key-on, and of a
 * heartbeat's look at the end. */
static void last_frames(void) {
    unsigned long least = ~0ul, most = 0ul;
    for(unsigned shift = 0; shift < 150u; ++shift) {
        fresh();
        if(shift & 1u)manatee1(false);
        else manatee2();
        now += 261u * shift;
        play(1000u, 1040u, 1u, 0);
        for(unsigned beats = 0; O.phase != KUI_TOY_CDDA_OUT_SILENT; ++beats) {
            CHECK(beats < 1000u);
            beat(2000u + 7u * shift);
        }
        const uint64_t frames = (uint64_t)(1040u - heard_play.base) * 588u;
        CHECK(silent() && !O.why && !wrong_frames && !cut_frames && !O.underruns && !O.clock_faults && key_ons == 2u &&
            heard_play.base < 1010u);
        /* Every frame of the play, and no more than the margin, a count and
         * a look's wait of silence behind it. */
        CHECK(last_heard + 1u >= frames && last_heard + 1u < frames + 1500u);
        const unsigned long over = (unsigned long)(last_heard + 1u - frames);
        if(over < least)least = over;
        if(over > most)most = over;
    }
    printf("PASS a play's last frames at 150 phases of the driver's count and of the heartbeats: every one played, and "
        "%lu to %lu frames of silence behind them before the key-off\n", least, most);
}

int main(void) {
    (void)sizes;
    for(unsigned build = 0; build < 3u; ++build)plays_through(build, false);
    if(KUI_TOY_CDDA_SPARE)
        for(unsigned build = 0; build < 3u; ++build)plays_through(build, true);
    pieces();
    drifting(2450);
    drifting(-6000);
    clocked();
    framed();
    families();
    refused();
    placing();
    contested();
    starved();
    volume();
    endings();
    last_frames();
    printf("CD-music output: %lu checks passed (VOLUME %d, ADPCM %d, SPARE %d); host model only\n", checks, KUI_TOY_CDDA_VOLUME,
        KUI_TOY_CDDA_ADPCM, KUI_TOY_CDDA_SPARE);
    return 0;
}
