/* SPDX-License-Identifier: GPL-3.0-only */
/* Actual extracted GUI worker + production FatFs/updater/app services.
 * The network origin and exclusive storage/audio/progress boundaries are modeled. */
#include "kui/shell.h"
#include "kui/update_boot.h"
#include "kui/update_handoff.h"
#include "kui/update_software_file.h"
#include "kui/update_zip.h"
#include "kui/update_nightly.h"
#include "kui/update_keep_file.h"
#include "kui/games_storage.h"
#include "kui/music.h"
#include "kui/storage_policy.h"
#include "kui/media.h"
#include "diskio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef KUI_BUILD_ID
#define KUI_BUILD_ID "111111111111"
#endif
#define UPDATES "0:/KUI/updates"
#define FULL_PATH UPDATES "/full.kup"
#define A_PATH UPDATES "/partial-a.kup"
#define B_PATH UPDATES "/partial-b.kup"
#define SAME_LABEL_PATH UPDATES "/same-label.kup"
#define WRAPPED_PATH UPDATES "/repeat.zip"
#define OLD_PACKAGE UPDATES "/legacy-gui2.kup"
#define HELPER "0:/KUI/apps/games/flow-helper.bin"
#define OLD_BANK "222222333333"
static uint8_t *disk;static size_t disk_bytes;
static FATFS fs;static bool mounted,paused;
static LBA_t fail_read_sector;
static unsigned checks,writes,update_tokens,generation,pauses,resumes;
static struct kui_shell_update_view update_reviewed;
static struct kui_update_boot_view update_boot_view;
static struct kui_games_selection last_selection;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"GUI updater flow failed %u: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint64_t blocks(void *unused) {(void)unused;return disk_bytes/512u;}
static int read_blocks(void *unused,uint32_t lba,size_t count,uint8_t *out) {
    (void)unused;if((uint64_t)lba+count>blocks(NULL))return -1;
    if(fail_read_sector && fail_read_sector>=lba && fail_read_sector-lba<count) {
        fail_read_sector=0;return -1;
    }
    memcpy(out,disk+(size_t)lba*512u,count*512u);return 0;
}
static int write_blocks(void *unused,uint32_t lba,size_t count,const uint8_t *in) {
    (void)unused;if((uint64_t)lba+count>blocks(NULL))return -1;
    ++writes;memcpy(disk+(size_t)lba*512u,in,count*512u);return 0;
}
static int sync_blocks(void *unused) {(void)unused;return 0;}
static const struct kui_media_ops media={NULL,blocks,read_blocks,write_blocks,sync_blocks};
uint32_t kui_clock_fattime(void) {return ((2026u-1980u)<<25)|(11u<<21)|(5u<<16);}
static void kui_log(const char *format,...) {(void)format;}
static bool kui_cancelled(void) {return false;}
static void app_progress(const struct kui_app_status *status) {CHECK(status && mounted);}
static void publish_music(void) {}
void kui_music_status_copy(struct kui_music_status *out) {
    *out=(struct kui_music_status){.enabled=true,.loaded=true,.paused=paused};
}
void kui_music_pause(void) {CHECK(!paused);paused=true;++pauses;}
void kui_music_resume(void) {CHECK(paused);paused=false;++resumes;}
static void kui_sd_set_params(unsigned transport,bool checked) {CHECK(transport==KUI_STORAGE_AUTO && checked);}
static unsigned kui_storage_selected(void) {return KUI_STORAGE_SCI;}
enum kui_games_storage_result kui_games_storage_open(kui_log_fn log) {
    CHECK(log==kui_log && !mounted);
    kui_media_set(&media); /* A new SD connection resets the failed-media latch. */
    if(f_mount(&fs,"0:",1)!=FR_OK)return KUI_GAMES_STORAGE_NO_MOUNT;
    mounted=true;return KUI_GAMES_STORAGE_OK;
}
bool kui_games_storage_release(void) {CHECK(mounted);mounted=false;return f_mount(NULL,"0:",0)==FR_OK;}

/* The worker, downloader, on-card cache, parser and installer are production
 * code. Only serve the chosen test KUP and its manifest at the HTTPS boundary. */
#define FEED "https://raw.githubusercontent.com/TacitusVox/K-UI-NeXT/refs/heads/codex/themes-wince-bba-ftp/evidence/themes-bba/"
#define SOURCE_ID "1234567890abcdef1234567890abcdef12345678"
#define DOWNLOADED KUI_NIGHTLY_DIRECTORY "/nightly-" SOURCE_ID ".kup"
static FILE *origin;
static uint32_t origin_size;
#define RAW "https://raw.githubusercontent.com/TacitusVox/K-UI-NeXT/"
#define ASSETS "https://github.com/TacitusVox/K-UI-NeXT/releases/download/"
static char origin_manifest[256], origin_asset_url[164], origin_catalog[512], origin_digest[65], historical_url[220];
static const char *origin_feed_url, *origin_info_url;
struct kui_internet {bool connected;};
static struct kui_internet session;

