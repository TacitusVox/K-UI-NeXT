/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/vmu_native_discovery.h"
#include "kui/hash.h"
#if !KUI_ON_CONSOLE || KUI_GUI_DIGEST_REUSE
#include "kui/sha256_prefix.h"
#endif
#include <string.h>

struct witness {int16_t offset;uint16_t value;};
/* Authored sparse contracts shared by the reviewed uniform SDK routines.
 * A new compiler/SDK shape needs another family, never a fuzzy acceptance. */
static const struct witness words[]={
    {0,0x9028},{2,0x63e2},{4,0x023e},{6,0x12c6},{8,0x9029},{10,0x63e2},
    {84,0x10f4},{94,0x0fd8},{-192,0x401b},{-258,0x60e2},{-256,0x001e},
    {-254,0x8801},{-252,0x8d02},{-248,0xa0d3},{-246,0xe0fd},
    {266,0x0316},{272,0x2320},{22,0x22ca},{24,0x0326},{58,0x22ca},
    {60,0x0326},{170,0x03d6},{176,0x23d0},{198,0x024e},{200,0x2228},
    {214,0x5036},{216,0xc801},{230,0x401b},{232,0x8906},{248,0xe000},
    {328,0x420b},{366,0x430b}
};
static uint16_t u16(const uint8_t *p) {return (uint16_t)(p[0]|(uint16_t)p[1]<<8);}
static uint32_t u32(const uint8_t *p) {return (uint32_t)u16(p)|(uint32_t)u16(p+2)<<16;}
static uint32_t physical(uint32_t a) {return a&0x1fffffffu;}
/* U0, P1 and P2 expose the same physical Dreamcast RAM. A disjoint virtual
 * destination may still overwrite the supposedly immutable executable. */
