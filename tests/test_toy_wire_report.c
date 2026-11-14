/* SPDX-License-Identifier: GPL-3.0-only
 * The Test 68 terminal report as the worker formats it for its two SD
 * sectors: every row present with plausible values, no row ever cut short,
 * and nothing written beyond the file even when every value is as wide as
 * it can be. The formatter is the production source, included as the worker
 * includes it. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define KUI_TOY_SUITE_MODE 66
#define KUI_TOY_WIRE_STREAM 1
#define KUI_TOY_WIRE_REPORT_HOST_TEST 1
#define KUI_NATIVE_BACKGROUND_COMPACT 1
#define KUI_NATIVE_GENERAL_LEAN_LAYOUT 1
#define KUI_NATIVE_TOY_MOVIE_CAPTURE 1
#define KUI_NATIVE_TOY_PLAYBACK_CAPTURE 1
#define KUI_RETAIL_ASYNC 1
#define KUI_RETAIL_GD_ASYNC 1
#define KUI_RETAIL_ASYNC_TEST 1
#define KUI_RETAIL_TRANSPORT 1
#define KUI_SCI_SD_NO_BLOCK 1
#define KUI_GENERAL_SCI_RESUME_FIX 1
#define KUI_VMU_NATIVE_BACKGROUND 1
#define KUI_BUILD_ID "DEADBEEF0123"

#include "kui/native_readahead.h"
#include "kui/toy_suite_report_wire.h"
#include "kui/toy_wire_report.h"
#include "kui/vmu_sd_lease.h"

static struct { struct kui_toy_report_wire toy_report; uintptr_t sd_lease; } config;
static struct { bool armed, pending; unsigned submitted, completed; } state;
static unsigned char issued_chain[3u * KUI_TOY_REPORT_SECTOR_BYTES];
static bool reader_only(void) { return true; }
static bool lease_valid(void) { return true; }
static bool stack_valid(void) { return true; }
static uint32_t current_sr(void) { return 0xf0u; }
static int kui_vmu_native_persistent_sd_transaction(void *c, struct kui_loader_sd *l) {
    (void)c; (void)l;
    assert(!"report must be disarmed in this formatter test");
    return 0;
}
static struct kui_toy_wire_report wire;
static const struct kui_toy_wire_report *toy_wire_report_block(void) { return &wire; }
#include "toy_suite_report_save.inc"

static const char *const labels[] = {
    "PIPE B I CALLS IRQ FAIL", "AHEAD B I HIT EDC STOP", "SD BLOCKS PIO START CRC",
    "W START SEG CUT WAIT FULL", "W BYTES WTICK TMR ERR DET", "W BLK FIX UNFIX HOLED NOCRC SEEK",
    "W GAPI GAPT BADCRC FRAME AMBIG", "W FILL BYTES MOST REFUSE BAD",
    "W STOP CLOSE JUMP SECT FRAME FAULT", "W DROP ERR END IDLE CMD WAIT OTHER",
    "W IRQ TICKS MAX PARITY DECL",
    "B SUBMIT CADENCE MAX AT TAIL", "B FIRST LAST CLK OUT SKIP", "B ENTER SCENE EXIT FRAME",
    "I SUBMIT CADENCE MAX AT TAIL", "I FIRST LAST CLK OUT SKIP", "I ENTER SCENE EXIT FRAME",
    "B WAIT USED NEED SKIP LBA", "B READ N VERIFIED TICK AGE DONE",
    "I WAIT USED NEED SKIP LBA", "I READ N VERIFIED TICK AGE DONE", "I LAG LIMIT",
};
/* The first ESSENTIAL rows are what Test 68 is read by; the rest are Test
 * 66's first-wait rows, written as far as room is left. */
enum { LABELS = sizeof(labels) / sizeof(labels[0]), ESSENTIAL = 17 };
static unsigned essential_rows;

/* Format into a guarded buffer and return the body's length. Every line
 * pair found is whole: a label line followed by its line of hex words. */
