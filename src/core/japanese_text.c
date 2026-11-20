/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/japanese_text.h"
#include "japanese_text_data.inc"

static bool is_lead(unsigned c) {
    return (c >= 0x81 && c <= 0x9f) || (c >= 0xe0 && c <= 0xfc);
}

static uint16_t pair_value(unsigned lead, unsigned trail) {
    /* Each row has 188 possible trailing bytes; 0x7f is reserved. */
    unsigned column = trail - (trail > 0x7f ? 0x41 : 0x40);
    if(lead >= 0xf0 && lead <= 0xf9)
        return (uint16_t)(0xe000 + (lead - 0xf0) * 188 + column);
    if(!sjis_rows[lead]) return 0;
    return sjis_values[(sjis_rows[lead] - 1u) * 188 + column];
}

size_t kui_sjis_to_utf8(char *out, size_t capacity, const uint8_t *field, size_t bytes) {
    if(!out || !capacity) return 0;
    out[0] = '\0';
    if(!field) return 0;
    size_t end = 0;
    while(end < bytes && field[end]) ++end;
    while(end && field[end - 1] == ' ') --end;

    size_t used = 0;
    for(size_t i = 0; i < end;) {
        unsigned c = field[i++];
        uint16_t value = '?';
        if(c >= 0x20 && c <= 0x7e) {
            value = (uint16_t)c;
        } else if(c >= 0xa1 && c <= 0xdf) {
            value = (uint16_t)(0xff61 + c - 0xa1);
        } else if(is_lead(c) && i < end) {
            unsigned trail = field[i];
            if(trail >= 0x40 && trail <= 0xfc && trail != 0x7f) {
                uint16_t mapped = pair_value(c, trail);
                if(mapped) {
                    ++i;
                    value = mapped;
                }
            }
        }
        unsigned length = value < 0x80 ? 1 : value < 0x800 ? 2 : 3;
        if(length >= capacity - used) break;
        if(length == 3) {
            out[used++] = (char)(0xe0 | (value >> 12));
            out[used++] = (char)(0x80 | ((value >> 6) & 0x3f));
        } else if(length == 2) {
            out[used++] = (char)(0xc0 | (value >> 6));
        }
        out[used++] = (char)(length == 1 ? value : (0x80 | (value & 0x3f)));
    }
    out[used] = '\0';
    return used;
}
