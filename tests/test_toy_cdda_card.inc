/* SPDX-License-Identifier: GPL-3.0-only
 * Test 85 (CARD): a CD-music track's audio from the card, between the game's
 * own reads, through the real low reader, auxiliary, card stream and worker's
 * heartbeat (src/loader/toy_cdda_aux_card.inc, src/loader/toy_cdda_card.c)
 * on the shared card model. Included by test_toy_wire_integration.c.
 *
 * The disc has audio tracks behind the fixture's own, their files on the
 * card whole or in pieces, of 2,352-byte sectors or 2,448 with subchannels.
 * The game plays them; the worker's heartbeat runs once a frame; the
 * consumer with no sound output takes the bytes, and every byte it takes is
 * held here against the image: the next byte of the play's audio, none
 * twice, none left out. The game's own reads beside it must be the image's
 * to the byte. None of this is a console result: the model lands a run of
 * the stream whole and has no bus timing. */
#include "kui/toy_cdda_card.h"
#include "kui/toy_cdda_mail.h"
#include "kui/toy_cdda_player.h"

/* ---- the disc's audio ---- */
enum { CD_CARD_FIRST = 2048u, CD_CARD_BLOCKS = 8192u, CD_TRACK_MAX = 12u, CD_FRAME_TICKS = 13021u };
static uint8_t cd_card[CD_CARD_BLOCKS * 512u];
static uint8_t cd_content(uint32_t lba, unsigned i) {
    if(lba < CD_CARD_FIRST) return image_card[lba * 512u + i];
    return lba - CD_CARD_FIRST < CD_CARD_BLOCKS ? cd_card[(lba - CD_CARD_FIRST) * 512u + i] : 0xee;
}
/* By track number less one. stride 0: one of the fixture's own tracks. */
static struct cd_track {
    uint32_t start, end, stride, first_block;
    bool mapped;
} cd_tracks[CD_TRACK_MAX];
static uint32_t cd_low, cd_high;
/* Byte `at` of a track's audio: no two places of a track alike for long. */
static uint8_t cd_audio(unsigned track, uint32_t at) {
    uint32_t x = track * UINT32_C(0x9e3779b1) + at * UINT32_C(0x85ebca6b);
    x ^= x >> 15; x *= UINT32_C(0x2c1b3c6d); x ^= x >> 12;
    return (uint8_t)x;
}
/* A track behind the last one, `gap` sectors after it, of `sectors` sectors
 * stored `stride` bytes apart. Its file begins `offset` bytes into its
 * first card block and lies in `pieces` pieces on the card, none next to
 * another and not in the file's order; 0: listed without its file. The
 * track's number. */
static unsigned cd_add(uint32_t gap, uint32_t sectors, uint32_t stride, uint32_t offset, unsigned pieces) {
    const unsigned index = manifest.track_count;
    CHECK(index < CD_TRACK_MAX && offset < 512u && (stride == 2352u || stride == 2448u));
    /* Tracks come first in the map, then every track's pieces. */
    memmove(&manifest.slots[index + 1u], &manifest.slots[index], manifest.extent_count * sizeof(manifest.slots[0]));
    for(unsigned i = 0; i < index; ++i) ++manifest.slots[i].track.first_extent;
    struct kui_retail_track *t = &manifest.slots[index].track;
    const uint32_t start = manifest.slots[index - 1u].track.end_lba + gap;
    *t = (struct kui_retail_track){.start_lba = start, .end_lba = start + sectors,
        .control = (uint8_t)((stride == 2448u ? KUI_RETAIL_TRACK_2448 : 0u) |
            (offset & 256u ? KUI_RETAIL_TRACK_OFFSET_HIGH : 0u)),
        .first_extent = (uint16_t)((index + 1u + manifest.extent_count) | (offset & 255u) << 8)};
    ++manifest.track_count;
    cd_tracks[index] = (struct cd_track){start, start + sectors, stride, 0u, pieces != 0u};
    const uint32_t bytes = offset + sectors * stride, blocks = (bytes + 511u) / 512u;
    for(uint32_t n = 0u, piece = 0u; pieces && n < blocks; ++piece) {
        const uint32_t take = piece + 1u == pieces ? blocks - n : blocks / pieces;
        /* Even pieces from the low end of the card, odd ones from its middle. */
        uint32_t *place = piece & 1u ? &cd_high : &cd_low;
        const uint32_t at = *place;
        *place += take + 5u;
        CHECK(take && at + take <= (piece & 1u ? CD_CARD_BLOCKS : CD_CARD_BLOCKS / 2u));
        if(!n) cd_tracks[index].first_block = CD_CARD_FIRST + at;
        manifest.slots[manifest.track_count + manifest.extent_count++].extent =
            (struct kui_retail_extent){n, CD_CARD_FIRST + at, take};
        ++t->extent_count;
        for(uint32_t p = 0u; p < take * 512u; ++p) {
            const uint32_t file = n * 512u + p;
            uint8_t value = 0x77;                       /* another file's bytes, or the block's padding */
            if(file >= offset && file < bytes) {
                const uint32_t in = (file - offset) % stride, sector = (file - offset) / stride;
                /* Subchannel bytes are never audio. */
                value = in < 2352u ? cd_audio(index + 1u, sector * 2352u + in) : (uint8_t)(0xc0u | (in & 0x3fu));
            }
            cd_card[at * 512u + p] = value;
        }
        n += take;
    }
    R.shared.service.track_count = manifest.track_count;
    return index + 1u;
}
static int cd_track_of(uint32_t lba) {
    for(unsigned i = 0; i < manifest.track_count; ++i)
        if(lba >= cd_tracks[i].start && lba < cd_tracks[i].end) return (int)i;
    return -1;
}
static bool cd_sounds(uint32_t lba) {
    const int t = cd_track_of(lba);
    return t >= 0 && cd_tracks[t].stride && cd_tracks[t].mapped;
}
/* ---- the worker's side as the fixture has it ---- */
static bool cd_unbound;                 /* no auxiliary bound */
static uint32_t cd_window = 2344u;      /* the heartbeat's soft window */
static uint32_t cd_cost;                /* TMU0 ticks one visit takes */
static uint32_t cd_sr = 0xf0u;          /* SR at the heartbeat */
static unsigned long cd_ticks;          /* TICK visits made */
static unsigned cd_beat_ticks, cd_beat_most; /* in the heartbeat under way; the most in one */
static unsigned cd_wait_most;           /* most bytes a visit clocked that left a programmed read waiting */
static uint32_t cd_beat_first;          /* the clock at the heartbeat's first visit */
static uint32_t cd_seek_cost;           /* and more ticks for a visit with a programmed read in it */
static unsigned long cd_seek_visits;    /* such visits */
static uint32_t cd_last_have, cd_last_card, cd_last_sink; /* what the heartbeat's last visit answered */
static uint32_t cd_spoil_sink;          /* a count the next visit leaves in the sink, whatever it read */
static uint32_t cd_spoil_offset;        /* and bytes the next visit's sink is moved on by */
static bool cd_spoilt;                  /* the bytes in the sink are not where the segment had got to */
uint32_t kui_toy_cdda_card_window(void) { return cd_unbound ? 0u : cd_window; }
void kui_toy_cdda_card_tick(void) {
    if(!cd_unbound) {
        const struct kui_toy_cdda_mail *mail = &kui_toy_cdda_mail;
        const uint32_t waits = mail->seek_waits, polled = mail->seek_blocks;
        if(!cd_beat_ticks) cd_beat_first = clock_counter;
        else {
            /* The heartbeat's rule for one more visit: the last one answered
             * for this segment that its stream runs, and left bytes; and the
             * time its visits have taken and the longest one seen that only
             * read the stream fit its window. (The worker's clock at its
             * start is this one at the first visit's: a look at it costs the
             * fixture's clock the same each time.) */
            CHECK(mail->want_generation && cd_last_have == mail->want_generation);
            CHECK(cd_last_card == KUI_TOY_CDDA_CARD_STREAMING && cd_last_sink);
            CHECK(cd_beat_first - clock_counter + mail->round_longest <= cd_window);
        }
        /* A visit is asked for whole blocks, as many as the sink holds at
         * most, with the sink empty. */
        if(mail->want_generation)
            CHECK(mail->want_blocks && mail->want_blocks <= KUI_TOY_CDDA_SINK_BYTES / 512u && !mail->sink_bytes);
        ++cd_ticks;
        ++cd_beat_ticks;
        CHECK(kui_native_readahead_aux_visit(KUI_NATIVE_RA_TICK, 0u, &R, 0u) == 1u);
        if(mail->seek_waits != waits && cd_visit_bytes > cd_wait_most) cd_wait_most = cd_visit_bytes;
        clock_counter -= cd_cost;
        if(mail->seek_waits != waits || mail->seek_blocks != polled) {
            clock_counter -= cd_seek_cost;
            ++cd_seek_visits;
        }
        if(cd_spoil_sink) {
            kui_toy_cdda_mail.sink_bytes = cd_spoil_sink;
            cd_spoil_sink = 0u;
        }
        if(cd_spoil_offset && mail->sink_bytes) {
            kui_toy_cdda_mail.sink_offset += cd_spoil_offset;
            cd_spoil_offset = 0u;
            cd_spoilt = true;
        }
        cd_last_have = mail->have_generation; cd_last_card = mail->card; cd_last_sink = mail->sink_bytes;
    }
}
uint32_t kui_toy_cdda_card_test_sr(void) { return hw.sr; }

/* ---- what the consumer takes ---- */
static struct {
    uint32_t lba, byte;      /* the next byte of the play's audio */
    uint32_t starts;         /* the consumer's starts seen */
    unsigned long bytes, laps, begun;
    /* Since the consumer last began: the play's pass its first segment lay
     * in, the laps counted then, and the bytes taken. */
    unsigned long pass, laps_then, bytes_then;
} cap;
static uint32_t cd_play_sector(void) {
    const struct kui_toy_cdda_drive *d = &kui_toy_cdda_drive;
    const uint32_t length = d->end - d->first, ticks = d->base_counter - clock_counter;
    int64_t at = (int64_t)d->base_offset + (int64_t)((uint64_t)ticks * 3u / KUI_TOY_CDDA_TICKS_3_SECTORS);
    if(at < 0) at = 0;
    return d->first + (uint32_t)(at % length);
}
static uint64_t cd_play_position(void) {
    const struct kui_toy_cdda_drive *d = &kui_toy_cdda_drive;
    const uint32_t ticks = d->base_counter - clock_counter;
    int64_t at = (int64_t)d->base_offset + (int64_t)((uint64_t)ticks * 3u / KUI_TOY_CDDA_TICKS_3_SECTORS);
    return (uint64_t)d->done * (d->end - d->first) + (uint64_t)(at < 0 ? 0 : at);
}
/* The first sector at or behind lba, in the play's order, whose audio the
 * card has. */
static uint32_t cd_skip(uint32_t lba, unsigned long *laps) {
    const struct kui_toy_cdda_drive *d = &kui_toy_cdda_drive;
    for(uint32_t n = 0u; n < 2u * (d->end - d->first) + 2u; ++n, ++lba) {
        if(lba >= d->end) {lba = d->first; ++*laps;}
        if(cd_sounds(lba)) return lba;
    }
    return UINT32_MAX;
}
void kui_toy_cdda_card_test_taken(const struct kui_toy_cdda_want *segment, const uint8_t *bytes,
    uint32_t count, uint32_t offset) {
    /* From the worker's heartbeat, never inside a visit; the worker's own
     * bytes; whole card blocks' worth at most. */
    CHECK(cd_watch && !(hw.sr & UINT32_C(0x10000000)) && bytes == kui_toy_cdda_mail.sink);
    CHECK(count && count <= KUI_TOY_CDDA_SINK_BYTES && segment->generation && segment->lba < segment->end);
    /* (A scenario has moved the sink's place on: the consumer is to notice,
     * and nothing further is held against these bytes.) */
    if(cd_spoilt) return;
    if(cap.starts != kui_toy_cdda_listener.starts) {
        /* The first bytes behind a start are a segment's first. */
        CHECK(!offset);
        cap.starts = kui_toy_cdda_listener.starts;
        cap.lba = segment->lba;
        cap.byte = 0u;
        ++cap.begun;
        cap.pass = kui_toy_cdda_listener.done;
        cap.laps_then = cap.laps;
        cap.bytes_then = cap.bytes;
    }
    {
        /* No sector is taken before the play is within the consumer's reach
         * of it, counted in the play's own sectors from its first pass on:
         * those passed in silence count as the others do. */
        const struct kui_toy_cdda_drive *d = &kui_toy_cdda_drive;
        const uint64_t sector = (uint64_t)(cap.pass + cap.laps - cap.laps_then) * (d->end - d->first) + (cap.lba - d->first);
        CHECK(sector <= cd_play_position() + KUI_TOY_CDDA_LISTENER_LEAD / 2352u + 1u);
    }
    for(uint32_t i = 0u; i < count; ++i) {
        const uint32_t at = offset + i, lba = segment->lba + at / 2352u, k = at % 2352u;
        /* The next byte of the play's audio, and the image's. */
        CHECK(lba < segment->end && lba == cap.lba && k == cap.byte);
        const int t = cd_track_of(lba);
        CHECK(t >= 0 && (unsigned)t + 1u == segment->track && cd_sounds(lba));
        CHECK(bytes[i] == cd_audio((unsigned)t + 1u, (lba - cd_tracks[t].start) * 2352u + k));
        if(++cap.byte == 2352u) {
            cap.byte = 0u;
            cap.lba = cd_skip(cap.lba + 1u, &cap.laps);
        }
    }
    cap.bytes += count;
    /* Never taken further ahead of the play than the consumer's reach. */
    CHECK((int32_t)(kui_toy_cdda_listener.ahead - (uint32_t)(cd_play_position() - kui_toy_cdda_listener.sync) * 2352u) <=
        (int32_t)KUI_TOY_CDDA_LISTENER_LEAD);
}

