/* SPDX-License-Identifier: GPL-3.0-only
 * The Test 68 terminal report as the worker formats it for its two SD
 * sectors: every row present with plausible values, no row ever cut short,
 * and nothing written beyond the file even when every value is as wide as
 * it can be. The formatter is the production source, included as the worker
 * includes it. */
#include <assert.h>
#include <setjmp.h>
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
/* Test 84's switches the expressions below use, off where the recipe does
 * not set them (an undefined name is 0 in #if, but not in C). */
#ifndef KUI_TOY_CDDA_DRIVE
#define KUI_TOY_CDDA_DRIVE 0
#endif
#ifndef KUI_TOY_CDDA_SURVEY
#define KUI_TOY_CDDA_SURVEY 0
#endif
/* And Test 85's. */
#ifndef KUI_TOY_CDDA_CARD
#define KUI_TOY_CDDA_CARD 0
#endif
#ifndef KUI_TOY_CDDA_OUTPUT
#define KUI_TOY_CDDA_OUTPUT 0
#endif

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
#if KUI_TOY_WIRE_TITLES
/* Test 79: the worker's screen and its pause between pages, as the page
 * loop uses them. Every line shown is kept; a pause ends a page; the
 * loop, which never returns, is left by a jump once enough was seen. */
enum { SHOWN_LINES = 400u };
static char shown_line[SHOWN_LINES][64];
static unsigned shown_lines, shown_pages, page_first[64], pages_wanted, busy_from, restores;
static const struct retail_display_state *restored;
static jmp_buf pages_done;
void retail_display_line(const char *text) {
    assert(shown_lines < SHOWN_LINES && strlen(text) < sizeof(shown_line[0]));
    strcpy(shown_line[shown_lines++], text);
}
void retail_display_restore(const struct retail_display_state *display) { restored = display; ++restores; }
static void toy_report_pause(unsigned frames, bool *start_released, bool *reboot_refused) {
    assert(frames == 300u && start_released && reboot_refused);
    /* The page is complete when its pause begins, and the screen was put
     * back behind every page before it. */
    assert(restores == shown_pages);
    if(++shown_pages >= busy_from) *reboot_refused = true;
    if(shown_pages == pages_wanted) longjmp(pages_done, 1);
    page_first[shown_pages] = shown_lines;
}
/* The resident's lease as the report asks for it. */
static unsigned leases;
static int lease_stub(kui_vmu_sd_transaction transaction, void *context) {
    assert(transaction == kui_vmu_native_persistent_sd_transaction && context == &config.toy_report);
    ++leases;
    return KUI_VMU_SD_LEASE_FAULT;
}
#endif
static struct kui_toy_wire_report wire;
static const struct kui_toy_wire_report *toy_wire_report_block(void) { return &wire; }
#if KUI_TOY_CDDA_DRIVE
/* Test 84: the worker's player, whose counters the report writes. */
#include "kui/toy_cdda_player.h"
struct kui_toy_cdda_drive kui_toy_cdda_drive;
#endif
#if KUI_TOY_CDDA_SURVEY
/* And its sound chip survey. */
#include "kui/toy_cdda_survey.h"
struct kui_toy_cdda_survey kui_toy_cdda_survey;
#endif
#if KUI_TOY_CDDA_CARD
/* Test 85: the block the audio's two parts share, whose counters the report
 * writes; and with the sound output, its state. */
#include "kui/toy_cdda_mail.h"
struct kui_toy_cdda_mail kui_toy_cdda_mail;
#endif
#if KUI_TOY_CDDA_OUTPUT
#include "kui/toy_cdda_output.h"
struct kui_toy_cdda_output kui_toy_cdda_output;
#endif
#include "toy_suite_report_save.inc"

#if KUI_TOY_WIRE_ANY
#if defined(KUI_TOY_CDDA_CARD) && KUI_TOY_CDDA_CARD
/* Test 85 (CARD): Test 84's rows under its own number. */
#if defined(KUI_TOY_CDDA_ADPCM) && KUI_TOY_CDDA_ADPCM
#define TEST_NUMBER "88"
#define RING_KIND "3"
#define RING_NONE "1000000"
#else
#define TEST_NUMBER "85"
#define RING_KIND "2"
#define RING_NONE "0"
#endif
#elif KUI_TOY_CDDA_ROOM
#define TEST_NUMBER "84"
#elif KUI_TOY_WIRE_TIMER
#define TEST_NUMBER "81"
#elif KUI_TOY_WIRE_QUERY
#define TEST_NUMBER "80"
#elif KUI_TOY_WIRE_TITLES
#define TEST_NUMBER "79"
#else
#define TEST_NUMBER "78"
#endif
/* Test 78's rows: Test 77's with the large requests a window served,
 * their sectors and the requests that went back to the low reader; and
 * without the five rows by piece of the second movie's course, which this
 * build does not keep. The first movie's player is still last. */
/* The row in XC's place where the sound output is built (Test 89). */
#if defined(KUI_TOY_CDDA_OUTPUT) && KUI_TOY_CDDA_OUTPUT
#define XC_ROW_ZERO "CD W BL AH CU\n0 0 0\n"
#else
#define XC_ROW_ZERO "XC KEPT LO HI\n0 0 0\n"
#endif
static const char *const labels[] = {
    "PIPE B I CALLS IRQ STAY PT", "AHEAD B I HIT EDC STOP OS", "X OW E I C W O",
#if KUI_TOY_WIRE_TITLES
    /* Test 79: whose disc it is, and the low reader's own totals. */
#if !KUI_TOY_CDDA_DRIVE
    "BIG N S BACK TOY",
#endif
    "SD BLK PIO OVR CRC DECL",
#if KUI_TOY_WIRE_QUERY
    /* Test 80: the commands that are no reads a window stood through, and
     * which such commands a window met. */
#if defined(KUI_TOY_CDDA_OUTPUT) && KUI_TOY_CDDA_OUTPUT
    /* Test 89: in its place, why the audio's stream ended at a block. */
    "CD W BL AH CU",
#else
    "XC KEPT LO HI",
#endif
#endif
#if KUI_TOY_CDDA_DRIVE
    /* Test 84: the drive's audio, in BIG's, K's and P's place. */
    "CD P P2 PA RE END LAP", "CD ST RD KEEP REF LOST PJ",
#if KUI_TOY_CDDA_SURVEY
    "CD SV DT B N WIN SKIP",
#endif
#if KUI_TOY_CDDA_OUTPUT
    /* Test 85: the sound output, */
    "CD O ST V ID RING N", "CD O KEY RST UND CLK DRP TK", "CD O HB LONG TICK KB SKIP LV",
    "CD O MX LD L2 AT TM LOST",
#endif
#if KUI_TOY_CDDA_CARD
    /* and the audio from the card, in the place of the window stream's
     * rows from V to W FILL. */
    "CD C BLK RB RR PIO WAIT YLD", "CD C HD HO HX REF DCL ERR", "CD C VIS TICK HB LONG RND SEEK",
#endif
#endif
#else
    "BIG N S BACK",
#endif
    "I SUBMIT CADENCE MAX AT", "I FIRST LAST SKIP EXIT SF", "B SUBMIT CADENCE MAX AT",
    "IP ON OT OC OI SN ST",
#if !KUI_TOY_CDDA_DRIVE
    "K LIGHT EDC MEND MB LB ORD",
#endif
#if !KUI_TOY_CDDA_CARD
    "V SCI TMR GAME N TICKS",
#if KUI_TOY_WIRE_QUERY
    /* Test 80: the longest single visits. */
    "VMAX SCI TMR GAME D",
#endif
#if !KUI_TOY_CDDA_DRIVE
    "P RING FEED CHK OUT SVC HALT",
#endif
    "D N T SEEK",
    "W START SEG CUT WAIT FULL", "W BYTES WTICK TMR ERR DET", "W BLK FIX UNFIX HOLED NOCRC SEEK",
    "S C S F LF AM BC", "W FILL REF BAD",
#endif
    "BP ON OT OC OI SN ST",
};
enum { LABELS = sizeof(labels) / sizeof(labels[0]),
    /* (Test 85: eight rows of the window's stream out, three of the audio's
     * in, and four more with the sound output.) */
    ESSENTIAL = (KUI_TOY_WIRE_QUERY ? 20 : KUI_TOY_WIRE_TITLES ? 18 : 17) - (KUI_TOY_CDDA_DRIVE ? 1 : 0) +
        (KUI_TOY_CDDA_SURVEY ? 1 : 0) - (KUI_TOY_CDDA_CARD ? 5 : 0) + (KUI_TOY_CDDA_OUTPUT ? 4 : 0),
    /* Test 80 leaves the movie player's rows out for a title that is not
     * Toy Commander: the second movie's two, the first movie's, and the
     * two players'. */
    MOVIE_ROWS = KUI_TOY_WIRE_QUERY ? 5 : 0 };
