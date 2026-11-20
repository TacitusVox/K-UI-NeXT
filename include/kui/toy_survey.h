/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_TOY_SURVEY_H
#define KUI_TOY_SURVEY_H
/* The stream survey's report file (docs/toy-wire-stream-test-83.md). Test
 * 83 is Test 81's stream reader with one change: started from the Games
 * menu, its cold stage finds 0:/KUI/SURVEY/STREAM.TXT, a file the owner
 * copies onto the card blank, and the report the reader showed on pages is
 * saved into it, with the game's name.
 *
 * The file is text in 512-byte sectors. A blank sector and a launch record
 * each end in a line feed, and so does the report's second sector:
 *   sector 0              the file's first sector: its ID and its slot count
 *   sector 1+3*s          slot s, part 0: the launch record (the cold stage)
 *   sector 2+3*s, 3+3*s   slot s, parts 1 and 2: the reader's 1,024-byte
 *                         report (the Toy worker, at the report gesture)
 * A blank sector names its file, its slot and its part, so no sector of the
 * card but a blank sector of a file with this ID can be taken for it: no
 * game, no save, nothing of the file system. (Keep one copy of a blank file
 * on a card, and a new ID for every refill: tools/toy_survey.py gives each
 * file its own. Were the volume damaged so that this file's chain led into a
 * second copy with the same ID, that copy's blank slot would be written.)
 * The stage writes a launch record only
 * over a sector it has just read back as that slot's blank part 0, and hands
 * the worker only two sectors it has just read back as the same slot's blank
 * parts 1 and 2; the worker checks their CRC-32 again before it writes
 * (src/loader/toy_suite_report_write.inc). */
#include <stddef.h>
#include <stdint.h>
#include "kui/toy_suite_report_wire.h"
#include "kui/wce_probe.h"

#define KUI_TOY_SURVEY_SECTOR 512u
#define KUI_TOY_SURVEY_PARTS 3u
#define KUI_TOY_SURVEY_SLOTS_MAX 999u
#define KUI_TOY_SURVEY_ID_BYTES 16u
/* The slot search gives up at this many sectors the card does not give. */
#define KUI_TOY_SURVEY_UNREAD_MAX 4u
/* The Toy tests' report seed carries the test Card Tools started. */
#define KUI_TOY_SURVEY_SEED_TEST 66u

struct kui_toy_survey_file {
    uint8_t id[KUI_TOY_SURVEY_ID_BYTES];
    uint32_t slots;
};
/* The file sector of a slot's part. */
static inline uint32_t kui_toy_survey_sector(uint32_t slot,uint32_t part) {
    return 1u+slot*KUI_TOY_SURVEY_PARTS+part;
}
static inline uint32_t kui_toy_survey_bytes(uint32_t slots) {
    return (1u+slots*KUI_TOY_SURVEY_PARTS)*KUI_TOY_SURVEY_SECTOR;
}

/* The exact text of the file's first sector. */
void kui_toy_survey_file_sector(const struct kui_toy_survey_file *file,uint8_t out[512]);
/* Reads a first sector: 0 if it is exactly the first sector of a survey
 * file, with that file's ID and slot count in *out. */
int kui_toy_survey_file_parse(const uint8_t sector[512],struct kui_toy_survey_file *out);
/* The exact text of a blank part (part 0, 1 or 2) of a slot. */
void kui_toy_survey_blank_sector(const struct kui_toy_survey_file *file,uint32_t slot,uint32_t part,
    uint8_t out[512]);

/* What the stage knows of a launch. Text fields need no terminator. */
struct kui_toy_survey_launch {
    const char *build;                 /* the module's build label */
    const char *title,*product,*bootfile,*region;
    uint32_t title_bytes,product_bytes,bootfile_bytes,region_bytes;
    uint32_t ip_crc32,gdi_crc32,boot_crc32,boot_bytes; /* the disc's map */
    uint32_t exec_crc32,exec_bytes;    /* the executable as loaded */
    uint32_t reader,transport,flags,source;
    uint32_t tracks,raw,cooked,other,extents;
    /* Audio tracks: all, those at or above the session (the disc's CD
     * music), and those the map holds the card's place of. */
    uint32_t audio,audio_high,audio_mapped;
    uint32_t session_lba,boot_lba;
};
/* The launch record of a slot: its first line names the file, the slot and
 * the part as the blank sector does, with LAUNCH for EMPTY. */
void kui_toy_survey_launch_sector(const struct kui_toy_survey_file *file,uint32_t slot,
    const struct kui_toy_survey_launch *launch,uint8_t out[512]);

/* Claiming a slot (the stage, and its host test). Read-only: the first slot
 * whose three parts all read back as their blank text. Every other slot is
 * passed over, whatever it holds: a launch record, a report, a write that
 * was cut off, bytes that are not the file's at all, a sector the card does
 * not give. FULL: no slot is blank. IO: the first sector could not be read,
 * or no blank slot was found and some sector could not be read. */
enum kui_toy_survey_claim_result {
    KUI_TOY_SURVEY_CLAIM_OK = 0, KUI_TOY_SURVEY_CLAIM_IO, KUI_TOY_SURVEY_CLAIM_NOT_FILE,
    KUI_TOY_SURVEY_CLAIM_FULL
};
struct kui_toy_survey_claim {
    struct kui_toy_survey_file file;
    uint32_t slot,used;                /* used: the slots before it */
    uint32_t lba[KUI_TOY_SURVEY_PARTS];
    uint32_t blank_crc[KUI_TOY_SURVEY_PARTS];
};
/* work and expect each hold 512 bytes. */
enum kui_toy_survey_claim_result kui_toy_survey_claim_find(const struct kui_wce_probe_volume *volume,
    const struct kui_wce_probe_layout *layout,uint8_t work[512],uint8_t expect[512],
    struct kui_toy_survey_claim *out);
/* The Toy worker's report seed for a claimed slot: its two report sectors,
 * their blank CRC-32, the card's identity. */
void kui_toy_survey_seed_build(const struct kui_toy_survey_claim *claim,const uint8_t cid[16],
    uint32_t high_capacity,struct kui_toy_report_wire *seed);
#endif
