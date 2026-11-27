/* SPDX-License-Identifier: GPL-3.0-only */
/* Actual frontend include, actual seed/cache codecs and detector; bounded
 * fake file/image I/O injects failures at publication boundaries. */
#define main resolution_cache_codec_main
#include "test_vmu_native_resolution_cache.c"
#undef main
#define KUI_PROBE_H
typedef void (*kui_log_fn)(const char *,...);
typedef bool (*kui_cancel_fn)(void);
#include "kui/runtime.h"
#include "kui/retail_image.h"
#include "kui/retail_loader_layout.h"
#include "kui/vmu_native_cache_seed.h"
#include "kui/native_dual_reader.h"
#include "kui/sha256_prefix.h"

typedef unsigned UINT;
typedef unsigned FRESULT;
enum {FR_OK,FR_NO_FILE,FR_NO_PATH,FR_EXIST,FR_DISK_ERR,FR_DENIED};
enum {AM_DIR=16u,FA_READ=1u,FA_WRITE=2u,FA_CREATE_NEW=4u};
typedef struct {size_t fsize;unsigned fattrib;} FILINFO;
typedef struct {unsigned node;size_t at;} FIL;
struct fake_node {char path[128];uint8_t data[KUI_VMU_NATIVE_RESOLUTION_CACHE_BYTES+64u];size_t size;bool directory,exists;};
static struct fake_node disk[20];
static struct {
    const uint8_t *boot;size_t bytes;unsigned reads,opens,closed,writes,syncs,renames,unlinks,builds,replays,logs;
    unsigned cancel_checks,cancel_at,allocation_at,allocations,stats,mkdirs;
    unsigned fail,cancel_when;bool cancelled;
    const uint8_t *runtime_boot;size_t runtime_bytes,source_sha_bytes;
    size_t forced_program_bytes;
    uint32_t forced_pool,forced_pool_bytes;
    unsigned source_sha_calls,extent_calls,vmu_path_lookups;
    bool authored_resolution,fail_extent;
} io;
void __real_kui_sha256_update(struct kui_sha256 *,const void *,size_t);
void __wrap_kui_sha256_update(struct kui_sha256 *sha,const void *data,size_t bytes) {
    uintptr_t at=(uintptr_t)data,first=(uintptr_t)io.runtime_boot;
    if(io.runtime_boot && at>=first && at-first<=io.runtime_bytes && bytes<=io.runtime_bytes-(at-first)) {
        io.source_sha_bytes+=bytes;++io.source_sha_calls;
    }
    __real_kui_sha256_update(sha,data,bytes);
}
enum {FAIL_NONE,FAIL_OPEN,FAIL_WRITE,FAIL_SHORT,FAIL_SYNC,FAIL_CLOSE,FAIL_READBACK,FAIL_RENAME,FAIL_MKDIR,
    FAIL_BOOT_READ,FAIL_BOOT_CANCEL};
static int node_find(const char *path) {
    if(!strncmp(path,"0:/KUI/VMU",10u) && (!path[10] || path[10]=='/'))++io.vmu_path_lookups;
    for(unsigned i=0;i<20u;i++) if(disk[i].exists && !strcmp(disk[i].path,path)) return (int)i;
    return -1;
}
static unsigned node_add(const char *path,bool directory) {
    int existing=node_find(path);if(existing>=0) return (unsigned)existing;
    for(unsigned i=0;i<20u;i++) if(!disk[i].exists) {
        memset(&disk[i],0,sizeof(disk[i]));CHECK(strlen(path)<sizeof(disk[i].path));
        strcpy(disk[i].path,path);disk[i].exists=true;disk[i].directory=directory;return i;
    }
    CHECK(false);return 0;
}
static FRESULT f_stat(const char *path,FILINFO *out) {
    ++io.stats;
    int n=node_find(path);if(n<0) return FR_NO_FILE;
    *out=(FILINFO){disk[n].size,disk[n].directory?AM_DIR:0u};return FR_OK;
}
static FRESULT f_open(FIL *f,const char *path,unsigned flags) {
    if((flags&FA_WRITE) && io.fail==FAIL_OPEN) return FR_DENIED;
    int n=node_find(path);
    if(flags&FA_CREATE_NEW) {if(n>=0) return FR_EXIST;n=(int)node_add(path,false);}
    if(n<0 || disk[n].directory) return FR_NO_FILE;
    *f=(FIL){(unsigned)n,0};++io.opens;
    if((flags&FA_WRITE) && io.cancel_when==2u) io.cancelled=true;
    return FR_OK;
}
static FRESULT f_read(FIL *f,void *out,UINT size,UINT *got) {
    if(io.fail==FAIL_READBACK && strstr(disk[f->node].path,".tmp")) {*got=0;return FR_DISK_ERR;}
    if(strstr(disk[f->node].path,".tmp") && io.cancel_when==6u) io.cancelled=true;
    size_t take=disk[f->node].size-f->at;if(take>size) take=size;
    memcpy(out,disk[f->node].data+f->at,take);f->at+=take;*got=(UINT)take;return FR_OK;
}
static FRESULT f_write(FIL *f,const void *in,UINT size,UINT *put) {
    ++io.writes;CHECK(strstr(disk[f->node].path,".tmp"));CHECK(f->at+size<=sizeof(disk[0].data));
    if(io.cancel_when==3u) io.cancelled=true;
    if(io.fail==FAIL_WRITE) {*put=0;return FR_DISK_ERR;}
    UINT take=io.fail==FAIL_SHORT?size-1u:size;
    memcpy(disk[f->node].data+f->at,in,take);f->at+=take;disk[f->node].size=f->at;*put=take;return FR_OK;
}
static FRESULT f_sync(FIL *f) {
    (void)f;++io.syncs;if(io.cancel_when==4u) io.cancelled=true;
    return io.fail==FAIL_SYNC?FR_DISK_ERR:FR_OK;
}
static FRESULT f_close(FIL *f) {
    ++io.closed;
    if(strstr(disk[f->node].path,".tmp") && io.cancel_when==5u) io.cancelled=true;
    return io.fail==FAIL_CLOSE && strstr(disk[f->node].path,".tmp")?FR_DISK_ERR:FR_OK;
}
static FRESULT f_mkdir(const char *path) {
    ++io.mkdirs;
    if(io.cancel_when==1u) io.cancelled=true;
    if(io.fail==FAIL_MKDIR) return FR_DENIED;
    if(node_find(path)>=0) return FR_EXIST;
    (void)node_add(path,true);return FR_OK;
}
static FRESULT f_unlink(const char *path) {
    ++io.unlinks;int n=node_find(path);if(n<0) return FR_NO_FILE;
    if(!strstr(path,".tmp") && io.cancel_when==7u) io.cancelled=true;
    CHECK(!disk[n].directory);disk[n].exists=false;return FR_OK;
}
static FRESULT f_rename(const char *from,const char *to) {
    ++io.renames;if(io.fail==FAIL_RENAME) return FR_DISK_ERR;
    CHECK(node_find(to)<0);int n=node_find(from);CHECK(n>=0 && !disk[n].directory);
    CHECK(strstr(from,".tmp") && strstr(to,".nvr"));strcpy(disk[n].path,to);
    if(io.cancel_when==8u) io.cancelled=true;
    return FR_OK;
}
enum kui_game_result kui_game_image_read(const struct kui_game_image *image,uint32_t lba,
    unsigned count,enum kui_game_sector_format format,void *out,size_t capacity) {
    (void)image;CHECK(lba==100u && count==(io.bytes+2047u)/2048u &&
        format==KUI_GAME_SECTOR_MODE1 && capacity==(size_t)count*2048u);
    size_t at=(size_t)(lba-100u)*2048u;CHECK(at<io.bytes);++io.reads;
    io.runtime_boot=out;io.runtime_bytes=io.bytes;
    if(io.fail==FAIL_BOOT_READ)return KUI_GAME_IO;
    if(io.fail==FAIL_BOOT_CANCEL)return KUI_GAME_CANCELLED;
    size_t n=io.bytes-at;CHECK(n<=capacity);memset(out,0,capacity);memcpy(out,io.boot+at,n);
    return KUI_GAME_OK;
}
static bool fixture_replay_hashed(const void *image,size_t bytes,uint32_t address,const void *wire,size_t wire_bytes,
    const uint8_t pin[32],const uint8_t fresh[32],struct kui_vmu_native_resolution *out) {
    uint8_t expected[32];digest(io.boot,bytes,expected);CHECK(!memcmp(expected,fresh,sizeof(expected)));
    ++io.replays;return kui_vmu_native_resolution_cache_replay_hashed(image,bytes,address,wire,wire_bytes,pin,fresh,out);
}
static bool fixture_extent(const void *image,size_t bytes,uint32_t address,size_t *program_bytes) {
    ++io.extent_calls;
    if(io.fail_extent)return false;
    if(io.forced_program_bytes) {
        CHECK(io.forced_program_bytes<bytes);*program_bytes=io.forced_program_bytes;return true;
    }
    return kui_vmu_native_program_extent(image,bytes,address,program_bytes);
}
/* The default run supplies authored SDK metadata for this small generated
 * image. With an original executable the actual full detector is used. */