#elif KUI_TOY_WIRE_SEQ
#define TEST_NUMBER "77"
/* Test 77's rows: Test 76's with the sectors served from windows outside
 * the movie files, how many such windows were opened, and why windows
 * were given up; without the wire's standstill by piece and the capacity
 * of the player's buffer, which have served; and the first movie's player
 * last, the row to go when ordinary reads have widened the others. */
static const char *const labels[] = {
    "PIPE B I CALLS IRQ STAY PT", "AHEAD B I HIT EDC STOP OS", "X OW E I C W O",
    "I SUBMIT CADENCE MAX AT", "I FIRST LAST SKIP EXIT SF", "B SUBMIT CADENCE MAX AT",
    "IP ON OT OC OI SN ST",
    "K LIGHT EDC MEND MB LB ORD",
    "V SCI TMR GAME N TICKS", "P RING FEED CHK OUT SVC HALT", "D N T SEEK",
    "TF", "TD", "TW", "TB",
    "W START SEG CUT WAIT FULL", "W BYTES WTICK TMR ERR DET", "W BLK FIX UNFIX HOLED NOCRC SEEK",
    "S C S F LF AM BC", "W FILL REF BAD",
    "TR",
    "BP ON OT OC OI SN ST",
};
enum { LABELS = sizeof(labels) / sizeof(labels[0]), ESSENTIAL = 21 };
#elif KUI_TOY_WIRE_FLUID
#define TEST_NUMBER "76"
/* Test 76's rows: Test 75's without the two that have served, the
 * reader's time by piece in one row, and three things new: the ticks
 * spent ending streams, the player's buffer and the wire's standstill by
 * piece. */
static const char *const labels[] = {
    "PIPE B I CALLS IRQ STAY PT", "AHEAD B I HIT EDC STOP",
    "I SUBMIT CADENCE MAX AT", "I FIRST LAST SKIP EXIT SF", "B SUBMIT CADENCE MAX AT",
    "IP ON OT OC OI SN ST", "BP ON OT OC OI SN ST",
    "K LIGHT EDC MEND MB LB ORD",
    "V SCI TMR GAME N TICKS", "P RING FEED CHK OUT SVC HALT", "D N T SEEK CAP",
    "TF", "TD", "TW", "TB", "TP",
    "W START SEG CUT WAIT FULL", "W BYTES WTICK TMR ERR DET", "W BLK FIX UNFIX HOLED NOCRC SEEK",
    "S C S F LF AM BC", "W FILL REF BAD",
    "TR",
};
enum { LABELS = sizeof(labels) / sizeof(labels[0]), ESSENTIAL = 21 };
#elif KUI_TOY_WIRE_STEADY
#define TEST_NUMBER "75"
/* Test 75's rows: what the game showed, what its player did, how sectors
 * were checked, the reader's time by caller and by what it was doing, the
 * second movie's course; then the stream and why streams and windows were
 * given up. */
static const char *const labels[] = {
    "PIPE B I CALLS IRQ STAY PT", "AHEAD B I HIT EDC STOP",
    "I SUBMIT CADENCE MAX AT", "I FIRST LAST SKIP EXIT SF", "B SUBMIT CADENCE MAX AT",
    "IP ON OT OC OI SN ST", "BP ON OT OC OI SN ST",
    "K LIGHT EDC MEND MB LB ORD",
    "V SCI TMR GAME N TICKS", "P RING FEED CHK OUT SVC LATER", "D N T G N T SEEK",
    "TF", "TD", "TW",
    "W START SEG CUT WAIT FULL", "W BYTES WTICK TMR ERR DET", "W BLK FIX UNFIX HOLED NOCRC SEEK",
    "S C S F LF AM BC", "X E N I C W O", "W FILL REF BAD",
    "TI", "TG",
};
enum { LABELS = sizeof(labels) / sizeof(labels[0]), ESSENTIAL = 19 };
#elif KUI_TOY_WIRE_LEAN
#define TEST_NUMBER "74"
/* Test 74's rows: what the game showed, what its player did, how sectors
 * were checked and the bit order, the reader's time, the second movie's
 * course; then the stream. */
static const char *const labels[] = {
    "PIPE B I CALLS IRQ", "AHEAD B I HIT EDC STOP",
    "I SUBMIT CADENCE MAX AT", "I FIRST LAST SKIP EXIT WF SF", "B SUBMIT CADENCE MAX AT",
    "IP ON OT OC OI SN ST", "BP ON OT OC OI SN ST", "IC RDY WGD NOGD",
    "K LIGHT EDC MEND MB LB ORD",
    "V SCI TMR GAME N TICKS", "U S W D G S W D", "D N T G N T",
    "TF", "TD", "TW",
    "W START SEG CUT WAIT FULL", "W BYTES WTICK TMR ERR DET", "W BLK FIX UNFIX HOLED NOCRC SEEK",
    "R C8 32 128 W8 32 128", "W FILL BYTES MOST REF BAD",
    "TI", "TG",
};
enum { LABELS = sizeof(labels) / sizeof(labels[0]), ESSENTIAL = 18 };
#elif KUI_TOY_WIRE_IDLE
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

#if KUI_TOY_WIRE_TITLES
/* Test 79: a report's text as lines for the screen. Every character of the
 * body is shown once and in order; no line is wider than the screen, none
 * is empty and none begins or ends with a space; and the lines end where
 * the body does. Returns how many lines, and how many of them were cut. */
static unsigned screen_cut;
static unsigned screen(const char *text, size_t body) {
    const char *at = text, *const end = text + KUI_TOY_REPORT_FILE_BYTES - 12u;
    static char shown[KUI_TOY_REPORT_FILE_BYTES], wanted[KUI_TOY_REPORT_FILE_BYTES];
    char row[TOY_PAGE_PLACES + 1u];
    size_t shown_n = 0, wanted_n = 0;
    unsigned lines = 0;
    screen_cut = 0;
    for(const char *line = at; toy_report_screen_line(&at, end, row); line = at, ++lines) {
        const size_t n = strlen(row);
        assert(n && n <= TOY_PAGE_PLACES && row[0] != ' ' && row[n - 1u] != ' ' && !memcmp(line, row, n));
        /* A line that is cut is cut at a space, which is not shown. (A
         * row's label and its values have no word of fifty places.) */
        if(line[n] != '\n') { ++screen_cut; assert(line[n] == ' ' && at == line + n + 1u); }
        for(size_t i = 0; i < n; ++i) if(row[i] != ' ') shown[shown_n++] = row[i];
    }
    for(const char *p = text; p < text + body; ++p) if(*p != ' ' && *p != '\n') wanted[wanted_n++] = *p;
    assert(shown_n == wanted_n && !memcmp(shown, wanted, wanted_n) && at == text + body);
    return lines;
}