/* ---- a disc, the game's calls and the frames ---- */
static unsigned cd_a, cd_b, cd_c, cd_d;   /* the tracks of cd_open's disc */
/* The drive's status as the game was answered: 3, playing, and any other. */
static unsigned long cd_status_3, cd_status_other;
/* The fixture's disc with four audio tracks behind its own: A, 120 sectors
 * of 2,352 bytes in one piece; two sectors on, B, 90 sectors of 2,448 bytes
 * in five pieces, its file beginning inside a block; C, 60 sectors of 2,352
 * bytes in three pieces at an odd byte; D, 40 sectors listed without their
 * file: more than the consumer's reach. And the game has read once: the low
 * reader has the bus. */
static void cd_open(void) {
    open_card(2000u, false, true, file_amid, false);
    card_content = cd_content;
    card.blocks = CD_CARD_FIRST + CD_CARD_BLOCKS;
    memset(cd_card, 0x5c, sizeof(cd_card));
    memset(cd_tracks, 0, sizeof(cd_tracks));
    for(unsigned i = 0; i < manifest.track_count; ++i)
        cd_tracks[i] = (struct cd_track){manifest.slots[i].track.start_lba, manifest.slots[i].track.end_lba, 0u, 0u, false};
    cd_low = 0u; cd_high = CD_CARD_BLOCKS / 2u;
    cd_a = cd_add(98u, 120u, 2352u, 0u, 1u);
    cd_b = cd_add(2u, 90u, 2448u, 100u, 5u);
    cd_c = cd_add(0u, 60u, 2352u, 37u, 3u);
    cd_d = cd_add(0u, 40u, 2352u, 0u, 0u);
    manifest.card_sectors = CD_CARD_FIRST + CD_CARD_BLOCKS;
    manifest.partition_end = CD_CARD_FIRST + CD_CARD_BLOCKS - 8u;
    {
        /* The map is one the launcher would hand this reader. */
        static struct kui_retail_manifest checked;
        checked = manifest;
        checked.storage_transport = KUI_STORAGE_SCI;
        CHECK(kui_retail_manifest_validate(&checked) == KUI_GAME_OK);
    }
    memset(&kui_toy_cdda_drive, 0, sizeof(kui_toy_cdda_drive));
    memset(&kui_toy_cdda_mail, 0, sizeof(kui_toy_cdda_mail));
    memset(&kui_toy_cdda_listener, 0, sizeof(kui_toy_cdda_listener));
    memset(&cap, 0, sizeof(cap));
    cd_watch = true;
    cd_unbound = false; cd_window = 2344u; cd_cost = 120u; cd_sr = 0xf0u;
    cd_ticks = 0u; cd_beat_most = 0u; cd_wait_most = 0u;
    cd_seek_cost = 0u; cd_seek_visits = 0u; cd_spoil_sink = 0u; cd_spoil_offset = 0u; cd_spoilt = false;
    cd_interrupt_visits = cd_timer_visits = 0u;
    cd_status_3 = cd_status_other = 0u;
    /* A run of the stream lands when its time has passed, between the
     * heartbeats, and its end asks for the SCI's interrupt. */
    m.delay = 100000u;
    /* The low reader's visits come through the gate here, as on the
     * console: masked, never re-entered, and looked at (cd_watch). */
    R.vector600[KUI_NATIVE_RA_ENTRY_INDEX] = (uint32_t)(uintptr_t)kui_native_readahead_aux_visit;
    plain_read(45020u, 10u);
    CHECK(R.engine.opened && !stream_on() && !kui_native_readahead_aux_context.movie_ahead.variant);
}
static unsigned long cd_interrupt_total, cd_timer_total;
static void cd_close(void) {
    cd_watch = false;
    cd_interrupt_total += cd_interrupt_visits;
    cd_timer_total += cd_timer_visits;
}
static void cd_command(uint32_t command, uint32_t a, uint32_t b, uint32_t c) {
    put(PARAM, a); put(PARAM + 4u, b); put(PARAM + 8u, c); put(PARAM + 12u, 0u);
    const int32_t token = gd(KUI_GD_REQUEST, command, PARAM);
    CHECK(token > 0 && gd(KUI_GD_EXEC, 0, 0) == 0);
    CHECK(gd(KUI_GD_CHECK, (uint32_t)token, STATUS) == KUI_GD_COMPLETED);
    invariants();
}
static void cd_play(unsigned from, unsigned to, uint32_t repeat) {
    cd_command(KUI_RETAIL_GD_PLAY, from, to, repeat);
    CHECK(kui_toy_cdda_drive.state == KUI_TOY_CDDA_PLAYING);
}
static uint32_t cd_status(void) {
    CHECK(gd(KUI_GD_DRIVE, STATUS + 32u, 0u) == 0);
    invariants();
    return get(STATUS + 32u);
}
/* The worker's heartbeat; with cd_no_beat, the same moment without it. */
static bool cd_no_beat;
static void cd_beat(void) {
    const struct kui_toy_cdda_listener *l = &kui_toy_cdda_listener;
    const bool playing = kui_toy_cdda_drive.state == KUI_TOY_CDDA_PLAYING;
    const uint32_t starts = l->starts, polled = R.shared.stream.stats.polled;
    const uint32_t stood = playing ? cd_play_sector() : 0u, sr = hw.sr;
    /* How far ahead of the play the consumer is, before it asks. */
    const int32_t lead = playing && l->following ?
        (int32_t)(l->ahead - (uint32_t)(cd_play_position() - l->sync) * 2352u) : 0;
    hw.sr = cd_sr;
    cd_beat_ticks = 0u;
    if(!cd_no_beat) kui_toy_cdda_consumer_heartbeat();
    hw.sr = sr;
    if(cd_beat_ticks > cd_beat_most) cd_beat_most = cd_beat_ticks;
    /* It says that it runs low exactly while it holds less than a third of
     * its reach, and at a start, when it holds nothing. */
    if(cd_beat_ticks && kui_toy_cdda_mail.want_generation)
        CHECK(kui_toy_cdda_mail.yield_ok == (l->starts != starts || lead < (int32_t)KUI_TOY_CDDA_LISTENER_LOW));
    /* One block by programmed transfers a heartbeat at most. */
    CHECK(R.shared.stream.stats.polled - polled <= 1u);
    if(l->starts != starts) {
        /* Begun at the sector the play stood at. */
        unsigned long laps = 0u;
        CHECK(playing && l->starts == starts + 1u);
        if(l->want.generation) CHECK(l->want.lba == cd_skip(stood, &laps));
    }
    CHECK(!l->wrong);
    invariants();
}
/* Frames of a sixtieth of a second: whatever interrupts are due, a call of
 * the game's for the drive's status every `poll_every` (0: none), and the
 * worker's heartbeat. */
static void cd_frames(unsigned frames, unsigned poll_every) {
    for(unsigned n = 0u; n < frames; ++n) {
        const uint32_t began = clock_counter;
        /* A frame's worth of the wire at most: 26 KB at its rate, every
         * run's end and every wait's end an interrupt. */
        for(unsigned k = 0u; k < 64u && hardware(); ++k) {}
        if(poll_every && n % poll_every == 0u) {
            if(cd_status() == 3u) ++cd_status_3;
            else ++cd_status_other;
        }
        cd_beat();
        const uint32_t spent = began - clock_counter;
        if(spent < CD_FRAME_TICKS) clock_counter -= CD_FRAME_TICKS - spent;
    }
}
/* A read of the game's while the worker's heartbeat goes on: by a caller the
 * reader does not know, as plain_read, with a heartbeat every few rounds.
 * The status asked in between is the playing drive's. */
static void cd_read(uint32_t lba, uint32_t count, bool playing) {
    memset(ram + (OUTPUT - BEGIN), 0xa5, 64u * 2352u + 16u);
    const int32_t token = request(lba, count, OUTPUT);
    invariants();
    int32_t status = KUI_GD_PROCESSING;
    for(unsigned n = 0u; status == KUI_GD_PROCESSING; ++n) {
        CHECK(n < 200000u);
        (void)hardware();
        if(n % 3u == 0u) CHECK(gd(KUI_GD_EXEC, 0, 0) == 0);
        if(n % 4u == 1u) cd_beat();
        if(playing && n % 5u == 2u) CHECK(cd_status() == 3u);
        status = gd(KUI_GD_CHECK, (uint32_t)token, STATUS);
        invariants();
    }
    CHECK(status == KUI_GD_COMPLETED);
    CHECK(get(STATUS + 8u) == count * R.shared.service.sector_bytes && !get(STATUS + 4u));
    compare(lba, count, OUTPUT);
}
/* The window's own counts are the window's: the audio's stream leaves the
 * streams begun, the windows given up and the streams it ended alone. */
static void cd_window_untouched(uint32_t starts) {
    const struct kui_toy_wire_report *w = report();
    CHECK(w->n.starts == starts);
    no_drops(w);
}

/* The audio's counters, for a look at a scenario (WIRE_VERBOSE). */
static void cd_dump(const char *what) {
    const struct kui_toy_cdda_mail *mail = &kui_toy_cdda_mail;
    const struct kui_toy_cdda_listener *l = &kui_toy_cdda_listener;
    fprintf(stderr, "%s: blocks %u rebuilt %u rereads %u seek_blocks %u seek_waits %u yields %u declined %u data %u other %u "
        "ext %u refused %u why %u err %u/%x | visits %u ticks %u hb %u longest %u round %u | wire starts %u cuts %u waits %u "
        "fulls %u blocks %u rebuilt %u holed %u unchecked %u badcrc %u lostframe %u ambig %u order %u | L starts %u seg %u "
        "bytes %u silent %u skipped %u | cap bytes %lu laps %lu begun %lu | ticks %lu most %u waitmost %u irq %lu timer %lu\n",
        what, mail->blocks, mail->rebuilt, mail->rereads, mail->seek_blocks, mail->seek_waits, mail->yields, mail->declined, mail->data_halts,
        mail->other_halts, mail->external_halts, mail->refused, mail->why, mail->first_error, mail->first_detail, mail->visits, mail->visit_ticks,
        mail->heartbeats, mail->longest, mail->round_longest, mail->wire.starts, mail->wire.cuts, mail->wire.waits, mail->wire.fulls,
        mail->wire.blocks, mail->wire.rebuilt, mail->wire.holed, mail->wire.unchecked, mail->wire.bad_crc, mail->wire.lost_frame,
        mail->wire.ambiguous, mail->wire.order, l->starts, l->segments, l->bytes, l->silent, l->skipped, cap.bytes, cap.laps,
        cap.begun, cd_ticks, cd_beat_most, cd_wait_most, cd_interrupt_visits, cd_timer_visits);
}
/* What the audio's counters always say of each other. Every stream begun
 * was ended once, by the audio itself, for a read of the game's or by the
 * reader, unless it still runs (a programmed read that waited when it was
 * given up is counted with them); every block the stream rebuilt was handed
 * on; every block it handed the reader that was not checked, and every
 * stream that could not be followed, was read again. */
static void cd_counts(void) {
    const struct kui_toy_cdda_mail *mail = &kui_toy_cdda_mail;
    CHECK(mail->other_halts + mail->data_halts + mail->external_halts + (stream_on() ? 1u : 0u) >= mail->wire.starts);
    CHECK(mail->rebuilt == mail->wire.rebuilt && mail->blocks <= mail->wire.blocks);
    CHECK(mail->wire.blocks - mail->blocks <= mail->rereads);
    CHECK(mail->rereads == mail->wire.blocks - mail->blocks + mail->wire.bad_crc + mail->wire.lost_frame + mail->wire.ambiguous);
    CHECK(mail->wire.holed == mail->wire.blocks - mail->blocks && mail->heartbeats <= mail->visits);
    /* A programmed read is timed between two looks at the clock, each of
     * which costs the fixture's clock three ticks. */
    CHECK(mail->seek_ticks >= 3u * (mail->seek_blocks + mail->seek_waits) && mail->visits == cd_ticks);
}
/* ---- scenarios ---- */

/* A track played for ever with nothing else on the card: the consumer has
 * its audio in order, pass after pass. */