static size_t format(const struct kui_native_ra_public *ra, const kui_retail_async_region_type *r,
                     char out[KUI_TOY_REPORT_FILE_BYTES + 1u], unsigned *rows) {
    char buffer[KUI_TOY_REPORT_FILE_BYTES + 2u];
    memset(buffer, '!', sizeof(buffer));
    char *text = buffer + 1u;
    toy_report_format(text, ra, &r->shared.stream.stats, 66u);
    assert(buffer[0] == '!' && buffer[sizeof(buffer) - 1u] == '!');
    assert(!memcmp(text + KUI_TOY_REPORT_FILE_BYTES - 12u, "\nEND REPORT\n", 12u));
    memcpy(out, text, KUI_TOY_REPORT_FILE_BYTES);
    out[KUI_TOY_REPORT_FILE_BYTES] = 0;
    const char *end = out + KUI_TOY_REPORT_FILE_BYTES - 12u;
    while(end > out && end[-1] == ' ') --end;
    assert(end > out && end[-1] == '\n');
    /* Card Tools starts this reader from its Test 66 entry; the file says 68. */
    assert(!strncmp(out, "K-UI TOY COMMANDER REPORT\nTEST 68\nBUILD DEADBEEF0123\nHEX / 781250 HZ\n", 69u));
    *rows = 0;
    unsigned next = 0, essential = 0;
    const char *at = out + 69u;
    while(at < end) {
        const char *label_end = memchr(at, '\n', (size_t)(end - at));
        assert(label_end);
        unsigned which = LABELS;
        for(unsigned i = 0; i < LABELS; ++i)
            if(strlen(labels[i]) == (size_t)(label_end - at) && !memcmp(labels[i], at, strlen(labels[i]))) which = i;
        /* In the formatter's order. A row that does not fit is left out
         * whole; a shorter one behind it may still be written. */
        assert(which < LABELS && which >= next);
        next = which + 1u;
        if(which < ESSENTIAL) ++essential;
        const char *values = label_end + 1u, *values_end = memchr(values, '\n', (size_t)(end - values));
        assert(values_end && values_end > values);
        for(const char *p = values; p < values_end; ++p)
            assert((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p == ' ' && p[1] != ' ' && p > values));
        at = values_end + 1u;
        ++*rows;
    }
    assert(at == end);
    essential_rows = essential;
    return (size_t)(end - out);
}

