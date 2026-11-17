/* SPDX-License-Identifier: GPL-3.0-only */
/* Production game_image.c compressed-input staging against a frozen copy of
 * the per-take GUI decode path, on independently authored LZ4 and LZO1X ZSO
 * containers. Bytes and results must match for valid, corrupt, truncated,
 * failing and cancelled reads. File-callback counts are host model counts,
 * not console timings. */
#include "kui/game_image.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned allocations, releases, denied, deny_allocation;
static uintptr_t stage_low, stage_high;
static void *stage_malloc(size_t bytes) {
    if(deny_allocation) { ++denied; return NULL; }
    void *memory = malloc(bytes);
    assert(memory);
    ++allocations;
    stage_low = (uintptr_t)memory; stage_high = stage_low + bytes;
    return memory;
}
static void *image_calloc(size_t count, size_t bytes) {
    void *memory = calloc(count, bytes);
    assert(memory);
    ++allocations;
    return memory;
}
static void stage_free(void *memory) {
    if(memory) {
        ++releases;
        if((uintptr_t)memory == stage_low) stage_low = stage_high = 0u;
    }
    free(memory);
}
#define malloc stage_malloc
#define calloc image_calloc
#define free stage_free
#include "../src/core/game_image.c"
#undef malloc
#undef calloc
#undef free

/* More than127 blocks, so one whole-image request refills its index stage. */
#define BLOCKS 160u
#define CAP (BLOCKS * 2600u + 4096u)
#define NAME "game.zso"
#define GUARD 16u
static uint8_t container[CAP], iso[BLOCKS][2048];
static uint8_t frozen_out[BLOCKS * 2048u + 2u * GUARD], staged_out[BLOCKS * 2048u + 2u * GUARD];
/* Per block: indexed span [starts[b], starts[b+1]); encoded bytes end at
 * consumed[b]; the rest is admitted alignment padding. */
static uint32_t container_bytes, starts[BLOCKS + 1u], consumed[BLOCKS];
static unsigned checks, compared, compared_failures, padding_only;
#define CHECK(x) do { ++checks; assert(x); } while(0)

static void put32(uint8_t *p, uint32_t v) { for(unsigned i = 0; i < 4u; ++i) p[i] = (uint8_t)(v >> (8u * i)); }
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void both32(uint8_t *p, uint32_t v) {
    put32(p, v);
    for(unsigned i = 0; i < 4u; ++i) p[4u + i] = (uint8_t)(v >> ((3u - i) * 8u));
}

/* Independent encoders: LZ4 per lz4_Block_format.md, LZO1X per its token
 * grammar. No production encoder or decoder creates these bytes. */
