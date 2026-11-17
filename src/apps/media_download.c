/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/media_download.h"
#include "kui/media_container.h"
#include "kui/cover_image.h"
#include "kui/games_covers.h"
#include "kui/games_media_storage.h"
#include "kui/games_storage.h"
#include "kui/game_launch_settings.h"
#include "kui/hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define MEDIA_DIR "0:/KUI/media"
/* Sole storage worker owns this staging file; restart can reclaim it. */
#define MEDIA_TEMP MEDIA_DIR "/fetch.tmp"
#define HEADER_BYTES 4096u
#define BODY_CHUNK 8192u
#define CONNECT_MS 10000u
#define HEADER_MS 600000u /* The companion may be fetching and converting. */
#define BODY_IDLE_MS 20000u
#define TOTAL_MS 1080000u
struct asset_type { const char *name, *mime, *ext; uint32_t maximum; };
static const struct asset_type types[] = {
    {"cover", "image/png", "png", 256u * 1024u},
    {"manual", "application/x-kui-manual", "kdm", KUI_MANUAL_BYTES_MAX},
    {"preview", "application/x-kui-preview", "kpv", KUI_PREVIEW_HEADER_BYTES + KUI_PREVIEW_FRAMES_MAX * KUI_PREVIEW_FRAME_BYTES}
};
static bool stopped(kui_cancel_fn cancel) { return cancel && cancel(); }

bool kui_media_file_path(const struct kui_games_entry *entry, enum kui_media_kind kind,
        char out[KUI_GAMES_FILE_CAP]) {
    if(!entry || !out || (unsigned)kind > KUI_MEDIA_PREVIEW ||
       !memchr(entry->name, 0, sizeof(entry->name)) || !memchr(entry->path, 0, sizeof(entry->path))) return false;
    return kui_games_media_path(entry->path, kind == KUI_MEDIA_COVER ? KUI_DOWNLOADED_COVERS : MEDIA_DIR,
        types[kind].ext, out, KUI_GAMES_FILE_CAP);
}

/* IP.BIN fields have fixed limits. Encode even spaces, ampersands, CR,
 * percent signs and non-ASCII bytes before building the HTTP request. */
static char *encode(char *dst, const char *src) {
    static const char hex[] = "0123456789ABCDEF";
    for(const unsigned char *s = (const unsigned char *)src; *s; ++s) {
        unsigned c = *s;
        if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c == '.' || c == '~') *dst++ = (char)c;
        else { *dst++ = '%'; *dst++ = hex[c >> 4]; *dst++ = hex[c & 15]; }
    }
    *dst = 0;
    return dst;
}
static size_t request(char buf[1024], const struct asset_type *type, const uint8_t ip[4],
        uint16_t port, const struct kui_games_disc_identity *id) {
    char *at = buf + sprintf(buf, "GET /v1/dreamcast/%s?product=", type->name);
    at = encode(at, id->product);
    memcpy(at, "&region=", 8); at = encode(at+8, id->region);
    memcpy(at, "&title=", 7); at = encode(at+7, id->title);
    at += sprintf(at, " HTTP/1.0\r\nHost: %u.%u.%u.%u:%u\r\nConnection: close\r\n\r\n",
        (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3], (unsigned)port);
    return (size_t)(at - buf);
}

