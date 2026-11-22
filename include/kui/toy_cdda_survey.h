/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_TOY_CDDA_SURVEY_H
#define KUI_TOY_CDDA_SURVEY_H
/* Test 84 (SURVEY): while the drive plays, the worker's heartbeat looks at the
 * sound chip and only reads it (src/loader/toy_cdda_survey.c,
 * docs/cdda-background-test-84.md). It writes nothing to the chip, its memory
 * or the bus: what it learns is counted for the report and for Test 85. */
#include <stdint.h>

/* The sound driver, by its version bytes at sound memory 0x20 (byte 0x20 the
 * major, 0x21 the minor, 0x22 the build). */
enum kui_toy_cdda_family {
    KUI_TOY_CDDA_FAMILY_UNKNOWN = 0u,  /* none of these, or none running */
    KUI_TOY_CDDA_FAMILY_GEN2 = 1u,     /* MANATEE 2.xx: 02, 'EXEC' */
    KUI_TOY_CDDA_FAMILY_GEN1 = 2u,     /* MANATEE 1.01.37 and later: 01 01 >=37 */
    KUI_TOY_CDDA_FAMILY_GEN1_OLD = 3u, /* MANATEE 1.01 before .37: 01 01 <37 */
    KUI_TOY_CDDA_FAMILY_NAMIHEI = 4u,  /* 01 00, 'SEGA' */
    KUI_TOY_CDDA_FAMILY_AUDIO64 = 5u,  /* 01 50 or 01 5A, 64 host ports from channel 0 */
    KUI_TOY_CDDA_FAMILY_HYBRID = 6u,   /* the same versions, 16 host ports from channel 48 */
    KUI_TOY_CDDA_FAMILY_RESET = 7u     /* the sound CPU is held in reset */
};
/* The 48 KiB from 0x1F4000 to the end of sound memory, where AUDIO64's heap
 * may end; read 256 bytes a survey. */
#define KUI_TOY_CDDA_WINDOW_KIB 48u

struct kui_toy_cdda_survey {
    /* The driver last identified: its version word, a 16-bit hash of its
     * build date's text (0x50..0x5B), its state word (0xF8) where the family
     * keeps one (MANATEE; else 0) and its family; changes: how often the
     * version or the date changed after the first. */
    uint32_t version, date_hash, state_word, family, changes, identified;
    /* proved: surveys that read the family's two candidate channels (the two
     * Test 85 would take); busy[k]: of those, the ones that found candidate k
     * in use by the game; unproved: surveys whose family's records did not
     * prove their own layout. */
    uint32_t proved, busy[2], unproved;
    /* The window: passes completed, the next offset, the hash being taken,
     * whether the first pass met a non-zero word, the KiB whose hash changed
     * in a later pass (a bit each), and the first pass's hashes. */
    uint32_t window_passes, window_offset, window_hash, window_nonzero;
    uint32_t window_changed[2];
    uint16_t window_first[KUI_TOY_CDDA_WINDOW_KIB];
    /* Surveys not made: SR.BL set, a G2 DMA channel enabled or running, the
     * bus's FIFO not emptying within its budget; heartbeats with TMU0 out of
     * the Toy profile. last: TMU0 at the last survey. */
    uint32_t skipped_bl, skipped_dma, timeouts, last_tick, started, skipped_clock;
};
extern struct kui_toy_cdda_survey kui_toy_cdda_survey;
/* At most one survey each sixtieth of a second, and only while the drive plays. */
void kui_toy_cdda_survey_heartbeat(void);
#endif
