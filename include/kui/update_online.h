/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_UPDATE_ONLINE_H
#define KUI_UPDATE_ONLINE_H
#include <stdbool.h>
#include <stdint.h>

#ifndef KUI_UPDATE_SOURCE_VERIFIED
#define KUI_UPDATE_SOURCE_VERIFIED 0
#endif

enum kui_update_channel { KUI_UPDATE_RELEASES, KUI_UPDATE_NIGHTLIES, KUI_UPDATE_HARDWARE };
#define KUI_UPDATE_NIGHTLY_LIMIT 10u
struct kui_update_choice {
    char commit[41], digest[65], published[21];
    uint32_t size;
};
struct kui_update_choices {
    unsigned count;
    struct kui_update_choice items[KUI_UPDATE_NIGHTLY_LIMIT];
};
struct kui_update_online_info {
    bool newer;
    char source_build[13];
    unsigned note_count;
    char notes[4][81];
};
#endif
