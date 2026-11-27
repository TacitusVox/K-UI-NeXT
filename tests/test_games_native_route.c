/* SPDX-License-Identifier: GPL-3.0-only */
/* The actual Games launcher and FatFs on a card image. A synthetic executable
 * never passes the real SDK check, so that check's answer is the one explicit
 * adversary here: an authored record whose SDK pool is 448 KiB, or the real
 * refusal. The card can also hold one saved record of another game, the one
 * state in which a scan would offer it as this game's. A "large" launch is
 * refused the memory for the whole program, as the console refuses it for a
 * program larger than K-UI's free memory; the launcher then reads the
 * program in pieces. This decides routes and their messages. It makes no
 * claim about a console or a game. */
#define main retail_fixture_main
#include "games_retail.c"
#undef main
#define main codec_fixture_main
#include "test_vmu_native_resolution_cache.c"
#undef main
#include "kui/vmu_native_cache_seed.h"
#include "kui/games_retail_refusal.h"
static const char *pool;
static unsigned builds,refused_allocations;
static bool large;
/* The launcher is built with -DKUI_NATIVE_CACHE_BOOT_ALLOC=route_boot_alloc:
 * its one allocation for the whole program comes here. */
void *route_boot_alloc(size_t bytes) {
    assert(test.active && bytes==4096u);
    if(!large)return malloc(bytes);
    ++refused_allocations;return NULL;
}
bool __real_kui_vmu_native_resolution_cache_build_prefixed(const void *,size_t,uint32_t,void *,size_t,
    struct kui_vmu_native_resolution *,const struct kui_sha256_prefix *);
bool __wrap_kui_vmu_native_resolution_cache_build_prefixed(const void *source,size_t bytes,uint32_t address,
    void *wire,size_t wire_bytes,struct kui_vmu_native_resolution *out,const struct kui_sha256_prefix *marks) {
    assert(test.active && (bytes==3001u || bytes==4096u) && address==load && wire_bytes==1024u && out);
    assert(marks && marks->source==source && marks->bytes==bytes);
    ++builds;
    assert(!__real_kui_vmu_native_resolution_cache_build_prefixed(source,bytes,address,wire,wire_bytes,out,marks));
    if(!strcmp(pool,"unresolved"))return false;
    assert(!strcmp(pool,"small"));
    struct kui_vmu_native_resolution record;mock(source,bytes,&record);
    record.heap.initializer_slot=0x8c010100u;
    /* Tokyo Xtreme Racer's pool. */
    record.call.entry_begin=0x8cdb0000u;record.call.entry_bytes=0x70000u;
    assert(!kui_vmu_native_pool_overlaps_program(&record));
    assert(kui_vmu_native_resolution_cache_encode(&record,wire,wire_bytes));*out=record;return true;
}
/* Another game's record, as a launch of it with Virtual VMU On saved it: a
 * smaller program with an ordinary pool, alone in the cache directory. */
