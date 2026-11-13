/* SPDX-License-Identifier: GPL-3.0-only */
#include "../dreamcast/sci_sd_storage.h"
#include "kui/vmu_backing.h"

static int toy_suite_report_write(struct kui_loader_sd *loan) {
    const struct kui_toy_report_wire *seed=&config.toy_report;
    if(!loan || !state.armed || state.error!=0x4fu || state.pending ||
       state.submitted!=state.completed || !seed->test || !loan->ready || loan->slow ||
       loan->high_capacity!=!!seed->high_capacity ||
       seed->lba[0]==seed->lba[1] || seed->lba[0]>=loan->blocks || seed->lba[1]>=loan->blocks ||
       kui_retail_crc32(0,seed,KUI_TOY_REPORT_CRC_OFFSET)!=seed->crc32)return -1;
    struct kui_vmu_sd_identity identity;
    if(kui_vmu_sd_identity_read(loan,&identity)!=KUI_VMU_BACKING_OK ||
       memcmp(identity.cid,seed->cid,sizeof(identity.cid)))return -1;
    uint8_t *verify=issued_chain+KUI_TOY_REPORT_FILE_BYTES;
    for(unsigned i=0;i<2u;i++) {
        if(kui_loader_sd_read(loan,seed->lba[i],1u,verify)!=KUI_LOADER_SD_OK ||
           kui_retail_crc32(0,verify,KUI_TOY_REPORT_SECTOR_BYTES)!=seed->placeholder_crc[i])return -1;
    }
    /* Match the durable writer's PIO path. Both slots have been checked before
     * the first write, and no filesystem metadata is updated in-game. */
    bool (*block)(void *,const uint8_t *,uint8_t *,size_t,bool,uint16_t *)=loan->bus.transfer_block;
    loan->bus.transfer_block=NULL;
    int result=0;
    for(unsigned i=0;i<2u;i++) {
        if(kui_sci_sd_write(loan,seed->lba[i],1u,issued_chain+i*KUI_TOY_REPORT_SECTOR_BYTES)!=KUI_LOADER_SD_OK) {
            result=-1;break;
        }
    }
    loan->bus.transfer_block=block;
    if(result || kui_sci_sd_sync(loan)!=KUI_LOADER_SD_OK)return -1;
    for(unsigned i=0;i<2u;i++) {
        if(kui_loader_sd_read(loan,seed->lba[i],1u,verify)!=KUI_LOADER_SD_OK ||
           memcmp(verify,issued_chain+i*KUI_TOY_REPORT_SECTOR_BYTES,KUI_TOY_REPORT_SECTOR_BYTES))return -1;
    }
    return 0;
}
