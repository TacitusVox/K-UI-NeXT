/* SPDX-License-Identifier: GPL-3.0-only */
/* The CE stream reader's per-block check, written for its hot loop
 * (KUI_CE_SCI_FAST_TAKE): each 32-bit word of a received block is
 * bit-reversed in place when it came from the wire (SCI is LSB first) and
 * the block's CRC16 (0x1021, zero start) is computed over its 512 bytes.
 * Same bytes written and same CRC as the reader's general loop.
 *
 * The table is folded on bit 0: table[x ^ 1] = table[x] XOR 0x1021, so the
 * 128 even entries (256 bytes, as the bit-7 fold it replaces) reconstruct
 * every byte update. On SH-4 one r0 value then serves the offset mask and
 * the indexed load, so the step needs no stack spill. The state is left
 * unmasked between bytes (only bits 8..15 index the table and the low 16
 * bits stay exact) and masked once at the end; entries load sign-extended,
 * touching only those ignored high bits. Included by sci_stream.c and by
 * the SH-4 emulator test (tests/sh4/test_check_words.c). */
#ifndef KUI_SCI_STREAM_CHECK_H
#define KUI_SCI_STREAM_CHECK_H
#include <stdbool.h>
#include <stdint.h>

static const uint16_t kui_sci_check_table[128] __attribute__((aligned(32))) = {
    0x0000u, 0x2042u, 0x4084u, 0x60c6u, 0x8108u, 0xa14au, 0xc18cu, 0xe1ceu,
    0x1231u, 0x3273u, 0x52b5u, 0x72f7u, 0x9339u, 0xb37bu, 0xd3bdu, 0xf3ffu,
    0x2462u, 0x0420u, 0x64e6u, 0x44a4u, 0xa56au, 0x8528u, 0xe5eeu, 0xc5acu,
    0x3653u, 0x1611u, 0x76d7u, 0x5695u, 0xb75bu, 0x9719u, 0xf7dfu, 0xd79du,
    0x48c4u, 0x6886u, 0x0840u, 0x2802u, 0xc9ccu, 0xe98eu, 0x8948u, 0xa90au,
    0x5af5u, 0x7ab7u, 0x1a71u, 0x3a33u, 0xdbfdu, 0xfbbfu, 0x9b79u, 0xbb3bu,
    0x6ca6u, 0x4ce4u, 0x2c22u, 0x0c60u, 0xedaeu, 0xcdecu, 0xad2au, 0x8d68u,
    0x7e97u, 0x5ed5u, 0x3e13u, 0x1e51u, 0xff9fu, 0xdfddu, 0xbf1bu, 0x9f59u,
    0x9188u, 0xb1cau, 0xd10cu, 0xf14eu, 0x1080u, 0x30c2u, 0x5004u, 0x7046u,
    0x83b9u, 0xa3fbu, 0xc33du, 0xe37fu, 0x02b1u, 0x22f3u, 0x4235u, 0x6277u,
    0xb5eau, 0x95a8u, 0xf56eu, 0xd52cu, 0x34e2u, 0x14a0u, 0x7466u, 0x5424u,
    0xa7dbu, 0x8799u, 0xe75fu, 0xc71du, 0x26d3u, 0x0691u, 0x6657u, 0x4615u,
    0xd94cu, 0xf90eu, 0x99c8u, 0xb98au, 0x5844u, 0x7806u, 0x18c0u, 0x3882u,
    0xcb7du, 0xeb3fu, 0x8bf9u, 0xabbbu, 0x4a75u, 0x6a37u, 0x0af1u, 0x2ab3u,
    0xfd2eu, 0xdd6cu, 0xbdaau, 0x9de8u, 0x7c26u, 0x5c64u, 0x3ca2u, 0x1ce0u,
    0xef1fu, 0xcf5du, 0xaf9bu, 0x8fd9u, 0x6e17u, 0x4e55u, 0x2e93u, 0x0ed1u,
};
typedef uint32_t kui_sci_check_word __attribute__((__may_alias__));

/* One byte's CRC update; the byte is word's low 8 bits. */
static inline __attribute__((__always_inline__)) uint32_t kui_sci_check_step(
    uint32_t crc, uint32_t word, const uint16_t *table, uint32_t poly) {
#if defined(__sh__) && !defined(KUI_SCI_CHECK_PORTABLE)
    uint32_t index, value;
    __asm__("mov   %[crc],%[index]\n\t"
            "shlr8 %[index]\n\t"
            "xor   %[word],%[index]\n\t"     /* index = crc >> 8 ^ word */
            "shll8 %[crc]\n\t"
            "mov   %[index],r0\n\t"
            "and   #254,r0\n\t"               /* byte offset: bit 0 clear */
            "mov.w @(r0,%[table]),%[value]\n\t"
            "shlr  %[index]\n\t"              /* T = index bit 0 */
            "subc  %[index],%[index]\n\t"     /* 0 or all ones */
            "and   %[poly],%[index]\n\t"
            "xor   %[value],%[crc]\n\t"
            "xor   %[index],%[crc]"
            : [crc] "+r"(crc), [index] "=&r"(index), [value] "=&r"(value)
            : [word] "r"(word), [table] "r"(table), [poly] "r"(poly)
            : "r0", "t");
    return crc;
#else
    uint32_t index = (crc >> 8) ^ word;
    uint32_t value = (uint32_t)(int32_t)*(const int16_t *)(const void *)
        ((const uint8_t *)table + (index & 0xfeu));
    if(index & 1u) value ^= poly;
    return (crc << 8) ^ value;
#endif
}
/* Each byte's bits in reverse order, bytes in place. */
static inline __attribute__((__always_inline__)) uint32_t kui_sci_check_reverse(uint32_t x) {
    uint32_t t = (x ^ (x >> 1)) & UINT32_C(0x55555555);
    x ^= t ^ (t << 1);
    t = (x ^ (x >> 2)) & UINT32_C(0x33333333);
    x ^= t ^ (t << 2);
    t = (x ^ (x >> 4)) & UINT32_C(0x0f0f0f0f);
    return x ^ t ^ (t << 4);
}
/* The 128 words of a received block: reversed in place if wire, and their
 * bytes' CRC16 in memory order (little-endian words). */
static inline uint32_t kui_sci_check_words(kui_sci_check_word *words, bool wire) {
    const uint16_t *table = kui_sci_check_table;
    const uint32_t poly = 0x1021u;
    uint32_t crc = 0;
    for(unsigned w = 0; w < 128u; ++w) {
        uint32_t word = words[w];
        if(wire) words[w] = word = kui_sci_check_reverse(word);
        crc = kui_sci_check_step(crc, word, table, poly);
        crc = kui_sci_check_step(crc, word >> 8, table, poly);
        crc = kui_sci_check_step(crc, word >> 16, table, poly);
        crc = kui_sci_check_step(crc, word >> 24, table, poly);
    }
    return crc & 0xffffu;
}
#endif