static void cd_steady(unsigned which, uint32_t mean, unsigned dense) {
    cd_open();
    const unsigned track = which == 0u ? cd_a : which == 1u ? cd_b : cd_c;
    const struct cd_track *t = &cd_tracks[track - 1u];
    const struct kui_toy_cdda_mail *mail = &kui_toy_cdda_mail;
    const uint32_t window_starts = report()->n.starts;
    stall_mean = mean; stall_dense = dense;
    cd_play(track, track, 15u);
    cd_frames(3u * (t->end - t->start), 3u);
    if(getenv("WIRE_VERBOSE")) cd_dump("a track played for ever");
    /* Every byte the image's and in its place. On a quiet bus the card
     * keeps up with the play, two passes and more. On a bus far busier than
     * a console's, where most blocks of the stream lose two bytes and more
     * and are read again one a heartbeat, it falls behind and hands on what
     * it has: less, never anything else. */
    CHECK(cap.begun == 1u && cap.bytes >= 16u * 2352u);
    if(!mean) CHECK(cap.laps >= 2u && cap.bytes >= 2u * (t->end - t->start) * 2352u && mail->blocks);
    CHECK(kui_toy_cdda_listener.starts == 1u && kui_toy_cdda_listener.bytes == cap.bytes);
    /* A segment a pass, a stream a piece of each at least; every block of
     * the stream checked, every other one read by programmed transfers. */
    const unsigned pieces = manifest.slots[track - 1u].track.extent_count;
    CHECK(kui_toy_cdda_listener.segments == cap.laps + 1u && mail->wire.starts >= pieces * cap.laps);
    CHECK(mail->seek_blocks >= pieces * cap.laps && !mail->refused && !mail->declined && !mail->external_halts);
    CHECK(mail->blocks + mail->seek_blocks >= cap.bytes / 512u && !mail->data_halts);
    cd_counts();
    /* Every run of the stream ends in a lost byte: blocks with one rebuilt
     * from their CRC16 are handed on all the time. */
    if(!mean) CHECK(mail->rebuilt);
    else CHECK(mail->wire.cuts && mail->rereads);
    /* The play's status was the playing drive's throughout. */
    CHECK(cd_status_3 && !cd_status_other);
    cd_window_untouched(window_starts);
    /* No delivery of the low reader's took the stream's reception for its
     * own, and the stream never lost its channel or the SCI. */
    CHECK(!R.shared.stream.stats.foreign && !mail->first_error && !R.shared.stream.stats.overruns);
    /* Interrupts answered the stream, and none of their visits sent the
     * card a byte. */
    CHECK(cd_interrupt_visits && eris);
    if(mean) CHECK(cd_timer_visits && timers);
    /* STOP: the consumer asks for nothing and the audio leaves the card. */
    stall_mean = 0u;
    plain_command(KUI_GD_STOP, 0u);
    CHECK(kui_toy_cdda_drive.state == KUI_TOY_CDDA_IDLE);
    const unsigned long bytes = cap.bytes;
    cd_frames(3u, 0u);
    CHECK(!stream_on() && !mail->have_generation && mail->card == KUI_TOY_CDDA_CARD_IDLE && cap.bytes == bytes);
    const unsigned long ticks = cd_ticks;
    cd_frames(5u, 1u);
    CHECK(cd_ticks == ticks && cd_status_other);
    /* And the game reads on as it always did. */
    plain_read(45100u, 20u);
    cd_close();
}

/* One block of the stream damaged on its way: a lost data byte is rebuilt
 * from the block's CRC16 and the block handed on; a lost CRC byte, two lost
 * bytes, or a CRC16 that does not fit end the stream, and the block is read
 * again by programmed transfers, which the low reader checks. The consumer
 * has the image's bytes every time. */
enum { CD_LOST_DATA, CD_LOST_CRC, CD_LOST_TWO, CD_BAD_CRC, CD_LOST_KINDS };
static void cd_lost(unsigned kind) {
    cd_open();
    const struct kui_toy_cdda_mail *mail = &kui_toy_cdda_mail;
    /* The track's forty-first block: well inside the first stream. */
    const uint32_t block = cd_tracks[cd_a - 1u].first_block + 40u;
    if(kind == CD_BAD_CRC) {card.crc_lba = block; card.crc_xor = 0x1234u; card.crc_count = 1u;}
    else {
        cut_lba = block;
        cut_index = kind == CD_LOST_CRC ? 512u : 100u;
        cut_again = kind == CD_LOST_TWO ? 300u : 0u;
    }
    cd_play(cd_a, cd_a, 0u);
    cd_frames(8u, 2u);
    /* The damage has been met, and nothing else has ended a stream. */
    CHECK(!cut_lba && !card.crc_count && cap.bytes > 60u * 512u);
    cd_counts();
    if(kind == CD_LOST_DATA) {
        /* Handed on from the stream: nothing was read again. */
        CHECK(mail->rebuilt >= 1u && !mail->rereads && mail->seek_blocks == 1u && mail->wire.starts == 1u);
    } else {
        /* Not handed on: the stream ended there and the block was read by
         * programmed transfers, then a stream took over again. */
        CHECK(mail->rereads == 1u && mail->seek_blocks == 2u && mail->wire.starts == 2u && mail->other_halts == 1u);
        CHECK(kind == CD_LOST_CRC ? mail->wire.unchecked == 1u && mail->wire.holed == 1u :
            kind == CD_LOST_TWO ? mail->wire.holed == 1u && !mail->wire.unchecked : mail->wire.bad_crc == 1u);
    }
    cd_frames(100u, 3u);
    /* Played once, to its end: every byte, once. */
    CHECK(kui_toy_cdda_drive.state == KUI_TOY_CDDA_ENDED && cap.bytes == 120u * 2352u && cap.laps == 1u);
    CHECK(!stream_on() && !mail->have_generation && !mail->refused);
    cd_close();
}
/* The block to read again is the last of its piece on the card: the file
 * goes on elsewhere, and no stream takes over behind the programmed read. */
static void cd_lost_at_piece_end(void) {
    cd_open();
    const struct kui_toy_cdda_mail *mail = &kui_toy_cdda_mail;
    const struct kui_retail_track *c = &manifest.slots[cd_c - 1u].track;
    const struct kui_retail_extent *piece = &manifest.slots[kui_retail_track_first_extent(c)].extent;
    CHECK(c->extent_count == 3u && piece->blocks > 4u);
    cut_lba = piece->card_lba + piece->blocks - 1u;
    cut_index = 512u;
    cd_play(cd_c, cd_c, 0u);
    cd_frames(70u, 3u);
    CHECK(!cut_lba && mail->rereads == 1u && mail->wire.unchecked == 1u && !mail->refused);
    CHECK(kui_toy_cdda_drive.state == KUI_TOY_CDDA_ENDED && cap.bytes == 60u * 2352u && !stream_on());
    /* A programmed read at the segment's start, one for the block read
     * again, and one where each further piece begins. */
    CHECK(mail->seek_blocks == 4u && mail->wire.starts == 3u);
    cd_counts();
    cd_close();
}
/* A block the card never sends whole: after eight tries the segment is given
 * up, with a reason, and what is left of it passes in silence. Nothing but
 * the image's bytes was handed on, up to the block before. */
static void cd_dead_block(void) {
    cd_open();
    const struct kui_toy_cdda_mail *mail = &kui_toy_cdda_mail;
    card.crc_lba = cd_tracks[cd_a - 1u].first_block + 40u;
    card.crc_xor = 0x4321u;
    card.crc_count = 1000u;
    cd_play(cd_a, cd_a, 0u);
    cd_frames(30u, 3u);
    /* Eight programmed reads, each refused by the low reader's CRC16; the
     * consumer was told, and asks for nothing more of this play's one pass. */
    CHECK(mail->refused == 1u && mail->why == KUI_TOY_CDDA_WHY_CARD && !mail->have_generation);
    CHECK(mail->rereads == 1u && cap.bytes == 40u * 512u && !stream_on() && mail->seek_blocks == 1u);
    CHECK(R.shared.stream.stats.crc_errors == 8u && !R.shared.port.fault);
    CHECK(kui_toy_cdda_listener.want.generation == 0u && kui_toy_cdda_listener.segments == 1u);
    const unsigned long ticks = cd_ticks;
    const unsigned clocks = card.clocks;
    cd_frames(100u, 3u);
    /* No further card work for a segment given up; the play goes on to its
     * end in silence. */
    CHECK(kui_toy_cdda_drive.state == KUI_TOY_CDDA_ENDED && cap.bytes == 40u * 512u && card.clocks == clocks);
    CHECK(cd_ticks == ticks && !mail->have_generation);
    card.crc_count = 0u;
    plain_read(45100u, 20u);
    cd_close();
}
/* The game reads while the music plays. A read ends the audio's stream in
 * the visit that finds it and is served as it always was, by the low reader
 * or through a window; the audio comes back behind it at the byte it had
 * got to. A window that stays open behind a read keeps the card: the audio
 * waits for it. */
static void cd_reads(uint32_t mean, unsigned dense) {
    cd_open();
    const struct kui_toy_cdda_mail *mail = &kui_toy_cdda_mail;
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    stall_mean = mean; stall_dense = dense;
    cd_play(cd_a, cd_c, 15u);
    cd_frames(20u, 3u);
    CHECK(stream_on() && mail->wire.starts && !mail->data_halts);
    /* A read elsewhere on the disc: the low reader's. */
    const uint32_t window_starts = report()->n.starts;
    cd_read(45210u, 20u, true);
    CHECK(mail->data_halts == 1u && !stream_on() && !c->movie_ahead.variant && report()->n.starts == window_starts);
    unsigned long bytes = cap.bytes;
    cd_frames(30u, 3u);
    CHECK(cap.bytes > bytes && cap.begun == 1u);
    /* Reads one behind the other: the second opens a window, and its
     * stream is the window's. */
    cd_read(45040u, 5u, true);
    cd_read(45045u, 5u, true);
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && report()->n.starts > window_starts);
    const uint32_t halts = mail->data_halts, streams = mail->wire.starts, polled = mail->seek_blocks;
    CHECK(halts >= 2u);
    bytes = cap.bytes;
#if KUI_TOY_CDDA_YIELD
    /* Test 85 (YIELD): the window stands while the consumer holds enough,
     * the audio waiting with no card work of its own. When the consumer says
     * that it runs low the window is given up for the audio, by its own way
     * out: its stream ended, and no window's drop counted. */
    const uint32_t closes = report()->stops[KUI_TOY_WIRE_STOP_CLOSE], windows = report()->other_windows;
    unsigned waited = 0u;
    while(c->movie_ahead.variant == OTHER_WINDOW) {
        CHECK(++waited < 40u && cap.bytes == bytes && !mail->yields && mail->seek_blocks == polled);
        cd_frames(1u, 3u);
        if(c->movie_ahead.variant == OTHER_WINDOW) CHECK(!cd_beat_ticks || !mail->yield_ok);
    }
    CHECK(waited > 5u && mail->yields == 1u && mail->yield_ok && mail->data_halts == halts);
    CHECK(report()->stops[KUI_TOY_WIRE_STOP_CLOSE] == closes + 1u && report()->other_windows == windows);
    no_drops(report());
    cd_frames(10u, 3u);
    CHECK(cap.bytes > bytes && stream_on() && mail->wire.starts == streams + 1u && mail->yields == 1u);
    /* The read that follows on ends the audio's stream and has a window
     * again; one elsewhere ends that. */
    cd_read(45050u, 20u, true);
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && c->stats.hit_requests == 2u && mail->data_halts == halts + 1u);
    CHECK(report()->other_windows == windows + 1u);
#else
    cd_frames(40u, 3u);
    /* The window stands: the audio has waited, with no card work of its own. */
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && cap.bytes == bytes && mail->card == KUI_TOY_CDDA_CARD_IDLE);
    CHECK(mail->wire.starts == streams && mail->seek_blocks == polled && mail->data_halts == halts && !mail->yields);
    /* A read from the window, then one elsewhere that ends it. */
    cd_read(45050u, 20u, true);
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && c->stats.hit_requests == 2u && mail->data_halts == halts);
#endif
    cd_read(45020u, 10u, true);
    CHECK(!c->movie_ahead.variant);
    cd_frames(60u, 3u);
    CHECK(cap.bytes > bytes && cap.begun == 1u && mail->wire.starts > streams);
    /* Every status asked while the music played was the playing drive's,
     * the reads' own polls among them (KEEP). */
    CHECK(cd_status_3 && !cd_status_other && kui_toy_cdda_drive.reads_in_play == 5u && kui_toy_cdda_drive.keeps);
    cd_counts();
    CHECK(!R.shared.stream.stats.foreign && !mail->refused && !mail->declined && !mail->external_halts);
    stall_mean = 0u;
    plain_command(KUI_GD_STOP, 0u);
    cd_frames(2u, 0u);
    CHECK(!stream_on() && !mail->have_generation);
    cd_close();
}
/* Where a play ends: once through, three times through, across tracks with
 * a gap between them and one of 2,448-byte sectors, between two sectors
 * inside a track, inside a gap, and over a track whose file is not on the
 * card. */
