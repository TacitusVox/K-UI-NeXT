/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/internet_extra.h"
#include "kui/internet_cover.h"
#include <stdio.h>
#include <string.h>

struct extra {
    uint32_t release;
    unsigned pages;
    const char *manual, *pdf, *movie;
};

static const struct extra media[] = {
#include "internet_extra_data.inc"
};

static const struct extra *find_extra(const struct kui_games_disc_identity *id) {
    const char *name = id ? kui_internet_cover_name(id) : NULL;
    if(!name) return NULL;
    uint32_t hash = UINT32_C(2166136261);
    for(const unsigned char *p = (const unsigned char *)name; *p; ++p)
        hash = (hash ^ *p) * UINT32_C(16777619);
    size_t low = 0, high = sizeof(media)/sizeof(*media);
    while(low < high) {
        size_t mid = low + (high-low)/2;
        if(media[mid].release == hash) return media + mid;
        if(media[mid].release < hash) low = mid+1;
        else high = mid;
    }
    return NULL;
}

unsigned kui_internet_manual_pages(const struct kui_games_disc_identity *id) {
    const struct extra *entry = find_extra(id);
    return entry ? entry->pages : 0;
}

bool kui_internet_manual_page_url(const struct kui_games_disc_identity *id, unsigned page,
                                  char *url, size_t cap) {
    const struct extra *entry = find_extra(id);
    if(!url || !cap || !entry || page >= entry->pages) return false;
    int n = snprintf(url, cap, "https://archive.org/download/%s/page/n%u_w512.jpg", entry->manual, page);
    return n >= 0 && (size_t)n < cap;
}

static bool video_url(const char *name, char *url, size_t cap) {
    static const char prefix[] = "https://archive.org/download/SegaDreamcastVideoSnaps/";
    static const char hex[] = "0123456789ABCDEF";
    size_t pos = sizeof(prefix)-1;
    if(cap <= pos) return false;
    memcpy(url, prefix, pos);
    for(const unsigned char *p = (const unsigned char *)name; *p; ++p) {
        unsigned ch = *p;
        if((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') || strchr("-._~", (int)ch)) {
            if(cap-pos <= 1) return false;
            url[pos++] = (char)ch;
        } else {
            if(cap-pos <= 3) return false;
            url[pos++] = '%'; url[pos++] = hex[ch>>4]; url[pos++] = hex[ch&15];
        }
    }
    if(cap-pos < sizeof(".ogv")) return false;
    memcpy(url+pos, ".ogv", sizeof(".ogv"));
    return true;
}

bool kui_internet_extra_url(const struct kui_games_disc_identity *id, enum kui_media_kind kind,
                            char *url, size_t cap) {
    const struct extra *entry = find_extra(id);
    if(!url || !cap || !entry) return false;
    if(kind == KUI_MEDIA_PREVIEW && entry->movie)
        return video_url(entry->movie, url, cap);
    if(kind == KUI_MEDIA_MANUAL && entry->pdf) {
        int n = snprintf(url, cap, "https://www.digitpress.com/library/manuals/dreamcast/%s", entry->pdf);
        return n >= 0 && (size_t)n < cap;
    }
    return false;
}
