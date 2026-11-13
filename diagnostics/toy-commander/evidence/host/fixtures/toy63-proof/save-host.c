#include <stdio.h>
#include <assert.h>
#include <string.h>

#define main protocol_test_main
#include "tests/test_sci_sd_storage.c"
#undef main
#include TRACE_SOURCE

enum scenario { PASS, BAD_CID, BAD_PLACEHOLDER, BAD_READBACK, BAD_INIT, BAD_RELEASE };
static enum scenario scenario;
static struct mock wire;
static uint8_t sectors[2][512];
static bool claimed;
static unsigned reads, releases, inits, rejected_block_calls, commands13;

static unsigned sector_index(uint32_t lba) {
    assert(lba == 19u || lba == 29u);
    return lba == 29u;
}

static bool reject_tx(void *ctx, const uint8_t *tx, uint8_t *rx,
                      size_t bytes, bool slow, uint16_t *crc) {
    assert(ctx == &wire && tx && !rx && bytes == 512u && !slow && crc);
    ++rejected_block_calls;
    return false;
}

static uint8_t persist_bytes(void *ctx, uint8_t value, bool slow) {
    unsigned before = wire.blocks;
    unsigned commands = wire.packets;
    uint8_t result = transfer(ctx, value, slow);
    if (wire.blocks != before) {
        assert(claimed && reads == 2u);
        memcpy(sectors[sector_index(wire.address)], wire.data, sizeof(sectors[0]));
    }
    if (wire.packets != commands && (wire.frame[0] & 63u) == 13u)
        ++commands13;
    return result;
}

static const struct kui_loader_sd_bus bus = {
    .ctx = &wire, .select = select_card, .transfer = persist_bytes, .ticks = ticks,
    .transfer_block = reject_tx,
};

enum kui_loader_sd_result kui_sci_sd_acquire(void) {
    assert(!claimed);
    claimed = true;
    return KUI_LOADER_SD_OK;
}

void kui_sci_sd_release(void) {
    assert(claimed);
    claimed = false;
    ++releases;
}

const struct kui_loader_sd_bus *kui_sci_sd_bus(void) { return &bus; }

bool kui_sci_sd_healthy(void) { return claimed; }

bool kui_sci_sd_faulted(void) { return scenario == BAD_RELEASE; }

enum kui_loader_sd_result kui_loader_sd_init_bus(struct kui_loader_sd *card,
                                                 const struct kui_loader_sd_bus *supplied_bus) {
    assert(claimed && supplied_bus == &bus);
    ++inits;
    *card = (struct kui_loader_sd){
        .bus = *supplied_bus, .ready = true, .high_capacity = true, .blocks = 2048u,
    };
    return scenario == BAD_INIT ? KUI_LOADER_SD_TIMEOUT : KUI_LOADER_SD_OK;
}

int kui_vmu_sd_identity_read(struct kui_loader_sd *card, struct kui_vmu_sd_identity *identity) {
    assert(claimed && card->ready && card->bus.transfer_block == reject_tx);
    *identity = (struct kui_vmu_sd_identity){.sectors = card->blocks, .high_capacity = true};
    memcpy(identity->cid, kui_disc_toy_config.report.cid, sizeof(identity->cid));
    if (scenario == BAD_CID)
        identity->cid[0] ^= 1u;
    return KUI_VMU_BACKING_OK;
}

enum kui_loader_sd_result kui_loader_sd_read(struct kui_loader_sd *card,
                                             uint32_t lba, uint32_t count, void *out) {
    assert(claimed && card->ready && count == 1u && card->bus.transfer_block == reject_tx);
    ++reads;
    memcpy(out, sectors[sector_index(lba)], sizeof(sectors[0]));
    if (scenario == BAD_READBACK && reads == 3u)
        ((uint8_t *)out)[0] ^= 1u;
    return KUI_LOADER_SD_OK;
}