static void cd_ends(void) {
    const struct kui_toy_cdda_mail *mail = &kui_toy_cdda_mail;
    const struct kui_toy_cdda_listener *l = &kui_toy_cdda_listener;
    cd_open();
    /* (The consumer's last segment number was the last there is: the next
     * is not the one that asks for nothing.) */
    kui_toy_cdda_listener.generation = UINT32_MAX;
    cd_play(cd_a, cd_a, 0u);
    cd_frames(110u, 3u);
    CHECK(kui_toy_cdda_drive.state == KUI_TOY_CDDA_ENDED && cap.bytes == 120u * 2352u && cap.laps == 1u);
    CHECK(l->segments == 1u && l->generation == 1u && !stream_on() && !mail->have_generation && cd_status_other);
    /* Three passes. */
    cd_play(cd_a, cd_a, 2u);
    cd_frames(320u, 3u);
    CHECK(kui_toy_cdda_drive.state == KUI_TOY_CDDA_ENDED && cap.bytes == 4u * 120u * 2352u && cap.laps == 4u);
    CHECK(l->segments == 4u && l->starts == 2u && cap.begun == 2u && !stream_on() && !mail->have_generation);
    cd_close();

    cd_open();
    /* Tracks A to D: two sectors belong to no track, and D has no file. */
    cd_play(cd_a, cd_d, 0u);
    cd_frames(270u, 3u);
    CHECK(kui_toy_cdda_drive.state == KUI_TOY_CDDA_ENDED && cap.bytes == (120u + 90u + 60u) * 2352u);
    CHECK(l->silent == 2u && l->segments == 4u && mail->refused == 1u && mail->why == KUI_TOY_CDDA_WHY_UNMAPPED);
    CHECK(!stream_on() && !mail->have_generation);
    cd_counts();
    /* The same for ever: each pass's A is taken when the play comes within
     * the consumer's reach of it, not when D's 40 sectors were refused (the
     * capture holds every sector against the play's clock). */
    cd_play(cd_a, cd_d, 15u);
    cd_frames(700u, 3u);
    CHECK(kui_toy_cdda_drive.state == KUI_TOY_CDDA_PLAYING && cap.laps >= 3u && mail->refused >= 3u && l->silent >= 6u);
    CHECK(cap.bytes - cap.bytes_then >= 2u * (120u + 90u + 60u) * 2352u && cap.begun == 2u);
    cd_counts();
    plain_command(KUI_GD_STOP, 0u);
    cd_frames(2u, 0u);
    cd_close();

    cd_open();
    /* PLAY2 from a sector inside A to one inside A. */
    const uint32_t start = cd_tracks[cd_a - 1u].start, end = cd_tracks[cd_a - 1u].end;
    cd_command(KUI_RETAIL_GD_PLAY2, start + 30u + 150u, start + 50u + 150u, 0u);
    CHECK(kui_toy_cdda_drive.state == KUI_TOY_CDDA_PLAYING && kui_toy_cdda_drive.first == start + 30u);
    cd_frames(30u, 3u);
    CHECK(kui_toy_cdda_drive.state == KUI_TOY_CDDA_ENDED && cap.bytes == 20u * 2352u && l->segments == 1u);
    CHECK(l->want.generation == 0u && !stream_on() && !mail->have_generation && !l->silent);
    /* And to the first of the two sectors behind A that no track holds: that
     * one passes in silence, and nothing of B is asked for. */
    cd_command(KUI_RETAIL_GD_PLAY2, end - 20u + 150u, end + 1u + 150u, 0u);
    CHECK(kui_toy_cdda_drive.state == KUI_TOY_CDDA_PLAYING && kui_toy_cdda_drive.end == end + 1u);
    cd_frames(30u, 3u);
    CHECK(kui_toy_cdda_drive.state == KUI_TOY_CDDA_ENDED && cap.bytes == 40u * 2352u && l->segments == 2u);
    CHECK(l->silent == 1u && !mail->refused && !stream_on() && !mail->have_generation);
    cd_close();
}
/* PAUSE and RELEASE, and another PLAY during a play: the audio leaves the
 * card when the drive stops playing and begins again at the sector the play
 * stands at when it plays. */
static void cd_stops(void) {
    cd_open();
    const struct kui_toy_cdda_mail *mail = &kui_toy_cdda_mail;
    const struct kui_toy_cdda_listener *l = &kui_toy_cdda_listener;
    cd_play(cd_b, cd_b, 15u);
    cd_frames(20u, 3u);
    CHECK(stream_on() && cap.begun == 1u);
    cd_command(KUI_RETAIL_GD_PAUSE, 0u, 0u, 0u);
    CHECK(kui_toy_cdda_drive.state == KUI_TOY_CDDA_PAUSED);
    unsigned long bytes = cap.bytes;
    cd_frames(10u, 3u);
    CHECK(!stream_on() && !mail->have_generation && cap.bytes == bytes && !l->following);
    const unsigned long ticks = cd_ticks;
    cd_frames(10u, 3u);
    CHECK(cd_ticks == ticks);
    /* Released: from where it stood. */
    const uint32_t stood = kui_toy_cdda_drive.first + kui_toy_cdda_drive.frozen;
    cd_command(KUI_RETAIL_GD_RELEASE, 0u, 0u, 0u);
    CHECK(kui_toy_cdda_drive.state == KUI_TOY_CDDA_PLAYING);
    cd_frames(20u, 3u);
    CHECK(cap.begun == 2u && l->starts == 2u && cap.bytes > bytes && stream_on());
    CHECK(l->sync == (uint32_t)kui_toy_cdda_drive.done * (kui_toy_cdda_drive.end - kui_toy_cdda_drive.first) +
        (stood - kui_toy_cdda_drive.first));
    /* Paused and released between two heartbeats: begun again all the same. */
    cd_command(KUI_RETAIL_GD_PAUSE, 0u, 0u, 0u);
    cd_command(KUI_RETAIL_GD_RELEASE, 0u, 0u, 0u);
    cd_frames(20u, 3u);
    CHECK(cap.begun == 3u && l->starts == 3u);
    /* Another PLAY during the play. */
    bytes = cap.bytes;
    cd_play(cd_c, cd_c, 15u);
    cd_frames(20u, 3u);
    CHECK(cap.begun == 4u && l->starts == 4u && cap.bytes > bytes && l->want.track == cd_c);
    cd_counts();
    CHECK(!mail->refused && !mail->data_halts && !R.shared.stream.stats.foreign);
    plain_command(KUI_GD_STOP, 0u);
    cd_frames(2u, 0u);
    CHECK(!stream_on() && !mail->have_generation);
    cd_close();
}

/* The worker's heartbeat asked directly: what one call answers. */
static unsigned long cd_direct_bytes;
static uint32_t cd_direct_next;
static void cd_direct_take(void *context, const uint8_t *bytes, uint32_t count, uint32_t offset) {
    const struct kui_toy_cdda_want *want = context;
    CHECK(bytes == kui_toy_cdda_mail.sink && count && count <= KUI_TOY_CDDA_SINK_BYTES && offset == cd_direct_next);
    const int t = cd_track_of(want->lba);
    CHECK(t >= 0);
    for(uint32_t i = 0u; i < count; ++i)
        CHECK(bytes[i] == cd_audio((unsigned)t + 1u, (want->lba - cd_tracks[t].start) * 2352u + offset + i));
    cd_direct_next += count;
    cd_direct_bytes += count;
}
static void cd_no_take(void *context, const uint8_t *bytes, uint32_t count, uint32_t offset) {
    (void)context; (void)bytes; (void)count; (void)offset;
    CHECK(!"nothing is handed on");
}
static uint32_t cd_ask_only(const struct kui_toy_cdda_want *want, uint32_t room, kui_toy_cdda_take take) {
    const uint32_t sr = hw.sr;
    hw.sr = 0xf0u;
    cd_beat_ticks = 0u;
    const uint32_t state = kui_toy_cdda_card_heartbeat(want, room, take, (void *)(uintptr_t)want);
    hw.sr = sr;
    return state;
}
static uint32_t cd_ask(const struct kui_toy_cdda_want *want, uint32_t room, kui_toy_cdda_take take) {
    const uint32_t state = cd_ask_only(want, room, take);
    invariants();
    return state;
}
/* Segments the auxiliary refuses, each with its reason and without a byte
 * to the card: no such track, a data track, a track without its file, pieces
 * outside the map, sectors outside the track, a map that does not hold the
 * first block. And one whose later piece the map does not hold: refused
 * there, with what came before handed on. */
static void cd_refused(void) {
    cd_open();
    const struct kui_toy_cdda_mail *mail = &kui_toy_cdda_mail;
    struct kui_retail_track *a = &manifest.slots[cd_a - 1u].track;
    const struct kui_retail_track kept = *a;
    const uint32_t first = a->start_lba, extents = manifest.extent_count;
    uint32_t generation = 0u;
    const struct { uint32_t track, lba, end, why; unsigned damage; } cases[] = {
        {0u, first, first + 4u, KUI_TOY_CDDA_WHY_TRACK, 0u},
        {manifest.track_count + 1u, first, first + 4u, KUI_TOY_CDDA_WHY_TRACK, 0u},
        {3u, 45010u, 45014u, KUI_TOY_CDDA_WHY_KIND, 0u},
        {cd_a, first, first + 4u, KUI_TOY_CDDA_WHY_KIND, 1u},
        {cd_a, first, first + 4u, KUI_TOY_CDDA_WHY_KIND, 2u},
        {cd_a, first, first + 4u, KUI_TOY_CDDA_WHY_KIND, 3u},
        {cd_a, first, first + 4u, KUI_TOY_CDDA_WHY_KIND, 9u},
        {cd_d, cd_tracks[cd_d - 1u].start, cd_tracks[cd_d - 1u].end, KUI_TOY_CDDA_WHY_UNMAPPED, 0u},
        {cd_a, first, first + 4u, KUI_TOY_CDDA_WHY_PIECES, 4u},
        {cd_a, first, first + 4u, KUI_TOY_CDDA_WHY_PIECES, 5u},
        {cd_a, first, first + 4u, KUI_TOY_CDDA_WHY_PIECES, 6u},
        {cd_a, first, first + 4u, KUI_TOY_CDDA_WHY_TRACK, 7u},
        {cd_a, first - 1u, first + 4u, KUI_TOY_CDDA_WHY_RANGE, 0u},
        {cd_a, first + 100u, a->end_lba + 1u, KUI_TOY_CDDA_WHY_RANGE, 0u},
        {cd_a, first + 4u, first + 4u, KUI_TOY_CDDA_WHY_RANGE, 0u},
        {cd_a, first + 5u, first + 4u, KUI_TOY_CDDA_WHY_RANGE, 0u},
        {cd_a, first, first + 4u, KUI_TOY_CDDA_WHY_BEGIN, 8u},
    };
    for(unsigned i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        struct kui_retail_extent *piece = &manifest.slots[kui_retail_track_first_extent(&kept)].extent;
        const struct kui_retail_extent piece_kept = *piece;
        switch(cases[i].damage) {
        case 1u: a->control |= 4u; break;                              /* a data track */
        case 2u: a->control |= KUI_RETAIL_TRACK_MODE2; break;
        case 3u: a->control |= KUI_RETAIL_TRACK_2336; break;
        case 4u: a->first_extent = (uint16_t)(manifest.track_count - 1u); break; /* a track's slot */
        case 5u: a->first_extent = (uint16_t)(manifest.track_count + extents + 3u); break; /* behind the map's pieces */
        case 6u: a->extent_count = (uint8_t)(extents + 1u); break;      /* more pieces than the map has left */
        case 7u: manifest.extent_count = KUI_RETAIL_MANIFEST_SLOTS; break;
        case 8u: piece->file_block = 3u; break;                        /* the file's first blocks in no piece */
        case 9u: a->control |= KUI_RETAIL_TRACK_COOKED; break;
        default: break;
        }
        const struct kui_toy_cdda_want want = {++generation, cases[i].track, cases[i].lba, cases[i].end, false};
        const unsigned bytes = bus_bytes, clocks = card.clocks;
        const uint32_t refused = mail->refused;
        CHECK(cd_ask(&want, 4096u, cd_no_take) == KUI_TOY_CDDA_CARD_REFUSED);
        CHECK(mail->why == cases[i].why && mail->refused == refused + 1u && mail->have_generation == generation);
        /* Asked again, it stays refused and is not looked at again. */
        CHECK(cd_ask(&want, 4096u, cd_no_take) == KUI_TOY_CDDA_CARD_REFUSED && mail->refused == refused + 1u);
        CHECK(bus_bytes == bytes && card.clocks == clocks && !stream_on() && cd_beat_ticks == 1u);
        *a = kept;
        *piece = piece_kept;
        manifest.extent_count = extents;
    }
    /* The same track as it is: read. What the consumer says of itself is
     * passed on with each ask. */
    struct kui_toy_cdda_want want = {++generation, cd_a, first, first + 4u, true};
    cd_direct_bytes = 0u; cd_direct_next = 0u;
    for(unsigned n = 0u; n < 20u && cd_ask(&want, 4096u, cd_direct_take) != KUI_TOY_CDDA_CARD_DONE; ++n) {
        CHECK(mail->yield_ok == (n % 2u ? 0u : 1u) && mail->want_track == cd_a && mail->want_lba == first);
        want.yield_ok = n % 2u;
        for(unsigned k = 0u; k < 8u && hardware(); ++k) {}
    }
    /* Its last sector handed on: the stream is ended there. */
    CHECK(cd_direct_bytes == 4u * 2352u && mail->card == KUI_TOY_CDDA_CARD_DONE && !stream_on());
    {
        /* Another segment asked for with no room: not yet the auxiliary's,
         * and nothing of the one before it is answered for it. */
        const struct kui_toy_cdda_want next = {generation + 1000u, cd_a, first + 4u, first + 8u, false};
        const unsigned long visits = cd_ticks;
        CHECK(cd_ask(&next, 0u, cd_no_take) == KUI_TOY_CDDA_CARD_IDLE && cd_ask(&next, 511u, cd_no_take) == KUI_TOY_CDDA_CARD_IDLE);
        CHECK(cd_ticks == visits && mail->have_generation == generation);
    }
    /* C lies in three pieces; the map loses the second. */
    const struct kui_retail_track *c = &manifest.slots[cd_c - 1u].track;
    struct kui_retail_extent *second = &manifest.slots[kui_retail_track_first_extent(c) + 1u].extent;
    const uint32_t held = second->file_block, refused = mail->refused;
    second->file_block += 2u;
    want = (struct kui_toy_cdda_want){++generation, cd_c, c->start_lba, c->end_lba, false};
    cd_direct_bytes = 0u; cd_direct_next = 0u;
    uint32_t state = KUI_TOY_CDDA_CARD_IDLE;
    for(unsigned n = 0u; n < 200u && state != KUI_TOY_CDDA_CARD_REFUSED; ++n) {
        state = cd_ask(&want, 4096u, cd_direct_take);
        CHECK(state != KUI_TOY_CDDA_CARD_DONE);
        for(unsigned k = 0u; k < 8u && hardware(); ++k) {}
    }
    /* Every byte of the first piece that is the track's, and none behind it. */
    CHECK(state == KUI_TOY_CDDA_CARD_REFUSED && mail->why == KUI_TOY_CDDA_WHY_FEED && mail->refused == refused + 1u);
    CHECK(cd_direct_bytes == held * 512u - 37u && !stream_on());
    second->file_block = held;
    /* Nothing wanted: told once, and the audio has nothing of the card. */
    want.generation = 0u;
    unsigned long ticks = cd_ticks;
    CHECK(cd_ask(&want, 4096u, cd_no_take) == KUI_TOY_CDDA_CARD_IDLE && cd_ticks == ticks + 1u && !mail->have_generation);
    CHECK(cd_ask(&want, 4096u, cd_no_take) == KUI_TOY_CDDA_CARD_IDLE && cd_ticks == ticks + 1u);
    plain_read(45100u, 20u);
    cd_close();
}
/* When the card is not the audio's to take: a read of the game's in any
 * state, the bus not the low reader's or faulted, a movie request of the low
 * reader's own path, a window, a wait of the low reader's own left standing,
 * a reception in flight, an interrupt's visit, a loan. Each time no byte goes
 * to the card and the answer is that it is not the audio's now; with the
 * cause gone the same segment is read. */
