/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/update_nightly.h"
#include "kui/update.h"
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
static bool parse_manifest(const struct manifest_buffer *buffer, struct kui_nightly_asset *asset) {
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

static void verified_bytes(void *context, uint32_t bytes) {
    struct job *job = context;
    job->status->done = bytes;
    if(!bytes) message(job, "Verifying nightly on storage");
    else if(job->progress) job->progress(job->status);
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

/* The private partial belongs to the manifest recorded by prepare_partial.
 * Its length is the resume offset. Re-read all bytes before publication. */
static bool download(struct kui_internet *session, const struct kui_nightly_asset *asset,
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

bool kui_update_nightly_download(enum kui_internet_route route, const char *running_build, unsigned keep,
        char path[KUI_UPDATE_BROWSER_PATH_BYTES], struct kui_app_status *status,
        kui_log_fn log, kui_cancel_fn cancel, kui_app_progress_fn progress) {
    if(!path || !status) return false;
    *path = 0;
    memset(status, 0, sizeof(*status));
    struct job job = { .status = status, .progress = progress, .cancel = cancel };
    bool ok = false, tracked = true;
    char problem[128] = "";
    struct kui_internet *session = NULL;
    if(!kui_update_keep_valid(keep)) {
        message(&job, "Invalid nightly retention setting");
        goto done;
    }
    message(&job, "Connecting to GitHub for nightly update");
    session = kui_internet_open(route, log, cancel, problem);
    if(!session) {
        message(&job, problem[0] ? problem : "Cannot open network connection");
        goto done;
    }
    struct manifest_buffer buffer = {0};
    struct kui_nightly_asset asset;
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
        goto trim;
    }
    if(!ensure_directory("0:/KUI") || !ensure_directory(KUI_UPDATE_BROWSER_ROOT) ||
       !ensure_directory(KUI_NIGHTLY_DIRECTORY) || !ensure_directory(KUI_NIGHTLY_STATE)) {
        message(&job, "Cannot create nightly download folders on storage");
        goto done;
    }
    status->total = asset.size;
    snprintf(status->lines[0], sizeof(status->lines[0]), "GitHub %.12s   %lu bytes",
        asset.commit, (unsigned long)asset.size);
    status->line_count = 1;
    char final[KUI_UPDATE_BROWSER_PATH_BYTES];
    snprintf(final, sizeof(final), KUI_NIGHTLY_PACKAGE, asset.commit);
    enum kui_nightly_verify existing = kui_nightly_verify_file(final, &asset, cancel, verified_bytes, &job);
    if(existing == KUI_NIGHTLY_MISMATCH) {
        message(&job, "A different file occupies the nightly path. Move it before retrying.");
        goto done;
    }
    if(existing == KUI_NIGHTLY_MISSING) {
        if(!kui_nightly_prepare_partial(&asset)) {
            message(&job, "Cannot prepare the nightly resume slot on storage");
            goto done;
        }
        if(!download(session, &asset, KUI_NIGHTLY_PART, &job)) goto done;
        enum kui_nightly_verify checked = kui_nightly_verify_file(KUI_NIGHTLY_PART, &asset, cancel, verified_bytes, &job);
        if(checked != KUI_NIGHTLY_VERIFIED) {
            if(checked == KUI_NIGHTLY_MISMATCH) {
                if(f_unlink(KUI_NIGHTLY_PART) == FR_OK) message(&job, "Nightly SHA-256 or size mismatch. Retry to download again.");
                else message(&job, "Nightly failed verification. Cannot remove damaged partial file.");
            } else message(&job, "Could not verify nightly on storage");
            goto done;
        }
        if(cancel && cancel()) goto done;
        if(f_rename(KUI_NIGHTLY_PART, final) != FR_OK) {
            message(&job, "Cannot publish verified nightly on storage");
            goto done;
        }
        /* A reset in this window can leave an untracked verified package.
         * Never pre-claim a destination that publication might fail to fill. */
        tracked = kui_nightly_record_download(&asset, cancel);
    } else if(existing != KUI_NIGHTLY_VERIFIED) {
        message(&job, "Cannot verify existing nightly package");
        goto done;
    }
    ok = true;
    strcpy(path, final);
    message(&job, tracked ? "Verified nightly ready for review" :
        "Verified nightly ready. Could not record it for automatic cleanup.");
trim:
    kui_internet_close(session);
    session = NULL;
    if(tracked && !kui_nightly_trim(keep, asset.commit, cancel))
        message(&job, *path ? "Nightly ready. Download cleanup incomplete. Retry the check." :
            "Already running the nightly. Download cleanup incomplete. Retry the check.");
    if(cancel && cancel()) {
        ok = false;
        *path = 0;
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
