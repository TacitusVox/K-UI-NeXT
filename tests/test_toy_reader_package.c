/* SPDX-License-Identifier: GPL-3.0-only
 * Card Tools' own admission of a Toy test reader module, run on the actual
 * packaged bytes. The installed Card Tools (09c9a4b686d4) loads the module
 * its "66 SCI movie playback" entry names by path and admits it by these
 * checks alone: nothing pins a build, a size or a checksum. The predicates
 * in the headers are the ones that build compiled; the functions marked
 * COPY repeat src/apps/games_retail.c, src/apps/games_vmu_card.inc and
 * src/apps/toy_suite_report_prepare.inc of that build (the source that
 * built it is in the handoff bundle, not in this tree). A module that
 * passes is launched; one that also has exactly one blank report seed gets
 * its report file bound. This is a host check of bytes, not a console
 * launch. */
#include "kui/runtime.h"
#include "kui/retail_loader_layout.h"
#include "kui/mdk2_cdda_layout.h"
#include "kui/retail_image.h"
#include "kui/native_dual_reader.h"
#include "kui/native_session.h"
#include "kui/vmu_native_toggle.h"
#include "kui/vmu_native_mode.h"
#include "kui/vmu_native_cache_seed.h"
#include "kui/toy_suite_report_wire.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
/* stubs for link */
uint32_t kui_crc32(uint32_t crc,const void *data,size_t n){const uint8_t *p=data;crc=~crc;while(n--){crc^=*p++;for(int i=0;i<8;i++)crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);}return ~crc;}
bool kui_vmu_native_resolution_cache_record_digest(const void *w,size_t n,uint8_t d[32]){(void)w;(void)n;(void)d;return false;}
/* COPY games_retail.c:126 layout_codec(ce=false,zso=false) */
static bool layout(const struct kui_runtime_image *image){
    const uint32_t stage=KUI_RETAIL_STAGE_ADDRESS,begin=KUI_RETAIL_STAGE_BLOB_OFFSET,max=KUI_RETAIL_STAGE_MAX_BYTES;
    if(!image->data || image->info.payload_bytes<begin+4 || image->info.payload_bytes>begin+max ||
        image->info.memory_bytes!=image->info.payload_bytes) return false;
    const uint8_t *h=(const uint8_t *)image->data+KUI_RETAIL_HEADER_OFFSET;
    uint32_t n=le32(h+28);const uint32_t resident=le32(h+52),limit=le32(h+56);
    bool known=(resident==KUI_RETAIL_LEGACY_RESIDENT_ADDRESS && limit==KUI_RETAIL_LEGACY_STANDARD_LIMIT) ||
        (resident==KUI_RETAIL_LOW_RESIDENT_ADDRESS && (limit==KUI_RETAIL_LOW_STANDARD_LIMIT || limit==KUI_MDK2_CDDA_LOW_LIMIT));
    if(memcmp(h,KUI_RETAIL_PACKAGE_MAGIC,8) || le32(h+8)!=KUI_RETAIL_PACKAGE_VERSION ||
        le32(h+12)!=KUI_RETAIL_HEADER_BYTES || le32(h+16)!=KUI_RETAIL_MAP_OFFSET ||
        le32(h+20)!=KUI_RETAIL_IMAGE_WIRE_BYTES || le32(h+24)!=stage || !n || n%4 || n>max ||
        begin+n!=image->info.payload_bytes || le32(h+32)!=stage || le32(h+36)!=KUI_RETAIL_EXEC_ADDRESS ||
        le32(h+40)!=KUI_RETAIL_EXEC_MAX_BYTES || le32(h+44)!=KUI_RETAIL_STAGE_STACK ||
        le32(h+48)!=begin || !known || le32(h+60)) return false;
    for(unsigned i=0;i<KUI_RETAIL_IMAGE_WIRE_BYTES;i++) if(((const uint8_t *)image->data)[KUI_RETAIL_MAP_OFFSET+i]) return false;
    return true;
}
/* COPY native_background_route.h:16 (header itself drags FatFs) */
static bool background_matches(uint32_t reader,const void *data,size_t bytes){
    size_t at=0u;if(kui_native_dual_descriptor_find(data,bytes,&at)!=1)return false;
    uint32_t lease=kui_native_dual_u32((const uint8_t *)data+at+24u);
    int session=kui_native_session_descriptor_find(data,bytes,NULL);
    if(session==0 && lease!=KUI_NATIVE_DUAL_COMPACT_LEASE_BYTES)return false;
    if(session==1 && lease!=KUI_NATIVE_DUAL_GENERAL_LEASE_BYTES)return false;
    return reader==KUI_RETAIL_READER_ASYNC?session>=0:reader==KUI_RETAIL_READER_ASYNC_EAGER && session==0;
}
/* COPY games_vmu_card.inc:126 games_card_emulation_mode(enabled=false) */
static const char *emulation_off(uint8_t *payload,size_t bytes,size_t *where){
    int capable=kui_vmu_native_toggle_find(payload,bytes,NULL);
    if(capable!=1)return "VMU Off needs the matching toggle-capable native module";
    size_t slot=SIZE_MAX;static const uint8_t magic[8]={'K','U','I','N','V','M','0','1'};
    for(size_t at=0;bytes>=KUI_VMU_NATIVE_MODE_BYTES && at<=bytes-KUI_VMU_NATIVE_MODE_BYTES;at++){
        if(memcmp(payload+at,magic,sizeof(magic)))continue;
        const uint8_t *wire=payload+at;uint32_t mode=le32(wire+16u);bool reserved=false;
        for(unsigned i=20u;i<KUI_VMU_NATIVE_MODE_BYTES;i++)reserved|=wire[i]!=0;
        if(le32(wire+8u)!=1u || le32(wire+12u)!=KUI_VMU_NATIVE_MODE_BYTES || reserved || mode>KUI_VMU_NATIVE_MODE_READER_ONLY)continue;
        if((mode!=KUI_VMU_NATIVE_MODE_NORMAL && mode!=KUI_VMU_NATIVE_MODE_READER_ONLY) || slot!=SIZE_MAX)return "toggle mode damaged or ambiguous";
        slot=at;
    }
    if(slot==SIZE_MAX)return "toggle mode missing";
    *where=slot;return NULL;
}
/* COPY toy_suite_report_prepare.inc:6 */
static size_t report_slot(const uint8_t *payload,size_t bytes){
    size_t found=SIZE_MAX;
    for(size_t at=0;bytes>=KUI_TOY_REPORT_WIRE_BYTES && at<=bytes-KUI_TOY_REPORT_WIRE_BYTES;at+=4u){
        const uint8_t *p=payload+at;
        if(le32(p)!=KUI_TOY_REPORT_MAGIC0 || le32(p+4u)!=KUI_TOY_REPORT_MAGIC1 || le32(p+8u)!=KUI_TOY_REPORT_VERSION || le32(p+12u)!=KUI_TOY_REPORT_WIRE_BYTES)continue;
        unsigned nonblank=0;for(unsigned i=KUI_TOY_REPORT_TEST_OFFSET;i<KUI_TOY_REPORT_WIRE_BYTES;i++)nonblank|=p[i];
        if(nonblank || found!=SIZE_MAX)return SIZE_MAX;
        found=at;
    }
    return found;
}
int main(int argc,char **argv){
    if(argc!=2)return 2;
    FILE *f=fopen(argv[1],"rb");if(!f)return 2;
    fseek(f,0,SEEK_END);long size=ftell(f);fseek(f,0,SEEK_SET);
    uint8_t *file=malloc((size_t)size);if(fread(file,1,(size_t)size,f)!=(size_t)size)return 2;fclose(f);
    struct kui_runtime_image image={0};int fail=0;
    enum kui_runtime_result r=size>=64?kui_runtime_header(file,(uint64_t)size,&image.info):KUI_RUNTIME_SIZE;
    printf("envelope: %s build=%s payload=%u memory=%u crc=%08x\n",kui_runtime_result_name(r),image.info.build,
        (unsigned)image.info.payload_bytes,(unsigned)image.info.memory_bytes,(unsigned)image.info.crc32);
    if(r!=KUI_RUNTIME_OK)return 1;
    image.data=file+64;size_t n=image.info.payload_bytes;
    bool crc=kui_crc32(0,image.data,n)==image.info.crc32;printf("payload CRC32: %s\n",crc?"ok":"MISMATCH");fail|=!crc;
    bool lay=layout(&image);printf("layout_codec: %s\n",lay?"ok":"REFUSED (unsupported game-launch package layout)");fail|=!lay;
    size_t at=0;int dual=kui_native_dual_descriptor_find(image.data,n,&at);
    printf("dual descriptor: %d",dual);if(dual==1)printf(" at 0x%zx lease=0x%x",at,(unsigned)le32((uint8_t *)image.data+at+24));printf("\n");fail|=dual!=1;
    int session=kui_native_session_descriptor_find(image.data,n,NULL);printf("session descriptor: %d\n",session);
    bool match=background_matches(KUI_RETAIL_READER_ASYNC,image.data,n);printf("background_module_matches(ASYNC): %s\n",match?"ok":"REFUSED");fail|=!match;
    size_t toggle=0;int tg=kui_vmu_native_toggle_find(image.data,n,&toggle);printf("toggle capability: %d",tg);if(tg==1)printf(" at 0x%zx",toggle);printf("\n");
    size_t mode=0;const char *why=emulation_off(image.data,n,&mode);
    if(why)printf("emulation Off: REFUSED (%s)\n",why);else printf("emulation Off: ok, mode slot at 0x%zx (GUI writes 7 at +16)\n",mode);fail|=why!=NULL;
    size_t seed=kui_vmu_native_cache_seed_find(image.data,n);
    if(seed==SIZE_MAX)printf("native cache seed: absent/ambiguous (optional; GUI CRCs the boot file itself)\n");else printf("native cache seed: blank slot at 0x%zx\n",seed);
    size_t report=report_slot(image.data,n);
    if(report==SIZE_MAX)printf("toy report seed: MISSING/ambiguous (the launch would continue without an SD report)\n");else printf("toy report seed: blank slot at 0x%zx\n",report);
    fail|=report==SIZE_MAX; /* the test is read by its saved report */
    printf("handoff staging limit 0x112000: %s\n",n<=0x112000u?"ok":"REFUSED");fail|=n>0x112000u;
    printf(fail?"RESULT: REFUSED\n":"RESULT: admitted by Card Tools' module checks, report seed bound\n");
    return fail;
}
