/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/core.h"
#include "kui/game_vga.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void make_ip(uint8_t ip[256], const struct kui_game_vga_profile *p) {
    memset(ip, ' ', 256);
    size_t slot = p->area == 'J' ? 0 : p->area == 'U' ? 1 : 2;
    ip[0x30 + slot] = (uint8_t)p->area;
    if (p->product[0] == '#')
        memcpy(ip + 0x60, p->product + 1, strlen(p->product + 1));
    else
        memcpy(ip + 0x40, p->product, strlen(p->product));
    if (p->version) memcpy(ip + 0x4a, p->version, strlen(p->version));
}

static void put_word(uint8_t *at, uint16_t value) {
    at[0] = (uint8_t)value;
    at[1] = (uint8_t)(value >> 8);
}

static void exercise_profile(const struct kui_game_vga_profile *p) {
    uint8_t ip[256], digest[32] = {0};
    uint8_t *source = malloc(p->bytes);
    uint8_t *exec = malloc(p->bytes);
    assert(source && exec);
    memset(source, 0xcc, p->bytes);
    make_ip(ip, p);
    /* Also rejects duplicate profile keys: lookup must return each entry itself. */
    assert(kui_game_vga_candidate(ip, p->bytes) == p);
    assert(!kui_game_vga_candidate(ip, p->bytes - 1));
    size_t field_offset = p->product[0] == '#' ? 0x60 : 0x40;
    ip[field_offset] = '?';
    assert(!kui_game_vga_candidate(ip, p->bytes));
    make_ip(ip, p);
    ip[0x4a] = '?';
    if (p->version) assert(!kui_game_vga_candidate(ip, p->bytes));
    else assert(kui_game_vga_candidate(ip, p->bytes) == p);
    make_ip(ip, p);
    memset(ip + 0x30, ' ', 8);
    if (p->area) assert(!kui_game_vga_candidate(ip, p->bytes));
    else {
        assert(kui_game_vga_candidate(ip, p->bytes) == p);
        memcpy(ip + 0x40, "OTHER-0000V9.999", 16);
        memcpy(ip + 0x30, "JUE", 3);
        assert(kui_game_vga_candidate(ip, p->bytes) == p);
    }
    make_ip(ip, p);
    assert(p->bytes >= 2 && (p->count != 0) != (p->span_count != 0));
    for (size_t i = 0; i < p->count; ++i) {
        const struct kui_game_vga_edit *e = &p->edits[i];
        assert(e->offset <= p->bytes - 2 && !(e->offset & 1u));
        for (size_t j = 0; j < i; ++j) assert(e->offset != p->edits[j].offset);
        put_word(source + e->offset, e->expected);
    }
    memcpy(exec, source, p->bytes);
    if (kui_game_vga_target_sha(p)) {
        assert(strlen(p->sha256 + 1) == 64);
        assert(kui_game_vga_span_guard(p, exec, p->bytes));
        assert(!kui_game_vga_capture(p, exec, p->bytes, p->crc, NULL));
        assert(!kui_game_vga_apply_profile(p, exec, p->bytes, p->crc, NULL, 0));
        assert(!memcmp(source, exec, p->bytes));
        free(exec);
        free(source);
        return;
    }
    if (p->sha256) {
        size_t length = strlen(p->sha256);
        assert(length == 64 || (length == 129 && p->sha256[64] == '|'));
        for (size_t offset = 0; offset < length; offset += 65) {
            for (size_t i = 0; i < sizeof(digest); ++i) {
                unsigned pair;
                assert(sscanf(p->sha256 + offset + 2 * i, "%2x", &pair) == 1);
                digest[i] = (uint8_t)pair;
            }
            assert(kui_game_vga_source_sha_matches(p->sha256, digest));
        }
    }
    uint32_t guard = kui_game_vga_capture(p, exec, p->bytes, p->crc, digest);
    if (p->span_count) assert(guard != 0);
    if (!p->crc && !p->sha256) {
        assert(p->adler && guard == kui_game_vga_adler_update(1, exec, p->bytes));
        guard = p->adler; /* model the saved identity of the actual release */
    }

    if (p->crc) {
        assert(!kui_game_vga_apply_profile(p, exec, p->bytes, p->crc ^ 1u, digest, guard));
    }
    if (p->sha256) {
        digest[0] ^= 1u;
        assert(!kui_game_vga_apply_profile(p, exec, p->bytes, p->crc, digest, guard));
        digest[0] ^= 1u;
    } else if (!p->crc) {
        assert(!kui_game_vga_apply_profile(p, exec, p->bytes, p->crc, digest, guard ^ 1u));
    }
    assert(!memcmp(source, exec, p->bytes));

    uint32_t last = p->count ? p->edits[p->count - 1].offset : p->spans[p->span_count - 1].offset;
    exec[last] ^= 1;
    assert(!kui_game_vga_apply_profile(p, exec, p->bytes, p->crc, digest, guard));
    exec[last] ^= 1;
    assert(!memcmp(source, exec, p->bytes));

    if (p->span_count) {
        assert(!kui_game_vga_capture(p, exec, p->bytes, p->crc, NULL));
        assert(!kui_game_vga_apply_profile(p, exec, p->bytes, p->crc, NULL, guard));
        assert(!kui_game_vga_apply_profile(p, exec, p->bytes, p->crc, digest, 0));
        for (size_t i = 0; i < p->span_count; ++i) {
            const struct kui_game_vga_span *s = &p->spans[i];
            if (s->replacement) continue;
            exec[kui_game_vga_source_offset(s)] ^= 1;
            assert(!kui_game_vga_apply_profile(p, exec, p->bytes, p->crc, digest, guard));
            exec[kui_game_vga_source_offset(s)] ^= 1;
        }
        assert(!memcmp(source, exec, p->bytes));
    }
    /* For Dream Match, these full-buffer CRCs were obtained from the
     * independently decoded DCP fixture, with 0xcc outside the target sites. */
    bool dream_match = !strcmp(p->product, "T3101N");
    if (dream_match) assert(kui_crc32(0, exec, p->bytes) == 0xdd4ecd93u);
    assert(kui_game_vga_apply_profile(p, exec, p->bytes, p->crc, digest, guard));
    if (dream_match) assert(kui_crc32(0, exec, p->bytes) == 0x7e3250a2u);
    uint8_t *expected = malloc(p->bytes);
    assert(expected);
    memcpy(expected, source, p->bytes);
    for (size_t i = 0; i < p->count; ++i)
        put_word(expected + p->edits[i].offset, p->edits[i].replacement);
    for (size_t i = 0; i < p->span_count; ++i) {
        const struct kui_game_vga_span *s = &p->spans[i];
        memcpy(expected + s->offset, s->replacement ? s->replacement : source + kui_game_vga_source_offset(s), s->length);
    }
    assert(!memcmp(expected, exec, p->bytes));
    free(expected);
    free(exec);
    free(source);
}

