/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/update_keep_file.h"
#include "kui/update_retention.h"
#include "kui/update.h"
#include <string.h>

static const char *const paths[] = {
    KUI_UPDATE_SYSTEM "/nightly-keep-a.bin", KUI_UPDATE_SYSTEM "/nightly-keep-b.bin"
};

/* 8-byte magic/version, LE sequence, limit, preferred channel (zero on legacy cards), two reserved bytes, LE CRC32. */
enum { RECORD_BYTES = 20 };
struct slot {
    uint32_t sequence; /* Zero means missing or invalid. */
    unsigned keep;
    enum kui_update_channel channel;
    bool present;
};

static uint32_t get32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 |
           (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

static void put32(uint8_t *bytes, uint32_t value) {
    for(unsigned i = 0; i < 4; ++i) bytes[i] = (uint8_t)(value >> (8 * i));
}

static bool read_slot(unsigned index, struct slot *slot) {
    *slot = (struct slot){0};
    FIL file;
    FRESULT result = f_open(&file, paths[index], FA_READ);
    if(result == FR_NO_FILE || result == FR_NO_PATH) return true;
    if(result != FR_OK) return false;
    slot->present = true;

    uint8_t record[RECORD_BYTES];
    bool sized = f_size(&file) == sizeof(record);
    UINT size = 0;
    if(sized) result = f_read(&file, record, sizeof(record), &size);
    FRESULT closed = f_close(&file);
    if(result != FR_OK || closed != FR_OK || (sized && size != sizeof(record))) return false;
    if(sized && !memcmp(record, "KUIKEEP1", 8) &&
       kui_update_keep_valid(record[12]) && record[13] <= KUI_UPDATE_NIGHTLIES && !record[14] && !record[15] &&
       get32(record + 16) == kui_crc32(0, record, 16)) {
        slot->sequence = get32(record + 8);
        slot->keep = record[12];
        slot->channel = (enum kui_update_channel)record[13];
    }
    return true;
}

static bool latest_slot(struct slot *latest, unsigned *index) {
    struct slot slots[2];
    if(!read_slot(0, &slots[0]) || !read_slot(1, &slots[1])) return false;
    *index = slots[1].sequence > slots[0].sequence ? 1u : 0u;
    *latest = slots[*index];
    latest->present = slots[0].present || slots[1].present;
    return true;
}

bool kui_update_preferences_load(unsigned *keep, enum kui_update_channel *channel) {
    if(!keep || !channel) return false;
    *keep = KUI_UPDATE_KEEP_DEFAULT;
    *channel = KUI_UPDATE_RELEASES;
    struct slot latest;
    unsigned index;
    if(!latest_slot(&latest, &index)) return false;
    if(latest.sequence) { *keep = latest.keep; *channel = latest.channel; }
    else if(latest.present) *keep = 0;
    return true;
}

static bool ensure_directory(const char *path) {
    FRESULT result = f_mkdir(path);
    return result == FR_OK || result == FR_EXIST;
}

bool kui_update_preferences_save(unsigned keep, enum kui_update_channel channel) {
    struct slot latest;
    unsigned index;
    if((unsigned)channel > KUI_UPDATE_NIGHTLIES || !kui_update_keep_valid(keep) || !latest_slot(&latest, &index)) return false;
    if(latest.sequence && latest.keep == keep && latest.channel == channel) return true;
    if(latest.sequence == UINT32_MAX) return false;
    if(!latest.sequence && (!ensure_directory("0:/KUI") || !ensure_directory(KUI_UPDATE_SYSTEM))) return false;

    uint8_t record[RECORD_BYTES] = "KUIKEEP1";
    put32(record + 8, latest.sequence + 1);
    record[12] = (uint8_t)keep;
    record[13] = (uint8_t)channel;
    put32(record + 16, kui_crc32(0, record, 16));
    unsigned target = latest.sequence ? 1u - index : 0u;
    FIL file;
    if(f_open(&file, paths[target], FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return false;
    UINT written = 0;
    bool ok = f_write(&file, record, sizeof(record), &written) == FR_OK && written == sizeof(record);
    if(ok) ok = f_sync(&file) == FR_OK;
    FRESULT closed = f_close(&file);
    struct slot saved;
    return ok && closed == FR_OK && read_slot(target, &saved) &&
           saved.sequence == latest.sequence + 1 && saved.keep == keep && saved.channel == channel;
}


bool kui_update_keep_load(unsigned *keep) {
    enum kui_update_channel channel;
    return kui_update_preferences_load(keep, &channel);
}

bool kui_update_keep_save(unsigned keep) {
    enum kui_update_channel channel;
    unsigned old_keep;
    return kui_update_keep_valid(keep) && kui_update_preferences_load(&old_keep, &channel) &&
           kui_update_preferences_save(keep, channel);
}