struct kui_internet *kui_internet_open(enum kui_internet_route route,kui_log_fn log,kui_cancel_fn cancel,char why[128]) {
    (void)log;(void)why;
    CHECK(route==KUI_INTERNET_BBA && origin && mounted && !session.connected && !cancel());
    session.connected=true;
    return &session;
}
void kui_internet_close(struct kui_internet *connection) {
    CHECK(connection==&session && session.connected);session.connected=false;
}
const char *kui_internet_problem(const struct kui_internet *connection) {
    CHECK(connection==&session);return "Mock origin read failed";
}
enum kui_http_result kui_internet_get(struct kui_internet *connection,const char *url,uint32_t limit,
    kui_http_write_fn write,void *ctx,kui_http_progress_fn progress,void *progress_ctx) {
    (void)progress;(void)progress_ctx;
    CHECK(connection==&session && session.connected);
    if(origin_catalog[0] && !strcmp(url,RAW "nightly-channel/nightlies-v2.txt")) {
        size_t n=strlen(origin_catalog);CHECK(n<=limit && write(ctx,origin_catalog,n));return KUI_HTTP_OK;
    }
    if(!strcmp(url,RAW "nightly-channel/nightlies.txt") || !strcmp(url,RAW "nightly-channel/nightlies-v2.txt") ||
       !strcmp(url,RAW "release-channel/releases.txt")) return KUI_HTTP_MISSING;
    if(!strcmp(url,historical_url) || (origin_info_url && !strcmp(url,origin_info_url))) return KUI_HTTP_MISSING;
    CHECK(!strcmp(url,origin_feed_url));
    size_t length=strlen(origin_manifest);
    CHECK(length<=limit && write(ctx,origin_manifest,length));return KUI_HTTP_OK;
}
enum kui_http_result kui_internet_get_remainder(struct kui_internet *connection,const char *url,uint32_t start,
    uint32_t size,kui_http_write_fn write,void *ctx,kui_http_progress_fn progress,void *progress_ctx) {
    CHECK(connection==&session && session.connected && !strcmp(url,origin_asset_url));
    CHECK(size==origin_size && start<size && !fseek(origin,(long)start,SEEK_SET));
    uint8_t block[8192];
    for(uint32_t at=start;at<size;) {
        size_t bytes=size-at<sizeof(block)?size-at:sizeof(block);
        CHECK(fread(block,1,bytes,origin)==bytes && write(ctx,block,bytes));
        at+=(uint32_t)bytes;progress(progress_ctx,at-start,size-start);
    }
    return KUI_HTTP_OK;
}
#include "actual_gui_update_helpers.inc"

