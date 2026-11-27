/* SPDX-License-Identifier: GPL-3.0-only */
/* The Maple discovery's witness, searched in a program K-UI cannot hold.
 *
 * The combined Background module's stage runs the SDK check on the loaded
 * program and stops where it fails. The launcher runs the same check before
 * the launch, but only on a program it holds whole, and the console leaves
 * K-UI under 8 MiB: a larger program got no answer and was started all the
 * same (Plasma Sword, 9.07 MiB, console 27 November 2026).
 *
 * The check's first step, kui_vmu_native_discover, needs one site in the
 * program at which 32 instruction words match: sdk_at's table in
 * src/core/vmu_native_discovery.c. The words lie from 258 bytes before the
 * site to 368 after it, so they can be searched for as the program is read
 * in pieces. No such site anywhere in the file: that step fails for every
 * program extent, and the stage stops. A site: no answer here, because the
 * rest of the check needs the whole program.
 *
 * The table and the four bounds below are the discovery's own, value for
 * value. tests/test_games_native_witness.py holds them to its source. */
struct native_witness_word {int16_t offset;uint16_t value;};
static const struct native_witness_word native_witness_words[]={
    {0,0x9028},{2,0x63e2},{4,0x023e},{6,0x12c6},{8,0x9029},{10,0x63e2},
    {84,0x10f4},{94,0x0fd8},{-192,0x401b},{-258,0x60e2},{-256,0x001e},
    {-254,0x8801},{-252,0x8d02},{-248,0xa0d3},{-246,0xe0fd},
    {266,0x0316},{272,0x2320},{22,0x22ca},{24,0x0326},{58,0x22ca},
    {60,0x0326},{170,0x03d6},{176,0x23d0},{198,0x024e},{200,0x2228},
    {214,0x5036},{216,0xc801},{230,0x401b},{232,0x8906},{248,0xe000},
    {328,0x420b},{366,0x430b}
};
/* The first site the discovery tests, the bytes it needs after its last
 * one, and the reach of the table before and after a site. */
#define NATIVE_WITNESS_FIRST 0x53cu
#define NATIVE_WITNESS_TAIL 0x1b8u
#define NATIVE_WITNESS_BEFORE 258u
#define NATIVE_WITNESS_AFTER 368u
static bool native_witness_at(const uint8_t *site) {
    for(unsigned i=0u;i<sizeof(native_witness_words)/sizeof(native_witness_words[0]);i++) {
        const uint8_t *word=site+native_witness_words[i].offset;
        if((uint16_t)(word[0]|(uint16_t)word[1]<<8)!=native_witness_words[i].value)return false;
    }
    return true;
}
/* data holds the file's bytes [base, base+have). Tests the sites from *next
 * on whose whole reach lies in data, in the discovery's order and bounds
 * (even offsets from FIRST to file_bytes-TAIL), and leaves in *next the
 * first site not tested yet. The caller keeps at least BEFORE+AFTER bytes
 * before that site in data. True at the first witness, and also for a site
 * whose reach is no longer held: such a site cannot be ruled out. */
static bool native_witness_search(const uint8_t *data,uint32_t base,uint32_t have,
    uint32_t file_bytes,uint32_t *next) {
    if(file_bytes<NATIVE_WITNESS_FIRST+NATIVE_WITNESS_TAIL)return false;
    const uint32_t last=file_bytes-NATIVE_WITNESS_TAIL;
    uint32_t site=*next<NATIVE_WITNESS_FIRST?NATIVE_WITNESS_FIRST:*next;
    for(;site<=last && have>=NATIVE_WITNESS_AFTER && site>=base &&
          site-base<=have-NATIVE_WITNESS_AFTER;site+=2u) {
        if(site-base<NATIVE_WITNESS_BEFORE || native_witness_at(data+(site-base))) {*next=site;return true;}
    }
    *next=site;return site<base;
}
