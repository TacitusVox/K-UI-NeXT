/* SPDX-License-Identifier: GPL-3.0-only */
/* Test 87: the window of one heartbeat's visits for a sound output
 * (src/loader/toy_cdda_card.c, kui_toy_cdda_card_heartbeat).
 *
 * The heartbeat makes one visit, and another only for a stream that has
 * just handed bytes on while the longest such visit seen still fits its
 * soft window. For the consumer that drops the bytes the window is the
 * family's share of the reader's: half of it under the early SDK's drivers.
 * A sound output has the whole of the reader's window whatever the family's
 * share, whether it says it runs low or not: how many visits a heartbeat
 * makes is then the ring's room. This runs the worker's heartbeat as
 * compiled with a sound output, with the auxiliary, the clock and the
 * family's share in the test's hands, and counts the visits. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kui/native_readahead.h"
#include "kui/toy_cdda_card.h"
#include "kui/toy_cdda_mail.h"
#include "kui/toy_cdda_player.h"

_Static_assert(KUI_TOY_CDDA_CARD_WINDOW_TICKS == KUI_NATIVE_RA_PIO_WINDOW_TICKS,
    "The window a sound output has is the reader's whole soft window");
#define CHECK(x) do { ++checks; if(!(x)) { \
    fprintf(stderr, "test_toy_cdda_card_window.c:%d: Assertion failed: %s\n", __LINE__, #x); exit(1); } } while(0)
static unsigned long checks;

struct kui_toy_cdda_drive kui_toy_cdda_drive;
static uint32_t counter = UINT32_C(0xfff00000);   /* TMU0, counting down */
static uint32_t window;                           /* the family's share */
static uint32_t cost;                             /* what a visit takes */
static unsigned visits, takes;
static uint32_t taken;
static bool streaming = true, last;                /* a visit streams; it hands the segment's last bytes on */

uint32_t kui_native_readahead_aux_test_read(uint32_t address, unsigned width);
uint32_t kui_native_readahead_aux_test_read(uint32_t address, unsigned width) {
    (void)width;
    /* The Toy timer profile, and the count. */
    if(address == UINT32_C(0xffc00000))return 0x0e0au;
    if(address == UINT32_C(0xffd80008))return UINT32_MAX;
    if(address == UINT32_C(0xffd80010))return 2u;
    if(address == UINT32_C(0xffd80004))return 1u;
    CHECK(address == UINT32_C(0xffd8000c));
    return counter;
}
uint32_t kui_toy_cdda_card_window(void) { return window; }
/* A visit of the auxiliary's: it takes the segment up and hands two blocks
 * on, as long as it is asked for two. */
void kui_toy_cdda_card_tick(void) {
    struct kui_toy_cdda_mail *m = &kui_toy_cdda_mail;
    ++visits;
    counter -= cost;
    CHECK(m->magic == KUI_TOY_CDDA_MAIL_MAGIC && !m->sink_bytes);
    m->have_generation = m->want_generation;
    m->card = last ? KUI_TOY_CDDA_CARD_DONE : streaming ? KUI_TOY_CDDA_CARD_STREAMING : KUI_TOY_CDDA_CARD_SEEKING;
    if((streaming || last) && m->want_blocks >= 2u)m->sink_bytes = 1024u;
}
static void take(void *context, const uint8_t *bytes, uint32_t count, uint32_t offset) {
    (void)context;
    CHECK(bytes == kui_toy_cdda_mail.sink && count == 1024u && offset == taken);
    taken += count;
    ++takes;
}
static unsigned beat(bool low, uint32_t room) {
    const struct kui_toy_cdda_want want = {7u, 3u, 1000u, 9000u, low};
    visits = 0u;
    counter -= 20000u;
    const uint32_t state = kui_toy_cdda_card_heartbeat(&want, room, take, 0);
    CHECK(state == (last ? KUI_TOY_CDDA_CARD_DONE : streaming ? KUI_TOY_CDDA_CARD_STREAMING : KUI_TOY_CDDA_CARD_SEEKING));
    /* (What the consumer says of its ring goes on to the auxiliary as it
     * is: an idle read-ahead window is given up on that word alone.) */
    CHECK(kui_toy_cdda_mail.yield_ok == low);
    return visits;
}

int main(void) {
    cost = 300u;
    /* The early SDK's share, half the reader's window: 1,172 ticks. The
     * first heartbeat learns what a visit takes. */
    window = KUI_NATIVE_RA_PIO_WINDOW_TICKS / 2u;
    (void)beat(false, 1u << 20);
    CHECK(kui_toy_cdda_mail.round_longest == 300u);
    /* The sound output has the whole window all the same: seven visits
     * (2,100 ticks, and one more visit's 300 would not fit 2,344), where
     * the share would end them at three. Whether the ring runs low or holds
     * enough makes no difference to it. */
    CHECK(beat(false, 1u << 20) == 7u);
    CHECK(beat(true, 1u << 20) == 7u);
    CHECK(beat(false, 1u << 20) == 7u);
    /* The whole window is the most there is: under the newer drivers,
     * whose share it is, the same seven. */
    window = KUI_NATIVE_RA_PIO_WINDOW_TICKS;
    CHECK(beat(false, 1u << 20) == 7u && beat(true, 1u << 20) == 7u);
    /* A family's share larger than the reader's window gives no more. */
    window = 4u * KUI_NATIVE_RA_PIO_WINDOW_TICKS;
    CHECK(beat(false, 1u << 20) == 7u);
    /* A heartbeat asks for no more than there is room for, */
    window = KUI_NATIVE_RA_PIO_WINDOW_TICKS / 2u;
    CHECK(beat(true, 2048u) == 2u && beat(false, 2048u) == 2u && beat(true, 1024u) == 1u && beat(true, 511u) == 0u);
    /* makes no second visit behind one that did not stream: one that
     * handed nothing on, or one that handed the segment's last bytes on, */
    streaming = false;
    CHECK(beat(true, 1u << 20) == 1u && beat(false, 1u << 20) == 1u);
    last = true;
    CHECK(beat(true, 1u << 20) == 1u && beat(false, 1u << 20) == 1u);
    streaming = true;
    last = false;
    CHECK(beat(false, 1u << 20) == 7u);
    /* reckons with the longest visit seen: with visits of 700 ticks three
     * fit (2,100, and one more would not), */
    cost = 700u;
    (void)beat(false, 1u << 20);
    CHECK(kui_toy_cdda_mail.round_longest == 700u && beat(false, 1u << 20) == 3u);
    /* and does nothing with no auxiliary bound. */
    window = 0u;
    visits = 0u;
    const struct kui_toy_cdda_want want = {7u, 3u, 1000u, 9000u, true};
    CHECK(kui_toy_cdda_card_heartbeat(&want, 1u << 20, take, 0) == KUI_TOY_CDDA_CARD_IDLE && !visits);
    CHECK(takes * 1024u == taken && kui_toy_cdda_mail.visits);
    printf("PASS the audio's window with a sound output: the reader's whole window whatever the family's share and "
        "whatever the ring holds (7 visits of 300 ticks in 2,344, where half of it would end them at 3); %lu checks\n", checks);
    return 0;
}
