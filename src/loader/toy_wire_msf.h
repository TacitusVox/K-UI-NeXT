/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_TOY_WIRE_MSF_H
#define KUI_TOY_WIRE_MSF_H
#include <stdint.h>

/* The address a raw sector carries in its bytes 12 to 14: minutes, seconds
 * and frames of its frame address, two decimal digits to a byte, 75 frames
 * to a second, 60 seconds to a minute. A GD-ROM runs past 99 minutes and
 * goes on counting in the upper digit (122 is 0xc2), which is how
 * src/core/hash.c takes it too. Given one sector's three bytes, packed with
 * the minutes lowest as they lie in memory, this is the next sector's:
 * Test 74 checks a sector it takes without its EDC against the address
 * that follows the one before it, and needs no division for that. */
static inline uint32_t kui_toy_msf_next(uint32_t msf) {
#define KUI_TOY_BCD_NEXT(x) ((x) + (((x) & 15u) == 9u ? 7u : 1u))
    uint32_t minutes = msf & 255u, seconds = msf >> 8 & 255u, frames = KUI_TOY_BCD_NEXT(msf >> 16 & 255u);
    if(frames == 0x75u) {
        frames = 0u;
        seconds = KUI_TOY_BCD_NEXT(seconds);
        if(seconds == 0x60u) {
            seconds = 0u;
            minutes = KUI_TOY_BCD_NEXT(minutes);
        }
    }
#undef KUI_TOY_BCD_NEXT
    return minutes | seconds << 8 | frames << 16;
}
#endif
