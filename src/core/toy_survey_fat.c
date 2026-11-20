/* SPDX-License-Identifier: GPL-3.0-only */
/* The stream survey's report file, 0:/KUI/SURVEY/STREAM.TXT
 * (include/kui/toy_survey.h), found with the Windows CE data probe's
 * read-only FAT32/exFAT lookup. Only the path differs: on FAT32 the lookup
 * compares the 8.3 names, on exFAT the names without regard to case. Linked
 * into Test 83's cold stage and its host test only, where the probe's own
 * file is never looked for. */
#define KUI_TOY_WIRE_SAVE_PATH \
    {"KUI",{'K','U','I',' ',' ',' ',' ',' ',' ',' ',' '}}, \
    {"SURVEY",{'S','U','R','V','E','Y',' ',' ',' ',' ',' '}}, \
    {"STREAM.TXT",{'S','T','R','E','A','M',' ',' ','T','X','T'}}
#include "wce_probe_fat.c"
