/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_ZSO_H
#define KUI_ZSO_H
#include <stddef.h>
#include <stdint.h>

#define KUI_ZSO_HEADER_BYTES 24u
#define KUI_ZSO_BLOCK_BYTES 2048u
#define KUI_ZSO_MAX_SHIFT 9u
#define KUI_ZSO_MAX_SPAN 4096u
#define KUI_ZSO_MAX_BYTES UINT32_C(0x80000000)

/* Explicit out-of-band selection: historical LZO1X and standard LZ4 files
 * share ZISO magic. No reader may infer a codec by retrying a failed block.
 * Detached builds admit exactly one decoder; ordinary host/runtime builds
 * admit all three. DEFLATE is selected only for CISO v1 CSO files. These values also identify the separately typed launch maps. */
enum kui_zso_codec { KUI_ZSO_LZ4 = 0, KUI_ZSO_LZO1X = 1, KUI_ZSO_DEFLATE = 2 };
#ifndef KUI_ZSO_CODECS
#define KUI_ZSO_CODECS 7
#endif
#if KUI_ZSO_CODECS < 1 || KUI_ZSO_CODECS > 7
#error Invalid ZSO codec build mask
#endif

enum kui_zso_result {
    KUI_ZSO_OK, KUI_ZSO_INVALID, KUI_ZSO_UNSUPPORTED,
    KUI_ZSO_IO, KUI_ZSO_CANCELLED, KUI_ZSO_RANGE
};
/* Physical container and decoded ISO lengths are deliberately distinct.
 * index_bytes includes block_count+1 little-endian u32 entries at offset24.
 * Blocks are independent 2048-byte outputs. The codec is selected by the
 * caller, never taken from the ambiguous container signature. */
struct kui_zso_info {
    uint32_t file_bytes, total_bytes, block_count, index_bytes;
    uint8_t shift, codec;
};
/* Zero-copy physical input shared by every reader and decoder. view() makes
 * the byte at offset readable and returns a pointer to it with the number of
 * contiguous readable bytes there: at least 1 and at most wanted. Callers pass
 * wanted >= 1 and never exceed the admitted file range. The bytes stay valid
 * until the next call through the same context. The callback owns caching,
 * card streams and cancellation. No source pointer/context is retained by any
 * API. Sources stay immutable after index validation. There is no heap,
 * complete-index or compressed-input buffer. */
typedef enum kui_zso_result (*kui_zso_view_fn)(void *context, uint32_t offset,
    uint32_t wanted, const uint8_t **data, uint32_t *available);
/* Copy exactly size physical bytes through view(). Any error is returned
 * unchanged; out may then hold a prefix. size may be zero. */
enum kui_zso_result kui_zso_copy(kui_zso_view_fn view, void *context,
    uint32_t offset, void *out, uint32_t size);
/* Sequential reader over the views of one file range. p..e are the unread
 * bytes of the current view and at is the file offset just past e, so the
 * next unread byte is at at-(e-p); end bounds every view. Decoders read bytes
 * in place, calling kui_zso_cursor_fill only when p == e. */
struct kui_zso_cursor {
    kui_zso_view_fn view;
    void *context;
    const uint8_t *p, *e;
    uint32_t at, end;
    uint32_t got; /* The last view's byte count. */
};
/* Replace an exhausted view with the next one. KUI_ZSO_INVALID at the bound
 * (a stream running past its range), KUI_ZSO_RANGE for a view of zero or more
 * than end-at bytes; a source error is returned unchanged. */
enum kui_zso_result kui_zso_cursor_fill(struct kui_zso_cursor *cursor);
/* Copy size bytes from the cursor into out, refilling as needed. */
enum kui_zso_result kui_zso_cursor_take(struct kui_zso_cursor *cursor,
    void *out, uint32_t size);
/* Validate only the fixed header and declared geometry; info unchanged on
 * error. Separately validate the entire index before publishing an image. */
enum kui_zso_result kui_zso_header_parse(const uint8_t header[KUI_ZSO_HEADER_BYTES],
                                       uint64_t file_bytes, struct kui_zso_info *info);
/* LZ4 requires standard version1/header24/reserved-zero geometry. LZO1X
 * additionally admits historical version0 and ignored header-size/reserved
 * fields; its physical index still begins at24. Other bounds are identical.
 * DEFLATE admits only CISO v1/header24/reserved-zero CSO with 2048-byte
 * independent raw-DEFLATE blocks. Existing header_parse remains strict LZ4. */
enum kui_zso_result kui_zso_header_parse_codec(const uint8_t header[KUI_ZSO_HEADER_BYTES],
    uint64_t file_bytes, enum kui_zso_codec codec, struct kui_zso_info *info);
/* Bounded-page scan checks every offset, index exclusion and terminal/tail
 * bounds. Does not decompress payload blocks. */
enum kui_zso_result kui_zso_validate_index(const struct kui_zso_info *info,
                                          kui_zso_view_fn view, void *context);
/* Read two index entries, recheck their bounds/span, then decode exactly one
 * whole block directly into out. Raw blocks use the same bounded reader.
 * Alignment padding is permitted only below 1<<shift bytes. LZ4 validates
 * match/literal lengths and canonical end conditions. On error out may be
 * partially changed; it must never be used as valid decoded data then. */
enum kui_zso_result kui_zso_read_block(const struct kui_zso_info *info,
                                     uint32_t block, kui_zso_view_fn view,
                                     void *context, uint8_t out[KUI_ZSO_BLOCK_BYTES]);
const char *kui_zso_result_name(enum kui_zso_result result);
#endif
