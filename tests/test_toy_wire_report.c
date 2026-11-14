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

#if KUI_TOY_WIRE_IDLE
#define TEST_NUMBER "73"
/* Test 73's rows: what the game showed, what its player did and had, the
 * reader's time, the second movie's course; then the stream. */
static const char *const labels[] = {
    "PIPE B I CALLS IRQ", "AHEAD B I HIT EDC STOP",
    "I SUBMIT CADENCE MAX AT", "I FIRST LAST SKIP EXIT WF SF", "B SUBMIT CADENCE MAX AT",
    "IP ON OT OC OI SN ST", "BP ON OT OC OI SN ST", "IB SKU SKN WTU WTN CAP ALL", "IC RDY WGD NOGD OTH",
    "V SCI TMR GAME N TICKS", "U S W D G S W D", "D N TICKS",
    "TF", "TD", "TW", "TB", "TL",
    "W START SEG CUT WAIT FULL", "W BYTES WTICK TMR ERR DET", "W BLK FIX UNFIX HOLED NOCRC SEEK",
    "TI", "TG",
};
enum { LABELS = sizeof(labels) / sizeof(labels[0]), ESSENTIAL = 17 };
#elif KUI_TOY_WIRE_WORDS
#if KUI_TOY_WIRE_PACE
#define TEST_NUMBER "72"
#define LAST_ROW "U SEEN WORK DUE"
#else
#define TEST_NUMBER "71"
#define LAST_ROW "F END HELD TWICE"
#endif
/* Test 71's rows: Test 70's down to the requests' waits, the second
 * movie's course, the stream. */
static const char *const labels[] = {
    "PIPE B I CALLS IRQ", "AHEAD B I HIT EDC STOP",
    "I SUBMIT CADENCE MAX AT", "I FIRST LAST SKIP EXIT WF SF",
    "B SUBMIT CADENCE MAX AT", "B FIRST LAST SKIP EXIT WF SF",
    "V SCI TMR GAME N TICKS", "V MAX SCI TMR GAME", "Q B N T MAX I N T MAX",
    "P BUDGET RT RMAX", "TF", "TD", "TW", "TI", "TG", "TX",
    "W START SEG CUT WAIT FULL", "W BYTES WTICK TMR ERR DET", "W BLK FIX UNFIX HOLED NOCRC SEEK",
    "W GAPI GAPT BADCRC FRAME AMBIG", "W FILL BYTES MOST REF BAD DECL",
    "W STOP C J S F X", LAST_ROW,
};
enum { LABELS = sizeof(labels) / sizeof(labels[0]), ESSENTIAL = 16 };
#elif KUI_TOY_WIRE_MEASURE
#if KUI_TOY_WIRE_TICK
#define TEST_NUMBER "69"
#define P_ROW "P TICK BUDGET RT RMAX"
#else
#define TEST_NUMBER "70"
#define P_ROW "P CCR MMU BUDGET RT RMAX"
#endif
/* Test 69's and Test 70's rows, most telling first. All but the last three are what the
 * test is read by. */
static const char *const labels[] = {
    "PIPE B I CALLS IRQ", "AHEAD B I HIT EDC STOP",
    "I SUBMIT CADENCE MAX AT", "I FIRST LAST SKIP EXIT WF SF",
    "B SUBMIT CADENCE MAX AT", "B FIRST LAST SKIP EXIT WF SF",
    "V SCI TMR GAME N TICKS", "V MAX SCI TMR GAME", "Q B N T MAX I N T MAX",
    P_ROW,
    "W START SEG CUT WAIT FULL", "W BYTES WTICK TMR ERR DET", "W BLK FIX UNFIX HOLED NOCRC SEEK",
    "W GAPI GAPT BADCRC FRAME AMBIG", "W FILL BYTES MOST REF BAD DECL",
    "W STOP C J S F X", "W DROP E N I C W O", "F BIT03 BIT47 KIND",
    "F1", "F2", "SD BLK PIO START CRC",
};
enum { LABELS = sizeof(labels) / sizeof(labels[0]), ESSENTIAL = 18 };
#else
#define TEST_NUMBER "68"
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
#endif
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
    assert(!strncmp(out, "K-UI TOY COMMANDER REPORT\nTEST " TEST_NUMBER "\nBUILD DEADBEEF0123\nHEX / 781250 HZ\n", 69u));
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
#if KUI_TOY_WIRE_MEASURE
#if KUI_TOY_WIRE_CACHED
    wire.ccr = 0x105; wire.mmucr = 0;