static void check_spans(void) {
    uint8_t data[32];
    for (size_t i = 0; i < sizeof(data); ++i) data[i] = (uint8_t)i;
    static const uint8_t literal[] = {100, 101, 102, 103};
    uint8_t expected[sizeof(data)], now[sizeof(data)];
    uint8_t digest[32] = {0};
    struct kui_game_vga_span spans[] = {{0, 4, literal, 0}, {16, 4, NULL, 12}};
    struct kui_game_vga_profile p = {
        .bytes = sizeof(data), .sha256 = "0000000000000000000000000000000000000000000000000000000000000000",
        .spans = spans, .span_count = 2
    };
    uint32_t guard = kui_game_vga_capture(&p, data, sizeof(data), 0, digest);
    assert(guard);
    memcpy(now, data, sizeof(now));
    memcpy(expected, data, sizeof(expected));
    memcpy(expected, literal, sizeof(literal));
    memcpy(expected + 16, data + 12, 4);
    assert(kui_game_vga_apply_profile(&p, now, sizeof(now), 0, digest, guard));
    assert(!memcmp(now, expected, sizeof(now)));
    data[15] ^= 1; /* Last byte of copy source. */
    assert(kui_game_vga_span_guard(&p, data, sizeof(data)) != guard);
    spans[1].source_offset = 2; /* overlaps the first destination */
    assert(!kui_game_vga_span_guard(&p, data, sizeof(data)));
    spans[1].source_offset = UINT32_MAX;
    assert(!kui_game_vga_span_guard(&p, data, sizeof(data)));
    spans[1].source_offset = 12;
    spans[1].offset = 3; /* overlapping destinations */
    assert(!kui_game_vga_span_guard(&p, data, sizeof(data)));
    spans[1].offset = 29;
    assert(!kui_game_vga_span_guard(&p, data, sizeof(data)));
    spans[1].length = 0;
    assert(!kui_game_vga_span_guard(&p, data, sizeof(data)));
}