static void cd_guards(void) {
    cd_open();
    const struct kui_toy_cdda_mail *mail = &kui_toy_cdda_mail;
    struct kui_toy_cdda_mail *shared = &kui_toy_cdda_mail;
    struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    const uint32_t first = cd_tracks[cd_a - 1u].start;
    struct kui_retail_gd *s = &R.shared.service;
    /* A segment read to its end first: what the auxiliary answered for it
     * is no answer for the next. */
    const struct kui_toy_cdda_want before = {6u, cd_a, first + 50u, first + 52u, false};
    cd_direct_bytes = 0u; cd_direct_next = 0u;
    for(unsigned n = 0u; n < 20u && cd_ask(&before, 4096u, cd_direct_take) != KUI_TOY_CDDA_CARD_DONE; ++n)
        for(unsigned k = 0u; k < 8u && hardware(); ++k) {}
    CHECK(cd_direct_bytes == 2u * 2352u && mail->card == KUI_TOY_CDDA_CARD_DONE && mail->have_generation == 6u);
    const uint32_t polled = mail->seek_blocks, streams = mail->wire.starts;
    const struct kui_toy_cdda_want want = {7u, cd_a, first, first + 6u, false};
    /* The loan first: it admits no visit's work at all, and the segment is
     * not even taken up. */
    static const unsigned order[] = {11u, 0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 12u, 13u};
    for(unsigned i = 0u; i < sizeof(order) / sizeof(order[0]); ++i) {
        const unsigned what = order[i];
        const struct kui_retail_gd service = *s;
        const uint32_t state = R.shared.stream.state;
        switch(what) {
        case 0u: R.engine.active = 1u; break;
        case 1u: s->executing = 1u; break;
        case 2u: s->pending = 1u; s->command = KUI_GD_PIOREAD; break;
        case 3u: s->pending = 1u; s->command = KUI_GD_DMAREAD; break;
        case 4u: R.engine.opened = 0u; break;
        case 5u: R.shared.port.fault = true; break;
        case 6u: R.shared.port.acquired = false; break;
        case 7u: R.shared.stream.toy_suite_active = 1u; break;
        case 8u: c->movie_ahead.variant = OTHER_WINDOW; break;
        case 9u: R.shared.stream.state = 5u; break;     /* a token search of the low reader's */
        case 10u: R.shared.stream.state = 6u; break;    /* its wait for the card to end a command */
        case 11u: R.engine.native_loan_state = KUI_VMU_NATIVE_ASYNC_LOAN_WRITER; break;
        case 12u: R.engine.in_irq = 1u; break;          /* a visit inside an interrupt */
        default: R.shared.stream.state = 2u; break;     /* a reception of the low reader's in flight */
        }
        const unsigned bytes = bus_bytes, clocks = card.clocks;
        /* (Not held against the fixture's invariants while the bus is
         * marked at fault. More asks than a block is given tries.) */
        for(unsigned n = 0u; n < 10u; ++n) CHECK(cd_ask_only(&want, 4096u, cd_no_take) == KUI_TOY_CDDA_CARD_IDLE);
        CHECK(bus_bytes == bytes && card.clocks == clocks && mail->seek_blocks == polled && mail->wire.starts == streams);
        /* The segment is taken up all the same; only its reading waits. */
        CHECK(mail->have_generation == (what == 11u ? 6u : 7u) && !mail->refused && !mail->sink_bytes);
        CHECK(mail->card == (what == 11u ? KUI_TOY_CDDA_CARD_DONE : KUI_TOY_CDDA_CARD_IDLE));
        *s = service;
        R.engine.active = 0u;
        R.engine.opened = 1u;
        R.engine.in_irq = 0u;
        R.shared.port.fault = false;
        R.shared.port.acquired = true;
        R.shared.stream.toy_suite_active = 0u;
        R.shared.stream.state = state;
        c->movie_ahead.variant = 0u;
        R.engine.native_loan_state = KUI_VMU_NATIVE_ASYNC_LOAN_IDLE;
    }
    /* A visit the worker has not allowed a block by programmed transfers
     * reads none; one it has allowed reads one, and the allowance is spent. */
    shared->want_blocks = 2u; shared->seek_ok = 0u;
    CHECK(kui_native_readahead_aux_visit(KUI_NATIVE_RA_TICK, 0u, &R, 0u) == 1u);
    CHECK(mail->seek_blocks == polled && !mail->sink_bytes && mail->card == KUI_TOY_CDDA_CARD_SEEKING);
    /* A pending command that is no read does not hold the audio back. */
    s->pending = 1u; s->command = KUI_RETAIL_GD_REQ_STAT;
    cd_direct_bytes = 0u; cd_direct_next = 0u;
    CHECK(cd_ask(&want, 512u, cd_direct_take) == KUI_TOY_CDDA_CARD_STREAMING && mail->seek_blocks == polled + 1u);
    CHECK(!mail->seek_ok && cd_direct_bytes == 512u && stream_on());
    s->pending = 0u; s->command = 0u;
    /* Asked for one block, a visit hands on one; and with what it handed on
     * still in the sink, the next adds only what the sink holds. */
    for(unsigned k = 0u; k < 16u && hardware(); ++k) {}
    shared->want_blocks = 1u; shared->seek_ok = 1u;
    CHECK(kui_native_readahead_aux_visit(KUI_NATIVE_RA_TICK, 0u, &R, 0u) == 1u);
    CHECK(mail->sink_bytes == 512u && mail->card == KUI_TOY_CDDA_CARD_STREAMING && mail->seek_blocks == polled + 1u);
    shared->want_blocks = 2u;
    CHECK(kui_native_readahead_aux_visit(KUI_NATIVE_RA_TICK, 0u, &R, 0u) == 1u && mail->sink_bytes == 1024u);
    CHECK(kui_native_readahead_aux_visit(KUI_NATIVE_RA_TICK, 0u, &R, 0u) == 1u && mail->sink_bytes == 1024u);
    CHECK(mail->card == KUI_TOY_CDDA_CARD_STREAMING && !mail->refused && mail->sink_offset == 512u);
    /* The worker takes them and goes on to the segment's end. */
    cd_direct_take((void *)(uintptr_t)&want, mail->sink, mail->sink_bytes, mail->sink_offset);
    shared->sink_offset += shared->sink_bytes; shared->sink_bytes = 0u;
    for(unsigned n = 0u; n < 20u && cd_ask(&want, 4096u, cd_direct_take) != KUI_TOY_CDDA_CARD_DONE; ++n)
        for(unsigned k = 0u; k < 8u && hardware(); ++k) {}
    CHECK(cd_direct_bytes == 6u * 2352u && mail->card == KUI_TOY_CDDA_CARD_DONE);
    /* An argument the heartbeat does not send asks for no card work. */
    shared->want_generation = 8u;
    CHECK(kui_native_readahead_aux_visit(KUI_NATIVE_RA_TICK, 1u, &R, 0u) == 1u && mail->have_generation == 7u);
    /* Nor a visit that may hand on no block; nor a block the worker has not
     * claimed as its own. */
    shared->want_blocks = 0u; shared->seek_ok = 1u;
    CHECK(kui_native_readahead_aux_visit(KUI_NATIVE_RA_TICK, 0u, &R, 0u) == 1u);
    CHECK(mail->have_generation == 8u && mail->card == KUI_TOY_CDDA_CARD_IDLE && mail->seek_blocks == polled + 1u);
    shared->want_generation = 9u; shared->want_blocks = 2u;
    shared->magic ^= 1u;
    CHECK(kui_native_readahead_aux_visit(KUI_NATIVE_RA_TICK, 0u, &R, 0u) == 1u && mail->have_generation == 8u);
    shared->magic ^= 1u;
    const struct kui_toy_cdda_want none = {0u, 0u, 0u, 0u, false};
    CHECK(cd_ask(&none, 0u, cd_no_take) == KUI_TOY_CDDA_CARD_IDLE && !mail->have_generation);
    plain_read(45100u, 20u);
    cd_close();
}
#if KUI_TOY_CDDA_YIELD
/* Test 85 (YIELD): an idle window is given up for the audio only when the
 * consumer says that it runs low, with a segment wanted and no request of
 * the game's in any state; and then by its own way out, which counts no
 * window's drop. */
static void cd_yield(void) {
    cd_open();
    const struct kui_toy_cdda_mail *mail = &kui_toy_cdda_mail;
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    const struct kui_toy_wire_report *w = report();
    struct kui_retail_gd *s = &R.shared.service;
    const uint32_t first = cd_tracks[cd_a - 1u].start;
    /* Reads one behind the other: a window, which stands. */
    plain_read(45040u, 5u);
    plain_read(45045u, 5u);
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && stream_on() && w->other_windows == 1u);
    const uint32_t closes = w->stops[KUI_TOY_WIRE_STOP_CLOSE];
    const unsigned cmd12 = card.cmd12, cmd18 = card.cmd18;
    struct kui_toy_cdda_want want = {1u, cd_a, first, first + 6u, false};
    /* The consumer holds enough: the audio waits. */
    for(unsigned n = 0u; n < 3u; ++n) CHECK(cd_ask(&want, 4096u, cd_no_take) == KUI_TOY_CDDA_CARD_IDLE);
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && !mail->yields && mail->have_generation == 1u);
    /* It runs low, and a command of the game's that is no read stands. */
    want.yield_ok = true;
    s->pending = 1u; s->command = KUI_RETAIL_GD_REQ_STAT;
    CHECK(cd_ask_only(&want, 4096u, cd_no_take) == KUI_TOY_CDDA_CARD_IDLE);
    s->pending = 0u; s->command = 0u;
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && !mail->yields && stream_on());
    /* A read of the game's stands, in each of its states. */
    for(unsigned what = 0u; what < 3u; ++what) {
        if(what == 0u) R.engine.active = 1u;
        else if(what == 1u) s->executing = 1u;
        else {s->pending = 1u; s->command = KUI_GD_PIOREAD;}
        CHECK(cd_ask_only(&want, 4096u, cd_no_take) == KUI_TOY_CDDA_CARD_IDLE);
        R.engine.active = 0u; s->executing = 0u; s->pending = 0u; s->command = 0u;
        CHECK(c->movie_ahead.variant == OTHER_WINDOW && !mail->yields && stream_on());
    }
    /* Nothing wanted: the window is not the audio's to end. */
    const struct kui_toy_cdda_want none = {0u, 0u, 0u, 0u, true};
    CHECK(cd_ask(&none, 0u, cd_no_take) == KUI_TOY_CDDA_CARD_IDLE && !mail->have_generation);
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && !mail->yields && stream_on());
    CHECK(card.cmd12 == cmd12 && card.cmd18 == cmd18 && w->stops[KUI_TOY_WIRE_STOP_CLOSE] == closes);
    /* Wanted, the consumer low, nothing of the game's standing; but the
     * card does not take the stop command the window's way out sends. The
     * window's stream is ended, the window is not yet given up, and the
     * audio does nothing with the card in that visit. */
    want.generation = 2u;
    R.shared.card.device.sd.bus.transfer = bad_stop_transfer;
    bad_stop = card.cmd12 + 1u; bad_stop_done = false;
    CHECK(cd_ask_only(&want, 4096u, cd_no_take) == KUI_TOY_CDDA_CARD_IDLE);
    R.shared.card.device.sd.bus.transfer = bus_transfer;
    CHECK(bad_stop_done && card.cmd12 == cmd12 + 1u && card.cmd18 == cmd18 && !mail->yields && !stream_on());
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && !mail->seek_blocks && !mail->seek_waits);
    /* The next heartbeat: the window is given up, and in the same visit the
     * audio brings the card to its block. */
    cd_direct_bytes = 0u; cd_direct_next = 0u;
    (void)cd_ask(&want, 4096u, cd_direct_take);
    CHECK(mail->yields == 1u && !c->movie_ahead.variant && w->stops[KUI_TOY_WIRE_STOP_CLOSE] == closes + 1u);
    CHECK(w->other_windows == 1u && card.cmd12 == cmd12 + 2u && mail->seek_blocks + mail->seek_waits == 1u);
    no_drops(w);
    for(unsigned n = 0u; n < 30u && cd_ask(&want, 4096u, cd_direct_take) != KUI_TOY_CDDA_CARD_DONE; ++n)
        for(unsigned k = 0u; k < 8u && hardware(); ++k) {}
    CHECK(cd_direct_bytes == 6u * 2352u && mail->card == KUI_TOY_CDDA_CARD_DONE && mail->yields == 1u);
    /* The game reads on where it was, and has a window again. */
    plain_read(45050u, 5u);
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && w->other_windows == 2u);
    no_drops(w);
    (void)cd_ask(&none, 0u, cd_no_take);
    plain_read(45100u, 20u);
    cd_close();
}
#endif
/* The card's wait in the seek is the low reader's slices: a heartbeat's
 * visit clocks one slice of it and leaves, and a later one takes it up. A
 * read of the game's that comes while it waits is served as always, with its
 * window where it follows on, and the audio begins again behind it. */