enum kind { STORED, RLE, DENSE, LITERAL };
static enum kind kind_of(unsigned block) {
    if(block == 16u || block == 20u || block % 11u == 7u) return STORED;
    if(block % 4u == 0u) return RLE;
    if(block % 5u == 3u || block == BLOCKS - 1u) return LITERAL;
    return DENSE;
}
static size_t run_length(bool lzo, uint8_t *out, unsigned n) {
    size_t used = 0;
    if(lzo) { while(n > 255u) { out[used++] = 0; n -= 255u; } }
    else { while(n >= 255u) { out[used++] = 255u; n -= 255u; } }
    out[used++] = (uint8_t)n;
    return used;
}
static size_t encode(bool lzo, unsigned block, uint8_t *out) {
    uint8_t *expect = iso[block];
    size_t at = 0;
    unsigned done = 0;
    switch(kind_of(block)) {
    case STORED:
        if(block != 16u && block != 20u)
            for(unsigned i = 0; i < 2048u; ++i) expect[i] = (uint8_t)(block * 13u + i * 7u + (i >> 9) + 0x41u);
        memcpy(out, expect, 2048u);
        return 2048u;
    case RLE: {
        uint8_t value = (uint8_t)(block * 3u + 0x51u);
        memset(expect, value, 2048u);
        if(lzo) { /* One literal, a distance-1 M3 copy, then the end marker. */
            out[at++] = 18u; out[at++] = value; out[at++] = 32u;
            at += run_length(true, out + at, 2048u - 1u - 33u);
            out[at++] = 0; out[at++] = 0; out[at++] = 17u; out[at++] = 0; out[at++] = 0;
        } else { /* One literal, a distance-1 match, five final literals. */
            out[at++] = 0x1f; out[at++] = value; out[at++] = 1u; out[at++] = 0;
            at += run_length(false, out + at, 2048u - 1u - 5u - 19u);
            out[at++] = 0x50; memset(out + at, value, 5u); at += 5u;
        }
        return at;
    }
    case LITERAL:
        for(unsigned i = 0; i < 2048u; ++i) expect[i] = (uint8_t)(block * 29u + i * 11u + (i >> 8) * 5u + 0x43u);
        out[at++] = lzo ? 0u : 0xf0u;
        at += run_length(lzo, out + at, 2048u - (lzo ? 18u : 15u));
        memcpy(out + at, expect, 2048u); at += 2048u;
        if(lzo) { out[at++] = 17u; out[at++] = 0; out[at++] = 0; }
        return at;
    case DENSE:
        /* Hundreds of one-byte takes: distance-4 copies joined by literals. */
        if(lzo) {
            out[at++] = 17u + 4u;
            while(done < 4u) { uint8_t v = (uint8_t)(block * 7u + done * 29u + 3u); out[at++] = v; expect[done++] = v; }
            while(done < 2048u) {
                out[at++] = 77u; out[at++] = 0;
                for(unsigned i = 0; i < 3u; ++i, ++done) expect[done] = expect[done - 4u];
                uint8_t v = (uint8_t)(block * 5u + done * 11u + 1u); out[at++] = v; expect[done++] = v;
            }
            out[at++] = 17u; out[at++] = 0; out[at++] = 0;
        } else {
            out[at++] = 0x40;
            while(done < 4u) { uint8_t v = (uint8_t)(block * 7u + done * 29u + 3u); out[at++] = v; expect[done++] = v; }
            out[at++] = 4u; out[at++] = 0;
            for(unsigned i = 0; i < 4u; ++i, ++done) expect[done] = expect[done - 4u];
            while(done < 2048u - 15u) {
                uint8_t v = (uint8_t)(block * 5u + done * 11u + 1u);
                out[at++] = 0x10; out[at++] = v; expect[done++] = v;
                out[at++] = 4u; out[at++] = 0;
                for(unsigned i = 0; i < 4u; ++i, ++done) expect[done] = expect[done - 4u];
            }
            out[at++] = 0xf0; out[at++] = 0;
            while(done < 2048u) { uint8_t v = (uint8_t)(block + done); out[at++] = v; expect[done++] = v; }
        }
        CHECK(done == 2048u);
        return at;
    }
    return 0;
}
static void build(bool lzo, unsigned shift) {
    memset(container, 0xcc, sizeof(container));
    memset(iso, 0, sizeof(iso));
    /* Standalone ISO base recovery: PVD at 16, root '.' record at 20. */
    uint8_t *pvd = iso[16], *root = pvd + 156u;
    pvd[0] = 1u; memcpy(pvd + 1u, "CD001", 5u); pvd[6] = 1u;
    root[0] = 34u; root[25] = 2u; root[32] = 1u; both32(root + 2u, 20u);
    memcpy(iso[20], root, 34u);
    memcpy(container, "ZISO", 4u); put32(container + 4u, 24u);
    put32(container + 8u, BLOCKS * 2048u); put32(container + 12u, 0);
    put32(container + 16u, 2048u); container[20] = 1u; container[21] = (uint8_t)shift;
    container[22] = container[23] = 0;
    const uint32_t mask = (1u << shift) - 1u;
    uint32_t at = (24u + (BLOCKS + 1u) * 4u + mask) & ~mask;
    for(uint32_t i = 24u + (BLOCKS + 1u) * 4u; i < at; ++i) container[i] = 0xad;
    for(unsigned block = 0; block < BLOCKS; ++block) {
        starts[block] = at;
        put32(container + 24u + block * 4u, (at >> shift) |
              (kind_of(block) == STORED ? UINT32_C(0x80000000) : 0u));
        at += (uint32_t)encode(lzo, block, container + at);
        consumed[block] = at;
        while(at & mask) container[at++] = 0xad; /* Legal nonzero padding. */
        CHECK(at + 4096u <= CAP);
    }
    starts[BLOCKS] = at;
    put32(container + 24u + BLOCKS * 4u, at >> shift);
    container_bytes = at;
    for(unsigned i = 17u; i < 20u; ++i) CHECK(iso[i][0] != 34u);
}

