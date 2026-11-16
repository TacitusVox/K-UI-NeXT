/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/toy_sd_crc16.h"

/* The XMODEM/SD CRC (0x1021, initial zero) is linear in the index.
 * Enumerate XOR combinations of its eight single-bit responses in
 * ascending byte order. Each value is byte swapped for SH-4 LE loads. */
#define CRC2(x,a) (int16_t)(x),(int16_t)((x)^(a))
#define CRC4(x,a,b) CRC2(x,a),CRC2((x)^(b),a)
#define CRC8(x,a,b,c) CRC4(x,a,b),CRC4((x)^(c),a,b)
#define CRC16(x,a,b,c,d) CRC8(x,a,b,c),CRC8((x)^(d),a,b,c)
#define CRC32(x,a,b,c,d,e) CRC16(x,a,b,c,d),CRC16((x)^(e),a,b,c,d)
#define CRC64(x,a,b,c,d,e,f) CRC32(x,a,b,c,d,e),CRC32((x)^(f),a,b,c,d,e)
#define CRC128(x,a,b,c,d,e,f,g) CRC64(x,a,b,c,d,e,f),CRC64((x)^(g),a,b,c,d,e,f)
#define CRC256(x,a,b,c,d,e,f,g,h) CRC128(x,a,b,c,d,e,f,g),CRC128((x)^(h),a,b,c,d,e,f,g)

_Alignas(32) static const int16_t table[2][256] = {
    /* First input byte followed by one zero byte. */
    { CRC256(0,0x3133u,0x6266u,0xc4ccu,0xa989u,0x7303u,0xe606u,0xcc0du,0x981bu) },
    /* Second input byte. */
    { CRC256(0,0x2110u,0x4220u,0x8440u,0x0881u,0x3112u,0x6224u,0xc448u,0x8891u) }
};
_Static_assert(sizeof(table)==KUI_TOY_SD_CRC16_TABLE_BYTES,"Toy SCI CRC16 table layout");

static inline uint32_t reverse_bytes(uint32_t word) {
    uint32_t t=(word^(word>>1))&UINT32_C(0x55555555);
    word^=t^(t<<1);
    t=(word^(word>>2))&UINT32_C(0x33333333);
    word^=t^(t<<2);
    t=(word^(word>>4))&UINT32_C(0x0f0f0f0f);
    return word^t^(t<<4);
}

/* Keep the CRC state byte swapped, so it XORs directly with the two
 * little-endian input bytes. Signed table loads only affect bits above 16. */
static inline uint32_t crc_pair(uint32_t swapped,uint32_t word,
    const int16_t *first,const int16_t *second) {
    const uint32_t index=swapped^word;
    return (uint32_t)(first[index&255u]^second[(index>>8)&255u]);
}

bool kui_toy_sd_crc16_check(uint8_t *area,uint32_t tail,bool wire,uint32_t lost) {
    typedef uint32_t alias_word __attribute__((__may_alias__));
    alias_word *words=(void *)area;
    const int16_t *first=table[0],*second=table[1];
#ifdef __sh__
    /* Keep both row bases resident for SH-4 indexed loads. */
    __asm__("" : "+r"(first),"+r"(second));
#endif
    uint32_t crc=0u;
    for(unsigned w=0;w<128u;++w) {
        uint32_t word=words[w];
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        if(wire)words[w]=word=reverse_bytes(word);
        word=__builtin_bswap32(word);
#else
        if(wire)words[w]=word=reverse_bytes(word);
#endif
        crc=crc_pair(crc,word,first,second);
        crc=crc_pair(crc,word>>16,first,second);
    }
    uint32_t sent=((uint32_t)area[512]<<8)|tail;
    if(wire)sent=reverse_bytes(sent);
    crc=((crc>>8)&255u)|((crc&255u)<<8);
    if(!lost)return crc==sent;
    /* The same single-byte reconstruction as the resident's verified
     * RAW path. The staged sector EDC still has the final word. */
    if(crc==sent)return true;
    if((sent&254u)==254u || lost>512u)return false;
    uint32_t syndrome=crc^sent;
    for(uint32_t n=(512u-lost)*2u;n;--n)
        syndrome=(syndrome>>4)^((syndrome&15u)*0x1102u);
    const uint32_t t=(syndrome^(syndrome<<5))&255u;
    const uint32_t value=t^(t>>4);
    const uint32_t expected=(uint16_t)second[value];
    if((((expected>>8)&255u)|((expected&255u)<<8))!=syndrome)return false;
    area[lost-1u]=(uint8_t)value;
    return true;
}
