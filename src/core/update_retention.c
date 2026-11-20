/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/update_retention.h"
#include "kui/update.h"
#include "kui/hash.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define RESUME_RECORD KUI_NIGHTLY_STATE "/resume.id"
#define RELEASE_MAGIC "KUIRELS1"

/* Fixed byte layout shared by the resume marker (order zero) and completed
 * download receipts. CRC and commit/name checks fail closed on torn metadata. */
struct receipt {
    char magic[8], commit[40], digest[64];
    uint8_t size[4], order[4], crc[4];
};

static uint32_t read32(const uint8_t p[4]) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void put32(uint8_t p[4], uint32_t n) {
    for(unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(n >> (8 * i));
}
static bool is_hex(const char *s, size_t n) {
    for(size_t i = 0; i < n; ++i)
        if((s[i] < '0' || s[i] > '9') && (s[i] < 'a' || s[i] > 'f')) return false;
    return true;
}

static FRESULT read_receipt(const char *path, struct receipt *rec, bool *valid) {
    *valid = false;
    FIL file;
    FRESULT fr = f_open(&file, path, FA_READ);
    if(fr != FR_OK) return fr;
    if(f_size(&file) == sizeof(*rec)) {
        UINT n = 0;
        fr = f_read(&file, rec, sizeof(*rec), &n);
        uint32_t size = fr == FR_OK && n == sizeof(*rec) ? read32(rec->size) : 0;
        *valid = size >= KUI_UPDATE_HEADER_BYTES && size <= KUI_UPDATE_MAX_PAYLOAD + KUI_UPDATE_RECEIPT_MAX &&
            (!memcmp(rec->magic, "KUICACHE", 8) || !memcmp(rec->magic, RELEASE_MAGIC, 8)) && is_hex(rec->commit, 40) && is_hex(rec->digest, 64) &&
            read32(rec->crc) == kui_crc32(0, rec, offsetof(struct receipt, crc));
    }
    FRESULT close = f_close(&file);
    return fr == FR_OK ? close : fr;
}

static bool write_receipt(const char *path, const struct kui_nightly_asset *asset, uint32_t order) {
    struct receipt rec;
    memcpy(rec.magic, asset->release ? RELEASE_MAGIC : "KUICACHE", 8);
    memcpy(rec.commit, asset->commit, 40);
    memcpy(rec.digest, asset->digest, 64);
    put32(rec.size, asset->size);
    put32(rec.order, order);
    put32(rec.crc, kui_crc32(0, &rec, offsetof(struct receipt, crc)));
    FIL file;
    UINT n = 0;
    if(f_open(&file, path, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return false;
    bool ok = f_write(&file, &rec, sizeof(rec), &n) == FR_OK && n == sizeof(rec);
    return f_close(&file) == FR_OK && ok;
}

bool kui_nightly_prepare_partial(const struct kui_nightly_asset *asset) {
    struct receipt rec;
    bool valid;
    FRESULT fr = read_receipt(RESUME_RECORD, &rec, &valid);
    if(fr != FR_OK && fr != FR_NO_FILE) return false;
    bool owned = valid && !read32(rec.order);
    if(owned && asset->release == !memcmp(rec.magic, RELEASE_MAGIC, 8) && read32(rec.size) == asset->size && !memcmp(asset->commit, rec.commit, 40) &&
       !memcmp(asset->digest, rec.digest, 64)) return true;
    /* Discard the old bytes before changing their identity. Without a valid
     * marker, preserve anything already occupying the private resume path. */
    FILINFO info;
    fr = f_stat(KUI_NIGHTLY_PART, &info);
    if(fr == FR_OK) {
        if(!owned || (info.fattrib & AM_DIR) || f_unlink(KUI_NIGHTLY_PART) != FR_OK) return false;
    } else if(fr != FR_NO_FILE) return false;
    return write_receipt(RESUME_RECORD, asset, 0);
}

enum kui_nightly_verify kui_nightly_verify_file(const char *path, const struct kui_nightly_asset *asset,
        kui_cancel_fn cancel, void (*progress)(void *, uint32_t), void *context) {
    FIL file;
    FRESULT fr = f_open(&file, path, FA_READ);
    if(fr == FR_NO_FILE || fr == FR_NO_PATH) return KUI_NIGHTLY_MISSING;
    if(fr != FR_OK) return KUI_NIGHTLY_STORAGE_ERROR;
    enum kui_nightly_verify result = KUI_NIGHTLY_MISMATCH;
    if(f_size(&file) == asset->size) {
        struct kui_sha256 hash;
        kui_sha256_init(&hash);
        uint8_t buffer[4096], digest[32];
        uint32_t bytes = 0;
        if(progress) progress(context, bytes);
        while(bytes < asset->size) {
            if(cancel && cancel()) { result = KUI_NIGHTLY_STOPPED; break; }
            UINT n = asset->size - bytes, got;
            if(n > sizeof(buffer)) n = sizeof(buffer);
            if(f_read(&file, buffer, n, &got) != FR_OK || got != n) {
                result = KUI_NIGHTLY_STORAGE_ERROR;
                break;
            }
            kui_sha256_update(&hash, buffer, got);
            bytes += got;
            if(progress && (bytes % 65536u == 0 || bytes == asset->size)) progress(context, bytes);
        }
        if(bytes == asset->size) {
            char hex[65];
            kui_sha256_digest(&hash, digest);
            kui_hex(digest, sizeof(digest), hex);
            if(!strcmp(hex, asset->digest)) result = KUI_NIGHTLY_VERIFIED;
        }
    }
    return f_close(&file) == FR_OK ? result : KUI_NIGHTLY_STORAGE_ERROR;
}

struct inventory {
    uint32_t high, newest[KUI_UPDATE_KEEP_MAX], threshold;
    unsigned keep;
    const char *protected_commit;
    bool protected_release, pinned, pruning, wrote;
};

/* Keep at most ten newest sequence numbers in RAM, regardless of how many
 * packages were downloaded under Unlimited. Ties are retained on ambiguity. */
static void remember(struct inventory *inv, uint32_t order) {
    for(unsigned i = 0; i < inv->keep; ++i) {
        if(order <= inv->newest[i]) continue;
        uint32_t displaced = inv->newest[i];
        inv->newest[i] = order;
        order = displaced;
    }
}

static bool visit(struct inventory *inv, const char *commit, const struct receipt *rec,
                  const char *receipt_path, kui_cancel_fn cancel) {
    uint32_t order = read32(rec->order);
    if(order > inv->high) inv->high = order;
    if(!inv->keep) return true;
    char path[KUI_UPDATE_BROWSER_PATH_BYTES];
    bool stable = !memcmp(rec->magic, RELEASE_MAGIC, 8);
    snprintf(path, sizeof(path), stable ? KUI_RELEASE_PACKAGE : KUI_NIGHTLY_PACKAGE, commit);
    bool pinned = inv->protected_release == stable && inv->protected_commit && !strcmp(commit, inv->protected_commit);
    if(!inv->pruning) {
        FILINFO info;
        FRESULT fr = f_stat(path, &info);
        if(fr == FR_NO_FILE || fr == FR_NO_PATH) return true;
        if(fr != FR_OK) return false;
        if((info.fattrib & AM_DIR) || info.fsize != read32(rec->size)) return true;
        if(pinned) inv->pinned = true;
        else remember(inv, order);
        return true;
    }
    if(pinned || (inv->threshold && order >= inv->threshold)) return true;
    struct kui_nightly_asset asset = {.size = read32(rec->size)};
    memcpy(asset.digest, rec->digest, 64);
    enum kui_nightly_verify match = kui_nightly_verify_file(path, &asset, cancel, NULL, NULL);
    if(match == KUI_NIGHTLY_STORAGE_ERROR || match == KUI_NIGHTLY_STOPPED || (cancel && cancel())) return false;
    /* If a user replaced the package, retire its receipt but preserve their
     * file. Only exact bytes recorded at publication are eligible for unlink. */
    inv->wrote = true;
    if(match == KUI_NIGHTLY_VERIFIED && f_unlink(path) != FR_OK) return false;
    return f_unlink(receipt_path) == FR_OK;
}

static bool scan(struct inventory *inv, kui_cancel_fn cancel) {
    DIR dir;
    FRESULT fr = f_opendir(&dir, KUI_NIGHTLY_STATE);
    if(fr == FR_NO_PATH || fr == FR_NO_FILE) return true;
    if(fr != FR_OK) return false;
    bool ok = true;
    FILINFO info;
    while((fr = f_readdir(&dir, &info)) == FR_OK && info.fname[0]) {
        if(cancel && cancel()) { ok = false; break; }
        if((info.fattrib & AM_DIR) || strlen(info.fname) != 44 || !is_hex(info.fname, 40)) continue;
        bool release = !strcmp(info.fname + 40, ".rel");
        if(!release && strcmp(info.fname + 40, ".rec")) continue;
        char path[KUI_UPDATE_BROWSER_PATH_BYTES];
        info.fname[40] = 0;
        snprintf(path, sizeof(path), KUI_NIGHTLY_STATE "/%.40s.%s", info.fname, release ? "rel" : "rec");
        struct receipt rec;
        bool valid;
        fr = read_receipt(path, &rec, &valid);
        if(fr != FR_OK) break;
        if(!valid || release != !memcmp(rec.magic, RELEASE_MAGIC, 8) || !read32(rec.order) || memcmp(rec.commit, info.fname, 40)) continue;
        if(!visit(inv, info.fname, &rec, path, cancel)) { ok = false; break; }
    }
    return f_closedir(&dir) == FR_OK && fr == FR_OK && ok;
}

bool kui_nightly_record_download(const struct kui_nightly_asset *asset, kui_cancel_fn cancel) {
    struct inventory inv = {0};
    if(!scan(&inv, cancel) || inv.high == UINT32_MAX) return false;
    char path[KUI_UPDATE_BROWSER_PATH_BYTES];
    snprintf(path, sizeof(path), KUI_NIGHTLY_STATE "/%.40s.%s", asset->commit, asset->release ? "rel" : "rec");
    return write_receipt(path, asset, inv.high + 1);
}

bool kui_online_trim_report(unsigned keep, const char *protected_commit, bool stable, kui_cancel_fn cancel, bool *wrote) {
    *wrote = false;
    if(!kui_update_keep_valid(keep)) return false;
    if(!keep) return true;
    struct inventory inv = {.keep = keep, .protected_commit = protected_commit, .protected_release = stable};
    if(!scan(&inv, cancel)) return false;
    unsigned slots = keep - (inv.pinned ? 1u : 0u);
    inv.threshold = slots ? inv.newest[slots - 1] : 0;
    if(slots && !inv.threshold) return true;
    inv.pruning = true;
    bool ok = scan(&inv, cancel);
    *wrote = inv.wrote;
    return ok;
}

bool kui_nightly_trim_report(unsigned keep, const char *protected_commit, kui_cancel_fn cancel, bool *wrote) {
    return kui_online_trim_report(keep, protected_commit, false, cancel, wrote);
}

bool kui_nightly_trim(unsigned keep, const char *protected_commit, kui_cancel_fn cancel) {
    bool wrote;
    return kui_nightly_trim_report(keep, protected_commit, cancel, &wrote);
}