/* A filesystem model: calls, position changes (seeks), sticky cancellation,
 * a one-shot first-call failure, a persistent failing byte and truncation. */
struct source {
    unsigned calls, seeks, cancel_call;
    uint32_t position, readable, bad_at, cancel_at;
    bool bad, cancel_touch, cancelled, fail_first;
    enum kui_game_result failure;
    size_t largest;
};
static struct source source;
static bool touches(uint64_t offset, size_t size, uint32_t at) { return offset <= at && at - offset < size; }
static enum kui_game_result source_stat(void *ctx, const char *name, uint64_t *bytes) {
    CHECK(ctx == &source && !strcmp(name, NAME));
    *bytes = container_bytes;
    return KUI_GAME_OK;
}
static enum kui_game_result source_read(void *ctx, const char *name, uint64_t offset, void *out, size_t size) {
    struct source *s = ctx;
    CHECK(s == &source && !strcmp(name, NAME) && out);
    ++s->calls;
    if(offset != s->position) ++s->seeks;
    if(size > s->largest) s->largest = size;
    /* Stage fills keep the file offset's residue modulo 32 for SCI DMA. */
    if((uintptr_t)out >= stage_low && (uintptr_t)out < stage_high)
        CHECK(!(((uintptr_t)out - (uintptr_t)offset) & 31u));
    if(s->cancel_call && s->calls >= s->cancel_call) s->cancelled = true;
    if(s->cancel_touch && touches(offset, size, s->cancel_at)) s->cancelled = true;
    if(s->cancelled) return KUI_GAME_CANCELLED;
    if(s->fail_first) { s->fail_first = false; return s->failure; }
    if(s->bad && touches(offset, size, s->bad_at)) return s->failure;
    if(offset > s->readable || size > s->readable - offset) return KUI_GAME_IO;
    memcpy(out, container + offset, size);
    s->position = (uint32_t)(offset + size);
    return KUI_GAME_OK;
}

/* Frozen copy of the GUI decode before compressed-input staging
 * (logical_read, zso_read and zso_result): one file callback per take. */
struct frozen_context { const struct kui_game_image *image; const char *name; enum kui_game_result failure; };
static enum kui_zso_result frozen_read(void *opaque, uint32_t offset, void *out, size_t size) {
    struct frozen_context *context = opaque;
    enum kui_game_result result = context->image->files.read(context->image->files.ctx,
        context->name, offset, out, size);
    if(result != KUI_GAME_OK) context->failure = result;
    return result == KUI_GAME_OK ? KUI_ZSO_OK :
        result == KUI_GAME_CANCELLED ? KUI_ZSO_CANCELLED : KUI_ZSO_IO;
}
static enum kui_game_result frozen_result(enum kui_zso_result result,
    const struct frozen_context *context) {
    switch(result) {
    case KUI_ZSO_OK: return KUI_GAME_OK;
    case KUI_ZSO_UNSUPPORTED: return KUI_GAME_UNSUPPORTED;
    case KUI_ZSO_CANCELLED: return KUI_GAME_CANCELLED;
    case KUI_ZSO_RANGE: return KUI_GAME_RANGE;
    case KUI_ZSO_IO: return context->failure != KUI_GAME_OK ? context->failure : KUI_GAME_IO;
    default: return KUI_GAME_SYNTAX;
    }
}
static enum kui_game_result frozen_logical_read(const struct kui_game_image *image,
    const char *name, uint64_t offset, void *out, size_t size) {
    if(!out || offset > image->zso.total_bytes || size > image->zso.total_bytes - offset)
        return KUI_GAME_RANGE;
    struct frozen_context context = {image, name, KUI_GAME_OK};
    uint8_t scratch[KUI_ZSO_BLOCK_BYTES], *destination = out;
    while(size) {
        uint32_t block = (uint32_t)(offset / KUI_ZSO_BLOCK_BYTES);
        uint32_t inside = (uint32_t)(offset % KUI_ZSO_BLOCK_BYTES);
        size_t take = KUI_ZSO_BLOCK_BYTES - inside;
        if(take > size) take = size;
        uint8_t *decoded = inside || take != KUI_ZSO_BLOCK_BYTES ? scratch : destination;
        enum kui_zso_result result = kui_zso_read_block(&image->zso, block, frozen_read, &context, decoded);
        if(result != KUI_ZSO_OK) return frozen_result(result, &context);
        if(decoded == scratch) memcpy(destination, scratch + inside, take);
        destination += take; offset += take; size -= take;
    }
    return KUI_GAME_OK;
}

