/* SPDX-License-Identifier: GPL-3.0-only */
/* Runs on a newly seeded FAT32/exFAT image, before the first media scan. */
#include "kui/media_scan_report.h"

static void report_test_entry(unsigned i,struct kui_media_report_entry *row) {
    static const char *const assets[]={"Artwork","Manual","Preview"};
    memset(row,0,sizeof(*row));
    snprintf(row->title,sizeof(row->title),"Report Game [%02u]",i);
    snprintf(row->path,sizeof(row->path),"/Games/Series %u/Disc 1/image.gdi",i);
    snprintf(row->category,sizeof(row->category),"%s",i==48?"Unmatched":"Unavailable");
    snprintf(row->asset,sizeof(row->asset),"%s",i==48?"Game":assets[i%3]);
    snprintf(row->reason,sizeof(row->reason),"%s",i==48?"Unknown disc identity":"No media listed for this release");
    if(i!=48) snprintf(row->product,sizeof(row->product),"FAT-%02u",i);
}

static void report_check_page(unsigned cursor,unsigned first,unsigned total,const char *status,
        struct kui_media_report_page *out) {
    assert(kui_media_report_read_page(cursor,out,log_line)==KUI_MEDIA_REPORT_OK);
    assert(out->total==total && out->first==first && !strcmp(out->status,status));
    assert(!strcmp(out->scope,total?"All games":"Selected game or set"));
    assert(!strcmp(out->kinds,total?"Artwork Manual Preview":"Preview"));
    unsigned expected=total-first;
    if(expected>4) expected=4;
    assert(out->count==expected);
    for(unsigned i=0;i<expected;i++) {
        struct kui_media_report_entry row;
        report_test_entry(first+i,&row);
        assert(!strcmp(out->entries[i].category,row.category) && !strcmp(out->entries[i].asset,row.asset));
        assert(!strcmp(out->entries[i].title,row.title) && !strcmp(out->entries[i].product,row.product));
        assert(!strcmp(out->entries[i].reason,row.reason) && !strcmp(out->entries[i].path,row.path));
    }
}

static void media_scan_report_card(void) {
    static const char final[]="0:/KUI/media/last-scan.txt";
    const unsigned kinds=KUI_GAMES_MEDIA_ART|KUI_GAMES_MEDIA_MANUAL|KUI_GAMES_MEDIA_PREVIEW;
    struct kui_media_report_page page;
    assert(kui_media_report_read_page(0,&page,log_line)==KUI_MEDIA_REPORT_NONE);
    struct kui_media_report_writer *writer=kui_media_report_begin(true,kinds,log_line);
    assert(writer);
    for(unsigned i=0;i<49;i++) {
        struct kui_media_report_entry row;
        report_test_entry(i,&row);
        kui_media_report_add(writer,&row);
        /* An asset fetch or cold catalogue can release the mounted FatFs
         * work area. A report writer must not keep a live FIL across it. */
        if(i%7==3) assert(kui_games_storage_release());
    }
    assert(kui_media_report_finish(writer,"Completed",true) && kui_media_report_saved());
    assert(kui_games_storage_release());
    unsigned cursor=0,previous=0;
    for(unsigned first=0;first<49;first+=4) {
        report_check_page(cursor,first,49,"Completed",&page);
        unsigned next=page.next;
        if(first) {
            assert(page.previous==previous);
            report_check_page(page.previous,first-4,49,"Completed",&page);
        }
        if(first+4<49) assert(next && next!=cursor);
        else assert(next==0);
        previous=cursor;
        cursor=next;
    }

    /* A torn temporary report is never offered in place of the completed scan. */
    assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
    art_write("0:/KUI/media/last-scan.tmp","incomplete",10);
    assert(kui_games_storage_release());
    report_check_page(0,0,49,"Completed",&page);

    /* Refuse the final rename. The existing report remains readable across
     * a remount even after its directory entry was moved to a backup. */
    writer=kui_media_report_begin(false,KUI_GAMES_MEDIA_PREVIEW,log_line);
    assert(writer);
    art_fault.kind=ART_RENAME_ERROR;art_fault.path=final;art_fault.touched=false;
    assert(!kui_media_report_finish(writer,"Completed",true));
    assert(art_fault.touched);
    memset(&art_fault,0,sizeof(art_fault));
    assert(kui_games_storage_release());
    report_check_page(0,0,49,"Previous: Completed",&page);

    /* A later good scan with zero failures must replace the old 49 rows. */
    writer=kui_media_report_begin(false,KUI_GAMES_MEDIA_PREVIEW,log_line);
    assert(writer && kui_media_report_finish(writer,"Completed",true));
    assert(kui_games_storage_release());
    report_check_page(0,0,0,"Completed",&page);
    assert(page.next==0 && page.previous==0);
    assert(kui_media_report_read_page(1,&page,log_line)==KUI_MEDIA_REPORT_ERROR);
    /* A corrupt final header can never hide the complete backup. */
    assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
    art_write(final,"truncated final",15);
    assert(kui_games_storage_release());
    report_check_page(0,0,49,"Previous: Completed",&page);
    assert(page.next);
    report_check_page(page.next,4,49,"Previous: Completed",&page);
    report_check_page(page.previous,0,49,"Previous: Completed",&page);
    assert(kui_games_storage_release());
    puts("PASS 48 unavailable assets and one unmatched game, offline paging, interrupted report, failed publication, and zero-result replacement");
}
