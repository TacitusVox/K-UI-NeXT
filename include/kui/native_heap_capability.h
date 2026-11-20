/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_NATIVE_HEAP_CAPABILITY_H
#define KUI_NATIVE_HEAP_CAPABILITY_H

/* Fixed entry record. A missing or unfamiliar record keeps the original
 * heap-call ABI. The cold stage validates each call against the live image. */
#define KUI_NATIVE_HEAP_CAP_OFFSET 0x170
#define KUI_NATIVE_HEAP_CAP_MAGIC "KUIHAP01"
#define KUI_NATIVE_HEAP_CAP_VERSION 1
#define KUI_NATIVE_HEAP_CAP_TAIL 1

#ifndef __ASSEMBLER__
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static inline bool kui_native_heap_tail_supported(const void *module,size_t bytes) {
    static const uint8_t fields[8]={KUI_NATIVE_HEAP_CAP_VERSION,0,0,0,KUI_NATIVE_HEAP_CAP_TAIL,0,0,0};
    if(!module || bytes<KUI_NATIVE_HEAP_CAP_OFFSET+16u)return false;
    const uint8_t *record=(const uint8_t *)module+KUI_NATIVE_HEAP_CAP_OFFSET;
    return !memcmp(record,KUI_NATIVE_HEAP_CAP_MAGIC,8u) && !memcmp(record+8u,fields,sizeof(fields));
}
#endif
#endif