#endif
#if KUI_TOY_WIRE_WORDS
    /* A second movie that plays its first ten seconds and its last five
     * and skips in between, as wide as each piece's byte can show. */
    for(unsigned i = 0; i < KUI_TOY_WIRE_BUCKETS; ++i) {
        const bool plays = i < 2u || i == 11u;
        wire.course_frames[i] = plays ? 161u : 9u + i;
        wire.course_sectors[i] = plays ? 2112u : 2600u + 16u * i;
        wire.course_stops[i] = (uint16_t)(3u * i);
        wire.course_wait[i] = plays ? 0x3000u * i : 0x180000u + 0x4000u * i;
        wire.course_interrupt[i] = 0x90000u + 0x4000u * i;
        wire.course_game[i] = plays ? 0x30000u : 0x500000u;
    }
    wire.n.frame_end = 0x80; wire.n.frame_held = 0x7c; wire.n.frame_twice = 0x79;
#endif
#if KUI_TOY_WIRE_PACE
    wire.updates = 0x2d500; wire.update_work = 0x1200; wire.update_due = 0x4a00;
#endif
#if KUI_TOY_WIRE_IDLE
    /* A second movie whose player puts out a third of its frames, each in
     * an update of 45 ms, and skips the rest in no time; a first movie
     * that puts out all of them in 20 ms each. Its megabyte of buffer
     * empties in the pieces that skip, and its lag counter climbs. */
    wire.idles = 0x1d100; wire.idle_work = 0x900; wire.idle_due = 0x4100;
    wire.deliver_calls = 0x1e000; wire.deliver_ticks = 0x2a0000;
    wire.player[1] = (struct kui_toy_wire_player){0x290, 0x16e3600, 0x1a00000, 0x1c0000, 0x520, 0x9000};
    wire.player[0] = (struct kui_toy_wire_player){0x14b, 0x4f0000, 0x7e0000, 0x30000, 0, 0};
    wire.update_ticks = 0x1900000; wire.buffer_capacity = 0x100000;
    for(unsigned i = 0; i < KUI_TOY_WIRE_BUCKETS; ++i) {
        const bool plays = i < 2u || i == 11u;
        wire.course_free[i] = plays ? 0x8000u + 0x2000u * i : 0xf0000u + 0x1000u * i;
        wire.course_lag[i] = plays ? i : 40u + i;
    }
#endif
    /* Both movies with the reader at Test 68's measured cost, most of its
     * interrupt time now behind the timer; the console's count of streams
     * that could not be followed, and two of them noted. */
    wire.n.ticks = 0x9c40; wire.n.lost_frame = 0x4a;
    wire.n.frame_bit[0] = 0x02010305; wire.n.frame_bit[1] = 0x003f0102; wire.n.frame_kind = 0x3d0a0300;
    wire.n.frame_note[0] = (struct kui_wire_frame_note){0x1c5affff, 0xbf9d3311, 0x04000002, 0x1f3, 0x30000123};
    wire.n.frame_note[1] = (struct kui_wire_frame_note){0xffffffff, 0x3fa0e6c4, 0x04100001, 0xffffffff, 0x1000fa37};
    wire.visit_calls[0] = 0xc67c; wire.visit_calls[1] = 0xd000; wire.visit_calls[2] = 0x26265;
    wire.visit_ticks[0] = 0x3a0000; wire.visit_ticks[1] = 0x4e0000; wire.visit_ticks[2] = 0x5b0000;
    wire.visit_max[0] = 0x1023; wire.visit_max[1] = 0x1a0; wire.visit_max[2] = 0x7ff;
    wire.budget_ends = 0xa000; wire.restart_ticks = 0x1d0000; wire.restart_max = 0xc350;
    wire.wait_calls[0] = 3; wire.wait_ticks[0] = 0x2ee0; wire.wait_max[0] = 0x1770;
    wire.wait_calls[1] = 0x4a; wire.wait_ticks[1] = 0x1e8480; wire.wait_max[1] = 0xea60;
#else
    wire.handler_calls = 0xbe00; wire.handler_ticks = 0x1e8480; wire.handler_max = 0x1a0;