struct outcome { enum kui_game_result result; unsigned calls, seeks; size_t largest; };
static struct outcome run(bool staged_path, const struct kui_game_image *image,
    uint64_t offset, size_t size, const struct source *setup) {
    source = *setup;
    uint8_t *out = staged_path ? staged_out : frozen_out;
    memset(out, 0x5a, size + 2u * GUARD);
    enum kui_game_result result = staged_path ? logical_read(image, NAME, offset, out + GUARD, size) :
        frozen_logical_read(image, NAME, offset, out + GUARD, size);
    for(unsigned i = 0; i < GUARD; ++i) CHECK(out[i] == 0x5a && out[GUARD + size + i] == 0x5a);
    CHECK(allocations == releases);
    return (struct outcome){result, source.calls, source.seeks, source.largest};
}
/* Identical result, and identical bytes whenever the request succeeds. */
static struct outcome same(const struct kui_game_image *image, uint64_t offset, size_t size,
    const struct source *setup, struct outcome *frozen) {
    struct outcome old = run(false, image, offset, size, setup), now = run(true, image, offset, size, setup);
    CHECK(old.result == now.result);
    if(old.result == KUI_GAME_OK) CHECK(!memcmp(frozen_out, staged_out, size + 2u * GUARD));
    else ++compared_failures;
    ++compared;
    if(frozen) *frozen = old;
    return now;
}
static unsigned block_at(uint32_t offset) {
    unsigned block = 0;
    while(block + 1u < BLOCKS && starts[block + 1u] <= offset) ++block;
    return block;
}

static void clean_reads(const struct kui_game_image *image, bool lzo, unsigned shift) {
    const struct source clean = {.readable = container_bytes};
    static const struct { uint64_t offset; size_t size; } requests[] = {
        {0, 2048u}, {16u * 2048u, 2048u}, {15u * 2048u, 3u * 2048u}, {100u, 32u}, {2047u, 2u},
        {5u * 2048u + 1000u, 5000u}, {(BLOCKS - 1u) * 2048u, 2048u},
        {(BLOCKS - 2u) * 2048u + 7u, 2048u + 100u}, {0, BLOCKS * 2048u},
    };
    for(unsigned i = 0; i < sizeof(requests) / sizeof(requests[0]); ++i) {
        struct outcome now = same(image, requests[i].offset, requests[i].size, &clean, NULL);
        CHECK(now.result == KUI_GAME_OK &&
              !memcmp(staged_out + GUARD, (const uint8_t *)iso + requests[i].offset, requests[i].size));
    }
    /* One whole-image request: two index stage fills (161 entries), then one
     * read per block; the staged path still makes a file call per block. */
    struct outcome whole = run(true, image, 0, BLOCKS * 2048u, &clean);
    CHECK(whole.result == KUI_GAME_OK && whole.calls == BLOCKS + 2u);
    /* The public sector API takes the same path. */
    source = clean;
    CHECK(kui_game_image_read(image, image->data_lba + 3u, 2u, KUI_GAME_SECTOR_MODE1,
                              staged_out, 2u * 2048u) == KUI_GAME_OK);
    CHECK(!memcmp(staged_out, iso[3], 2u * 2048u) && source.calls == 3u && allocations == releases);
    /* The launcher's executable CRC reads 16 sectors per request. */
    struct outcome old_total = {0}, new_total = {0};
    for(unsigned first = 0; first < BLOCKS; first += 16u) {
        unsigned count = BLOCKS - first < 16u ? BLOCKS - first : 16u;
        struct outcome old, now = same(image, (uint64_t)first * 2048u, count * 2048u, &clean, &old);
        CHECK(now.result == KUI_GAME_OK && !memcmp(staged_out + GUARD, iso[first], count * 2048u));
        old_total.calls += old.calls; old_total.seeks += old.seeks;
        new_total.calls += now.calls; new_total.seeks += now.seeks;
        if(old.largest > old_total.largest) old_total.largest = old.largest;
        if(now.largest > new_total.largest) new_total.largest = now.largest;
    }
    printf("GUI ZSO %s shift%u: %u sectors as 16-sector requests: %u -> %u file callbacks, "
           "%u -> %u position changes, largest read %zu -> %zu bytes\n", lzo ? "LZO1X" : "LZ4", shift,
           BLOCKS, old_total.calls, new_total.calls, old_total.seeks, new_total.seeks,
           old_total.largest, new_total.largest);
    /* Allocation failure keeps the per-take path, call for call. */
    deny_allocation = 1u;
    const unsigned refused = denied;
    struct outcome old, now = same(image, 0, BLOCKS * 2048u, &clean, &old);
    deny_allocation = 0u;
    CHECK(now.result == KUI_GAME_OK && denied == refused + 1u && old.calls == now.calls &&
          old.seeks == now.seeks && old.largest == now.largest);
}

