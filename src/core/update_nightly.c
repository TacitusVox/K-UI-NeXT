/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/update_nightly.h"
#include "kui/update.h"
#include "kui/hash.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>

#ifndef KUI_UPDATE_REVIEW_FEED
#define KUI_UPDATE_REVIEW_FEED 0
#endif
#if KUI_UPDATE_REVIEW_FEED == 1
#define MANIFEST_URL "https://raw.githubusercontent.com/TacitusVox/K-UI-NeXT/refs/heads/codex/updater-overhaul/evidence/updater-bba/nightly.txt"
#define REVIEW_ASSET "https://github.com/TacitusVox/K-UI-NeXT/raw/refs/heads/codex/updater-overhaul/evidence/updater-bba/K-UI.kup"
#elif KUI_UPDATE_REVIEW_FEED == 0
#define MANIFEST_URL "https://raw.githubusercontent.com/TacitusVox/K-UI-NeXT/nightly-channel/nightly.txt"
#define RELEASE_BASE "https://github.com/TacitusVox/K-UI-NeXT/releases/download/nightly-"
#else
#error "KUI_UPDATE_REVIEW_FEED must be 0 or 1"
#endif
#define FILE_LIMIT (KUI_UPDATE_MAX_PAYLOAD + KUI_UPDATE_RECEIPT_MAX)

struct nightly {
    char commit[41], digest[65];
    uint32_t size;
};

struct manifest_buffer {
    char data[160];
    size_t used;
};

struct job {
    struct kui_app_status *status;
    kui_app_progress_fn progress;
    kui_cancel_fn cancel;
    uint32_t offset;
};

static void message(struct job *job, const char *text) {
    snprintf(job->status->message, sizeof(job->status->message), "%s", text);
    if(job->progress) job->progress(job->status);
}

static bool collect(void *context, const void *data, size_t bytes) {
    struct manifest_buffer *buffer = context;
    if(bytes > sizeof(buffer->data) - buffer->used) return false;
    memcpy(buffer->data + buffer->used, data, bytes);
    buffer->used += bytes;
    return true;
}

static bool hex_field(const char *text, unsigned size) {
    for(unsigned i = 0; i < size; ++i) {
        char c = text[i];
        if((c < '0' || c > '9') && (c < 'a' || c > 'f')) return false;
    }
    return true;
}

/* Exact wire format: magic, Git object ID, decimal byte length, and SHA-256.
 * The authenticated manifest supplies no URL or local filesystem components. */
static bool parse_manifest(const struct manifest_buffer *buffer, struct nightly *asset) {
    static const char magic[] = "KUI-NIGHTLY-1\n";
    const size_t fixed = sizeof(magic)-1 + 41 + 1 + 65;
    if(buffer->used <= fixed || buffer->used > fixed + 9) return false;
    const char *commit = buffer->data + sizeof(magic)-1;
    const char *number = commit + 41;
    const char *digest = buffer->data + buffer->used - 65;
    if(memcmp(buffer->data, magic, sizeof(magic)-1) || commit[40] != '\n' ||
       !hex_field(commit, 40) || *number == '0' || digest[-1] != '\n' ||
       !hex_field(digest, 64) || digest[64] != '\n') return false;
    uint32_t size = 0;
    for(const char *p = number; p < digest-1; ++p) {
        if(*p < '0' || *p > '9') return false;
        size = size * 10u + (unsigned)(*p - '0');
    }
    if(size < KUI_UPDATE_HEADER_BYTES || size > FILE_LIMIT) return false;
    asset->size = size;
    memcpy(asset->commit, commit, 40); asset->commit[40] = 0;
    memcpy(asset->digest, digest, 64); asset->digest[64] = 0;
    return true;
}

static bool ensure_directory(const char *path) {
    FILINFO info;
    FRESULT fr = f_stat(path, &info);
    if(fr == FR_OK) return (info.fattrib & AM_DIR) != 0;
    return fr == FR_NO_FILE && f_mkdir(path) == FR_OK;
}