static bool fixture_build_prefixed(const void *image,size_t bytes,uint32_t address,void *wire,
    size_t wire_bytes,struct kui_vmu_native_resolution *out,const struct kui_sha256_prefix *marks) {
    ++io.builds;
    if(io.authored_resolution) {
        mock(image,bytes,out);
        if(io.forced_pool) {
            out->call.entry_begin=io.forced_pool;
            out->call.entry_bytes=io.forced_pool_bytes?io.forced_pool_bytes:0x100000u;
        }
        return kui_vmu_native_resolution_cache_encode(out,wire,wire_bytes);
    }
    return kui_vmu_native_resolution_cache_build_prefixed(image,bytes,address,wire,wire_bytes,out,marks);
}
static void *fixture_malloc(size_t bytes) {
    if(++io.allocations==io.allocation_at) return NULL;
    return malloc(bytes);
}
#define kui_vmu_native_resolution_cache_build_prefixed fixture_build_prefixed
#define kui_vmu_native_resolution_cache_replay_hashed fixture_replay_hashed
#define kui_vmu_native_program_extent fixture_extent
#define malloc fixture_malloc
#include "../src/apps/games_native_cache.inc"
#undef malloc
#undef kui_vmu_native_resolution_cache_build_prefixed
#undef kui_vmu_native_resolution_cache_replay_hashed
#undef kui_vmu_native_program_extent