static bool direct_ram_span(uintptr_t a,size_t n,uint32_t *p) {
#if UINTPTR_MAX > UINT32_MAX
    if(a>UINT32_MAX) return false;
#endif
    uint32_t address=(uint32_t)a,area=address&0xe0000000u;
    *p=physical(address);
    return (area==0u || area==0x80000000u || area==0xa0000000u) &&
        *p>=0x0c000000u && *p<0x0d000000u && n<=0x0d000000u-*p;
}
static bool aliased_overlap(uintptr_t a,size_t an,uintptr_t b,size_t bn) {
    uint32_t ap,bp;
    return direct_ram_span(a,an,&ap) && direct_ram_span(b,bn,&bp) &&
        ap<bp+bn && bp<ap+an;
}
static bool ram(uint32_t a) {
    uint32_t area=a&0xe0000000u,p=physical(a);
    return (area==0u || area==0x80000000u || area==0xa0000000u) &&
        p>=0x0c010000u && p<0x0d000000u;
}
static bool valid(const void *image,size_t n,uint32_t base) {
    return image && n>=2u && n<=0xc00000u && n<=UINTPTR_MAX-(uintptr_t)image &&
        ram(base) && !(base&3u) && n<=0x0d000000u-physical(base);
}
static bool index_of(uint32_t a,uint32_t base,size_t n,size_t bytes,size_t *at) {
    uint32_t p=physical(a),b=physical(base);
    if(!ram(a) || (a&3u) || p<b || p-b>n || bytes>n-(p-b)) return false;
    *at=p-b;return true;
}
static void hash(const void *p,size_t n,uint8_t out[32]) {
    struct kui_sha256 h;kui_sha256_init(&h);kui_sha256_update(&h,p,n);kui_sha256_digest(&h,out);
}
static bool sdk_at(const uint8_t *p,size_t n,uint32_t base,size_t site,
    struct kui_vmu_native_discovery *r) {
    if(site<0x53cu || site>n || n-site<0x1b8u || (site&1u)) return false;
    for(unsigned i=0;i<sizeof(words)/sizeof(words[0]);i++)
        if(u16(p+site+words[i].offset)!=words[i].value) return false;
    size_t init=site-0x53cu;
    static const uint16_t begin[5]={0x2fe6,0xde3e,0xd33e,0x9071,0x6233};
    for(unsigned i=0;i<5u;i++) if(u16(p+init+2u*i)!=begin[i]) return false;
    uint32_t manager_cell=u32(p+site+0x1a4u),lock_cell=u32(p+site+0x1a0u);
    /* Decode the initializer's two exact PC-relative loads, including its
     * halfword-aligned instruction position, rather than guessing RAM holes. */
    size_t cell=((base+(uint32_t)init+6u)&~3u)-base+4u*0x3eu;
    size_t object=((base+(uint32_t)init+8u)&~3u)-base+4u*0x3eu;
    if(cell>n || n-cell<4u || object>n || n-object<4u) return false;
    uint32_t manager=u32(p+object);
    if(!ram(manager_cell) || !ram(manager) || !ram(lock_cell) ||
       ((manager_cell|manager|lock_cell)&3u) ||
       u32(p+cell)!=manager_cell || physical(manager)>0x0d000000u-0x1100u ||
       physical(manager_cell)!=physical(manager)+0x10fcu) return false;
    size_t lock;
    if(!index_of(lock_cell,base,n,4u,&lock) || u32(p+lock)!=lock_cell+4u) return false;
    r->submit=base+(uint32_t)site;r->resume=r->submit+12u;
    r->initializer=base+(uint32_t)init;r->manager_cell=manager_cell;
    r->manager_initial=manager;r->lock_cell=lock_cell;return true;
}
static bool wrapper_at(const uint8_t *p,size_t n,uint32_t base,size_t site,size_t call,
    struct kui_vmu_native_discovery *r) {
    if(call<8u || call>n || n-call<6u || (call&1u)) return false;
    uint16_t load=u16(p+call),test=u16(p+call-8u);
    if((load&0xff00u)!=0xd300u || u16(p+call+2u)!=0x430bu ||
       u16(p+call+4u)!=0x0009u || (test!=0x4c11u && test!=0x4d11u) ||
       u16(p+call-6u)!=0x8901u) return false;
    uint32_t address=((base+(uint32_t)call+4u)&~3u)+4u*(load&255u);
    size_t slot;
    if(!index_of(address,base,n,20u,&slot) || slot<4u) return false;
    uint32_t consumer=u32(p+slot),gate=u32(p+slot-4u),ready=u32(p+slot+16u);
    if(!ram(consumer) || !ram(gate) || !ram(ready) || ((consumer|gate|ready)&1u) ||
       physical(consumer)!=physical(base)+(uint32_t)site+0x116u ||
       physical(gate)!=physical(base)+(uint32_t)site+0xbeu ||
       physical(ready)!=physical(base)+(uint32_t)site+0x100u) return false;
    r->completion_slot=address;r->completion_call=base+(uint32_t)call+2u;
    r->consumer=consumer;r->gate=gate;r->ready=ready;r->gate_register=(test>>8)&15u;
    return true;
}
/* A PC-relative MOV.L of register reg at offset at, decoded to its literal. */
static bool pc_literal(const uint8_t *p,size_t n,uint32_t base,size_t at,unsigned reg,uint32_t *value) {
    if(at>n || n-at<2u || (u16(p+at)&0xff00u)!=(0xd000u|reg<<8)) return false;
    size_t slot;
    if(!index_of(((base+(uint32_t)at+4u)&~3u)+4u*(u16(p+at)&255u),base,n,4u,&slot)) return false;
    *value=u32(p+slot);return true;
}
static bool pc_word(const uint8_t *p,size_t n,uint32_t base,size_t at,unsigned reg,uint16_t value) {
    if(at>n || n-at<2u || (u16(p+at)&0xff00u)!=(0x9000u|reg<<8)) return false;
    size_t slot=at+4u+2u*(u16(p+at)&255u);(void)base;
    return slot<=n && n-slot>=2u && u16(p+slot)==value;
}
/* One compiler emits its frame sizes and a contiguous pointer island after a
 * forward BRA/NOP. A pointer's low half can decode as BSR to the delay slot
 * of the heap call. Recognize data only when every word has its own complete
 * preceding load, neither scalar has another reader, and there is no entry
 * anywhere in the island (including the barrier delay). Never exempt a raw
 * pointer or an address-generation instruction in the startup-head scan. */
