/* SPDX-License-Identifier: GPL-3.0-only */
/* Count application/decoder allocations during one production Games scan.
 * Test-fixture reads and libc-internal allocations are outside this count. */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

void *__real_malloc(size_t n);
void *__real_calloc(size_t count, size_t size);
void *__real_realloc(void *p, size_t n);
void __real_free(void *p);

static struct allocation { void *p; size_t n; } live[512];
static size_t total, peak;
static bool tracking;

static void *observe(void *p, size_t bytes) {
    if(!tracking || !p) return p;
    for(size_t i=0;i<sizeof(live)/sizeof(*live);++i) {
        if(live[i].p) continue;
        live[i]=(struct allocation){p,bytes};
        total+=bytes;
        if(total>peak) peak=total;
        return p;
    }
    abort();
}
static struct allocation *lookup(void *p) {
    if(tracking && p)
        for(size_t i=0;i<sizeof(live)/sizeof(*live);++i)
            if(live[i].p==p) return &live[i];
    return NULL;
}
void *__wrap_malloc(size_t n) { return observe(__real_malloc(n),n); }
void *__wrap_calloc(size_t count, size_t size) { return observe(__real_calloc(count,size),count*size); }
void *__wrap_realloc(void *p, size_t n) {
    struct allocation *old=lookup(p);
    void *next=__real_realloc(p,n);
    if(old && (next || !n)) { total-=old->n; old->p=NULL; }
    return observe(next,n);
}
void __wrap_free(void *p) {
    struct allocation *old=lookup(p);
    if(old) { total-=old->n; old->p=NULL; }
    __real_free(p);
}
void kui_test_start_scan_heap(void) {
    assert(!tracking && !peak);
    tracking=true;
}
size_t kui_test_stop_scan_heap(void) {
    tracking=false;
    return peak;
}