/* The splitter by itself, on lines no report has: one of exactly fifty
 * places, one of fifty-one with no space in it, one of two words of
 * thirty, and a short last one. (Every line of the text ends with a
 * newline; its padding follows the last.) Never more than fifty places to
 * a row, the row closed behind them, nothing lost and nothing twice. */
static void screen_lines(void) {
    char text[KUI_TOY_REPORT_FILE_BYTES + 1u];
    memset(text, ' ', sizeof(text));
    size_t n = 0;
    memset(text + n, 'A', 50u); n += 50u; text[n++] = '\n';
    memset(text + n, 'B', 51u); n += 51u; text[n++] = '\n';
    memset(text + n, 'C', 30u); n += 30u; text[n++] = ' '; memset(text + n, 'D', 30u); n += 30u; text[n++] = '\n';
    memset(text + n, 'E', 7u); n += 7u; text[n++] = '\n';
    const char *at = text, *const end = text + KUI_TOY_REPORT_FILE_BYTES - 12u;
    char row[TOY_PAGE_PLACES + 1u];
    static const struct { char letter; unsigned places; } want[] = {{'A', 50u}, {'B', 50u}, {'B', 1u}, {'C', 30u}, {'D', 30u}, {'E', 7u}};
    for(unsigned i = 0; i < sizeof(want) / sizeof(want[0]); ++i) {
        memset(row, '!', sizeof(row));
        assert(toy_report_screen_line(&at, end, row));
        assert(strlen(row) == want[i].places && row[0] == want[i].letter && row[want[i].places - 1u] == want[i].letter);
    }
    assert(!toy_report_screen_line(&at, end, row) && at == text + n);
    /* A text that fills its room to the last place before the end mark. */
    memset(text, 'F', KUI_TOY_REPORT_FILE_BYTES - 12u);
    at = text;
    unsigned rows = 0;
    while(toy_report_screen_line(&at, end, row)) { assert(strlen(row) <= TOY_PAGE_PLACES); ++rows; }
    assert(at == end && rows == (KUI_TOY_REPORT_FILE_BYTES - 12u + TOY_PAGE_PLACES - 1u) / TOY_PAGE_PLACES);
}

/* The page loop on a report's text: what the worker shows where the
 * report could not be saved. Each page is the heading, at most twelve of
 * the text's lines and the line for Start; the pages together are the
 * text's lines, all of them and in order; behind the last the first comes
 * again; the screen is put back behind every page. From the page on which
 * Start could not restart the console, the last line says so. */
static void pages(const char *text, size_t body, unsigned lines) {
    static const struct retail_display_state display;
    const unsigned per_round = (lines + TOY_PAGE_LINES - 1u) / TOY_PAGE_LINES;
    shown_lines = shown_pages = restores = 0u; page_first[0] = 0u; restored = NULL;
    pages_wanted = 2u * per_round + 1u; busy_from = per_round + 1u;
    if(!setjmp(pages_done)) toy_report_pages(text, &display, 300u);
    assert(shown_pages == pages_wanted && restores == pages_wanted - 1u && restored == &display);
    const char *at = text, *const end = text + KUI_TOY_REPORT_FILE_BYTES - 12u;
    char row[TOY_PAGE_PLACES + 1u];
    unsigned seen = 0;
    for(unsigned page = 0; page < pages_wanted; ++page) {
        const unsigned first = page_first[page], count = (page + 1u < pages_wanted ? page_first[page + 1u] : shown_lines) - first;
        if(page % per_round == 0u) { at = text; seen = 0u; } /* a new round */
        /* The screen has sixteen rows; a page has its own two and the text's. */
        assert(count >= 3u && count <= TOY_PAGE_LINES + 2u);
        assert(!strcmp(shown_line[first], "REPORT NOT SAVED / FILM EVERY PAGE"));
        assert(!strcmp(shown_line[first + count - 1u],
            page + 1u >= busy_from + 1u ? "RESTART BUSY / TRY AGAIN OR POWER OFF" : "START / REBOOT TO K-UI CD"));
        for(unsigned i = 1u; i + 1u < count; ++i, ++seen) {
            assert(toy_report_screen_line(&at, end, row) && !strcmp(shown_line[first + i], row));
        }
        /* Only a round's last page may be short of twelve lines. */
        assert(count == TOY_PAGE_LINES + 2u || page % per_round == per_round - 1u);
        if(page % per_round == per_round - 1u) assert(seen == lines && at == text + body);
    }
}
#endif

#if KUI_TOY_CDDA_CARD
/* Test 85: the audio's rows. They stand behind the drive's, the sound
 * output's first; the window stream's rows from V to W FILL are not written.
 * What the rest of this file says of those rows holds for the builds that
 * write them, so a build that reads the audio is checked here alone: with
 * nothing counted, with each field of the new rows at a value that shows its
 * place and its cap, and with every value of the report at eight digits. */