static struct kui_game_image game;
static struct kui_retail_manifest map;
static uint8_t module[4096],before[4096];
static struct kui_runtime_image package;
static struct native_cache_source_proof source_proof;
static bool cancelled(void) {
    ++io.cancel_checks;
    if(io.cancel_at && io.cancel_checks==io.cancel_at) io.cancelled=true;
    return io.cancelled;
}
static bool allow_persistent_cache=true;
static void logger(const char *format,...) {CHECK(format);++io.logs;}
static void setup(const uint8_t *boot,size_t bytes) {
    allow_persistent_cache=true;
    memset(&io,0,sizeof(io));memset(disk,0,sizeof(disk));io.boot=boot;io.bytes=bytes;
    memset(&game,0,sizeof(game));game.format=KUI_GAME_IMAGE_GDI;
    memset(&map,0,sizeof(map));map.storage_transport=KUI_STORAGE_SCI;map.boot_lba=100u;map.boot_bytes=(uint32_t)bytes;
    memset(module,0x59,sizeof(module));kui_vmu_native_cache_seed_template(module+128u);
    memcpy(before,module,sizeof(module));package=(struct kui_runtime_image){.data=module,.info={.payload_bytes=sizeof(module)}};
}
static void cache_path(char out[120],const uint8_t *boot,size_t bytes) {
    uint8_t hash[32];digest(boot,bytes,hash);char hex[65];
    for(unsigned i=0;i<32u;i++) snprintf(hex+2u*i,3u,"%02x",hash[i]);
    CHECK(snprintf(out,120,"0:/KUI/VMU/native-cache/v%u/%s.nvr",KUI_VMU_NATIVE_RESOLUTION_DETECTOR_EPOCH,hex)<120);
}
static unsigned seed_cache(const uint8_t *boot,size_t bytes,bool actual) {
    char path[120];cache_path(path,boot,bytes);unsigned n=node_add(path,false);
    if(actual) {
        size_t program_bytes;CHECK(kui_vmu_native_program_extent(boot,bytes,load,&program_bytes));
        CHECK(kui_vmu_native_resolution_cache_build(boot,program_bytes,load,disk[n].data,1024u,NULL));
    }
    else {struct kui_vmu_native_resolution source;mock(boot,bytes,&source);CHECK(kui_vmu_native_resolution_cache_encode(&source,disk[n].data,1024u));}
    disk[n].size=1024u;return n;
}
static enum native_cache_result prepare(bool native,uint32_t options,bool patches,const char *source) {
    source_proof=(struct native_cache_source_proof){.complete=true,.crc32=UINT32_MAX};
    bool standalone=true;
    enum native_cache_result result=native_cache_prepare(&game,&map,native,options,patches,&package,source,allow_persistent_cache,logger,cancelled,&source_proof,&standalone);
    CHECK(io.opens==io.closed);
    CHECK(!standalone); /* No fixture executable carries the SDK SCI family. */
    if(source_proof.complete) {
        CHECK(io.reads==1u);
        CHECK(io.extent_calls==1u);
        CHECK(source_proof.crc32==kui_crc32(0u,io.boot,io.bytes));
        uint8_t digest[32];struct kui_sha256 hash;kui_sha256_init(&hash);
        kui_sha256_update(&hash,io.boot,io.bytes);kui_sha256_digest(&hash,digest);
        CHECK(!memcmp(source_proof.sha256,digest,sizeof(digest)));
    } else CHECK(!source_proof.crc32);
    if(result!=NATIVE_CACHE_READY) CHECK(!memcmp(module,before,sizeof(module)));
    else {
        CHECK(!memcmp(module,before,128u) && !memcmp(module+128u+1120u,before+128u+1120u,sizeof(module)-128u-1120u));
        const uint8_t *wire=NULL,*pin=NULL;struct kui_vmu_native_resolution out;
        CHECK(kui_vmu_native_cache_seed_read(module+128u,1120u,&wire,&pin));
        size_t program_bytes=io.forced_program_bytes;
        if(!program_bytes)CHECK(kui_vmu_native_program_extent(io.boot,io.bytes,load,&program_bytes));
        CHECK(kui_vmu_native_resolution_cache_replay(io.boot,program_bytes,load,wire,1024u,pin,&out));
        CHECK(out.driver.image_bytes==program_bytes);
    }
    return result;
}
static void gates_and_hits(void) {
    uint8_t boot[1024];for(unsigned i=0;i<sizeof(boot);i++) boot[i]=(uint8_t)(i*17u+1u);
    setup(boot,sizeof(boot));seed_cache(boot,sizeof(boot),false);
    CHECK(prepare(true,0u,false,"/Games/PS2")==NATIVE_CACHE_READY);
    CHECK(io.reads==1u && io.replays==1u && !io.builds && !io.writes && !io.renames);
    CHECK(io.source_sha_bytes==sizeof(boot) && io.source_sha_calls==1u);
    /* Isolate the prefix identity boundary from the independently tested
     * lifetime detector: the source/cache filename still covers all bytes,
     * while the proved program and its trusted record cover only a prefix. */
    setup(boot,sizeof(boot));io.forced_program_bytes=768u;
    char prefix_path[120];cache_path(prefix_path,boot,sizeof(boot));
    unsigned prefix_node=node_add(prefix_path,false);struct kui_vmu_native_resolution prefix;
    mock(boot,io.forced_program_bytes,&prefix);
    CHECK(kui_vmu_native_resolution_cache_encode(&prefix,disk[prefix_node].data,1024u));
    disk[prefix_node].size=1024u;
    CHECK(prepare(true,0u,false,"/Games/Prefix")==NATIVE_CACHE_READY);
    CHECK(io.source_sha_bytes==sizeof(boot)+768u && io.source_sha_calls==2u &&
        io.replays==1u && !io.builds && io.reads==1u && io.extent_calls==1u);
    for(unsigned variant=0;variant<15u;variant++) {
        setup(boot,sizeof(boot));bool native=true,patched=false;uint32_t options=0;
        if(variant==0u) native=false; /* WinCE */
        if(variant==1u) options=1u; /* background */
        if(variant==2u) options=UINT32_C(0x20); /* music / other option */
        if(variant==3u) options=UINT32_C(0x40); /* forced encoding */
        if(variant==4u) patched=true;
        if(variant==5u) game.format=KUI_GAME_IMAGE_ZSO;
        if(variant==6u) game.compressed=true;
        if(variant==7u) game.scrambled=true;
        if(variant==8u) game.cd_image=true;
        if(variant==9u) map.storage_transport=KUI_STORAGE_SCIF;
        if(variant==10u) map.reader=KUI_RETAIL_READER_ASYNC;
        if(variant==11u) memset(module+128u,0,1120u);
        if(variant==12u) kui_vmu_native_cache_seed_template(module+2048u);
        if(variant==13u) module[128u+56u]=1u;
        if(variant==14u) memcpy(module+128u,"KUINVS01untyped coincidence",26u);
        memcpy(before,module,sizeof(module));
        CHECK(prepare(native,options,patched,"/Games/PS2")==NATIVE_CACHE_SKIPPED);
        CHECK(!io.reads && !io.opens && !io.writes && !io.builds && !io.replays);
    }
    for(unsigned allocation=1;allocation<=2u;allocation++) {
        setup(boot,sizeof(boot));io.allocation_at=allocation;
        CHECK(prepare(true,0u,false,"/Games/PS2")==NATIVE_CACHE_SKIPPED && !io.reads && !io.writes);
    }
    setup(boot,sizeof(boot));io.cancel_at=1u;
    CHECK(prepare(true,0u,false,"/Games/PS2")==NATIVE_CACHE_CANCELLED && !io.reads);
    setup(boot,sizeof(boot));seed_cache(boot,sizeof(boot),false);io.cancel_at=3u;
    CHECK(prepare(true,0u,false,"/Games/PS2")==NATIVE_CACHE_CANCELLED && !io.writes);
    setup(boot,sizeof(boot));CHECK(prepare(true,0u,false,"/Games/PS2")==NATIVE_CACHE_SKIPPED && io.builds==1u && !io.writes);
}
static void dual_gates_and_hits(void) {
    uint8_t boot[1024];for(unsigned i=0;i<sizeof(boot);i++)boot[i]=(uint8_t)(i*17u+1u);
    const struct kui_native_dual_capability cap={KUI_NATIVE_DUAL_MAGIC0,KUI_NATIVE_DUAL_MAGIC1,
        1u,KUI_NATIVE_DUAL_CAPABILITY_BYTES,KUI_RETAIL_READER_ASYNC,
        KUI_RETAIL_READER_ASYNC_EAGER,KUI_NATIVE_DUAL_LEASE_BYTES,0u};
    for(unsigned reader=KUI_RETAIL_READER_ASYNC;reader<=KUI_RETAIL_READER_ASYNC_EAGER;reader++) {
        setup(boot,sizeof(boot));map.reader=reader;memcpy(module+3072u,&cap,sizeof(cap));
        memcpy(before,module,sizeof(module));seed_cache(boot,sizeof(boot),false);
        CHECK(prepare(true,reader,false,"/Games/PS2")==NATIVE_CACHE_READY);
        CHECK(io.reads==1u && io.replays==1u && !io.builds && !io.writes);
        CHECK(io.source_sha_bytes==sizeof(boot) && io.source_sha_calls==1u);
        for(unsigned fault=0u;fault<16u;fault++) {
            setup(boot,sizeof(boot));map.reader=reader;memcpy(module+3072u,&cap,sizeof(cap));
            uint32_t options=reader;bool native=true,patched=false;
            if(fault==0u)memset(module+3072u,0,sizeof(cap));
            if(fault==1u)memcpy(module+3501u,&cap,sizeof(cap)); /* unaligned duplicate */
            if(fault>=2u && fault<10u)module[3072u+4u*(fault-2u)]^=1u;
            if(fault==10u)options=reader==1u?2u:1u;
            if(fault==11u)options|=0x20u;
            if(fault==12u)map.storage_transport=KUI_STORAGE_SCIF;
            if(fault==13u)native=false;
            if(fault==14u)patched=true;
            if(fault==15u)map.flags|=KUI_RETAIL_IMAGE_CD;
            memcpy(before,module,sizeof(module));
            CHECK(prepare(native,options,patched,"/Games/PS2")==NATIVE_CACHE_SKIPPED);
            CHECK(!io.reads && !io.opens && !io.writes && !io.builds && !io.replays);
        }
        setup(boot,sizeof(boot));map.reader=reader;memcpy(module+3072u,&cap,sizeof(cap));
        memcpy(before,module,sizeof(module));io.cancel_at=1u;
        CHECK(prepare(true,reader,false,"/Games/PS2")==NATIVE_CACHE_CANCELLED && !io.reads);
        for(unsigned fault=FAIL_BOOT_READ;fault<=FAIL_BOOT_CANCEL;fault++) {
            setup(boot,sizeof(boot));map.reader=reader;memcpy(module+3072u,&cap,sizeof(cap));
            memcpy(before,module,sizeof(module));io.fail=fault;
            CHECK(prepare(true,reader,false,"/Games/PS2")==
                (fault==FAIL_BOOT_CANCEL?NATIVE_CACHE_CANCELLED:NATIVE_CACHE_SKIPPED));
            CHECK(!source_proof.complete && !source_proof.crc32 && io.reads==1u && !io.builds);
        }
    }
    uint8_t tail[4097];for(unsigned i=0;i<sizeof(tail);i++)tail[i]=(uint8_t)(i*43u+3u);
    setup(tail,sizeof(tail));map.reader=KUI_RETAIL_READER_ASYNC;
    memcpy(module+3072u,&cap,sizeof(cap));memcpy(before,module,sizeof(module));
    /* The combined module's stage would stop on this executable after the
     * bootstrap: the preflight says so. The complete source read still
     * proves the bytes, and nothing is seeded or saved. */
    CHECK(prepare(true,KUI_RETAIL_READER_ASYNC,false,"/Games/Unknown")==NATIVE_CACHE_UNSUPPORTED);
    CHECK(source_proof.complete && io.reads==1u && io.builds==1u && !io.writes);
}
/* A fixed-arena record (kind 2) needs the combined module's trampoline. A
 * single-reader module is never seeded with one, even from a cache hit; the
 * combined module receives it for either background reader. */