#endif
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
        .wait_missing = 0xa34, .skip_frame = 0xc4, .skip_used = 0x1f000, .skip_missing = 0x6800};
    ra.movie_chunk[1] = (struct kui_toy_movie_chunk_trace){0x7b0, 0x1234, 0x56, 1};
    ra.wait[0] = (struct kui_toy_movie_wait_trace){0x69263, 0x10, 0xc, 0x235678, 0x1345, 0x1ce1};
    ra.wait[1] = (struct kui_toy_movie_wait_trace){0x6e450, 0x11, 0x3, 0x2e4567, UINT32_MAX, UINT32_MAX};
    body = format(&ra, &region, text, &rows);
#if KUI_TOY_WIRE_IDLE
    /* Everything fits at a full run's values. */
    assert(essential_rows == ESSENTIAL && rows == LABELS);
    const char *last = strstr(text, "TL\n12A2B 2C2D2E2F 3031320B\n");
    assert(last);
    const size_t needed = (size_t)(last - text) + strlen("TL\n12A2B 2C2D2E2F 3031320B\n");
    assert(needed + 100u <= KUI_TOY_REPORT_FILE_BYTES - 12u);
    assert(strstr(text, "PIPE B I CALLS IRQ\n2 3 9000 BE00\n"));
    assert(strstr(text, "I SUBMIT CADENCE MAX AT\n7B0 7AE 92345 124\n"));
    assert(strstr(text, "I FIRST LAST SKIP EXIT WF SF\n23576 7B0 2 FFFFF843 BB C4\n"));
    assert(strstr(text, "B SUBMIT CADENCE MAX AT\n14B 14A 12ABC F6\n"));
    /* The player's rows: the second movie's, then the first's. */
    assert(strstr(text, "IP ON OT OC OI SN ST\n290 16E3600 1A00000 1C0000 520 9000\n"));
    assert(strstr(text, "BP ON OT OC OI SN ST\n14B 4F0000 7E0000 30000 0 0\n"));
    assert(strstr(text, "IB SKU SKN WTU WTN CAP ALL\n1F000 6800 8789 A34 100000 1900000\n"));
    assert(strstr(text, "IC RDY WGD NOGD OTH\n7B0 1234 56 1\n"));
    assert(strstr(text, "V SCI TMR GAME N TICKS\nC67C D000 26265 3A0000 4E0000 5B0000\n"));
    assert(strstr(text, "U S W D G S W D\n2D500 1200 4A00 1D100 900 4100\n"));
    assert(strstr(text, "D N TICKS\n1E000 2A0000\n"));
    assert(strstr(text, "TF\nA1A10B0C D0E0F10 111213A1\n"));
    assert(strstr(text, "TD\n8484A4A5 A6A7A8A9 AAABAC84\n"));
    assert(strstr(text, "TW\n6263 64656667 68696A08\n"));
    /* The buffer's free space in pieces of 8 KiB, the lag as counted. */
    assert(strstr(text, "TB\n4057979 7A7A7B7B 7C7C7D0F\n"));
    assert(strstr(text, "W BLK FIX UNFIX HOLED NOCRC SEEK\n24C00 6100 5 1C00 90 2F\n"));
    assert(strstr(text, "TI\n24252627 28292A2B 2C2D2E2F\n"));
    assert(strstr(text, "TG\nC0CFFFF FFFFFFFF FFFFFF0C\n"));
#elif KUI_TOY_WIRE_WORDS
    /* Everything fits at a full run's values. */
    assert(essential_rows == ESSENTIAL && rows == LABELS);
    const char *last = strstr(text, "TX\n30609 C0F1215 181B1E21\n");
    assert(last);
    const size_t needed = (size_t)(last - text) + strlen("TX\n30609 C0F1215 181B1E21\n");
    assert(needed + 60u <= KUI_TOY_REPORT_FILE_BYTES - 12u);
    assert(strstr(text, "I SUBMIT CADENCE MAX AT\n7B0 7AE 92345 124\n"));
    assert(strstr(text, "V SCI TMR GAME N TICKS\nC67C D000 26265 3A0000 4E0000 5B0000\n"));
    assert(strstr(text, "Q B N T MAX I N T MAX\n3 2EE0 1770 4A 1E8480 EA60\n"));
    assert(strstr(text, "P BUDGET RT RMAX\nA000 1D0000 C350\n"));
    /* Frames as counted; sectors in sixteens; ticks in pieces of 2^14;
     * each held to a byte, the earliest piece first. */
    assert(strstr(text, "TF\nA1A10B0C D0E0F10 111213A1\n"));
    assert(strstr(text, "TD\n8484A4A5 A6A7A8A9 AAABAC84\n"));
    assert(strstr(text, "TW\n6263 64656667 68696A08\n"));
    assert(strstr(text, "TI\n24252627 28292A2B 2C2D2E2F\n"));
    assert(strstr(text, "TG\nC0CFFFF FFFFFFFF FFFFFF0C\n"));
    assert(strstr(text, "W STOP C J S F X\n3 4 5 6 7\n"));
