/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_VMU_NATIVE_RESOLUTION_CACHE_H
#define KUI_VMU_NATIVE_RESOLUTION_CACHE_H
#include "kui/vmu_native_layout.h"
#include "kui/vmu_native_heap.h"
#include "kui/vmu_native_exit.h"

#define KUI_VMU_NATIVE_RESOLUTION_CACHE_BYTES 1024u
#define KUI_VMU_NATIVE_RESOLUTION_CACHE_VERSION 1u
/* Bump whenever discovery/witness/uniqueness or patch-admission semantics
 * change. A compatible struct alone does not make an old proof current. */
#define KUI_VMU_NATIVE_RESOLUTION_DETECTOR_EPOCH 8u
struct kui_vmu_native_resolution {
    struct kui_vmu_native_discovery driver;
    struct kui_vmu_native_layout layout;
    struct kui_vmu_native_heap heap;
    struct kui_vmu_native_heap_call call;
    struct kui_vmu_native_exit exits;
};
/* Both bottom and tail leases require the whole pool to be outside the
 * live program. The GUI can disprove placement from these immutable fields
 * before handing control to the stage. Disjoint spans alone grant no lease. */
static inline bool kui_vmu_native_pool_overlaps_program(const struct kui_vmu_native_resolution *r) {
    const uint32_t physical=UINT32_C(0x1fffffff);
    uint32_t program=r->driver.load_address&physical,pool=r->call.entry_begin&physical;
    uint32_t alias=r->call.entry_begin&~physical;
    if((alias!=0u && alias!=UINT32_C(0x80000000) && alias!=UINT32_C(0xa0000000)) ||
       pool<UINT32_C(0x0c000000) || pool>=UINT32_C(0x0d000000) ||
       !r->call.entry_bytes || r->call.entry_bytes>UINT32_C(0x0d000000)-pool ||
       !r->driver.image_bytes)return false;
    return pool>=program?pool-program<r->driver.image_bytes:program-pool<r->call.entry_bytes;
}
/* These two SDK variants require startup and exit witnesses that earlier
 * detached modules cannot replay. Keep their cache records distinguishable. */
static inline bool kui_vmu_native_extended_sdk_proof(const struct kui_vmu_native_resolution *r) {
    return r->exits.family==KUI_VMU_NATIVE_EXIT_EARLY_FIRMWARE_LITERAL_POOL ||
        kui_vmu_native_deferred_maple_startup(r->driver.family,r->call.call,r->layout.initializer_call);
}
/* Complete fresh immutable-image discovery, uniqueness and six-window proof.
 * For an executable without an SDK allocator, a reviewed arena plan for that
 * exact image replaces the allocator proof, and the submit window alone
 * guards its exits (kind 3).
 * Emits canonical little-endian metadata only: no live allocation, SD state,
 * manager snapshot, lease or authority to install anything is serialized.
 * Both output buffers must be disjoint from each other and the image. All
 * APIs leave their outputs untouched on failure. out may be NULL for build. */
bool kui_vmu_native_resolution_cache_build(const void *image,size_t image_bytes,
    uint32_t load,void *wire,size_t wire_bytes,struct kui_vmu_native_resolution *out);
/* GUI only. source must come from hashing the identical, exclusively owned
 * image allocation during this preparation. image_bytes is the independently
 * proven program extent, and may exclude a suffix. No write to the image or
 * checkpoint workspace is permitted between hashing and this call. All fresh
 * discovery checks still run, and ordinary stages hash their own images. */
bool kui_vmu_native_resolution_cache_build_prefixed(const void *image,size_t image_bytes,
    uint32_t load,void *wire,size_t wire_bytes,struct kui_vmu_native_resolution *out,
    const struct kui_sha256_prefix *source);
/* Encode already completely discovered metadata. This only checks schema;
 * it does NOT certify that discovery took place. A caller that pins this
 * record must independently establish the same complete fresh proof. */
bool kui_vmu_native_resolution_cache_encode(const struct kui_vmu_native_resolution *,
    void *wire,size_t wire_bytes);
/* Verify canonical encoding, schema, epoch and the record's integrity hash.
 * The returned digest is not an authentication certificate. In particular,
 * computing it from an untrusted file does not satisfy replay's trust rule. */
bool kui_vmu_native_resolution_cache_record_digest(const void *wire,size_t wire_bytes,
    uint8_t digest[32]);
/* GUI configuration inspection only: validate canonical encoding, integrity
 * and the current detector epoch, then copy immutable metadata. This does
 * not check any executable bytes, certify discovery, or grant live ownership.
 * It may select a hint for a detached stage, which must still hash its own
 * loaded image and perform all ordinary admission checks. Output is unchanged
 * on failure and must not overlap the wire. */
bool kui_vmu_native_resolution_cache_inspect_config(const void *wire,size_t wire_bytes,
    struct kui_vmu_native_resolution *out);
/* Fast replay requires an INDEPENDENTLY TRUSTED expected digest of a record
 * produced by build (or an equally complete fresh proof). The digest must
 * never be taken from the same untrusted cache file; its storage must not
 * overlap wire, image or out. It may be pinned in the immutable personalized
 * module, or supplied under the launcher's explicit ordinary-config trust.
 * Fresh image SHA256+bytes+load and detector epoch must match exactly. These
 * equalities replay the original complete immutable proof, including every
 * uniqueness check, without rescanning target addresses. Live binding,
 * memory ownership, lifetime and installation guards remain mandatory.
 * A false return is a cache miss: caller may run full discovery normally. */
bool kui_vmu_native_resolution_cache_replay(const void *image,size_t image_bytes,
    uint32_t load,const void *wire,size_t wire_bytes,const uint8_t expected_digest[32],
    struct kui_vmu_native_resolution *out);
/* GUI-only invocation-local variant: fresh_image_digest must have just been
 * computed from these exact immutable image bytes in the same exclusively
 * owned preparation. It must never come from a cache record or another
 * launch. No image write is allowed between hashing and replay. The digest
 * must be disjoint from image, wire, expected_digest and out. All record,
 * schema, epoch and SDK call checks are identical to replay above. Detached
 * stages keep using replay, which hashes their own freshly loaded image. */
bool kui_vmu_native_resolution_cache_replay_hashed(const void *image,size_t image_bytes,
    uint32_t load,const void *wire,size_t wire_bytes,const uint8_t expected_digest[32],
    const uint8_t fresh_image_digest[32],struct kui_vmu_native_resolution *out);
/* Strict path for an untrusted record with no independent digest. Rebuilds
 * the entire proof and compares canonical bytes; intentionally not fast. */
bool kui_vmu_native_resolution_cache_validate_untrusted(const void *image,size_t image_bytes,
    uint32_t load,const void *wire,size_t wire_bytes,struct kui_vmu_native_resolution *out);
#endif