static bool startup_frame_pool(const uint8_t *p,size_t n,uint32_t base,size_t pos,
    size_t *lo,size_t *hi) {
    enum { POINTERS=13 };
    size_t word=pos&~(size_t)3u,begin=word;
    if(n-word<4u || !ram(u32(p+word))) return false;
    while(begin>=4u && word-begin<4u*POINTERS && ram(u32(p+begin-4u))) begin-=4u;
    size_t end=begin+4u*POINTERS;
    if(begin<12u || end>n || word>=end) return false;
    const size_t first=begin-6u,barrier=first-4u;
    uint16_t jump=u16(p+barrier);
    if((jump&0xf800u)!=0xa000u || barrier+4u+2u*(jump&0x7ffu)<end ||
       barrier+4u+2u*(jump&0x7ffu)>n-2u || u16(p+barrier+2u)!=0x0009u ||
       u16(p+first)!=0xff7cu || u16(p+first+2u)!=0x0084u || u16(p+first+4u)!=0u) return false;
    for(unsigned i=0;i<POINTERS;i++) if(!ram(u32(p+begin+4u*i))) return false;
    uint32_t mask=0u;unsigned dec=0u,inc=0u;
    for(size_t at=0;at+2u<=n;at+=2u) {
        uint16_t op=u16(p+at);
        size_t target=SIZE_MAX;
        if((op&0xe000u)==0xa000u) {
            int d=op&4095u;if(d&2048)d-=4096;
            target=(size_t)((int32_t)at+4+2*d);
        } else if((op&0xff00u)==0x8900u || (op&0xff00u)==0x8b00u ||
                  (op&0xff00u)==0x8d00u || (op&0xff00u)==0x8f00u) {
            target=(size_t)((int32_t)at+4+2*(int8_t)op);
        } else if((op&0xff00u)==0xc700u) target=((at+4u)&~(size_t)3u)+4u*(op&255u);
        if(target>=barrier+2u && target<end) return false;
        if(!(at&3u) && n-at>=4u) {
            uint32_t addr=u32(p+at),v=physical(addr);
            if(ram(addr) && v>=physical(base)+barrier+2u && v<physical(base)+end) return false;
        }
        if((op&0xf000u)==0xd000u) {
            target=((at+4u)&~(size_t)3u)+4u*(op&255u);
            if(target>=barrier+2u && target<end) {
                if(at>=barrier || target<begin) return false;
                mask|=1u<<((target-begin)/4u);
            }
        } else if((op&0xf000u)==0x9000u) {
            target=at+4u+2u*(op&255u);
            if(target>=barrier+2u && target<end) {
                if(at+4u>barrier) return false;
                if(target==first && (op>>8)==0x90u && u16(p+at+2u)==0x3f0cu) ++dec;
                else if(target==first+2u && (op>>8)==0x91u && u16(p+at+2u)==0x3f1cu) ++inc;
                else return false;
            }
        }
    }
    *lo=begin;*hi=end;
    return mask==(1u<<POINTERS)-1u && dec==1u && inc==1u;
}
/* No bypass of the lock normalization on the way to the borrowed heap call.
 * The deferred layout also proves through the later Maple call. Both heads
 * have no internal branches or literal pools. Only syntactic branches from
 * the separately proven frame pool are data; raw pointers and MOVA into
 * either head always refuse. */