static void fixed_arena_needs_dual(void) {
    uint8_t boot[1024];for(unsigned i=0;i<sizeof(boot);i++)boot[i]=(uint8_t)(i*29u+7u);
    const struct kui_native_dual_capability cap={KUI_NATIVE_DUAL_MAGIC0,KUI_NATIVE_DUAL_MAGIC1,
        1u,KUI_NATIVE_DUAL_CAPABILITY_BYTES,KUI_RETAIL_READER_ASYNC,
        KUI_RETAIL_READER_ASYNC_EAGER,KUI_NATIVE_DUAL_LEASE_BYTES,0u};
    for(unsigned reader=KUI_RETAIL_READER_STANDARD;reader<=KUI_RETAIL_READER_ASYNC_EAGER;reader++) {
        setup(boot,sizeof(boot));map.reader=reader;
        if(reader!=KUI_RETAIL_READER_STANDARD) memcpy(module+3072u,&cap,sizeof(cap));
        memcpy(before,module,sizeof(module));
        char path[120];cache_path(path,boot,sizeof(boot));unsigned n=node_add(path,false);
        struct kui_vmu_native_resolution source;mock(boot,sizeof(boot),&source);
        source.heap.initializer_slot=0u;source.heap.initializer_target=0u;source.call.return_pr=source.call.call+4u;
        CHECK(kui_vmu_native_resolution_cache_encode(&source,disk[n].data,1024u));
        CHECK(get32(disk[n].data+20u)==2u);disk[n].size=1024u;
        CHECK(prepare(true,reader,false,"/Games/Fixed")==
            (reader==KUI_RETAIL_READER_STANDARD?NATIVE_CACHE_SKIPPED:NATIVE_CACHE_READY));
        CHECK(io.reads==1u && io.replays==1u && !io.builds && !io.writes && !io.renames);
    }
}
/* An arena lease (kind 3) is the whole lease: only a combined module whose own
 * lease is exactly the record's arena is seeded with it, for either
 * background reader. A single-reader module, or a combined module with
 * another lease, is never seeded; a cache hit changes nothing. */
static void arena_needs_compact_dual(void) {
    uint8_t boot[1024];for(unsigned i=0;i<sizeof(boot);i++)boot[i]=(uint8_t)(i*31u+11u);
    const uint32_t leases[]={0u,KUI_NATIVE_DUAL_COMPACT_LEASE_BYTES,KUI_NATIVE_DUAL_GENERAL_LEASE_BYTES,
        KUI_NATIVE_DUAL_DIAGNOSTIC_LEASE_BYTES};
    unsigned seeded=0u;
    for(unsigned reader=KUI_RETAIL_READER_STANDARD;reader<=KUI_RETAIL_READER_ASYNC_EAGER;reader++)
        for(unsigned lease=0u;lease<sizeof(leases)/sizeof(leases[0]);lease++) {
            setup(boot,sizeof(boot));map.reader=reader;
            if(leases[lease]) {
                const struct kui_native_dual_capability cap={KUI_NATIVE_DUAL_MAGIC0,KUI_NATIVE_DUAL_MAGIC1,
                    1u,KUI_NATIVE_DUAL_CAPABILITY_BYTES,KUI_RETAIL_READER_ASYNC,
                    KUI_RETAIL_READER_ASYNC_EAGER,leases[lease],0u};
                memcpy(module+3072u,&cap,sizeof(cap));
            }
            memcpy(before,module,sizeof(module));
            char path[120];cache_path(path,boot,sizeof(boot));unsigned n=node_add(path,false);
            struct kui_vmu_native_resolution source;mock(boot,sizeof(boot),&source);arena_record(&source);
            source.call.entry_bytes=KUI_NATIVE_DUAL_COMPACT_LEASE_BYTES;
            CHECK(kui_vmu_native_resolution_cache_encode(&source,disk[n].data,1024u));
            CHECK(get32(disk[n].data+20u)==3u);disk[n].size=1024u;
            const bool ready=reader!=KUI_RETAIL_READER_STANDARD &&
                leases[lease]==KUI_NATIVE_DUAL_COMPACT_LEASE_BYTES;
            CHECK(prepare(true,reader,false,"/Games/Arena")==(ready?NATIVE_CACHE_READY:NATIVE_CACHE_SKIPPED));
            /* A background reader without a combined module never reads. */
            const unsigned reads=reader==KUI_RETAIL_READER_STANDARD || leases[lease]?1u:0u;
            CHECK(io.reads==reads && io.replays==reads && !io.builds && !io.writes && !io.renames);
            seeded+=ready;
        }
    CHECK(seeded==2u);
}
/* The owner's arena executable (kind 3, no SDK allocator): the compact
 * combined module is seeded for either background reader. A combined module
 * with another lease is not; New then takes the reader-only module, which the
 * executable's SDK SCI family admits. A single-reader module never is. */
static void actual_arena_frontend(uint8_t *boot,size_t bytes) {
    const uint32_t leases[]={KUI_NATIVE_DUAL_COMPACT_LEASE_BYTES,KUI_NATIVE_DUAL_GENERAL_LEASE_BYTES};
    for(unsigned reader=KUI_RETAIL_READER_STANDARD;reader<=KUI_RETAIL_READER_ASYNC_EAGER;reader++)
        for(unsigned lease=0u;lease<2u;lease++) {
            setup(boot,bytes);map.reader=reader;
            const struct kui_native_dual_capability cap={KUI_NATIVE_DUAL_MAGIC0,KUI_NATIVE_DUAL_MAGIC1,
                1u,KUI_NATIVE_DUAL_CAPABILITY_BYTES,KUI_RETAIL_READER_ASYNC,
                KUI_RETAIL_READER_ASYNC_EAGER,leases[lease],0u};
            memcpy(module+3072u,&cap,sizeof(cap));memcpy(before,module,sizeof(module));
            bool standalone=false;source_proof=(struct native_cache_source_proof){0};
            enum native_cache_result result=native_cache_prepare(&game,&map,true,reader,false,&package,
                "/Games/Arena",true,logger,cancelled,&source_proof,&standalone);
            const bool dual=reader!=KUI_RETAIL_READER_STANDARD;
            CHECK(io.opens==io.closed && io.builds==1u && io.renames==1u && source_proof.complete);
            CHECK(result==(dual && !lease?NATIVE_CACHE_READY:NATIVE_CACHE_SKIPPED));
            CHECK(standalone==(lease && reader==KUI_RETAIL_READER_ASYNC));
            if(result!=NATIVE_CACHE_READY) {CHECK(!memcmp(module,before,sizeof(module)));continue;}
            const uint8_t *wire=NULL,*pin=NULL;struct kui_vmu_native_resolution out;
            CHECK(kui_vmu_native_cache_seed_read(module+128u,1120u,&wire,&pin) && get32(wire+20u)==3u);
            CHECK(kui_vmu_native_resolution_cache_replay(boot,bytes,load,wire,1024u,pin,&out));
            CHECK(out.heap.family==KUI_VMU_NATIVE_HEAP_ARENA &&
                  out.call.entry_bytes==KUI_NATIVE_DUAL_COMPACT_LEASE_BYTES);
        }
    free(boot);
}
/* Ready 2 Rumble's SDK pool begins 8 KiB inside the verified program.
 * The same placement decision applies to physical, P1 and P2 addresses.
 * Only the verified prefix stays live; overlap with a reclaimed tail is OK. */