static void mount_volume(void) {CHECK(!mounted && f_mount(&fs,"0:",1)==FR_OK);mounted=true;}
static void release_volume(void) {CHECK(mounted && f_mount(NULL,"0:",0)==FR_OK);mounted=false;}
static void parents(const char *path) {
    char copy[192];CHECK(strlen(path)<sizeof(copy));strcpy(copy,path);
    for(char *p=copy+3u;*p;p++)if(*p=='/') {*p=0;FRESULT r=f_mkdir(copy);CHECK(r==FR_OK || r==FR_EXIST);*p='/';}
}
static void file_write(const char *path,const void *data,size_t size) {
    parents(path);FIL file;UINT done=0u;
    CHECK(f_open(&file,path,FA_WRITE|FA_CREATE_ALWAYS)==FR_OK);
    CHECK(f_write(&file,data,(UINT)size,&done)==FR_OK && done==size);
    CHECK(f_sync(&file)==FR_OK && f_close(&file)==FR_OK);
}
static void copy_host(const char *source,const char *destination) {
    FILE *in=fopen(source,"rb");CHECK(in && !fseek(in,0,SEEK_END));long size=ftell(in);
    CHECK(size>0 && !fseek(in,0,SEEK_SET));uint8_t *data=malloc((size_t)size);CHECK(data);
    CHECK(fread(data,1u,(size_t)size,in)==(size_t)size && !fclose(in));file_write(destination,data,(size_t)size);free(data);
}
static LBA_t first_sector(const char *path) {
    FIL file;uint8_t byte;UINT got=0;
    CHECK(kui_update_physical_open(&file,path,FA_READ)==FR_OK);
    CHECK(f_read(&file,&byte,1,&got)==FR_OK && got==1 && file.sect);
    LBA_t sector=file.sect;CHECK(f_close(&file)==FR_OK);return sector;
}
static struct kui_update_activation activation(void) {
    struct kui_update_activation value;unsigned slot=0u;
    CHECK(kui_update_activation_latest(&value,&slot,kui_cancelled)==KUI_UPDATE_OK);return value;
}
static void activation_write(unsigned slot,const struct kui_update_activation *value) {
    uint8_t wire[128];CHECK(kui_update_activation_encode(value,wire));
    file_write(slot?KUI_UPDATE_ACTIVE1:KUI_UPDATE_ACTIVE0,wire,sizeof(wire));
}
static void corrupt_selection(void) {
    file_write(KUI_UPDATE_ACTIVE0,"corrupt selector A",18u);file_write(KUI_UPDATE_ACTIVE1,"corrupt selector B",18u);
}
static void view_ok(const struct kui_shell_update_view *view) {
    if(!view->status.passed)fprintf(stderr,"GUI status: %s\n",view->status.message);
    CHECK(view->status.complete && view->status.passed && !view->status.errors && !mounted && !paused && pauses==resumes);
}
static struct kui_shell_update_request request(enum kui_shell_update_op op,const char *path) {
    struct kui_shell_update_request value={.op=op,.generation=++generation};strcpy(value.directory,UPDATES);
    if(path)strcpy(value.path,path);
    return value;
}
static struct kui_shell_update_view check_package(const char *path) {
    struct kui_shell_update_request input=request(KUI_SHELL_UPDATE_CHECK,path);struct kui_shell_update_view view;
    update_work(&input,&view);view_ok(&view);CHECK(view.ready && view.stage==KUI_SHELL_UPDATE_REVIEW && view.space_known);
    CHECK(!view.already_installed && view.reviewed.token && (view.reviewed.package.flags&KUI_UPDATE_REQUIRES_ENGINE3));return view;
}
static void check_repeat(const char *path,bool pending) {
    mount_volume();struct kui_update_activation before=activation();release_volume();
    unsigned original_writes=writes;
    struct kui_shell_update_request input=request(KUI_SHELL_UPDATE_CHECK,path);
    struct kui_shell_update_view view;update_work(&input,&view);view_ok(&view);
    CHECK(view.stage==KUI_SHELL_UPDATE_REVIEW && view.already_installed && !view.ready && !view.reviewed.token);
    CHECK(view.restart_required==pending && !strcmp(view.selected,before.selected));
    CHECK(!update_reviewed.ready);
    if(!kui_update_browser_suffix(path,".zip"))CHECK(writes==original_writes);
    /* ZIP inspection is allowed to stage its KUP; activation is read-only. */
    mount_volume();struct kui_update_activation after=activation();release_volume();
    CHECK(update_activation_same(&before,&after));
}
static void check_same_label_and_wrapped_repeat(const char *control,const char *wrapped) {
    mount_volume();copy_host(control,SAME_LABEL_PATH);copy_host(wrapped,WRAPPED_PATH);release_volume();
    struct kui_shell_update_view different=check_package(SAME_LABEL_PATH);
    CHECK(!strcmp(different.gui_build,KUI_BUILD_ID));
    check_repeat(WRAPPED_PATH,false);
    struct kui_shell_update_view denied;unsigned before=writes;
    struct kui_shell_update_request stale=different.reviewed;stale.generation=++generation;
    update_work(&stale,&denied);
    CHECK(denied.status.complete && !denied.status.passed && !denied.ready && writes==before);
}
static struct kui_shell_update_view install_review(const struct kui_shell_update_view *review) {
    struct kui_shell_update_request input=review->reviewed;input.generation=++generation;
    struct kui_shell_update_view view;update_work(&input,&view);view_ok(&view);
    CHECK(view.restart_required && view.selected[0]);return view;
}
static void set_origin(const char *package_path,enum kui_update_channel channel,const char *commit) {
    origin=fopen(package_path,"rb");CHECK(origin);
    struct kui_sha256 sha;kui_sha256_init(&sha);uint8_t buffer[8192],digest[32];char hex[65];
    size_t length;origin_size=0;
    while((length=fread(buffer,1u,sizeof(buffer),origin))!=0) {
        CHECK(length<=UINT32_MAX-origin_size);
        origin_size+=(uint32_t)length;kui_sha256_update(&sha,buffer,length);
    }
    CHECK(!ferror(origin) && origin_size);
    kui_sha256_digest(&sha,digest);kui_hex(digest,sizeof(digest),hex);
    strcpy(origin_digest,hex);
    bool stable=channel==KUI_UPDATE_RELEASES, test=channel==KUI_UPDATE_HARDWARE;
    origin_feed_url=stable?RAW "release-channel/release.txt":test?FEED "nightly.txt":RAW "nightly-channel/nightly.txt";
    origin_info_url=test?NULL:stable?RAW "release-channel/update-info.txt":RAW "nightly-channel/update-info.txt";
    if(stable)strcpy(origin_asset_url,ASSETS "v2.1.0/K-UI-update.kup");
    else if(test)strcpy(origin_asset_url,FEED "K-UI.kup");
    else snprintf(origin_asset_url,sizeof(origin_asset_url),ASSETS "nightly-%s/K-UI.kup",commit);
    snprintf(origin_manifest,sizeof(origin_manifest),"%s\n%s\n%u\n%s\n%s",stable?"KUI-RELEASE-1":"KUI-NIGHTLY-1",
        commit,origin_size,hex,stable?"v2.1.0\nK-UI-update.kup\n":"");
}