static bool decimal(const char *value, uint32_t maximum, uint32_t *out) {
    uint32_t n = 0;
    if(!*value) return false;
    while(*value) {
        unsigned digit = (unsigned)(*value++ - '0');
        if(digit > 9u || n > (maximum-digit)/10u) return false;
        n = n*10u + digit;
    }
    *out = n;
    return n > 0;
}
static bool header_status(const char *line, unsigned *code) {
    if(strncmp(line, "HTTP/1.0 ", 9) && strncmp(line, "HTTP/1.1 ", 9)) return false;
    const char *p = line + 9;
    for(unsigned i = 0; i < 3; ++i) if(p[i] < '0' || p[i] > '9') return false;
    if(p[3] && p[3] != ' ') return false;
    *code = (unsigned)((p[0]-'0')*100 + (p[1]-'0')*10 + p[2]-'0');
    return true;
}
/* Header includes the final CRLFCRLF, is NUL-terminated and mutable. */
static enum kui_media_result parse_header(char *data, const struct asset_type *type, uint32_t *length, kui_log_fn log) {
    char *next = strstr(data, "\r\n");
    if(!next) return KUI_MEDIA_ERROR;
    *next = 0;
    unsigned status;
    if(!header_status(data, &status)) return KUI_MEDIA_ERROR;
    if(status != 200 && log) log("Media: companion returned HTTP %u for %s", status, type->name);
    if(status == 404 || status == 413 || status == 415 || status == 422) return KUI_MEDIA_MISSING;
    if(status == 429) return KUI_MEDIA_LIMITED;
    if(status != 200) return KUI_MEDIA_ERROR;
    bool have_length = false, have_type = false;
    for(char *line = next+2; strcmp(line, "\r\n"); line = next+2) {
        next = strstr(line, "\r\n");
        if(!next || *line == ' ' || *line == '\t') return KUI_MEDIA_ERROR;
        *next = 0;
        if(strpbrk(line, "\r\n")) return KUI_MEDIA_ERROR;
        char *value = strchr(line, ':');
        if(!value || value == line) return KUI_MEDIA_ERROR;
        *value++ = 0;
        for(const char *p = line; *p; ++p)
            if((unsigned char)*p <= 32u || (unsigned char)*p >= 127u) return KUI_MEDIA_ERROR;
        while(*value == ' ' || *value == '\t') ++value;
        char *end = next;
        while(end > value && (end[-1] == ' ' || end[-1] == '\t')) *--end = 0;
        if(!strcasecmp(line, "Content-Length")) {
            if(have_length || !decimal(value, type->maximum, length)) return KUI_MEDIA_ERROR;
            have_length = true;
        } else if(!strcasecmp(line, "Content-Type")) {
            if(have_type || strcasecmp(value, type->mime)) return KUI_MEDIA_ERROR;
            have_type = true;
        } else if(!strcasecmp(line, "Transfer-Encoding") ||
                 (!strcasecmp(line, "Content-Encoding") && strcasecmp(value, "identity"))) return KUI_MEDIA_ERROR;
    }
    return have_length && have_type ? KUI_MEDIA_INSTALLED : KUI_MEDIA_ERROR;
}

