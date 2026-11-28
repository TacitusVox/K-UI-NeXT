/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_GAME_VGA_H
#define KUI_GAME_VGA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "kui/hash.h"

/* Ordinary copy sources remain immutable. Explicit snapshots support small
 * displacements where a delta reads a source slice overwritten by a later or
 * earlier span. The upper bit is outside the Dreamcast executable address range. */
#define KUI_VGA_SNAPSHOT_BIT UINT32_C(0x80000000)
#define KUI_VGA_SNAPSHOT_SOURCE(offset) (KUI_VGA_SNAPSHOT_BIT | (offset))
#define KUI_VGA_SNAPSHOT_BYTES 256u
#define KUI_VGA_TARGET_SHA256(hex) "@" hex
/* Use a documented IP.BIN boot filename when the internal product ID is unknown. */
#define KUI_VGA_BOOT_FILENAME(name) "#" name

struct kui_game_vga_edit {
    uint32_t offset;
    uint16_t expected, replacement;
};

/* A literal overlay or a copy from the original executable. */
struct kui_game_vga_span {
    uint32_t offset;
    uint16_t length;
    const uint8_t *replacement;
    uint32_t source_offset;
};

struct kui_game_vga_profile {
    const char *product, *version;
    char area;
    uint32_t bytes, crc;
    const struct kui_game_vga_edit *edits;
    size_t count;
    const char *sha256;
    uint32_t adler;
    const struct kui_game_vga_span *spans;
    size_t span_count;
};

#include "game_vga_catalog.inc"

/* IP.BIN stores space-padded fields. Region letters occupy fixed slots. */
static inline bool kui_game_vga_field(const uint8_t *field, size_t width, const char *value) {
    size_t i = 0;
    while (i < width && *value) {
        if (field[i++] != (uint8_t)*value++) return false;
    }
    if (*value) return false;
    while (i < width) {
        if (field[i++] != ' ') return false;
    }
    return true;
}

/* Some publishers include a known-good patched executable with a DCP, but
 * publish no source hash. Verify its complete prospective bytes, before any
 * loader modifications, by hashing original gaps interleaved with overlays. */
static inline bool kui_game_vga_target_sha(const struct kui_game_vga_profile *p) {
    return p->sha256 && p->sha256[0] == '@';
}

static inline const struct kui_game_vga_profile *kui_game_vga_candidate(
        const uint8_t *ip, uint32_t bytes) {
    if (!ip) return NULL;
    const struct kui_game_vga_profile *publisher_release = NULL;
    for (size_t i = 0; i < sizeof(kui_game_vga_profiles) / sizeof(kui_game_vga_profiles[0]); ++i) {
        const struct kui_game_vga_profile *p = &kui_game_vga_profiles[i];
        if (p->bytes != bytes) continue;
        if (p->product[0] == '#') {
            if (!kui_game_vga_target_sha(p) || !p->span_count ||
                    !kui_game_vga_field(ip + 0x60, 16, p->product + 1)) continue;
            /* Some prototype publications identify the program but do not
             * document an IP release header. Prefer the ordinary exact matches. */
            if (!p->version && !p->area) {
                publisher_release = p;
                continue;
            }
        } else if (!kui_game_vga_field(ip + 0x40, 10, p->product)) {
            continue;
        }
        if (!p->version || !kui_game_vga_field(ip + 0x4a, 6, p->version)) continue;
        unsigned area = p->area == 'J' ? 0 : p->area == 'U' ? 1 : 2;
        if (ip[0x30 + area] == (uint8_t)p->area) return p;
    }
    return publisher_release;
}

/* Some published executable deltas supply a whole-file Adler-32. Scan
 * loaded RAM only for a matching revision that uses that form of proof. */
static inline uint32_t kui_game_vga_adler_update(uint32_t initial, const uint8_t *data, uint32_t bytes) {
    uint32_t a = initial & 0xffffu, b = initial >> 16;
    while (bytes) {
        uint32_t chunk = bytes > 5552 ? 5552 : bytes;
        bytes -= chunk;
        for (uint32_t i = 0; i < chunk; ++i) {
            a += *data++;
            b += a;
        }
        a %= 65521u;
        b %= 65521u;
    }
    return (b << 16) | a;
}

static inline bool kui_game_vga_sha_matches(const char *hex, const uint8_t digest[32]) {
    static const char digits[] = "0123456789abcdef";
    if (!digest) return false;
    for (size_t i = 0; i < 32; ++i) {
        if (hex[2*i] != digits[digest[i] >> 4] || hex[2*i+1] != digits[digest[i] & 15])
            return false;
    }
    return true;
}

/* A publisher's source image and its CD rebuild can differ only in the disc
 * sector address. Both complete source hashes must be verified for the edits. */
static inline bool kui_game_vga_source_sha_matches(const char *hex, const uint8_t digest[32]) {
    return kui_game_vga_sha_matches(hex, digest)
        || (hex[64] == '|' && kui_game_vga_sha_matches(hex + 65, digest));
}

static inline uint32_t kui_game_vga_source_offset(const struct kui_game_vga_span *s) {
    return s->source_offset & ~KUI_VGA_SNAPSHOT_BIT;
}

