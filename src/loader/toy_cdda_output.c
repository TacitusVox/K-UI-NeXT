/* SPDX-License-Identifier: GPL-3.0-only */
/* Test 85 (OUTPUT, VOLUME): the CD music of Test 84's drive, heard
 * (docs/cdda-background-test-85.md).
 *
 * While the drive plays, the worker's heartbeat asks the card for the play's
 * audio (include/kui/toy_cdda_card.h), copies it into a ring in sound memory
 * and has two of the sound chip's channels loop that ring: the left samples
 * on one, the right on the other. It runs after the survey, under the survey's
 * rules for the bus (src/loader/toy_cdda_survey.c): TMU0 in the Toy profile,
 * nothing with SR.BL set, nothing while a G2 DMA channel is enabled or
 * running, the interrupt mask at 15 throughout, the G2 FIFO empty before
 * every burst of at most eight accesses, a word the sound CPU may be writing
 * read until two reads agree, and each stretch of bus work within 3 ms of
 * TMU0. With no play it touches nothing.
 *
 * Whose channels and whose memory. Nothing here is the loader's by right: the
 * game's sound driver owns the chip. Output is given only where the driver
 * running is one of the builds whose records were read and whose two
 * candidate channels a console survey found free all through play (Test 84):
 * - MANATEE 2.00.09 (ChuChu Rocket): the top two stream ports, channels 62
 *   and 63, free while their port records' playing bits are clear;
 * - MANATEE 1.01.3b (The King of Fighters Dream Match 1999) and 1.01.37 (POD
 *   SpeedZone): the channel pool's entries 13 and 14, channels 61 and 62,
 *   free while their owner words are zero.
 * The build is known by its version bytes, its build date's text and its
 * running mark, read again at every heartbeat. The ring lies at or above
 * 0x18000, where the driver's own memory ends, clear of every address and
 * size pair of the driver's bank table (0x14000..0x1429F: the game's banks,
 * stream rings and effect memory), with a guard line before and after it.
 * A place below 0x1DFE00 is taken first: from there up a game's CRI library
 * registers its stream slots in that table when it opens them. Before a
 * heartbeat writes anything, the driver, the channels' records, the whole
 * table and the guards are read again; whatever no longer holds silences the
 * play, with its reason kept for the report, and what is no longer ours is
 * not written again. A ring that is lost alone (a pair of the table came to
 * lie in it, a guard line was overwritten) is given up, and the play takes
 * a place again and begins there.
 *
 * What is written: the ring and its guards, and registers +0x00 to +0x44 of
 * the two channels. Nothing else: no mixer, no monitor select (the driver's),
 * no other channel. Starting and stopping keys the two channels on and off
 * with one execute strobe, as the drivers themselves do at every note. A
 * key-on starts only a channel that is keyed off, so the two are keyed off
 * before every start.
 *
 * The loop. The chip goes back to the loop's start when its position reaches
 * the loop end register, so a plane of N frames loops with that register at
 * N (the adapters' N - 1 leaves the last frame out of every lap, which they
 * do not notice: they follow the chip's position). Should a chip play the
 * frame at N as well, it finds the plane's first frame there once more: each
 * plane is followed by two frames that repeat its first two.
 *
 * The clock is the sound chip's own, as the driver counts it: the chip's
 * position in the ring belongs to the driver, but its timer interrupt counts
 * 4 ms of the chip's time, 176.4 frames, in a word of its own (0x12418 under
 * MANATEE 2, 0x13418 under MANATEE 1; 250 to a second of 44,100 frames,
 * exactly: it reloads its timer for 44 frames, leaves out every 441st and
 * counts every fourth). The count is read at key-on and at every heartbeat:
 * the chip has played their difference, a count more or less. TMU0 is not
 * that clock: on the console it runs a quarter of a percent slow against the
 * chip (Test 86), 108 frames a second. It is kept beside the count as a
 * witness: a count that stands still or runs away from TMU0 begins the ring
 * again, as a key dropped does. The play starts over from the drive's
 * position when the driver keyed a channel off or when the card did not keep
 * up.
 *
 * ADPCM (Test 86): the ring holds the chip's 4-bit ADPCM, which the worker
 * makes of the card's samples: a quarter of the memory and of the bus
 * traffic of 16-bit samples, eight frames a word. It is keyed in the format
 * whose decoder runs on across the loop (PCMS 3) and begins at key-on from
 * signal 0 and step 127; the coder keeps each plane's decoder as the chip's
 * will stand behind the codes written, and begins with the ring.
 *
 * SPARE (Test 86): the ring lies in the driver's own memory, in the end of
 * it that no build of the driver uses, and the bank table is not asked:
 * 0x16000..0x17FFF under MANATEE 2 (8,128 frames a plane of ADPCM),
 * 0x15000..0x17FFF under MANATEE 1 (12,224). The driver's work areas end
 * below it, and it tells the game that free memory begins at 0x18000; its
 * start-up, and its start again on the game's command, clear part or all
 * of it, which the guard lines show.
 *
 * VOLUME: the two channels play at the level the game's driver gave the CD
 * input's mixers (0xa0702040 and 0xa0702044, which the loader only reads):
 * the larger of the two, the left plane hard left and the right plane hard
 * right. Without VOLUME they play at full level, as the two CD-music
 * adapters do.
 *
 * The driver facts are static readings of the drivers
 * (docs/cd-audio-corpus.md); that these two channels and these three builds
 * leave them alone in play is Test 84's console survey. */
#include <stdbool.h>
#include <stdint.h>
#include "kui/toy_cdda_card.h"
#include "kui/toy_cdda_output.h"
#include "kui/toy_cdda_player.h"
#include "kui/toy_cdda_survey.h"
#include "kui/toy_pilot_clock.h"

#if !KUI_TOY_CDDA_OUTPUT
#error "The CD-music output is Test 85's (KUI_TOY_CDDA_OUTPUT)"
#endif
#ifndef KUI_TOY_CDDA_VOLUME
#define KUI_TOY_CDDA_VOLUME 0
#endif
#ifndef KUI_TOY_CDDA_ADPCM
#define KUI_TOY_CDDA_ADPCM 0
#endif
#ifndef KUI_TOY_CDDA_SPARE
#define KUI_TOY_CDDA_SPARE 0
#endif
#if KUI_TOY_CDDA_SPARE && !KUI_TOY_CDDA_ADPCM
#error "The driver's spare memory holds a ring of ADPCM only (KUI_TOY_CDDA_ADPCM)"
#endif

struct kui_toy_cdda_output kui_toy_cdda_output;
#define O kui_toy_cdda_output
#define D kui_toy_cdda_drive

#define SOUND UINT32_C(0xa0800000)
#define CHANNELS UINT32_C(0xa0700000)
#define MIXER UINT32_C(0xa0702040)
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
#define POLL_LIMIT 40000u
#define TICK_LIMIT 2344u          /* 3 ms of TMU0 at 781,250 Hz */
#define CADENCE 13021u            /* a sixtieth of a second: every heartbeat that far apart looks */
#define SPACING 1628u             /* 2 ms: no two closer, however much the ring wants */
#define BURST 8u
#define EXEC UINT32_C(0x43455845) /* 'EXEC' */
#define SEGA UINT32_C(0x41474553) /* 'SEGA' */
#define TABLE UINT32_C(0x14000)   /* the driver's bank table: 84 pairs of address and size */
#define TABLE_PAIRS 84u
#define MEMORY UINT32_C(0x200000)
#define LOWEST UINT32_C(0x18000)  /* the driver's own memory ends here */
/* Test 86 (SPARE): the end of the driver's own memory, which no build of it
 * uses: under MANATEE 2 from 0x16000 (the host's MIDI ring ends below it),
 * under MANATEE 1 from 0x15000 (the command history ends below it). */