static void corrupt_payload(const struct kui_game_image *image, unsigned shift) {
    const struct source clean = {.readable = container_bytes};
    for(unsigned block = 0; block < BLOCKS; ++block) {
        bool every = block == 1u || block == 3u || block == BLOCKS - 1u;
        if(!every && block > 24u) continue;
        unsigned stride = every ? 1u : 11u + shift;
        for(uint32_t at = starts[block]; at < starts[block + 1u]; at += stride) {
            const uint8_t saved = container[at];
            for(unsigned v = 0; v < 2u; ++v) {
                container[at] = (uint8_t)(saved ^ (v ? 0xffu : 0x01u));
                same(image, (uint64_t)block * 2048u, 2048u, &clean, NULL);
            }
            container[at] = saved;
        }
    }
}

static void corrupt_index(const struct kui_game_image *image, unsigned shift) {
    const struct source clean = {.readable = container_bytes};
    for(unsigned entry = 0; entry <= BLOCKS; entry += entry < 4u || entry + 4u > BLOCKS ? 1u : 9u) {
        uint8_t *slot = container + 24u + entry * 4u;
        const uint32_t value = get32(slot);
        const uint32_t variants[] = {
            0u, 1u, value + 1u, value - 1u, value ^ UINT32_C(0x80000000), UINT32_C(0x7fffffff),
            UINT32_MAX, (container_bytes >> shift) + 1u, value + (KUI_ZSO_MAX_SPAN >> shift) + 1u,
            entry ? get32(slot - 4u) : value,
        };
        unsigned first = entry ? entry - 1u : 0u, count = entry && entry < BLOCKS ? 2u : 1u;
        for(unsigned v = 0; v < sizeof(variants) / sizeof(variants[0]); ++v) {
            put32(slot, variants[v]);
            same(image, (uint64_t)first * 2048u, count * 2048u, &clean, NULL);
        }
        put32(slot, value);
    }
}

/* Truncation after open, and a persistently failing byte. The staged path
 * reads each block's whole indexed span. A device fails whole 512-byte
 * sectors, and valid padding ends inside the last consumed byte's sector,
 * so only this byte-granular model reaches padding the per-take decoder
 * never reads: those cases must fail staged and succeed per take. */
static void missing_bytes(const struct kui_game_image *image, unsigned shift, bool truncate) {
    const uint32_t first_data = starts[0];
    for(uint32_t at = 24u; at < container_bytes;
        at += at < first_data ? 4u : at < starts[9] ? 3u : 101u) {
        /* The byte's block and its predecessor: a non-last and a last block. */
        unsigned block = at < first_data ? 2u : block_at(at);
        unsigned first = block ? block - 1u : 0u, count = 2u;
        struct source setup = {.readable = container_bytes};
        if(truncate) setup.readable = at;
        else { setup.bad = true; setup.bad_at = at; setup.failure = KUI_GAME_IO; }
        unsigned last = first + count - 1u;
        bool padding = at >= first_data && at >= consumed[block] && at < starts[block + 1u];
        bool divergent = padding && (truncate ? block == last : block >= first && block <= last);
        if(divergent) {
            CHECK(shift);
            struct outcome old = run(false, image, (uint64_t)first * 2048u, count * 2048u, &setup);
            struct outcome now = run(true, image, (uint64_t)first * 2048u, count * 2048u, &setup);
            CHECK(old.result == KUI_GAME_OK && now.result == KUI_GAME_IO);
            ++padding_only;
            continue;
        }
        struct outcome now = same(image, (uint64_t)first * 2048u, count * 2048u, &setup, NULL);
        if(at >= first_data && at < starts[first + count]) CHECK(now.result == KUI_GAME_IO);
    }
}

