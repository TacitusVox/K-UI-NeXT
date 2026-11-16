/* SPDX-License-Identifier: GPL-3.0-only
 * Independent implementation of the SHA-256 algorithm in FIPS 180-4.
 * https://doi.org/10.6028/NIST.FIPS.180-4
 */
#include "kui/hash.h"
#include <string.h>
static uint32_t rotate(uint32_t n, unsigned r) { return n >> r | n << (32-r); }
static void compress(struct kui_sha256 *s, const uint8_t *b) {
    static const uint32_t k[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    uint32_t w[64];
    for(unsigned i=0;i<16;i++) w[i]=(uint32_t)b[4*i]<<24 | (uint32_t)b[4*i+1]<<16 |
        (uint32_t)b[4*i+2]<<8 | b[4*i+3];
    for(unsigned i=16;i<64;i++) {
        uint32_t x=w[i-15], y=w[i-2];
        w[i]=w[i-16]+(rotate(x,7)^rotate(x,18)^(x>>3))+w[i-7]+(rotate(y,17)^rotate(y,19)^(y>>10));
    }
    uint32_t a=s->h[0],c=s->h[2],d=s->h[3],e=s->h[4],f=s->h[5],g=s->h[6],h=s->h[7],bb=s->h[1];
    for(unsigned i=0;i<64;i++) {
        uint32_t t=h+(rotate(e,6)^rotate(e,11)^rotate(e,25))+((e&f)^(~e&g))+k[i]+w[i];
        uint32_t u=(rotate(a,2)^rotate(a,13)^rotate(a,22))+((a&bb)^(a&c)^(bb&c));
        h=g; g=f; f=e; e=d+t; d=c; c=bb; bb=a; a=t+u;
    }
    s->h[0]+=a;s->h[1]+=bb;s->h[2]+=c;s->h[3]+=d;s->h[4]+=e;s->h[5]+=f;s->h[6]+=g;s->h[7]+=h;
}
void kui_sha256_init(struct kui_sha256 *s) {
    *s=(struct kui_sha256){.h={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}};
}
void kui_sha256_update(struct kui_sha256 *s, const void *data, size_t size) {
    const uint8_t *p=data;
    while(size) {
        size_t used=(size_t)(s->bytes%64), n=64-used;
        if(n>size) n=size;
        memcpy(s->block+used,p,n); s->bytes+=n; p+=n; size-=n;
        if(s->bytes%64==0) compress(s,s->block);
    }
}
void kui_sha256_digest(const struct kui_sha256 *original, uint8_t digest[32]) {
    struct kui_sha256 s=*original;
    uint64_t bits=s.bytes*8;
    uint8_t padding[128]={0x80};
    size_t count=64-(size_t)(s.bytes%64);
    if(count<9) count+=64;
    for(unsigned i=0;i<8;i++) padding[count-1-i]=(uint8_t)(bits>>(8*i));
    kui_sha256_update(&s,padding,count);
    for(unsigned i=0;i<32;i++) digest[i]=(uint8_t)(s.h[i/4]>>(24-8*(i%4)));
}
void kui_hex(const uint8_t *bytes, size_t size, char *out) {
    static const char digits[]="0123456789abcdef";
    for(size_t i=0;i<size;i++) { out[2*i]=digits[bytes[i]>>4];out[2*i+1]=digits[bytes[i]&15]; }
    out[2*size]=0;
}
uint32_t kui_crc32_capture(uint32_t previous,const void *data,size_t size) {
    static uint32_t table[4][256];static bool ready;
    if(!ready) {
        for(unsigned i=0;i<256u;++i) {
            uint32_t value=i;
            for(unsigned bit=0;bit<8u;++bit)
                value=(value>>1)^(UINT32_C(0xedb88320)&(0u-(value&1u)));
            table[0][i]=value;
        }
        for(unsigned part=1;part<4u;++part)
            for(unsigned i=0;i<256u;++i) {
                uint32_t value=table[part-1u][i];
                table[part][i]=(value>>8)^table[0][value&255u];
            }
        ready=true;
    }
    const uint8_t *bytes=data;uint32_t crc=~previous;
    /* Read individual bytes: the SH-4 cannot dereference an unaligned word.
     * Explicit little-endian assembly also keeps the result host-independent.
     * Four independent table lookups reduce the per-byte dependency chain. */
    while(size>=4u) {
        crc^=(uint32_t)bytes[0]|(uint32_t)bytes[1]<<8|
            (uint32_t)bytes[2]<<16|(uint32_t)bytes[3]<<24;
        crc=table[3][crc&255u]^table[2][(crc>>8)&255u]^
            table[1][(crc>>16)&255u]^table[0][crc>>24];
        bytes+=4u;size-=4u;
    }
    for(size_t i=0;i<size;++i)crc=(crc>>8)^table[0][(crc^bytes[i])&255u];
    return ~crc;
}
uint32_t kui_cd_edc(const uint8_t *data, size_t size) {
    /* CD EDC reflected polynomial, zero initialization, no final complement. */
#if KUI_CD_EDC_CACHED_PAIR
    /* The read-ahead linker places these writable objects in P2. Give the
     * table its own cache lines before using its read-only P1 alias. */
    _Alignas(32) static uint32_t table[2][256];
    static bool ready;
    const uint32_t (*lookup)[256];
#if defined(__sh__) && !defined(KUI_NATIVE_READAHEAD_AUX_HOST_TEST)
    const uintptr_t cached=(uintptr_t)table&~(uintptr_t)UINT32_C(0x20000000);
    lookup=(const uint32_t (*)[256])cached;
#else
    lookup=(const uint32_t (*)[256])table;
#endif
    if(!ready) {
        for(unsigned i=0;i<256;i++) {
            uint32_t x=i;
            for(unsigned b=0;b<8;b++) x=(x>>1)^(UINT32_C(0xd8018001)&(0u-(x&1u)));
            table[0][i]=x;
        }
        for(unsigned i=0;i<256;i++) {
            const uint32_t x=table[0][i];
            table[1][i]=(x>>8)^table[0][x&255u];
        }
#if defined(__sh__) && !defined(KUI_NATIVE_READAHEAD_AUX_HOST_TEST)
        for(uintptr_t line=cached;line<cached+sizeof(table);line+=32u)
            __asm__ volatile("ocbi @%0"::"r"(line):"memory");
#endif
        ready=true; /* The sole I/O worker owns this routine. */
    }
    uint32_t crc=0;
#if defined(__GNUC__) && defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    /* Consume any leading unaligned byte before using halfword loads. */
    typedef uint16_t pair_word __attribute__((__may_alias__));
    if(size && ((uintptr_t)data&1u)) {
        crc=lookup[0][*data++];
        --size;
    }
#endif
    for(;size>=2u;size-=2u,data+=2) {
#if defined(__GNUC__) && defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        const uint32_t input=crc^*(const pair_word *)(const void *)data;
#else
        const uint32_t input=crc^((uint32_t)data[0]|(uint32_t)data[1]<<8);
#endif
        crc=(crc>>16)^lookup[1][input&255u]^lookup[0][(input>>8)&255u];
    }
    if(size)crc=(crc>>8)^lookup[0][(crc^data[0])&255u];
    return crc;
#else
    static uint32_t table[256];
    static bool ready;
    if(!ready) {
        for(unsigned i=0;i<256;i++) {
            uint32_t x=i;
            for(unsigned b=0;b<8;b++) x=(x>>1)^(UINT32_C(0xd8018001)&(0u-(x&1u)));
            table[i]=x;
        }
        ready=true; /* The sole I/O worker owns this routine. */
    }
    uint32_t crc=0;
    for(size_t i=0;i<size;i++) crc=(crc>>8)^table[(crc^data[i])&255];
    return crc;
#endif
}
bool kui_sector_edc_valid(const uint8_t raw[KUI_RAW_BYTES]) {
    int offset=kui_data_offset(raw);
    if(offset<0) return false;
    unsigned end=(unsigned)offset+KUI_DATA_BYTES;
    uint32_t stored=(uint32_t)raw[end]|(uint32_t)raw[end+1]<<8|
        (uint32_t)raw[end+2]<<16|(uint32_t)raw[end+3]<<24;
    unsigned begin=offset==16?0:16;
    return stored==kui_cd_edc(raw+begin,end-begin);
}
static bool mode1_fad_valid(const uint8_t *raw,uint32_t fad) {
    /* GD-ROM extends the minute byte beyond 99 (for example 122 -> 0xc2).
     * Refuse an expected address that would wrap this representation. */
    if(fad<150u || fad>719999u)return false;
    const unsigned fields[3]={fad/4500u,(fad/75u)%60u,fad%75u};
    for(unsigned i=0;i<3u;++i)
        if(raw[12u+i]!=(uint8_t)((fields[i]/10u)*16u+fields[i]%10u))return false;
    return true;
}
bool kui_sector_edc_valid_at(const uint8_t raw[KUI_RAW_BYTES],uint32_t fad) {
    return kui_sector_edc_valid(raw) && (raw[15]!=1u || mode1_fad_valid(raw,fad));
}

#if KUI_TOY_SUITE_MODE == 52
void kui_cd_edc_table_init(struct kui_cd_edc_table *table) {
    for(unsigned i=0;i<256u;++i) {
        uint32_t value=i;
        for(unsigned bit=0;bit<8u;++bit)
            value=(value>>1)^(UINT32_C(0xd8018001)&(0u-(value&1u)));
        table->entry[0][i]=value;
    }
    for(unsigned part=1;part<4u;++part)
        for(unsigned i=0;i<256u;++i) {
            const uint32_t value=table->entry[part-1u][i];
            table->entry[part][i]=(value>>8)^table->entry[0][value&255u];
        }
}

bool kui_mode1_edc_table_valid_at(const uint8_t raw[KUI_RAW_BYTES],uint32_t fad,
    const struct kui_cd_edc_table *table) {
    enum { EDC_OFFSET=16u+KUI_DATA_BYTES };
    if(((uintptr_t)raw&3u) || raw[15]!=1u || kui_data_offset(raw)!=16)return false;
    const uint32_t *table0=table->entry[0],*table1=table->entry[1];
    const uint32_t *table2=table->entry[2],*table3=table->entry[3];
#ifdef __sh__
    /* SH-4 indexed loads can use each table base directly. Keep GCC
     * from rebuilding the row offsets on every lookup. */
    __asm__("" : "+r"(table0),"+r"(table1),"+r"(table2),"+r"(table3));
#endif
    uint32_t edc=0;
#if defined(__GNUC__) && defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    typedef uint32_t raw_word __attribute__((__may_alias__));
#endif
    for(unsigned i=0;i<EDC_OFFSET;i+=4u) {
        /* A staged Mode 1 sector and its EDC are both word aligned. */
#if defined(__GNUC__) && defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        const uint32_t word=*(const raw_word *)(const void *)(raw+i);
#else
        const uint32_t word=(uint32_t)raw[i]|(uint32_t)raw[i+1u]<<8|
            (uint32_t)raw[i+2u]<<16|(uint32_t)raw[i+3u]<<24;
#endif
        const uint32_t value=edc^word;
        edc=table3[value&255u]^table2[(value>>8)&255u]^table1[(value>>16)&255u]^table0[value>>24];
    }
#if defined(__GNUC__) && defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    const uint32_t stored=*(const raw_word *)(const void *)(raw+EDC_OFFSET);
#else
    const uint32_t stored=(uint32_t)raw[EDC_OFFSET]|(uint32_t)raw[EDC_OFFSET+1u]<<8|
        (uint32_t)raw[EDC_OFFSET+2u]<<16|(uint32_t)raw[EDC_OFFSET+3u]<<24;
#endif
    return edc==stored && mode1_fad_valid(raw,fad);
}
#endif
