/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_MEDIA_SCAN_REPORT_H
#define KUI_MEDIA_SCAN_REPORT_H

#include "kui/games.h"

#define KUI_MEDIA_REPORT_ROWS 4u
#define KUI_MEDIA_REPORT_PATH "0:/KUI/media/last-scan.txt"

struct kui_media_report_entry {
    char title[KUI_DEST_NAME_CAP],path[KUI_GAMES_FILE_CAP];
    char category[24],asset[32],reason[128],product[16];
};
struct kui_media_report_page {
    struct kui_media_report_entry entries[KUI_MEDIA_REPORT_ROWS];
    unsigned count,first,total,previous,next;
    char status[32],scope[32],kinds[32];
};
enum kui_media_report_result { KUI_MEDIA_REPORT_OK, KUI_MEDIA_REPORT_NONE, KUI_MEDIA_REPORT_ERROR };
struct kui_media_report_writer;
/* Only the storage worker calls these. No file handle survives an API call. */
struct kui_media_report_writer *kui_media_report_begin(bool all,unsigned kinds,kui_log_fn log);
void kui_media_report_add(struct kui_media_report_writer *writer,const struct kui_media_report_entry *entry);
bool kui_media_report_finish(struct kui_media_report_writer *writer,const char *status,bool publish);
bool kui_media_report_saved(void);
/* 0 opens the first page. Other cursors come from that report's next/previous fields. */
enum kui_media_report_result kui_media_report_read_page(unsigned cursor,struct kui_media_report_page *out,kui_log_fn log);
#endif
