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

struct small_cover {
    uint16_t release;
    const char *item, *stem; /* NULL uses the jewel-case collection or the release name. */
};
struct cover_tile {
    uint16_t release, bytes;
    uint32_t crc;
};
#include "internet_cover_small.inc"

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

/* These US discs have distinct printed and internal product numbers. The
 * measured boot title and market must agree before using the printed key.
 * A folder rename is never used as evidence. */
struct boot_product {
    const char *product, *title, *catalog_product;
};
static const struct boot_product us_boot_products[] = {
    {"t1711n",  "evolution2",               "t17711n"},
    {"t11010n", "sierrasportsmaximumpool", "t11002n"},
    {"t12509n", "superrunabout",           "t12511n"}
};

static const char *shenmue_us_box(const struct kui_games_disc_identity *id) {
    /* The three game discs and Passport share the IP title and product.
     * The disc ordinal and revision identify the two different covers. */
    if(id->total != 4) return NULL;
    if(id->number >= 1 && id->number <= 3 && !memcmp(id->version, "V1.003", 7))
        return "Shenmue (USA)";
    if(id->number == 4 && !memcmp(id->version, "V1.004", 7))
        return "Shenmue (USA) (Shenmue Passport)";
    return NULL;
}

static const struct cover_release *find_release(const struct kui_games_disc_identity *id) {
    if(!id || !memchr(id->product, 0, sizeof(id->product)) ||
       !memchr(id->title, 0, sizeof(id->title)) ||
       !memchr(id->region, 0, sizeof(id->region))) return NULL;

    unsigned markets = market_mask(id->region);
    char key[sizeof(id->title)], serial[sizeof(id->product)];
    if(!normalize(id->title, strlen(id->title), key, sizeof(key))) key[0] = 0;
    if(normalize(id->product, strlen(id->product), serial, sizeof(serial))) {
        /* The US retail IP title is "PSO". Its serial is also listed for
         * the Sonic Adventure 2 trial. The bare serial is a US alias. */
        bool bare_pso_serial = !strcmp(serial, "51100");
        bool pso = !strcmp(key, "pso") && (bare_pso_serial || !strcmp(serial, "mk51100"));
        if(pso && bare_pso_serial && markets != 2u) return NULL;
        const char *serial_title = pso ? "phantasystaronline" : key;
        const char *catalog_product = serial;
        for(size_t i = 0; i < COUNT(us_boot_products); ++i) {
            const struct boot_product *alias = &us_boot_products[i];
            if(strcmp(alias->product, serial)) continue;
            if(markets != 2u || strcmp(alias->title, key)) return NULL;
            catalog_product = alias->catalog_product;
            break;
        }
        const char *box = NULL;
        if(markets == 2u && !strcmp(serial, "mk51059") && !strcmp(key, "shenmue")) {
            box = shenmue_us_box(id);
            if(!box) return NULL;
        }
        const struct cover_release *match = NULL, *named = NULL;
        unsigned matches = 0, named_matches = 0;
        for(size_t i = first_serial(catalog_product);
            i < COUNT(cover_serials) && !strcmp(cover_serials[i].key, catalog_product); ++i) {
            const struct cover_release *item = &cover_releases[cover_serials[i].release];
            if(markets && !(item->markets & markets)) continue;
            match = item;
            ++matches;
            if(box ? !strcmp(item->name, box) : title_matches(item, serial_title)) {
                named = item;
                ++named_matches;
            }
        }
        if(box || pso || matches > 1) return named_matches == 1 ? named : NULL;
        if(matches == 1) return match;
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
    return match;
}

const char *kui_internet_cover_name(const struct kui_games_disc_identity *id) {
    const struct cover_release *match = find_release(id);
    return match ? match->name : NULL;
}

static bool image_url(char *url, size_t cap, const char *prefix, const char *folder,
        const char *name, const char *suffix) {
    static const char hex[] = "0123456789ABCDEF";
    if(!url || !cap) return false;
    url[0] = 0;
    size_t prefix_size = strlen(prefix), folder_size = strlen(folder), end_size = strlen(suffix) + 1;
    if(!name || prefix_size + folder_size + 1 + end_size > cap) return false;
    memcpy(url, prefix, prefix_size);
    memcpy(url + prefix_size, folder, folder_size);
    size_t used = prefix_size + folder_size;
    url[used++] = '/';
    for(; *name; ++name) {
        unsigned char c = (unsigned char)*name;
        bool plain = ascii_key((char)c) || strchr("-._~", c);
        size_t len = plain ? 1 : 3;
        if(cap - used < len + end_size) {
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
    memcpy(url + used, suffix, end_size);
    return true;
}

bool kui_internet_cover_url(const struct kui_games_disc_identity *id, char *url, size_t cap) {
    return image_url(url, cap,
        "https://raw.githubusercontent.com/libretro-thumbnails/Sega_-_Dreamcast/",
        KUI_BOXART_REV "/Named_Boxarts", kui_internet_cover_name(id), ".png");
}

static const struct small_cover *find_small(const struct cover_release *match) {
    if(!match) return NULL;
    unsigned release = (unsigned)(match - cover_releases);
    size_t lo = 0, hi = COUNT(small_covers);
    while(lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if(small_covers[mid].release < release) lo = mid + 1;
        else hi = mid;
    }
    if(lo == COUNT(small_covers) || small_covers[lo].release != release) return NULL;
    return &small_covers[lo];
}

bool kui_internet_cover_small_url(const struct kui_games_disc_identity *id, char *url, size_t cap) {
    if(!url || !cap) return false;
    url[0] = 0;
    const struct cover_release *match = find_release(id);
    const struct small_cover *cover = find_small(match);
    if(!cover) return false;
    return image_url(url, cap, "https://archive.org/download/",
        cover->item ? cover->item : KUI_JEWEL_CASE_ITEM,
        cover->stem ? cover->stem : match->name, "_thumb.jpg");
}

static const struct cover_tile *find_tile(const struct small_cover *cover) {
    if(cover) for(size_t i=0;i<COUNT(cover_tiles);++i)
        if(cover_tiles[i].release==cover->release) return &cover_tiles[i];
    return NULL;
}
bool kui_internet_cover_small_pin(const struct kui_games_disc_identity *id,uint32_t *bytes,uint32_t *crc) {
    const struct cover_tile *tile=find_tile(find_small(find_release(id)));
    if(!tile) return false;
    *bytes=tile->bytes;
    *crc=tile->crc;
    return true;
}
bool kui_internet_cover_small_matches(const struct kui_games_disc_identity *id,uint32_t bytes,uint32_t crc) {
    const struct small_cover *cover=find_small(find_release(id));
    if(!cover || !bytes || bytes>128u*1024u) return false;
    const struct cover_tile *tile=find_tile(cover);
    return !tile || (tile->bytes==bytes && tile->crc==crc);
}
