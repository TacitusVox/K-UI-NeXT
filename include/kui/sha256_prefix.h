/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_SHA256_PREFIX_H
#define KUI_SHA256_PREFIX_H
#include "kui/hash.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/* One SHA-256 pass over a buffer that also yields the exact SHA-256 of any
 * later-chosen prefix. The streaming state is saved at every stride boundary
 * (a multiple of the 64-byte block, so no partial block is pending); a prefix
 * digest resumes from the nearest saved state at or below the prefix and
 * hashes at most one stride more. Each digest is computed afresh from the
 * prefix bytes themselves; no digest is derived from another one. If the
 * marks cannot be allocated, a prefix digest hashes the whole prefix. */
#define KUI_SHA256_PREFIX_STRIDE 65536u
struct kui_sha256_prefix {
    struct kui_sha256 *marks; /* marks[k]: state after k*STRIDE bytes */
    size_t count,bytes;
};
static inline void kui_sha256_prefix_free(struct kui_sha256_prefix *p) {
    if(!p) return;
    free(p->marks);p->marks=NULL;p->count=p->bytes=0;
}
/* Digest all of data[0..bytes) into `digest`, keeping marks. */
static inline void kui_sha256_prefix_hash(struct kui_sha256_prefix *p,const void *data,size_t bytes,
        uint8_t digest[32]) {
    const uint8_t *at=(const uint8_t *)data;
    struct kui_sha256 sha;kui_sha256_init(&sha);
    kui_sha256_prefix_free(p);
    size_t count=bytes/KUI_SHA256_PREFIX_STRIDE+1u;
    p->marks=(struct kui_sha256 *)malloc(count*sizeof(*p->marks));
    if(p->marks) {p->count=count;p->marks[0]=sha;}
    p->bytes=bytes;
    for(size_t done=0,k=1;done<bytes;k++) {
        size_t n=bytes-done<KUI_SHA256_PREFIX_STRIDE?bytes-done:KUI_SHA256_PREFIX_STRIDE;
        kui_sha256_update(&sha,at+done,n);done+=n;
        if(p->marks && n==KUI_SHA256_PREFIX_STRIDE && k<p->count) p->marks[k]=sha;
    }
    kui_sha256_digest(&sha,digest);
}
/* Exact SHA-256 of data[0..prefix). `data` must be the buffer passed to
 * kui_sha256_prefix_hash and unchanged since. `rehashed` reports the bytes
 * hashed for this call. False only for a prefix beyond the hashed bytes. */
static inline bool kui_sha256_prefix_digest(const struct kui_sha256_prefix *p,const void *data,size_t prefix,
        uint8_t digest[32],size_t *rehashed) {
    if(rehashed) *rehashed=0;
    if(!p || !data || prefix>p->bytes) return false;
    size_t k=p->marks?prefix/KUI_SHA256_PREFIX_STRIDE:0u;
    if(k>=p->count) k=p->count?p->count-1u:0u;
    struct kui_sha256 sha;
    if(p->marks) sha=p->marks[k];
    else {kui_sha256_init(&sha);k=0;}
    size_t start=k*KUI_SHA256_PREFIX_STRIDE;
    kui_sha256_update(&sha,(const uint8_t *)data+start,prefix-start);
    kui_sha256_digest(&sha,digest);
    if(rehashed) *rehashed=prefix-start;
    return true;
}
#endif
