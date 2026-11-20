/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_NATIVE_MAP_CAPACITY_H
#define KUI_NATIVE_MAP_CAPACITY_H

/* Optional native Background header in previously unused fixed package
 * padding. Existing GUI builds ignore it; a blank field is the original
 * 64-slot reader. The disc map itself remains the version 3 wire format. */
#define KUI_NATIVE_MAP_CAP_OFFSET 0x160
#define KUI_NATIVE_MAP_CAP_BYTES 16
#define KUI_NATIVE_MAP_CAP_MAGIC "KUINMP01"
#define KUI_NATIVE_MAP_CAP_VERSION 1
#define KUI_NATIVE_MAP_CAP_SLOTS 160
#define KUI_NATIVE_MAP_LEGACY_SLOTS 64

#ifndef __ASSEMBLER__
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static inline unsigned kui_native_map_capacity(const void *module,size_t bytes) {
    static const uint8_t empty[KUI_NATIVE_MAP_CAP_BYTES]={0};
    if(!module || bytes<KUI_NATIVE_MAP_CAP_OFFSET+KUI_NATIVE_MAP_CAP_BYTES)return 0;
    const uint8_t *record=(const uint8_t *)module+KUI_NATIVE_MAP_CAP_OFFSET;
    if(!memcmp(record,empty,sizeof(empty)))return KUI_NATIVE_MAP_LEGACY_SLOTS;
    static const uint8_t fields[8]={KUI_NATIVE_MAP_CAP_VERSION,0,0,0,KUI_NATIVE_MAP_CAP_SLOTS,0,0,0};
    return !memcmp(record,KUI_NATIVE_MAP_CAP_MAGIC,8u) && !memcmp(record+8u,fields,sizeof(fields))?
        KUI_NATIVE_MAP_CAP_SLOTS:0;
}
#endif
#endif
