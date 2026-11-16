/* SPDX-License-Identifier: GPL-3.0-only */
#include "retail_storage.h"
#include "kui/retail_image.h"
#include "kui/vmu_native_mode.h"
#include "kui/vmu_launch_package.h"
#include <assert.h>
#include <string.h>

static struct kui_retail_manifest manifest;
static struct kui_retail_storage card;
static struct kui_retail_image image;
static uint8_t raw_boot[KUI_GAME_DATA_BYTES], volume[KUI_GAME_DATA_BYTES], directory[KUI_GAME_DATA_BYTES];
static uint32_t boot_crc, exec_bytes, largest_range;
static unsigned acquire, release;
static bool fail_stop;

static uint32_t native_runtime_mode(void) { return KUI_VMU_NATIVE_MODE_READER_ONLY; }
enum kui_loader_sd_result kui_retail_storage_acquire(struct kui_retail_storage *s) {
    assert(s==&card);++acquire;return KUI_LOADER_SD_OK;
}
enum kui_loader_sd_result kui_retail_storage_stop(struct kui_retail_storage *s) {
    assert(s==&card);return fail_stop?KUI_LOADER_SD_TIMEOUT:KUI_LOADER_SD_OK;
}
void kui_retail_storage_release(struct kui_retail_storage *s) { assert(s==&card);++release; }
enum kui_game_result kui_retail_image_read(struct kui_retail_image *img,uint32_t lba,
    uint32_t count,enum kui_game_sector_format format,void *out,size_t bytes) {
    assert(img==&image && count==1u && format==KUI_GAME_SECTOR_MODE1 && bytes==KUI_GAME_DATA_BYTES);
    if(lba==45016u)memcpy(out,volume,bytes);
    else if(lba==45020u)memcpy(out,directory,bytes);
    else return KUI_GAME_RANGE;
    return KUI_GAME_OK;
}
enum kui_game_result kui_retail_image_check_validated(const struct kui_retail_manifest *m,
    uint32_t lba,uint32_t count,enum kui_game_sector_format format) {
    assert(m==&manifest && format==KUI_GAME_SECTOR_MODE1 && count<=KUI_RETAIL_IMAGE_MAX_SECTORS);
    if(count>largest_range)largest_range=count;
    return lba>=49000u && lba+count<52000u?KUI_GAME_OK:KUI_GAME_RANGE;
}
#include "toy_movie_iso_stage.inc"

static void dual(uint8_t *p,uint32_t value) {
    for(unsigned i=0;i<4u;i++)p[i]=p[7u-i]=(uint8_t)(value>>(i*8u));
}
static unsigned record(uint8_t *p,const char *name,unsigned length,uint32_t lba,
    uint32_t bytes,uint8_t flags) {
    unsigned record_size=33u+length+!(length&1u);
    memset(p,0,record_size);p[0]=record_size;p[25]=flags;p[32]=length;
    dual(p+2,lba);dual(p+10,bytes);memcpy(p+33,name,length);
    return record_size;
}
static void reset(void) {
    memset(toy_movie_ranges,0,sizeof(toy_movie_ranges));
    memset(volume,0,sizeof(volume));memset(directory,0,sizeof(directory));
    manifest.reader=KUI_RETAIL_READER_ASYNC;manifest.session_lba=45000u;
    manifest.ip_crc32=KUI_VMU_LAUNCH_TOY_IP_CRC;boot_crc=KUI_VMU_LAUNCH_TOY_EXEC_CRC;
    exec_bytes=KUI_VMU_LAUNCH_TOY_EXEC_BYTES;
    volume[0]=1u;memcpy(volume+1,"CD001\1",6u);volume[129]=volume[130]=8u;
    record(volume+156,"\0",1u,45020u,2048u,2u);
    const char bumper[]="60hz_Bumper_LRTF.avi;1",intro[]="60HZ_INTRO_LRTF.AVI";
    unsigned next=record(directory,bumper,sizeof(bumper)-1u,49000u,130u*2048u,0u);
    record(directory+next,intro,sizeof(intro)-1u,51000u,340u*2048u+1u,0u);
    acquire=release=largest_range=0u;fail_stop=false;
}
int main(void) {
    reset();toy_movie_scan();
    assert(acquire==1u && release==1u && largest_range==64u);
    assert(!toy_movie_ranges[0].count && toy_movie_ranges[1].lba==49000u && toy_movie_ranges[1].count==130u);
    assert(!toy_movie_ranges[2].count && toy_movie_ranges[3].lba==51000u && toy_movie_ranges[3].count==341u);
    reset();fail_stop=true;toy_movie_scan();assert(release==1u && !toy_movie_ranges[1].count);
    reset();directory[25]=128u;toy_movie_scan();assert(release==1u && !toy_movie_ranges[1].count);
    reset();directory[9]^=1u;toy_movie_scan();assert(release==1u && !toy_movie_ranges[1].count);
    reset();boot_crc=0u;toy_movie_scan();assert(!acquire && !toy_movie_ranges[1].count);
    return 0;
}