static void cd_slices(void) {
    cd_open();
    const struct kui_toy_cdda_mail *mail = &kui_toy_cdda_mail;
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    const uint8_t budget = R.shared.stream.token_budget;
    /* The card takes 600 bytes to answer a read's first block: more slices
     * than a block is given tries. */
    card.nac_first = 600u;
    cd_play(cd_a, cd_a, 15u);
    unsigned beats = 0u;
    while(!cap.bytes) {
        CHECK(++beats < 30u);
        cd_frames(1u, 0u);
        /* Between the heartbeats the search stands, where the low reader
         * left it, with its own bound for a call of the game's put back; and
         * a visit that waited made no second one. */
        if(!cap.bytes) {
            CHECK(R.shared.stream.state == 5u && !stream_on() && mail->card == KUI_TOY_CDDA_CARD_SEEKING);
            CHECK(!R.shared.stream.token_bounded && R.shared.stream.token_budget == budget && cd_beat_ticks == 1u);
        }
    }
    /* Ten slices of 64 bytes for the 600, each visit's bytes within one
     * slice and the commands around it; then the block, and the stream. */
    CHECK(beats >= 10u && mail->seek_waits == beats - 1u && mail->seek_blocks == 1u && stream_on() && !mail->refused);
    CHECK(cd_wait_most && cd_wait_most <= KUI_SCI_STREAM_TOKEN_SLICE + 40u);
    cd_frames(10u, 3u);
    /* A read of the game's, by the low reader; then the audio's search
     * waits again (the play paused and released: begun afresh). */
    cd_read(45040u, 5u, true);
    cd_command(KUI_RETAIL_GD_PAUSE, 0u, 0u, 0u);
    cd_frames(2u, 0u);
    cd_command(KUI_RETAIL_GD_RELEASE, 0u, 0u, 0u);
    cd_frames(2u, 0u);
    CHECK(R.shared.stream.state == 5u && !stream_on() && mail->card == KUI_TOY_CDDA_CARD_SEEKING);
    /* The read that follows on from the last one comes while it waits: the
     * search is given up for it, it has its window, and the audio waits. */
    uint32_t halts = mail->data_halts;
    unsigned long bytes = cap.bytes;
    cd_read(45045u, 5u, true);
    CHECK(mail->data_halts == halts + 1u && c->movie_ahead.variant == OTHER_WINDOW && c->stats.hit_requests == 1u);
    cd_frames(5u, 3u);
#if KUI_TOY_CDDA_YIELD
    /* (Test 85's YIELD: the consumer, begun afresh, holds nothing and says
     * so, and the window is given up for it at once.) */
    CHECK(!c->movie_ahead.variant && mail->yields == 1u);
#else
    CHECK(cap.bytes == bytes && c->movie_ahead.variant == OTHER_WINDOW);
#endif
    /* A read elsewhere ends the window; the audio goes on. */
    cd_read(45100u, 12u, true);
    cd_frames(25u, 3u);
    CHECK(cap.bytes > bytes && stream_on() && !c->movie_ahead.variant);
    /* And the card's busy time behind a stop command, 150 bytes of it: the
     * read ends the audio's stream, and what the audio then asks of the card
     * waits for the card to end its command, in slices too. */
    card.nac_first = 1u;
    card.busy = 150u;
    halts = mail->data_halts;
    cd_read(45200u, 12u, true);
    CHECK(mail->data_halts == halts + 1u);
    bytes = cap.bytes;
    unsigned waits = 0u;
    for(unsigned n = 0u; n < 12u && cap.bytes == bytes; ++n) {
        cd_frames(1u, 0u);
        if(R.shared.stream.state == 6u) {
            ++waits;
            /* Not streaming while it waits. */
            CHECK(mail->card == KUI_TOY_CDDA_CARD_SEEKING && !stream_on() && cd_beat_ticks == 1u);
        }
    }
    CHECK(waits >= 2u && cap.bytes > bytes);
    cd_read(45100u, 12u, true);
    cd_frames(15u, 3u);
    CHECK(cd_wait_most <= KUI_SCI_STREAM_TOKEN_SLICE + 40u && cap.begun == 2u);
    CHECK(!R.shared.stream.token_bounded && R.shared.stream.token_budget == budget);
    cd_counts();
    plain_command(KUI_GD_STOP, 0u);
    cd_frames(2u, 0u);
    CHECK(!stream_on() && !mail->refused);
    cd_close();
}
/* The card asked of the reader while the audio's stream runs (the report's
 * save): the loan's quiesce stops the stream, the card's command is ended
 * and the card handed over. Given back, the audio finds its stream ended and
 * goes on behind the game's next read at the byte it had got to. */
static void cd_lent(void) {
    cd_open();
    const struct kui_toy_cdda_mail *mail = &kui_toy_cdda_mail;
    cd_play(cd_a, cd_a, 15u);
    cd_frames(15u, 3u);
    CHECK(stream_on() && !R.shared.service.pending && !R.engine.active);
    const unsigned cmd12 = card.cmd12;
    const uint32_t sr = hw.sr, window_stops = report()->stops[KUI_TOY_WIRE_STOP_CLOSE];
    hw.sr = 0xf0u;
    struct kui_loader_sd copied;
    CHECK(kui_retail_native_async_loan_begin(&copied) == KUI_VMU_NATIVE_ASYNC_LOAN_OK);
    CHECK(!stream_on() && card.cmd12 == cmd12 + 1u && !R.engine.hooked && !R.engine.opened &&
        !R.shared.port.acquired && !R.shared.stream.state);
    CHECK(!(m.chcr & 1u) && m.sar == 0x11111111u && m.dar == 0x22222222u && m.tcr == 0x33u);
    unsigned channel;
    CHECK(!timer_running(&channel) && !(wx.ipra & 0x0ff0u));
    /* A heartbeat during the loan is admitted no card work. */
    const unsigned clocks = card.clocks;
    hw.sr = sr;
    unsigned long bytes = cap.bytes;
    cd_frames(2u, 0u);
    CHECK(card.clocks == clocks && cap.bytes == bytes);
    hw.sr = 0xf0u;
    CHECK(kui_retail_native_async_loan_end(true) == KUI_VMU_NATIVE_ASYNC_LOAN_OK);
    hw.sr = sr;
    /* Given back: the stream was ended by the reader, once; the bus is not
     * the low reader's until the game reads. */
    cd_frames(3u, 0u);
    CHECK(mail->external_halts == 1u && card.clocks == clocks && cap.bytes == bytes &&
        mail->card == KUI_TOY_CDDA_CARD_IDLE && report()->stops[KUI_TOY_WIRE_STOP_CLOSE] == window_stops + 1u);
    /* The game reads; behind it the audio searches for its block (the card
     * slow to answer), and says so: it does not think that it still streams. */
    card.nac_first = 300u;
    cd_read(45100u, 10u, true);
    cd_frames(1u, 0u);
    CHECK(R.shared.stream.state == 5u && mail->card == KUI_TOY_CDDA_CARD_SEEKING && cap.bytes == bytes);
    cd_frames(20u, 3u);
    CHECK(cap.bytes > bytes && cap.begun == 1u && stream_on() && mail->external_halts == 1u);
    /* The game's library writes the interrupt levels for its own devices,
     * and with them the SCI's: the next visit a heartbeat makes for the
     * stream puts the reader's back. (The consumer makes none while it has
     * no room; the fixture's invariants hold the level to the stream, and
     * are not asked until it is back.) */
    hw.iprb = (uint16_t)((hw.iprb & ~0x00f0u) | 0x0050u);
    for(unsigned n = 0u; n < 10u && (hw.iprb & 0x00f0u) != 0x0010u; ++n) {
        hw.sr = cd_sr;
        cd_beat_ticks = 0u;
        clock_counter -= CD_FRAME_TICKS;
        kui_toy_cdda_consumer_heartbeat();
        hw.sr = sr;
    }
    CHECK(stream_on() && (hw.iprb & 0x00f0u) == 0x0010u);
    cd_frames(3u, 3u);
    /* Lent again, and the game reads before any heartbeat: the audio finds
     * its stream ended at the read's own visit, and it was the reader that
     * ended it, not the read. */
    const uint32_t halts = mail->data_halts;
    card.nac_first = 1u;
    hw.sr = 0xf0u;
    CHECK(kui_retail_native_async_loan_begin(&copied) == KUI_VMU_NATIVE_ASYNC_LOAN_OK && !stream_on());
    CHECK(kui_retail_native_async_loan_end(true) == KUI_VMU_NATIVE_ASYNC_LOAN_OK);
    hw.sr = sr;
    bytes = cap.bytes;
    cd_read(45200u, 10u, true);
    CHECK(mail->external_halts == 2u && mail->data_halts == halts);
    cd_frames(20u, 3u);
    CHECK(cap.bytes > bytes && cap.begun == 1u && stream_on());
    cd_counts();
    plain_command(KUI_GD_STOP, 0u);
    cd_frames(2u, 0u);
    cd_close();
}
/* A heartbeat's visits are bounded by its soft window of TMU0 time: the
 * first is always made; another only for a stream that has just handed
 * bytes on, and only if the longest such visit seen still fits. And by the
 * consumer's room. */
static void cd_bounds(void) {
    static const struct { uint32_t window, cost; unsigned most; } cases[] = {
        {2344u, 700u, 3u}, {1172u, 700u, 1u}, {2344u, 3000u, 1u}, {2344u, 120u, 13u}, {1172u, 120u, 6u},
    };
    for(unsigned i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        cd_open();
        const struct kui_toy_cdda_mail *mail = &kui_toy_cdda_mail;
        cd_window = cases[i].window; cd_cost = cases[i].cost;
        /* (A visit with a programmed read in it takes far longer: that is
         * not the time a visit that reads the stream is reckoned at.) */
        cd_seek_cost = 900u;
        cd_play(cd_a, cd_a, 15u);
        /* A visit takes its cost and the fixture's three looks at the clock,
         * 19 ticks each. Until a visit that only read the stream has been
         * seen there is no time to reckon with, and the first heartbeat of
         * the first stream may make one visit more than fits. */
        const uint32_t spent = cases[i].cost + 57u;
        cd_frames(5u, 3u);
        CHECK(mail->round_longest == spent && mail->longest <= cases[i].window + spent + 900u && stream_on());
        CHECK(cd_seek_visits == 1u && mail->visit_ticks == mail->visits * spent + 900u);
        /* From then on: never more visits than fit (every one was held
         * against the rule as it was made), and as many as fit where the
         * stream had the blocks, the window over a visit's time; one where
         * not even one fits. */
        cd_beat_most = 0u;
        const uint32_t heartbeats = mail->heartbeats, ticks = mail->visit_ticks, visits = mail->visits;
        cd_frames(55u, 3u);
        CHECK(mail->round_longest == spent && cap.bytes && mail->blocks);
        CHECK(cd_beat_most == cases[i].most && cases[i].most == (spent <= cases[i].window ? cases[i].window / spent : 1u));
        CHECK(mail->visits == cd_ticks && mail->heartbeats == heartbeats + 55u);
        CHECK(mail->visit_ticks - ticks == (mail->visits - visits) * spent && mail->visits - visits <= 55u * cases[i].most);
        CHECK(mail->longest <= cases[i].window + spent + 900u && cd_seek_visits == 1u);
        /* The longest heartbeat kept: the first, with its programmed read, or
         * one of as many visits as fit. */
        CHECK(mail->longest >= cases[i].most * spent && mail->longest >= spent + 900u);
        plain_command(KUI_GD_STOP, 0u);
        cd_frames(2u, 0u);
        cd_close();
    }
    /* Room: nothing is handed on beyond it, and under a block's worth no
     * visit is made. */
    cd_open();
    const struct kui_toy_cdda_mail *mail = &kui_toy_cdda_mail;
    const uint32_t first = cd_tracks[cd_a - 1u].start;
    const struct kui_toy_cdda_want want = {3u, cd_a, first, first + 40u, false};
    cd_cost = 0u;
    cd_direct_bytes = 0u; cd_direct_next = 0u;
    static const uint32_t rooms[] = {0u, 511u, 512u, 1023u, 1024u, 1500u, 100000u};
    for(unsigned n = 0u; n < 400u && mail->card != KUI_TOY_CDDA_CARD_DONE; ++n) {
        const uint32_t room = rooms[n % (sizeof(rooms) / sizeof(rooms[0]))];
        const unsigned long before = cd_direct_bytes, ticks = cd_ticks;
        (void)cd_ask(&want, room, cd_direct_take);
        CHECK(cd_direct_bytes - before <= room);
        if(room < 512u) CHECK(cd_ticks == ticks && cd_direct_bytes == before);
        if(room < 1024u) CHECK(cd_direct_bytes - before <= 512u);
        for(unsigned k = 0u; k < 8u && hardware(); ++k) {}
    }
    CHECK(cd_direct_bytes == 40u * 2352u && mail->card == KUI_TOY_CDDA_CARD_DONE);
    /* More room than a count of blocks in a byte can say: asked for what
     * the sink holds all the same. */
    const struct kui_toy_cdda_want large = {4u, cd_a, first + 40u, first + 60u, false};
    cd_direct_bytes = 0u; cd_direct_next = 0u;
    for(unsigned n = 0u; n < 100u && cd_ask(&large, 131072u, cd_direct_take) != KUI_TOY_CDDA_CARD_DONE; ++n) {
        CHECK(mail->want_blocks == KUI_TOY_CDDA_SINK_BYTES / 512u);
        for(unsigned k = 0u; k < 8u && hardware(); ++k) {}
    }
    CHECK(cd_direct_bytes == 20u * 2352u && mail->card == KUI_TOY_CDDA_CARD_DONE);
    /* A sink that says it holds more than the consumer has room for (no
     * visit leaves one so): nothing of it is handed on. */
    const struct kui_toy_cdda_want spoilt = {5u, cd_a, first + 60u, first + 80u, false};
    cd_direct_bytes = 0u; cd_direct_next = 0u;
    for(unsigned n = 0u; n < 20u && cd_ask(&spoilt, 512u, cd_direct_take) != KUI_TOY_CDDA_CARD_STREAMING; ++n)
        for(unsigned k = 0u; k < 8u && hardware(); ++k) {}
    CHECK(stream_on() && cd_direct_bytes);
    for(unsigned k = 0u; k < 16u && hardware(); ++k) {}
    cd_spoil_sink = 1024u;
    CHECK(cd_ask_only(&spoilt, 512u, cd_no_take) == KUI_TOY_CDDA_CARD_IDLE && mail->sink_bytes == 1024u && cd_beat_ticks == 1u);
    const struct kui_toy_cdda_want none = {0u, 0u, 0u, 0u, false};
    CHECK(cd_ask(&none, 0u, cd_no_take) == KUI_TOY_CDDA_CARD_IDLE && !mail->have_generation && !stream_on());
    plain_read(45100u, 20u);
    cd_close();
}
/* Where the heartbeat does nothing: no auxiliary bound; TMU0 out of the Toy
 * profile; SR.BL set at the consumer's call. And where the stream cannot be
 * begun: the game on DMA channel 1, or the SCI's route not the reader's. */