static bool startup_head_no_entries(const uint8_t *p,size_t n,uint32_t base,size_t head,bool deferred) {
    uint32_t first=physical(base)+(uint32_t)head,last=first+(deferred?130u:86u);
    size_t pool_begin=0u,pool_end=0u;
    for(size_t at=0;at+2u<=n;at+=2u) {
        uint16_t op=u16(p+at);uint32_t target=0u;bool transport=false;
        if((op&0xe000u)==0xa000u) {
            int32_t d=op&0xfffu;if(d&0x800) d-=0x1000;
            target=physical(base)+(uint32_t)at+4u+(uint32_t)(2*d);transport=true;
        } else if((op&0xff00u)==0x8900u || (op&0xff00u)==0x8b00u ||
                  (op&0xff00u)==0x8d00u || (op&0xff00u)==0x8f00u) {
            int32_t d=op&255u;if(d&128) d-=256;
            target=physical(base)+(uint32_t)at+4u+(uint32_t)(2*d);transport=true;
        } else if((op&0xff00u)==0xc700u) {
            target=((physical(base)+(uint32_t)at+4u)&~3u)+4u*(op&255u);transport=true;
        }
        if(transport && target>first && target<last) {
            if(!deferred || (op&0xff00u)==0xc700u) return false;
            if(!(at>=pool_begin && at<pool_end) &&
               (pool_end || !startup_frame_pool(p,n,base,at,&pool_begin,&pool_end))) return false;
        }
        if(!(at&3u) && n-at>=4u) {
            uint32_t value=u32(p+at);
            if(ram(value) && physical(value)>first && physical(value)<last) return false;
        }
    }
    return true;
}
static bool source_code(uint32_t value,uint32_t base,size_t n) {
    return ram(value) && !(value&1u) && physical(value)>=physical(base) &&
        physical(value)-physical(base)<=n-2u;
}
/* One exact startup register allocation, independently linked to the lock
 * cell and to the later heap/Maple role records. Its source lock starts as
 * cell+4; MOV.L/OR/MOV.L installs only its P2 alias. No inherited register
 * chooses a pointer, and every instruction through the call delay is fixed.
 * The following ordinary calls are not assumed to preserve cache state:
 * the later live coherence gate must prove both idle copies explicitly. */
static bool startup_p2_at(const uint8_t *p,size_t n,uint32_t base,size_t s,
    const struct kui_vmu_native_discovery *driver,bool deferred) {
    static const struct witness w[]={
        {0,0x2fe6},{2,0x2fd6},{4,0x2fc6},{6,0x2fb6},{8,0x2fa6},{10,0x2f96},
        {12,0x4f22},{14,0x7ff0},{20,0x0002},{26,0x2019},
        {30,0xcbf0},{34,0x6242},{36,0x223b},{38,0x2422},{40,0x400e},
        {44,0x420b},{46,0x0009},{50,0xeee0},{54,0x6522},{58,0x2539},{64,0x6032},
        {66,0x251b},{68,0x202b},{72,0x20e9},{74,0x3508},{76,0x6403},{78,0x1f03},
        {80,0x75e0},{82,0x420b},{84,0x7420}
    };
    static const struct witness ordinary[]={
        {16,0x1f41},{28,0x1f52},{32,0x2f62},{88,0x55f2},{90,0x66f2},{92,0x430b},
        {94,0x54f1},{98,0x420b},{100,0x0009},{108,0x430b},{110,0xe400}
    };
    static const struct witness extended[]={
        {16,0x2f42},{28,0x1f52},{32,0x1f61},{86,0x60f2},{90,0xc903},{92,0x430b},
        {94,0x6403},{96,0x55f2},{98,0xe700},{102,0xe601},{104,0x430b},{106,0x64f2},
        {110,0x430b},{112,0x54f1},{116,0x420b},{118,0x0009},{126,0x430b},{128,0xe400}
    };
    if(s>n || n-s<(deferred?130u:112u)) return false;
    const struct witness *tail=deferred?extended:ordinary;
    unsigned count=deferred?sizeof(extended)/sizeof(*extended):sizeof(ordinary)/sizeof(*ordinary);
    for(unsigned i=0;i<count;i++) if(u16(p+s+tail[i].offset)!=tail[i].value) return false;
    for(unsigned i=0;i<sizeof(w)/sizeof(w[0]);i++)
        if(u16(p+s+w[i].offset)!=w[i].value) return false;
    uint32_t cell,alias,end_cell,again,mask,area,upper,heap,rx,tx,value;
    size_t end;
    if(!pc_literal(p,n,base,s+18u,4u,&cell) || cell!=driver->lock_cell ||
       !pc_word(p,n,base,s+22u,1u,0xff0fu) ||
       !pc_literal(p,n,base,s+24u,3u,&alias) || alias!=0xa0000000u ||
       !pc_literal(p,n,base,s+42u,2u,&value) || !source_code(value,base,n) ||
       !pc_literal(p,n,base,s+48u,2u,&end_cell) || !index_of(end_cell,base,n,4u,&end) ||
       !ram(u32(p+end)) || !pc_literal(p,n,base,s+52u,3u,&mask) || mask!=0xe0000000u ||
       !pc_literal(p,n,base,s+56u,2u,&area) || area!=0x80000000u ||
       !pc_literal(p,n,base,s+60u,3u,&again) || again!=end_cell ||
       !pc_literal(p,n,base,s+62u,1u,&upper) || upper!=0x0d000000u ||
       !pc_literal(p,n,base,s+70u,2u,&heap) || !source_code(heap,base,n)) return false;
    if(deferred && (!pc_literal(p,n,base,s+88u,3u,&value) || !source_code(value,base,n) ||
                    !pc_literal(p,n,base,s+100u,3u,&value) || !source_code(value,base,n))) return false;
    size_t tail_base=s+(deferred?18u:0u);
    if(!pc_literal(p,n,base,s+(deferred?108u:86u),3u,&value) || !source_code(value,base,n) ||
       !pc_literal(p,n,base,tail_base+96u,2u,&value) || !source_code(value,base,n) ||
       !pc_literal(p,n,base,tail_base+102u,3u,&value) || !source_code(value,base,n) ||
       !pc_literal(p,n,base,tail_base+104u,5u,&rx) || !ram(rx) ||
       !pc_literal(p,n,base,tail_base+106u,6u,&tx) || !ram(tx)) return false;
    return startup_head_no_entries(p,n,base,s,deferred);
}
static const uint16_t startup_p2_prologue[]={
    0x2fe6,0x2fd6,0x2fc6,0x2fb6,0x2fa6,0x2f96,0x4f22,0x7ff0,0x1f41};
