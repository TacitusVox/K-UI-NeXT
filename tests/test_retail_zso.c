/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/retail_zso.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define LOGICAL_BLOCKS 128u
#define FILE_CAP (LOGICAL_BLOCKS*2048u+4096u)
#define CARD_BLOCKS 8192u
static uint8_t file[FILE_CAP],expected[LOGICAL_BLOCKS][2048],card[CARD_BLOCKS][512];
static struct kui_retail_zso_manifest map,decoded;
static uint32_t file_bytes;
/* read_run also models kui_retail_sd_read_run's stream rule: continue only at
 * next_lba while the hint covers what remains, otherwise CMD12 and a new
 * CMD18 for at most64 blocks. starts counts CMD18s; stops counts early CMD12s. */
struct storage {unsigned calls,fail_on,max_hint;bool run;
    bool active;uint32_t next_lba,remaining,starts,stops,limit_lba,start_lba,start_count;};
static struct storage storage;
static void put(uint8_t *p,uint32_t n) {for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static uint32_t get(const uint8_t *p) {return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void wide(uint8_t *p,uint64_t n) {put(p,(uint32_t)n);put(p+4,(uint32_t)(n>>32));}
static uint32_t crc(const uint8_t *p,size_t bytes) {
    uint32_t c=UINT32_MAX;
    for(size_t i=0;i<bytes;i++){c^=p[i];for(unsigned b=0;b<8;b++)c=(c>>1)^((c&1)?UINT32_C(0xedb88320):0);}
    return ~c;
}
static void reseal(uint8_t wire[4096]) {put(wire+16,0);put(wire+16,crc(wire,4096));}
static size_t extension(uint8_t *out,unsigned n) {
#if KUI_TEST_LZO
    size_t used=0;while(n>255){out[used++]=0;n-=255;}out[used++]=(uint8_t)n;return used;
#else
    size_t used=0;while(n>=255){out[used++]=255;n-=255;}out[used++]=(uint8_t)n;return used;
#endif
}
/* A dense block is hundreds of one-byte takes spanning at least four
 * physical blocks: four literals, then distance-4 copies each joined by one
 * literal. Plain RLE blocks stay inside one or two physical blocks. */
static size_t dense(uint8_t *out,uint8_t *expect,unsigned block) {
    size_t at=0;unsigned done=0;
#if KUI_TEST_LZO
    out[at++]=17u+4u;
    while(done<4u){uint8_t v=(uint8_t)(block*7u+done*29u+3u);out[at++]=v;expect[done++]=v;}
    while(done<2048u) { /* M2: three bytes from distance4, then one literal. */
        out[at++]=77u;out[at++]=0;
        for(unsigned i=0;i<3u;i++,done++)expect[done]=expect[done-4u];
        uint8_t v=(uint8_t)(block*5u+done*11u+1u);out[at++]=v;expect[done++]=v;
    }
    out[at++]=17u;out[at++]=0;out[at++]=0;
#else
    out[at++]=0x40;
    while(done<4u){uint8_t v=(uint8_t)(block*7u+done*29u+3u);out[at++]=v;expect[done++]=v;}
    out[at++]=4;out[at++]=0;for(unsigned i=0;i<4u;i++,done++)expect[done]=expect[done-4u];
    while(done<2048u-15u) { /* One literal and a four-byte copy from distance4. */
        uint8_t v=(uint8_t)(block*5u+done*11u+1u);out[at++]=0x10;out[at++]=v;expect[done++]=v;
        out[at++]=4;out[at++]=0;for(unsigned i=0;i<4u;i++,done++)expect[done]=expect[done-4u];
    }
    out[at++]=0xf0;out[at++]=0;
    while(done<2048u){uint8_t v=(uint8_t)(block+done);out[at++]=v;expect[done++]=v;}
#endif
    assert(done==2048u);return at;
}
enum layout {BASIC,DENSE};
static void fixture_layout(uint32_t session,bool all_raw,unsigned requested_extents,enum layout layout,unsigned shift) {
    memset(&map,0,sizeof(map));memset(&storage,0,sizeof(storage));
    memset(card,0xe9,sizeof(card));memset(file,0,sizeof(file));
    memcpy(file,"ZISO",4);put(file+4,24);put(file+8,LOGICAL_BLOCKS*2048u);put(file+16,2048);file[20]=1;file[21]=(uint8_t)shift;
    const uint32_t mask=(1u<<shift)-1u;
    uint32_t at=(24u+(LOGICAL_BLOCKS+1u)*4u+mask)&~mask;
    for(unsigned block=0;block<LOGICAL_BLOCKS;block++) {
        bool raw=all_raw || block==0 || (layout==DENSE && block==64u);
        put(file+24u+block*4u,(at>>shift)|(raw?UINT32_C(0x80000000):0));
        if(layout==DENSE && !raw && block%4u && block!=LOGICAL_BLOCKS-1u) {
            at+=(uint32_t)dense(file+at,expected[block],block);
        } else if(raw || block==LOGICAL_BLOCKS-1u) {
            for(unsigned i=0;i<2048;i++)expected[block][i]=(uint8_t)(block*11u+i*37u+(i>>8)*19u);
            if(!raw){
#if KUI_TEST_LZO
                file[at++]=0;at+=(uint32_t)extension(file+at,2048-18);
#else
                file[at++]=0xf0;at+=(uint32_t)extension(file+at,2048-15);
#endif
            }
            memcpy(file+at,expected[block],2048);at+=2048;
#if KUI_TEST_LZO
            if(!raw){file[at++]=17;file[at++]=0;file[at++]=0;}
#endif
        } else {
            uint8_t value=(uint8_t)(block+17u);memset(expected[block],value,2048);
#if KUI_TEST_LZO
            file[at++]=18;file[at++]=value;file[at++]=32;
            at+=(uint32_t)extension(file+at,2048-1-33);
            file[at++]=0;file[at++]=0;file[at++]=17;file[at++]=0;file[at++]=0;
#else
            file[at++]=0x1f;file[at++]=value;file[at++]=1;file[at++]=0;
            at+=(uint32_t)extension(file+at,2048-1-5-19);
            file[at++]=0x50;memset(file+at,value,5);at+=5;
#endif
        }
        while(at&mask)file[at++]=0xad; /* Admitted nonzero alignment padding. */
    }
    put(file+24u+LOGICAL_BLOCKS*4u,at>>shift);file_bytes=at;
    map.zso=(struct kui_zso_info){.file_bytes=at,.total_bytes=LOGICAL_BLOCKS*2048u,
        .block_count=LOGICAL_BLOCKS,.index_bytes=(LOGICAL_BLOCKS+1u)*4u,.shift=(uint8_t)shift};
#if KUI_TEST_LZO
    map.zso.codec=KUI_ZSO_LZO1X;
#endif
    struct kui_retail_manifest *m=&map.manifest;
    m->track_count=1;m->storage_transport=KUI_STORAGE_SCI;m->reader=KUI_RETAIL_READER_STANDARD;
    m->card_sectors=CARD_BLOCKS;m->partition_start=64;m->partition_end=CARD_BLOCKS;
    m->session_lba=session;m->boot_lba=session+16;m->boot_bytes=3001;
    m->flags=KUI_RETAIL_IMAGE_BOOT_CRC|(session<45000?KUI_RETAIL_IMAGE_CD:0);
    strcpy(m->title,"Independent ZSO fixture");strcpy(m->product,"KUI-ZSO");strcpy(m->bootfile,"1ST_READ.BIN");strcpy(m->region,"JUE");
    m->gdi_crc32=crc(file,24);m->ip_crc32=crc(&expected[0][0],16u*2048u);m->boot_crc32=crc(&expected[16][0],3001);
    uint32_t blocks=(at+511u)/512u;unsigned extents=requested_extents;
    if(extents>blocks)extents=blocks;
    assert(extents && extents<=31);m->extent_count=extents;
    m->slots[0].track=(struct kui_retail_track){session,session+LOGICAL_BLOCKS,1,(uint8_t)extents,0x84};
    uint32_t next=0;
    for(unsigned i=1;i<=extents;i++) {
        struct kui_retail_extent *e=&m->slots[i].extent;
        *e=(struct kui_retail_extent){next,100u*i,i<extents?1u:blocks-next};
        assert(e->card_lba+e->blocks<=CARD_BLOCKS);
        for(uint32_t b=0;b<e->blocks;b++) {
            uint32_t offset=(next+b)*512u,bytes=offset<at?at-offset:0;if(bytes>512)bytes=512;
            memcpy(card[e->card_lba+b],file+offset,bytes);
        }
        next+=e->blocks;
    }
    assert(kui_retail_zso_manifest_validate(&map)==KUI_GAME_OK);
}
static void fixture(uint32_t session,bool all_raw,unsigned requested_extents) {
    fixture_layout(session,all_raw,requested_extents,BASIC,0);
}
static uint32_t physical(uint32_t file_offset) {
    uint32_t block=file_offset/512u;
    for(unsigned i=1;i<=map.manifest.extent_count;i++) {
        const struct kui_retail_extent *e=&map.manifest.slots[i].extent;
        if(block>=e->file_block && block-e->file_block<e->blocks)return e->card_lba+block-e->file_block;
    }
    assert(!"fixture file block missing");return 0;
}
static void change_file_byte(uint32_t at,uint8_t value) {card[physical(at)][at%512u]=value;}
static int read_card(void *context,uint32_t lba,uint8_t out[512]) {
    struct storage *s=context;assert(lba<CARD_BLOCKS);++s->calls;
    bool mapped=false;
    for(unsigned i=1;i<=map.manifest.extent_count;i++) {
        const struct kui_retail_extent *e=&map.manifest.slots[i].extent;
        if(lba>=e->card_lba && lba-e->card_lba<e->blocks)mapped=true;
    }
    assert(mapped);
    if(s->fail_on && s->calls==s->fail_on){memset(out,0xcf,512);return -1;}
    memcpy(out,card[lba],512);return 0;
}
static int read_run(void *context,uint32_t lba,uint32_t available,uint8_t out[512]) {
    struct storage *s=context;assert(available);if(available>s->max_hint)s->max_hint=available;
    bool bounded=false;
    for(unsigned i=1;i<=map.manifest.extent_count;i++) {
        const struct kui_retail_extent *e=&map.manifest.slots[i].extent;
        if(lba>=e->card_lba && lba-e->card_lba<e->blocks && available<=e->blocks-(lba-e->card_lba))bounded=true;
    }
    assert(bounded);
    /* Not permission to prefetch: no hint passes the selected request's span. */
    if(s->limit_lba)assert(lba+available-1u<=s->limit_lba);
    if(s->active && (s->next_lba!=lba || available<s->remaining)){s->active=false;++s->stops;}
    if(!s->active) {
        s->active=true;s->next_lba=s->start_lba=lba;s->remaining=s->start_count=available<64u?available:64u;++s->starts;
    }
    ++s->next_lba;if(!--s->remaining)s->active=false;
    int failed=read_card(context,lba,out);
    if(failed)s->active=false; /* A failed stream is finished by the transport. */
    return failed;
}
/* The resident and stage stop any remaining stream on every image return. */
static void stream_stop(struct storage *s) {if(s->active){s->active=false;++s->stops;}}
static void independent_wire(uint8_t wire[4096]) {
    const struct kui_retail_manifest *m=&map.manifest;
    memset(wire,0,4096);
#if KUI_TEST_LZO
    memcpy(wire,"KUIRLS01",8);
#else
    memcpy(wire,"KUIRZS01",8);
#endif
    put(wire+8,1);put(wire+12,4096);
    put(wire+20,1);put(wire+24,m->extent_count);put(wire+28,KUI_STORAGE_SCI);
    wide(wire+32,CARD_BLOCKS);wide(wire+40,64);wide(wire+48,CARD_BLOCKS);
    put(wire+56,m->session_lba);put(wire+60,m->boot_lba);put(wire+64,3001);put(wire+68,m->gdi_crc32);
    memcpy(wire+72,"Independent ZSO fixture",23);memcpy(wire+200,"KUI-ZSO",7);memcpy(wire+216,"1ST_READ.BIN",12);memcpy(wire+240,"JUE",3);
    put(wire+256,m->boot_crc32);put(wire+260,m->ip_crc32);put(wire+268,m->flags);
    put(wire+272,file_bytes);put(wire+276,LOGICAL_BLOCKS*2048u);put(wire+280,LOGICAL_BLOCKS);put(wire+284,(LOGICAL_BLOCKS+1u)*4u);
    put(wire+320,m->session_lba);put(wire+324,m->session_lba+LOGICAL_BLOCKS);wire[328]=0x84;wire[329]=(uint8_t)m->extent_count;
    for(unsigned i=1;i<=m->extent_count;i++) {
        const struct kui_retail_extent *e=&m->slots[i].extent;uint8_t *out=wire+320u+i*12u;
        put(out,e->file_block);put(out+4,e->card_lba);put(out+8,e->blocks);
    }
    reseal(wire);
}
static void wire_reject(const uint8_t wire[4096]) {
    memset(&decoded,0xa5,sizeof(decoded));assert(kui_retail_zso_manifest_decode(wire,&decoded)!=KUI_GAME_OK);
    const uint8_t *bytes=(const void *)&decoded;for(size_t i=0;i<sizeof(decoded);i++)assert(!bytes[i]);
}
static void wire_and_maps(void) {
    fixture(11700,false,31);uint8_t expected_wire[4096],wire[4096],bad[4096];independent_wire(expected_wire);
    assert(kui_retail_zso_manifest_encode(&map,wire)==KUI_GAME_OK && !memcmp(wire,expected_wire,4096));
    assert(kui_retail_zso_manifest_decode(wire,&decoded)==KUI_GAME_OK);
    assert(decoded.zso.file_bytes==file_bytes && decoded.manifest.session_lba==11700);
    assert(decoded.zso.codec==map.zso.codec);
    assert(!memcmp(&decoded.manifest,&map.manifest,sizeof(map.manifest)));
    assert(!memcmp(decoded.manifest.slots,map.manifest.slots,(1u+map.manifest.extent_count)*12u));
    const unsigned offsets[]={8,12,20,24,24,28,48,264,268,268,272,276,280,284,288,288,292,324,328,330};
    const uint32_t values[]={2,4095,2,0,32,KUI_STORAGE_SCIF,63,KUI_RETAIL_READER_ASYNC,0,8,0,2049,127,0,10,256,1,11701,0x84,1};
    for(unsigned i=0;i<sizeof(offsets)/sizeof(offsets[0]);i++) {
        memcpy(bad,wire,4096);put(bad+offsets[i],values[i]);reseal(bad);wire_reject(bad);
    }
    memcpy(bad,wire,4096);memcpy(bad,"KUIRTI03",8);reseal(bad);wire_reject(bad);
#if KUI_TEST_LZO
    memcpy(bad,wire,4096);memcpy(bad,"KUIRZS01",8);reseal(bad);wire_reject(bad);
#endif
    memcpy(bad,wire,4096);bad[320u+(1u+map.manifest.extent_count)*12u]=1;reseal(bad);wire_reject(bad);
    memcpy(bad,wire,4096);bad[80]^=1;wire_reject(bad);wire_reject(NULL);
    struct kui_retail_zso_manifest saved=map;
    for(unsigned fault=0;fault<12;fault++) {
        map=saved;struct kui_retail_manifest *m=&map.manifest;
        switch(fault) {
        case 0:m->extent_count=32;break;
        case 1:m->slots[2].extent.file_block++;break;
        case 2:m->slots[2].extent.card_lba=m->slots[1].extent.card_lba;break;
        case 3:m->slots[1].extent.card_lba=63;break;
        case 4:m->slots[1].extent.card_lba=CARD_BLOCKS;break;
        case 5:m->slots[m->extent_count].extent.blocks--;break;
        case 6:m->slots[m->extent_count].extent.blocks++;break;
        case 7:m->partition_start=m->partition_end;break;
        case 8:m->card_sectors=UINT64_C(0x100000001);break;
        case 9:m->boot_lba=m->slots[0].track.end_lba;break;
        case 10:m->slots[0].track.end_lba++;break;
        case 11:m->title[50]='x';break; /* noncanonical bytes after NUL */
        }
        assert(kui_retail_zso_manifest_validate(&map)!=KUI_GAME_OK);
        memset(bad,0x5a,4096);assert(kui_retail_zso_manifest_encode(&map,bad)!=KUI_GAME_OK);
        for(unsigned i=0;i<4096;i++)assert(bad[i]==0x5a);
    }
    fixture(45000,true,31);assert(map.manifest.extent_count==31);
    assert(kui_retail_zso_manifest_encode(&map,wire)==KUI_GAME_OK);
    fixture(0,false,31);map.manifest.card_sectors=map.manifest.partition_end=UINT64_C(0x100000000);
    struct kui_retail_extent *last=&map.manifest.slots[map.manifest.extent_count].extent;assert(last->blocks==1);
    last->card_lba=UINT32_MAX;assert(kui_retail_zso_manifest_validate(&map)==KUI_GAME_OK);
    assert(kui_retail_zso_manifest_encode(&map,wire)==KUI_GAME_OK);
    assert(kui_retail_zso_manifest_decode(wire,&decoded)==KUI_GAME_OK);
    assert(decoded.manifest.card_sectors==UINT64_C(0x100000000) && decoded.manifest.partition_end==UINT64_C(0x100000000));
    last->blocks=2;map.zso.file_bytes+=512;
    assert(kui_retail_zso_manifest_validate(&map)!=KUI_GAME_OK); /* last physical end must not wrap32bits */
    puts("PASS ZSO retail wire: independent bytes/CRC, disjoint ABI, canonical reserved bytes,31 extents and64-bit physical bounds");
}
static void readers(void) {
    const uint32_t starts[]={0,11700,45000};
    for(unsigned s=0;s<3;s++)for(unsigned run=0;run<2;run++) {
        fixture(starts[s],run!=0,31);struct kui_retail_zso_image image;
        assert(kui_retail_zso_image_init(&image,&map,read_card,&storage)==KUI_GAME_OK && !storage.calls);
        if(run)image.read_run=read_run;
        assert(kui_retail_zso_verify_source(&image)==KUI_GAME_OK);
        static uint8_t out[64*2048u+32];memset(out,0x96,sizeof(out));
        assert(kui_retail_zso_image_read(&image,starts[s],64,KUI_GAME_SECTOR_MODE1,out+16,64u*2048u)==KUI_GAME_OK);
        assert(!memcmp(out+16,expected,64u*2048u));
        for(unsigned i=0;i<16;i++)assert(out[i]==0x96 && out[sizeof(out)-1-i]==0x96);
        assert(kui_retail_zso_image_read(&image,starts[s]+127,1,KUI_GAME_SECTOR_MODE1,out,2048)==KUI_GAME_OK);
        assert(!memcmp(out,expected[127],2048));
        if(run)assert(storage.max_hint>1);
        const uint32_t lbas[]={starts[s],starts[s]+128,starts[s]+127,UINT32_MAX,starts[s]};
        const uint32_t counts[]={0,1,2,1,65};
        for(unsigned bad=0;bad<5;bad++) {
            struct kui_retail_zso_image before=image;unsigned calls=storage.calls;memset(out,0xa7,2048);
            assert(kui_retail_zso_image_read(&image,lbas[bad],counts[bad],KUI_GAME_SECTOR_MODE1,out,2048)!=KUI_GAME_OK);
            assert(!memcmp(&before,&image,sizeof(image)) && storage.calls==calls);
            for(unsigned i=0;i<2048;i++)assert(out[i]==0xa7);
        }
        struct kui_retail_zso_image before=image;unsigned calls=storage.calls;
        assert(kui_retail_zso_image_read(&image,starts[s],1,KUI_GAME_SECTOR_RAW,out,2048)==KUI_GAME_UNSUPPORTED);
        assert(kui_retail_zso_image_read(&image,starts[s],1,KUI_GAME_SECTOR_MODE1,out,2047)==KUI_GAME_RANGE);
        assert(!memcmp(&before,&image,sizeof(image)) && storage.calls==calls);
    }
    puts("PASS ZSO retail reads: CD/GD session bases, fragmented index/payload, raw/LZ4 bytes, run bounds and no-I/O preflight");
}
static void failure_and_reset(void) {
    fixture(45000,false,31);struct kui_retail_zso_image image;uint8_t out[4096];
    assert(kui_retail_zso_image_init(&image,&map,read_card,&storage)==KUI_GAME_OK);
    assert(kui_retail_zso_image_read(&image,45000,2,KUI_GAME_SECTOR_MODE1,out,sizeof(out))==KUI_GAME_OK);
    unsigned calls=storage.calls;
    for(unsigned fault=1;fault<=calls;fault++) {
        assert(kui_retail_zso_image_init(&image,&map,read_card,&storage)==KUI_GAME_OK);
        storage.calls=0;storage.fail_on=fault;
        assert(kui_retail_zso_image_read(&image,45000,2,KUI_GAME_SECTOR_MODE1,out,sizeof(out))==KUI_GAME_IO);
        assert(!image.cache_valid);
        storage.fail_on=0;assert(kui_retail_zso_image_read(&image,45000,2,KUI_GAME_SECTOR_MODE1,out,sizeof(out))==KUI_GAME_OK);
        assert(!memcmp(out,expected,4096));
    }
    assert(kui_retail_zso_image_init(&image,&map,read_card,&storage)==KUI_GAME_OK);
    storage.calls=0;assert(kui_retail_zso_verify_source(&image)==KUI_GAME_OK);calls=storage.calls;
    for(unsigned fault=1;fault<=calls;fault++) {
        assert(kui_retail_zso_image_init(&image,&map,read_card,&storage)==KUI_GAME_OK);
        storage.calls=0;storage.fail_on=fault;
        assert(kui_retail_zso_verify_source(&image)==KUI_GAME_IO && !image.cache_valid);storage.fail_on=0;
    }
    change_file_byte(20,2);assert(kui_retail_zso_image_init(&image,&map,read_card,&storage)==KUI_GAME_OK);
    assert(kui_retail_zso_verify_source(&image)!=KUI_GAME_OK && !image.cache_valid);change_file_byte(20,1);
    /* After explicit reset, changed source metadata and malformed selected
     * blocks cannot reuse data from the previous physical-sector cache. */
    uint32_t compressed_start=24u+(LOGICAL_BLOCKS+1u)*4u+2048u;
    uint8_t saved_token=file[compressed_start+2];
    change_file_byte(compressed_start+2,0);
    assert(kui_retail_zso_image_init(&image,&map,read_card,&storage)==KUI_GAME_OK);
    assert(kui_retail_zso_image_read(&image,45001,1,KUI_GAME_SECTOR_MODE1,out,2048)!=KUI_GAME_OK && !image.cache_valid);
    change_file_byte(compressed_start+2,saved_token);
    assert(kui_retail_zso_image_init(&image,&map,read_card,&storage)==KUI_GAME_OK && !image.blocks_read && !image.cache_valid && !image.read_run);
    assert(kui_retail_zso_verify_source(&image)==KUI_GAME_OK);
    assert(kui_retail_zso_image_read(&image,45001,1,KUI_GAME_SECTOR_MODE1,out,2048)==KUI_GAME_OK && !memcmp(out,expected[1],2048));
    puts("PASS ZSO retail faults: every physical read failure, poisoned-cache invalidation, late source changes and clean reset/retry");
}
/* Each selected block's compressed bytes arrive in one modeled CMD18 stream,
 * from its first physical block through the block ending its indexed span. */
static void streams(void) {
    static uint8_t out[64u*2048u];
    static const unsigned shifts[]={0,4,KUI_ZSO_MAX_SHIFT};
    for(unsigned k=0;k<3u;k++)for(unsigned fragmented=0;fragmented<2u;fragmented++) {
        const unsigned shift=shifts[k];
        fixture_layout(45000,false,fragmented?31u:1u,DENSE,shift);
        struct kui_retail_zso_image image;
        assert(kui_retail_zso_image_init(&image,&map,read_card,&storage)==KUI_GAME_OK);
        image.read_run=read_run;
        assert(kui_retail_zso_verify_source(&image)==KUI_GAME_OK && !image.span_end);stream_stop(&storage);
        unsigned starts=storage.starts,blocks=storage.calls,stops=storage.stops,dense_blocks=0,longest=0;
        for(unsigned block=0;block<LOGICAL_BLOCKS;block++) {
            uint32_t entry=get(file+24u+block*4u),start=(entry&UINT32_C(0x7fffffff))<<shift;
            uint32_t end=(get(file+28u+block*4u)&UINT32_C(0x7fffffff))<<shift;
            unsigned before=storage.starts;memset(out,0x5c,2048u);
            if(!fragmented)storage.limit_lba=physical(end-1u);
            assert(kui_retail_zso_image_read(&image,45000u+block,1,KUI_GAME_SECTOR_MODE1,out,2048u)==KUI_GAME_OK);
            stream_stop(&storage);storage.limit_lba=0;
            assert(!memcmp(out,expected[block],2048u) && !image.span_end);
            if(fragmented)continue;
            /* One stream for the index pair, one for every span byte. */
            assert(storage.starts-before==2u && storage.start_lba==physical(start) &&
                   storage.start_count==physical(end-1u)-physical(start)+1u);
            if(storage.start_count>longest)longest=storage.start_count;
            if(!(entry&UINT32_C(0x80000000)) && block%4u && block!=LOGICAL_BLOCKS-1u) {
                ++dense_blocks;assert(storage.start_count>=4u);
            }
        }
        starts=storage.starts-starts;blocks=storage.calls-blocks;stops=storage.stops-stops;
        /* Valid padding is below2^shift and ends on an aligned offset, so it
         * never reaches a physical block the decoder does not consume. */
        assert(fragmented || !stops);
        if(!fragmented) {
            assert(dense_blocks==95u && starts==2u*LOGICAL_BLOCKS);
            printf("ZSO retail streams: shift%u, %u single-sector reads use %u streams for %u physical blocks (%u early stops), longest %u\n",
                   shift,LOGICAL_BLOCKS,starts,blocks,stops,longest);
        }
        for(unsigned first=0;first<LOGICAL_BLOCKS;first+=64u) {
            unsigned before=storage.starts;memset(out,0x5c,sizeof(out));
            assert(kui_retail_zso_image_read(&image,45000u+first,64,KUI_GAME_SECTOR_MODE1,out,sizeof(out))==KUI_GAME_OK);
            stream_stop(&storage);
            assert(!memcmp(out,expected[first],sizeof(out)) && !image.span_end);
            assert(fragmented || storage.starts-before==2u*64u);
        }
        if(fragmented)continue;
        /* A failure at any physical read, including inside a span stream,
         * still returns IO, invalidates the cache and leaves no span hint. */
        image.cache_valid=0;unsigned calls=storage.calls;
        assert(kui_retail_zso_image_read(&image,45001u,2,KUI_GAME_SECTOR_MODE1,out,2u*2048u)==KUI_GAME_OK);
        stream_stop(&storage);calls=storage.calls-calls;assert(calls>8u);
        for(unsigned fault=1;fault<=calls;fault++) {
            image.cache_valid=0;storage.calls=0;storage.fail_on=fault;
            assert(kui_retail_zso_image_read(&image,45001u,2,KUI_GAME_SECTOR_MODE1,out,2u*2048u)==KUI_GAME_IO);
            stream_stop(&storage);assert(!image.cache_valid && !image.span_end);storage.fail_on=0;
            assert(kui_retail_zso_image_read(&image,45001u,2,KUI_GAME_SECTOR_MODE1,out,2u*2048u)==KUI_GAME_OK);
            stream_stop(&storage);assert(!memcmp(out,expected[1],2u*2048u) && !image.span_end);
        }
    }
    puts("PASS ZSO retail streams: dense/stored/literal spans in one bounded stream each, alignment padding, extent-bounded fragments and64-sector requests");
}
int main(void){wire_and_maps();readers();failure_and_reset();streams();return 0;}
