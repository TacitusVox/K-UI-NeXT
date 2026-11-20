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
#define CACHED KUI_UPDATE_BROWSER_ROOT "/GitHub/nightly-"
#define PART "0:/KUI/nightly-cache/download.part"
#define REVIEW_BASE "https://raw.githubusercontent.com/TacitusVox/K-UI-NeXT/refs/heads/codex/themes-wince-bba-ftp/evidence/themes-bba/"
#define EXPECT_MANIFEST "https://raw.githubusercontent.com/TacitusVox/K-UI-NeXT/nightly-channel/nightly.txt"
#define RELEASE_BASE "https://github.com/TacitusVox/K-UI-NeXT/releases/download/nightly-"
static bool test_feed;
static FILE *image;
static uint8_t package[204800];
static char commit[41] = COMMIT, manifest[200], path[KUI_UPDATE_BROWSER_PATH_BYTES];
static unsigned keep = KUI_UPDATE_KEEP_DEFAULT;
static uint32_t requested, size, opens, closes, transfers;
static bool synced_checkpoint;
static void (*after_transfer)(void);
static long corrupt_on_next_write = -1;
static bool interrupt, ignore_range, stopped;
static struct kui_app_status status;
static const char *running;
static enum kui_internet_route selected_route = KUI_INTERNET_DREAMPI;

static uint64_t blocks(void *ctx) { (void)ctx; return size; }
static int read_blocks(void *ctx, uint32_t block, size_t count, uint8_t *out) {
    (void)ctx;
    return fseek(image, (long)block*512, SEEK_SET) || fread(out, 512, count, image) != count ? -1 : 0;
}
static int write_blocks(void *ctx, uint32_t block, size_t count, const uint8_t *in) {
    (void)ctx;
    if(fseek(image, (long)block*512, SEEK_SET) || fwrite(in, 512, count, image) != count) return -1;
    if(corrupt_on_next_write >= 0) {
        assert(!fseek(image, corrupt_on_next_write, SEEK_SET) && fputc(0xe7, image) != EOF);
        corrupt_on_next_write = -1;
    }
    return 0;
}
static int sync_blocks(void *ctx) { (void)ctx; synced_checkpoint = true; return fflush(image); }
static const struct kui_media_ops media = {NULL, blocks, read_blocks, write_blocks, sync_blocks};
uint32_t kui_clock_fattime(void) { return ((2026u-1980u)<<25) | (11u<<21) | (18u<<16); }

struct kui_internet { int id; };
static struct kui_internet session;
struct kui_internet *kui_internet_open(enum kui_internet_route route, kui_log_fn log, kui_cancel_fn cancel, char why[128]) {
    assert(route == selected_route && !log && cancel && !cancel());
    (void)why;
    ++opens;
    return &session;
}
const char *kui_internet_problem(const struct kui_internet *s) { assert(s == &session); return "Network error"; }
void kui_internet_close(struct kui_internet *s) {
    assert(s == &session);
    ++closes;
    if(after_transfer) after_transfer();
}

enum kui_http_result kui_internet_get(struct kui_internet *s, const char *url, uint32_t limit,
        kui_http_write_fn write, void *ctx, kui_http_progress_fn progress, void *progress_ctx) {
    (void)progress; (void)progress_ctx;
    assert(s == &session && !strcmp(url, test_feed ? REVIEW_BASE "nightly.txt" : EXPECT_MANIFEST));
    size_t n = strlen(manifest);
    if(n > limit) return KUI_HTTP_TOO_LARGE;
    assert(write(ctx, manifest, n/2) && write(ctx, manifest + n/2, n - n/2));
    return KUI_HTTP_OK;
}
enum kui_http_result kui_internet_get_remainder(struct kui_internet *s, const char *url, uint32_t start,
        uint32_t total, kui_http_write_fn write, void *ctx, kui_http_progress_fn progress, void *progress_ctx) {
    char expected[160];
    snprintf(expected, sizeof(expected), RELEASE_BASE "%s/K-UI.kup", commit);
    assert(!strcmp(url, test_feed ? REVIEW_BASE "K-UI.kup" : expected));
    assert(s == &session);
    assert(total == sizeof(package));
    ++transfers;
    requested = start;
    if(start && ignore_range) return KUI_HTTP_RANGE_IGNORED;
    uint32_t n = total - start;
    if(interrupt && n > 140000) n = 140000;
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
    bool ok = kui_update_nightly_download(selected_route, test_feed, running, keep, path, &status, NULL, cancel, report);
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
    snprintf(manifest, sizeof(manifest), "KUI-NIGHTLY-1\n%s\n%u\n%s\n", commit, (unsigned)sizeof(package), hex);
}
static void verify_cached(void) {
    char expected[KUI_UPDATE_BROWSER_PATH_BYTES];
    snprintf(expected, sizeof(expected), CACHED "%s.kup", commit);
    assert(!strcmp(path, expected));
    assert(kui_update_browser_package(path) && !kui_update_browser_package(PART));
    FIL file;
    UINT n;
    uint8_t saved[sizeof(package)];
    assert(f_open(&file, path, FA_READ) == FR_OK && f_size(&file) == sizeof(package));
    assert(f_read(&file, saved, sizeof(saved), &n) == FR_OK && n == sizeof(saved) && !memcmp(saved, package, n));
    assert(f_close(&file) == FR_OK);
}
static void write_bytes(const char *name, const void *data, UINT bytes, BYTE flags) {
    FIL file;
    UINT count;
    assert(f_open(&file, name, flags) == FR_OK);
    assert(f_write(&file, data, bytes, &count) == FR_OK && count == bytes);
    assert(f_close(&file) == FR_OK);
}

