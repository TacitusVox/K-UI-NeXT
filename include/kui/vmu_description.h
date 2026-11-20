/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_VMU_DESCRIPTION_H
#define KUI_VMU_DESCRIPTION_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* A one-byte halfwidth kana may expand to three UTF-8 bytes. */
#define KUI_VMU_NAME_UTF8_CAP (12u*3u+1u)
#define KUI_VMU_SHORT_UTF8_CAP (16u*3u+1u)
#define KUI_VMU_LONG_UTF8_CAP (32u*3u+1u)

struct kui_vmu_description {
    char description[KUI_VMU_LONG_UTF8_CAP],app_id[KUI_VMU_SHORT_UTF8_CAP];
};
/* Display-only interpretation of the 128-byte embedded VMS header. package_bytes
 * is the file capacity remaining from its directory header-block offset. This
 * checks the documented package dimensions, not CRC or payload contents, and
 * never changes save bytes or authorizes importing/restoring a package. Fixed
 * text fields are bounded and decoded from Shift-JIS to trimmed display UTF-8.
 * On an implausible/truncated header, out is empty and false is returned. */
bool kui_vmu_description_parse(const uint8_t *header,size_t header_bytes,
    size_t package_bytes,struct kui_vmu_description *out);
#endif
