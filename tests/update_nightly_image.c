/* SPDX-License-Identifier: GPL-3.0-only */
/* Exercise the console downloader on real media images. Only the HTTPS origin
 * and block device are fixtures. The separate transport suite uses real TLS. */
#include "kui/update_nightly.h"
#include "kui/media.h"
#include "kui/hash.h"
#include "ff.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define COMMIT "1234567890abcdef1234567890abcdef12345678"
#define BASE KUI_UPDATE_BROWSER_ROOT "/nightly-" COMMIT
#if KUI_UPDATE_REVIEW_FEED
#define EXPECT_MANIFEST "https://raw.githubusercontent.com/TacitusVox/K-UI-NeXT/refs/heads/codex/updater-overhaul/evidence/updater-bba/nightly.txt"
#define EXPECT_ASSET "https://github.com/TacitusVox/K-UI-NeXT/raw/refs/heads/codex/updater-overhaul/evidence/updater-bba/K-UI.kup"
#else
#define EXPECT_MANIFEST "https://raw.githubusercontent.com/TacitusVox/K-UI-NeXT/nightly-channel/nightly.txt"
#define EXPECT_ASSET "https://github.com/TacitusVox/K-UI-NeXT/releases/download/nightly-" COMMIT "/K-UI.kup"
#endif
static FILE *image;
static uint8_t package[204800];
static char manifest[200], path[KUI_UPDATE_BROWSER_PATH_BYTES];
static uint32_t requested, size, opens, closes, transfers;
static bool synced_checkpoint;
static bool interrupt, ignore_range, stopped;
static struct kui_app_status status;
static const char *running;

static uint64_t blocks(void *ctx) { (void)ctx; return size; }
static int read_blocks(void *ctx, uint32_t block, size_t count, uint8_t *out) {
    (void)ctx;
    return fseek(image, (long)block*512, SEEK_SET) || fread(out, 512, count, image) != count ? -1 : 0;
}
static int write_blocks(void *ctx, uint32_t block, size_t count, const uint8_t *in) {
    (void)ctx;
    return fseek(image, (long)block*512, SEEK_SET) || fwrite(in, 512, count, image) != count ? -1 : 0;
}
static int sync_blocks(void *ctx) { (void)ctx; synced_checkpoint = true; return fflush(image); }
static const struct kui_media_ops media = {NULL, blocks, read_blocks, write_blocks, sync_blocks};
uint32_t kui_clock_fattime(void) { return ((2026u-1980u)<<25) | (11u<<21) | (18u<<16); }

struct kui_internet { int id; };
static struct kui_internet session;
struct kui_internet *kui_internet_open(enum kui_internet_route route, kui_log_fn log, kui_cancel_fn cancel, char why[128]) {
    assert(route == KUI_INTERNET_DREAMPI && !log && cancel && !cancel());
    (void)why;
    ++opens;
    return &session;
}
const char *kui_internet_problem(const struct kui_internet *s) { assert(s == &session); return "Network error"; }
void kui_internet_close(struct kui_internet *s) { assert(s == &session); ++closes; }