static void expect_bytes(const char *name, const void *data, UINT bytes, FSIZE_t length) {
    FIL file;
    UINT count;
    char saved[32];
    assert(bytes <= sizeof(saved));
    assert(f_open(&file, name, FA_READ) == FR_OK && f_size(&file) == length);
    assert(f_read(&file, saved, bytes, &count) == FR_OK && count == bytes && !memcmp(saved, data, bytes));
    assert(f_close(&file) == FR_OK);
}

static void build_id(unsigned build, char id[41]) {
    /* The clock is fixed for this entire test, and commit names sort in
     * a different order from download completion. */
    snprintf(id, 41, "%02x%038x", (build * 97u) & 255u, build);
}

static void choose_build(unsigned build) {
    build_id(build, commit);
    package[0] = (uint8_t)build;
    set_manifest();
}

static void build_path(unsigned build, char out[KUI_UPDATE_BROWSER_PATH_BYTES]) {
    char id[41];
    build_id(build, id);
    snprintf(out, KUI_UPDATE_BROWSER_PATH_BYTES, CACHED "%s.kup", id);
}

static void expect_build(unsigned build, bool present) {
    char name[KUI_UPDATE_BROWSER_PATH_BYTES];
    FILINFO info;
    build_path(build, name);
    assert(f_stat(name, &info) == (present ? FR_OK : FR_NO_FILE));
    if(present) assert(info.fsize == sizeof(package));
}

static char fault_path[KUI_UPDATE_BROWSER_PATH_BYTES];

static void change_published_file(void) {
    after_transfer = NULL;
    write_bytes(fault_path, "bad", 3, FA_WRITE);
}

static void arm_retention_fault(void) {
    after_transfer = NULL;
    FIL file;
    assert(f_open(&file, fault_path, FA_READ) == FR_OK && file.obj.sclust >= 2);
    assert(file.obj.fs->csize);
    corrupt_on_next_write = 512L * (file.obj.fs->database +
        (file.obj.sclust - 2) * file.obj.fs->csize);
    assert(f_close(&file) == FR_OK);
}

static void test_late_damage(void) {
    unsigned before = transfers;
    keep = 0;
    choose_build(26);
    build_path(26, fault_path);
    after_transfer = change_published_file;
    assert(!fetch() && !path[0] && transfers == before + 1 && !after_transfer);
    expect_bytes(fault_path, "bad", 3, sizeof(package));

    choose_build(27);
    assert(fetch());
    choose_build(28);
    assert(fetch());
    build_path(28, fault_path);
    before = transfers;
    keep = 1;
    after_transfer = arm_retention_fault;
    assert(!fetch() && !path[0] && transfers == before && !after_transfer && corrupt_on_next_write < 0);
    expect_bytes(fault_path, "\xe7", 1, sizeof(package));
    expect_build(27, false);
}