static void official_channels(const char *full,const char *partial,const char *reader,const char *full_nightly) {
    struct kui_shell_update_request check=request(KUI_SHELL_UPDATE_NIGHTLY,NULL);
    strcpy(check.directory,KUI_NIGHTLY_DIRECTORY);check.keep=5;check.route=KUI_INTERNET_BBA;
    const char *source=KUI_BUILD_ID "0000000000000000000000000000";
    struct kui_shell_update_view view;
    set_origin(full,KUI_UPDATE_RELEASES,source);
    check.op=KUI_SHELL_UPDATE_NIGHTLY_LIST;
    update_work(&check,&view);view_ok(&view);
    CHECK(view.stage==KUI_SHELL_UPDATE_NIGHTLIES && view.nightlies.count==1 && !strcmp(view.nightlies.items[0].tag,"v2.1.0"));
    check.nightly=view.nightlies.items[0];
    check.op=KUI_SHELL_UPDATE_NIGHTLY;
    update_work(&check,&view);view_ok(&view);
    CHECK(view.ready && view.reviewed.token && (view.reviewed.package.flags&KUI_UPDATE_FULL));
    CHECK(!strcmp(view.gui_build,KUI_BUILD_ID) && view.replacement_count>1 && !strcmp(view.reviewed.nightly.tag,"v2.1.0"));
    CHECK(!fclose(origin));origin=NULL;

    /* A matching file hash alone cannot authorize the wrong payload scope or
     * GUI identity. Each origin below returns a real, structurally valid KUP. */
    const struct {const char *file, *commit;enum kui_update_channel channel;} rejected[]={
        {full,source,KUI_UPDATE_NIGHTLIES}, {partial,SOURCE_ID,KUI_UPDATE_RELEASES},
        {full,"3333333333333333333333333333333333333333",KUI_UPDATE_RELEASES}
    };
    for(size_t i=0;i<sizeof(rejected)/sizeof(rejected[0]);i++) {
        check.channel=rejected[i].channel;check.change_channel=true;
        set_origin(rejected[i].file,check.channel,rejected[i].commit);
        {
            check.op=KUI_SHELL_UPDATE_NIGHTLY_LIST;
            update_work(&check,&view);view_ok(&view);
            CHECK(view.stage==KUI_SHELL_UPDATE_NIGHTLIES && view.nightlies.count==1);
            check.nightly=view.nightlies.items[0];
            check.nightly.digest[0]^=1;
            check.op=KUI_SHELL_UPDATE_NIGHTLY;
            update_work(&check,&view);
            CHECK(view.status.complete && !view.status.passed && !view.ready && strstr(view.status.message,"selection changed"));
            check.nightly.digest[0]^=1;
        }
        update_work(&check,&view);
        CHECK(view.status.complete && !view.status.passed && !view.ready && !view.reviewed.token &&
              strstr(view.status.message,"official channel"));
        CHECK(!fclose(origin));origin=NULL;
    }
    const char *mixed_source=KUI_BUILD_ID "ffffffffffffffffffffffffffff";
    const struct {const char *file,*source,*summary; bool accepted;unsigned changed;} scoped[]={
        {reader,"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","partial 1 0 0 2",true,1},
        {full_nightly,mixed_source,"full 3 0 0 7",true,3},
        {full_nightly,mixed_source,"partial 3 0 0 7",false,0},
        {full_nightly,mixed_source,"full 2 0 0 7",false,0},
        {full_nightly,mixed_source,"full 3 0 0 1",false,0},
        {reader,"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","partial 1 0 0 1",false,0}
    };
    for(unsigned i=0;i<sizeof(scoped)/sizeof(scoped[0]);++i) {
        check.channel=KUI_UPDATE_NIGHTLIES;check.change_channel=true;check.op=KUI_SHELL_UPDATE_NIGHTLY_LIST;
        set_origin(scoped[i].file,check.channel,scoped[i].source);
        snprintf(origin_catalog,sizeof(origin_catalog),"KUI-NIGHTLIES-2\n1\n%s %u %s 2026-11-22T01:00:00Z %s\n",
                 scoped[i].source,origin_size,origin_digest,scoped[i].summary);
        snprintf(historical_url,sizeof(historical_url),RAW "nightly-channel/history/%s/update-info.txt",scoped[i].source);
        update_work(&check,&view);view_ok(&view);
        CHECK(view.nightlies.count==1 && view.nightlies.items[0].described);
        check.nightly=view.nightlies.items[0];check.op=KUI_SHELL_UPDATE_NIGHTLY;
        update_work(&check,&view);
        if(scoped[i].accepted) {
            view_ok(&view);
            CHECK(view.ready && view.replacement_count==scoped[i].changed);
            if(scoped[i].file==reader) CHECK(!view.gui_build[0]);
            else CHECK(!strcmp(view.gui_build,KUI_BUILD_ID));
        } else CHECK(!view.status.passed && !view.ready && strstr(view.status.message,"official channel"));
        CHECK(!fclose(origin));origin=NULL;
    }
    historical_url[0]=origin_catalog[0]=0;
}