static void failures_and_cancellation(const struct kui_game_image *image) {
    const struct source clean = {.readable = container_bytes};
    /* Exact mapping of the first callback's own result. */
    static const enum kui_game_result codes[] = {
        KUI_GAME_IO, KUI_GAME_NOT_FOUND, KUI_GAME_CANCELLED, KUI_GAME_RANGE, KUI_GAME_INVALID,
    };
    for(unsigned i = 0; i < sizeof(codes) / sizeof(codes[0]); ++i) {
        struct source setup = clean;
        setup.fail_first = true; setup.failure = codes[i];
        struct outcome now = same(image, 3u * 2048u, 4u * 2048u, &setup, NULL);
        CHECK(now.result == codes[i] && now.calls == 1u);
    }
    struct source setup = clean;
    setup.cancelled = true;
    struct outcome now = same(image, 0, BLOCKS * 2048u, &setup, NULL);
    CHECK(now.result == KUI_GAME_CANCELLED && now.calls == 1u);
    /* Cancellation requested by the read reaching a block's span or its
     * middle, sticky afterwards, as the launcher's cancel source is. */
    for(unsigned block = 0; block < BLOCKS; block += block < 24u ? 1u : 7u) {
        for(unsigned where = 0; where < 2u; ++where) {
            setup = clean; setup.cancel_touch = true;
            setup.cancel_at = where ? (starts[block] + consumed[block]) / 2u : starts[block];
            now = same(image, 0, BLOCKS * 2048u, &setup, NULL);
            CHECK(now.result == KUI_GAME_CANCELLED);
        }
    }
    /* A cancel seen by any one staged file call stops the request, and
     * every block makes at least one such call. */
    struct outcome whole = run(true, image, 0, BLOCKS * 2048u, &clean);
    CHECK(whole.result == KUI_GAME_OK && whole.calls >= BLOCKS);
    for(unsigned n = 1; n <= whole.calls + 1u; ++n) {
        setup = clean; setup.cancel_call = n;
        now = run(true, image, 0, BLOCKS * 2048u, &setup);
        CHECK(now.result == (n <= whole.calls ? KUI_GAME_CANCELLED : KUI_GAME_OK));
    }
}

int main(void) {
    static const unsigned shifts[] = {0u, 4u, KUI_ZSO_MAX_SHIFT};
    for(unsigned codec = 0; codec < 2u; ++codec) {
        for(unsigned k = 0; k < sizeof(shifts) / sizeof(shifts[0]); ++k) {
            const bool lzo = codec != 0u;
            build(lzo, shifts[k]);
            source = (struct source){.readable = container_bytes};
            struct kui_game_file_ops ops = {&source, source_stat, source_read};
            struct kui_game_image image;
            CHECK(kui_game_image_open_named_codec(NAME, &ops, lzo ? KUI_ZSO_LZO1X : KUI_ZSO_LZ4,
                                                  &image) == KUI_GAME_OK);
            CHECK(image.compressed && image.zso.shift == shifts[k] && image.data_lba == 0u &&
                  allocations == releases);
            clean_reads(&image, lzo, shifts[k]);
            corrupt_index(&image, shifts[k]);
            if(shifts[k] != KUI_ZSO_MAX_SHIFT) corrupt_payload(&image, shifts[k]);
            missing_bytes(&image, shifts[k], true);
            missing_bytes(&image, shifts[k], false);
            failures_and_cancellation(&image);
        }
    }
    CHECK(allocations == releases);
    CHECK(compared_failures && padding_only);
    printf("PASS GUI ZSO staging: %u requests (%u failing) match the per-take path in result and "
           "bytes: LZ4/LZO1X stored, RLE, dense, literal and last blocks, shift 0/4/9, corrupt "
           "payload/index, truncation, failing bytes, mapped failures, cancellation and allocation "
           "fallback; %u byte-granular padding-only faults fail only when staged\n",
           compared, compared_failures, padding_only);
    return 0;
}
