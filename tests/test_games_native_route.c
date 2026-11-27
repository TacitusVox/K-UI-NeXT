/* SPDX-License-Identifier: GPL-3.0-only */
/* The actual Games launcher and FatFs on a card image. A synthetic executable
 * never passes the real SDK check, so that check's answer is the one explicit
 * adversary here: an authored record whose SDK pool is 448 KiB. This decides
 * routes and their messages. It makes no claim about a console or a game. */
#define main retail_fixture_main
#include "games_retail.c"
#undef main
#define main codec_fixture_main
#include "test_vmu_native_resolution_cache.c"
#undef main
#include "kui/vmu_native_cache_seed.h"
static const char *pool;
static unsigned builds;
bool __real_kui_vmu_native_resolution_cache_build_prefixed(const void *,size_t,uint32_t,void *,size_t,
    struct kui_vmu_native_resolution *,const struct kui_sha256_prefix *);
bool __wrap_kui_vmu_native_resolution_cache_build_prefixed(const void *source,size_t bytes,uint32_t address,
    void *wire,size_t wire_bytes,struct kui_vmu_native_resolution *out,const struct kui_sha256_prefix *marks) {
    assert(test.active && (bytes==3001u || bytes==4096u) && address==load && wire_bytes==1024u && out);
    assert(marks && marks->source==source && marks->bytes==bytes);
    ++builds;
    assert(!__real_kui_vmu_native_resolution_cache_build_prefixed(source,bytes,address,wire,wire_bytes,out,marks));
    assert(!strcmp(pool,"small"));
    struct kui_vmu_native_resolution record;mock(source,bytes,&record);
    record.heap.initializer_slot=0x8c010100u;
    /* Tokyo Xtreme Racer's pool. */
    record.call.entry_begin=0x8cdb0000u;record.call.entry_bytes=0x70000u;
    assert(!kui_vmu_native_pool_overlaps_program(&record));
    assert(kui_vmu_native_resolution_cache_encode(&record,wire,wire_bytes));*out=record;return true;
}
int main(int argc,char **argv) {
    assert(argc==6);pool=argv[5];
    struct stat st;assert(!lstat(argv[1],&st));test.image=fopen(argv[1],"r+b");assert(test.image);
    test.blocks=(uint64_t)st.st_size/512u;test.fault=argv[4];
    assert(native_standalone_case());
    if(!strcmp(argv[3],"seed")) {
        kui_media_set(&media);assert(kui_mount(&fs,log_line));seed(argv[2]);
        /* The host launcher has no virtual-card code and refuses a Background
         * launch with Virtual VMU Off before the SDK check: On throughout. */
        assert(kui_vmu_emulation_settings_save(true,log_line));
        assert(f_mount(NULL,"0:",0)==FR_OK);kui_media_set(NULL);
    } else {
        assert(!strcmp(argv[3],"check"));
        test.active=true;test.hash_begin[0]=0u;test.hash_end[0]=16u*track_stride(2);
        test.hash_begin[1]=21u*track_stride(2);test.hash_end[1]=23u*track_stride(2);test.hash_bounded=true;
        const uint32_t reader=strstr(test.fault,"-y")?KUI_RETAIL_READER_ASYNC_EAGER:KUI_RETAIL_READER_ASYNC;
        struct kui_runtime_image staged={0};struct kui_game_ram_patches patches={0};
        const bool prepared=kui_games_retail_prepare_launch(selected,reader,&patches,&staged,log_line,cancel);
        assert(!test.files && !test.connected && test.connects==test.disconnects);
        /* One source read answers the SDK check; a refusal reads no more. */
        assert(test.native_source_passes==1u && builds==1u && test.hash_reads[1]==1u && !test.direct_crc_passes);
        bool standalone=false,seeded=false;
        if(prepared) {
            assert(staged.data && staged.info.payload_bytes);
            standalone=kui_native_standalone_descriptor_find(staged.data,staged.info.payload_bytes,NULL)==1;
            assert(standalone!=(kui_native_dual_descriptor_find(staged.data,staged.info.payload_bytes,NULL)==1));
            size_t slot=kui_vmu_native_cache_seed_find(staged.data,staged.info.payload_bytes);
            const uint8_t *wire,*pin;
            seeded=slot!=SIZE_MAX && kui_vmu_native_cache_seed_read((uint8_t *)staged.data+slot,
                KUI_VMU_NATIVE_CACHE_SEED_BYTES,&wire,&pin);
        } else assert(!staged.data && !staged.info.payload_bytes && !test.writes);
        printf("ROUTE prepared=%u standalone=%u seeded=%u family=%u standalone-opens=%u writes=%u\n",prepared,standalone,
            seeded,test.family_calls,test.standalone_package_opens,test.writes?1u:0u);
        kui_runtime_free(&staged);
    }
    assert(!fclose(test.image));return 0;
}