#define SPARE_GEN2 UINT32_C(0x16000)
#define SPARE_GEN1 UINT32_C(0x15000)
#define CRI UINT32_C(0x1dfe00)    /* CRI's stream slots begin here */
#define GUARD_BYTES 32u
#if KUI_TOY_CDDA_ADPCM
/* Test 86 (ADPCM): a plane holds the chip's 4-bit ADPCM, eight frames a
 * word, in the format whose decoder runs on across the loop (PCMS 3). */
#define UNIT 8u                   /* frames a word of a plane holds */
#define PLANE_REPEAT 0u
#define KEY_FORMAT 0x0180u
#define WIPE UINT32_C(0x80808080) /* a step up, a step down: silence at any step */
#else
#define UNIT 2u
#define PLANE_REPEAT 4u           /* bytes after a plane that repeat its first two frames */
#define KEY_FORMAT 0u
#define WIPE 0u
#endif
#define GUARD UINT32_C(0x4b434452)
#define SECTOR_BYTES 2352u
#define PRIME_FRAMES 4410u        /* a tenth of a second */
#define WATER_FRAMES 2646u        /* 60 ms */
#define PIECE_FRAMES 256u         /* room worth a heartbeat of its own: a kilobyte */
#define FILL_FRAMES 2048u         /* frames of the card's a heartbeat writes at most */
/* Frames of silence a heartbeat writes at most. As ADPCM silence costs
 * what the card's frames cost, and a heartbeat writes as much of it: a gap
 * between two tracks is two seconds, and a game's heartbeats come a frame
 * of video apart, 737 frames of sound. Of 16-bit samples, a store a frame:
 * a third of a heartbeat's 3 ms. */
#if KUI_TOY_CDDA_ADPCM
#define QUIET_FRAMES FILL_FRAMES
#else
#define QUIET_FRAMES 512u
#endif
#define WIPE_WORDS 1024u          /* words of a new ring cleared a heartbeat */
#define EDGE 64u
/* The driver's count of the chip's time: 4 ms, 882 frames in five counts;
 * COUNT_FRAMES is one count's frames, rounded up. */
#define COUNT_GEN2 UINT32_C(0x12418)
#define COUNT_GEN1 UINT32_C(0x13418)
#define COUNT_FRAMES 177u
#define REARMS 8u
#define KEY_ON 0x4200u            /* KYONB, loop */
#define KEY_BIT 0x4000u
#define KEY_EXECUTE 0x8000u
#define LEVEL_LEFT 0x0f1fu
#define LEVEL_RIGHT 0x0f0fu