enum verify_result { VERIFIED, MISSING, MISMATCH, STORAGE_ERROR, STOPPED };
static enum verify_result verify_file(const char *path, const struct nightly *asset, struct job *job) {
    FIL file;
    FRESULT fr = f_open(&file, path, FA_READ);
    if(fr == FR_NO_FILE) return MISSING;
    if(fr != FR_OK) return STORAGE_ERROR;
    enum verify_result result = MISMATCH;
    if(f_size(&file) == asset->size) {
        struct kui_sha256 hash;
        kui_sha256_init(&hash);
        job->status->done = 0;
        message(job, "Verifying nightly on storage");
        uint8_t buffer[4096], digest[32];
        uint32_t bytes = 0;
        result = VERIFIED;
        while(bytes < asset->size) {
            if(job->cancel && job->cancel()) { result = STOPPED; break; }
            UINT amount = asset->size - bytes;
            if(amount > sizeof(buffer)) amount = sizeof(buffer);
            UINT got;
            if(f_read(&file, buffer, amount, &got) != FR_OK || got != amount) {
                result = STORAGE_ERROR;
                break;
            }
            kui_sha256_update(&hash, buffer, got);
            bytes += got;
            job->status->done = bytes;
            if(job->progress && (bytes % 65536u == 0 || bytes == asset->size)) job->progress(job->status);
        }
        if(result == VERIFIED) {
            char hex[65];
            kui_sha256_digest(&hash, digest);
            kui_hex(digest, sizeof(digest), hex);
            if(strcmp(asset->digest, hex)) result = MISMATCH;
        }
    }
    return f_close(&file) == FR_OK ? result : STORAGE_ERROR;
}

static bool write_file(void *context, const void *data, size_t size) {
    FIL *file = context;
    FSIZE_t before = f_tell(file);
    UINT written;
    if(f_write(file, data, (UINT)size, &written) != FR_OK || written != size) return false;
    /* Keep long modem transfers resumable after a reset. Rehashing the full
     * file at the end also covers the persisted prefix. */
    const unsigned checkpoint = 128u * 1024u;
    return before / checkpoint == f_tell(file) / checkpoint || f_sync(file) == FR_OK;
}

static void received(void *context, uint32_t bytes, uint32_t total) {
    (void)total;
    struct job *job = context;
    job->status->done = job->offset + bytes;
    if(job->progress) job->progress(job->status);
}

static bool check_room(uint32_t remaining) {
    DWORD clusters;
    FATFS *fs;
    return !remaining || (f_getfree("0:", &clusters, &fs) == FR_OK && fs && fs->csize &&
        (uint64_t)clusters * fs->csize * 512u >= remaining);
}

/* The only file ever opened for writing is the commit-specific partial. Its
 * length is the resume offset. All bytes are re-read before publication. */
static bool download(struct kui_internet *session, const struct nightly *asset,
                     const char *part, struct job *job) {
    FIL file;
    if(f_open(&file, part, FA_READ | FA_WRITE | FA_OPEN_ALWAYS) != FR_OK) {
        message(job, "Cannot open nightly download on storage");
        return false;
    }
#if KUI_UPDATE_REVIEW_FEED
    const char *url = REVIEW_ASSET;
#else
    char url[sizeof(RELEASE_BASE) + 60];
    snprintf(url, sizeof(url), RELEASE_BASE "%s/K-UI.kup", asset->commit);
#endif
    FSIZE_t length = f_size(&file);
    bool ok = f_lseek(&file, length <= asset->size ? length : 0) == FR_OK;
    if(ok && length > asset->size) ok = f_truncate(&file) == FR_OK;
    if(!ok) message(job, "Cannot prepare partial nightly on storage");
    while(ok && f_tell(&file) < asset->size) {
        job->offset = (uint32_t)f_tell(&file);
        if(!check_room(asset->size - job->offset)) {
            message(job, "Cannot confirm enough free space for nightly");
            ok = false;
            break;
        }
        job->status->done = job->offset;
        message(job, job->offset ? "Resuming nightly download" : "Downloading nightly");
        enum kui_http_result r = kui_internet_get_remainder(session, url, job->offset, asset->size,
            write_file, &file, received, job);
        if(r == KUI_HTTP_RANGE_IGNORED && job->offset) {
            ok = f_lseek(&file, 0) == FR_OK && f_truncate(&file) == FR_OK;
            if(!ok) message(job, "Cannot restart partial download on storage");
            continue;
        }
        ok = r == KUI_HTTP_OK && f_tell(&file) == asset->size;
        if(!ok) {
            const char *problem = kui_internet_problem(session);
            message(job, r == KUI_HTTP_STOPPED ? "Download stopped. Partial retained." :
                *problem ? problem : "Nightly download incomplete. Retry to resume.");
        }
        break;
    }
    if(f_close(&file) != FR_OK) {
        message(job, "Could not finish writing partial nightly to storage");
        return false;
    }
    return ok;
}