static void save_other_game(void) {
    static uint8_t other[1024],wire[1024];
    for(unsigned i=0u;i<sizeof(other);i++)other[i]=(uint8_t)(i*53u+9u);
    struct kui_vmu_native_resolution record;mock(other,sizeof(other),&record);
    record.heap.initializer_slot=0x8c010100u;
    record.call.entry_begin=0x8c800000u;record.call.entry_bytes=0x400000u;
    assert(kui_vmu_native_resolution_cache_encode(&record,wire,sizeof(wire)));
    const char *const folders[]={"0:/KUI/VMU","0:/KUI/VMU/native-cache","0:/KUI/VMU/native-cache/v8"};
    for(unsigned i=0u;i<3u;i++) {FRESULT made=f_mkdir(folders[i]);assert(made==FR_OK || made==FR_EXIST);}
    char path[128]="0:/KUI/VMU/native-cache/v8/";size_t at=strlen(path);
    for(unsigned i=0u;i<32u;i++)snprintf(path+at+2u*i,3u,"%02x",record.driver.image_sha256[i]);
    strcpy(path+at+64u,".nvr");
    write_file(path,wire,sizeof(wire));
}
int main(int argc,char **argv) {
    assert(argc==8);pool=argv[5];
    large=!strcmp(argv[7],"large");assert(large || !strcmp(argv[7],"whole"));
    const bool other_game=!strcmp(argv[6],"other-game");assert(other_game || !strcmp(argv[6],"empty"));
    struct stat st;assert(!lstat(argv[1],&st));test.image=fopen(argv[1],"r+b");assert(test.image);
    test.blocks=(uint64_t)st.st_size/512u;test.fault=argv[4];
    assert(native_standalone_case());
    if(!strcmp(argv[3],"seed")) {
        kui_media_set(&media);assert(kui_mount(&fs,log_line));seed(argv[2]);
        /* The host launcher has no virtual-card code and refuses a Background
         * launch with Virtual VMU Off before the SDK check: On throughout. */
        assert(kui_vmu_emulation_settings_save(true,log_line));
        if(other_game)save_other_game();
        assert(f_mount(NULL,"0:",0)==FR_OK);kui_media_set(NULL);
    } else {
        assert(!strcmp(argv[3],"check"));
        test.active=true;test.hash_begin[0]=0u;test.hash_end[0]=16u*track_stride(2);
        test.hash_begin[1]=21u*track_stride(2);test.hash_end[1]=23u*track_stride(2);test.hash_bounded=true;
        const uint32_t reader=strstr(test.fault,"-y")?KUI_RETAIL_READER_ASYNC_EAGER:KUI_RETAIL_READER_ASYNC;
        struct kui_runtime_image staged={0};struct kui_game_ram_patches patches={0};
        const bool prepared=kui_games_retail_prepare_launch(selected,reader,&patches,&staged,log_line,cancel);
        assert(!test.files && !test.connected && test.connects==test.disconnects);
        /* One source read answers the SDK check; a refusal reads no more.
         * A program that cannot be held is read once all the same, never
         * reaches the SDK check, and its CRC needs no second pass. */
        assert(test.hash_reads[1]==1u && !test.direct_crc_passes && refused_allocations==(large?1u:0u));
        if(large)assert(test.native_piece_passes==1u && !test.native_source_passes && !builds && !test.family_calls);
        else assert(test.native_source_passes==1u && !test.native_piece_passes && builds==1u);
        bool standalone=false,seeded=false;
        if(prepared) {
            assert(staged.data && staged.info.payload_bytes);
            standalone=kui_native_standalone_descriptor_find(staged.data,staged.info.payload_bytes,NULL)==1;
            assert(standalone!=(kui_native_dual_descriptor_find(staged.data,staged.info.payload_bytes,NULL)==1));
            size_t slot=kui_vmu_native_cache_seed_find(staged.data,staged.info.payload_bytes);
            const uint8_t *wire,*pin;
            seeded=slot!=SIZE_MAX && kui_vmu_native_cache_seed_read((uint8_t *)staged.data+slot,
                KUI_VMU_NATIVE_CACHE_SEED_BYTES,&wire,&pin);
            /* A prepared launch leaves no reason behind. */
            assert(!kui_games_retail_last_refusal()[0]);
        } else {
            assert(!staged.data && !staged.info.payload_bytes && !test.writes);
            /* The reason the console's worker repeats after the timing lines. */
            assert(strlen(kui_games_retail_last_refusal())>76u-13u);
            printf("REFUSAL %s\n",kui_games_retail_last_refusal());
        }
        printf("ROUTE prepared=%u standalone=%u seeded=%u family=%u standalone-opens=%u writes=%u\n",prepared,standalone,
            seeded,test.family_calls,test.standalone_package_opens,test.writes?1u:0u);
        kui_runtime_free(&staged);
    }
    assert(!fclose(test.image));return 0;
}