#ifdef KUI_TOY_CDDA_OUTPUT_HOST_TEST
extern uint32_t kui_toy_cdda_output_test_read(uint32_t address, unsigned width);
extern void kui_toy_cdda_output_test_write(uint32_t address, uint32_t value);
extern uint32_t kui_toy_cdda_output_test_sr(void);
extern void kui_toy_cdda_output_test_set_sr(uint32_t value);
#define read32(a) kui_toy_cdda_output_test_read((a), 4u)
#define read16(a) kui_toy_cdda_output_test_read((a), 2u)
#define read8(a) kui_toy_cdda_output_test_read((a), 1u)
#define write32 kui_toy_cdda_output_test_write
#define sr_read kui_toy_cdda_output_test_sr
#define sr_write kui_toy_cdda_output_test_set_sr
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
static void write32(uint32_t address, uint32_t value) {
    *(volatile uint32_t *)(uintptr_t)address = value;
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

/* The builds given output: the version word (sound memory 0x20), the
 * 16-bit hash of the build date's text as the survey takes it, the family. */
static const struct profile { uint32_t version; uint16_t date_hash; uint8_t family; } profiles[] = {
    {UINT32_C(0x00090002), 0xc46du, KUI_TOY_CDDA_FAMILY_GEN2}, /* MANATEE 2.00.09, 1999.06.16 */
    {UINT32_C(0x003b0101), 0x1c1fu, KUI_TOY_CDDA_FAMILY_GEN1}, /* MANATEE 1.01.3b, 1998.12.08 */
    {UINT32_C(0x00370101), 0xe50fu, KUI_TOY_CDDA_FAMILY_GEN1}  /* MANATEE 1.01.37, 1998.11.02 */
};
#if KUI_TOY_CDDA_SPARE
#elif KUI_TOY_CDDA_ADPCM
static const uint16_t plane_frames[] = {65472u, 32768u, 16384u};
#else
static const uint16_t plane_frames[] = {16384u, 8192u, 4096u};
#endif

/* One heartbeat's use of the bus: when it began, the start of the stretch
 * being timed (TMU0 counts down: ~TCNT0 grows), its polls, its accesses in
 * bursts, and whether it may go on. */
struct bus { uint32_t began, start, polls, burst; bool ok; };
static bool budget(struct bus *b) {
    if(b->ok && (b->polls >= POLL_LIMIT || ~read32(TCNT0) - b->start >= TICK_LIMIT))b->ok = false;
    return b->ok;
}
static bool drain(struct bus *b) {
    while(budget(b)) {
        ++b->polls;
        if(!(read32(FIFO) & FIFO_MASK))return true;
    }
    return false;
}
/* The next access of a burst: the FIFO empty before each eight. */
static bool turn(struct bus *b) {
    if(!(b->burst & (BURST - 1u)) && !drain(b))return false;
    ++b->burst;
    return b->ok;
}
/* A word only the stopped game writes: read once. */
static bool once(struct bus *b, uint32_t address, uint32_t *value) {
    if(!turn(b))return false;
    *value = read32(address);
    return true;
}
/* A word the sound CPU may be writing: two reads that agree. */
static bool stable(struct bus *b, uint32_t address, uint32_t *value) {
    while(drain(b)) {
        const uint32_t first = read32(address), second = read32(address);
        b->burst = 0u;
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
/* The ring: a guard line, the left plane and its repeat, the right plane and
 * its repeat, a guard line. A plane's bytes: two a frame as 16-bit samples,
 * half a byte as ADPCM. */
static uint32_t plane_bytes(uint32_t frames) {
    return frames * 4u / UNIT;
}
static uint32_t ring_size(uint32_t frames) {
    return 2u * GUARD_BYTES + 2u * (plane_bytes(frames) + PLANE_REPEAT);
}
static uint32_t ring_bytes(void) {
    return ring_size(O.frames);
}
static uint32_t plane(unsigned side) {
    return O.ring + GUARD_BYTES + side * (plane_bytes(O.frames) + PLANE_REPEAT);
}
static uint32_t channel(unsigned side) {
    return CHANNELS + 0x80u * O.channel[side];
}
/* The only places a store of this file may reach: the placed ring with its
 * guards, and registers +0x00..+0x44 of the two channels. */
static bool put(struct bus *b, uint32_t address, uint32_t value) {
    bool ours = O.placed && address - (SOUND + O.ring) < ring_bytes();
    for(unsigned side = 0; side < 2u; ++side)
        if(O.channel[side] && address - channel(side) <= 0x44u)ours = true;
    if(!ours || (address & 3u)) {
        ++O.faults;
        b->ok = false;
        return false;
    }
    if(!turn(b))return false;
    write32(address, value);
    return true;
}

/* 44,100 frames a second on TMU0's 781,250 ticks: 882 frames in 15,625. */
static uint32_t frames_of(uint32_t ticks) {
    return ticks / 15625u * 882u + ticks % 15625u * 882u / 15625u;
}
/* The frames of that many of the driver's counts: 882 in five. */
static uint32_t frames_counted(uint32_t counts) {
    return counts / 5u * 882u + counts % 5u * 882u / 5u;
}
/* The chip is within a count of what the driver's counts say, either way,
 * and the edge is for what a heartbeat itself takes. */
#define MARGIN (COUNT_FRAMES + EDGE)
/* And as far as the count and TMU0 may differ, beside a sixty-fourth of
 * the time: a count at either end. */
#define SLACK (2u * COUNT_FRAMES + EDGE)
static uint32_t apart(uint32_t a, uint32_t b) {
    return a > b ? a - b : b - a;
}
static uint32_t count_at(void) {
    return O.family == KUI_TOY_CDDA_FAMILY_GEN2 ? COUNT_GEN2 : COUNT_GEN1;
}

/* The driver running now: 1 the admitted one (with none admitted yet: any
 * of the builds, which is then admitted), 0 another, or none running, with
 * the reason; -1 the bus. */
static int driver(struct bus *b, uint32_t *why) {
    uint32_t reset, version, state, date, hash = UINT32_C(0x811c9dc5);
    if(!stable(b, ARM_RESET, &reset))return -1;
    if(reset & 1u) {
        *why = KUI_TOY_CDDA_OUT_WHY_ARM;
        return 0;
    }
    if(!sound(b, 0x20u, &version))return -1;
    for(uint32_t at = 0x50u; at < 0x5cu; at += 4u) {
        if(!sound(b, at, &date))return -1;
        for(unsigned i = 0; i < 4u; ++i)hash = (hash ^ (date >> 8 * i & 255u)) * UINT32_C(0x01000193);
    }
    hash = (hash ^ hash >> 16) & 0xffffu;
    if(!sound(b, 0xf8u, &state))return -1;
    O.seen_version = version;
    O.seen_hash = hash;
    *why = O.family ? KUI_TOY_CDDA_OUT_WHY_ID : KUI_TOY_CDDA_OUT_WHY_DRIVER;
    for(unsigned i = 0; i < sizeof(profiles) / sizeof(profiles[0]); ++i) {
        const struct profile *p = &profiles[i];
        if(version != p->version || hash != p->date_hash ||
           state != (p->family == KUI_TOY_CDDA_FAMILY_GEN2 ? EXEC : SEGA))continue;
        if(O.family)return version == O.version;
        O.version = version;
        O.family = p->family;
        const bool top = p->family == KUI_TOY_CDDA_FAMILY_GEN2;
        O.channel[0] = top ? 62u : 61u;
        O.channel[1] = top ? 63u : 62u;
        return 1;
    }
    return 0;
}
/* The two channels by the driver's own records: 1 read, with whether the
 * game uses each; 0 the records do not prove them; -1 the bus. */
static int records(struct bus *b, bool busy[2]) {
    uint32_t word, other;
    busy[0] = busy[1] = true;
    if(O.family == KUI_TOY_CDDA_FAMILY_GEN2) {
        /* The split bytes: MIDI voices, one-shot ports, stream ports, 64 in
         * all, the stream ports the top channels from the base word on. */
        if(!sound(b, 0x12060u, &word) || !sound(b, 0x12068u, &other))return -1;
        const uint32_t streams = word >> 16 & 255u;
        if(streams < 2u || streams > 16u || (word & 255u) + (word >> 8 & 255u) + streams != 64u ||
           other != UINT32_C(0x00800000) + 0x80u * (64u - streams))return 0;
        for(unsigned side = 0; side < 2u; ++side) {
            if(!sound(b, 0x13800u + 0x40u * (streams - 2u + side), &word))return -1;
            busy[side] = word & 1u;
        }
        return 1;
    }
    bool used[2];
    for(unsigned side = 0; side < 2u; ++side) {
        const uint32_t entry = 13u + side, at = 0x12080u + 12u * entry;
        if(!sound(b, at + 8u, &word) || !sound(b, at + 4u, &other))return -1;
        if(word != UINT32_C(0x00801800) + 0x80u * entry)return 0;
        used[side] = other != 0u;
    }
    busy[0] = used[0];
    busy[1] = used[1];
    return 1;
}
#if !KUI_TOY_CDDA_SPARE
/* The driver's bank table against [at, at + bytes): 1 no pair reaches into
 * it; 0 one does, which begins at *below; -2 a pair this reading does not
 * explain, *below its index; -1 the bus. */
static int table(struct bus *b, uint32_t at, uint32_t bytes, uint32_t *below) {
    for(uint32_t pair = 0; pair < TABLE_PAIRS; ++pair) {
        uint32_t address, size;
        if(!once(b, SOUND + TABLE + 8u * pair, &address) ||
           !once(b, SOUND + TABLE + 8u * pair + 4u, &size))return -1;
        if(!size)continue;
        if(address >= MEMORY) {
            *below = pair;
            return -2;
        }
        if(size > MEMORY - address)size = MEMORY - address;
        if(address < at + bytes && address + size > at) {
            *below = address;
            return 0;
        }
    }
    return 1;
}
#endif
static bool guards(struct bus *b, bool store) {
    for(unsigned line = 0; line < 2u; ++line) {
        const uint32_t at = SOUND + O.ring + line * (ring_bytes() - GUARD_BYTES);
        for(uint32_t word = 0; word < GUARD_BYTES; word += store ? 4u : GUARD_BYTES - 4u) {
            uint32_t value;
            if(store) {
                if(!put(b, at + word, GUARD))return false;
            } else if(!once(b, at + word, &value) || value != GUARD)return false;
        }
    }
    return true;
}
/* The ring takes a place: its guard lines are written. */
static int settle(struct bus *b, uint32_t at, uint32_t frames) {
    O.ring = at;
    O.frames = frames;
    O.placed = 1u;
    O.wiped = 0u;
    ++O.places;
    if(guards(b, true))return 1;
    O.placed = 0u;
    return 0;
}
#if KUI_TOY_CDDA_SPARE
/* The ring's place: the driver's spare memory, the whole of it. The game's
 * own memory is not looked at: what its table leaves free at one moment is
 * where the game may load its next sound data, and the table names that
 * only after it has been loaded. */
static int place(struct bus *b, uint32_t *detail) {
    const uint32_t at = O.family == KUI_TOY_CDDA_FAMILY_GEN2 ? SPARE_GEN2 : SPARE_GEN1;
    (void)detail;
    O.spare = 1u;
    return settle(b, at, LOWEST - at - 2u * GUARD_BYTES);
}
#else
/* A place for the ring, the largest size first, and for each size first
 * below CRI's slots, then anywhere: the highest the table leaves free.
 * 1 placed, 0 still looking, -1 none (the reason's detail in *detail). */
static int place(struct bus *b, uint32_t *detail) {
    for(unsigned scans = 0; scans < 4u; ++scans) {
        if(O.place_try >= 2u * sizeof(plane_frames) / sizeof(plane_frames[0])) {
            *detail = KUI_TOY_CDDA_OUT_RING_NONE;
            return -1;
        }
        const uint32_t frames = plane_frames[O.place_try >> 1];
        const uint32_t bytes = ring_size(frames);
        if(!O.place_at)O.place_at = ((O.place_try & 1u ? MEMORY : CRI) - bytes) & ~UINT32_C(31);
        uint32_t below = 0u;
        const int clear = table(b, O.place_at, bytes, &below);
        if(clear == -1)return 0;
        if(clear == -2) {
            *detail = KUI_TOY_CDDA_OUT_RING_PAIR + below;
            return -1;
        }
        if(clear)return settle(b, O.place_at, frames);
        if(below < LOWEST + bytes) {
            ++O.place_try;
            O.place_at = 0u;
        } else O.place_at = (below - bytes) & ~UINT32_C(31);
    }
    return 0;
}
#endif
/* A ring newly placed holds what lay there. It is cleared before its first
 * use, so that a play the card lets run dry falls silent. */
static bool wipe(struct bus *b) {
    for(uint32_t words = 0; O.wiped < ring_bytes() - 2u * GUARD_BYTES; O.wiped += 4u, ++words)
        if(words == WIPE_WORDS || !put(b, SOUND + O.ring + GUARD_BYTES + O.wiped, WIPE))return false;
    return true;
}

/* A channel's first register as it stands keyed on. */
static uint32_t key_word(unsigned side) {
    return KEY_ON | KEY_FORMAT | (plane(side) >> 16 & 0x7fu);
}
/* A channel's first register: whether it still names the ring's plane (1;
 * 0 another address or no loop: the driver's), and its key bit. -1 the bus. */
static int held(struct bus *b, unsigned side, bool *on) {
    uint32_t word;
    if(!stable(b, channel(side), &word))return -1;
    *on = (word & KEY_BIT) != 0u;
    return ((word ^ key_word(side)) & 0x07ffu) == 0u;
}
/* (The last level written that was one is kept for the report: the terminal
 * key-off writes none.) */
static void level_write(struct bus *b, unsigned side, uint32_t value) {
    if(!put(b, channel(side) + 0x24u, value))return;
    O.level_set[side] = value;
    if(value)O.level_last[side] = value;
}
/* Nothing sounds on the channels named: the level first, then the key, with
 * one execute strobe for both. */
static bool off(struct bus *b, const bool ours[2]) {
    for(unsigned side = 0; side < 2u; ++side)
        if(ours[side])level_write(b, side, 0u);
    for(unsigned side = 0; side < 2u; ++side)
        if(ours[side] && !put(b, channel(side),
            (key_word(side) & ~KEY_BIT) | (side || !ours[1] ? KEY_EXECUTE : 0u)))break;
    return b->ok;
}
/* What was keyed on is keyed off, on the channels that are still ours: free
 * by the driver's records and naming the ring. */
static void quiet(struct bus *b, const bool busy[2]) {
    if(!O.keyed)return;
    bool ours[2], on;
    for(unsigned side = 0; side < 2u; ++side)ours[side] = !busy[side] && held(b, side, &on) == 1;
    if(b->ok && off(b, ours))O.keyed = 0u;
}
static void key_on(struct bus *b) {
    static const uint16_t image[] = {0x1fu, 0x1fu, 0u, 0x4210u, 0u, 0u, 4u,
        0x1fffu, 0x1fffu, 0x1fffu, 0x1fffu, 0x1fffu, 0u, 0u};
    for(unsigned side = 0; side < 2u; ++side) {
        const uint32_t at = channel(side);
        if(!put(b, at + 4u, plane(side) & 0xffffu) || !put(b, at + 8u, 0u) ||
           !put(b, at + 0x0cu, O.frames))return;
        for(unsigned i = 0; i < sizeof(image) / sizeof(image[0]); ++i)
            if(!put(b, at + 0x10u + 4u * i, i == 5u ? O.level[side] : image[i]))return;
        O.level_set[side] = O.level[side];
        if(O.level[side])O.level_last[side] = O.level[side];
    }
    O.keyed = 1u;
    if(!put(b, channel(0), key_word(0)) || !put(b, channel(1), key_word(1) | KEY_EXECUTE))return;
    O.key_tick = read32(TCNT0);
    ++O.keys;
}
/* The levels the two channels should have: the level the game's driver
 * gave the CD input's mixers, or full level; hard left and right. */
static void levels(struct bus *b) {
#if KUI_TOY_CDDA_VOLUME
    uint32_t value[2];
    if(!stable(b, MIXER, &value[0]) || !stable(b, MIXER + 4u, &value[1]))return;
    for(unsigned side = 0; side < 2u; ++side) {
        O.mixer[side] = value[side] & 0xffffu;
    }
    /* One level for both planes, the larger of the two inputs', and each
     * plane on its own side. Which of the two registers is the disc's left
     * is not known (the drivers' own full-level words pan the first right
     * and the second left), and POD SpeedZone on the console gave the first
     * level 10 and the middle and the second none: a plane given its
     * register's word could come out on the wrong side, or not at all. */
    uint32_t level = value[0] & 0x0f00u;
    if((value[1] & 0x0f00u) > level)level = value[1] & 0x0f00u;
    O.level[0] = level ? level | (LEVEL_LEFT & 0x1fu) : 0u;
    O.level[1] = level ? level | (LEVEL_RIGHT & 0x1fu) : 0u;
    if(O.mixer[0] | O.mixer[1]) {
        O.mixer_zero = 0u;
        return;
    }
    /* MANATEE 2 keeps the game's CD level and pan bytes beside the
     * registers. Mixers that read nothing for eight heartbeats while those
     * say a level stand for registers that do not read back. */
    if(O.family != KUI_TOY_CDDA_FAMILY_GEN2 || ++O.mixer_zero < 8u)return;
    O.mixer_zero = 8u;
    if(!sound(b, 0x12470u, &value[0]))return;
    O.shadow = value[0];
    if(!(value[0] & UINT32_C(0x00ff00ff)))return;
    O.level[0] = LEVEL_LEFT;
    O.level[1] = LEVEL_RIGHT;
    ++O.fallbacks;
#else
    (void)b;
    O.level[0] = LEVEL_LEFT;
    O.level[1] = LEVEL_RIGHT;
#endif
}

#if KUI_TOY_CDDA_ADPCM
/* Eight frames of one side as a plane's word: eight codes, the first in
 * the word's lowest four bits, and the side's coder as the chip's decoder
 * will stand behind them. The chip's starts at key-on from signal 0 and
 * step 127; a code moves the signal by (twice its low three bits and one)
 * eighths of the step, down with its top bit set, and multiplies the step
 * by its size's 256ths below. A move of more than 32,767, which the largest
 * steps allow, is not asked for: there the accounts of the chip differ.
 * sample: the side's first, a 16-bit word; the next is two on.
 *
 * Every frame of a play goes through this twice, and the worker's stack and
 * state lie at uncached addresses, each use of them a cycle on the memory
 * bus. The compiler's code for the loop below keeps a dozen values on the
 * stack; written out by hand it takes R0 to R7 and no stack at all, and
 * reads the coder once and writes it once. What the two compute is the
 * same: the host model runs the C, and tests/test_toy_cdda_output_linked.py
 * runs the routine as linked against a coder of its own. */
#ifdef KUI_TOY_CDDA_OUTPUT_HOST_TEST
extern void kui_toy_cdda_output_test_even(const void *address);
static const uint16_t scale[8] = {230u, 230u, 230u, 230u, 307u, 409u, 512u, 614u};
static uint32_t kui_toy_cdda_squeeze(const int16_t *sample, struct kui_toy_cdda_coder *c) {
    kui_toy_cdda_output_test_even(sample);
    int32_t signal = c->signal;
    uint32_t step = c->step, word = 0u;
    for(unsigned shift = 0; shift < 32u; shift += 4u, sample += 2) {
        int32_t delta = *sample - signal;
        uint32_t code = 0u, sign = 0u;
        if(delta < 0) {
            sign = 8u;
            delta = -delta;
        }
        uint32_t rest = (uint32_t)delta, part = step;
        if(rest >= part) {
            code = 4u;
            rest -= part;
        }
        part >>= 1;
        if(rest >= part) {
            code |= 2u;
            rest -= part;
        }
        part >>= 1;
        if(rest >= part)code |= 1u;
        uint32_t move = step * (2u * code + 1u) >> 3;
        while(move > 0x7fffu) {
            --code;
            move = step * (2u * code + 1u) >> 3;
        }
        signal += sign ? -(int32_t)move : (int32_t)move;
        if(signal > 32767)signal = 32767;
        if(signal < -32768)signal = -32768;
        step = step * scale[code] >> 8;
        if(step < 127u)step = 127u;
        if(step > 24576u)step = 24576u;
        word |= (code | sign) << shift;
    }
    c->signal = signal;
    c->step = step;
    return word;
}
#else
uint32_t kui_toy_cdda_squeeze(const int16_t *sample, struct kui_toy_cdda_coder *c);
#define OUT_TEXT2(x) #x
#define OUT_TEXT(x) OUT_TEXT2(x)
#define OUT_NAME(name) OUT_TEXT(__USER_LABEL_PREFIX__) #name
/* R1 the signal, R2 the step, R3 the word (it begins as one bit, which
 * falls out at the top when the eighth code is in), R4 the samples, R5 the
 * coder, R6 the difference and then the code, R7 and R0 at hand. The codes
 * go in at the word's low end, so the first comes to lie at the top: the
 * word's sixteen half-bytes are turned round at the end. */
__asm__(
"    .section .text.kui_toy_cdda_squeeze,\"ax\"\n"
"    .balign 4\n"
"    .global " OUT_NAME(kui_toy_cdda_squeeze) "\n"
"    .type " OUT_NAME(kui_toy_cdda_squeeze) ",@function\n"
OUT_NAME(kui_toy_cdda_squeeze) ":\n"
"    mov.l   @r5,r1\n"
"    mov.l   @(4,r5),r2\n"
"    mov     #1,r3\n"
"1:  mov.w   @r4+,r6\n"          /* the side's next sample */
"    add     #2,r4\n"            /* (the other side's lies between) */
"    sub     r1,r6\n"            /* how far from the signal, and which way */
"    cmp/pz  r6\n"
"    bt.s    2f\n"
"    mov     #0,r0\n"
"    neg     r6,r6\n"
"    mov     #8,r0\n"
"2:  cmp/hs  r2,r6\n"            /* the step, */
"    bf.s    3f\n"
"    mov     r2,r7\n"
"    sub     r2,r6\n"
"    or      #4,r0\n"
"3:  shlr    r7\n"               /* its half, */
"    cmp/hs  r7,r6\n"
"    bf      4f\n"
"    sub     r7,r6\n"
"    or      #2,r0\n"
"4:  shlr    r7\n"               /* its quarter */
"    cmp/hs  r7,r6\n"
"    bf      5f\n"
"    or      #1,r0\n"
"5:  mov     r0,r6\n"            /* the code, kept */
"    and     #7,r0\n"
"6:  mov     r0,r7\n"            /* the move: (twice the size and one) eighths of the step */
"    add     r7,r7\n"
"    add     #1,r7\n"
"    mul.l   r2,r7\n"
"    sts     macl,r7\n"
"    shlr2   r7\n"
"    shlr    r7\n"
"    mov.w   8f,r0\n"            /* none beyond 32,767: a size less */
"    cmp/hi  r0,r7\n"
"    bf      7f\n"
"    add     #-1,r6\n"
"    mov     r6,r0\n"
"    bra     6b\n"
"    and     #7,r0\n"
"7:  mov     r6,r0\n"
"    tst     #8,r0\n"
"    bt      9f\n"
"    neg     r7,r7\n"
"9:  add     r7,r1\n"            /* the signal moves, and stays within 16 bits */
"    exts.w  r1,r7\n"
"    cmp/eq  r7,r1\n"
"    bt      11f\n"
"    mov.w   8f,r7\n"            /* beyond them: the nearest end */
"    cmp/pz  r1\n"
"    bt      10f\n"
"    not     r7,r7\n"
"10: mov     r7,r1\n"
"11: shll2   r3\n"               /* the code into the word; T: that was the eighth */
"    shll    r3\n"
"    shll    r3\n"
"    or      r6,r3\n"
"    mov     r6,r0\n"
"    and     #7,r0\n"
"    add     r0,r0\n"
"    movt    r6\n"
"    mov     r0,r7\n"            /* the step by its size's 256ths, between 127 and 24,576 */
"    mova    14f,r0\n"
"    mov.w   @(r0,r7),r7\n"
"    mul.l   r2,r7\n"
"    sts     macl,r2\n"
"    shlr8   r2\n"
"    mov     #127,r7\n"
"    cmp/hs  r7,r2\n"
"    bt      12f\n"
"    mov     r7,r2\n"
"12: mov.w   15f,r7\n"
"    cmp/hi  r7,r2\n"
"    bf      13f\n"
"    mov     r7,r2\n"
"13: tst     r6,r6\n"
"    bt      1b\n"
"    mov.l   r1,@r5\n"           /* the decoder stands here now */
"    mov.l   r2,@(4,r5)\n"
"    swap.b  r3,r0\n"            /* first code lowest: the bytes turned round, */
"    swap.w  r0,r0\n"
"    swap.b  r0,r0\n"
"    mov.l   16f,r7\n"           /* and the two halves of each */
"    mov     r0,r6\n"
"    and     r7,r6\n"
"    shll2   r6\n"
"    shll2   r6\n"
"    shlr2   r0\n"
"    shlr2   r0\n"
"    and     r7,r0\n"
"    rts\n"
"    or      r6,r0\n"
"    .balign 4\n"
"16: .long   0x0f0f0f0f\n"
"14: .word   230, 230, 230, 230, 307, 409, 512, 614\n"
"8:  .word   32767\n"
"15: .word   24576\n"
"    .size " OUT_NAME(kui_toy_cdda_squeeze) ",.-" OUT_NAME(kui_toy_cdda_squeeze) "\n"
"    .text\n");
#endif
/* Whole units of eight frames from where they lie, at an even address,
 * into the ring at its write position: a word a plane a unit, as many of
 * `units` as the bus's stretch lets through, each `advance` bytes behind
 * the last (none: the same unit again, which is how silence comes). Returns
 * the units stored; the write position and the counts move on by exactly
 * those, and each side's coder with its words. The bus's rule is kept as put() keeps it: the G2
 * FIFO empty, and the stretch's time not run out, before each eight
 * stores, which are four units; and the stores go to the two planes of the
 * placed ring at the write position, nowhere else. A unit's two words are
 * stored together or not at all. */
static __attribute__((noinline)) uint32_t pour(struct bus *b, const uint8_t *from, uint32_t units, uint32_t advance) {
    const uint32_t frames = O.frames, span = plane_bytes(frames), base = SOUND + plane(0u);
    uint32_t at = O.at, done = 0u;
    if(!O.placed || at >= frames || (at & (UNIT - 1u))) {
        ++O.faults;
        b->ok = false;
        return 0u;
    }
    for(; done < units; ++done, from += advance) {
        if(!(done & 3u)) {
            b->burst = 0u;
            if(!drain(b))break;
        }
        const int16_t *unit = __builtin_assume_aligned(from, 2);
        const uint32_t to = base + at / 2u;
        write32(to, kui_toy_cdda_squeeze(unit, &O.coder[0]));
        write32(to + span, kui_toy_cdda_squeeze(unit + 1, &O.coder[1]));
        at += UNIT;
        if(at >= frames)at = 0u;
    }
    /* (Stores since the FIFO was last seen empty: two a unit.) */
    b->burst = done ? 2u * (((done - 1u) & 3u) + 1u) : 0u;
    O.at = at;
    O.written += UNIT * done;
    O.bytes += 4u * UNIT * done;
    return done;
}
#else
/* Units of two frames of 16-bit samples from where they lie into the ring
 * at its write position, a word a plane a unit, with a plane's first two
 * once more behind its last; each `advance` bytes behind the last. Returns
 * the units stored: nothing is counted for one that is not, and it may come
 * again. */
static uint32_t pour(struct bus *b, const uint8_t *from, uint32_t units, uint32_t advance) {
    uint32_t done = 0u;
    for(; done < units; ++done, from += advance) {
        const uint32_t at = O.at * 4u / UNIT;
        const uint32_t word[2] = {from[0] | from[1] << 8 | from[4] << 16 | (uint32_t)from[5] << 24,
            from[2] | from[3] << 8 | from[6] << 16 | (uint32_t)from[7] << 24};
        for(unsigned side = 0; side < 2u; ++side)
            if(!put(b, SOUND + plane(side) + at, word[side]) ||
               (!at && !put(b, SOUND + plane(side) + 2u * O.frames, word[side])))return done;
        O.at += UNIT;
        if(O.at >= O.frames)O.at = 0u;
        O.written += UNIT;
        O.bytes += 4u * UNIT;
    }
    return done;
}
#endif
/* Bytes of frames on their way into the ring, the card's or (no bytes
 * named) silence: whole units are stored, straight from where they lie
 * when none is begun, in one run where they lie at an even address (the
 * card's always do: a sector is 2,352 bytes, a block 512); the rest of one
 * waits for its other bytes. Returns the bytes taken; a unit that could not
 * be stored waits whole. */
static uint32_t feed(struct bus *b, const uint8_t *bytes, uint32_t count) {
    static const _Alignas(4) uint8_t nothing[4u * UNIT];
    uint32_t taken = 0u;
    while(taken < count) {
        if(O.carried == 4u * UNIT) {
            if(!pour(b, O.carry, 1u, 0u))return taken;
            O.carried = 0u;
        }
        if(!O.carried && count - taken >= 4u * UNIT) {
            uint32_t units = (count - taken) / (4u * UNIT), advance = 4u * UNIT;
            const uint8_t *from = nothing;
            /* (Silence: the one unit of it, as often as asked.) */
            if(!bytes)advance = 0u;
            else if(!((uintptr_t)(bytes + taken) & 1u))from = bytes + taken;
            else {
                /* (At an odd address: by way of the unit's own room.) */
                for(unsigned i = 0; i < 4u * UNIT; ++i)O.carry[i] = bytes[taken + i];
                from = O.carry;
                units = 1u;
            }
            const uint32_t done = pour(b, from, units, advance);
            taken += 4u * UNIT * done;
            if(done < units)return taken;
            continue;
        }
        O.carry[O.carried++] = bytes ? bytes[taken] : 0u;
        ++taken;
    }
    if(O.carried == 4u * UNIT && pour(b, O.carry, 1u, 0u))O.carried = 0u;
    return taken;
}
static uint32_t silence(struct bus *b, uint32_t frames) {
    const uint32_t done = feed(b, 0, 4u * frames) / 4u;
    O.silence_frames += done;
    return done;
}
/* The card's bytes, in their order. The card took its time: the bus's
 * stretch starts here. A byte that does not reach the ring would leave
 * every later one in the wrong sample, so the play starts over instead. */
static void take(void *context, const uint8_t *bytes, uint32_t count, uint32_t offset) {
    struct bus *b = context;
    if(O.broken || offset != O.got) {
        O.broken = 1u;
        return;
    }
    b->start = ~read32(TCNT0);
    b->polls = 0u;
    O.got += count;
    if(feed(b, bytes, count) != count)O.broken = 1u;
}
/* The piece of the play that starts at O.sector: one track's sectors to its
 * end or the range's, a sector of a gap between tracks, or nothing more
 * after the last pass. */
static void piece(void) {
    if(O.sector >= D.end) {
        if(O.passes && !--O.passes) {
            O.finished = 1u;
            O.audio_end = O.written;
            return;
        }
        O.sector = D.first;
    }
    uint32_t start, end;
    O.track = kui_toy_cdda_track_of(O.sector, &start, &end);
    O.piece_end = O.track ? (end < D.end ? end : D.end) : O.sector + 1u;
    O.got = 0u;
    O.card = KUI_TOY_CDDA_CARD_IDLE;
    if(!++O.segment)O.segment = 1u;
}
/* Where the drive stands now, by its clock, without moving it: the sector,
 * and the passes it has still to play (0: for ever). False: by its clock
 * the play is over. */
static bool drive_now(uint32_t now) {
    const uint32_t length = D.end - D.first, ticks = D.base_counter - now;
    uint32_t done = D.done;
    int32_t offset = D.base_offset + (int32_t)(ticks / KUI_TOY_CDDA_TICKS_3_SECTORS * 3u +
        ticks % KUI_TOY_CDDA_TICKS_3_SECTORS * 3u / KUI_TOY_CDDA_TICKS_3_SECTORS);
    if(offset < 0)offset = 0;
    for(unsigned laps = 0; (uint32_t)offset >= length; ++laps) {
        if(laps == 4096u)return false;
        offset -= (int32_t)length;
        ++done;
    }
    if(D.passes && done >= D.passes)return false;
    O.sector = D.first + (uint32_t)offset;
    O.passes = D.passes ? D.passes - done : 0u;
    return true;
}

static void stop(struct bus *b, const bool busy[2], uint32_t phase, uint32_t why, uint32_t detail) {
    quiet(b, busy);
    if(why && !O.why) {
        O.why = why;
        O.why_detail = detail;
    }
    O.phase = phase;
}
/* The play starts over from where the drive stands. */
static void restart(struct bus *b, const bool busy[2]) {
    ++O.restarts;
    stop(b, busy, KUI_TOY_CDDA_OUT_OFF, 0u, 0u);
}
/* The card's and the gaps' audio into the ring, as far as there is room;
 * after the play's last frame, silence for as long as the ring still runs. */
static void fill(struct bus *b, uint32_t room, bool low) {
    bool asked = false;
    if(room > FILL_FRAMES)room = FILL_FRAMES;
    while(room >= UNIT && b->ok && !O.broken) {
        const uint32_t quiet = room < QUIET_FRAMES ? room : QUIET_FRAMES;
        if(O.finished) {
            (void)silence(b, quiet);
            return;
        }
        if(O.sector >= O.piece_end) {
            piece();
            continue;
        }
        const uint32_t bytes = (O.piece_end - O.sector) * SECTOR_BYTES;
        if(!O.track || O.card == KUI_TOY_CDDA_CARD_REFUSED) {
            /* A gap, or a track the card does not hold: silence in its
             * place, from the next whole frame on. */
            O.got &= ~UINT32_C(3);
            O.carried &= ~UINT32_C(3);
            /* A stretch of silence, and what it leaves waits for the
             * next heartbeat. */
            const uint32_t left = (bytes - O.got) / 4u, done = silence(b, left < quiet ? left : quiet);
            O.got += 4u * done;
            if(done < left)return;
#if KUI_TOY_CDDA_ADPCM
            /* (A gap goes by a sector a piece: on to the next, as far as
             * the heartbeat's room goes.) */
            room -= done;
#else
            room = room - done < 2u * PIECE_FRAMES ? room - done : 2u * PIECE_FRAMES;
#endif
        } else {
            if(asked)return;
            asked = true;
            /* The card hands on whole card blocks' worth, and none without
             * room for one: it is told the ring's room, not the piece's
             * rest, which it knows. */
            const struct kui_toy_cdda_want want = {O.segment, O.track, O.sector, O.piece_end, low};
            const uint32_t before = O.written;
            O.asked = 1u;
            O.card = kui_toy_cdda_card_heartbeat(&want, 4u * room - O.carried, take, b);
            room -= O.written - before;
            if(O.card == KUI_TOY_CDDA_CARD_REFUSED) {
                ++O.refusals;
                continue;
            }
            if(O.got < bytes)return;
        }
        if(O.got >= bytes)O.sector = O.piece_end;
    }
}

/* The driver and the two channels by its records: 1 read, and busy says
 * which the game uses; 2 the records do not prove them (both busy); 0
 * another driver, or none (why); -1 the bus. */
static int owned(struct bus *b, bool busy[2], uint32_t *why) {
    const int known = driver(b, why);
    if(known <= 0)return known;
    const int read = records(b, busy);
    if(read < 0)return -1;
    return read ? 1 : 2;
}
static void work(struct bus *b, bool wanted, bool tail, uint32_t now) {
    bool busy[2] = {true, true};
    uint32_t why = 0u, detail = 0u;
    int state = 0;
    if(O.family) {
        state = owned(b, busy, &why);
        if(state < 0)return;
        if(!state) {
            /* Another driver, or this one being started again: the channels
             * and the memory are its own, and nothing more is written. */
            O.keyed = O.family = O.placed = O.place_try = O.place_at = O.spare = 0u;
        }
    }
    if((!wanted && !tail) || O.phase == KUI_TOY_CDDA_OUT_SILENT || (O.started && !state)) {
        if(!wanted && !tail) {
            stop(b, busy, KUI_TOY_CDDA_OUT_OFF, 0u, 0u);
            O.started = 0u;
        } else if(O.phase != KUI_TOY_CDDA_OUT_SILENT)stop(b, busy, KUI_TOY_CDDA_OUT_SILENT, why, 0u);
        else quiet(b, busy);
        /* Silent: the card is the game's. */
        if(O.asked) {
            const struct kui_toy_cdda_want none = {0u, 0u, 0u, 0u, false};
            (void)kui_toy_cdda_card_heartbeat(&none, 0u, take, b);
            O.asked = 0u;
        }
        return;
    }
    if(O.phase == KUI_TOY_CDDA_OUT_OFF) {
        if(!O.started || D.generation != O.generation) {
            O.generation = D.generation;
            O.started = 1u;
            O.rearms = O.why = O.why_detail = 0u;
        }
        if(!O.family) {
            state = owned(b, busy, &why);
            if(state < 0)return;
            if(!state) {
                stop(b, busy, KUI_TOY_CDDA_OUT_SILENT, why, 0u);
                return;
            }
        }
        /* A play that is over by the drive's clock is not begun. */
        if(!drive_now(now)) {
            stop(b, busy, KUI_TOY_CDDA_OUT_SILENT, 0u, 0u);
            return;
        }
    }
    /* Before anything is written: the channels are free by the driver's
     * records, the table leaves the ring alone and its guards stand. */
    if(state == 2) {
        stop(b, busy, KUI_TOY_CDDA_OUT_SILENT, KUI_TOY_CDDA_OUT_WHY_PROOF, 0u);
        return;
    }
    if(busy[0] || busy[1]) {
        ++O.takes;
        stop(b, busy, KUI_TOY_CDDA_OUT_SILENT, KUI_TOY_CDDA_OUT_WHY_TAKE, busy[0] | busy[1] << 1);
        return;
    }
    if(!O.placed) {
        const int placed = place(b, &detail);
        if(placed < 0)stop(b, busy, KUI_TOY_CDDA_OUT_SILENT, KUI_TOY_CDDA_OUT_WHY_RING, detail);
        if(placed <= 0)return;
    }
#if KUI_TOY_CDDA_SPARE
    /* (The driver's spare memory is no place of the table's.) */
    const int clear = 1;
#else
    const int clear = table(b, O.ring, ring_bytes(), &detail);
    if(clear == -1)return;
#endif
    if(clear != 1 || !guards(b, false)) {
        if(!b->ok)return;
        if(clear != 1)++O.lost_table;
        else ++O.lost_guard;
        /* What lies there is no longer the ring. The play takes a place
         * again and begins there; one that loses its ring again and again
         * is given up, as with a key dropped. */
        if(++O.rearms > REARMS)
            stop(b, busy, KUI_TOY_CDDA_OUT_SILENT, KUI_TOY_CDDA_OUT_WHY_RING,
                clear != 1 ? KUI_TOY_CDDA_OUT_RING_TABLE : KUI_TOY_CDDA_OUT_RING_GUARD);
        else restart(b, busy);
        O.placed = O.place_try = O.place_at = O.spare = 0u;
        return;
    }
    if(O.phase == KUI_TOY_CDDA_OUT_OFF) {
        /* A ring that began again when the bus's stretch had run out left
         * its channels keyed on: they would loop what lies in the ring.
         * They are keyed off before the new ring is primed. */
        if(O.keyed) {
            quiet(b, busy);
            if(O.keyed)return;
        }
        /* The play is taken up where the drive stands. */
        if(!wipe(b))return;
        O.piece_end = O.sector;
        O.finished = O.written = O.played = O.broken = O.at = O.carried = 0u;
        O.coder[0] = O.coder[1] = (struct kui_toy_cdda_coder){0, 127u};
        O.phase = KUI_TOY_CDDA_OUT_PRIME;
        O.prime_ticks = now;
    }
    levels(b);
    if(O.phase == KUI_TOY_CDDA_OUT_PRIME) {
        const uint32_t prime = PRIME_FRAMES < O.frames / 2u ? PRIME_FRAMES : O.frames / 2u;
        /* (The ring's room, not the prime's rest: the card hands on
         * whole blocks.) */
        if(O.written < prime)fill(b, O.frames - 2u * EDGE - O.written, true);
        if(O.broken) {
            restart(b, busy);
            return;
        }
        if(!b->ok || (O.written < prime && !O.finished))return;
        /* A key-on starts only a channel that is keyed off: one left keyed
         * on, by the driver before us or by its strobe behind our key-off,
         * is keyed off first. */
        bool on[2];
        if(held(b, 0u, &on[0]) < 0 || held(b, 1u, &on[1]) < 0)return;
        if(on[0] || on[1]) {
            static const bool both[2] = {true, true};
            ++O.rekeys;
            (void)off(b, both);
            return;
        }
        /* The driver's count as the two channels start: the ring's clock
         * from here on. */
        uint32_t count;
        if(!sound(b, count_at(), &count))return;
        if(!O.keys)O.lead_least = UINT32_MAX;
        key_on(b);
        if(!b->ok)return;
        O.key_count = count;
        O.played = O.moved = 0u;
        O.played_tick = O.moved_tick = O.key_tick;
        O.prime_ticks -= O.key_tick;
        O.phase = KUI_TOY_CDDA_OUT_RUN;
        return;
    }
    /* Running: both channels still name the ring and are keyed on. */
    bool moved = false;
    for(unsigned side = 0; side < 2u; ++side) {
        bool on;
        const int holds = held(b, side, &on);
        if(holds < 0)return;
        if(!holds) {
            ++O.takes;
            busy[side] = true;
            stop(b, busy, KUI_TOY_CDDA_OUT_SILENT, KUI_TOY_CDDA_OUT_WHY_TAKE, 4u << side);
            return;
        }
        if(!on) {
            /* Keyed off behind our back: the driver's stop-all, or its
             * strobe's read and write across our key-on. */
            ++O.drops;
            if(++O.rearms > REARMS)stop(b, busy, KUI_TOY_CDDA_OUT_SILENT, KUI_TOY_CDDA_OUT_WHY_FLAP, O.rearms);
            else restart(b, busy);
            return;
        }
        /* What a command of the driver's for an idle port may have changed
         * is written again. The plane's address, the loop's end and the
         * pitch move the place the chip plays at: with one of those
         * changed, the ring begins again. */
        static const uint8_t at[] = {0x04u, 0x0cu, 0x18u, 0x20u, 0x24u};
        const uint32_t want[] = {plane(side) & 0xffffu, O.frames, 0u, 0u, O.level[side]};
        for(unsigned i = 0; i < sizeof(at); ++i) {
            uint32_t value;
            if(!stable(b, channel(side) + at[i], &value))return;
            if((value & 0xffffu) == want[i])continue;
            if(i < 4u || (value & 0xffffu) != O.level_set[side])++O.fixes;
            if(!put(b, channel(side) + at[i], want[i]))return;
            if(i == 4u) {
                O.level_set[side] = want[i];
                if(want[i])O.level_last[side] = want[i];
            }
            if(i < 3u)moved = true;
        }
    }
    if(moved) {
        if(++O.rearms > REARMS)stop(b, busy, KUI_TOY_CDDA_OUT_SILENT, KUI_TOY_CDDA_OUT_WHY_FLAP, O.rearms);
        else restart(b, busy);
        return;
    }
    /* The driver's count of the sound chip's time, and TMU0 beside it from
     * the first look on: what the two clocks differ by, for the report. A
     * count that fell is the driver started again. */
    uint32_t chip;
    if(!sound(b, count_at(), &chip))return;
    const uint32_t tmu = read32(TCNT0);
    if(!O.clocked || (int32_t)(chip - O.chip_first - O.chip_ticks) < 0) {
        O.clocked = 1u;
        O.chip_first = chip;
        O.tmu_first = tmu;
    }
    O.chip_ticks = chip - O.chip_first;
    O.tmu_ticks = O.tmu_first - tmu;
    /* What the chip has played since key-on, by the driver's count: a
     * count more or less. TMU0's account of the same time is the witness,
     * since key-on and since the count was last seen to move: a
     * sixty-fourth of it and two counts is as far as the two may differ
     * (the console's quarter of a percent is a sixth of that), and a count
     * moves every 4 ms. */
    const uint32_t margin = MARGIN, counted = frames_counted(chip - O.key_count);
    const uint32_t timed = frames_of(O.key_tick - tmu), lately = frames_of(O.moved_tick - tmu);
    bool borne = apart(counted, timed) <= timed / 64u + SLACK;
    if(counted == O.moved)borne = borne && lately <= SLACK;
    else {
        borne = borne && apart(counted - O.moved, lately) <= lately / 64u + SLACK;
        O.moved = counted;
        O.moved_tick = tmu;
    }
    /* (For the report: the longest wait between two looks at a ring that
     * runs.) */
    if(O.played_tick - tmu > O.gap_longest)O.gap_longest = O.played_tick - tmu;
    O.played = counted;
    O.played_tick = tmu;
    if(!borne) {
        /* The count stands still, fell, or is no count of this play's
         * time: the ring begins again with the count as it is then. */
        ++O.clock_faults;
        if(++O.rearms > REARMS)stop(b, busy, KUI_TOY_CDDA_OUT_SILENT, KUI_TOY_CDDA_OUT_WHY_CLOCK, 2u);
        else restart(b, busy);
        return;
    }
    /* (For the report: the least a look found the ring ahead of the chip,
     * the look that found it dry among them.) */
    const uint32_t ahead = O.written > O.played ? O.written - O.played : 0u;
    if(!O.finished && ahead < O.lead_least)O.lead_least = ahead;
    if(O.finished) {
        if(O.played >= O.audio_end + margin) {
            stop(b, busy, KUI_TOY_CDDA_OUT_SILENT, 0u, 0u);
            return;
        }
    } else if(ahead < margin) {
        ++O.underruns;
        restart(b, busy);
        return;
    }
    if(tail && !O.finished) {
        /* The drive's play has ended and the card is the game's again:
         * what is written is the play's last. */
        O.finished = 1u;
        O.audio_end = O.written;
    }
    const uint32_t most = O.frames - margin - EDGE;
    if(ahead < most) {
        fill(b, most - ahead, ahead < (O.frames / 3u > WATER_FRAMES ? O.frames / 3u : WATER_FRAMES));
        if(O.broken)restart(b, busy);
    }
}

static bool bus_begin(struct bus *b, uint32_t now, uint32_t *sr) {
    *sr = sr_read();
    if(*sr & SR_BL) {
        ++O.skipped_bl;
        return false;
    }
    sr_write(*sr | 0xf0u);
    for(uint32_t dma = 0; dma < 4u; ++dma) {
        const uint32_t base = G2_DMA + 0x20u * dma;
        if((read32(base + 0x14u) | read32(base + 0x18u)) & 1u) {
            ++O.skipped_dma;
            sr_write(*sr);
            return false;
        }
    }
    *b = (struct bus){~now, ~now, 0u, 0u, true};
    return true;
}
static void bus_end(struct bus *b, uint32_t sr) {
    b->burst = 0u;
    if(b->ok)(void)drain(b);
    if(!b->ok)++O.timeouts;
    const uint32_t ticks = ~read32(TCNT0) - b->began;
    if(ticks > O.longest)O.longest = ticks;
    O.bus_ticks += ticks;
    ++O.heartbeats;
    sr_write(sr);
}

__attribute__((noinline)) void kui_toy_cdda_output_heartbeat(void) {
    bool wanted = D.state == KUI_TOY_CDDA_PLAYING &&
        (O.phase == KUI_TOY_CDDA_OUT_OFF || D.generation == O.generation);
    /* A play that ended once has its last frames still in the ring. */
    bool tail = D.state == KUI_TOY_CDDA_ENDED && O.phase == KUI_TOY_CDDA_OUT_RUN &&
        D.generation == O.generation + 1u;
    if(!wanted && !tail && O.phase == KUI_TOY_CDDA_OUT_OFF && !O.started && !O.keyed && !O.asked)return;
    const uint32_t frqcr = read16(FRQCR), tcor = read32(TCOR0), tcr = read16(TCR0), tstr = read8(TSTR);
    const uint32_t now = read32(TCNT0);
    if(!kui_toy_pilot_clock_profile(frqcr, tcor, tcr, tstr)) {
        /* No clock: no play is followed. What sounds is keyed off within
         * the polls' budget alone. */
        if(!O.why)O.why = KUI_TOY_CDDA_OUT_WHY_CLOCK;
        wanted = tail = false;
    } else if(O.heartbeats) {
        /* Every sixtieth of a second, and sooner when the ring has room for
         * a kilobyte more. */
        const uint32_t since = O.last_tick - now;
        bool due = since >= CADENCE;
        if(!due && since >= SPACING && wanted) {
            /* (What the chip has played by now: as counted at the last
             * look, and TMU0's time since.) */
            const uint32_t played = O.played + frames_of(O.played_tick - now);
            due = O.phase == KUI_TOY_CDDA_OUT_OFF || O.phase == KUI_TOY_CDDA_OUT_PRIME ||
                (O.phase == KUI_TOY_CDDA_OUT_RUN && !O.finished &&
                 (int32_t)(O.written - played + MARGIN + EDGE + PIECE_FRAMES - O.frames) <= 0);
        }
        if(!due)return;
    }
    O.last_tick = now;
    uint32_t sr;
    struct bus b;
    if(!bus_begin(&b, now, &sr))return;
    work(&b, wanted, tail, now);
    bus_end(&b, sr);
}
/* The worker's heartbeat calls the audio's consumer
 * (include/kui/toy_cdda_player.h): with the output built, the output. */
void kui_toy_cdda_consumer_heartbeat(void);
void kui_toy_cdda_consumer_heartbeat(void) {
    if(kui_vmu_native_passive_stack_room() < KUI_TOY_CDDA_OUTPUT_STACK) {
        ++O.skipped_stack;
        return;
    }
    kui_toy_cdda_output_heartbeat();
}
void kui_toy_cdda_output_terminal(void) {
    if(!O.keyed || kui_vmu_native_passive_stack_room() < KUI_TOY_CDDA_OUTPUT_STACK)return;
    uint32_t sr, why;
    struct bus b;
    bool busy[2];
    if(!bus_begin(&b, read32(TCNT0), &sr))return;
    if(O.family && owned(&b, busy, &why) == 1)quiet(&b, busy);
    bus_end(&b, sr);
}