static void reset_scenario(enum scenario next) {
    scenario = next;
    wire = (struct mock){0};
    reads = releases = inits = rejected_block_calls = commands13 = 0;
    claimed = false;
    memset(&kui_disc_toy_config, 0, sizeof(kui_disc_toy_config));
    struct kui_toy_report_wire *seed = &kui_disc_toy_config.report;
    seed->test=63u;
    seed->lba[0] = 19u;
    seed->lba[1] = 29u;
    seed->high_capacity = 1u;
    memset(seed->cid, 0x42, sizeof(seed->cid));
    memset(sectors[0], 'A', sizeof(sectors[0]));
    memset(sectors[1], 'B', sizeof(sectors[1]));
    seed->placeholder_crc[0] = kui_stage_crc32(0, sectors[0], sizeof(sectors[0]));
    seed->placeholder_crc[1] = kui_stage_crc32(0, sectors[1], sizeof(sectors[1]));
    for (size_t i = 0; i < sizeof(report_text); ++i)
        report_text[i] = (char)((i * 37u + i / 512u) & 0x7f);
    if (scenario == BAD_PLACEHOLDER)
        sectors[1][0] ^= 1u;
}

static void expect_release(void) {
    assert(!claimed && releases == 1u && inits == 1u && !wire.selected);
}

#ifndef HISTORICAL
static unsigned screen_lines;

void retail_display_values(const char *line, const uint32_t *values, unsigned count) {
    assert(strlen(line) <= 49u && values && count && count <= 5u);
    screen_lines += 2u;
}

static void expect_fallback(void) {
    memset(&trace, 0xff, sizeof(trace));
    memset(&C, 0xff, sizeof(C));
    format_report();
    assert(!memcmp(report_text+1012, "\nEND REPORT\n", 12));
    char text[sizeof(report_text)+1];
    memcpy(text, report_text, sizeof(report_text));
    text[sizeof(report_text)] = '\0';
    assert(strstr(text,"TEST 63\nBUILD AABBCCDDEEFF\nHEX / 781250 HZ\nPHYSICAL GD-ROM\n"));
    assert(strstr(text,"I ENTER SCENE EXIT FRAME\nFFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF\n"));
    assert(strstr(text,"I LAG LIMIT\nFFFFFFFF FFFFFFFF\n"));
    const char *at = text + 1012;
    while (at > text && at[-1] == ' ') --at;
    assert(at > text && at[-1] == '\n');
    printf("Physical Test 63 format: %td body bytes; 15 and 14 total fallback rows\n",at-text);
    screen_lines=0;
    for(unsigned i=0;i<2u;i++)movie_rows(NULL,i);
    assert(screen_lines==12u);
    screen_lines=0;
    for(unsigned i=0;i<2u;i++)transport_rows(NULL,i);
    assert(screen_lines==8u);
}
#endif

int main(void) {
    reset_scenario(PASS);
#ifdef HISTORICAL
    assert(!save_report());
    expect_release();
    assert(reads == 2u && rejected_block_calls == 1u && wire.blocks == 0u && commands13 == 0u);
    puts("Original Report 30 reproduced: CMD24 reached the disabled block TX path before a sector write");
#else
    assert(save_report() == NULL);
    expect_release();
    assert(reads == 4u && !rejected_block_calls && wire.blocks == 2u && commands13 == 3u);
    assert(memcmp(sectors, report_text, sizeof(sectors)) == 0);

    reset_scenario(BAD_CID);
    assert(save_report() != NULL);
    expect_release();
    assert(reads == 0u && !rejected_block_calls && wire.blocks == 0u && !wire.packets);

    reset_scenario(BAD_PLACEHOLDER);
    assert(save_report() != NULL);
    expect_release();
    assert(reads == 2u && !rejected_block_calls && wire.blocks == 0u && !wire.packets);

    reset_scenario(BAD_READBACK);
    assert(save_report() != NULL);
    expect_release();
    assert(reads == 3u && !rejected_block_calls && wire.blocks == 2u && commands13 == 3u);

    reset_scenario(BAD_INIT);
    assert(save_report() != NULL);
    expect_release();
    assert(reads == 0u && !rejected_block_calls && wire.blocks == 0u && !wire.packets);

    reset_scenario(BAD_RELEASE);
    assert(save_report() != NULL);
    expect_release();
    assert(reads == 4u && !rejected_block_calls && wire.blocks == 2u && commands13 == 3u);

    reset_scenario(PASS);
    kui_disc_toy_config.report.test=0u;
    assert(!strcmp(save_report(),"PHOTO ONLY / NO SD REPORT RESERVED"));
    assert(!reads && !inits && !releases && !wire.packets);
    expect_fallback();
    puts("Report 63: two CRC-checked byte-PIO writes, sync and readback passed. Wrong CID and placeholder blocked writes. Readback, init and release failures surfaced.");
#endif
    return 0;
}
