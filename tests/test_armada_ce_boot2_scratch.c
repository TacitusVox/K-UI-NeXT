/* SPDX-License-Identifier: GPL-3.0-only */
/* Synthetic maps only; no disc, bootstrap or kernel bytes. Built for the
 * Virtua Cop 2 module's profile: Bust-A-Move 4's sound group with Virtua Cop
 * 2's kernel record (KUI_WINCE_CDDA_BAM4_KERNEL_VIRTUA_COP_2). */
#define KUI_ARMADA_CE_STREAM 1
#include "kui/armada_ce_boot2_scratch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { assert(x); ++checks; } while(0)

static struct kui_retail_manifest full_map(void) {
    const unsigned count=6u;
    struct kui_retail_manifest map={.card_sectors=1000000u,.partition_start=1u,
        .partition_end=1000000u,.track_count=count,.extent_count=count,.session_lba=45000u,
        .boot_lba=45010u,.boot_bytes=KUI_WINCE_CDDA_BAM4_KERNEL_BYTES,
        .storage_transport=KUI_STORAGE_SCI,.reader=KUI_RETAIL_READER_STANDARD,
        .flags=KUI_RETAIL_IMAGE_BOOT_CRC,.ip_crc32=KUI_ARMADA_CE_BOOT2_SCRATCH_IP_CRC};
    strcpy(map.title,"Synthetic");strcpy(map.product,"OTHER");strcpy(map.bootfile,"0WINCEOS.BIN");
    strcpy(map.region,"J");
    uint32_t card=1u;
    for(unsigned i=0;i<count;i++) {
        uint32_t start=i<2u?i*100u:i==2u?45000u:47000u+(i-3u)*100u;
        uint32_t sectors=i==2u?2000u:100u;
        uint32_t blocks=(sectors*2352u+511u)/512u;
        map.slots[i].track=(struct kui_retail_track){start,start+sectors,
            (uint16_t)(count+i),1u,(uint8_t)(i==0u || i==2u?4u:0u)};
        map.slots[count+i].extent=(struct kui_retail_extent){0u,card,blocks};card+=blocks;
    }
    return map;
}
static struct kui_retail_manifest data_map(const struct kui_retail_manifest *full) {
    struct kui_retail_manifest map=*full;
    memset(map.slots+map.track_count,0,sizeof(map.slots)-map.track_count*sizeof(*map.slots));
    map.reader=KUI_RETAIL_READER_ASYNC;map.extent_count=0u;
    for(unsigned i=0;i<map.track_count;i++) {
        struct kui_retail_track *track=&map.slots[i].track;
        track->first_extent=(uint16_t)(map.track_count+map.extent_count);
        track->extent_count=0u;
        if(kui_retail_track_control(track)==4u) {
            track->extent_count=1u;
            map.slots[map.track_count+map.extent_count++].extent=
                full->slots[kui_retail_track_first_extent(&full->slots[i].track)].extent;
        }
    }
    return map;
}

static struct kui_armada_ce_stream_seed seed;
static struct kui_retail_manifest stage;
static uint8_t map[KUI_RETAIL_IMAGE_WIRE_BYTES],original[KUI_RETAIL_IMAGE_WIRE_BYTES];
static uint8_t work[KUI_ARMADA_CE_STREAM_GUARD_END-KUI_ARMADA_CE_STREAM_WORK_BEGIN];

static void reset(void) {
    const uint32_t blank[KUI_ARMADA_CE_STREAM_SEED_WORDS]=KUI_ARMADA_CE_STREAM_BLANK_WORDS;
    struct kui_retail_manifest full=full_map();
    memcpy(seed.words,blank,sizeof(blank));
    CHECK(kui_retail_manifest_encode(&full,seed.wire)==KUI_GAME_OK);
    seed.words[KUI_ARMADA_CE_STREAM_MAP_CRC_WORD]=(uint32_t)seed.wire[16]|(uint32_t)seed.wire[17]<<8|
        (uint32_t)seed.wire[18]<<16|(uint32_t)seed.wire[19]<<24;
    stage=data_map(&full);
    /* The stage's installation, then the bootstrap's scratch use: its stack
     * below the map's end (700 bytes; 760 on its compaction path) and its
     * 64-byte flash block at the start of the work page. */
    memcpy(original,seed.wire,sizeof(original));
    memcpy(map,seed.wire,sizeof(map));memset(work,0,sizeof(work));
    for(unsigned i=0xd08u;i<sizeof(map);i++) map[i]=(uint8_t)(0x5au+i);
    for(unsigned i=0;i<60u;i++) work[i]=(uint8_t)(0xa5u+i);
}
static bool repaired(void) {
    return !memcmp(map,seed.wire,sizeof(map)) &&
        !memcmp(work,(uint8_t[sizeof(work)]){0},sizeof(work));
}
static bool untouched(void) {
    for(unsigned i=0xd08u;i<sizeof(map);i++) if(map[i]!=(uint8_t)(0x5au+i)) return false;
    for(unsigned i=0;i<60u;i++) if(work[i]!=(uint8_t)(0xa5u+i)) return false;
    return !memcmp(map,original,0xd08u);
}