static void cd_unable(void) {
    cd_open();
    const struct kui_toy_cdda_mail *mail = &kui_toy_cdda_mail;
    const struct kui_toy_cdda_listener *l = &kui_toy_cdda_listener;
    const unsigned clocks = card.clocks;
    cd_play(cd_a, cd_a, 15u);
    cd_unbound = true;
    cd_frames(5u, 3u);
    CHECK(!cd_ticks && !mail->magic && card.clocks == clocks && l->starts == 1u && l->want.generation);
    cd_unbound = false;
    refill_clock_valid = false;
    cd_frames(5u, 0u);
    CHECK(!cd_ticks && !mail->magic && card.clocks == clocks);
    {
        /* Asked directly, the card's heartbeat says the same. */
        const struct kui_toy_cdda_want want = {1u, cd_a, cd_tracks[cd_a - 1u].start, cd_tracks[cd_a - 1u].end, false};
        CHECK(cd_ask(&want, 4096u, cd_no_take) == KUI_TOY_CDDA_CARD_IDLE && !cd_ticks && !mail->magic);
    }
    refill_clock_valid = true;
    cd_sr = UINT32_C(0x100000f0);
    cd_frames(5u, 0u);
    CHECK(!cd_ticks && !mail->magic && card.clocks == clocks && l->skipped == 5u);
    cd_sr = 0xf0u;
    /* The clock out of its profile while a segment is read: nothing more
     * is asked for, and the audio is told to leave the card all the same. */
    cd_frames(10u, 0u);
    CHECK(stream_on() && cap.bytes);
    refill_clock_valid = false;
    const unsigned long bytes = cap.bytes, begun = cap.begun;
    const uint32_t starts = l->starts;
    cd_frames(3u, 0u);
    CHECK(!stream_on() && !mail->have_generation && cap.bytes == bytes && !l->following);
    /* In its profile again: the consumer begins again at the sector the
     * play stands at, not where it left its segment (cd_beat and the
     * capture hold it to that). */
    refill_clock_valid = true;
    cd_frames(10u, 0u);
    CHECK(stream_on() && cap.bytes > bytes && cap.begun == begun + 1u && l->starts == starts + 1u);
    plain_command(KUI_GD_STOP, 0u);
    cd_frames(2u, 0u);
    cd_close();

    /* The game uses DMA channel 1: no stream. The one block the low reader
     * read is handed on; the game's reads are as they were. */
    cd_open();
    m.chcr = 1u;
    cd_play(cd_a, cd_a, 15u);
    cd_frames(20u, 3u);
    CHECK(mail->declined >= 15u && !mail->wire.starts && mail->first_error == KUI_WIRE_E_CHANNEL && !stream_on());
    CHECK(mail->seek_blocks == 1u && cap.bytes == 512u && mail->card == KUI_TOY_CDDA_CARD_SEEKING && m.chcr == 1u);
    cd_read(45100u, 12u, true);
    cd_frames(5u, 3u);
    CHECK(m.chcr == 1u && !mail->wire.starts && !R.shared.port.fault);
    m.chcr = 0u;
    plain_command(KUI_GD_STOP, 0u);
    cd_frames(2u, 0u);
    cd_close();

    /* The SCI's interrupt is not routed to the reader: no stream. */
    cd_open();
    R.vector600[KUI_NATIVE_SDK_SCI_COOKIE_INDEX] = 0u;
    cd_play(cd_a, cd_a, 15u);
    cd_frames(20u, 3u);
    CHECK(mail->declined >= 15u && !mail->wire.starts && !mail->first_error && !stream_on() && cap.bytes == 512u);
    R.vector600[KUI_NATIVE_SDK_SCI_COOKIE_INDEX] = KUI_NATIVE_SDK_SCI_COOKIE;
    cd_frames(20u, 3u);
    CHECK(mail->wire.starts == 1u && stream_on() && cap.bytes > 512u);
    plain_command(KUI_GD_STOP, 0u);
    cd_frames(2u, 0u);
    cd_close();
}
/* A play the game does not ask after: the player moves it on only at the
 * game's disc calls, so its passes stand uncounted, and the consumer reckons
 * where the play stands by the clock alone. Begun again in a later pass, it
 * begins at that pass's sector; and it ends with the play's last pass
 * though the drive still says that it plays. */
static void cd_unpolled(void) {
    cd_open();
    const struct kui_toy_cdda_mail *mail = &kui_toy_cdda_mail;
    const struct kui_toy_cdda_listener *l = &kui_toy_cdda_listener;
    const struct kui_toy_cdda_drive *d = &kui_toy_cdda_drive;
    /* C, 60 sectors, four times through. */
    cd_play(cd_c, cd_c, 3u);
    cd_frames(100u, 0u);
    /* 125 sectors by the clock: in the third pass. */
    CHECK(d->state == KUI_TOY_CDDA_PLAYING && !d->done && !d->laps && cap.laps >= 2u && cap.begun == 1u && l->done == 2u);
    /* The timer out of its profile for one heartbeat. */
    refill_clock_valid = false;
    cd_frames(1u, 0u);
    refill_clock_valid = true;
    CHECK(!l->following && !stream_on() && !d->done);
    cd_frames(1u, 0u);
    /* (cd_beat has held the sector it began at against the clock.) */
    CHECK(l->starts == 2u && l->done == 2u && l->sync >= 2u * 60u + 5u && l->sync < 3u * 60u && l->following);
    CHECK(l->sector == d->first + (l->sync - 2u * 60u));
    cd_frames(150u, 0u);
    /* To the last byte of the fourth pass, and no further. */
    CHECK(cap.begun == 2u && cap.bytes - cap.bytes_then == (uint64_t)(4u * 60u - l->sync) * 2352u && l->done == 4u);
    CHECK(d->state == KUI_TOY_CDDA_PLAYING && !d->done && !stream_on() && !mail->have_generation && !mail->refused);
    const unsigned long ticks = cd_ticks;
    cd_frames(10u, 0u);
    CHECK(cd_ticks == ticks);
    /* The game asks at last: the play has ended. */
    CHECK(cd_status() != 3u && d->state == KUI_TOY_CDDA_ENDED);
    cd_frames(3u, 1u);
    CHECK(cd_ticks == ticks);
    cd_counts();
    cd_close();
}
/* The sink is the worker's memory. The auxiliary writes a block's bytes only
 * where they follow on and fit, whatever the shared block says: a count or a
 * place no worker leaves there refuses the segment, and not a byte is
 * written. */
static void cd_sink(void) {
    for(unsigned kind = 0u; kind < 3u; ++kind) {
        cd_open();
        const struct kui_toy_cdda_mail *mail = &kui_toy_cdda_mail;
        struct kui_toy_cdda_mail *shared = &kui_toy_cdda_mail;
        const uint32_t first = cd_tracks[cd_a - 1u].start;
        /* The segment taken up, by a visit that may hand on no block. */
        shared->magic = KUI_TOY_CDDA_MAIL_MAGIC;
        shared->want_generation = 1u; shared->want_track = cd_a; shared->want_lba = first; shared->want_end = first + 8u;
        CHECK(kui_native_readahead_aux_visit(KUI_NATIVE_RA_TICK, 0u, &R, 0u) == 1u);
        CHECK(mail->have_generation == 1u && !mail->sink_offset && !mail->sink_bytes && !mail->refused);
        const uint32_t held = kind == 0u ? 0u : kind == 1u ? 1000u : 2000u;
        switch(kind) {
        case 0u: shared->sink_offset = 1u; break;               /* the block's bytes would not follow on */
        case 1u: shared->sink_offset = 0u - held; break;        /* they would not fit behind what is there */
        default: shared->sink_offset = 0u - held; break;        /* more is there than the sink holds */
        }
        shared->sink_bytes = held;
        memset(shared->sink, 0xa7, sizeof(shared->sink));
        shared->want_blocks = 2u; shared->seek_ok = 1u;
        CHECK(kui_native_readahead_aux_visit(KUI_NATIVE_RA_TICK, 0u, &R, 0u) == 1u);
        CHECK(mail->card == KUI_TOY_CDDA_CARD_REFUSED && mail->why == KUI_TOY_CDDA_WHY_FEED && mail->refused == 1u);
        CHECK(mail->sink_bytes == held && mail->seek_blocks == 1u && !stream_on());
        for(unsigned i = 0u; i < sizeof(shared->sink); ++i) CHECK(mail->sink[i] == 0xa7);
        /* Refused for good. */
        shared->sink_offset = 0u; shared->sink_bytes = 0u; shared->seek_ok = 1u;
        const unsigned clocks = card.clocks;
        CHECK(kui_native_readahead_aux_visit(KUI_NATIVE_RA_TICK, 0u, &R, 0u) == 1u);
        CHECK(mail->card == KUI_TOY_CDDA_CARD_REFUSED && mail->refused == 1u && card.clocks == clocks);
        const struct kui_toy_cdda_want none = {0u, 0u, 0u, 0u, false};
        CHECK(cd_ask(&none, 0u, cd_no_take) == KUI_TOY_CDDA_CARD_IDLE && !mail->have_generation);
        plain_read(45100u, 20u);
        cd_close();
    }
}
/* The game takes DMA channel 1 with a run of the audio's stream in flight.
 * The channel is left as its owner has it. Where the audio means to end its
 * stream then, the stream cannot be ended whole: the bus is marked at fault
 * and no further byte goes to the card for the audio. Where the run's end
 * finds the channel another's, the stream is at fault, its error is kept, the
 * SCI is handed back and the audio looks for its block again; no stream is
 * begun on a channel that is not free. */
static void cd_stop_failed(bool found) {
    cd_open();
    const struct kui_toy_cdda_mail *mail = &kui_toy_cdda_mail;
    cd_play(cd_a, cd_a, 15u);
    unsigned n = 0u;
    do {
        CHECK(++n < 60u);
        cd_frames(1u, 0u);
    } while(!(m.rx && stream_on() && (m.chcr & 1u)));
    const uint32_t source = m.sar, halts = mail->other_halts, control = m.chcr, streams = mail->wire.starts;
    const struct kui_toy_cdda_want none = {0u, 0u, 0u, 0u, false};
    m.sar = UINT32_C(0x11111111);
    if(found) {
        /* The run ends: the stream's interrupt, or the next heartbeat's
         * visit, finds the channel another's. */
        for(unsigned k = 0u; k < 64u && hardware(); ++k) {}
        const uint32_t sr = hw.sr;
        hw.sr = cd_sr;
        cd_beat_ticks = 0u;
        kui_toy_cdda_consumer_heartbeat();
        hw.sr = sr;
        CHECK(mail->first_error == KUI_WIRE_E_FOREIGN && mail->card == KUI_TOY_CDDA_CARD_SEEKING && !R.shared.port.fault);
    } else {
        /* The consumer asks for nothing: the stream is to be ended. (Not
         * held against the fixture's invariants from here: the bus is at
         * fault.) */
        CHECK(cd_ask_only(&none, 0u, cd_no_take) == KUI_TOY_CDDA_CARD_IDLE && !mail->have_generation);
        CHECK(R.shared.port.fault);
    }
    CHECK(!stream_on() && mail->other_halts == halts + 1u);
    CHECK((m.chcr & ~2u) == (control & ~2u) && (m.chcr & 1u) && m.sar == UINT32_C(0x11111111));
    if(found) {
        const uint32_t polled = mail->seek_blocks;
        const unsigned long bytes = cap.bytes;
        cd_frames(6u, 0u);
        CHECK(mail->seek_blocks == polled + 1u && mail->declined >= 5u && mail->wire.starts == streams && !stream_on());
        CHECK(cap.bytes == bytes + 512u && (m.chcr & 1u) && m.sar == UINT32_C(0x11111111));
        plain_command(KUI_GD_STOP, 0u);
        cd_frames(2u, 0u);
    } else {
        /* Asked again and again, the audio sends the card nothing. */
        const unsigned clocks = card.clocks, bytes = bus_bytes;
        const struct kui_toy_cdda_want want = {9u, cd_a, cd_tracks[cd_a - 1u].start, cd_tracks[cd_a - 1u].end, false};
        for(unsigned k = 0u; k < 5u; ++k) CHECK(cd_ask_only(&want, 4096u, cd_no_take) == KUI_TOY_CDDA_CARD_IDLE);
        CHECK(card.clocks == clocks && bus_bytes == bytes && !mail->declined && mail->wire.starts == streams);
        (void)cd_ask_only(&none, 0u, cd_no_take);
    }
    m.sar = source;
    m.delay = 0u;
    R.shared.port.fault = false;
    cd_close();
}
/* Blocks the card sends wrong twice over, to the stream and to the first
 * programmed read behind it, then right: nine of them along the play. A
 * segment is given up for eight tries in a row that fail, not for eight in
 * all: a read that gets through begins the count again. All nine lie in one
 * segment, the play's first pass. */