static void test_retention(FATFS *fs) {
    static const char manual[] = KUI_UPDATE_BROWSER_ROOT "/nightly-" COMMIT ".kup";
    static const char manual_zip[] = KUI_UPDATE_BROWSER_ROOT "/local.zip";
    char collision[KUI_UPDATE_BROWSER_PATH_BYTES], exact_copy[KUI_UPDATE_BROWSER_PATH_BYTES];
    FILINFO info;
    running = NULL;
    selected_route = KUI_INTERNET_BBA;
    ignore_range = false;
    write_bytes(manual, "user", 4, FA_WRITE | FA_CREATE_NEW);
    write_bytes(manual_zip, "test", 4, FA_WRITE | FA_CREATE_NEW);

    /* Changing the feed after an interrupted transfer starts the new commit
     * at zero, even if both assets are the same size. */
    choose_build(1);
    interrupt = true;
    assert(!fetch() && status.stopped && !requested);
    /* Even a republished asset under the same source commit is a distinct
     * transfer when its authenticated digest changes. */
    package[0] ^= 1;
    set_manifest();
    assert(!fetch() && status.stopped && !requested);
    choose_build(2);
    interrupt = false;
    assert(fetch() && !requested && f_stat(PART, &info) == FR_NO_FILE);
    verify_cached();
    expect_build(1, false);
    for(unsigned build = 3; build <= 7; build++) {
        choose_build(build);
        assert(fetch() && !requested);
        verify_cached();
    }
    expect_build(2, false);
    for(unsigned build = 3; build <= 7; build++) expect_build(build, true);

    /* A failed replacement must leave all five complete downloads available. */
    choose_build(16);
    keep = 1;
    interrupt = true;
    assert(!fetch() && status.stopped);
    for(unsigned build = 3; build <= 7; build++) expect_build(build, true);
    keep = 5;
    interrupt = false;

    /* An unowned package at the exact destination is never removed or claimed.
     * A byte-identical manually copied nightly stays unowned as well. */
    choose_build(8);
    build_path(8, collision);
    write_bytes(collision, "keep", 4, FA_WRITE | FA_CREATE_NEW);
    assert(!fetch());
    expect_bytes(collision, "keep", 4, 4);
    choose_build(9);
    build_path(9, exact_copy);
    write_bytes(exact_copy, package, sizeof(package), FA_WRITE | FA_CREATE_NEW);
    if(fetch()) verify_cached();
    expect_bytes(exact_copy, package, 1, sizeof(package));

    keep = 0;
    for(unsigned build = 10; build <= 15; build++) {
        choose_build(build);
        assert(fetch());
    }
    for(unsigned build = 3; build <= 7; build++) expect_build(build, true);
    for(unsigned build = 10; build <= 15; build++) expect_build(build, true);

    /* Simulate a changed file among older owned downloads. The updater must
     * compare bytes before trimming it, even if its name and size still fit. */
    char changed[KUI_UPDATE_BROWSER_PATH_BYTES];
    build_path(5, changed);
    write_bytes(changed, "user", 4, FA_WRITE);
    assert(f_mount(NULL, "0:", 0) == FR_OK && f_mount(fs, "0:", 1) == FR_OK);
    unsigned before = transfers;
    keep = 3;
    assert(fetch() && transfers == before);
    verify_cached();
    expect_bytes(changed, "user", 4, sizeof(package));
    expect_build(3, false);
    expect_build(4, false);
    expect_build(6, false);
    expect_build(7, false);
    for(unsigned build = 10; build <= 15; build++) expect_build(build, build >= 13);
    expect_bytes(collision, "keep", 4, 4);
    expect_build(9, true);
    expect_bytes(manual, "user", 4, 4);
    expect_bytes(manual_zip, "test", 4, 4);

    char current[13];
    memcpy(current, commit, 12);
    current[12] = 0;
    running = current;
    keep = 1;
    assert(fetch() && !path[0] && transfers == before);
    expect_build(13, false);
    expect_build(14, false);
    expect_build(15, true);
    running = NULL;

    /* Changing Settings must be able to prune offline without checking the
     * feed, dialing the modem or pinning an arbitrary package. */
    keep = 0;
    choose_build(17);
    assert(fetch());
    choose_build(18);
    assert(fetch());
    before = opens;
    assert(kui_nightly_trim(1, NULL, cancel) && opens == before);
    expect_build(15, false);
    expect_build(17, false);
    expect_build(18, true);

    /* A feed can roll back. Returning an older cached package for review
     * must protect its path even when it falls outside the nominal limit. */
    keep = 0;
    choose_build(20);
    assert(fetch());
    choose_build(21);
    assert(fetch());
    before = transfers;
    choose_build(18);
    keep = 1;
    assert(fetch() && transfers == before);
    verify_cached();
    expect_build(20, false);
    expect_build(21, false);

    /* Power loss or a damaged receipt must leave the package unowned. */
    keep = 0;
    choose_build(22);
    assert(fetch());
    char receipt[KUI_UPDATE_BROWSER_PATH_BYTES];
    snprintf(receipt, sizeof(receipt), KUI_NIGHTLY_STATE "/%s.rec", commit);
    write_bytes(receipt, "torn", 4, FA_WRITE);
    choose_build(23);
    keep = 1;
    assert(fetch());
    expect_build(18, false);
    expect_build(22, true);
    expect_build(23, true);
    expect_bytes(collision, "keep", 4, 4);
    expect_build(9, true);
}