static void pool_placement_tests(void) {
    uint8_t boot[0x9000]={0};
    const uint32_t live=0x8000u;
    const uint32_t aliases[]={0x0c010000u,0x8c010000u,0xac010000u};
    const struct kui_native_dual_capability cap={KUI_NATIVE_DUAL_MAGIC0,KUI_NATIVE_DUAL_MAGIC1,
        1u,KUI_NATIVE_DUAL_CAPABILITY_BYTES,KUI_RETAIL_READER_ASYNC,KUI_RETAIL_READER_ASYNC_EAGER,
        KUI_NATIVE_DUAL_COMPACT_LEASE_BYTES,0u};
    for(unsigned i=0;i<sizeof(aliases)/sizeof(aliases[0]);++i) {
        for(unsigned overlap=0u;overlap<2u;++overlap) {
            setup(boot,sizeof(boot));allow_persistent_cache=false;
            map.reader=KUI_RETAIL_READER_ASYNC;
            memcpy(module+3072u,&cap,sizeof(cap));memcpy(before,module,sizeof(module));
            io.forced_program_bytes=live;io.forced_pool=aliases[i]+live-(overlap?8192u:0u);
            /* 4 MiB, the smallest general pool seen: placement alone decides. */
            io.forced_pool_bytes=0x400000u;
            io.authored_resolution=true;
            CHECK(prepare(true,map.reader,false,"/Games/Placement")==
                (overlap?NATIVE_CACHE_POOL_OVERLAP:NATIVE_CACHE_READY));
            CHECK(io.reads==1u && io.builds==1u && !io.writes && !io.vmu_path_lookups);
            if(overlap)CHECK(!memcmp(before,module,sizeof(module)));
        }
    }
    puts("PASS native pool placement: 8 KiB live overlap refuses every RAM alias; adjacent pool in reclaimed tail admits; single source read");
}
/* Tokyo Xtreme Racer's SDK pool is 448 KiB and its startup asks it for
 * 458,480 bytes: a combined module's lease is not offered where it would take
 * more than a quarter of the pool. The refusal changes no module byte and
 * costs one source read. Overlap is still reported first. */
static void pool_size_tests(void) {
    uint8_t boot[0x9000]={0};
    const uint32_t live=0x8000u,begin=0x8cdb0000u;
    const uint32_t leases[]={KUI_NATIVE_DUAL_COMPACT_LEASE_BYTES,KUI_NATIVE_DUAL_GENERAL_LEASE_BYTES,
        KUI_NATIVE_DUAL_DIAGNOSTIC_LEASE_BYTES};
    unsigned small=0u,ready=0u;
    for(unsigned lease=0u;lease<sizeof(leases)/sizeof(leases[0]);++lease) {
        const struct kui_native_dual_capability cap={KUI_NATIVE_DUAL_MAGIC0,KUI_NATIVE_DUAL_MAGIC1,
            1u,KUI_NATIVE_DUAL_CAPABILITY_BYTES,KUI_RETAIL_READER_ASYNC,KUI_RETAIL_READER_ASYNC_EAGER,
            leases[lease],0u};
        const uint32_t edge=4u*(leases[lease]+32u);
        /* Buzz 2000, Tokyo Xtreme Racer, Pro Yakyu; the edge; 4 MiB and 11 MiB. */
        const uint32_t pools[]={0x31ec0u,0x70000u,0x80000u,edge-32u,edge-1u,edge,edge+32u,0x400000u,0xb00000u};
        for(unsigned pool=0u;pool<sizeof(pools)/sizeof(pools[0]);++pool)
            for(unsigned reader=KUI_RETAIL_READER_ASYNC;reader<=KUI_RETAIL_READER_ASYNC_EAGER;++reader) {
                setup(boot,sizeof(boot));allow_persistent_cache=false;map.reader=reader;
                memcpy(module+3072u,&cap,sizeof(cap));memcpy(before,module,sizeof(module));
                io.forced_program_bytes=live;io.forced_pool=begin;io.forced_pool_bytes=pools[pool];
                io.authored_resolution=true;
                const bool expected=pools[pool]<edge;
                CHECK(prepare(true,reader,false,"/Games/Pool")==
                    (expected?NATIVE_CACHE_POOL_SMALL:NATIVE_CACHE_READY));
                CHECK(io.reads==1u && io.builds==1u && !io.writes && !io.vmu_path_lookups);
                if(expected)CHECK(!memcmp(before,module,sizeof(module)));
                small+=expected;ready+=!expected;
            }
    }
    CHECK(small==30u && ready==24u);
    /* A small pool that also overlaps the program keeps the overlap answer. */
    const struct kui_native_dual_capability compact={KUI_NATIVE_DUAL_MAGIC0,KUI_NATIVE_DUAL_MAGIC1,
        1u,KUI_NATIVE_DUAL_CAPABILITY_BYTES,KUI_RETAIL_READER_ASYNC,KUI_RETAIL_READER_ASYNC_EAGER,
        KUI_NATIVE_DUAL_COMPACT_LEASE_BYTES,0u};
    setup(boot,sizeof(boot));allow_persistent_cache=false;map.reader=KUI_RETAIL_READER_ASYNC;
    memcpy(module+3072u,&compact,sizeof(compact));memcpy(before,module,sizeof(module));
    io.forced_program_bytes=live;io.forced_pool=0x8c010000u+live-8192u;io.forced_pool_bytes=0x70000u;
    io.authored_resolution=true;
    CHECK(prepare(true,map.reader,false,"/Games/Pool")==NATIVE_CACHE_POOL_OVERLAP);
    /* With Virtual VMU On a refusal for the pool saves no record, like a
     * refusal for overlap; a 4 MiB pool saves its record as before. */
    for(unsigned general=0u;general<2u;++general) {
        setup(boot,sizeof(boot));map.reader=KUI_RETAIL_READER_ASYNC;
        memcpy(module+3072u,&compact,sizeof(compact));memcpy(before,module,sizeof(module));
        io.forced_program_bytes=live;io.forced_pool=begin;io.forced_pool_bytes=general?0x400000u:0x70000u;
        io.authored_resolution=true;
        CHECK(prepare(true,map.reader,false,"/Games/Pool")==(general?NATIVE_CACHE_READY:NATIVE_CACHE_POOL_SMALL));
        CHECK(io.reads==1u && io.builds==1u && io.renames==general && (io.writes!=0u)==general);
    }
    /* A record saved earlier (by the Standard module's launch) answers the
     * same way. The Standard module has its own lease and is not held to the
     * combined module's; a reviewed fixed arena is no pool. */
    uint8_t image[1024];for(unsigned i=0;i<sizeof(image);i++)image[i]=(uint8_t)(i*37u+5u);
    for(unsigned fixed=0u;fixed<2u;++fixed)
        for(unsigned reader=KUI_RETAIL_READER_STANDARD;reader<=KUI_RETAIL_READER_ASYNC_EAGER;++reader) {
            setup(image,sizeof(image));map.reader=reader;
            if(reader!=KUI_RETAIL_READER_STANDARD)memcpy(module+3072u,&compact,sizeof(compact));
            memcpy(before,module,sizeof(module));
            char path[120];cache_path(path,image,sizeof(image));unsigned n=node_add(path,false);
            struct kui_vmu_native_resolution source;mock(image,sizeof(image),&source);
            source.call.entry_begin=begin;source.call.entry_bytes=0x70000u;
            if(fixed) {
                source.heap.initializer_slot=0u;source.heap.initializer_target=0u;
                source.call.return_pr=source.call.call+4u;
            }
            CHECK(kui_vmu_native_resolution_cache_encode(&source,disk[n].data,1024u));
            CHECK(get32(disk[n].data+20u)==(fixed?2u:1u));disk[n].size=1024u;
            const bool dual=reader!=KUI_RETAIL_READER_STANDARD;
            CHECK(prepare(true,reader,false,"/Games/Pool")==
                (fixed?(dual?NATIVE_CACHE_READY:NATIVE_CACHE_SKIPPED):
                 (dual?NATIVE_CACHE_POOL_SMALL:NATIVE_CACHE_READY)));
            CHECK(io.reads==1u && io.replays==1u && !io.builds && !io.writes && !io.renames);
        }
    puts("PASS native pool size: a lease over a quarter of the SDK pool is refused for the combined module at three lease sizes; edge, fixed arena, Standard module and saved records keep their answers");
}
/* The combined module's stage repeats this complete check on the loaded
 * executable and stops after the bootstrap where it fails. An unproved extent
 * or an unrecognized SDK is therefore the preflight's answer for either
 * Background reader and Virtual VMU setting: one source read, the module
 * unchanged, nothing saved. The Standard module's launch is not refused. */