static void cd_stumbles(void) {
    cd_open();
    const struct kui_toy_cdda_mail *mail = &kui_toy_cdda_mail;
    const uint32_t first = cd_tracks[cd_a - 1u].first_block;
    cd_play(cd_a, cd_a, 15u);
    cd_frames(5u, 3u);
    CHECK(card.streaming && stream_on() && card.lba >= first);
    /* The first a little ahead of where the card stands; each next one a
     * few blocks behind the last, which the stream has yet to pass. */
    uint32_t block = card.lba + 35u;
    for(unsigned k = 1u; k <= 9u; ++k, block += 6u) {
        card.crc_lba = block;
        card.crc_xor = 0x0f0fu;
        card.crc_count = 2u;
        for(unsigned n = 0u; n < 200u && card.crc_count; ++n) cd_frames(1u, 3u);
        CHECK(!card.crc_count && !mail->refused && mail->rereads == k && R.shared.stream.stats.crc_errors == k);
    }
    CHECK(block < first + 540u && kui_toy_cdda_listener.segments == 1u && mail->wire.bad_crc == 9u);
    cd_frames(20u, 3u);
    CHECK(!mail->refused && stream_on() && cap.begun == 1u && mail->seek_blocks == 10u && kui_toy_cdda_listener.segments == 1u);
    cd_counts();
    plain_command(KUI_GD_STOP, 0u);
    cd_frames(2u, 0u);
    cd_close();
}
/* The consumer with no output counts bytes that do not follow on where its
 * segment had got to. No visit hands such on: here the fixture moves the
 * sink's place between the visit and the consumer. */
static void cd_misplaced(void) {
    cd_open();
    const struct kui_toy_cdda_listener *l = &kui_toy_cdda_listener;
    cd_play(cd_a, cd_a, 15u);
    cd_frames(10u, 3u);
    CHECK(stream_on() && cap.bytes && !l->wrong);
    const uint32_t sr = hw.sr;
    cd_spoil_offset = 4u;
    for(unsigned n = 0u; n < 10u && !cd_spoilt; ++n) {
        hw.sr = cd_sr;
        cd_beat_ticks = 0u;
        clock_counter -= CD_FRAME_TICKS;
        kui_toy_cdda_consumer_heartbeat();
        hw.sr = sr;
    }
    CHECK(cd_spoilt && l->wrong >= 1u);
    plain_command(KUI_GD_STOP, 0u);
    hw.sr = cd_sr;
    cd_beat_ticks = 0u;
    kui_toy_cdda_consumer_heartbeat();
    hw.sr = sr;
    CHECK(!stream_on() && !kui_toy_cdda_mail.have_generation);
    plain_read(45100u, 20u);
    cd_close();
}
/* The auxiliary bound anew (the cold stage binds it once; the fixture for
 * every disc): what the audio had of the card before is nothing of the new
 * binding's. */
static void cd_rebound(void) {
    cd_open();
    cd_play(cd_b, cd_b, 15u);
    cd_frames(30u, 3u);
    CHECK(stream_on() && cap.bytes && kui_toy_cdda_mail.have_generation == 1u);
    /* Left as it stands: its stream running, in the middle of its segment. */
    cd_close();
    cd_open();
    const struct kui_toy_cdda_mail *mail = &kui_toy_cdda_mail;
    CHECK(!stream_on() && !mail->magic);
    cd_play(cd_a, cd_a, 15u);
    cd_frames(30u, 3u);
    /* The first segment again, from its first byte. */
    CHECK(stream_on() && cap.bytes && cap.begun == 1u && mail->have_generation == 1u);
    CHECK(!mail->refused && !mail->external_halts && !mail->other_halts && mail->seek_blocks == 1u);
    cd_counts();
    plain_command(KUI_GD_STOP, 0u);
    cd_frames(2u, 0u);
    cd_close();
}
/* With no play the heartbeat asks for nothing: no visit, the shared block
 * untouched, and the card sent exactly what it is sent without a heartbeat
 * at all. */
static uint32_t cd_quiet_run(bool beats) {
    cd_open();
    cd_no_beat = !beats;
    traffic = 0u;
    for(unsigned n = 0u; n < 40u; ++n) {
        cd_frames(1u, 1u);
        if(n % 8u == 3u) cd_read(45040u + n, 6u, false);
        if(n % 8u == 4u) cd_read(45046u + n, 6u, false);
        if(n == 20u) {
            /* Commands that are the audio's, with no audio track named,
             * and a play of a track that is none: refused. */
            cd_command(KUI_RETAIL_GD_PAUSE, 0u, 0u, 0u);
            cd_command(KUI_RETAIL_GD_RELEASE, 0u, 0u, 0u);
            cd_command(KUI_RETAIL_GD_PLAY, 3u, 3u, 0u);
            CHECK(kui_toy_cdda_drive.state == KUI_TOY_CDDA_IDLE && kui_toy_cdda_drive.refused == 1u);
        }
    }
    cd_no_beat = false;
    static const struct kui_toy_cdda_mail zero_mail;
    static const struct kui_toy_cdda_listener zero_listener;
    CHECK(!cd_ticks && !memcmp(&kui_toy_cdda_mail, &zero_mail, sizeof(zero_mail)));
    CHECK(!memcmp(&kui_toy_cdda_listener, &zero_listener, sizeof(zero_listener)) && !cd_status_3 && cd_status_other == 40u);
    cd_close();
    return traffic;
}
static void cd_quiet(void) {
    const uint32_t kept = traffic;
    const uint32_t with = cd_quiet_run(true), without = cd_quiet_run(false);
    CHECK(with == without && with);
    traffic = kept;
}
/* The shared block's place as the stage and the auxiliary ask it. */
static void cd_place(void) {
    const uint32_t begin = UINT32_C(0xac10a920), end = UINT32_C(0xac110100), size = (uint32_t)sizeof(struct kui_toy_cdda_mail);
    CHECK(size == 1312u && offsetof(struct kui_toy_cdda_mail, sink) == size - KUI_TOY_CDDA_SINK_BYTES);
    CHECK(kui_toy_cdda_mail_address(UINT32_C(0xac10fbe0), begin, end));
    CHECK(kui_toy_cdda_mail_address(end - size, begin, end));
    CHECK(kui_toy_cdda_mail_address(UINT32_C(0xac10a920), begin, end));
    CHECK(!kui_toy_cdda_mail_address(end - size + 32u, begin, end));
    CHECK(!kui_toy_cdda_mail_address(UINT32_C(0xac10fbe4), begin, end));
    CHECK(!kui_toy_cdda_mail_address(UINT32_C(0xac10fbf0), begin, end));
    /* Off the cache lines, with all the room there is. */
    CHECK(!kui_toy_cdda_mail_address(begin + 4u, begin, end) && !kui_toy_cdda_mail_address(begin + 16u, begin, end));
    CHECK(kui_toy_cdda_mail_address(begin + 32u, begin, end));
    CHECK(!kui_toy_cdda_mail_address(begin - 32u, begin, end));
    CHECK(!kui_toy_cdda_mail_address(end, begin, end) && !kui_toy_cdda_mail_address(end + 32u, begin, end));
    CHECK(!kui_toy_cdda_mail_address(UINT32_C(0x8c10fbe0), begin, end) && !kui_toy_cdda_mail_address(0u, begin, end));
    CHECK(!kui_toy_cdda_mail_address(UINT32_C(0xffffffe0), begin, UINT32_C(0xffffffff)));
    /* The disc's table as the player keeps it: which track holds a sector. */
    cd_open();
    uint32_t start = 0u, end_lba = 0u;
    /* Before the game's first disc call there is no table: no track. */
    memset(&kui_toy_cdda_drive, 0, sizeof(kui_toy_cdda_drive));
    CHECK(!kui_toy_cdda_track_of(cd_tracks[cd_a - 1u].start, &start, &end_lba));
    (void)cd_status();
    for(unsigned n = 0u; n < manifest.track_count; ++n) {
        const struct kui_retail_track *t = &manifest.slots[n].track;
        const bool audio = !kui_retail_track_control(t);
        start = end_lba = 0u;
        CHECK(kui_toy_cdda_track_of(t->start_lba, &start, &end_lba) == (audio ? n + 1u : 0u));
        if(audio) CHECK(start == t->start_lba && end_lba == t->end_lba);
        CHECK(kui_toy_cdda_track_of(t->end_lba - 1u, &start, &end_lba) == (audio ? n + 1u : 0u));
    }
    /* The gaps between tracks belong to none. */
    CHECK(!kui_toy_cdda_track_of(cd_tracks[cd_a - 1u].start - 1u, &start, &end_lba));
    CHECK(!kui_toy_cdda_track_of(cd_tracks[cd_a - 1u].end, &start, &end_lba));
    CHECK(!kui_toy_cdda_track_of(cd_tracks[cd_d - 1u].end, &start, &end_lba) && !kui_toy_cdda_track_of(UINT32_MAX, &start, &end_lba));
    cd_close();
}

static void cd_scenarios(void) {
    printf(" Test 85, the audio from the card:\n");
    cd_place();
    for(sdir_answer = 0u; sdir_answer < 2u; ++sdir_answer) {
        cd_steady(0u, 0u, 0u);
        cd_steady(1u, 0u, 0u);
        cd_steady(2u, 0u, 0u);
        cd_steady(0u, 1500u, 20u);
        cd_steady(1u, 500u, 50u);
        cd_steady(2u, 150u, 80u);
    }
    sdir_answer = 0u;
    printf("  a track played for ever, whole on the card and in pieces, 2,352 and 2,448 bytes a sector, on a quiet "
           "bus and a busy one: every byte the consumer took the image's audio in its order, pass after pass\n");
    for(sdir_answer = 0u; sdir_answer < 2u; ++sdir_answer)
        for(unsigned kind = 0u; kind < CD_LOST_KINDS; ++kind) cd_lost(kind);
    sdir_answer = 0u;
    cd_lost_at_piece_end();
    cd_dead_block();
    cd_stumbles();
    printf("  a block of the stream with one lost data byte handed on rebuilt; with a lost CRC byte, two lost bytes or "
           "a CRC16 that does not fit never handed on, and read again by the low reader; a block the card never "
           "sends whole given up after eight tries, the rest in silence\n");
    cd_reads(0u, 0u);
    cd_reads(6000u, 5u);
    printf("  the game's reads during the music: each ends the audio's stream at once and is the image's to the "
           "byte, by the low reader and through a window; the audio waits for a window that stays open and goes on "
           "behind the reads at the byte it had got to; the status stays the playing drive's\n");
    cd_ends();
    cd_stops();
    cd_unpolled();
    printf("  a play once through, three times through, across tracks and a gap, between two sectors of a track, "
           "over a track without its file; PAUSE, RELEASE and another PLAY: exactly the play's bytes, and the audio "
           "off the card whenever the drive does not play\n");
    cd_refused();
    cd_sink();
    cd_guards();
#if KUI_TOY_CDDA_YIELD
    cd_yield();
    printf("  Test 85's YIELD: an idle window given up for the audio only with a segment wanted, the consumer low and "
           "no request of the game's in any state; by the window's own way out, no drop counted\n");
#endif
    cd_stop_failed(false);
    cd_stop_failed(true);
    printf("  segments refused each with its reason and no byte to the card; the card not taken with a read of the "
           "game's in any state, without the bus, with a window, a wait or a loan standing\n");
    cd_slices();
    cd_lent();
    cd_bounds();
    cd_unable();
    cd_misplaced();
    cd_rebound();
    cd_quiet();
    printf("  the card's wait in the seek cut into slices over heartbeats, one block by programmed transfers a "
           "heartbeat at most; a loan's quiesce stops the stream; the heartbeat's visits within its window and the "
           "consumer's room; nothing without an auxiliary, the timer's profile or a play\n");
    printf("  %lu of the stream's interrupts and %lu of its timer's answered in the audio's scenarios, no visit of "
           "theirs with a byte to the card\n", cd_interrupt_total, cd_timer_total);
}