static void test_unowned_partial(void) {
    static const char marker[] = KUI_NIGHTLY_STATE "/resume.id";
    static const char moved[] = KUI_NIGHTLY_STATE "/user.part";
    choose_build(24);
    interrupt = true;
    assert(!fetch() && status.stopped && !requested);
    unsigned before = transfers;
    assert(f_rename(marker, KUI_NIGHTLY_STATE "/previous.id") == FR_OK);
    write_bytes(PART, "user", 4, FA_WRITE);
    choose_build(25);
    interrupt = false;
    assert(!fetch() && transfers == before);
    expect_bytes(PART, "user", 4, 140000);
    write_bytes(marker, "torn", 4, FA_WRITE | FA_CREATE_NEW);
    assert(!fetch() && transfers == before);
    expect_bytes(PART, "user", 4, 140000);

    assert(f_rename(PART, moved) == FR_OK);
    assert(fetch() && !requested && transfers == before + 1);
    verify_cached();
    expect_bytes(moved, "user", 4, 140000);

    /* Even a valid previous marker cannot authorize removing a directory. */
    assert(f_mkdir(PART) == FR_OK);
    choose_build(26);
    assert(!fetch() && transfers == before + 1);
    FILINFO info;
    assert(f_stat(PART, &info) == FR_OK && (info.fattrib & AM_DIR));
}
int main(int argc, char **argv) {
    assert(argc == 2 && (image = fopen(argv[1], "r+b")) && !fseek(image, 0, SEEK_END));
    size = (uint32_t)(ftell(image) / 512);
    FATFS fs;
    kui_media_set(&media);
    assert(f_mount(&fs, "0:", 1) == FR_OK);
    assert(kui_nightly_trim(KUI_UPDATE_KEEP_DEFAULT, NULL, cancel) && !opens);
    FILINFO initial;
    assert(f_stat("0:/KUI", &initial) == FR_NO_FILE);
    for(unsigned i = 0; i < sizeof(package); ++i) package[i] = (uint8_t)(i*17u);
    set_manifest();
    assert(kui_update_test_feed_available() == (KUI_UPDATE_REVIEW_FEED != 0));
    test_feed = true;
    if(!KUI_UPDATE_REVIEW_FEED) {
        assert(!fetch() && !opens);
        test_feed = false;
    }
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
    assert(f_stat(PART, &info) == FR_OK && info.fsize == 140000 && f_stat(CACHED COMMIT ".kup", &info) == FR_NO_FILE);
    interrupt = false;
    assert(fetch() && transfers == 2 && requested == 140000);
    verify_cached();
    assert(f_stat(PART, &info) == FR_NO_FILE);
    assert(fetch() && transfers == 2);
    verify_cached();
    /* Replacing bytes in place must neither produce a cache hit nor overwrite
     * a package the user may have put there. Moving it permits a retry. */
    write_bytes(CACHED COMMIT ".kup", "bad", 3, FA_WRITE);
    assert(!fetch() && transfers == 2);
    expect_bytes(CACHED COMMIT ".kup", "bad", 3, sizeof(package));
    assert(f_unlink(CACHED COMMIT ".kup") == FR_OK);
    interrupt = true;
    assert(!fetch() && transfers == 3 && status.stopped);
    interrupt = false;
    ignore_range = true;
    assert(fetch() && transfers == 5 && !requested);
    verify_cached();
    assert(f_unlink(path) == FR_OK);
    package[0] ^= 1;
    assert(!fetch() && transfers == 6 && strstr(status.message, "mismatch"));
    assert(f_stat(PART, &info) == FR_NO_FILE && f_stat(CACHED COMMIT ".kup", &info) == FR_NO_FILE);
    test_retention(&fs);
    test_late_damage();
    if(KUI_UPDATE_REVIEW_FEED) {
        test_feed = false;
        choose_build(27);
        assert(fetch());
        verify_cached();
    }
    test_unowned_partial();
    assert(f_mount(NULL, "0:", 0) == FR_OK && !fclose(image));
    puts("PASS nightly manifest, interruption, resume, ignored Range, cache readback, collision preservation, digest refusal, download retention");
}
