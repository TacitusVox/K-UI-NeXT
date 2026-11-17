/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/internet_cover.h"
#include "kui/games.h"

#include <stdint.h>
#include <string.h>

struct cover_release {
    const char *name;
    uint8_t markets, title_length, retail;
};
struct cover_serial {
    const char *key;
    uint16_t release;
};
#include "internet_cover_catalog.inc"

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

static char ascii_key(char c) {
    if(c >= 'A' && c <= 'Z') return (char)(c + ('a' - 'A'));
    if((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) return c;
    return 0;
}

static bool normalize(const char *s, size_t n, char *dest, size_t cap) {
    size_t used = 0;
    for(size_t i = 0; i < n; ++i) {
        if((unsigned char)s[i] >= 128) return false;
        char c = ascii_key(s[i]);
        if(c) {
            if(used + 1 >= cap) return false;
            dest[used++] = c;
        }
    }
    dest[used] = 0;
    return used != 0;
}

static bool title_matches(const struct cover_release *item, const char *key) {
    if(!key[0]) return false;
    const char *s = item->name;
    size_t n = item->title_length;
    char title[sizeof(((struct kui_games_disc_identity *)0)->title)];
    if(normalize(s, n, title, sizeof(title)) && !strcmp(key, title)) return true;
    /* Catalogs file leading articles as ", The", ", An" or ", A". */
    static const char *const endings[] = {", The", ", An", ", A"};
    for(size_t i = 0; i < COUNT(endings); ++i) {
        size_t end = strlen(endings[i]);
        if(end >= n || memcmp(s + n - end, endings[i], end)) continue;
        size_t article = end - 2;
        for(size_t j = 0; j < article; ++j) title[j] = ascii_key(endings[i][2+j]);
        return normalize(s, n - end, title + article, sizeof(title) - article)
               && !strcmp(key, title);
    }
    return false;
}

static unsigned market_mask(const char *region) {
    unsigned mask = 0;
    for(; *region; ++region) {
        switch(ascii_key(*region)) {
        case 'j': mask |= 1; break;
        case 'u': mask |= 2; break;
        case 'e': mask |= 4; break;
        default: break;
        }
    }
    return mask;
}

static size_t first_serial(const char *key) {
    size_t lo = 0, hi = COUNT(cover_serials);
    while(lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if(strcmp(cover_serials[mid].key, key) < 0) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

const char *kui_internet_cover_name(const struct kui_games_disc_identity *id) {
    if(!id || !memchr(id->product, 0, sizeof(id->product)) ||
       !memchr(id->title, 0, sizeof(id->title)) ||
       !memchr(id->region, 0, sizeof(id->region))) return NULL;

    unsigned markets = market_mask(id->region);
    char key[sizeof(id->title)], serial[sizeof(id->product)];
    if(!normalize(id->title, strlen(id->title), key, sizeof(key))) key[0] = 0;
    if(normalize(id->product, strlen(id->product), serial, sizeof(serial))) {
        const struct cover_release *match = NULL, *named = NULL;
        unsigned matches = 0, named_matches = 0;
        for(size_t i = first_serial(serial);
            i < COUNT(cover_serials) && !strcmp(cover_serials[i].key, serial); ++i) {
            const struct cover_release *item = &cover_releases[cover_serials[i].release];
            if(markets && !(item->markets & markets)) continue;
            match = item;
            ++matches;
            if(title_matches(item, key)) {
                named = item;
                ++named_matches;
            }
        }
        if(matches == 1) return match->name;
        if(matches > 1) return named_matches == 1 ? named->name : NULL;
    }
    if(!key[0]) return NULL;

    const struct cover_release *match = NULL;
    for(size_t i = 0; i < COUNT(cover_releases); ++i) {
        const struct cover_release *item = &cover_releases[i];
        if(!item->retail || (markets && !(item->markets & markets)) || !title_matches(item, key))
            continue;
        if(match) return NULL;
        match = item;
    }
    return match ? match->name : NULL;
}

bool kui_internet_cover_url(const struct kui_games_disc_identity *id, char *url, size_t cap) {
    static const char prefix[] =
        "https://raw.githubusercontent.com/libretro-thumbnails/Sega_-_Dreamcast/"
        KUI_BOXART_REV "/Named_Boxarts/";
    static const char hex[] = "0123456789ABCDEF";
    if(!url || !cap) return false;
    url[0] = 0;
    const char *name = kui_internet_cover_name(id);
    if(!name || cap < sizeof(prefix) + sizeof(".png") - 1) return false;
    size_t used = sizeof(prefix) - 1;
    memcpy(url, prefix, used);
    for(; *name; ++name) {
        unsigned char c = (unsigned char)*name;
        bool plain = ascii_key((char)c) || strchr("-._~", c);
        size_t len = plain ? 1 : 3;
        if(cap - used < len + sizeof(".png")) {
            url[0] = 0;
            return false;
        }
        if(plain) url[used++] = (char)c;
        else {
            url[used++] = '%';
            url[used++] = hex[c >> 4];
            url[used++] = hex[c & 15];
        }
    }
    memcpy(url + used, ".png", sizeof(".png"));
    return true;
}
