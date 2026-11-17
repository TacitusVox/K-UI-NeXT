/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/deflate.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>

/* zlib is a host-only independent compressor and acceptance oracle. Explicit
 * bit streams exercise invalid code trees and tokens a compressor cannot emit.
 * The production decoder has no zlib dependency. */
#define CAP 8192u
static struct {
    _Alignas(4) uint8_t data[CAP];
    uint32_t size, base, limit, calls, fail_on, chunk, preload;
    size_t largest;
    enum kui_zso_result fault;
} source;
static struct {uint8_t before[19], data[2048], after[23];} output;
static struct {uint8_t before[13]; struct kui_deflate_workspace data; uint8_t after[17];} work;
static uint8_t expected[2048], oracle[2048], backup[CAP];
static uint32_t bit_count, random_state = 0x6af198c3u;
static uint32_t random_word(void) {
    random_state ^= random_state << 13; random_state ^= random_state >> 17;
    random_state ^= random_state << 5; return random_state;
}
/* Views point into the aligned stream; a nonzero chunk returns short views so
 * fault injection reaches every byte boundary. */
static enum kui_zso_result view_source(void *context, uint32_t offset, uint32_t wanted,
    const uint8_t **data, uint32_t *available) {
    (void)context;
    ++source.calls;
    assert(source.calls < 16384u);
    assert(wanted && offset >= source.base && offset < source.limit && wanted <= source.limit - offset);
    assert(offset - source.base < source.size && wanted <= source.size - (offset - source.base));
    if(wanted > source.largest) source.largest = wanted;
    if(source.calls == source.fail_on) return source.fault;
    *data = source.data + (offset - source.base);
    *available = source.chunk && wanted > source.chunk ? source.chunk : wanted;
    return KUI_ZSO_OK;
}
static void canaries(void) {
    for(unsigned i = 0; i < sizeof(output.before); ++i) assert(output.before[i] == 0xa6);
    for(unsigned i = 0; i < sizeof(output.after); ++i) assert(output.after[i] == 0xa6);
    for(unsigned i = 0; i < sizeof(work.before); ++i) assert(work.before[i] == 0xa6);
    for(unsigned i = 0; i < sizeof(work.after); ++i) assert(work.after[i] == 0xa6);
}
static enum kui_zso_result decode(uint32_t bytes, uint32_t *consumed) {
    assert(bytes <= source.size && bytes <= UINT32_MAX - source.base);
    source.limit = source.base + bytes; source.calls = 0;
    memset(&output, 0xa6, sizeof(output)); memset(&work, 0xa6, sizeof(work));
    /* A preloaded cursor already holds the stream's first bytes in its view. */
    uint32_t k = source.preload < bytes ? source.preload : 0;
    work.data.in = (struct kui_zso_cursor){view_source, &source, k ? source.data : NULL,
        k ? source.data + k : NULL, source.base + k, source.limit, 0};
    enum kui_zso_result result = kui_deflate_decode(&work.data, output.data, consumed);
    if(bytes && bytes <= KUI_ZSO_MAX_SPAN) {
        assert(work.data.in.view == NULL && work.data.in.context == NULL);
    }
    canaries(); return result;
}
static void begin(void) {
    memset(&source, 0, sizeof(source)); source.base = 37; bit_count = 0;
}
static void put(unsigned value, unsigned bits) {
    while(bits--) {
        assert(bit_count / 8u < CAP);
        source.data[bit_count / 8u] |= (uint8_t)((value & 1u) << (bit_count & 7u));
        ++bit_count; value >>= 1;
    }
    source.size = (bit_count + 7u) / 8u;
}
static void huffman(unsigned value, unsigned bits) {
    while(bits--) put((value >> bits) & 1u, 1u);
}
static void fixed_symbol(unsigned value) {
    if(value < 144u) huffman(value + 48u, 8u);
    else if(value < 256u) huffman(value - 144u + 400u, 9u);
    else if(value < 280u) huffman(value - 256u, 7u);
    else huffman(value - 280u + 192u, 8u);
}
static void fixed_header(bool final) {put(final ? 1u : 0u, 1u); put(1u, 2u);}
static void singleton_header(bool final) {
    /* Code-length tree: symbol18 has code0/1bit; symbols0,1 have
     * codes10,11/2bits. A literal tree with only EOB and no distance tree
     * is permitted when this DEFLATE block emits no matches. */
    static const unsigned order[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    put(final ? 1u : 0u, 1u); put(2, 2u); put(0, 5u); put(0, 5u); put(14, 4u);
    for(unsigned i = 0; i < 18u; ++i)
        put(order[i] == 18u ? 1u : order[i] <= 1u ? 2u : 0u, 3u);
    huffman(0, 1u); put(127, 7u); /* 138 zero widths. */
    huffman(0, 1u); put(107, 7u); /* 118 zero widths. */
    huffman(3, 2u); /* Literal EOB256 has width1. */
    huffman(2, 2u); /* Sole declared distance symbol has width0. */
    huffman(0, 1u); /* EOB. */
}
static void stored(unsigned bytes, bool final) {
    put(final ? 1u : 0u, 1u); put(0, 2u);
    while(bit_count & 7u) put(1u, 1u); /* Ignored alignment bits may be nonzero. */
    put(bytes, 16u); put(bytes ^ 65535u, 16u);
    for(unsigned i = 0; i < bytes; ++i) put((i * 37u) & 255u, 8u);
}
static void oracle_check(const uint8_t *bytes, unsigned size, const uint8_t *wanted) {
    z_stream stream; memset(&stream, 0, sizeof(stream));
    assert(inflateInit2(&stream, -15) == Z_OK);
    stream.next_in = (Bytef *)bytes; stream.avail_in = size;
    stream.next_out = oracle; stream.avail_out = sizeof(oracle);
    assert(inflate(&stream, Z_FINISH) == Z_STREAM_END);
    assert(stream.total_out == sizeof(oracle) && stream.total_in == size);
    assert(!memcmp(oracle, wanted, sizeof(oracle)));
    assert(inflateEnd(&stream) == Z_OK);
}
static void valid(bool faults) {
    oracle_check(source.data, source.size, expected);
    static const uint32_t chunks[] = {0, 7, 1};
    uint32_t consumed = 0xdeadbeefu, calls = 0;
    for(unsigned c = 0; c < sizeof(chunks) / sizeof(chunks[0]); ++c) {
        source.chunk = chunks[c]; consumed = 0xdeadbeefu;
        assert(decode(source.size, &consumed) == KUI_ZSO_OK && consumed == source.size);
        assert(!memcmp(output.data, expected, sizeof(expected)));
        calls = source.calls;
    }
    /* Decoding starts at the cursor's position, inside a partly read view. */
    for(uint32_t preload = 1; preload <= 5; preload += 4) {
        source.preload = preload; source.chunk = preload; consumed = 0xdeadbeefu;
        assert(decode(source.size, &consumed) == KUI_ZSO_OK && consumed == source.size);
        assert(!memcmp(output.data, expected, sizeof(expected)));
    }
    source.preload = 0; source.chunk = 1;
    /* One-byte views: a fault at every call reaches every byte boundary. */
    if(faults) for(unsigned error = 0; error < 2; ++error) {
        source.fault = error ? KUI_ZSO_CANCELLED : KUI_ZSO_IO;
        for(uint32_t call = 1; call <= calls; ++call) {
            source.fail_on = call; consumed = 0xdeadbeefu;
            assert(decode(source.size, &consumed) == source.fault);
            assert(consumed == 0xdeadbeefu && source.calls == call);
        }
    }
    source.fail_on = 0; source.chunk = 0;
}
static void token_forms(void) {
    begin(); stored(2048, true);
    for(unsigned i = 0; i < 2048; ++i) expected[i] = (uint8_t)(i * 37u);
    valid(true);
    begin(); fixed_header(true);
    for(unsigned i = 0; i < 2048; ++i) {expected[i] = (uint8_t)i; fixed_symbol(expected[i]);}
    fixed_symbol(256); valid(true);
    /* Multiple DEFLATE blocks share this independent CSO sector's dictionary. */
    begin(); stored(3, false); fixed_header(false);
    fixed_symbol(285); huffman(2, 5u); /* length258, distance3. */
    fixed_symbol(256); fixed_header(true);
    for(unsigned i = 261; i < 2048; ++i) fixed_symbol(i & 255u);
    fixed_symbol(256);
    expected[0] = 0; expected[1] = 37; expected[2] = 74;
    for(unsigned i = 3; i < 261; ++i) expected[i] = expected[i - 3u];
    for(unsigned i = 261; i < 2048; ++i) expected[i] = (uint8_t)i;
    valid(true);
    /* A final empty stored block after an exactly full compressed block. */
    begin(); fixed_header(false);
    for(unsigned i = 0; i < 2048; ++i) {expected[i] = (uint8_t)i; fixed_symbol(expected[i]);}
    fixed_symbol(256); stored(0, true); valid(true);
    begin(); singleton_header(false); stored(2048, true);
    for(unsigned i = 0; i < 2048; ++i) expected[i] = (uint8_t)(i * 37u);
    valid(true);
    puts("PASS DEFLATE tokens: stored/fixed ranges, arbitrary alignment bits, cross-block matches and empty final blocks against zlib");
}
static void compress_sample(unsigned sample, int strategy, int level) {
    begin();
    for(unsigned i = 0; i < 2048; ++i) {
        unsigned value = random_word();
        switch(sample % 7u) {
        case 0: expected[i] = (uint8_t)value; break;
        case 1: expected[i] = (uint8_t)(value % 4u); break;
        case 2: expected[i] = (uint8_t)(i % (sample % 127u + 1u)); break;
        case 3: expected[i] = (uint8_t)((i / 31u) % 3u); break;
        case 4: expected[i] = i > 513u && (value & 7u) ? expected[i - 513u] : (uint8_t)value; break;
        case 5: expected[i] = i > 7u && (value & 3u) ? expected[i - 7u] : (uint8_t)value; break;
        default: expected[i] = 'A'; break;
        }
    }
    z_stream stream; memset(&stream, 0, sizeof(stream));
    assert(deflateInit2(&stream, level, Z_DEFLATED, -15, 8, strategy) == Z_OK);
    stream.next_in = expected; stream.avail_in = sizeof(expected);
    stream.next_out = source.data; stream.avail_out = CAP;
    assert(deflate(&stream, Z_FINISH) == Z_STREAM_END);
    source.size = (uint32_t)stream.total_out;
    assert(deflateEnd(&stream) == Z_OK);
}
static void compressed_oracles(void) {
    const int strategies[] = {Z_DEFAULT_STRATEGY, Z_FILTERED, Z_HUFFMAN_ONLY, Z_RLE, Z_FIXED};
    unsigned types = 0;
    for(unsigned sample = 0; sample < 1800; ++sample) {
        compress_sample(sample, strategies[sample % 5u], (int)(sample % 10u));
        types |= 1u << ((source.data[0] >> 1) & 3u);
        valid(sample < 10u);
    }
    assert((types & 7u) == 7u);
    puts("PASS DEFLATE oracle: 1800 zlib blocks, levels0--9, all5 strategies and stored/fixed/dynamic trees");
}
static void reject(void) {
    uint32_t consumed = 0xdeadbeefu;
    assert(decode(source.size, &consumed) != KUI_ZSO_OK && consumed == 0xdeadbeefu);
}
static void code_header(unsigned first, unsigned second, unsigned third, unsigned fourth) {
    put(1, 1u); put(2, 2u); put(0, 5u); put(0, 5u); put(0, 4u);
    put(first, 3u); put(second, 3u); put(third, 3u); put(fourth, 3u);
}
static void malformed(void) {
    begin(); stored(2048, true); unsigned full = source.size;
    for(unsigned n = 0; n < full; ++n) {
        uint32_t consumed = 0xdeadbeefu;
        assert(decode(n, &consumed) == KUI_ZSO_INVALID && consumed == 0xdeadbeefu);
    }
    source.data[source.size++] = 0xa7; source.data[source.size++] = 0x19;
    uint32_t consumed = 0;
    assert(decode(source.size, &consumed) == KUI_ZSO_OK && consumed == full);
    source.base = UINT32_MAX - source.size;
    assert(decode(source.size, &consumed) == KUI_ZSO_OK && consumed == full);
    source.base = 37;
    source.data[3] ^= 1u; reject(); /* Bad stored LEN/NLEN complement. */
    for(unsigned dynamic = 0; dynamic < 2u; ++dynamic) {
        compress_sample(6, dynamic ? Z_HUFFMAN_ONLY : Z_FIXED, 6);
        assert(((source.data[0] >> 1) & 3u) == (dynamic ? 2u : 1u));
        full = source.size;
        for(unsigned n = 0; n < full; ++n) {
            uint32_t used = 0xdeadbeefu;
            assert(decode(n, &used) == KUI_ZSO_INVALID && used == 0xdeadbeefu);
        }
    }
    begin(); stored(2049, true); reject();
    begin(); stored(2047, true); reject();
    begin(); put(7, 3u); reject(); /* Reserved block type. */
    begin(); fixed_header(true); fixed_symbol(256); reject();
    for(unsigned reserved = 286; reserved < 288; ++reserved) {
        begin(); fixed_header(true); fixed_symbol(reserved); reject();
    }
    for(unsigned reserved = 30; reserved < 32; ++reserved) {
        begin(); fixed_header(true); fixed_symbol('A'); fixed_symbol(257);
        huffman(reserved, 5u); fixed_symbol(256); reject();
    }
    begin(); fixed_header(true); fixed_symbol(257); huffman(0, 5u); fixed_symbol(256); reject();
    begin(); fixed_header(true); fixed_symbol('A'); fixed_symbol(257);
    huffman(1, 5u); fixed_symbol(256); reject(); /* Match before output. */
    begin(); fixed_header(true);
    for(unsigned i = 0; i < 2048; ++i) fixed_symbol('A');
    fixed_symbol('B'); fixed_symbol(256); reject();
    begin(); fixed_header(true);
    for(unsigned i = 0; i < 2047; ++i) fixed_symbol('A');
    fixed_symbol(257); huffman(0, 5u); fixed_symbol(256); reject();
    begin(); code_header(0, 0, 0, 0); reject(); /* Empty code-length tree. */
    begin(); code_header(1, 1, 1, 1); reject(); /* Oversubscribed code tree. */
    begin(); code_header(2, 0, 0, 0); reject(); /* Incomplete code tree. */
    begin(); code_header(1, 0, 0, 1); huffman(1, 1u); put(0, 2u); reject(); /* Repeat16 before any width. */
    begin(); code_header(0, 0, 1, 1);
    huffman(1, 1u); put(127, 7u); huffman(1, 1u); put(127, 7u); reject(); /* Width-run overflow. */
    begin(); put(1, 1u); put(2, 2u); put(31, 5u); put(0, 5u); put(0, 4u); reject(); /* HLIT exceeds286. */
    begin(); memset(source.data, 0, 4097); source.size = 4097; reject();
    source.calls = 0;
    const struct kui_zso_cursor reversed = {view_source, NULL, NULL, NULL, 1, 0, 0},
        empty = {view_source, NULL, NULL, NULL, 1, 1, 0}, one = {view_source, NULL, NULL, NULL, 0, 1, 0};
    work.data.in = reversed;
    assert(kui_deflate_decode(&work.data, output.data, &consumed) == KUI_ZSO_INVALID);
    assert(work.data.in.view == NULL && work.data.in.context == NULL);
    work.data.in = empty;
    assert(kui_deflate_decode(&work.data, output.data, &consumed) == KUI_ZSO_INVALID);
    assert(work.data.in.view == NULL && work.data.in.context == NULL);
    work.data.in = one;
    assert(kui_deflate_decode(NULL, output.data, &consumed) == KUI_ZSO_INVALID);
    assert(kui_deflate_decode(&work.data, NULL, &consumed) == KUI_ZSO_INVALID);
    assert(kui_deflate_decode(&work.data, output.data, NULL) == KUI_ZSO_INVALID);
    assert(!source.calls);
    puts("PASS DEFLATE bounds: every stored truncation, padding/UINT32 boundary, reserved symbols, invalid trees, runs, lookbehind and output overflow");
}
static void mutations(void) {
    compress_sample(4, Z_HUFFMAN_ONLY, 6);
    assert(((source.data[0] >> 1) & 3u) == 2u);
    unsigned bytes = source.size; memcpy(backup, source.data, bytes);
    for(unsigned sample = 0; sample < 30000; ++sample) {
        source.size = bytes; memcpy(source.data, backup, bytes);
        unsigned flips = random_word() % 4u + 1u;
        while(flips--) source.data[random_word() % bytes] = (uint8_t)random_word();
        if(sample & 1u) source.size = random_word() % (bytes + 1u);
        uint32_t consumed = 0xdeadbeefu;
        source.chunk = sample % 3u == 0 ? 0u : sample % 3u == 1 ? 1u : 5u;
        enum kui_zso_result result = decode(source.size, &consumed);
        source.chunk = 0;
        if(result == KUI_ZSO_OK) {
            assert(consumed <= source.size); oracle_check(source.data, consumed, output.data);
        } else assert(consumed == 0xdeadbeefu);
    }
    puts("PASS DEFLATE hostile streams: 30000 deterministic mutations/truncations, output/workspace canaries and zlib acceptance oracle");
}
/* Canonical codes from widths (RFC 1951 3.2.2), for hand-built trees. */
static void canonical_codes(const uint8_t *widths, unsigned count, uint16_t *codes) {
    unsigned counts[16] = {0}, next[16] = {0}, code = 0;
    for(unsigned i = 0; i < count; ++i) if(widths[i]) ++counts[widths[i]];
    for(unsigned bits = 1; bits < 16u; ++bits) {code = (code + counts[bits - 1u]) << 1; next[bits] = code;}
    for(unsigned i = 0; i < count; ++i) if(widths[i]) codes[i] = (uint16_t)next[widths[i]]++;
}
/* A final dynamic block whose code-length code gives symbols 0..15 four bits
 * each (complete), so every width is sent literally; data are literals only. */
static void dynamic_literals(const uint8_t *literal_widths, unsigned literals,
    const uint8_t *distance_widths, unsigned distances, const uint8_t *data, unsigned size) {
    static const unsigned order[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    uint16_t codes[288];
    put(1, 1u); put(2, 2u); put(literals - 257u, 5u); put(distances - 1u, 5u); put(15, 4u);
    for(unsigned i = 0; i < 19u; ++i) put(order[i] <= 15u ? 4u : 0u, 3u);
    for(unsigned i = 0; i < literals; ++i) huffman(literal_widths[i], 4u);
    for(unsigned i = 0; i < distances; ++i) huffman(distance_widths[i], 4u);
    canonical_codes(literal_widths, literals, codes);
    for(unsigned i = 0; i < size; ++i) huffman(codes[data[i]], literal_widths[data[i]]);
    huffman(codes[256], literal_widths[256]);
}
static void extreme_trees(void) {
    uint8_t literal_widths[257] = {0}, distance_widths[1] = {0};
    /* All 256 literals at nine bits fill every rank bucket; EOB has one bit. */
    for(unsigned i = 0; i < 256u; ++i) literal_widths[i] = 9;
    literal_widths[256] = 1;
    begin();
    for(unsigned i = 0; i < 2048u; ++i) expected[i] = (uint8_t)(i * 7u + (i >> 8));
    dynamic_literals(literal_widths, 257u, distance_widths, 1u, expected, 2048u);
    valid(false);
    /* Widths 1..14 on 'A'..'N', then 'O' and EOB at the 15-bit maximum. */
    memset(literal_widths, 0, sizeof(literal_widths));
    for(unsigned i = 0; i < 14u; ++i) literal_widths['A' + i] = (uint8_t)(i + 1u);
    literal_widths['O'] = 15; literal_widths[256] = 15;
    begin();
    for(unsigned i = 0; i < 2048u; ++i) expected[i] = (uint8_t)('A' + (random_word() % 15u));
    dynamic_literals(literal_widths, 257u, distance_widths, 1u, expected, 2048u);
    assert(source.size <= KUI_ZSO_MAX_SPAN);
    valid(false);
    puts("PASS DEFLATE extreme trees: 256 nine-bit literals across all rank buckets and 1..15-bit codes against zlib");
}
int main(void) {
    token_forms(); compressed_oracles(); malformed(); mutations(); extreme_trees();
    puts("All DEFLATE tests passed"); return 0;
}
