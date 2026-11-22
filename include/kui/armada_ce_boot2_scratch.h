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
 * of the stream map (pool+0x6f000..pool+0x70000), and keeps a 64-byte flash
 * block at 0x8cfc0000, the start of the work page. Its other buffers lie above
 * the pool, in CE's driver globals, which the kernel sets up itself. Its only
 * lasting effect is in the console's flash.
 *
 * After that bootstrap has finished, the stage reinstalls the map and zeroes
 * the work and guard pages from its own copy, which it proves intact first:
 * the seed header, the map's CRC word and the decoded map against the stage's
 * own. Only for the exact IP.BIN whose bootstrap this is. The relay then
 * checks the pool exactly as for every other disc (seed words, map, audio
 * source bank, controller), so a write anywhere else still stops the launch. */
#define KUI_ARMADA_CE_BOOT2_SCRATCH_IP_CRC UINT32_C(0x3f7f1c3d) /* Virtua Cop 2 JPN */

static inline bool kui_armada_ce_boot2_scratch_repair(uint8_t *map,uint8_t *work,size_t work_bytes,
        const struct kui_armada_ce_stream_seed *seed,const struct kui_retail_manifest *stage_map,
        uint32_t ip_crc32) {
    struct kui_retail_manifest decoded;
    const uint8_t *crc=seed?seed->wire+16u:NULL;
    if(ip_crc32!=KUI_ARMADA_CE_BOOT2_SCRATCH_IP_CRC || !map || !work || !seed || !stage_map ||
       !kui_armada_ce_stream_seed_header_valid(seed->words) ||
       seed->words[KUI_ARMADA_CE_STREAM_MAP_CRC_WORD]!=
           ((uint32_t)crc[0]|(uint32_t)crc[1]<<8|(uint32_t)crc[2]<<16|(uint32_t)crc[3]<<24) ||
       kui_retail_manifest_decode(seed->wire,&decoded)!=KUI_GAME_OK ||
       !kui_armada_ce_stream_map_valid(&decoded) ||
       !kui_armada_ce_stream_maps_match(&decoded,stage_map)) return false;
    memcpy(map,seed->wire,sizeof(seed->wire));
    memset(work,0,work_bytes);
    return !memcmp(map,seed->wire,sizeof(seed->wire));
}
#endif