static const uint16_t startup_p2_lock[]={0x6242,0x223b,0x2422};
static const uint16_t startup_p2_heap[]={0x20e9,0x3508,0x6403,0x1f03,0x75e0,0x420b,0x7420};
static bool startup_words(const uint8_t *p,const uint16_t *expected,unsigned count) {
    for(unsigned i=0;i<count;i++) if(u16(p+2u*i)!=expected[i]) return false;
    return true;
}
/* Return 0 for no candidate, 1 for one complete normalizer, -1 for a hinted
 * but unproved/ambiguous head. Two independent disjoint anchors are only a
 * refusal hint, never acceptance. Thus a changed prologue, load register,
 * literal alias or lock operation cannot silently become cached-lock early
 * metadata. Every accepted head still passes the complete exact contract. */
static int startup_p2(const uint8_t *p,size_t n,uint32_t base,
    const struct kui_vmu_native_discovery *driver,size_t *chosen) {
    unsigned found=0;
    for(size_t at=0;at+2u<=n;at+=2u) {
        uint16_t op=u16(p+at);size_t s;
        if(op==startup_p2_prologue[0]) s=at;
        else if(op==startup_p2_lock[0] && at>=34u) s=at-34u;
        else if(op==startup_p2_heap[0] && at>=72u) s=at-72u;
        else continue;
        if(n-s<112u || (found && s==*chosen)) continue;
        bool deferred=u16(p+s+16u)==0x2f42u;
        bool prologue=startup_words(p+s,startup_p2_prologue,8u) &&
            (u16(p+s+16u)==startup_p2_prologue[8] || deferred);
        unsigned anchors=(unsigned)prologue+
            startup_words(p+s+34u,startup_p2_lock,sizeof(startup_p2_lock)/2u)+
            startup_words(p+s+72u,startup_p2_heap,sizeof(startup_p2_heap)/2u);
        if(anchors<2u) continue;
        if(!startup_p2_at(p,n,base,s,driver,deferred) || ++found>1u) return -1;
        *chosen=s;
    }
    return found?1:0;
}
bool kui_vmu_native_early_p2_calls_valid(const void *image,size_t n,
    const struct kui_vmu_native_discovery *driver,uint32_t heap_call,uint32_t maple_call) {
    size_t startup=0;
    return driver && driver->family==KUI_VMU_NATIVE_FAMILY_EARLY_P2_V1 &&
        driver->version==KUI_VMU_NATIVE_DISCOVERY_VERSION && driver->bytes==sizeof(*driver) &&
        driver->image_bytes==n && valid(image,n,driver->load_address) &&
        startup_p2(image,n,driver->load_address,driver,&startup)==1 &&
        heap_call==driver->load_address+(uint32_t)startup+82u &&
        maple_call==driver->load_address+(uint32_t)startup+
            (u16((const uint8_t *)image+startup+16u)==0x2f42u?126u:108u);
}
/* The pd 1.46 server entry, completion_call-0x3a: it saves R8-R14 and PR,
 * calls the lock helper with the busy word set to one in the delay slot, and
 * records a -2 (lock held) result in manager+0x10e8 for the API wrappers'
 * retry. The helper returns -3 or -1 unless the driver is idle and Maple DMA
 * has stopped, then TAS.Bs the byte the lock cell points at: -2 if it was
 * already held, 0 once this server holds it. The other words are the
 * unchanged exit branches, identical in every reviewed image. */