struct fetch_work {
    char header[HEADER_BYTES+1];
    uint8_t chunk[BODY_CHUNK];
    FIL file;
    bool opened;
    kui_media_progress_fn progress;
    void *progress_ctx;
};
static enum kui_media_result download(struct fetch_work *w, const struct kui_bba_port *port,
        struct kui_bba_session *s, const uint8_t ip[4], uint16_t port_number,
        const struct asset_type *type, const struct kui_games_disc_identity *id,
        kui_log_fn log, kui_cancel_fn cancel) {
    const struct kui_net_sockets *net = &s->net;
    if(!net->count || !net->connect(net->ctx, 0, 0, ip, port_number)) return KUI_MEDIA_ERROR;
    char request_text[1024];
    size_t request_bytes = request(request_text, type, ip, port_number, id);
    size_t sent = 0, headers = 0;
    uint32_t expected = 0, body = 0;
    uint64_t start = port->now_ms(), deadline = start + CONNECT_MS, last_update = start;
    enum kui_media_result result = KUI_MEDIA_ERROR;
    for(;;) {
        if(stopped(cancel)) { result = KUI_MEDIA_STOPPED; break; }
        uint64_t now = port->now_ms();
        if(now >= deadline || now-start >= TOTAL_MS) {
            if(log) log("Media: companion timed out during %s", !sent ? "connect" : !expected ? "response" : "transfer");
            break;
        }
        uint8_t state;
        if(!port->poll(s) || !net->state(net->ctx, 0, &state)) break;
        if(state == KUI_NET_CONNECTING) { port->pause(2); continue; }
        if(state != KUI_NET_ESTABLISHED && state != KUI_NET_PEER_CLOSED) break;
        if(sent < request_bytes) {
            size_t room = 0;
            if(!net->room(net->ctx, 0, &room)) break;
            if(!room) { port->pause(2); continue; }
            size_t n = request_bytes-sent < room ? request_bytes-sent : room;
            if(!net->send(net->ctx, 0, request_text+sent, n)) break;
            sent += n;
            if(sent == request_bytes) {
                deadline = now+HEADER_MS;
                if(w->progress) w->progress(w->progress_ctx, 0, 0);
            }
            continue;
        }
        size_t available = 0;
        if(!net->received(net->ctx, 0, &available)) break;
        if(!available) {
            if(state == KUI_NET_PEER_CLOSED) break;
            port->pause(2);
            continue;
        }
        if(available > sizeof(w->chunk)) available = sizeof(w->chunk);
        if(!net->receive(net->ctx, 0, w->chunk, available)) break;
        size_t pos = 0;
        if(!expected) {
            while(pos < available && headers < HEADER_BYTES) {
                uint8_t c = w->chunk[pos++];
                if((c < 32 && c != '\r' && c != '\n' && c != '\t') || c == 127) goto done;
                w->header[headers++] = (char)c;
                if(headers >= 4 && !memcmp(w->header+headers-4, "\r\n\r\n", 4)) {
                    w->header[headers] = 0;
                    result = parse_header(w->header, type, &expected, log);
                    if(result != KUI_MEDIA_INSTALLED) goto done;
                    result = KUI_MEDIA_ERROR;
                    if(f_open(&w->file, MEDIA_TEMP, FA_READ | FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) goto done;
                    w->opened = true;
                    if(w->progress) w->progress(w->progress_ctx, 0, expected);
                    deadline = now+BODY_IDLE_MS;
                    break;
                }
            }
            if(!expected && headers == HEADER_BYTES) break;
        }
        size_t count = available-pos;
        if(!count) continue;
        UINT written = 0;
        if(count > expected-body || f_write(&w->file, w->chunk+pos, (UINT)count, &written) != FR_OK ||
           written != count) break;
        body += written;
        if(w->progress && (body == expected || now-last_update >= 250u)) {
            w->progress(w->progress_ctx, body, expected);
            last_update = now;
        }
        deadline = now+BODY_IDLE_MS;
        if(body == expected) { result = KUI_MEDIA_INSTALLED; break; }
    }
done:
    if(!net->close(net->ctx, 0) && result == KUI_MEDIA_INSTALLED) result = KUI_MEDIA_ERROR;
    return result;
}
static uint32_t big32(const uint8_t *p) {
    return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3];
}
static bool complete_png(const uint8_t *data, size_t size) {
    size_t at = 8;
    while(size-at >= 12) {
        uint32_t n = big32(data+at);
        if(n > size-at-12 || kui_crc32_capture(0, data+at+4, n+4) != big32(data+at+8+n)) return false;
        if(!memcmp(data+at+4, "IEND", 4)) return !n && at+12 == size;
        at += n+12;
    }
    return false;
}
static bool valid_png(FIL *f) {
    size_t size = (size_t)f_size(f);
    if(size < 33 || size > types[KUI_MEDIA_COVER].maximum) return false;
    uint8_t *bytes = malloc(size), *pixels = NULL;
    unsigned width = 0, height = 0, channels = 0;
    UINT got = 0;
    bool ok = bytes && f_read(f, bytes, (UINT)size, &got) == FR_OK && got == size &&
        !memcmp(bytes, "\211PNG\r\n\032\n\0\0\0\rIHDR", 16);
    if(ok) {
        /* Keep PNG decompression inside the provider's 512x512 envelope. */
        uint32_t w = big32(bytes+16), h = big32(bytes+20);
        ok = w && h && w <= 512 && h <= 512 && complete_png(bytes, size) &&
            kui_cover_image_decode(bytes, size, &pixels, &width, &height, &channels) == KUI_COVER_IMAGE_OK;
    }
    kui_cover_image_free(pixels);
    free(bytes);
    return ok;
}
static bool ensure_dir(const char *path) {
    FILINFO info;
    FRESULT r = f_stat(path, &info);
    if(r == FR_OK) return (info.fattrib & AM_DIR) != 0;
    return (r == FR_NO_FILE || r == FR_NO_PATH) && f_mkdir(path) == FR_OK;
}
static enum kui_media_result fetch_mounted(struct fetch_work *work, const struct kui_bba_port *port,
        struct kui_bba_session *s, const uint8_t ip[4], uint16_t number, const struct kui_games_entry *entry,
        enum kui_media_kind kind, const char *final, kui_log_fn log, kui_cancel_fn cancel) {
    if(kind == KUI_MEDIA_COVER) {
        if(entry->cover_path[0]) return KUI_MEDIA_EXISTING;
        const char *leaf = strrchr(entry->path, '/');
        if(!leaf || !kui_game_image_name_supported(leaf+1)) return KUI_MEDIA_UNIDENTIFIED;
        if(strlen(entry->path) >= KUI_COVER_PATH_CAP) return KUI_MEDIA_MISSING;
        char key[KUI_COVER_PATH_CAP];
        if(!kui_cover_key(entry->name, key)) return KUI_MEDIA_MISSING;
        bool present;
        if(!kui_games_prepare_media_cover(entry, &present, log, cancel)) return KUI_MEDIA_ERROR;
        if(present) return KUI_MEDIA_EXISTING;
    } else {
        FRESULT r = f_open(&work->file, final, FA_READ);
        if(r == FR_OK) {
            bool valid = f_size(&work->file) <= types[kind].maximum &&
                kui_media_container_check(&work->file, kind == KUI_MEDIA_MANUAL);
            if(f_close(&work->file) != FR_OK) valid = false;
            if(!valid && log) log("Media: existing file is unreadable; check %s", final+2);
            return valid ? KUI_MEDIA_EXISTING : KUI_MEDIA_ERROR;
        }
        if(r != FR_NO_FILE && r != FR_NO_PATH) return KUI_MEDIA_ERROR;
    }
    struct kui_games_disc_identity identity;
    enum kui_games_identity_result id = kui_games_media_identity(entry->path, &identity, log, cancel);
    if(id != KUI_GAMES_IDENTITY_FOUND) return id == KUI_GAMES_IDENTITY_UNKNOWN ?
        KUI_MEDIA_UNIDENTIFIED : KUI_MEDIA_ERROR;
    if(stopped(cancel)) return KUI_MEDIA_STOPPED;
    if(!ensure_dir("0:/KUI") || !ensure_dir(MEDIA_DIR) ||
       (kind == KUI_MEDIA_COVER && !ensure_dir(KUI_DOWNLOADED_COVERS))) return KUI_MEDIA_ERROR;
    enum kui_media_result result = download(work, port, s, ip, number, &types[kind], &identity, log, cancel);
    if(work->opened) {
        if(result == KUI_MEDIA_INSTALLED) {
            bool valid = f_sync(&work->file) == FR_OK && f_lseek(&work->file, 0) == FR_OK;
            if(valid) {
                if(kind == KUI_MEDIA_COVER) valid = valid_png(&work->file);
                else if(kind == KUI_MEDIA_MANUAL) valid = kui_manual_check_pages(&work->file, cancel);
                else valid = kui_media_container_check(&work->file, false);
            }
            if(!valid) result = KUI_MEDIA_ERROR;
        }
        if(f_close(&work->file) != FR_OK) result = KUI_MEDIA_ERROR;
        if(stopped(cancel)) result = KUI_MEDIA_STOPPED;
        /* FatFs rename fails if the destination already exists, preserving
         * the existing asset even if its filename aliases the requested one. */
        if(result == KUI_MEDIA_INSTALLED && f_rename(MEDIA_TEMP, final) != FR_OK) result = KUI_MEDIA_ERROR;
        if(result != KUI_MEDIA_INSTALLED) (void)f_unlink(MEDIA_TEMP);
        else if(kind == KUI_MEDIA_COVER) {
            bool present = false;
            if(!kui_games_prepare_media_cover(entry, &present, log, cancel) || !present)
                result = KUI_MEDIA_ERROR;
        }
    }
    return result;
}
enum kui_media_result kui_media_fetch_progress(const struct kui_bba_port *port, struct kui_bba_session *s,
        const uint8_t ip[4], uint16_t number, const struct kui_games_entry *entry, enum kui_media_kind kind,
        kui_log_fn log, kui_cancel_fn cancel, kui_media_progress_fn progress, void *ctx) {
    if(stopped(cancel)) return KUI_MEDIA_STOPPED;
    char final[KUI_GAMES_FILE_CAP];
    if(!port || !s || !ip || !number || !entry || entry->disabled || entry->directory) return KUI_MEDIA_ERROR;
    if(!kui_media_file_path(entry, kind, final)) return KUI_MEDIA_MISSING;
    struct fetch_work *work = calloc(1, sizeof(*work));
    if(!work) return KUI_MEDIA_ERROR;
    work->progress = progress;
    work->progress_ctx = ctx;
    enum kui_media_result result = KUI_MEDIA_ERROR;
    if(kui_games_storage_open(log) == KUI_GAMES_STORAGE_OK) {
        result = fetch_mounted(work, port, s, ip, number, entry, kind, final, log, cancel);
        if(!kui_games_storage_done(result != KUI_MEDIA_ERROR)) result = KUI_MEDIA_ERROR;
    }
    free(work);
    return stopped(cancel) ? KUI_MEDIA_STOPPED : result;
}
enum kui_media_result kui_media_fetch(const struct kui_bba_port *port, struct kui_bba_session *s,
        const uint8_t ip[4], uint16_t number, const struct kui_games_entry *entry, enum kui_media_kind kind,
        kui_log_fn log, kui_cancel_fn cancel) {
    return kui_media_fetch_progress(port, s, ip, number, entry, kind, log, cancel, NULL, NULL);
}