static void unsupported_tests(void) {
    uint8_t boot[4097];for(unsigned i=0;i<sizeof(boot);i++)boot[i]=(uint8_t)(i*43u+3u);
    const struct kui_native_dual_capability cap={KUI_NATIVE_DUAL_MAGIC0,KUI_NATIVE_DUAL_MAGIC1,
        1u,KUI_NATIVE_DUAL_CAPABILITY_BYTES,KUI_RETAIL_READER_ASYNC,KUI_RETAIL_READER_ASYNC_EAGER,
        KUI_NATIVE_DUAL_COMPACT_LEASE_BYTES,0u};
    unsigned refused=0u;
    for(unsigned extent=0u;extent<2u;++extent)
        for(unsigned reader=KUI_RETAIL_READER_STANDARD;reader<=KUI_RETAIL_READER_ASYNC_EAGER;++reader)
            for(unsigned vmu=0u;vmu<2u;++vmu) {
                const bool dual=reader!=KUI_RETAIL_READER_STANDARD;
                setup(boot,sizeof(boot));map.reader=reader;allow_persistent_cache=vmu;
                if(dual)memcpy(module+3072u,&cap,sizeof(cap));
                memcpy(before,module,sizeof(module));io.fail_extent=extent;
                CHECK(prepare(true,dual?reader:0u,false,"/Games/Unknown")==
                    (dual?NATIVE_CACHE_UNSUPPORTED:NATIVE_CACHE_SKIPPED));
                CHECK(source_proof.complete && io.reads==1u && io.extent_calls==1u &&
                      io.builds==(extent?0u:1u) && !io.writes && !io.renames && !io.replays);
                CHECK(!memcmp(before,module,sizeof(module)));
                refused+=dual;
            }
    CHECK(refused==8u);
    /* No answer: a module without a seed is never asked, and a source that
     * could not be read leaves the decision to the stage as before. */
    setup(boot,sizeof(boot));map.reader=KUI_RETAIL_READER_ASYNC;
    memset(module,0x59,sizeof(module));memcpy(module+3072u,&cap,sizeof(cap));memcpy(before,module,sizeof(module));
    CHECK(prepare(true,KUI_RETAIL_READER_ASYNC,false,"/Games/Unknown")==NATIVE_CACHE_SKIPPED && !io.reads);
    setup(boot,sizeof(boot));map.reader=KUI_RETAIL_READER_ASYNC;
    memcpy(module+3072u,&cap,sizeof(cap));memcpy(before,module,sizeof(module));io.fail=FAIL_BOOT_READ;
    CHECK(prepare(true,KUI_RETAIL_READER_ASYNC,false,"/Games/Unknown")==NATIVE_CACHE_SKIPPED);
    CHECK(io.reads==1u && !io.builds && !source_proof.complete);
    puts("PASS native unsupported executable: unproved extent or unrecognized SDK answers for the combined module; Standard, seedless and unread launches keep theirs");
}
/* The launcher's answer for every result, Virtual VMU setting, reader-only
 * choice and installed state. A pool refusal speaks unless Virtual VMU is
 * Off and the reader-only module can run the game. An executable the SDK
 * check does not recognize speaks unless the reader-only module can run it,
 * which it does with Virtual VMU On too. Every other result is silent. */
static void refusal_tests(void) {
    static const char pool[]="This game's memory pool is too small for the Background worker.";
    static const char sdk[]="Background loading cannot recognize this game's system library.";
    unsigned spoken=0u,silent=0u;
    for(int result=NATIVE_CACHE_SKIPPED;result<=NATIVE_CACHE_UNSUPPORTED;++result)
        for(unsigned vmu=0u;vmu<2u;++vmu)for(unsigned reader_only=0u;reader_only<2u;++reader_only)
            for(int installed=-1;installed<=1;++installed) {
                const char *said=native_cache_refusal((enum native_cache_result)result,vmu,reader_only,installed);
                const char *expected=NULL;
                if(result==NATIVE_CACHE_POOL_SMALL) {
                    if(vmu)expected=reader_only?" Set Virtual VMU Off; saves then need a real VMU":
                        " Set Background loading Off and Virtual VMU Off; saves then need a real VMU";
                    else if(!reader_only)expected=" Set Background loading Off";
                    else if(!installed)expected=" Install the standalone New reader, or set Background loading Off";
                }
                if(result==NATIVE_CACHE_UNSUPPORTED) {
                    if(!reader_only)expected=vmu?
                        " Set Background loading Off and Virtual VMU Off; saves then need a real VMU":
                        " Set Background loading Off";
                    else if(!installed)expected=" Install the standalone New reader, or set Background loading Off";
                }
                if(!expected) {CHECK(!said);++silent;continue;}
                /* "Retail boot: " and the first sentence fill a 76-column line. */
                const char *first=result==NATIVE_CACHE_POOL_SMALL?pool:sdk;
                CHECK(said && strlen(first)==76u-13u && !strncmp(said,first,strlen(first)));
                CHECK(!strcmp(said+strlen(first),expected) && strlen(expected)<=76u);
                ++spoken;
            }
    CHECK(spoken==18u && silent==66u);
    puts("PASS native refusal text: a pool or SDK refusal names the setting that starts the game for each Virtual VMU and reader-only state; every other result is silent");
}
/* Exercise a new launch with the disk left intact but fresh package and RAM.
 * Both the source proof returned to the GUI and the seed validated by the
 * detached reader must still describe the original bytes after any cache hit. */