static bool early_server(const uint8_t *p,size_t n,uint32_t base,const struct kui_vmu_native_discovery *r) {
    static const uint16_t entry[31]={
        0x2fe6,0x2fd6,0x2fc6,0x2fb6,0x2fa6,0x2f96,0x2f86,0x4f22,0x7ffc,0xd234,0xea01,0xd332,0xed00,
        0xd82f,0xd92f,0x420b,0x23a2,0x88fe,0x8f04,0x6c03,0x9053,0x6282,0xa0ab,0x02a6,0x4c11,0x8901,
        0xa09c,0x0009,0xd32b,0x430b,0x0009};
    static const uint16_t helper[33]={
        0xd339,0x906b,0x4f22,0x6432,0x024e,0x2228,0x8902,0x4f26,0x000b,0xe0fd,0x9063,0x034e,0x5036,
        0xc801,0x8902,0x4f26,0x000b,0xe0ff,0xd32f,0x6032,0x401b,0x8906,0xd12f,0xb0b6,0x2f16,0x7f04,
        0x4f26,0x000b,0xe0fe,0xe000,0x4f26,0x000b,0x0009};
    size_t s,h;uint32_t value,busy,lock,cell;
    if(r->completion_call<base+0x3au || !index_of(r->completion_call-0x3au,base,n,sizeof(entry),&s)) return false;
    for(unsigned i=0;i<31u;i++) if(u16(p+s+2u*i)!=entry[i]) return false;
    if(!pc_literal(p,n,base,s+18u,2u,&value) || (value&1u) ||
       !index_of(value&~3u,base,n,sizeof(helper)+2u,&h)) return false;
    h+=value&2u;
    for(unsigned i=0;i<33u;i++) if(u16(p+h+2u*i)!=helper[i]) return false;
    return pc_literal(p,n,base,s+22u,3u,&busy) && ram(busy) && !(busy&3u) &&
        pc_literal(p,n,base,s+26u,8u,&value) && value==r->manager_cell &&
        pc_literal(p,n,base,s+28u,9u,&cell) && ram(cell) && !(cell&3u) &&
        pc_word(p,n,base,s+40u,0u,0x10e8u) &&
        pc_literal(p,n,base,h,3u,&value) && value==r->manager_cell &&
        pc_word(p,n,base,h+2u,0u,0xfc0u) && pc_word(p,n,base,h+20u,0u,0x10f4u) &&
        pc_literal(p,n,base,h+36u,3u,&lock) && lock==r->lock_cell;
}
static bool finish(struct kui_vmu_native_discovery *r,const void *p,size_t n,uint32_t base,
    const struct kui_sha256_prefix *source) {
    r->version=KUI_VMU_NATIVE_DISCOVERY_VERSION;r->bytes=sizeof(*r);
    r->family=early_server(p,n,base,r)?KUI_VMU_NATIVE_FAMILY_EARLY_V1:KUI_VMU_NATIVE_FAMILY_UNIFORM_V1;
    if(r->family==KUI_VMU_NATIVE_FAMILY_EARLY_V1) {
        size_t startup=0;int normalizer=startup_p2(p,n,base,r,&startup);
        if(normalizer<0) return false;
        if(normalizer) r->family=KUI_VMU_NATIVE_FAMILY_EARLY_P2_V1;
    }
    r->image_bytes=(uint32_t)n;
    r->load_address=base;r->requirements=KUI_VMU_NATIVE_REQUIREMENTS;
#if !KUI_ON_CONSOLE || KUI_GUI_DIGEST_REUSE
    if(source) {
        if(p!=source->source || n>source->bytes)return false;
        if(n==source->bytes)memcpy(r->image_sha256,source->whole,sizeof(r->image_sha256));
        else if(!kui_sha256_prefix_digest(source,p,n,r->image_sha256,NULL))return false;
        return true;
    }
#else
    (void)source;
#endif
    hash(p,n,r->image_sha256);
    return true;
}
#if !KUI_ON_CONSOLE || KUI_GUI_DIGEST_REUSE
enum kui_vmu_native_discovery_result kui_vmu_native_discover_prefixed(
    const void *image,size_t n,uint32_t base,struct kui_vmu_native_discovery *out,
    const struct kui_sha256_prefix *source) {
#else
enum kui_vmu_native_discovery_result kui_vmu_native_discover(
    const void *image,size_t n,uint32_t base,struct kui_vmu_native_discovery *out) {
    const struct kui_sha256_prefix *source=NULL;
#endif
    if(!out) return KUI_VMU_NATIVE_INVALID;
    uintptr_t start=(uintptr_t)image,dest=(uintptr_t)out;
    if(sizeof(*out)>UINTPTR_MAX-dest || n>UINTPTR_MAX-start ||
       (dest<start+n && start<dest+sizeof(*out)) ||
       aliased_overlap(start,n,dest,sizeof(*out))) return KUI_VMU_NATIVE_INVALID;
    memset(out,0,sizeof(*out));if(!valid(image,n,base)) return KUI_VMU_NATIVE_INVALID;
    const uint8_t *p=image;unsigned found=0,partial=0;
    struct kui_vmu_native_discovery chosen={0};
    for(size_t site=0x53cu;site<n && n-site>=0x1b8u;site+=2u) {
        if(u16(p+site)!=0x9028u || u16(p+site+2u)!=0x63e2u) continue;
        struct kui_vmu_native_discovery r={0};
        if(!sdk_at(p,n,base,site,&r)) continue;
        ++partial;
        for(size_t call=8u;call<n && n-call>=6u;call+=2u) {
            if(!wrapper_at(p,n,base,site,call,&r)) continue;
            if(++found>1u) return KUI_VMU_NATIVE_AMBIGUOUS;
            chosen=r;
        }
        /* More than one SDK body is ambiguous even if one wrapper is absent.
         * Never choose the first candidate from an image/overlay collection. */
        if(partial>1u) return KUI_VMU_NATIVE_AMBIGUOUS;
    }
    if(!found) return partial?KUI_VMU_NATIVE_INCOMPLETE:KUI_VMU_NATIVE_NOT_FOUND;
    if(!finish(&chosen,image,n,base,source)) return KUI_VMU_NATIVE_INCOMPLETE;
    *out=chosen;return KUI_VMU_NATIVE_RESOLVED;
}
#if !KUI_ON_CONSOLE || KUI_GUI_DIGEST_REUSE
enum kui_vmu_native_discovery_result kui_vmu_native_discover(
    const void *image,size_t n,uint32_t base,struct kui_vmu_native_discovery *out) {
    return kui_vmu_native_discover_prefixed(image,n,base,out,NULL);
}
#endif
bool kui_vmu_native_discovery_cache_valid(const struct kui_vmu_native_discovery *r,
    const void *image,size_t n,uint32_t base) {
    if(!r || !valid(image,n,base) || r->version!=KUI_VMU_NATIVE_DISCOVERY_VERSION ||
       r->bytes!=sizeof(*r) || !kui_vmu_native_family_valid(r->family) ||
       r->requirements!=KUI_VMU_NATIVE_REQUIREMENTS || r->image_bytes!=n ||
       r->load_address!=base || r->submit<base || r->completion_call<base+2u) return false;
    /* A cache is untrusted metadata. Recheck uniqueness as well as hashes:
     * a forged record must not select one of several plausible SDK copies.
     * This conservative reference implementation trades cache speed for
     * complete replay of discovery; an installer can never trust addresses
     * merely because a previous boot wrote a file containing them. */
    struct kui_vmu_native_discovery fresh;
    return kui_vmu_native_discover(image,n,base,&fresh)==KUI_VMU_NATIVE_RESOLVED &&
        !memcmp(r,&fresh,sizeof(fresh));
}