static struct kui_shell_update_view fetch_review(const char *package_path) {
    struct kui_shell_update_request input=request(KUI_SHELL_UPDATE_OPTIONS,NULL);
    struct kui_shell_update_view view;
    update_work(&input,&view);view_ok(&view);
    CHECK(view.keep_known && view.keep==KUI_UPDATE_KEEP_DEFAULT && view.test_feed_available);
    set_origin(package_path,KUI_UPDATE_HARDWARE,SOURCE_ID);
    input=request(KUI_SHELL_UPDATE_NIGHTLY,NULL);input.route=KUI_INTERNET_BBA;
    input.test_feed=true;input.keep=KUI_UPDATE_KEEP_DEFAULT;strcpy(input.directory,KUI_NIGHTLY_DIRECTORY);
    update_work(&input,&view);view_ok(&view);CHECK(!session.connected && !fclose(origin));origin=NULL;
    CHECK(view.ready && !view.restore && view.stage==KUI_SHELL_UPDATE_REVIEW && view.reviewed.token);
    CHECK(view.reviewed.op==KUI_SHELL_UPDATE_INSTALL && !strcmp(view.reviewed.path,DOWNLOADED));
    CHECK(!strcmp(view.reviewed.directory,input.directory) && view.reviewed.package.flags&KUI_UPDATE_REQUIRES_ENGINE3);
    return view;
}
static void browse_without_selection(void) {
    struct kui_shell_update_request input=request(KUI_SHELL_UPDATE_LIST,NULL);
    struct kui_shell_update_view view;unsigned before=writes;
    update_work(&input,&view);view_ok(&view);
    CHECK(view.stage==KUI_SHELL_UPDATE_PACKAGES && view.total==3u && !view.selection_known && writes==before);
    input=request(KUI_SHELL_UPDATE_CHECK,FULL_PATH);update_work(&input,&view);
    CHECK(view.status.complete && !view.status.passed && view.status.errors && !view.ready &&
        !view.selection_known && writes==before && !mounted && !paused && pauses==resumes);
    input=request(KUI_SHELL_UPDATE_INSTALL,FULL_PATH);update_work(&input,&view);
    CHECK(view.status.complete && !view.status.passed && view.status.errors && !view.ready &&
        !view.selection_known && writes==before && !mounted && !paused && pauses==resumes);
}
static void expect_helper(const char *expected) {
    FIL file;UINT got=0u;char data[64]={0};CHECK(f_open(&file,HELPER,FA_READ)==FR_OK);
    CHECK(f_read(&file,data,sizeof(data)-1u,&got)==FR_OK && got==strlen(expected) && !memcmp(data,expected,got));
    CHECK(f_close(&file)==FR_OK);
}
static void restart_selected(void) {
    mount_volume();struct kui_runtime_image image={0};
    CHECK(kui_update_boot_reload(KUI_BUILD_ID,KUI_STORAGE_SCI,&image,&update_boot_view,kui_log,kui_cancelled)==KUI_UPDATE_BOOT_CHAIN);
    CHECK(image.data && !strcmp(image.info.build,KUI_BUILD_ID));
    const uint8_t *body=image.data;uint32_t offset=UINT32_MAX;
    for(uint32_t i=0;i+KUI_UPDATE_HANDOFF_BYTES<=image.info.payload_bytes;i++)if(body[i]=='K') {
        struct kui_update_handoff parsed;
        if(kui_update_handoff_decode(body+i,&parsed)) {CHECK(offset==UINT32_MAX);offset=i;}
    }
    CHECK(offset!=UINT32_MAX);struct kui_update_handoff_context context={.payload_offset=offset};
    memcpy(context.wire,body+offset,sizeof(context.wire));kui_runtime_free(&image);kui_update_boot_reload_abort();
    kui_update_boot_reset();
    CHECK(kui_update_boot_read_handoff(KUI_BUILD_ID,KUI_STORAGE_SCI,&context,&image,&update_boot_view,kui_log,kui_cancelled)==KUI_UPDATE_BOOT_PINNED);
    CHECK(!image.data && !strcmp(update_boot_view.build,KUI_BUILD_ID));
    struct kui_update_package *receipt=malloc(sizeof(*receipt));CHECK(receipt);
    CHECK(kui_update_bank_receipt_read(update_boot_view.bank,receipt,kui_cancelled)==KUI_UPDATE_OK);
    CHECK(receipt->flags&KUI_UPDATE_REQUIRES_ENGINE3);free(receipt);release_volume();
}
static void shared_records(unsigned phase) {
    mount_volume();struct kui_game_launch_settings settings,found_settings;bool found=false;
    CHECK(kui_game_launch_settings_lookup("/Games/Original/disc.gdi",&found_settings,&found,kui_log) && found &&
        found_settings.reader==KUI_GAME_READER_BACKGROUND);
    for(unsigned i=1u;i<phase;i++) {
        char path[80];snprintf(path,sizeof(path),"/Games/Phase %u/disc.gdi",i);
        CHECK(kui_game_launch_settings_lookup(path,&found_settings,&found,kui_log) && found && found_settings.reader==i%KUI_GAME_READER_COUNT);
    }
    kui_game_launch_settings_default(&settings);settings.reader=phase%KUI_GAME_READER_COUNT;
    char path[80];snprintf(path,sizeof(path),"/Games/Phase %u/disc.gdi",phase);
    CHECK(kui_game_launch_settings_save(path,&settings,kui_log));
    CHECK(kui_game_launch_settings_lookup(path,&found_settings,&found,kui_log) && found && found_settings.reader==settings.reader);
    struct kui_games_selection loaded;CHECK(kui_games_selection_load(&loaded,kui_log) && kui_games_selection_equal(&loaded,&last_selection));
    snprintf(last_selection.name,sizeof(last_selection.name),"Phase %u",phase);
    CHECK(kui_games_selection_save(&last_selection,kui_log));
    CHECK(kui_games_selection_load(&loaded,kui_log) && kui_games_selection_equal(&loaded,&last_selection));release_volume();
}
static void seed_old_bank(void) {
    FIL file;UINT got=0;uint8_t data[576];CHECK(kui_update_physical_open(&file,"0:/KUI/old-gui.kui",FA_READ)==FR_OK);
    CHECK(f_read(&file,data,sizeof(data),&got)==FR_OK && got==sizeof(data) && f_close(&file)==FR_OK);
    struct kui_update_package *p=calloc(1,sizeof(*p));CHECK(p);
    p->version=2u;p->flags=KUI_UPDATE_FULL;p->count=1u;strcpy(p->build,OLD_BANK);
    struct kui_update_entry *entry=p->entries;strcpy(entry->path,"runtime.kui");strcpy(entry->build,"222222222222");
    entry->bytes=sizeof(data);entry->policy=KUI_UPDATE_SELF_KUI;
    struct kui_sha256 hash;kui_sha256_init(&hash);kui_sha256_update(&hash,data,sizeof(data));kui_sha256_digest(&hash,entry->sha256);
    CHECK(kui_update_package_seal(p)==KUI_UPDATE_OK);char path[160];
    CHECK(kui_update_bank_path(OLD_BANK,"runtime.kui",path,sizeof(path)));file_write(path,data,sizeof(data));
    uint8_t wire[KUI_UPDATE_RECEIPT_MAX];size_t bytes=0u;CHECK(kui_update_receipt_encode(p,wire,sizeof(wire),&bytes)==KUI_UPDATE_OK);
    snprintf(path,sizeof(path),KUI_UPDATE_SYSTEM "/%s/package.bin",OLD_BANK);file_write(path,wire,bytes);
    /* Deliberately construct a historical flags0 delivery as negative data.
     * The current producer correctly refuses to build this obsolete GUI. */
    p->flags=0u;CHECK(kui_update_package_seal(p)==KUI_UPDATE_OK);
    CHECK(kui_update_receipt_encode(p,wire,sizeof(wire),&bytes)==KUI_UPDATE_OK);
    uint8_t *legacy=malloc(bytes+sizeof(data));CHECK(legacy);
    memcpy(legacy,wire,bytes);memcpy(legacy+bytes,data,sizeof(data));
    file_write(OLD_PACKAGE,legacy,bytes+sizeof(data));free(legacy);free(p);
}
static void verify_real_members(const struct kui_update_package *package) {
    for(uint32_t i=0u;i<package->count;i++) {
        const struct kui_update_entry *entry=package->entries+i;char logical[192];
        CHECK(entry->policy==KUI_UPDATE_SELF_KUI || entry->policy==KUI_UPDATE_RAW);
        CHECK(snprintf(logical,sizeof(logical),"0:/KUI/%s",entry->path)<(int)sizeof(logical));
        FIL file;CHECK(f_open(&file,logical,FA_READ)==FR_OK && f_size(&file)==entry->bytes);
        uint8_t data[32768],digest[32];struct kui_sha256 hash;kui_sha256_init(&hash);uint64_t total=0u;
        for(;;) {
            UINT got=0u;CHECK(f_read(&file,data,sizeof(data),&got)==FR_OK);if(!got)break;
            kui_sha256_update(&hash,data,got);total+=got;
        }
        CHECK(f_close(&file)==FR_OK && total==entry->bytes);kui_sha256_digest(&hash,digest);
        CHECK(!memcmp(digest,entry->sha256,sizeof(digest)));
        if(entry->policy==KUI_UPDATE_SELF_KUI) {
            struct kui_runtime_image image={0};CHECK(kui_runtime_read(logical,&image,kui_log,kui_cancelled)==KUI_RUNTIME_OK);
            CHECK(!strcmp(image.info.build,entry->build));kui_runtime_free(&image);
        }
    }
}
static void restart_root(void) {
    mount_volume();struct kui_runtime_image image={0};
    CHECK(kui_update_boot_reload(KUI_BUILD_ID,KUI_STORAGE_SCI,&image,&update_boot_view,kui_log,kui_cancelled)==KUI_UPDATE_BOOT_CHAIN);
    CHECK(image.data && !strcmp(image.info.build,KUI_BUILD_ID) && !update_boot_view.bank[0]);
    struct kui_update_handoff_context context;CHECK(kui_update_handoff_empty(image.data,image.info.payload_bytes,&context.payload_offset));
    memcpy(context.wire,(uint8_t *)image.data+context.payload_offset,sizeof(context.wire));
    kui_runtime_free(&image);kui_update_boot_reload_abort();kui_update_boot_reset();
    CHECK(kui_update_boot_read_handoff(KUI_BUILD_ID,KUI_STORAGE_SCI,&context,&image,&update_boot_view,kui_log,kui_cancelled)==KUI_UPDATE_BOOT_BASELINE);
    CHECK(!image.data && !update_boot_view.bank[0]);release_volume();
}
static void real_artifact_flow(const char *runtime_path,const char *package_path,const char *control,const char *wrapped) {
    /* All positive executable/helper bytes come from the supplied real KUP.
     * Only the disk and mutable user records are generated by this test. */
    copy_host(runtime_path,KUI_RUNTIME_PATH);copy_host(package_path,FULL_PATH);
    struct kui_update_package *package=malloc(sizeof(*package));CHECK(package);
    struct kui_app_status status;CHECK(kui_update_inspect(FULL_PATH,package,&status,kui_log,kui_cancelled,NULL)==KUI_UPDATE_OK);
    CHECK(package->flags&KUI_UPDATE_REQUIRES_ENGINE3);FILE *input=fopen(package_path,"rb");CHECK(input);
    for(uint32_t i=0u;i<package->count;i++) {
        const struct kui_update_entry *entry=package->entries+i;char physical[192];
        CHECK(entry->policy==KUI_UPDATE_SELF_KUI || entry->policy==KUI_UPDATE_RAW);
        CHECK(entry->payload_offset<=0x7fffffffu && !fseek(input,(long)entry->payload_offset,SEEK_SET));
        uint8_t *data=malloc((size_t)entry->bytes);CHECK(data || !entry->bytes);
        CHECK(fread(data,1u,(size_t)entry->bytes,input)==entry->bytes);
        CHECK(snprintf(physical,sizeof(physical),"0:/KUI/%s",entry->path)<(int)sizeof(physical));
        file_write(physical,data,(size_t)entry->bytes);free(data);
    }
    CHECK(!fclose(input));struct kui_update_activation root={.sequence=1u};activation_write(0u,&root);
    root.sequence=2u;activation_write(1u,&root);
    struct kui_game_launch_settings settings;kui_game_launch_settings_default(&settings);settings.reader=KUI_GAME_READER_BACKGROUND;
    CHECK(kui_game_launch_settings_save("/Games/Original/disc.gdi",&settings,kui_log));
    kui_games_selection_default(&last_selection);strcpy(last_selection.name,"Original");CHECK(kui_games_selection_save(&last_selection,kui_log));
    kui_update_boot_require_engine(KUI_UPDATE_ENGINE_CURRENT);struct kui_runtime_image image={0};unsigned before=writes;
    CHECK(kui_update_boot_read(KUI_BUILD_ID,KUI_STORAGE_SCI,&image,&update_boot_view,kui_log,kui_cancelled)==KUI_UPDATE_BOOT_BASELINE);
    CHECK(!image.data && writes==before);verify_real_members(package);release_volume();
    struct kui_shell_update_request request_list=request(KUI_SHELL_UPDATE_LIST,NULL);struct kui_shell_update_view view;
    before=writes;update_work(&request_list,&view);view_ok(&view);
    CHECK(view.total==1u && view.selection_known && writes==before);
    struct kui_shell_update_view review=check_package(FULL_PATH);CHECK(writes==before);
    mount_volume();root.sequence=3u;activation_write(0u,&root);release_volume();
    struct kui_shell_update_request confirmation=review.reviewed;confirmation.generation=++generation;before=writes;
    update_work(&confirmation,&view);CHECK(!view.ready && !view.status.passed && writes==before);
    review=fetch_review(package_path);view=install_review(&review);
    CHECK(strcmp(view.selected,package->build));char bank[13];strcpy(bank,view.selected);
    check_repeat(FULL_PATH,true);restart_selected();check_repeat(FULL_PATH,false);
    check_same_label_and_wrapped_repeat(control,wrapped);
    mount_volume();verify_real_members(package);release_volume();shared_records(1u);
    struct kui_shell_update_request restore=request(KUI_SHELL_UPDATE_BASELINE,NULL);
    update_work(&restore,&review);view_ok(&review);CHECK(review.ready && review.restore && review.baseline && !review.reviewed.target[0]);
    confirmation=review.reviewed;confirmation.generation=++generation;update_work(&confirmation,&view);view_ok(&view);
    CHECK(!view.selected[0] && view.restart_required);restart_root();
    mount_volume();verify_real_members(package);corrupt_selection();release_volume();
    before=writes;update_work(&request_list,&view);view_ok(&view);CHECK(view.selection_known && writes==before);
    review=check_package(FULL_PATH);view=install_review(&review);CHECK(!strcmp(view.selected,bank));restart_selected();
    mount_volume();verify_real_members(package);release_volume();shared_records(2u);
    CHECK(!mounted && !paused && pauses==resumes);
    printf("PASS real artifact GUI flow: %u checks; GUI %.12s, exact %u-file KUP %.12s, physical bootstrap software, BBA download/review/install, stale review refusal, bank/root restart and restore, corrupt-selector recovery/reinstall, exact module/helper bytes and real shared presets/selection\n",
        checks,KUI_BUILD_ID,package->count,package->build);free(package);
}
int main(int argc,char **argv) {
    bool real=argc==7 && !strcmp(argv[1],"--real");CHECK(real || argc==11);
    FILE *host=fopen(argv[real?2:1],"rb");CHECK(host && !fseek(host,0,SEEK_END));long size=ftell(host);
    CHECK(size>0 && !fseek(host,0,SEEK_SET));disk_bytes=(size_t)size;disk=malloc(disk_bytes);CHECK(disk);
    CHECK(fread(disk,1u,disk_bytes,host)==disk_bytes && !fclose(host));kui_media_set(&media);mount_volume();
    if(real) {real_artifact_flow(argv[3],argv[4],argv[5],argv[6]);kui_update_boot_reset();free(disk);return 0;}
    copy_host(argv[2],KUI_RUNTIME_PATH);copy_host(argv[3],"0:/KUI/old-gui.kui");
    copy_host(argv[4],FULL_PATH);copy_host(argv[5],A_PATH);copy_host(argv[6],B_PATH);
    file_write(HELPER,"root helper",11u);corrupt_selection();
    LBA_t old_helper_sector=first_sector(HELPER),package_sector=first_sector(FULL_PATH);
    struct kui_game_launch_settings settings;kui_game_launch_settings_default(&settings);settings.reader=KUI_GAME_READER_BACKGROUND;
    CHECK(kui_game_launch_settings_save("/Games/Original/disc.gdi",&settings,kui_log));
    kui_games_selection_default(&last_selection);strcpy(last_selection.name,"Original");CHECK(kui_games_selection_save(&last_selection,kui_log));
    /* Without a checked running context, corrupt selectors must not prevent
     * browsing; review and publication remain unavailable and read-only. */
    release_volume();browse_without_selection();mount_volume();
    kui_update_boot_require_engine(KUI_UPDATE_ENGINE_CURRENT);struct kui_runtime_image image={0};unsigned before=writes;
    CHECK(kui_update_boot_read(KUI_BUILD_ID,KUI_STORAGE_SCI,&image,&update_boot_view,kui_log,kui_cancelled)==KUI_UPDATE_BOOT_BASELINE);
    CHECK(!image.data && writes==before);release_volume();
    struct kui_shell_update_request input=request(KUI_SHELL_UPDATE_LIST,NULL);struct kui_shell_update_view view;
    before=writes;update_work(&input,&view);view_ok(&view);
    CHECK(view.stage==KUI_SHELL_UPDATE_PACKAGES && view.total==3u && writes==before);
    kui_update_boot_block();browse_without_selection();mount_volume();before=writes;
    CHECK(kui_update_boot_read(KUI_BUILD_ID,KUI_STORAGE_SCI,&image,&update_boot_view,kui_log,kui_cancelled)==KUI_UPDATE_BOOT_BASELINE);
    CHECK(!image.data && writes==before);release_volume();
    /* An unreadable old helper is optional for a FULL replacement. Fail an
     * actual adapter read so production diskio marks this session unusable.
     * Confirmation reconnects and must still refuse an unreadable package. */
    fail_read_sector=old_helper_sector;
    struct kui_shell_update_view review=check_package(FULL_PATH);
    CHECK(!fail_read_sector && review.space_known && review.changes && !review.changes->known && writes==before);
    fail_read_sector=package_sector;
    input=review.reviewed;input.generation=++generation;update_work(&input,&view);
    CHECK(!fail_read_sector && !mounted && view.status.errors && !view.ready && !view.status.passed && writes==before);
    review=check_package(FULL_PATH);CHECK(writes==before);
    /* Change the real saved selection between review and confirmation. */
    mount_volume();struct kui_update_activation changed={.sequence=17u};activation_write(0u,&changed);release_volume();
    input=review.reviewed;input.generation=++generation;before=writes;update_work(&input,&view);
    CHECK(view.status.complete && !view.status.passed && !view.ready && strstr(view.status.message,"review changed") && writes==before);
    official_channels(argv[4],argv[5],argv[9],argv[10]);
    mount_volume();corrupt_selection();release_volume();fail_read_sector=old_helper_sector;
    review=fetch_review(argv[4]);CHECK(!fail_read_sector && review.changes && !review.changes->known);
    view=install_review(&review);
    CHECK(strcmp(view.selected,review.reviewed.package.build));char full_bank[13];strcpy(full_bank,view.selected);
    mount_volume();CHECK(!strcmp(activation().selected,view.selected));release_volume();
    check_repeat(FULL_PATH,true);restart_selected();check_repeat(FULL_PATH,false);
    check_same_label_and_wrapped_repeat(argv[7],argv[8]);
    mount_volume();expect_helper("full helper");release_volume();shared_records(1u);
    review=check_package(A_PATH);view=install_review(&review);char a_bank[13];strcpy(a_bank,view.selected);
    CHECK(strcmp(a_bank,full_bank));check_repeat(A_PATH,true);restart_selected();check_repeat(A_PATH,false);
    mount_volume();expect_helper("test A helper");release_volume();shared_records(2u);
    /* Leave the receipt and byte length intact, but corrupt the selected
     * payload. A cached hash or bank ID alone cannot establish an exact repeat. */
    char selected_helper[160];CHECK(kui_update_bank_path(a_bank,"apps/games/flow-helper.bin",selected_helper,sizeof(selected_helper)));
    mount_volume();file_write(selected_helper,"tEst A helper",13u);release_volume();
    input=request(KUI_SHELL_UPDATE_CHECK,A_PATH);before=writes;update_work(&input,&view);
    CHECK(view.status.complete && !view.status.passed && !view.ready && !view.already_installed && view.status.errors && writes==before);
    mount_volume();file_write(selected_helper,"test A helper",13u);release_volume();check_repeat(A_PATH,false);
    review=check_package(B_PATH);view=install_review(&review);CHECK(strcmp(view.selected,a_bank));restart_selected();
    mount_volume();expect_helper("test B helper");release_volume();shared_records(3u);
    /* No space needed for an old verified bank still does not mean selected. */
    review=check_package(A_PATH);CHECK(!review.required_bytes);
    input=request(KUI_SHELL_UPDATE_PREVIOUS,NULL);update_work(&input,&review);view_ok(&review);
    CHECK(review.ready && review.restore && !strcmp(review.reviewed.target,a_bank));view=install_review(&review);
    CHECK(!strcmp(view.selected,a_bank));restart_selected();mount_volume();expect_helper("test A helper");release_volume();shared_records(4u);
    /* Historical GUI2 data must be refused by real CHECK and Restore review
     * under production floor3, before A can enable any activation write. */
    mount_volume();seed_old_bank();struct kui_update_activation active=activation();++active.sequence;
    strcpy(active.previous,OLD_BANK);activation_write(0u,&active);release_volume();
    input=request(KUI_SHELL_UPDATE_CHECK,OLD_PACKAGE);before=writes;update_work(&input,&view);
    CHECK(!view.ready && !view.status.passed && view.status.errors && writes==before);
    CHECK(strstr(view.status.message,"updater") || strstr(view.status.message,"Upgrade"));
    input=request(KUI_SHELL_UPDATE_PREVIOUS,NULL);before=writes;update_work(&input,&view);
    CHECK(!view.ready && !view.status.passed && view.status.errors && writes==before);
    CHECK(strstr(view.status.message,"updater") || strstr(view.status.message,"Upgrade"));
    /* Reinstall a FULL package whose original complete bank is corrupt. The
     * GUI must plan and publish a safe alternate instead of blocking forever. */
    mount_volume();char path[160];CHECK(kui_update_bank_path(full_bank,"apps/games/flow-helper.bin",path,sizeof(path)));
    file_write(path,"corrupt",7u);release_volume();review=check_package(FULL_PATH);view=install_review(&review);
    CHECK(strcmp(view.selected,full_bank));restart_selected();mount_volume();expect_helper("full helper");release_volume();shared_records(5u);
    CHECK(!mounted && !paused && pauses==resumes);kui_update_boot_reset();free(disk);
    printf("PASS actual GUI update flow: %u checks; unknown/BLOCK read-only listing, corrupt-selector LIST/CHECK/FULL install, stale review rejection, BBA download/review/install, actual snapshot IDs, partial A/B, shared real presets/selection, restart/adoption, GUI2 CHECK/Restore admission and corrupt FULL repair\n",checks);return 0;
}
