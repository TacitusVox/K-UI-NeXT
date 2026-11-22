/* SPDX-License-Identifier: GPL-3.0-only */
/* Test 84 (SURVEY): the survey (src/loader/toy_cdda_survey.c), unmodified,
 * over sound memory made from an original sound-driver image the way the sd
 * library loads one: 2 MiB cleared, the image after its header copied to 0.
 * What the driver's own start-up writes before a game can play is added from
 * the drivers' static reading (docs/cd-audio-corpus.md), by kind:
 *   gen2     state word 'EXEC'; the default split 48/8/8 at 0x12060
 *   gen1     the channel pool: 16 entries at 0x12080, owner 0, register base
 *            0x00801800 + 0x80 x entry (no state word: 1.01.1d writes none)
 *   namihei  state word := the image's own word at 0x38 ('SEGA')
 *   audio64  the ports' base word before the playing table: 0x00800000
 *   midida   the same: 0x00801800
 *   aicadrv  the state word moves between surveys (its peak voice count)
 *   other    nothing
 * Lines "PATH OFFSET KIND" on stdin; one result line each on stdout. Driver
 * bytes are read from the owner's files and never written anywhere: the
 * results are the survey's identity fields and counts only
 * (tests/test_toy_cdda_survey_drivers.py). */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kui/toy_cdda_player.h"
#include "kui/toy_cdda_survey.h"

struct kui_toy_cdda_drive kui_toy_cdda_drive;
#define V kui_toy_cdda_survey
static uint8_t ram[0x200000];
static uint32_t sr = UINT32_C(0x40000000), tcnt = UINT32_C(0x70000000);
static unsigned long window_reads;

uint32_t kui_toy_cdda_survey_test_read(uint32_t address, unsigned width) {
    switch(address) {
    case UINT32_C(0xffc00000): return 0x0e0au;
    case UINT32_C(0xffd80004): return 1u;
    case UINT32_C(0xffd80008): return UINT32_MAX;
    case UINT32_C(0xffd80010): return 0x0002u;
    case UINT32_C(0xffd8000c): return tcnt -= 2u;
    case UINT32_C(0xa05f688c): return 0u;
    case UINT32_C(0xa0702c00): return 0u;
    default: break;
    }
    if(address >= UINT32_C(0xa05f7800) && address < UINT32_C(0xa05f7880)) return 0u;
    if(width != 4u || address < UINT32_C(0xa0800000) || address >= UINT32_C(0xa0a00000) || (address & 3u)) {
        fprintf(stderr, "a read the survey may not make: %#x\n", (unsigned)address);
        exit(2);
    }
    const uint32_t at = address - UINT32_C(0xa0800000);
    if(at >= UINT32_C(0x1f4000)) ++window_reads;
    uint32_t value;
    memcpy(&value, ram + at, 4u);
    return value;
}
uint32_t kui_toy_cdda_survey_test_sr(void) { return sr; }
void kui_toy_cdda_survey_test_set_sr(uint32_t value) { sr = value; }

static uint32_t word(uint32_t at) { uint32_t v; memcpy(&v, ram + at, 4u); return v; }
static void put(uint32_t at, uint32_t v) { memcpy(ram + at, &v, 4u); }

int main(void) {
    char path[4096], kind[32];
    long offset;
    kui_toy_cdda_drive.state = KUI_TOY_CDDA_PLAYING;
    while(scanf("%4095s %ld %31s", path, &offset, kind) == 3) {
        FILE *f = fopen(path, "rb");
        if(!f) { perror(path); return 2; }
        memset(ram, 0, sizeof(ram));
        if(fseek(f, offset, SEEK_SET)) { perror(path); return 2; }
        const size_t bytes = fread(ram, 1, sizeof(ram), f);
        fclose(f);
        if(bytes < 0x100u) { fprintf(stderr, "%s: too short\n", path); return 2; }
        memset(&V, 0, sizeof(V));
        window_reads = 0u;
        if(!strcmp(kind, "gen2")) {
            put(0xf8u, UINT32_C(0x43455845));
            put(0x12060u, UINT32_C(0x00080830));
        } else if(!strcmp(kind, "gen1")) {
            for(uint32_t e = 0; e < 16u; ++e) {
                put(0x12080u + 12u * e + 4u, 0u);
                put(0x12080u + 12u * e + 8u, UINT32_C(0x00801800) + 0x80u * e);
            }
        } else if(!strcmp(kind, "namihei")) {
            put(0xf8u, word(0x38u));
        } else if(!strcmp(kind, "audio64") || !strcmp(kind, "midida")) {
            const uint32_t table = word(0xe0u);
            if(table >= 0x104u && table < 0x10000u && !(table & 3u))
                put(table - 4u, !strcmp(kind, "audio64") ? UINT32_C(0x00800000) : UINT32_C(0x00801800));
        }
        for(unsigned i = 0; i < 4u; ++i) {
            if(!strcmp(kind, "aicadrv")) put(0xf8u, 5u + 7u * i);
            tcnt -= 13021u;
            kui_toy_cdda_survey_heartbeat();
            if(sr != UINT32_C(0x40000000)) { fprintf(stderr, "%s: SR not given back\n", path); return 2; }
        }
        printf("family=%u version=%08x date_hash=%04x state=%08x changes=%u proved=%u unproved=%u "
               "busy=%u/%u timeouts=%u window_reads=%lu\n", V.family, V.version, V.date_hash, V.state_word,
               V.changes, V.proved, V.unproved, V.busy[0], V.busy[1], V.timeouts, window_reads);
        fflush(stdout);
    }
    return 0;
}