static unsigned repeat_launch(const uint8_t *boot,size_t bytes,const char *root,bool authored,
    bool vmu_on,unsigned expected_scans,bool accepted) {
    memset(&io,0,sizeof(io));io.boot=boot;io.bytes=bytes;io.authored_resolution=authored;
    memcpy(module,before,sizeof(module));
    struct native_repeat_record *pending=NULL;
    bool standalone=false;
    struct native_cache_source_proof proof={0};
    enum native_cache_result result=native_cache_prepare_repeat(&game,&map,true,map.reader,false,
        &package,root,vmu_on,logger,cancelled,&proof,&standalone,&pending);
    /* The combined module is selected throughout: an executable its check
     * does not recognize is unsupported, never a silent unseeded launch. */
    CHECK(result==(accepted?NATIVE_CACHE_READY:NATIVE_CACHE_UNSUPPORTED));
    CHECK(io.reads==1u && io.extent_calls==expected_scans && io.builds==expected_scans);
    CHECK(proof.complete && proof.crc32==kui_crc32(0,boot,bytes));
    uint8_t expected[32];digest(boot,bytes,expected);CHECK(!memcmp(proof.sha256,expected,32u));
    if(accepted) {
        const uint8_t *wire=NULL,*pin=NULL;struct kui_vmu_native_resolution out;
        CHECK(!standalone && kui_vmu_native_cache_seed_read(module+128u,
            KUI_VMU_NATIVE_CACHE_SEED_BYTES,&wire,&pin));
        size_t program_bytes;CHECK(kui_vmu_native_program_extent(boot,bytes,load,&program_bytes));
        CHECK(kui_vmu_native_resolution_cache_replay(boot,program_bytes,load,wire,1024u,pin,&out));
    } else CHECK(!pending && !memcmp(before,module,sizeof(module)));
    CHECK(!io.writes && !io.renames);
    if(pending) {native_repeat_publish(pending,logger,cancelled);free(pending);}
    CHECK(!io.vmu_path_lookups && io.opens==io.closed);
    return io.renames;
}
static void repeat_tests(uint8_t *boot,size_t bytes,bool authored) {
    const struct kui_native_dual_capability cap={KUI_NATIVE_DUAL_MAGIC0,KUI_NATIVE_DUAL_MAGIC1,
        1u,KUI_NATIVE_DUAL_CAPABILITY_BYTES,KUI_RETAIL_READER_ASYNC,
        KUI_RETAIL_READER_ASYNC_EAGER,KUI_NATIVE_DUAL_COMPACT_LEASE_BYTES,0u};
    setup(boot,bytes);map.reader=KUI_RETAIL_READER_ASYNC;
    memcpy(module+3072u,&cap,sizeof(cap));memcpy(before,module,sizeof(module));
    memcpy(package.info.build,"0123456789ab",13u);package.info.crc32=kui_crc32(0,module,sizeof(module));
    const char *root="/Games/Native";
    CHECK(repeat_launch(boot,bytes,root,authored,false,1u,true)==1);
    CHECK(repeat_launch(boot,bytes,root,authored,false,0u,true)==0);
    unsigned saved=0u;
    while(saved<20u && !(disk[saved].exists && !disk[saved].directory &&
          !strncmp(disk[saved].path,"0:/KUI/GMCACHE/",15u)))++saved;
    CHECK(saved<20u);
    char stored_path[128];strcpy(stored_path,disk[saved].path);
    const struct fake_node original=disk[saved];
    for(unsigned damage=0;damage<4u;damage++) {
        int found=node_find(stored_path);CHECK(found>=0);saved=(unsigned)found;
        disk[saved]=original;
        if(damage==0u)disk[saved].size--;
        if(damage==1u)disk[saved].data[0]^=1u;
        if(damage==2u)disk[saved].directory=true;
        if(damage==3u)disk[saved].data[disk[saved].size-1u]^=1u;
        const struct fake_node damaged=disk[saved];
        bool owned=damage==3u;
        CHECK(repeat_launch(boot,bytes,root,authored,false,1u,true)==(unsigned)owned);
        if(owned)CHECK(repeat_launch(boot,bytes,root,authored,false,0u,true)==0);
        else CHECK(!memcmp(&damaged,&disk[saved],sizeof(damaged)));
    }
    /* An identical reader with VMU on may consume an existing Off record
     * after the GUI's separate advisory lookup misses. It must not publish. */
    CHECK(repeat_launch(boot,bytes,root,authored,true,0u,true)==0);
    const char *conflicts[]={"0:/kui/gmcache","/KUI/GMCACHE/disc","/KUI/VMU"};
    for(unsigned i=0;i<sizeof(conflicts)/sizeof(conflicts[0]);++i) {
        CHECK(repeat_launch(boot,bytes,conflicts[i],authored,false,1u,true)==0);
        CHECK(!io.stats && !io.opens && !io.mkdirs);
    }
    ++package.info.crc32;
    CHECK(repeat_launch(boot,bytes,root,authored,false,1u,true)==1);
    --package.info.crc32;
    map.reader=KUI_RETAIL_READER_ASYNC_EAGER;
    CHECK(repeat_launch(boot,bytes,root,authored,false,1u,true)==1);
    map.reader=KUI_RETAIL_READER_ASYNC;
    CHECK(repeat_launch(boot,bytes,root,authored,false,0u,true)==0);
    /* A different executable under the same GDI/path cannot inherit a
     * prior positive decision, even if it keeps the old executable length. */
    uint8_t *replacement=calloc(1,bytes);CHECK(replacement);
    CHECK(repeat_launch(replacement,bytes,root,false,false,1u,false)==0);
    free(replacement);
    if(!authored)printf("Original repeat: %zu original bytes read and CRC checked on every launch; "
        "GUI lifetime/discovery 1/1 cold, 0/0 warm; corruption and replacement missed\n",bytes);
}
static void actual_frontend(const char *path) {
    FILE *f=fopen(path,"rb");CHECK(f && !fseek(f,0,SEEK_END));long length=ftell(f);CHECK(length>128 && length<=0xc00000);rewind(f);
    size_t bytes=(size_t)length;uint8_t *boot=malloc(bytes);CHECK(boot && fread(boot,1,bytes,f)==bytes && !fclose(f));
    {
        uint8_t wire[1024];size_t program_bytes;
        CHECK(kui_vmu_native_program_extent(boot,bytes,load,&program_bytes));
        if(kui_vmu_native_resolution_cache_build(boot,program_bytes,load,wire,sizeof(wire),NULL) &&
           get32(wire+20u)==3u) {actual_arena_frontend(boot,bytes);return;}
    }
    setup(boot,bytes);CHECK(prepare(true,0u,false,"/Games/PS2")==NATIVE_CACHE_READY);
    CHECK(io.builds==1u && io.writes==1u && io.syncs==1u && io.renames==1u);
    const unsigned cold_cancel_visits=io.cancel_checks;
    char name[120];cache_path(name,boot,bytes);int stored=node_find(name);CHECK(stored>=0 && disk[stored].size==1024u);
    setup(boot,bytes);seed_cache(boot,bytes,true);
    CHECK(prepare(true,0u,false,"/Games/PS2")==NATIVE_CACHE_READY);
    CHECK(io.replays==1u && !io.builds && !io.writes && !io.renames);
    size_t program_bytes;CHECK(kui_vmu_native_program_extent(boot,bytes,load,&program_bytes));
    if(bytes-program_bytes==0x8000u) {
        /* A cache cannot authorize a changed or malformed source footer. */
        uint8_t saved=boot[bytes-1u];boot[bytes-1u]^=1u;setup(boot,bytes);
        CHECK(prepare(true,0u,false,"/Games/PS2")==NATIVE_CACHE_SKIPPED);
        CHECK(!io.builds && !io.replays && !io.writes && !io.renames);
        boot[bytes-1u]=saved;
    } else if(program_bytes!=bytes) {
        /* A reclaimed arena tail becomes heap once the game runs, but it is
         * still source: a changed tail names another record, so the stored
         * proof is never replayed and a fresh one is published. */
        setup(boot,bytes);seed_cache(boot,bytes,true);
        uint8_t saved=boot[bytes-1u];boot[bytes-1u]^=1u;
        CHECK(prepare(true,0u,false,"/Games/PS2")==NATIVE_CACHE_READY);
        CHECK(io.builds==1u && !io.replays && io.renames==1u);
        boot[bytes-1u]=saved;
    }
    /* Corrupt, truncated, old epoch and other-image records all rebuild and
     * replace the stale name; successful next launch no longer rescans. */
    for(unsigned corruption=0;corruption<4u;corruption++) {
        setup(boot,bytes);unsigned n=seed_cache(boot,bytes,true);
        if(corruption==0u) disk[n].data[600]^=1u;
        if(corruption==1u) disk[n].size--;
        if(corruption==2u) {disk[n].data[16]++;seal(disk[n].data);}
        if(corruption==3u) {disk[n].data[32]^=1u;seal(disk[n].data);}
        CHECK(prepare(true,0u,false,"/Games/PS2")==NATIVE_CACHE_READY && io.builds==1u && io.renames==1u);
        CHECK(node_find(name)>=0);
    }
    for(unsigned failure=FAIL_OPEN;failure<=FAIL_MKDIR;failure++) {
        setup(boot,bytes);io.fail=failure;
        CHECK(prepare(true,0u,false,"/Games/PS2")==NATIVE_CACHE_READY && io.builds==1u);
        CHECK(node_find(name)<0); /* no partial canonical cache publication */
    }
    const char *conflicts[]={"/KUI/VMU","/kui/vmu/native-cache/v1","/KUI/VMU/NATIVE~1/V1"};
    for(unsigned i=0;i<sizeof(conflicts)/sizeof(conflicts[0]);i++) {
        setup(boot,bytes);CHECK(prepare(true,0u,false,conflicts[i])==NATIVE_CACHE_READY);
        CHECK(io.builds==1u && !io.opens && !io.writes && !io.renames);
    }
    for(unsigned cancel_at=1u;cancel_at<=cold_cancel_visits;cancel_at++) {
        setup(boot,bytes);io.cancel_at=cancel_at;
        CHECK(prepare(true,0u,false,"/Games/PS2")==NATIVE_CACHE_CANCELLED);
    }
    for(unsigned boundary=1u;boundary<=8u;boundary++) {
        setup(boot,bytes);
        if(boundary==7u) {unsigned n=seed_cache(boot,bytes,true);disk[n].data[600]^=1u;}
        io.cancel_when=boundary;
        CHECK(prepare(true,0u,false,"/Games/PS2")==NATIVE_CACHE_CANCELLED);
        CHECK(io.renames==(boundary==8u?1u:0u)); /* only a complete cache file may have been renamed */
    }
    /* Both native background choices share the actual detector/cache path,
     * while their distinct wire reader values remain bound to the request. */
    const struct kui_native_dual_capability cap={KUI_NATIVE_DUAL_MAGIC0,KUI_NATIVE_DUAL_MAGIC1,
        1u,KUI_NATIVE_DUAL_CAPABILITY_BYTES,KUI_RETAIL_READER_ASYNC,
        KUI_RETAIL_READER_ASYNC_EAGER,KUI_NATIVE_DUAL_LEASE_BYTES,0u};
    for(unsigned reader=KUI_RETAIL_READER_ASYNC;reader<=KUI_RETAIL_READER_ASYNC_EAGER;reader++) {
        setup(boot,bytes);map.reader=reader;memcpy(module+3072u,&cap,sizeof(cap));
        memcpy(before,module,sizeof(module));
        CHECK(prepare(true,reader,false,"/Games/PS2")==NATIVE_CACHE_READY);
        CHECK(io.builds==1u && io.writes==1u && io.syncs==1u && io.renames==1u);
        setup(boot,bytes);seed_cache(boot,bytes,true);map.reader=reader;
        memcpy(module+3072u,&cap,sizeof(cap));memcpy(before,module,sizeof(module));
        CHECK(prepare(true,reader,false,"/Games/PS2")==NATIVE_CACHE_READY);
        CHECK(io.replays==1u && !io.builds && !io.writes && !io.renames);
        size_t program_bytes;CHECK(kui_vmu_native_program_extent(boot,bytes,load,&program_bytes));
        const size_t tail=program_bytes==bytes?0u:program_bytes%KUI_SHA256_PREFIX_STRIDE;
        CHECK(io.source_sha_bytes==bytes+tail);
        CHECK(io.source_sha_calls==(bytes+KUI_SHA256_PREFIX_STRIDE-1u)/KUI_SHA256_PREFIX_STRIDE+
            (program_bytes!=bytes) && io.extent_calls==1u);
        printf("  Native hot cache SHA: %s reader=%u source_bytes=%zu program_bytes=%zu before_bytes=%zu after_bytes=%zu fresh_reads=%u extent_calls=%u\n",
            path,reader,bytes,program_bytes,bytes+program_bytes,io.source_sha_bytes,io.reads,io.extent_calls);
    }
    /* VMU Off keeps both Background readers' complete executable proof in
     * RAM, and never even stats the VMU/cache tree. A damaged old cache is
     * deliberately present but must be irrelevant to this route. */
    for(unsigned reader=KUI_RETAIL_READER_STANDARD;reader<=KUI_RETAIL_READER_ASYNC_EAGER;reader++) {
        setup(boot,bytes);map.reader=reader;allow_persistent_cache=false;
        if(reader)memcpy(module+3072u,&cap,sizeof(cap));
        memcpy(before,module,sizeof(module));
        unsigned stale=seed_cache(boot,bytes,true);disk[stale].data[0]^=1u;
        io.fail=FAIL_OPEN;
        CHECK(prepare(true,reader,false,"/Games/PS2")==NATIVE_CACHE_READY);
        CHECK(io.builds==1u && !io.replays && !io.stats && !io.mkdirs && !io.opens &&
            !io.writes && !io.syncs && !io.renames && !io.unlinks);
    }
    repeat_tests(boot,bytes,false);free(boot);
}
#ifndef KUI_GAMES_NATIVE_CACHE_TEST_MAIN
#define KUI_GAMES_NATIVE_CACHE_TEST_MAIN main
#endif
static void advisory_alias_publication_faults(void) {
    uint8_t boot[1024],wire[1024],compare[1024];
    for(unsigned i=0u;i<sizeof(boot);i++)boot[i]=(uint8_t)(i*17u+1u);
    struct kui_vmu_native_resolution record;mock(boot,sizeof(boot),&record);
    CHECK(kui_vmu_native_resolution_cache_encode(&record,wire,sizeof(wire)));
    static const char path[]="0:/KUI/VMU/native-cache/v8/hints/0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef.nvr";
    for(unsigned fault=FAIL_NONE;fault<=FAIL_MKDIR;fault++) {
        setup(boot,sizeof(boot));CHECK(f_mkdir("0:/KUI/VMU/native-cache/v8/hints")==FR_OK);
        io.fail=fault;
        bool saved=native_cache_file_publish(path,wire,compare,cancelled);
        CHECK(saved==(fault==FAIL_NONE) && io.opens==io.closed);
        int final=node_find(path);
        if(saved)CHECK(final>=0 && disk[final].size==1024u && !memcmp(disk[final].data,wire,1024u));
        else CHECK(final<0);
    }
    for(unsigned cancel_when=1u;cancel_when<=8u;cancel_when++) {
        setup(boot,sizeof(boot));CHECK(f_mkdir("0:/KUI/VMU/native-cache/v8/hints")==FR_OK);
        io.cancel_when=cancel_when;
        (void)native_cache_file_publish(path,wire,compare,cancelled);
        int final=node_find(path);
        CHECK(io.opens==io.closed);
        CHECK(final<0 || (disk[final].size==1024u && !memcmp(disk[final].data,wire,1024u)));
    }
    puts("PASS actual advisory alias publisher: nested path, temporary/sync/exact readback/rename, every I/O failure and cancellation boundary; no partial final record");
}
int KUI_GAMES_NATIVE_CACHE_TEST_MAIN(int argc,char **argv) {
    CHECK(argc>=1 && argc<=4);codec_and_trust_tests();gates_and_hits();dual_gates_and_hits();
    fixed_arena_needs_dual();arena_needs_compact_dual();pool_placement_tests();pool_size_tests();unsupported_tests();refusal_tests();
    advisory_alias_publication_faults();
    uint8_t authored[1024];
    for(unsigned i=0;i<sizeof(authored);i++)authored[i]=(uint8_t)(i*17u+1u);
    repeat_tests(authored,sizeof(authored),true);
    for(int i=1;i<argc;i++) actual_frontend(argv[i]);
    printf("Native launcher cache: %u checks passed%s\n",checks,argc>1?" including actual-image rebuild/publication faults":"");return 0;
}
