/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_MEDIA_H
#define KUI_MEDIA_H
#include "kui/core.h"

struct kui_media_ops {
    void *ctx;
    uint64_t (*blocks)(void *);
    int (*read)(void *, uint32_t block, size_t count, uint8_t *data);
    int (*write)(void *, uint32_t block, size_t count, const uint8_t *data);
    int (*sync)(void *);
};
/* Set only while unmounted. One worker owns all filesystem/device calls. */
void kui_media_set(const struct kui_media_ops *ops);
/* CD bootstrap only, while unmounted: select an already validated partition
 * without weakening the normal runtime's single-volume policy. The copied
 * raw device needs only blocks/read. Writes are protected and sync is a no-op.
 * Bounds are checked here and again at initialization; filesystem validation
 * remains the caller's mount operation. Failure clears the previous device.
 * kui_media_set(NULL) releases this view before disconnect or another view. */
bool kui_media_boot_view(const struct kui_media_ops *raw,const struct kui_volume *view);
const struct kui_volume *kui_media_volume(void);
const char *kui_media_problem(void);
/* One exclusively owned, read-only metadata walk may memoize successful
 * single-sector reads in caller-owned scratch. Begin starts empty; end must
 * run before releasing the scratch or leaving the walk. Writes, I/O errors
 * and device/view resets disable it. Never use around image verification or
 * across operations: repeated verification passes must reach the device. */
bool kui_media_read_cache_begin(void *scratch,size_t bytes);
void kui_media_read_cache_end(void);
bool kui_media_read_cache_active(void);
/* A bounded lookup walk may pause between its own read-only lookups and keep
 * its memoized sectors. Suspend ends the cache only when `scratch` owns it.
 * Resume restores them only while no cache is active and the media mutation
 * epoch is unchanged: any write, I/O error, invalid request or device/view
 * reset since the suspension discards them. Never resume around image
 * verification; resumed scopes follow the same rules as begin/end. */
struct kui_media_read_cache_saved {void *scratch;size_t bytes,count,next;uint64_t epoch;};
bool kui_media_read_cache_suspend(void *scratch,struct kui_media_read_cache_saved *out);
bool kui_media_read_cache_resume(const struct kui_media_read_cache_saved *saved);
/* Read-only proofs may be carried only inside one exclusive worker operation.
 * Every attempted write, device/view replacement and I/O/refusal error changes
 * this generation. It grants no storage/card identity or lifetime by itself. */
uint64_t kui_media_mutation_epoch(void);

#endif