static int card_report(void) {
    static kui_retail_async_region_type region;
    static struct kui_native_ra_public ra;
    char text[KUI_TOY_REPORT_FILE_BYTES + 1u];
    unsigned rows;
    size_t body = format(&ra, &region, text, &rows);
    const size_t empty = body;
    static const char *const gone[] = {"V SCI", "VMAX", "D N T", "W START", "W BYTES", "W BLK", "S C S F", "W FILL",
        "SUBMIT", "IP ON", "BP ON", "BIG N", "K LIGHT", "P RING"};
    for(unsigned i = 0; i < sizeof(gone) / sizeof(gone[0]); ++i) assert(!strstr(text, gone[i]));
    assert(strstr(text, "X OW E I C W O\n0 0 0 0 0 0\nSD BLK PIO OVR CRC DECL\n0 0 0 0 0\n" XC_ROW_ZERO ""
                        "CD P P2 PA RE END LAP\n0 0 0 0 0 0\nCD ST RD KEEP REF LOST PJ\n0 0 0 0 0 0\n"
#if KUI_TOY_CDDA_SURVEY
                        "CD SV DT B N WIN SKIP\n0 0 0 0 0 0\n"
#endif
#if KUI_TOY_CDDA_OUTPUT
                        "CD O ST V ID RING N\n0 0 0 0 " RING_NONE "\nCD O KEY RST UND CLK DRP TK\n0 0 0 0 0 0\n"
                        "CD O HB LONG TICK KB SKIP LV\n0 0 0 0 0 0\nCD O MX LD L2 AT TM LOST\n0 0 0 0 0 0\n"
#endif
                        "CD C BLK RB RR PIO WAIT YLD\n0 0 0 0 0 0\nCD C HD HO HX REF DCL ERR\n0 0 0 0 0 0\n"
                        "CD C VIS TICK HB LONG RND SEEK\n0 0 0 0 0 0\n"));
    /* A title that is not Toy Commander: no movie rows, and the report is
     * at most three pages on the screen. */
    assert(essential_rows == ESSENTIAL - 4u && rows == LABELS - MOVIE_ROWS);
    assert(rows == 10u + KUI_TOY_CDDA_SURVEY + 4u * KUI_TOY_CDDA_OUTPUT && 4u + 2u * rows <= 3u * TOY_PAGE_LINES);
    const unsigned lines = screen(text, body);
    assert(lines == 4u + 2u * rows && !screen_cut);
    pages(text, body, lines);

    /* Each field where it belongs, and held at its cap. */
    kui_toy_cdda_mail = (struct kui_toy_cdda_mail){.blocks = 0x123456, .rebuilt = 0x12, .rereads = 0x34,
        .seek_blocks = 0x56, .seek_waits = 0x78, .yields = 0x9a, .data_halts = 0x11, .other_halts = 0x22,
        .external_halts = 0x33, .refused = 0x10004, .why = 5u, .declined = 0x44, .first_error = 0x12345,
        .first_detail = 0x6789, .visits = 0xabcde, .visit_ticks = 0x1234567, .heartbeats = 0x54321,
        .longest = 0x8ac, .round_longest = 0x123, .seek_ticks = 0x45678,
        .wire = {.bad_crc = 0x12, .lost_frame = 0x70000, .ambiguous = 3u, .holed = 0x45, .cuts = 6u, .fulls = 0x789}};
#if KUI_TOY_CDDA_OUTPUT
    kui_toy_cdda_output = (struct kui_toy_cdda_output){.phase = 3u, .why = 8u, .why_detail = 0x12345, .family = 2u,
        .rearms = 40u, .seen_version = 0xffffffffu, .seen_hash = 0xffffu, .channel = {62u, 63u}, .ring = 0x1effa0,
        .frames = 16384u, .places = 300u, .keys = 0x1234, .restarts = 0x56, .underruns = 0x12, .clock_faults = 0x34,
        .drops = 0x10005, .rekeys = 7u, .takes = 3u, .fixes = 0x20000, .heartbeats = 0x54321, .longest = 0x999,
        .bus_ticks = 0x7654321, .bytes = 0x12345678, .skipped_bl = 300u, .skipped_dma = 2u, .timeouts = 256u,
        .faults = 1u, .level_last = {0x0f1f, 0x0f0f}, .mixer = {0x0a1f, 0x0a0f}, .lead_least = 0x0a00, .gap_longest = 0x0500 << 6,
        .lead_run = 0x0b00, .shadow = 0x00400030, .fallbacks = 70000u, .chip_ticks = 0x13579, .tmu_ticks = 0xe9ac1c3, .spare = 7u,
        .lost_table = 2u, .refusals = 300u, .lost_guard = 1u, .skipped_stack = 3u};
#endif
    body = format(&ra, &region, text, &rows);
#if KUI_TOY_CDDA_OUTPUT
    assert(strstr(text, "CD O ST V ID RING N\n2345F283 FFFFFFFF 3F3EFFFF 1EFFA0 " RING_KIND "FF4000\n"
                        "CD O KEY RST UND CLK DRP TK\n1234 56 12 34 7FFFF FFFF0003\n"
                        "CD O HB LONG TICK KB SKIP LV\n54321 999 7654321 48D15 FF02FF01 F0F0F1F\n"
                        "CD O MX LD L2 AT TM LOST\nA0F0A1F 5000A00 FFFF0B00 13579 E9AC1C3 301FF02\n"
                        "CD C BLK"));
    /* Test 89: the audio stream's own reasons, 16 bits each and capped. */
    assert(strstr(text, "CD W BL AH CU\nFFFF0012 450003 7890006\n"));
#endif
    assert(strstr(text, "CD C BLK RB RR PIO WAIT YLD\n123456 12 34 56 78 9A\n"
                        "CD C HD HO HX REF DCL ERR\n11 22 33 5FFFF 44 67892345\n"
                        "CD C VIS TICK HB LONG RND SEEK\nABCDE 1234567 54321 8AC 123 45678\n"));
    const size_t full = body;

    /* A long session: every other row's values at eight digits, the
     * audio's as above. Every row of the audio is still written. (A build
     * with the survey's row and the output's four would have no such room;
     * none is made.) */
    memset(&kui_toy_cdda_drive, 0xff, sizeof(kui_toy_cdda_drive));
#if KUI_TOY_CDDA_SURVEY
    memset(&kui_toy_cdda_survey, 0xff, sizeof(kui_toy_cdda_survey));
#endif
    memset(&wire, 0xff, sizeof(wire));
    memset(&ra, 0xff, sizeof(ra));
    memset(&region, 0xff, sizeof(region));
    body = format(&ra, &region, text, &rows);
    const size_t wide = body;
#if !(KUI_TOY_CDDA_SURVEY && KUI_TOY_CDDA_OUTPUT)
    assert(strstr(text, "CD C VIS TICK HB LONG RND SEEK\nABCDE 1234567 54321 8AC 123 45678\n"));
#endif
    /* Every value of the report at eight digits: rows are left out whole,
     * from the end, and none is cut (format() holds every row found to its
     * label and its order). The sound output's rows are all written. */
    memset(&kui_toy_cdda_mail, 0xff, sizeof(kui_toy_cdda_mail));
#if KUI_TOY_CDDA_OUTPUT
    memset(&kui_toy_cdda_output, 0xff, sizeof(kui_toy_cdda_output));
#endif
    body = format(&ra, &region, text, &rows);
    assert(body <= KUI_TOY_REPORT_FILE_BYTES - 12u && rows <= LABELS);
#if KUI_TOY_CDDA_OUTPUT && !KUI_TOY_CDDA_SURVEY
    assert(strstr(text, "CD O MX LD L2 AT TM LOST\nFFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF\n"));
#endif
    printf("PASS Test " TEST_NUMBER " report (the audio's rows%s): %zu body bytes with nothing counted, %zu with the "
           "audio's fields set, %zu of 1012 with every other row at eight digits a value and the audio's rows all "
           "written; three pages on the screen for a title that is not Toy Commander\n",
        KUI_TOY_CDDA_OUTPUT ? " and the sound output's" : "", empty, full, wide);
    return 0;
}
#endif

