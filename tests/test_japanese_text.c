/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/japanese_text.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void field(const uint8_t *bytes, size_t count, size_t cap, const char *expected) {
    char result[64];
    memset(result, 'x', sizeof(result));
    size_t len = kui_sjis_to_utf8(result, cap, bytes, count);
    assert(len == strlen(expected));
    if(cap) {
        assert(!strcmp(result, expected));
        assert(kui_utf8_display_valid(result));
    }
    assert(result[cap] == 'x');
}

int main(void) {
    const uint8_t save[] = {0x93,0xfa,0x96,0x7b,0xb6,0x41,0x20,0x20,0,0xff};
    field(save, sizeof(save), 63, "日本ｶA");
    field(save, sizeof(save), 7, "日本");
    field(save, sizeof(save), 6, "日");
    field(save, sizeof(save), 3, "");
    field(save, sizeof(save), 0, "");
    field(save, 1, 2, "?");
    field(NULL, 128, 10, "");
    /* CP932 vendor alias ED40 and FA5C encode the same character. */
    const uint8_t vendors[] = {0xed,0x40,0xfa,0x5c,0x81,0x60,0xf0,0x40};
    field(vendors, sizeof(vendors), 20, "纊纊～\xee\x80\x80");
    const uint8_t invalid[] = {0x85,'A',0x81,0x1b,'!',0x81};
    field(invalid, sizeof(invalid), 20, "?A?" "?!?");

    uint32_t ch;
    assert(kui_utf8_next("", &ch) == 0 && ch == 0);
    assert(kui_utf8_next("A", &ch) == 1 && ch == 'A');
    assert(kui_utf8_next("¢", &ch) == 2 && ch == 0xa2);
    assert(kui_utf8_next("日", &ch) == 3 && ch == 0x65e5);
    assert(kui_utf8_next("𠮷", &ch) == 4 && ch == 0x20bb7);
    assert(kui_utf8_next("\xef\xbf\xbd", &ch) == 3 && ch == 0xfffd);
    const char *bad[] = {"\x80","\xc0\xaf","\xe0\x9f\x80","\xed\xa0\x80","\xf0\x80\x80\x80",
                         "\xf4\x90\x80\x80","\xf5\x80\x80\x80","\xf0\x90","\xe3\x81","\xc2"};
    for(size_t i = 0; i < sizeof(bad)/sizeof(*bad); ++i) {
        assert(kui_utf8_next(bad[i], &ch) == 1 && ch == 0xfffd);
        assert(kui_utf8_prefix(bad[i], 8) == 0 && !kui_utf8_display_valid(bad[i]));
    }
    assert(kui_utf8_prefix("A日ｶ",5) == 4);
    assert(kui_utf8_prefix("A日ｶ",7) == 7);
    assert(kui_utf8_prefix("A日ｶ",2) == 1);
    assert(kui_utf8_prefix("A日ｶ",0) == 0);
    assert(kui_utf8_display_valid("日ｶ𠮷"));
    assert(!kui_utf8_display_valid("A\x1f") && !kui_utf8_display_valid("\xc2\x85"));
    puts("Japanese fixed-width fields, CP932 aliases, UTF-8 validation and truncation passed.");
}