static inline bool kui_game_vga_projection_matches(const struct kui_game_vga_profile *p,
        const uint8_t *exec) {
    /* The span guard has checked all ranges before capture calls this. */
    struct kui_sha256 hash;
    uint8_t digest[32];
    uint32_t end = 0;
    kui_sha256_init(&hash);
    for (size_t i = 0; i < p->span_count; ++i) {
        const struct kui_game_vga_span *s = &p->spans[i];
        kui_sha256_update(&hash, exec + end, s->offset - end);
        const uint8_t *src = s->replacement ? s->replacement : exec + kui_game_vga_source_offset(s);
        kui_sha256_update(&hash, src, s->length);
        end = s->offset + s->length;
    }
    kui_sha256_update(&hash, exec + end, p->bytes - end);
    kui_sha256_digest(&hash, digest);
    return kui_game_vga_sha_matches(p->sha256 + 1, digest);
}

/* Check every span before reading it. Saved and current Adler-32 values
 * cover both destinations and any source slices, so later VMU/loader
 * edits in those ranges cause an atomic skip. 0 denotes no saved proof. */
static inline uint32_t kui_game_vga_span_guard(const struct kui_game_vga_profile *p,
        const uint8_t *exec, uint32_t bytes) {
    uint32_t adler = 1, previous_end = 0, snapshot_bytes = 0;
    for (size_t i = 0; i < p->span_count; ++i) {
        const struct kui_game_vga_span *s = &p->spans[i];
        if (!s->length || s->length > bytes || s->offset > bytes - s->length
                || s->offset < previous_end) return 0;
        previous_end = s->offset + s->length;
        adler = kui_game_vga_adler_update(adler, exec + s->offset, s->length);
        if (s->replacement) {
            if (s->source_offset) return 0;
            continue;
        }
        uint32_t src = kui_game_vga_source_offset(s);
        if (src > bytes - s->length) return 0;
        if (s->source_offset & KUI_VGA_SNAPSHOT_BIT) {
            if (s->length > KUI_VGA_SNAPSHOT_BYTES - snapshot_bytes) return 0;
            snapshot_bytes += s->length;
        } else {
            for (size_t j = 0; j < p->span_count; ++j) {
                const struct kui_game_vga_span *dst = &p->spans[j];
                if (dst->offset < src + s->length && (uint64_t)dst->offset + dst->length > src)
                    return 0;
            }
        }
        adler = kui_game_vga_adler_update(adler, exec + src, s->length);
    }
    return adler ? adler : UINT32_MAX;
}

static inline uint32_t kui_game_vga_capture(const struct kui_game_vga_profile *p,
        const uint8_t *exec, uint32_t bytes, uint32_t crc, const uint8_t *sha) {
    if (!p || !exec || p->bytes != bytes || (p->crc && p->crc != crc)) return 0;
    if (p->span_count) {
        if (!p->sha256 || p->count) return 0;
        if (!kui_game_vga_target_sha(p)) {
            if (!kui_game_vga_source_sha_matches(p->sha256, sha)) return 0;
            return kui_game_vga_span_guard(p, exec, bytes);
        }
        uint32_t guard = kui_game_vga_span_guard(p, exec, bytes);
        return guard && kui_game_vga_projection_matches(p, exec) ? guard : 0;
    }
    if (p->adler && !p->crc && !p->sha256)
        return kui_game_vga_adler_update(1, exec, bytes);
    return 0;
}

/* Identity checks apply to the unmodified program. Stage owners retain
 * their original checksum through bootstrap/VMU preparation; a changed
 * instruction at any target still vetoes the whole edit set at handoff. */
static inline bool kui_game_vga_apply_profile(const struct kui_game_vga_profile *p,
        uint8_t *executable, uint32_t bytes, uint32_t original_crc,
        const uint8_t *sha, uint32_t adler) {
    if (!p || !executable || p->bytes != bytes || bytes < 2 ||
            (p->crc && p->crc != original_crc)) return false;
    if (kui_game_vga_target_sha(p)) {
        if (!p->span_count || !adler) return false;
    } else if (p->sha256) {
        if (!kui_game_vga_source_sha_matches(p->sha256, sha)) return false;
    } else if (!p->crc && (!p->adler || p->adler != adler)) {
        return false;
    }

    if (p->span_count) {
        if (!p->sha256 || !adler || p->count ||
                kui_game_vga_span_guard(p, executable, bytes) != adler) return false;
        uint8_t snapshots[KUI_VGA_SNAPSHOT_BYTES];
        uint32_t saved = 0;
        for (size_t i = 0; i < p->span_count; ++i) {
            const struct kui_game_vga_span *span = &p->spans[i];
            if (span->source_offset & KUI_VGA_SNAPSHOT_BIT) {
                const uint8_t *src = executable + kui_game_vga_source_offset(span);
                for (uint32_t n = 0; n < span->length; ++n) snapshots[saved++] = src[n];
            }
        }
        saved = 0;
        for (size_t i = 0; i < p->span_count; ++i) {
            const struct kui_game_vga_span *span = &p->spans[i];
            const uint8_t *src = span->replacement;
            if (span->source_offset & KUI_VGA_SNAPSHOT_BIT) {
                src = snapshots + saved;
                saved += span->length;
            } else if (!src) {
                src = executable + span->source_offset;
            }
            for (uint32_t n = 0; n < span->length; ++n) executable[span->offset + n] = src[n];
        }
        return true;
    }
    for (size_t i = 0; i < p->count; ++i) {
        const struct kui_game_vga_edit *e = &p->edits[i];
        if (e->offset > bytes - 2) return false;
        const uint8_t *at = executable + e->offset;
        if ((uint16_t)(at[0] | ((uint16_t)at[1] << 8)) != e->expected) return false;
    }
    for (size_t i = 0; i < p->count; ++i) {
        const struct kui_game_vga_edit *e = &p->edits[i];
        executable[e->offset] = (uint8_t)e->replacement;
        executable[e->offset + 1] = (uint8_t)(e->replacement >> 8);
    }
    return true;
}

#endif
