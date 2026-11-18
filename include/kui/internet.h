/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_INTERNET_H
#define KUI_INTERNET_H
#include "kui/apps.h"

struct kui_internet;
enum kui_internet_route {
    KUI_INTERNET_AUTO, KUI_INTERNET_BBA, KUI_INTERNET_W5500, KUI_INTERNET_WIFI, KUI_INTERNET_DREAMPI,
    KUI_INTERNET_ROUTE_COUNT
};
enum kui_http_result {
    KUI_HTTP_OK, KUI_HTTP_MISSING, KUI_HTTP_LIMITED, KUI_HTTP_STOPPED, KUI_HTTP_ERROR, KUI_HTTP_TOO_LARGE
};
typedef bool (*kui_http_write_fn)(void *ctx, const void *data, size_t size);
typedef void (*kui_http_progress_fn)(void *ctx, uint32_t received, uint32_t total);

/* The exclusive Games I/O worker owns this session. Dial only when explicitly
 * selected. Automatic selection checks BBA, W5500 and the configured Wi-Fi
 * board. The SCI transports must leave an SCI storage card alone. */
struct kui_internet *kui_internet_open(enum kui_internet_route route, kui_log_fn log, kui_cancel_fn cancel, char problem[128]);
void kui_internet_close(struct kui_internet *session);
const char *kui_internet_adapter(const struct kui_internet *session);
const char *kui_internet_problem(const struct kui_internet *session);

/* Stream verified HTTPS to the caller; unknown total size is zero.
 * Redirect and error response bodies never reach write. A failed transfer
 * may have staged part of the body; callers discard it before retrying. */
enum kui_http_result kui_internet_get(struct kui_internet *session, const char *url, uint32_t limit,
    kui_http_write_fn write, void *write_ctx, kui_http_progress_fn progress, void *progress_ctx);
/* Optional early stop for a caller that has verified a complete playable
 * prefix. After a successful write, complete(write_ctx) may finish the GET
 * without reading the rest of the origin asset. At most limit bytes reach
 * write, even when the origin advertises a much longer Content-Length. */
enum kui_http_result kui_internet_get_prefix(struct kui_internet *session, const char *url, uint32_t limit,
    kui_http_write_fn write, void *write_ctx, kui_http_progress_fn progress, void *progress_ctx,
    bool (*complete)(void *write_ctx));
#endif