enum kui_http_result kui_internet_get(struct kui_internet *s, const char *url, uint32_t limit,
        kui_http_write_fn write, void *ctx, kui_http_progress_fn progress, void *progress_ctx) {
    (void)progress; (void)progress_ctx;
    assert(s == &session && !strcmp(url, EXPECT_MANIFEST));
    size_t n = strlen(manifest);
    if(n > limit) return KUI_HTTP_TOO_LARGE;
    assert(write(ctx, manifest, n/2) && write(ctx, manifest + n/2, n - n/2));
    return KUI_HTTP_OK;
}
enum kui_http_result kui_internet_get_remainder(struct kui_internet *s, const char *url, uint32_t start,
        uint32_t total, kui_http_write_fn write, void *ctx, kui_http_progress_fn progress, void *progress_ctx) {
    assert(s == &session && !strcmp(url, EXPECT_ASSET));
    assert(total == sizeof(package));
    ++transfers;
    requested = start;
    if(start && ignore_range) return KUI_HTTP_RANGE_IGNORED;
    uint32_t n = interrupt ? 140000 : total - start;
    synced_checkpoint = false;
    assert(write(ctx, package + start, n));
    if(start / 131072u < (start+n) / 131072u) assert(synced_checkpoint);
    progress(progress_ctx, n, total - start);
    stopped = interrupt;
    return stopped ? KUI_HTTP_STOPPED : KUI_HTTP_OK;
}
static bool cancel(void) { return stopped; }
static void report(const struct kui_app_status *v) { assert(v->done <= v->total); }
static bool fetch(void) {
    stopped = false;
    strcpy(path, "stale");
    bool ok = kui_update_nightly_download(KUI_INTERNET_DREAMPI, running, path, &status, NULL, cancel, report);
    assert(status.complete && ok == status.passed && (ok || !path[0]) && opens == closes);
    return ok;
}
static void set_manifest(void) {
    struct kui_sha256 sha;
    uint8_t digest[32];
    char hex[65];
    kui_sha256_init(&sha);
    kui_sha256_update(&sha, package, sizeof(package));
    kui_sha256_digest(&sha, digest);
    kui_hex(digest, 32, hex);
    snprintf(manifest, sizeof(manifest), "KUI-NIGHTLY-1\n" COMMIT "\n%u\n%s\n", (unsigned)sizeof(package), hex);
}
static void verify_cached(void) {
    assert(!strcmp(path, BASE ".kup"));
    assert(kui_update_browser_package(path) && !kui_update_browser_package(BASE ".part"));
    FIL file;
    UINT n;
    uint8_t saved[sizeof(package)];
    assert(f_open(&file, path, FA_READ) == FR_OK && f_size(&file) == sizeof(package));
    assert(f_read(&file, saved, sizeof(saved), &n) == FR_OK && n == sizeof(saved) && !memcmp(saved, package, n));
    assert(f_close(&file) == FR_OK);
}
int main(int argc, char **argv) {
    assert(argc == 2 && (image = fopen(argv[1], "r+b")) && !fseek(image, 0, SEEK_END));
    size = (uint32_t)(ftell(image) / 512);
    FATFS fs;
    kui_media_set(&media);
    assert(f_mount(&fs, "0:", 1) == FR_OK);
    for(unsigned i = 0; i < sizeof(package); ++i) package[i] = (uint8_t)(i*17u);
    set_manifest();
    running = "1234567890ab";
    assert(fetch() && !path[0] && !transfers && strstr(status.message, "Already running"));
    FILINFO uncreated;
    assert(f_stat("0:/KUI", &uncreated) == FR_NO_FILE);
    running = "1234567890ax";
    manifest[20] = '/';
    assert(!fetch() && !transfers);
    set_manifest();
    memcpy(strstr(manifest, "204800"), "000000", 6);
    assert(!fetch() && !transfers);
    set_manifest();
    strcat(manifest, "ignored\n");
    assert(!fetch() && !transfers);
    set_manifest();
    interrupt = true;
    assert(!fetch() && status.stopped && transfers == 1 && !requested);
    FILINFO info;
    assert(f_stat(BASE ".part", &info) == FR_OK && info.fsize == 140000 && f_stat(BASE ".kup", &info) == FR_NO_FILE);
    interrupt = false;
    assert(fetch() && transfers == 2 && requested == 140000);
    verify_cached();
    assert(f_stat(BASE ".part", &info) == FR_NO_FILE);
    assert(fetch() && transfers == 2);
    verify_cached();
    /* Same size, different contents: never trust a cache hit by its name. */
    FIL file;
    UINT n;
    assert(f_open(&file, BASE ".kup", FA_WRITE) == FR_OK && f_write(&file, "bad", 3, &n) == FR_OK && n == 3);
    assert(f_close(&file) == FR_OK && fetch() && transfers == 3);
    verify_cached();
    assert(f_unlink(path) == FR_OK);
    interrupt = true;
    assert(!fetch() && transfers == 4 && status.stopped);
    interrupt = false;
    ignore_range = true;
    assert(fetch() && transfers == 6 && !requested);
    verify_cached();
    assert(f_unlink(path) == FR_OK);
    package[0] ^= 1;
    assert(!fetch() && transfers == 7 && strstr(status.message, "mismatch"));
    assert(f_stat(BASE ".part", &info) == FR_NO_FILE && f_stat(BASE ".kup", &info) == FR_NO_FILE);
    assert(f_mount(NULL, "0:", 0) == FR_OK && !fclose(image));
    puts("PASS nightly manifest, interruption, resume, ignored Range, cache readback, damaged cache repair, digest refusal");
}
