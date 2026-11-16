/* SPDX-License-Identifier: GPL-3.0-only
 * Card Tools' own admission of a physical Toy trace module, run on the actual
 * packaged bytes: the launch-template layout for the Toy executable, exactly
 * one blank report seed and one blank movie-range seed where the installed
 * GUI searches for them, and the encode/validate round trip after binding.
 * The seed searches repeat src/apps/toy_suite_report_prepare.inc and
 * trace_movie_prepare() in src/apps/disc_launcher.c of Card Tools
 * 09c9a4b686d4; the layout functions are that build's very source. */
#include "kui/disc_launch.h"
#include "kui/disc_launch_limits.h"
#include "kui/hash.h"
#include "kui/toy_suite_report_wire.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if(!(x)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #x); return 1; } } while(0)
#define TOY_EXEC_BYTES 748444u

static uint32_t le32(const uint8_t *p) {
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void digest(const uint8_t *data, size_t n, uint8_t out[32]) {
    struct kui_sha256 s;
    kui_sha256_init(&s); kui_sha256_update(&s, data, n); kui_sha256_digest(&s, out);
}

int main(int argc, char **argv) {
    CHECK(argc == 2);
    FILE *file = fopen(argv[1], "rb");
    CHECK(file);
    static uint8_t package[1u << 20];
    const size_t size = fread(package, 1u, sizeof(package), file);
    CHECK(size > 64u && size < sizeof(package) && fgetc(file) == EOF && !fclose(file));
    /* The runtime envelope tools/runtime_package.py writes and verifies. */
    const uint32_t bytes = le32(package + 16u);
    CHECK(le32(package + 8u) == 1u && le32(package + 12u) == 64u && bytes == size - 64u && !(bytes & 3u));
    CHECK(kui_crc32(0u, package + 64u, bytes) == le32(package + 32u));
    const uint8_t *payload = package + 64u;

    struct kui_disc_launch_plan first, plan, checked;
    CHECK(kui_disc_launch_layout(payload, bytes, 4096u, &first));
    CHECK(first.flags == KUI_DISC_LAUNCH_FLAGS); /* the no-card physical route */
    CHECK(kui_disc_launch_layout(payload, bytes, TOY_EXEC_BYTES, &plan));
    CHECK(plan.module_bytes == bytes && plan.exec_bytes == TOY_EXEC_BYTES && plan.payload_bytes > bytes);

    size_t report = SIZE_MAX, ranges = SIZE_MAX;
    unsigned reports = 0, range_seeds = 0;
    static const uint8_t range_seed[40] = "KUITFR63";
    for(size_t at = 0; at + KUI_TOY_REPORT_WIRE_BYTES <= bytes; at += 4u) {
        const uint8_t *p = payload + at;
        if(le32(p) != KUI_TOY_REPORT_MAGIC0 || le32(p + 4u) != KUI_TOY_REPORT_MAGIC1 ||
           le32(p + 8u) != KUI_TOY_REPORT_VERSION || le32(p + 12u) != KUI_TOY_REPORT_WIRE_BYTES) continue;
        unsigned nonblank = 0;
        for(unsigned i = KUI_TOY_REPORT_TEST_OFFSET; i < KUI_TOY_REPORT_WIRE_BYTES; ++i) nonblank |= p[i];
        CHECK(!nonblank);
        report = at; ++reports;
    }
    for(size_t at = 0; at + sizeof(range_seed) <= bytes; at += 4u)
        if(!memcmp(payload + at, range_seed, sizeof(range_seed))) {ranges = at; ++range_seeds;}
    /* The code that checks these seeds keeps their magic words in literal
     * pools, so the words alone do occur elsewhere. Card Tools refuses a
     * second complete match, never a lone magic: exactly one of each. */
    CHECK(reports == 1u && range_seeds == 1u);

    /* Stage and bind as the launcher does, with stand-in disc bytes. */
    uint8_t *image = calloc(1u, plan.payload_bytes);
    CHECK(image);
    memcpy(image, payload, bytes);
    uint8_t *ip = image + plan.ip_offset, *exec = image + plan.exec_offset;
    memset(ip, ' ', KUI_DISC_LAUNCH_IP_BYTES); memcpy(ip, "SEGA SEGAKATANA ", 16u);
    memcpy(ip + 37u, "GD-ROM", 6u); memcpy(ip + 56u, "0000000 ", 8u);
    for(uint32_t i = 0; i < plan.exec_bytes; ++i) exec[i] = (uint8_t)(i * 29u + 7u);
    plan.ip_crc32 = kui_crc32(0u, ip, KUI_DISC_LAUNCH_IP_BYTES);
    plan.exec_crc32 = kui_crc32(0u, exec, plan.exec_bytes);
    digest(ip, KUI_DISC_LAUNCH_IP_BYTES, plan.ip_sha256); digest(exec, plan.exec_bytes, plan.exec_sha256);
    static const uint32_t movie[4][2] = {{430256u, 3801u}, {430256u, 3801u}, {452810u, 26208u}, {452810u, 26208u}};
    memcpy(image + ranges + 8u, movie, sizeof(movie));
    struct kui_toy_report_wire seed = {.magic = {KUI_TOY_REPORT_MAGIC0, KUI_TOY_REPORT_MAGIC1},
        .version = KUI_TOY_REPORT_VERSION, .bytes = KUI_TOY_REPORT_WIRE_BYTES, .test = 63u,
        .lba = {1234567u, 1234568u}};
    seed.crc32 = kui_crc32(0u, &seed, KUI_TOY_REPORT_CRC_OFFSET);
    memcpy(image + report, &seed, sizeof(seed));
    CHECK(kui_disc_launch_encode(image + KUI_DISC_LAUNCH_CONFIG_OFFSET, &plan));
    CHECK(kui_disc_launch_validate(image, plan.payload_bytes, &checked) && !memcmp(&plan, &checked, sizeof(plan)));
    free(image);
    printf("PASS Toy trace package admission: build %.12s payload %u bytes CRC32 %08X; "
           "report seed at %zu, range seed at %zu, staged payload %u bytes\n",
           (const char *)package + 40u, (unsigned)bytes, (unsigned)le32(package + 32u), report, ranges,
           (unsigned)plan.payload_bytes);
    return 0;
}