bool kui_update_nightly_download(enum kui_internet_route route, const char *running_build,
        char path[KUI_UPDATE_BROWSER_PATH_BYTES], struct kui_app_status *status,
        kui_log_fn log, kui_cancel_fn cancel, kui_app_progress_fn progress) {
    if(!path || !status) return false;
    *path = 0;
    memset(status, 0, sizeof(*status));
    struct job job = { .status = status, .progress = progress, .cancel = cancel };
    bool ok = false;
    char problem[128] = "";
    message(&job, "Connecting to GitHub for nightly update");
    struct kui_internet *session = kui_internet_open(route, log, cancel, problem);
    if(!session) {
        message(&job, problem[0] ? problem : "Cannot open network connection");
        goto done;
    }
    struct manifest_buffer buffer = {0};
    struct nightly asset;
    enum kui_http_result r = kui_internet_get(session, MANIFEST_URL, sizeof(buffer.data),
        collect, &buffer, NULL, NULL);
    if(r != KUI_HTTP_OK) {
        message(&job, r == KUI_HTTP_MISSING ? "No nightly channel published yet" : kui_internet_problem(session));
        goto done;
    }
    if(!parse_manifest(&buffer, &asset)) {
        message(&job, "GitHub returned an invalid nightly manifest");
        goto done;
    }
    if(running_build && strlen(running_build) == 12 && !memcmp(running_build, asset.commit, 12)) {
        ok = true;
        message(&job, "Already running the GitHub nightly build");
        goto done;
    }
    if(!ensure_directory("0:/KUI") || !ensure_directory(KUI_UPDATE_BROWSER_ROOT)) {
        message(&job, "Cannot create updates folder on storage");
        goto done;
    }
    status->total = asset.size;
    snprintf(status->lines[0], sizeof(status->lines[0]), "GitHub %.12s   %lu bytes",
        asset.commit, (unsigned long)asset.size);
    status->line_count = 1;
    char final[KUI_UPDATE_BROWSER_PATH_BYTES], part[KUI_UPDATE_BROWSER_PATH_BYTES];
    snprintf(final, sizeof(final), KUI_UPDATE_BROWSER_ROOT "/nightly-%s.kup", asset.commit);
    snprintf(part, sizeof(part), KUI_UPDATE_BROWSER_ROOT "/nightly-%s.part", asset.commit);
    enum verify_result existing = verify_file(final, &asset, &job);
    if(existing == VERIFIED) {
        ok = true;
    } else if(existing == MISSING || existing == MISMATCH) {
        if(!download(session, &asset, part, &job)) goto done;
        enum verify_result checked = verify_file(part, &asset, &job);
        if(checked != VERIFIED) {
            if(checked == MISMATCH) {
                if(f_unlink(part) == FR_OK) message(&job, "Nightly SHA-256 or size mismatch. Retry to download again.");
                else message(&job, "Nightly failed verification. Cannot remove damaged partial file.");
            } else message(&job, "Could not verify nightly on storage");
            goto done;
        }
        if(cancel && cancel()) goto done;
        if((existing == MISMATCH && f_unlink(final) != FR_OK) || f_rename(part, final) != FR_OK) {
            message(&job, "Cannot publish verified nightly on storage");
            goto done;
        }
        ok = true;
    } else message(&job, "Cannot verify existing nightly package");
    if(ok) {
        strcpy(path, final);
        message(&job, "Verified nightly ready for review");
    }
done:
    if(session) kui_internet_close(session);
    status->complete = true;
    status->passed = ok;
    status->stopped = !ok && cancel && cancel();
    status->errors = !ok && !status->stopped;
    if(status->stopped) snprintf(status->message, sizeof(status->message), "Nightly update stopped");
    if(progress) progress(status);
    if(log) log("GitHub nightly: %s", status->message);
    return ok;
}
