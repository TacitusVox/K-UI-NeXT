/* SPDX-License-Identifier: GPL-3.0-only */
/* Prefix digests from one SHA-256 pass equal fresh digests of each prefix. */
#include "kui/sha256_prefix.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void fresh(const uint8_t *data,size_t n,uint8_t out[32]) {
    struct kui_sha256 h;kui_sha256_init(&h);kui_sha256_update(&h,data,n);kui_sha256_digest(&h,out);
}
int main(void) {
    static uint8_t data[5u*KUI_SHA256_PREFIX_STRIDE+777u];
    uint32_t x=0x12345678u;
    for(size_t i=0;i<sizeof(data);i++) {x=x*1664525u+1013904223u;data[i]=(uint8_t)(x>>24);}
    /* NIST "abc" through the prefix path. */
    {
        struct kui_sha256_prefix p={0};uint8_t whole[32],part[32];
        kui_sha256_prefix_hash(&p,"abc",3u,whole);
        static const uint8_t abc[32]={0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
            0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
        assert(!memcmp(whole,abc,32));
        assert(kui_sha256_prefix_digest(&p,"abc",3u,part,NULL) && !memcmp(part,abc,32));
        kui_sha256_prefix_free(&p);
    }
    const size_t sizes[]={0u,1u,63u,64u,65u,KUI_SHA256_PREFIX_STRIDE-1u,KUI_SHA256_PREFIX_STRIDE,
        KUI_SHA256_PREFIX_STRIDE+1u,3u*KUI_SHA256_PREFIX_STRIDE,sizeof(data)};
    for(size_t s=0;s<sizeof(sizes)/sizeof(sizes[0]);s++) {
        size_t total=sizes[s];
        struct kui_sha256_prefix p={0};uint8_t whole[32],expect[32],got[32];
        kui_sha256_prefix_hash(&p,data,total,whole);fresh(data,total,expect);
        assert(!memcmp(whole,expect,32));
        const size_t prefixes[]={0u,1u,63u,64u,4095u,KUI_SHA256_PREFIX_STRIDE-1u,KUI_SHA256_PREFIX_STRIDE,
            KUI_SHA256_PREFIX_STRIDE+5u,2u*KUI_SHA256_PREFIX_STRIDE+12345u,total>0?total-1u:0u,total};
        for(size_t q=0;q<sizeof(prefixes)/sizeof(prefixes[0]);q++) {
            size_t prefix=prefixes[q],rehashed=~(size_t)0;
            if(prefix>total) {assert(!kui_sha256_prefix_digest(&p,data,prefix,got,&rehashed) && !rehashed);continue;}
            assert(kui_sha256_prefix_digest(&p,data,prefix,got,&rehashed));
            fresh(data,prefix,expect);assert(!memcmp(got,expect,32));
            assert(rehashed<=KUI_SHA256_PREFIX_STRIDE && rehashed==prefix%KUI_SHA256_PREFIX_STRIDE+
                (prefix && prefix%KUI_SHA256_PREFIX_STRIDE==0 && prefix/KUI_SHA256_PREFIX_STRIDE>=p.count?KUI_SHA256_PREFIX_STRIDE:0u));
        }
        /* A checkpoint from this allocation cannot attest another pointer. */
        uint8_t other[1]={0},unchanged[32];memset(unchanged,0x5a,sizeof(unchanged));
        memcpy(got,unchanged,sizeof(got));
        size_t rejected=1u;
        assert(!kui_sha256_prefix_digest(&p,other,0u,got,&rejected) && rejected==0u);
        assert(!memcmp(got,unchanged,sizeof(got)));
        /* Without marks every prefix still hashes correctly, from the start. */
        struct kui_sha256 *saved=p.marks;size_t count=p.count;p.marks=NULL;p.count=0;
        size_t rehashed;assert(kui_sha256_prefix_digest(&p,data,total,got,&rehashed));
        fresh(data,total,expect);assert(!memcmp(got,expect,32) && rehashed==total);
        p.marks=saved;p.count=count;kui_sha256_prefix_free(&p);
        assert(!p.marks && !p.count && !p.bytes);
    }
    assert(!kui_sha256_prefix_digest(NULL,data,0u,(uint8_t[32]){0},NULL));
    puts("PASS SHA-256 prefix marks: one pass, exact fresh digests for every prefix, stride bounds, no-marks fallback, NIST abc");
    return 0;
}