static void check_projection_and_snapshots(void) {
    uint8_t data[32], expected[32], work[32];
    for (size_t i = 0; i < sizeof(data); ++i) data[i] = (uint8_t)i;
    memcpy(expected, data, sizeof(data));
    memcpy(expected, data + 12, 4);
    memcpy(expected + 12, data, 4);
    const struct kui_game_vga_span swaps[] = {
        {0, 4, NULL, KUI_VGA_SNAPSHOT_SOURCE(12)},
        {12, 4, NULL, KUI_VGA_SNAPSHOT_SOURCE(0)}
    };
    struct kui_game_vga_profile p = {
        .bytes = sizeof(data), .spans = swaps, .span_count = 2,
        .sha256 = KUI_VGA_TARGET_SHA256("83f047f56641c641f282c90be48ba2f6e52dfb35d4315cb4f9c5920b14e1ac4e")
    };
    memcpy(work, data, sizeof(work));
    uint32_t guard = kui_game_vga_capture(&p, work, sizeof(work), 0, NULL);
    assert(guard && !memcmp(work, data, sizeof(work)));
    work[24] ^= 1; /* An untouched byte must match the publisher's target. */
    assert(!kui_game_vga_capture(&p, work, sizeof(work), 0, NULL));
    work[24] ^= 1;
    work[0] ^= 1; /* Overlapping original source also participates in the hash. */
    assert(!kui_game_vga_capture(&p, work, sizeof(work), 0, NULL));
    assert(!kui_game_vga_apply_profile(&p, work, sizeof(work), 0, NULL, guard));
    work[0] ^= 1;
    assert(!kui_game_vga_apply_profile(&p, work, sizeof(work), 0, NULL, 0));
    assert(!memcmp(work, data, sizeof(work)));
    assert(kui_game_vga_apply_profile(&p, work, sizeof(work), 0, NULL, guard));
    assert(!memcmp(expected, work, sizeof(work)));
    uint8_t large[520] = {0};
    struct kui_game_vga_span over[] = {
        {0, 130, NULL, KUI_VGA_SNAPSHOT_SOURCE(260)},
        {130, 127, NULL, KUI_VGA_SNAPSHOT_SOURCE(390)}
    };
    p.spans = over;
    assert(!kui_game_vga_span_guard(&p, large, sizeof(large)));
    over[1].length = 126;
    assert(kui_game_vga_span_guard(&p, large, sizeof(large)));
}

int main(void) {
    uint8_t blank_ip[256] = {0};
    uint8_t data[14000];
    memset(data, 255, sizeof(data));
    assert(kui_game_vga_adler_update(1, data, sizeof(data)) == 0xe4517c7bu);
    assert(!kui_game_vga_candidate(NULL, 0));
    assert(!kui_game_vga_apply_profile(kui_game_vga_candidate(blank_ip, sizeof(data)),
        data, sizeof(data), 0, NULL, 0));
    check_spans();
    check_projection_and_snapshots();
    const size_t count = sizeof(kui_game_vga_profiles) / sizeof(kui_game_vga_profiles[0]);
    for (size_t i = 0; i < count; ++i) exercise_profile(&kui_game_vga_profiles[i]);
    printf("PASS VGA: %zu profiles, identity checks, atomic edits, guarded source copies and Dream Match DCP oracle\n", count);
    return 0;
}