int main(int argc, char **argv) {
    static kui_retail_async_region_type region;
    static struct kui_native_ra_public ra;
    char text[KUI_TOY_REPORT_FILE_BYTES + 1u];
    unsigned rows;

    /* Nothing counted, no stream block yet: zeros, every row present. */
    size_t body = format(&ra, &region, text, &rows);
    assert(rows == LABELS && strstr(text, "W START SEG CUT WAIT FULL\n0 0 0 0 0\n"));
    const size_t empty = body;

    /* A full run of both movies as Test 67 measured the wire: the largest
     * values each counter can plausibly reach. */
    wire.magic = KUI_TOY_WIRE_REPORT_MAGIC;
    wire.n = (struct kui_wire_counters){.starts = 0x1f, .segments = 0x2800, .cuts = 0x83d7, .waits = 0x6622,
        .fulls = 0x300, .bytes = 0x4f0c2d5, .wait_ticks = 0x1b3a6c0, .blocks = 0x24c00, .rebuilt = 0x6100,
        .unsolved = 5, .holed = 0x1c00, .unchecked = 0x90, .gap_idle = 0x7a0, .gap_token = 0x41, .bad_crc = 3,
        .lost_frame = 1, .ambiguous = 2};
    wire.seek_blocks = 0x2f; wire.filled = 0x1500; wire.filled_bytes = 0x6a00; wire.most_lost = 0x1d;
    wire.refused = 4; wire.check_bad = 2; wire.parity = 1; wire.timer_mode = 1;
    wire.first_error = 0x11; wire.first_detail = 0x00a45005; wire.declined = 0;
    wire.handler_calls = 0xbe00; wire.handler_ticks = 0x1e8480; wire.handler_max = 0x1a0;
    for(unsigned i = 0; i < KUI_TOY_WIRE_STOPS; ++i) wire.stops[i] = 3u + i;
    for(unsigned i = 0; i < KUI_TOY_WIRE_DROPS; ++i) wire.drops[i] = 1u + i;
    uint32_t *trace = ra.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
    trace[0] = 0x1100; trace[1] = 0x6600; trace[2] = 2; trace[3] = 3; trace[4] = 0x9000; trace[5] = 0xbe00;
    ra.stats.hit_requests = 0x900; ra.stats.producer_blocks = 0x7700; ra.stats.abandoned = 2;
    region.shared.stream.stats.blocks = 0x40; region.shared.stream.stats.polled = 0x2f;
    region.shared.stream.stats.starts = 0x1f; region.shared.stream.stats.crc_errors = 0;
    ra.present[0] = (struct kui_toy_movie_present_trace){.submitted = 0x14b, .on_time = 0x14a,
        .max_gap = 0x12abc, .max_gap_frame = 0xf6, .first_gap = 0x19807, .last_frame = 0x14b,
        .render_entries = 0x14b, .scene_hits = 0x14b};
    ra.present[1] = (struct kui_toy_movie_present_trace){.submitted = 0x7b0, .on_time = 0x7ae,
        .max_gap = 0x92345, .max_gap_frame = 0x124, .tail_gap = 0x878a, .first_gap = 0x23576,
        .last_frame = 0x7b0, .clock_lost = 1, .render_entries = 0x7b0, .scene_hits = 0x7b0};
    ra.movie_exit[0] = (struct kui_toy_movie_exit_witness){.output_frames = 0x14a, .status = 0x7dc,
        .frame = 0x14b, .lag = 8, .limit = 6};
    ra.movie_exit[1] = (struct kui_toy_movie_exit_witness){.output_frames = 0x7ae, .skipped_frames = 2,
        .status = 0xfffff843, .frame = 0x7b0, .lag = 9, .limit = 7};
    ra.onset[0] = (struct kui_toy_movie_onset_trace){.wait_frame = 0x7c, .wait_used = 0x11000,
        .wait_missing = 0x400, .skip_frame = 0x95};
    ra.onset[1] = (struct kui_toy_movie_onset_trace){.wait_frame = 0xbb, .wait_used = 0x8789,
        .wait_missing = 0xa34, .skip_frame = 0xc4};
    ra.wait[0] = (struct kui_toy_movie_wait_trace){0x69263, 0x10, 0xc, 0x235678, 0x1345, 0x1ce1};
    ra.wait[1] = (struct kui_toy_movie_wait_trace){0x6e450, 0x11, 0x3, 0x2e4567, UINT32_MAX, UINT32_MAX};
    body = format(&ra, &region, text, &rows);
    assert(essential_rows == ESSENTIAL && rows >= ESSENTIAL + 3u);
    /* Room to spare behind the essential rows at these values. */
    const char *last = strstr(text, "I ENTER SCENE EXIT FRAME\n7B0 7B0 FFFFF843 7B0\n");
    assert(last);
    const size_t needed = (size_t)(last - text) + strlen("I ENTER SCENE EXIT FRAME\n7B0 7B0 FFFFF843 7B0\n");
    assert(needed + 160u <= KUI_TOY_REPORT_FILE_BYTES - 12u);
    assert(strstr(text, "W START SEG CUT WAIT FULL\n1F 2800 83D7 6622 300\n"));
    assert(strstr(text, "W BYTES WTICK TMR ERR DET\n4F0C2D5 1B3A6C0 1 11 A45005\n"));
    assert(strstr(text, "W BLK FIX UNFIX HOLED NOCRC SEEK\n24C00 6100 5 1C00 90 2F\n"));
    assert(strstr(text, "W GAPI GAPT BADCRC FRAME AMBIG\n7A0 41 3 1 2\n"));
    assert(strstr(text, "W FILL BYTES MOST REFUSE BAD\n1500 6A00 1D 4 2\n"));
    assert(strstr(text, "W STOP CLOSE JUMP SECT FRAME FAULT\n3 4 5 6 7\n"));
    assert(strstr(text, "W DROP ERR END IDLE CMD WAIT OTHER\n1 2 3 4 5 6\n"));
    assert(strstr(text, "W IRQ TICKS MAX PARITY DECL\nBE00 1E8480 1A0 1 0\n"));
    assert(strstr(text, "PIPE B I CALLS IRQ FAIL\n2 3 9000 BE00 0\n"));
    assert(strstr(text, "AHEAD B I HIT EDC STOP\n1100 6600 900 7700 2\n"));
    assert(strstr(text, "SD BLOCKS PIO START CRC\n40 2F 1F 0\n"));
    assert(strstr(text, "I ENTER SCENE EXIT FRAME\n7B0 7B0 FFFFF843 7B0\n"));
    assert(strstr(text, "B FIRST LAST CLK OUT SKIP\n19807 14B 0 14A 0\n"));
    assert(strstr(text, "B READ N VERIFIED TICK AGE DONE\n10 C 235678 1345 1CE1\n"));
    const size_t full = body;
    if(argc == 2) {
        FILE *out = fopen(argv[1], "wb");
        assert(out && fwrite(text, 1u, KUI_TOY_REPORT_FILE_BYTES, out) == KUI_TOY_REPORT_FILE_BYTES);
        assert(fclose(out) == 0);
    }

    /* Every value as wide as a word can be. Rows that no longer fit are
     * left out whole, from the end; the stream and playback rows stay. */
    memset(&ra, 0xff, sizeof(ra));
    memset(&region.shared.stream.stats, 0xff, sizeof(region.shared.stream.stats));
    memset(&wire, 0xff, sizeof(wire));
    wire.magic = KUI_TOY_WIRE_REPORT_MAGIC;
    body = format(&ra, &region, text, &rows);
    assert(essential_rows >= 12u && rows < LABELS && body <= KUI_TOY_REPORT_FILE_BYTES - 12u);
    const unsigned widest = essential_rows;

    /* A block that is not the stream's (never written) reads as zeros. */
    wire.magic = 0;
    (void)format(&ra, &region, text, &rows);
    assert(strstr(text, "W START SEG CUT WAIT FULL\n0 0 0 0 0\n"));
    assert(!toy_suite_save_report(&ra, &region));
    printf("PASS Test 68 report: %zu body bytes with nothing counted, %zu with a full run's values "
           "(essential rows end at %zu of 1012); at full width %u of the %u essential rows are written "
           "and no row is ever cut\n", empty, full, needed, widest, (unsigned)ESSENTIAL);
    return 0;
}