int main(int argc, char **argv) {
#if KUI_TOY_CDDA_CARD
    /* (The save to the card is the same code in every build, and is
     * checked with the builds whose rows the scenarios below name.) */
    (void)argc; (void)argv; (void)lease_stub; (void)toy_suite_save_report;
    screen_lines();
    return card_report();
#else
    static kui_retail_async_region_type region;
    static struct kui_native_ra_public ra;
    char text[KUI_TOY_REPORT_FILE_BYTES + 1u];
    unsigned rows;

    /* Nothing counted, no stream block yet: zeros, every row present. */
    size_t body = format(&ra, &region, text, &rows);
#if KUI_TOY_WIRE_ANY
    /* (Since Test 80 a title that is not Toy Commander, as a report of
     * zeros is, has no movie rows.) */
    assert(rows == LABELS - MOVIE_ROWS && strstr(text, "W START SEG CUT WAIT FULL\n0 0 0 0 0\n"));
#else
    assert(rows == LABELS && strstr(text, "W START SEG CUT WAIT FULL\n0 0 0 0 0\n"));
#endif
    const size_t empty = body;
#if KUI_TOY_WIRE_TITLES && KUI_TOY_CDDA_DRIVE
    /* Test 84: the drive's audio behind the commands a window met, with no
     * play: and BIG, K and P have made room. Fifteen rows. */
    assert(strstr(text, "X OW E I C W O\n0 0 0 0 0 0\nSD BLK PIO OVR CRC DECL\n0 0 0 0 0\n" XC_ROW_ZERO ""
                        "CD P P2 PA RE END LAP\n0 0 0 0 0 0\nCD ST RD KEEP REF LOST PJ\n0 0 0 0 0 0\n"
#if KUI_TOY_CDDA_SURVEY
                        "CD SV DT B N WIN SKIP\n0 0 0 0 0 0\n"
#endif
                        "V SCI TMR GAME N TICKS\n0 0 0 0 0 0\nVMAX SCI TMR GAME D\n0 0 0 0\nD N T SEEK\n"));
    assert(!strstr(text, "SUBMIT") && !strstr(text, "I FIRST") && !strstr(text, "IP ON") && !strstr(text, "BP ON"));
    assert(!strstr(text, "BIG N") && !strstr(text, "K LIGHT") && !strstr(text, "P RING"));
    assert(rows == 15u + KUI_TOY_CDDA_SURVEY && 4u + 2u * rows <= 3u * TOY_PAGE_LINES);
#elif KUI_TOY_WIRE_TITLES
    /* A title that is not the verified Toy Commander says so. */
    assert(strstr(text, "BIG N S BACK TOY\n0 0 0 0\nSD BLK PIO OVR CRC DECL\n0 0 0 0 0\n"));
#if KUI_TOY_WIRE_QUERY
    /* Test 80: no row of Toy Commander's movie player for it, and the
     * longest visits behind the visits. Sixteen rows: three pages on the
     * screen, where Test 79's report took four. */
    assert(strstr(text, "SD BLK PIO OVR CRC DECL\n0 0 0 0 0\n" XC_ROW_ZERO "K LIGHT EDC MEND MB LB ORD\n"));
    assert(!strstr(text, "SUBMIT") && !strstr(text, "I FIRST") && !strstr(text, "IP ON") && !strstr(text, "BP ON"));
    assert(strstr(text, "V SCI TMR GAME N TICKS\n0 0 0 0 0 0\nVMAX SCI TMR GAME D\n0 0 0 0\nP RING"));
    assert(rows == 16u && 4u + 2u * rows == 3u * TOY_PAGE_LINES);
#endif
#endif
#if KUI_TOY_WIRE_TITLES
    /* On the screen: four lines of heading and two for each row, none cut. */
    assert(screen(text, body) == 4u + 2u * rows && !screen_cut);
    screen_lines();
    pages(text, body, 4u + 2u * rows);
#endif

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
#if KUI_TOY_WIRE_CACHED && !KUI_TOY_WIRE_FLUID
    wire.ccr = 0x105; wire.mmucr = 0;
#endif
#if KUI_TOY_WIRE_WORDS
    /* A second movie that plays its first ten seconds and its last five
     * and skips in between, as wide as each piece's byte can show. */
    for(unsigned i = 0; i < KUI_TOY_WIRE_BUCKETS; ++i) {
        const bool plays = i < 2u || i == 11u;
        wire.course_frames[i] = plays ? 161u : 9u + i;
        wire.course_sectors[i] = plays ? 2112u : 2600u + 16u * i;
#if !KUI_TOY_WIRE_FLUID
        wire.course_stops[i] = (uint16_t)(3u * i);
#endif
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
    wire.buffer_capacity = 0x100000;
    for(unsigned i = 0; i < KUI_TOY_WIRE_BUCKETS; ++i) {
        const bool plays = i < 2u || i == 11u;
        wire.course_free[i] = plays ? 0x8000u + 0x2000u * i : 0xf0000u + 0x1000u * i;
#if KUI_TOY_WIRE_FLUID
        /* The wire standing still most where the movie plays worst. */
        wire.course_pause[i] = plays ? 0x9000u * i : 0x80000u + 0x8000u * i;
#else
        wire.course_lag[i] = plays ? i : 40u + i;
#endif
    }
#if !KUI_TOY_WIRE_FLUID
    wire.update_ticks = 0x1900000;
#endif
#endif
#if KUI_TOY_WIRE_LEAN
    /* Two thirds of the sectors taken on their blocks' CRC16, a tenth of a
     * million rebuilt bytes confirmed by their parity, none refused; the
     * receiver delivering MSB first; the game's calls timed whole; and the
     * runs cut again soon after they began. */
    wire.light = 0x4c00; wire.edc = 0x25e7; wire.mends = 0x1a90; wire.mend_bad = 3; wire.light_bad = 0;
    wire.n.order = KUI_WIRE_ORDER_MSB; wire.n.undecided = 1;
#if !KUI_TOY_WIRE_FLUID
    wire.gd_calls = 0x1d700; wire.gd_ticks = 0x6a0000;
#endif
    wire.n.again[0][0] = 0x1500; wire.n.again[0][1] = 0x600; wire.n.again[0][2] = 0x380;
    wire.n.again[1][0] = 0x900; wire.n.again[1][1] = 0x410; wire.n.again[1][2] = 0x2a0;
#endif
#if KUI_TOY_WIRE_STEADY
    /* The reader's seconds by what it did: six in the ring, two feeding
     * the window, one opening streams, one copying requests out; a third
     * of the interrupt visits leaving the reading; the first skip at frame
     * 27; and the requests the game waited for with their window kept, as
     * many as Test 74's run gave windows up for. */
    wire.parse_ticks = 0x4a0000; wire.feed_ticks = 0x1c0000; wire.seek_ticks = 0xb0000;
    wire.publish_ticks = 0xd0000; wire.later = 0x5200; wire.skip_first = 0x1b;
    wire.stays = 0x35;
    /* Of those, half a second in the sectors' checks and as much in the
     * stream's own service. */
    wire.check_ticks = 0x60000; wire.service_ticks = 0x5f000;
#endif
#if KUI_TOY_WIRE_FLUID
    /* Streams ended in a tenth of a second all told. */
    wire.halt_ticks = 0x13000;
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
    wire.budget_ends = 0xa000;
#if !KUI_TOY_WIRE_FLUID
    wire.visit_max[0] = 0x1023; wire.visit_max[1] = 0x1a0; wire.visit_max[2] = 0x7ff;
    wire.restart_ticks = 0x1d0000; wire.restart_max = 0xc350;
    wire.wait_calls[0] = 3; wire.wait_ticks[0] = 0x2ee0; wire.wait_max[0] = 0x1770;
    wire.wait_calls[1] = 0x4a; wire.wait_ticks[1] = 0x1e8480; wire.wait_max[1] = 0xea60;
#endif
#else
    wire.handler_calls = 0xbe00; wire.handler_ticks = 0x1e8480; wire.handler_max = 0x1a0;
#endif
    for(unsigned i = 0; i < KUI_TOY_WIRE_STOPS; ++i) wire.stops[i] = 3u + i;
    for(unsigned i = 0; i < KUI_TOY_WIRE_DROPS; ++i) wire.drops[i] = 1u + i;
#if KUI_TOY_WIRE_STEADY
    /* As many streams not followed as the console has shown, and a movie's
     * start of windows given up for a read that was waited for. */
    wire.stops[KUI_TOY_WIRE_STOP_FRAME] = 0x12c; wire.drops[KUI_TOY_WIRE_DROP_WAITING] = 0x35;
#endif
#if KUI_TOY_WIRE_SEQ
    /* The game's ordinary loading through windows of its own, twelve
     * thousand of them: more than a whole disc read sixty sectors at a
     * time would open. The counts that go with that many windows are as
     * large. Each is a stream begun behind a block of the low reader's,
     * is given up for a read elsewhere or for a command, and has its
     * stream stopped; each serves a request or two. */
    wire.other_windows = 0x2f00; wire.other_sectors = 0x8a000;
    wire.drops[KUI_TOY_WIRE_DROP_OTHER] = 0x2e00; wire.drops[KUI_TOY_WIRE_DROP_COMMAND] = 0xfe;
    wire.stops[KUI_TOY_WIRE_STOP_CLOSE] = 0x2f03; wire.n.starts = 0x318f; wire.seek_blocks = 0x31c0;
#endif
#if KUI_TOY_WIRE_ANY
    /* Large requests through those windows: seven thousand of them, half
     * a million sectors, and three that went back to the low reader. */
    wire.large_requests = 0x1c00; wire.large_sectors = 0x7b000; wire.large_returned = 3;
#endif
#if KUI_TOY_WIRE_QUERY
    /* A game that asks for the subcode behind every read, a command to a
     * window; and the commands its windows met: GETTOC2, SEEK and NOP,
     * STOP, GETSCD and REQ_STAT. */
    wire.queries = 0x2e02; wire.command_seen[0] = UINT32_C(0x28080000); wire.command_seen[1] = 0x16;
    /* The longest pumps: an interrupt visit at its budget, a timer's
     * answer, a call of the game's over a large request; and a delivery
     * visit of a frame and a half. */
    wire.visit_longest[0] = 0x494; wire.visit_longest[1] = 0x2c; wire.visit_longest[2] = 0x1d4c;
    wire.deliver_longest = 0x4c4b;
#endif
    uint32_t *trace = ra.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
    trace[0] = 0x1100; trace[1] = 0x6600; trace[2] = 2; trace[3] = 3; trace[4] = 0x9000; trace[5] = 0xbe00;
    ra.stats.hit_requests = 0x900; ra.stats.producer_blocks = 0x7700; ra.stats.abandoned = 2;
#if KUI_TOY_WIRE_SEQ
    ra.stats.hit_requests = 0x5900; /* with the other windows' requests */
#endif
#if KUI_TOY_WIRE_TITLES
    ra.demand.owner_verified = 1u;
#endif
    region.shared.stream.stats.blocks = 0x40; region.shared.stream.stats.polled = 0x2f;
    region.shared.stream.stats.starts = 0x1f; region.shared.stream.stats.crc_errors = 0;
#if KUI_TOY_WIRE_TITLES
    region.shared.stream.stats.overruns = 0x11; wire.declined = 6;
#endif
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
#if KUI_TOY_CDDA_DRIVE
    /* A long session of music: wide counters, a play standing. */
    kui_toy_cdda_drive = (struct kui_toy_cdda_drive){.state = 1u, .generation = 0x22c, .repeat = 15u,
        .track = 3u, .plays = 0x40, .play2s = 0x12, .pauses = 0x1b, .releases = 0x1a, .ends = 0xc,
        .laps = 0xf, .reads_in_play = 0x3e8, .keeps = 0x2710, .refused = 2u, .lost = 1u, .projections = 0x4e2};
#endif
#if KUI_TOY_CDDA_SURVEY
    /* The survey row at its widest: every count past its cap, every field
     * at its eight digits. */
    kui_toy_cdda_survey = (struct kui_toy_cdda_survey){.version = 0xffffffffu, .date_hash = 0xffffu,
        .state_word = 0xffffffffu, .family = 7u, .changes = 40u, .identified = 1u, .proved = 70000u,
        .busy = {0x10000u, 0x23456u}, .unproved = 0x10001u, .window_passes = 0x8001u, .window_nonzero = 1u,
        .window_changed = {UINT32_MAX, 0xffffu}, .skipped_bl = 300u, .skipped_dma = 0x1000u,
        .timeouts = 256u, .skipped_clock = 1000u};
#endif
    body = format(&ra, &region, text, &rows);
#if KUI_TOY_WIRE_SEQ
    /* Every row but the last fits at a full run's values with the game's
     * ordinary loading as wide as above: the row of the reader's time by
     * piece is written. The first movie's player behind it is the one row
     * that may be left out. (With no window but the movies' everything
     * fits: below.) */
#if KUI_TOY_WIRE_ANY
    /* Test 78 has no row by piece: everything fits, the first movie's
     * player too. */
#if KUI_TOY_CDDA_SURVEY
    /* Test 84's survey row at its widest leaves no room for that player,
     * the last row: every other is written. */
    assert(essential_rows == ESSENTIAL && rows == LABELS - 1u && !strstr(text, "BP ON"));
    const char *last = strstr(text, "W FILL REF BAD\n1500 4 2\n");
    assert(last);
#else
    assert(essential_rows == ESSENTIAL && rows == LABELS);
    const char *last = strstr(text, "W FILL REF BAD\n1500 4 2\n");
    assert(last && strstr(text, "BP ON") > last);
#endif
    const size_t needed = (size_t)(last - text) + strlen("W FILL REF BAD\n1500 4 2\n");
#else
    assert(essential_rows == ESSENTIAL && rows >= LABELS - 1u);
    const char *last = strstr(text, "TR\n3031FFFF FFFFFFFF FFFFFF3B\n");
    assert(last && (rows == LABELS) == (strstr(text, "BP ON") != NULL));
    const size_t needed = (size_t)(last - text) + strlen("TR\n3031FFFF FFFFFFFF FFFFFF3B\n");
#endif
    assert(needed <= KUI_TOY_REPORT_FILE_BYTES - 12u);
    const bool bumper_fits = rows == LABELS;
#elif KUI_TOY_WIRE_FLUID
    /* Everything fits at a full run's values. */
    assert(essential_rows == ESSENTIAL && rows == LABELS);
    const char *last = strstr(text, "W FILL REF BAD\n1500 4 2\n");
    assert(last);
    const size_t needed = (size_t)(last - text) + strlen("W FILL REF BAD\n1500 4 2\n");
    assert(needed + 30u <= KUI_TOY_REPORT_FILE_BYTES - 12u);
#endif
#if KUI_TOY_WIRE_FLUID
    /* The last value names the build's parts: Test 75's three, without
     * LATER, and each of Test 76's own that the build has. */
    {
        char parts[64];
        const unsigned have = KUI_TOY_WIRE_STAY | KUI_TOY_WIRE_LATER << 1 | KUI_TOY_WIRE_RING << 2 |
            KUI_TOY_WIRE_QUIT << 3 | KUI_TOY_WIRE_HOT << 4 | KUI_TOY_WIRE_COPY << 5 | KUI_TOY_WIRE_GAP << 6 |
            KUI_TOY_WIRE_APART << 7 | KUI_TOY_WIRE_SEQ << 8 | KUI_TOY_WIRE_ANY << 9 | KUI_TOY_WIRE_TITLES << 10 |
            KUI_TOY_WIRE_QUERY << 11 | KUI_TOY_WIRE_TIMER << 12;
        snprintf(parts, sizeof(parts), "PIPE B I CALLS IRQ STAY PT\n2 3 9000 BE00 35 %X\n", have);
        assert(strstr(text, parts) && have == KUI_TOY_WIRE_PARTS);
    }
#if KUI_TOY_WIRE_SEQ
    /* The movies' windows and sectors as before; the sectors of the other
     * windows; how many of those were opened, and why windows were given
     * up: error, the request state, a command, a waited request, a read
     * the window does not serve. The movie's end is no reason any more. */
    assert(strstr(text, "AHEAD B I HIT EDC STOP OS\n1100 6600 5900 7700 2 8A000\n"));
    assert(strstr(text, "X OW E I C W O\n2F00 1 3 FE 35 2E00\n"));
#if KUI_TOY_WIRE_ANY
    /* The large requests a window served, their sectors, and those that
     * went back to the low reader. */
#if KUI_TOY_WIRE_TITLES
    /* And whose disc it is: the verified Toy Commander here. Behind it the
     * low reader's own totals and the stream's refused take-overs. */
#if KUI_TOY_CDDA_DRIVE
    /* Test 84: BIG has made room; the low reader's totals follow X. */
    assert(!strstr(text, "BIG N S BACK") &&
        strstr(text, "X OW E I C W O\n2F00 1 3 FE 35 2E00\nSD BLK PIO OVR CRC DECL\n40 2F 11 0 6\n"));
#else
    assert(strstr(text, "BIG N S BACK TOY\n1C00 7B000 3 1\nSD BLK PIO OVR CRC DECL\n40 2F 11 0 6\n"));
#endif
#if KUI_TOY_WIRE_QUERY
    /* The commands a window stood through, and the commands windows met
     * by their numbers' bits: 19, 27 and 29; 33, 34 and 36. */
#if KUI_TOY_CDDA_DRIVE
    /* Test 84: then the drive's audio, at the widest values a session gives
     * it, before the movie player's rows; P has made room before D. */
    assert(strstr(text, "SD BLK PIO OVR CRC DECL\n40 2F 11 0 6\nXC KEPT LO HI\n2E02 28080000 16\n"
                        "CD P P2 PA RE END LAP\n40 12 1B 1A C F\n"
                        "CD ST RD KEEP REF LOST PJ\n30F2C01 3E8 2710 2 1 4E2\n"
#if KUI_TOY_CDDA_SURVEY
                        /* Each field held at its cap: changes at 15, busy and the two
                         * counts of N at 0xFFFF, passes at 0x7FFF, 48 KiB changed, the four
                         * skips at 255; nothing spills into a neighbour. */
                        "CD SV DT B N WIN SKIP\nFFFFFFFF 7FFFFFFF FFFFFFFF FFFFFFFF FFFF0030 FFFFFFFF\n"
#endif
                        "I SUBMIT"));
    assert(strstr(text, "\nVMAX SCI TMR GAME D\n494 2C 1D4C 4C4B\nD N T SEEK"));
#else
    assert(strstr(text, "SD BLK PIO OVR CRC DECL\n40 2F 11 0 6\nXC KEPT LO HI\n2E02 28080000 16\nI SUBMIT"));
    assert(strstr(text, "\nVMAX SCI TMR GAME D\n494 2C 1D4C 4C4B\nP RING"));
#endif
#endif
#else
    assert(strstr(text, "BIG N S BACK\n1C00 7B000 3\n") && !strstr(text, "BACK TOY"));
#endif
#else
    assert(!strstr(text, "BIG N S BACK"));
#endif
#else
    assert(strstr(text, "AHEAD B I HIT EDC STOP\n1100 6600 900 7700 2\n"));
#endif
    assert(strstr(text, "I SUBMIT CADENCE MAX AT\n7B0 7AE 92345 124\n"));
    assert(strstr(text, "I FIRST LAST SKIP EXIT SF\n23576 7B0 2 FFFFF843 1B\n"));
    assert(strstr(text, "B SUBMIT CADENCE MAX AT\n14B 14A 12ABC F6\n"));
    assert(strstr(text, "IP ON OT OC OI SN ST\n290 16E3600 1A00000 1C0000 520 9000\n"));
#if !KUI_TOY_WIRE_SEQ
    assert(strstr(text, "BP ON OT OC OI SN ST\n14B 4F0000 7E0000 30000 0 0\n"));
#endif
#if KUI_TOY_CDDA_DRIVE
    assert(!strstr(text, "K LIGHT") && !strstr(text, "P RING"));
#else
    assert(strstr(text, "K LIGHT EDC MEND MB LB ORD\n4C00 25E7 1A90 3 0 3\n"));
#endif
    assert(strstr(text, "V SCI TMR GAME N TICKS\nC67C D000 26265 3A0000 4E0000 5B0000\n"));
#if !KUI_TOY_CDDA_DRIVE
    /* The reader's ticks by what it was doing, and those spent ending streams. */
    assert(strstr(text, "P RING FEED CHK OUT SVC HALT\n4A0000 1C0000 60000 D0000 5F000 13000\n"));
#endif
#if KUI_TOY_WIRE_SEQ
    assert(strstr(text, "D N T SEEK\n1E000 2A0000 B0000\n") && !strstr(text, "CAP"));
#else
    /* The player's buffer's capacity stands beside the rows read by it. */
    assert(strstr(text, "D N T SEEK CAP\n1E000 2A0000 B0000 100000\n"));
#endif
#if KUI_TOY_WIRE_ANY
    /* Nothing by piece of the course. */
    assert(!strstr(text, "TF\n") && !strstr(text, "TD\n") && !strstr(text, "TW\n") && !strstr(text, "TB\n") &&
        !strstr(text, "TR\n"));
#else
    assert(strstr(text, "TF\nA1A10B0C D0E0F10 111213A1\n"));
    assert(strstr(text, "TD\n8484A4A5 A6A7A8A9 AAABAC84\n"));
    assert(strstr(text, "TW\n6263 64656667 68696A08\n"));
    /* The player's buffer, the most that was free in 4 KiB units: nearly
     * all of its megabyte in the pieces that skip. The wire's standstill. */
    assert(strstr(text, "TB\n80AF2F3 F4F5F6F7 F8F9FA1E\n"));
#endif
#if KUI_TOY_WIRE_SEQ
    assert(!strstr(text, "TP\n"));
#else
    assert(strstr(text, "TP\n22426 282A2C2E 30323418\n"));
#endif
#if KUI_TOY_WIRE_SEQ
    assert(strstr(text, "W START SEG CUT WAIT FULL\n318F 2800 83D7 6622 300\n"));
    assert(strstr(text, "W BLK FIX UNFIX HOLED NOCRC SEEK\n24C00 6100 5 1C00 90 31C0\n"));
    assert(strstr(text, "S C S F LF AM BC\n2F03 5 12C 4A 2 3\n"));
    assert(strstr(text, "W FILL REF BAD\n1500 4 2\n"));
#else
    assert(strstr(text, "W START SEG CUT WAIT FULL\n1F 2800 83D7 6622 300\n"));
    assert(strstr(text, "W BLK FIX UNFIX HOLED NOCRC SEEK\n24C00 6100 5 1C00 90 2F\n"));
    assert(strstr(text, "S C S F LF AM BC\n3 5 12C 4A 2 3\n"));
#endif
#if !KUI_TOY_WIRE_ANY
    /* The reader's pumps by piece: Test 75's two rows added. */
    assert(strstr(text, "TR\n3031FFFF FFFFFFFF FFFFFF3B\n"));
#endif
#if KUI_TOY_WIRE_SEQ
    /* The same run with the movies' four windows and no other, as every
     * console run up to Test 76 was: every row, the first movie's player
     * at the end. */
    size_t movies_alone;
    {
        wire.other_windows = wire.other_sectors = 0u;
        wire.drops[KUI_TOY_WIRE_DROP_OTHER] = 2u; wire.drops[KUI_TOY_WIRE_DROP_COMMAND] = 0u;
        wire.stops[KUI_TOY_WIRE_STOP_CLOSE] = 4u; wire.n.starts = 0x290; wire.seek_blocks = 0x2c7;
        ra.stats.hit_requests = 0x900;
        char movies[KUI_TOY_REPORT_FILE_BYTES + 1u];
        unsigned movie_rows;
        const size_t movie_body = format(&ra, &region, movies, &movie_rows);
        movies_alone = movie_body;
        assert(essential_rows == ESSENTIAL && movie_rows == LABELS && movie_body <= KUI_TOY_REPORT_FILE_BYTES - 12u);
        const char *bumper = strstr(movies, "BP ON OT OC OI SN ST\n14B 4F0000 7E0000 30000 0 0\n");
        assert(bumper && bumper > strstr(movies, KUI_TOY_WIRE_ANY ? "W FILL REF BAD\n" : "TR\n"));
        assert(strstr(movies, "X OW E I C W O\n0 1 3 0 35 2\n"));
        essential_rows = ESSENTIAL;
    }
#endif
    /* Test 75's rows of the GD calls and of the windows given up are gone,
     * and its two rows of the reader's time by caller. */
    assert(!strstr(text, "G N T") && !strstr(text, "X E N") && !strstr(text, "TI\n") && !strstr(text, "TG\n"));
#elif KUI_TOY_WIRE_STEADY
    /* Everything fits at a full run's values. */
    assert(essential_rows == ESSENTIAL && rows == LABELS);
    const char *last = strstr(text, "X E N I C W O\n1 2 3 4 35 6\n");
    assert(last);
    const size_t needed = (size_t)(last - text) + strlen("X E N I C W O\n1 2 3 4 35 6\n");
    assert(needed + 60u <= KUI_TOY_REPORT_FILE_BYTES - 12u);
    /* The last value names the build's parts: all three here unless one was left out. */
    {
        char parts[64];
        snprintf(parts, sizeof(parts), "PIPE B I CALLS IRQ STAY PT\n2 3 9000 BE00 35 %X\n",
            (unsigned)(KUI_TOY_WIRE_STAY | KUI_TOY_WIRE_LATER << 1 | KUI_TOY_WIRE_RING << 2));
        assert(strstr(text, parts));
    }
    assert(strstr(text, "AHEAD B I HIT EDC STOP\n1100 6600 900 7700 2\n"));
    assert(strstr(text, "I SUBMIT CADENCE MAX AT\n7B0 7AE 92345 124\n"));
    /* The first skipped frame comes from the player's own counters now. */
    assert(strstr(text, "I FIRST LAST SKIP EXIT SF\n23576 7B0 2 FFFFF843 1B\n"));
    assert(strstr(text, "B SUBMIT CADENCE MAX AT\n14B 14A 12ABC F6\n"));
    assert(strstr(text, "IP ON OT OC OI SN ST\n290 16E3600 1A00000 1C0000 520 9000\n"));
    assert(strstr(text, "BP ON OT OC OI SN ST\n14B 4F0000 7E0000 30000 0 0\n"));
    assert(strstr(text, "K LIGHT EDC MEND MB LB ORD\n4C00 25E7 1A90 3 0 3\n"));
    assert(strstr(text, "V SCI TMR GAME N TICKS\nC67C D000 26265 3A0000 4E0000 5B0000\n"));
    /* The reader's ticks by what it was doing, and the visits that left
     * the reading for later. */
    assert(strstr(text, "P RING FEED CHK OUT SVC LATER\n4A0000 1C0000 60000 D0000 5F000 5200\n"));
    assert(strstr(text, "D N T G N T SEEK\n1E000 2A0000 1D700 6A0000 B0000\n"));
    assert(strstr(text, "TF\nA1A10B0C D0E0F10 111213A1\n"));
    assert(strstr(text, "TD\n8484A4A5 A6A7A8A9 AAABAC84\n"));
    assert(strstr(text, "TW\n6263 64656667 68696A08\n"));
    assert(strstr(text, "W START SEG CUT WAIT FULL\n1F 2800 83D7 6622 300\n"));
    assert(strstr(text, "W BLK FIX UNFIX HOLED NOCRC SEEK\n24C00 6100 5 1C00 90 2F\n"));
    /* Why streams ended, and what could not be followed about them. */
    assert(strstr(text, "S C S F LF AM BC\n3 5 12C 4A 2 3\n"));
    assert(strstr(text, "W FILL REF BAD\n1500 4 2\n"));
    assert(strstr(text, "TI\n24252627 28292A2B 2C2D2E2F\n"));
    assert(strstr(text, "TG\nC0CFFFF FFFFFFFF FFFFFF0C\n"));
    /* Test 74's rows of the chunks, the idle calls and the runs cut again are gone. */
    assert(!strstr(text, "IC RDY") && !strstr(text, "U S W D") && !strstr(text, "R C8"));
#elif KUI_TOY_WIRE_LEAN
    /* Everything fits at a full run's values. */
    assert(essential_rows == ESSENTIAL && rows == LABELS);
    const char *last = strstr(text, "W BLK FIX UNFIX HOLED NOCRC SEEK\n24C00 6100 5 1C00 90 2F\n");
    assert(last);
    const size_t needed = (size_t)(last - text) + strlen("W BLK FIX UNFIX HOLED NOCRC SEEK\n24C00 6100 5 1C00 90 2F\n");
    assert(needed + 100u <= KUI_TOY_REPORT_FILE_BYTES - 12u);
    assert(strstr(text, "PIPE B I CALLS IRQ\n2 3 9000 BE00\n"));
    assert(strstr(text, "AHEAD B I HIT EDC STOP\n1100 6600 900 7700 2\n"));
    assert(strstr(text, "I SUBMIT CADENCE MAX AT\n7B0 7AE 92345 124\n"));
    assert(strstr(text, "I FIRST LAST SKIP EXIT WF SF\n23576 7B0 2 FFFFF843 BB C4\n"));
    assert(strstr(text, "B SUBMIT CADENCE MAX AT\n14B 14A 12ABC F6\n"));
    assert(strstr(text, "IP ON OT OC OI SN ST\n290 16E3600 1A00000 1C0000 520 9000\n"));
    assert(strstr(text, "BP ON OT OC OI SN ST\n14B 4F0000 7E0000 30000 0 0\n"));
    assert(strstr(text, "IC RDY WGD NOGD\n7B0 1234 56\n"));
    /* How the sectors were checked, and the receiver's bit order: 3 is
     * MSB first with a block's CRC16 passed that way. */
    assert(strstr(text, "K LIGHT EDC MEND MB LB ORD\n4C00 25E7 1A90 3 0 3\n"));
    assert(strstr(text, "V SCI TMR GAME N TICKS\nC67C D000 26265 3A0000 4E0000 5B0000\n"));
    assert(strstr(text, "U S W D G S W D\n2D500 1200 4A00 1D100 900 4100\n"));
    assert(strstr(text, "D N T G N T\n1E000 2A0000 1D700 6A0000\n"));
    assert(strstr(text, "TF\nA1A10B0C D0E0F10 111213A1\n"));
    assert(strstr(text, "TD\n8484A4A5 A6A7A8A9 AAABAC84\n"));
    assert(strstr(text, "TW\n6263 64656667 68696A08\n"));
    assert(strstr(text, "W START SEG CUT WAIT FULL\n1F 2800 83D7 6622 300\n"));
    assert(strstr(text, "R C8 32 128 W8 32 128\n1500 600 380 900 410 2A0\n"));
    assert(strstr(text, "W FILL BYTES MOST REF BAD\n1500 6A00 1D 4 2\n"));
    assert(strstr(text, "TI\n24252627 28292A2B 2C2D2E2F\n"));
    assert(strstr(text, "TG\nC0CFFFF FFFFFFFF FFFFFF0C\n"));
    /* Test 73's rows of the player's buffer and lag are gone. */
    assert(!strstr(text, "IB SKU") && !strstr(text, "TB\n") && !strstr(text, "TL\n"));
#elif KUI_TOY_WIRE_IDLE
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
#if KUI_TOY_WIRE_TITLES
    /* At full width a row of six values has 53 places: cut at its last
     * space, the sixth value on a line of its own. */
    const unsigned widest_lines = screen(text, body), widest_cut = screen_cut;
    assert(widest_cut && widest_lines == 4u + 2u * rows + widest_cut);
    pages(text, body, widest_lines);
#endif

    /* A block that is not the stream's (never written) reads as zeros. */
    wire.magic = 0;
    (void)format(&ra, &region, text, &rows);
    assert(strstr(text, "W START SEG CUT WAIT FULL\n0 0 0 0 0\n"));
    assert(!toy_suite_save_report(&ra, &region));
#if KUI_TOY_WIRE_TITLES
    /* Test 79: with no report file the armed worker still asks the
     * resident for the card, once, with the report's own transaction: the
     * loan is what ends a card stream still running. Nothing counts as
     * saved. (Unarmed, above, it asks for nothing.) */
    assert(!leases && !config.toy_report.test);
    config.sd_lease = (uintptr_t)lease_stub;
    state.armed = true;
    assert(!toy_suite_save_report(&ra, &region) && leases == 1u);
    /* The text is in the worker's buffer for the pages all the same. */
    assert(!memcmp(issued_chain, "K-UI TOY COMMANDER REPORT\nTEST " TEST_NUMBER "\n", 34u));
    state.pending = true;
    assert(!toy_suite_save_report(&ra, &region) && leases == 1u);
    state.pending = false; state.armed = false;
#endif
#if KUI_TOY_WIRE_TITLES
    printf("  the report as lines for the screen: every character once and in order, no line over %u places; "
           "%u lines at full width, %u of them cut at a space\n", (unsigned)TOY_PAGE_PLACES, widest_lines, widest_cut);
    printf("  the page loop: heading, at most %u lines and the line for Start to a page, every line of the text in "
           "order, round after round; the busy line once Start was refused; without a report file the card is "
           "asked for once and nothing counts as saved\n", (unsigned)TOY_PAGE_LINES);
#endif
    printf("PASS Test " TEST_NUMBER " report: %zu body bytes with nothing counted, %zu with a full run's values "
           "(essential rows end at %zu of 1012); at full width %u of the %u essential rows are written "
           "and no row is ever cut\n", empty, full, needed, widest, (unsigned)ESSENTIAL);
#if KUI_TOY_WIRE_SEQ
    printf("  with twelve thousand windows opened by ordinary reads beside the movies' the first movie's player, "
           "the last row, %s; with the movies' windows alone every row fits (%zu bytes)\n",
        bumper_fits ? "still fits" : "is the one row left out", movies_alone);
#endif
    return 0;
#endif
}
