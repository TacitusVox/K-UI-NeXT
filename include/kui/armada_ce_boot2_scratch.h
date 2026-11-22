/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_ARMADA_CE_BOOT2_SCRATCH_H
#define KUI_ARMADA_CE_BOOT2_SCRATCH_H
#include "kui/armada_ce_stream_seed.h"
#include <string.h>

/* A disc bootstrap that uses the top of the CD music pool as scratch.
 *
 * Virtua Cop 2 (JPN, HDR-0061) runs Sega's play-history recorder ("AIPf" over
 * the flash manager "fmMng") in its IP.BIN bootstrap 2, the code K-UI runs
 * before the game entry. The recorder sets its stack to 0x8cfc0000, the end
 * of the stream map (pool+0x6f000..pool+0x70000), and writes 60 bytes of its
 * work data at 0x8cfc0000, the start of the work page. Its other buffers lie above
 * the pool, in CE's driver globals, which the kernel sets up itself. Its only
 * lasting effect is in the console's flash.
 *
 * After that bootstrap has finished, the stage reinstalls the map and zeroes
 * the work and guard pages from its own copy, which it proves intact first:
 * the seed header, the map's CRC word and the decoded map against the stage's
 * own. Only for the exact IP.BIN whose bootstrap this is. The relay then
 * checks the pool exactly as for every other disc (seed words, map, audio
 * source bank, controller), so a write anywhere else still stops the launch.
 * For this disc a write inside the map, work or guard pages is not reported:
 * nothing but its bootstrap runs before the relay, and the pages are restored
 * whole to the state the stage left before the bootstrap.
 *
 * If the stage's copy fails its proof, the stage stops on its own screen,
 * BOOTSTRAP 2 SCRATCH COPY REFUSED, with the result below as DETAIL. */
#define KUI_ARMADA_CE_BOOT2_SCRATCH_IP_CRC UINT32_C(0x3f7f1c3d) /* Virtua Cop 2 JPN */
/* Its stack's top and the start of its other buffers. */
#define KUI_ARMADA_CE_BOOT2_SCRATCH_STACK_TOP UINT32_C(0x8cfc0000)
#define KUI_ARMADA_CE_BOOT2_SCRATCH_BUFFERS UINT32_C(0x8cfd0000)
_Static_assert(KUI_ARMADA_CE_BOOT2_SCRATCH_STACK_TOP==KUI_ARMADA_CE_STREAM_MAP_BEGIN+KUI_RETAIL_IMAGE_WIRE_BYTES &&
               KUI_ARMADA_CE_STREAM_MAP_BEGIN+KUI_RETAIL_IMAGE_WIRE_BYTES==KUI_ARMADA_CE_STREAM_WORK_BEGIN,
               "Virtua Cop 2's bootstrap stack ends at the map's end, where the work page begins");
_Static_assert(KUI_ARMADA_CE_BOOT2_SCRATCH_BUFFERS>=KUI_ARMADA_CE_STREAM_GUARD_END,
               "Virtua Cop 2's bootstrap buffers lie above the pool");

enum kui_armada_ce_boot2_scratch_result {
    KUI_ARMADA_CE_BOOT2_SCRATCH_OTHER_DISC=0,  /* another IP.BIN: nothing changes */
    KUI_ARMADA_CE_BOOT2_SCRATCH_REINSTALLED=1, /* map reinstalled, work and guard zeroed */
    /* This IP.BIN, refused; nothing changes: */
    KUI_ARMADA_CE_BOOT2_SCRATCH_NO_BUFFERS=2,
    KUI_ARMADA_CE_BOOT2_SCRATCH_SEED_HEADER=3,
    KUI_ARMADA_CE_BOOT2_SCRATCH_MAP_CRC_WORD=4,
    KUI_ARMADA_CE_BOOT2_SCRATCH_MAP_DECODE=5,
    KUI_ARMADA_CE_BOOT2_SCRATCH_MAP_INVALID=6,
    KUI_ARMADA_CE_BOOT2_SCRATCH_MAP_MISMATCH=7,
    /* This IP.BIN: the map reads back different after the copy. */
    KUI_ARMADA_CE_BOOT2_SCRATCH_COPY_CHANGED=8,
};

static inline enum kui_armada_ce_boot2_scratch_result kui_armada_ce_boot2_scratch_repair(
        uint8_t *map,uint8_t *work,size_t work_bytes,
        const struct kui_armada_ce_stream_seed *seed,const struct kui_retail_manifest *stage_map,
        uint32_t ip_crc32) {
    struct kui_retail_manifest decoded;
    if(ip_crc32!=KUI_ARMADA_CE_BOOT2_SCRATCH_IP_CRC) return KUI_ARMADA_CE_BOOT2_SCRATCH_OTHER_DISC;
    if(!map || !work || !seed || !stage_map) return KUI_ARMADA_CE_BOOT2_SCRATCH_NO_BUFFERS;
    const uint8_t *crc=seed->wire+16u;
    if(!kui_armada_ce_stream_seed_header_valid(seed->words)) return KUI_ARMADA_CE_BOOT2_SCRATCH_SEED_HEADER;
    if(seed->words[KUI_ARMADA_CE_STREAM_MAP_CRC_WORD]!=
       ((uint32_t)crc[0]|(uint32_t)crc[1]<<8|(uint32_t)crc[2]<<16|(uint32_t)crc[3]<<24))
        return KUI_ARMADA_CE_BOOT2_SCRATCH_MAP_CRC_WORD;
    if(kui_retail_manifest_decode(seed->wire,&decoded)!=KUI_GAME_OK)
        return KUI_ARMADA_CE_BOOT2_SCRATCH_MAP_DECODE;
    if(!kui_armada_ce_stream_map_valid(&decoded)) return KUI_ARMADA_CE_BOOT2_SCRATCH_MAP_INVALID;
    if(!kui_armada_ce_stream_maps_match(&decoded,stage_map)) return KUI_ARMADA_CE_BOOT2_SCRATCH_MAP_MISMATCH;
    memcpy(map,seed->wire,sizeof(seed->wire));
    memset(work,0,work_bytes);
    return memcmp(map,seed->wire,sizeof(seed->wire))?KUI_ARMADA_CE_BOOT2_SCRATCH_COPY_CHANGED:
        KUI_ARMADA_CE_BOOT2_SCRATCH_REINSTALLED;
}
#endif
