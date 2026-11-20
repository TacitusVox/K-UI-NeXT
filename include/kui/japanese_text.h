/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_JAPANESE_TEXT_H
#define KUI_JAPANESE_TEXT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Fixed-width Dreamcast display fields are Shift-JIS / CP932, often padded
 * with spaces. Stop at NUL, strip trailing ASCII spaces, and terminate the
 * result when capacity permits. A malformed character is shown as '?'. */
size_t kui_sjis_to_utf8(char *out, size_t capacity, const uint8_t *field, size_t bytes);

/* Read one scalar from a NUL-terminated string. A malformed byte consumes
 * one byte and produces U+FFFD; a literal UTF-8 U+FFFD consumes three. */
static inline size_t kui_utf8_next(const char *text, uint32_t *scalar) {
    const unsigned char *p = (const unsigned char *)text;
    uint32_t value = p[0];
    if(value < 0x80) {
        *scalar = value;
        return value != 0;
    }

    unsigned length;
    if(value >= 0xc2 && value <= 0xdf) {
        length = 2;
        value &= 0x1f;
    } else if(value >= 0xe0 && value <= 0xef) {
        length = 3;
        value &= 0x0f;
    } else if(value >= 0xf0 && value <= 0xf4) {
        length = 4;
        value &= 0x07;
    } else {
        *scalar = 0xfffd;
        return 1;
    }

    for(unsigned i = 1; i < length; ++i) {
        if((p[i] & 0xc0) != 0x80) {
            *scalar = 0xfffd;
            return 1;
        }
        value = (value << 6) | (p[i] & 0x3f);
    }
    if((length == 3 && value < 0x800) || (length == 4 && value < 0x10000) ||
       (value >= 0xd800 && value <= 0xdfff) || value > 0x10ffff) {
        *scalar = 0xfffd;
        return 1;
    }
    *scalar = value;
    return length;
}

/* Return the largest valid UTF-8 prefix within a byte limit, without
 * splitting a character. The input is a NUL-terminated string. */
static inline size_t kui_utf8_prefix(const char *text, size_t max_bytes) {
    size_t used = 0;
    while(used < max_bytes && text[used]) {
        uint32_t scalar;
        size_t length = kui_utf8_next(text + used, &scalar);
        if((length == 1 && scalar == 0xfffd) || length > max_bytes - used) break;
        used += length;
    }
    return used;
}

/* Printable display text only. Filesystem and identity fields have separate
 * rules. Callers validating fixed buffers must check termination first. */
static inline bool kui_utf8_display_valid(const char *text) {
    while(*text) {
        uint32_t scalar;
        size_t length = kui_utf8_next(text, &scalar);
        if((length == 1 && scalar == 0xfffd) || scalar < 0x20 ||
           (scalar >= 0x7f && scalar <= 0x9f)) return false;
        text += length;
    }
    return true;
}
#endif