int main(void) {
    CHECK(KUI_WINCE_CDDA_BAM4_KERNEL==KUI_WINCE_CDDA_BAM4_KERNEL_VIRTUA_COP_2);
    CHECK(KUI_ARMADA_CE_STREAM_MAP_BEGIN==0x8cfbf000u && KUI_ARMADA_CE_STREAM_WORK_BEGIN==0x8cfc0000u &&
          KUI_ARMADA_CE_STREAM_GUARD_END==0x8cfd0000u);
    /* The disc's bootstrap used the map and work pages: both reinstalled. */
    reset();CHECK(!repaired());
    CHECK(kui_armada_ce_boot2_scratch_repair(map,work,sizeof(work),&seed,&stage,stage.ip_crc32));
    CHECK(repaired());
    /* Idempotent on an intact pool. */
    CHECK(kui_armada_ce_boot2_scratch_repair(map,work,sizeof(work),&seed,&stage,stage.ip_crc32));
    CHECK(repaired());
    /* Any other IP.BIN: nothing changes, and the relay's check then stops. */
    reset();
    CHECK(!kui_armada_ce_boot2_scratch_repair(map,work,sizeof(work),&seed,&stage,stage.ip_crc32^1u));
    CHECK(untouched());
    /* A damaged stage copy is never installed. */
    for(unsigned word=0;word<KUI_ARMADA_CE_STREAM_SEED_WORDS;word++) {
        reset();seed.words[word]^=1u;
        CHECK(!kui_armada_ce_boot2_scratch_repair(map,work,sizeof(work),&seed,&stage,stage.ip_crc32));
        CHECK(untouched());
    }
    for(unsigned at=0;at<sizeof(seed.wire);at+=97u) {
        reset();seed.wire[at]^=0x10u;
        CHECK(!kui_armada_ce_boot2_scratch_repair(map,work,sizeof(work),&seed,&stage,stage.ip_crc32));
        CHECK(untouched());
    }
    /* A copy that no longer matches the stage's own map is refused. */
    reset();stage.boot_lba++;
    CHECK(!kui_armada_ce_boot2_scratch_repair(map,work,sizeof(work),&seed,&stage,stage.ip_crc32));
    CHECK(untouched());
    reset();stage.slots[4].track.start_lba++;
    CHECK(!kui_armada_ce_boot2_scratch_repair(map,work,sizeof(work),&seed,&stage,stage.ip_crc32));
    CHECK(untouched());
    /* Missing arguments. */
    reset();
    CHECK(!kui_armada_ce_boot2_scratch_repair(NULL,work,sizeof(work),&seed,&stage,stage.ip_crc32));
    CHECK(!kui_armada_ce_boot2_scratch_repair(map,NULL,sizeof(work),&seed,&stage,stage.ip_crc32));
    CHECK(!kui_armada_ce_boot2_scratch_repair(map,work,sizeof(work),NULL,&stage,stage.ip_crc32));
    CHECK(!kui_armada_ce_boot2_scratch_repair(map,work,sizeof(work),&seed,NULL,stage.ip_crc32));
    CHECK(untouched());
    printf("PASS bootstrap-2 scratch repair: Virtua Cop 2's IP.BIN reinstalls the map and work pages from a proven stage copy; another IP.BIN, a damaged or mismatched copy and missing arguments change nothing (%u checks)\n",checks);
    return 0;
}
