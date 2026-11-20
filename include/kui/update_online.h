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
enum kui_update_scope { KUI_UPDATE_SCOPE_UNKNOWN, KUI_UPDATE_SCOPE_FULL, KUI_UPDATE_SCOPE_PARTIAL };
enum kui_update_components {
    KUI_UPDATE_COMPONENT_GUI=1, KUI_UPDATE_COMPONENT_READERS=2, KUI_UPDATE_COMPONENT_OTHER=4
};
struct kui_update_choice {
    char commit[41], digest[65], published[21];
    char tag[35], asset[64];
    uint32_t size;
    enum kui_update_scope scope;
    uint16_t replace, remove, keep;
    uint8_t components;
    bool described;
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
