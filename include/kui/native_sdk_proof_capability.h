/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_NATIVE_SDK_PROOF_CAPABILITY_H
#define KUI_NATIVE_SDK_PROOF_CAPABILITY_H

/* Fixed in the package entry. The compact survey stage alone provides
 * these witnesses and its physical-controller reporting worker. */
#define KUI_NATIVE_SDK_PROOF_OFFSET 0x1f0
#define KUI_NATIVE_SDK_PROOF_MAGIC "KUISDP01"
#define KUI_NATIVE_SDK_PROOF_VERSION 1
#define KUI_NATIVE_SDK_PROOF_EXTENDED 1

#ifndef __ASSEMBLER__
#include "kui/native_heap_capability.h"
#include "kui/vmu_native_resolution_cache.h"

static inline bool kui_native_reader_admits_sdk_proof(const void *module,size_t bytes,bool dual,
    const struct kui_vmu_native_resolution *r) {
    if(kui_vmu_native_heap_call_is_tail(&r->heap,&r->call) &&
       (!dual || !kui_native_heap_tail_supported(module,bytes)))return false;
    if(!kui_vmu_native_extended_sdk_proof(r))return true;
    static const uint8_t cap[16]={ 'K','U','I','S','D','P','0','1',
        KUI_NATIVE_SDK_PROOF_VERSION,0,0,0,KUI_NATIVE_SDK_PROOF_EXTENDED,0,0,0 };
    return dual && module && bytes>=KUI_NATIVE_SDK_PROOF_OFFSET+sizeof(cap) &&
        !memcmp((const uint8_t *)module+KUI_NATIVE_SDK_PROOF_OFFSET,cap,sizeof(cap));
}
#endif
#endif
