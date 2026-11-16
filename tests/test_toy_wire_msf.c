/* SPDX-License-Identifier: GPL-3.0-only
 * Test 74's sector address successor against the arithmetic it stands for
 * (src/core/hash.c, mode1_fad_valid): every frame address a disc can have. */
#include "toy_wire_msf.h"
#include <assert.h>
#include <stdio.h>

static uint32_t msf_of(uint32_t fad) {
    const unsigned fields[3] = {fad / 4500u, (fad / 75u) % 60u, fad % 75u};
    uint32_t packed = 0;
    for(unsigned i = 0; i < 3u; ++i) packed |= (uint32_t)(uint8_t)((fields[i] / 10u) * 16u + fields[i] % 10u) << 8u * i;
    return packed;
}

int main(void) {
    unsigned seconds = 0, minutes = 0, hundred = 0;
    /* hash.c accepts 150 to 719999: 160 minutes, the minute byte up to 0xf9. */
    for(uint32_t fad = 150u; fad < 719999u; ++fad) {
        const uint32_t here = msf_of(fad), next = msf_of(fad + 1u);
        assert(kui_toy_msf_next(here) == next && next < UINT32_C(0x1000000));
        if(!(next >> 16)) ++seconds;
        if(!(next >> 8)) ++minutes;
        if((here & 255u) == 0x99u && (next & 255u) == 0xa0u) ++hundred;
    }
    /* Frames run out 9,597 times in that range, 159 of them at a minute's end. */
    assert(seconds == 9597u && minutes == 159u && hundred == 1u);
    printf("PASS sector address successor: 719849 addresses, %u second carries, %u minute carries, "
           "one past 99 minutes\n", seconds, minutes);
    return 0;
}
