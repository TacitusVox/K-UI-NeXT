/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/toy_suite_report_wire.h"

/* The GUI patches this object after verifying the module envelope. Assembly
 * keeps the blank initializer from being folded into the cold-stage reader. */
__asm__(".section .rodata.toy_report_seed,\"a\"\n.balign 4\n"
        ".global __toy_suite_report_seed\n.type __toy_suite_report_seed,@object\n"
        "__toy_suite_report_seed:\n.ascii \"KUITRS01\"\n.long 1,64\n.zero 48\n"
        ".size __toy_suite_report_seed,64\n.previous\n");
extern const struct kui_toy_report_wire toy_report_seed __asm__("__toy_suite_report_seed");

static void toy_suite_report_prepare(struct kui_toy_report_wire *out) {
    const struct kui_toy_report_wire *seed=&toy_report_seed;
#if KUI_TOY_FMV_COMPARE
    unsigned test=29u;
#else
    unsigned test=toy_suite_source==2u && KUI_TOY_SUITE_MODE==1 ? 5u : KUI_TOY_SUITE_MODE;
#endif
    if(!toy_suite_source || card.transport!=KUI_STORAGE_SCI || seed->test!=test ||
       seed->magic[0]!=KUI_TOY_REPORT_MAGIC0 || seed->magic[1]!=KUI_TOY_REPORT_MAGIC1 ||
       seed->version!=KUI_TOY_REPORT_VERSION || seed->bytes!=sizeof(*seed) || seed->reserved ||
       seed->high_capacity>1u || seed->lba[0]==seed->lba[1] ||
       kui_retail_crc32(0,seed,KUI_TOY_REPORT_CRC_OFFSET)!=seed->crc32)return;
    for(unsigned i=0;i<2u;i++) {
        uint32_t lba=seed->lba[i];
        if(!lba || lba<manifest.partition_start || lba>=manifest.partition_end || lba>=manifest.card_sectors)return;
        for(unsigned j=0;j<manifest.extent_count;j++) {
            const struct kui_retail_extent *e=&manifest.slots[manifest.track_count+j].extent;
            if(lba>=e->card_lba && lba-e->card_lba<e->blocks)return;
        }
    }
    *out=*seed;
}