#if KUI_TOY_WIRE_PACE
    assert(strstr(text, "U SEEN WORK DUE\n2D500 1200 4A00\n"));
#else
    assert(strstr(text, "F END HELD TWICE\n80 7C 79\n"));
#endif
#elif KUI_TOY_WIRE_MEASURE
    /* Everything fits at a full run's values. */
    assert(essential_rows == ESSENTIAL && rows == LABELS);
    /* And with the wider values a worse run would bring, the rows the
     * test is read by still do: room behind them. */
    const char *last = strstr(text, "F BIT03 BIT47 KIND\n2010305 3F0102 3D0A0300\n");
    assert(last);
    const size_t needed = (size_t)(last - text) + strlen("F BIT03 BIT47 KIND\n2010305 3F0102 3D0A0300\n");
    assert(needed + 100u <= KUI_TOY_REPORT_FILE_BYTES - 12u);
    assert(strstr(text, "PIPE B I CALLS IRQ\n2 3 9000 BE00\n"));
    assert(strstr(text, "AHEAD B I HIT EDC STOP\n1100 6600 900 7700 2\n"));
    assert(strstr(text, "I SUBMIT CADENCE MAX AT\n7B0 7AE 92345 124\n"));
    assert(strstr(text, "I FIRST LAST SKIP EXIT WF SF\n23576 7B0 2 FFFFF843 BB C4\n"));
    assert(strstr(text, "B SUBMIT CADENCE MAX AT\n14B 14A 12ABC F6\n"));
    assert(strstr(text, "B FIRST LAST SKIP EXIT WF SF\n19807 14B 0 7DC 7C 95\n"));
    assert(strstr(text, "V SCI TMR GAME N TICKS\nC67C D000 26265 3A0000 4E0000 5B0000\n"));
    assert(strstr(text, "V MAX SCI TMR GAME\n1023 1A0 7FF\n"));
    assert(strstr(text, "Q B N T MAX I N T MAX\n3 2EE0 1770 4A 1E8480 EA60\n"));
#if KUI_TOY_WIRE_CACHED
    assert(strstr(text, "P CCR MMU BUDGET RT RMAX\n105 0 A000 1D0000 C350\n"));
#else
    assert(strstr(text, "P TICK BUDGET RT RMAX\n9C40 A000 1D0000 C350\n"));
#endif
    assert(strstr(text, "W START SEG CUT WAIT FULL\n1F 2800 83D7 6622 300\n"));
    assert(strstr(text, "W BYTES WTICK TMR ERR DET\n4F0C2D5 1B3A6C0 1 11 A45005\n"));
    assert(strstr(text, "W BLK FIX UNFIX HOLED NOCRC SEEK\n24C00 6100 5 1C00 90 2F\n"));
    assert(strstr(text, "W GAPI GAPT BADCRC FRAME AMBIG\n7A0 41 3 4A 2\n"));
    assert(strstr(text, "W FILL BYTES MOST REF BAD DECL\n1500 6A00 1D 4 2 0\n"));
    assert(strstr(text, "W STOP C J S F X\n3 4 5 6 7\n"));
    assert(strstr(text, "W DROP E N I C W O\n1 2 3 4 5 6\n"));
    assert(strstr(text, "F1\n1C5AFFFF BF9D3311 4000002 1F3 30000123\n"));
    assert(strstr(text, "F2\nFFFFFFFF 3FA0E6C4 4100001 FFFFFFFF 1000FA37\n"));
    assert(strstr(text, "SD BLK PIO START CRC\n40 2F 1F 0\n"));
#else
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
#endif
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
    printf("PASS Test " TEST_NUMBER " report: %zu body bytes with nothing counted, %zu with a full run's values "
           "(essential rows end at %zu of 1012); at full width %u of the %u essential rows are written "
           "and no row is ever cut\n", empty, full, needed, widest, (unsigned)ESSENTIAL);
    return 0;
}
