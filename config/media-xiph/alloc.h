/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_MEDIA_XIPH_ALLOC_H
#define KUI_MEDIA_XIPH_ALLOC_H
#include <stdlib.h>
void *kui_xiph_alloc(size_t size);
void *kui_xiph_calloc(size_t count, size_t size);
void *kui_xiph_resize(void *ptr, size_t size);
void kui_xiph_free(void *ptr);
/* Included only in the decoder's third-party translation units. */
#define malloc kui_xiph_alloc
#define calloc kui_xiph_calloc
#define realloc kui_xiph_resize
#define free kui_xiph_free
#endif
