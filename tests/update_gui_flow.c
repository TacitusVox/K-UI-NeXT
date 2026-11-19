/* SPDX-License-Identifier: GPL-3.0-only */
/* Actual extracted GUI worker + production FatFs/updater/app services.
 * Only exclusive storage/audio/progress boundaries are modeled. */
#include "kui/shell.h"
#include "kui/boot_image.h"
#include "kui/ext4_boot.h"
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
#define KUI_GITHUB_NIGHTLY 1 /* Exercise an official nightly contacting the feed. */
#define UPDATES "0:/KUI/updates"
#define FULL_PATH UPDATES "/full.kup"
#define NIGHTLY_PATH KUI_NIGHTLY_DIRECTORY "/nightly-1111111111111111111111111111111111111111.kup"
#define A_PATH UPDATES "/partial-a.kup"
#define B_PATH UPDATES "/partial-b.kup"
#define OLD_PACKAGE UPDATES "/legacy-gui2.kup"
#define HELPER "0:/KUI/apps/games/flow-helper.bin"
#define OLD_BANK "222222333333"
static uint8_t *disk;static size_t disk_bytes;
static FATFS fs;static bool mounted,paused;
static unsigned checks,writes,update_tokens,generation,pauses,resumes;
static struct kui_shell_update_view update_reviewed;
static struct kui_update_boot_view update_boot_view;
static struct kui_games_selection last_selection;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"GUI updater flow failed %u: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint64_t blocks(void *unused) {(void)unused;return disk_bytes/512u;}
static int read_blocks(void *unused,uint32_t lba,size_t count,uint8_t *out) {
    (void)unused;if((uint64_t)lba+count>blocks(NULL))return -1;
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
static bool lose_selectors_after_install;
static bool lose_latest_after_install;
static void app_progress(const struct kui_app_status *status) {
    CHECK(status && mounted);
    if(lose_latest_after_install && status->passed && strstr(status->message,"Update installed")) {
        lose_latest_after_install=false;
        struct kui_update_activation newest;unsigned slot;
        CHECK(kui_update_activation_latest(&newest,&slot,NULL)==KUI_UPDATE_OK);
        CHECK(f_unlink(slot?KUI_UPDATE_ACTIVE1:KUI_UPDATE_ACTIVE0)==FR_OK);
    }
    if(lose_selectors_after_install && status->passed && strstr(status->message,"Update installed")) {
        lose_selectors_after_install=false;
        CHECK(f_unlink(KUI_UPDATE_ACTIVE0)==FR_OK && f_unlink(KUI_UPDATE_ACTIVE1)==FR_OK);
    }
}
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
    if(f_mount(&fs,"0:",1)!=FR_OK)return KUI_GAMES_STORAGE_NO_MOUNT;
    mounted=true;return KUI_GAMES_STORAGE_OK;
}
bool kui_games_storage_release(void) {CHECK(mounted);mounted=false;return f_mount(NULL,"0:",0)==FR_OK;}

/* The downloader has its own storage and transport tests. Use a KUP on
 * the real mounted image here to exercise the handoff into the GUI worker. */
static const char *download_result;
static bool trim_succeeds=true;
static unsigned trimmed_keep,trim_calls;
bool kui_nightly_trim(unsigned keep,const char *protected_commit,kui_cancel_fn cancel) {
    CHECK(mounted && !protected_commit && cancel==kui_cancelled);
    trimmed_keep=keep;++trim_calls;return trim_succeeds;
}
static unsigned expected_keep=KUI_UPDATE_KEEP_DEFAULT;
static bool expect_running_nightly=true;
bool kui_update_nightly_download(enum kui_internet_route route,const char *running_build,unsigned keep,char path[KUI_UPDATE_BROWSER_PATH_BYTES],
    struct kui_app_status *status,kui_log_fn log,kui_cancel_fn cancel,kui_app_progress_fn progress) {
    CHECK(expect_running_nightly ? running_build && !strcmp(running_build,KUI_BUILD_ID) : !running_build);
    CHECK(keep==expected_keep);
    CHECK(mounted && paused && route==KUI_INTERNET_DREAMPI && log==kui_log && cancel==kui_cancelled && progress==app_progress);
    if(!download_result) {
        strcpy(status->message,"Nightly download interrupted");
        return false;
    }
    strcpy(status->message,*download_result?"Nightly ready. Download cleanup incomplete. Retry the check.":"Latest GUI already running");
    strcpy(path,download_result);return true;
}

#include "actual_gui_update_helpers.inc"

static void mount_volume(void) {CHECK(!mounted && f_mount(&fs,"0:",1)==FR_OK);mounted=true;}
static void check_exfat_storage_pin(void) {
    uint8_t sector[512];
    CHECK(disk_read(0,sector,0,1)==RES_OK);
    if(memcmp(sector+3,"EXFAT   ",8))return;
    unsigned used=sector[112];
    struct kui_shell_update_storage before,after;
    CHECK(update_storage_read(&before));
    FATFS *volume=NULL;DWORD free_clusters=0;
    CHECK(f_getfree("0:",&free_clusters,&volume)==FR_OK && volume && free_clusters);
    FIL file;UINT written=0;
    CHECK(f_open(&file,"0:/review-space.tmp",FA_CREATE_ALWAYS|FA_WRITE)==FR_OK);
    static const uint8_t chunk[16384]={0};
    uint64_t amount=(uint64_t)volume->csize*512u*(volume->n_fatent-2u)/50u;
    for(uint64_t done=0;done<amount;done+=sizeof(chunk))
        CHECK(f_write(&file,chunk,sizeof(chunk),&written)==FR_OK && written==sizeof(chunk));
    CHECK(f_close(&file)==FR_OK && disk_read(0,sector,0,1)==RES_OK && sector[112]!=used);
    CHECK(update_storage_read(&after) && update_storage_same(&before,&after));
    sector[100]^=1u; /* The volume serial must still invalidate confirmation. */
    CHECK(disk_write(0,sector,0,1)==RES_OK);
    CHECK(update_storage_read(&after) && !update_storage_same(&before,&after));
    sector[100]^=1u;
    CHECK(disk_write(0,sector,0,1)==RES_OK && f_unlink("0:/review-space.tmp")==FR_OK);
}
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
    CHECK(view.reviewed.package.flags&KUI_UPDATE_REQUIRES_ENGINE3);return view;
}
static struct kui_shell_update_view install_review(const struct kui_shell_update_view *review) {
    struct kui_shell_update_request input=review->reviewed;input.generation=++generation;
    struct kui_shell_update_view view;update_work(&input,&view);view_ok(&view);
    CHECK(view.restart_required && view.selected[0]);return view;
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
static void check_nightly_review(void) {
    struct kui_shell_update_request input=request(KUI_SHELL_UPDATE_NIGHTLY,NULL);
    input.route=KUI_INTERNET_DREAMPI;
    strcpy(input.directory,KUI_NIGHTLY_DIRECTORY);input.keep=KUI_UPDATE_KEEP_DEFAULT;
    struct kui_shell_update_view view;
    unsigned before=writes;
    struct kui_shell_update_request options=request(KUI_SHELL_UPDATE_OPTIONS,NULL);
    options.change_keep=true;options.keep=1u; /* Opening options is always read-only. */
    update_work(&options,&view);view_ok(&view);
    CHECK(view.keep_known && view.keep==5u && !trim_calls && writes==before);
    download_result=NULL;
    update_work(&input,&view);
    CHECK(!view.ready && !view.status.passed && strstr(view.status.message,"download interrupted"));
    download_result="";
    update_work(&input,&view);view_ok(&view);
    CHECK(!view.ready && view.stage==KUI_SHELL_UPDATE_RESULT && !view.reviewed.token &&
        strstr(view.status.message,"already running"));
    download_result="0:/KUI/runtime.kui";
    update_work(&input,&view);
    CHECK(!view.ready && !view.status.passed && strstr(view.status.message,"path"));
    download_result=NIGHTLY_PATH;
    update_work(&input,&view);view_ok(&view);
    CHECK(view.ready && view.stage==KUI_SHELL_UPDATE_REVIEW && !view.restore && strstr(view.status.message,"cleanup incomplete"));
    CHECK(view.reviewed.op==KUI_SHELL_UPDATE_INSTALL && !strcmp(view.reviewed.path,NIGHTLY_PATH) && view.reviewed.token);
    CHECK(writes==before && view.keep_known && view.keep==KUI_UPDATE_KEEP_DEFAULT);
    /* Save and apply without going online. The next options request remounts
     * the card and loads the saved value before showing it to the user. */
    struct kui_shell_update_request save=request(KUI_SHELL_UPDATE_SAVE_LIMIT,NULL);
    save.keep=3;update_work(&save,&view);view_ok(&view);CHECK(view.keep_known && view.keep==3u);
    CHECK(trim_calls==1 && trimmed_keep==3);
    update_work(&options,&view);view_ok(&view);CHECK(view.keep_known && view.keep==3);
    trim_succeeds=false;update_work(&save,&view);
    CHECK(!view.status.passed && view.keep_known && strstr(view.status.message,"Limit saved"));
    trim_succeeds=true;update_work(&save,&view);view_ok(&view);CHECK(trimmed_keep==3);

    before=writes;save.keep=99u;update_work(&save,&view);
    CHECK(!view.status.passed && !view.keep_known && writes==before);
    expected_keep=3;download_result="";update_work(&input,&view);view_ok(&view);CHECK(writes==before);
    /* Apply an unsaved choice on download, even if the transfer then fails. */
    input.keep=0;input.change_keep=true;expected_keep=0;download_result=NULL;
    update_work(&input,&view);CHECK(!view.status.passed && view.keep_known && view.keep==0);
    update_work(&options,&view);view_ok(&view);CHECK(view.keep_known && view.keep==0);
}
static void expect_helper(const char *expected) {
    FIL file;UINT got=0u;char data[64]={0};CHECK(f_open(&file,HELPER,FA_READ)==FR_OK);
    CHECK(f_read(&file,data,sizeof(data)-1u,&got)==FR_OK && got==strlen(expected) && !memcmp(data,expected,got));
    CHECK(f_close(&file)==FR_OK);
}
/* The two fixtures are FAT32/exFAT. Entering the ext4 path on either is a
 * failure, so the CD loader is linked without the unrelated ext4 library. */
enum kui_ext4_boot_result kui_ext4_boot_read_path(const struct kui_media_ops *raw,
    const char *path,struct kui_runtime_image *out,kui_log_fn log,kui_cancel_fn cancel) {
    (void)raw;(void)path;(void)out;(void)log;(void)cancel;
    CHECK(false);return KUI_EXT4_BOOT_IO;
}

/* Compare the actual RAM payload to independently supplied input bytes.
 * Only the transport marker and a legitimate RAM handoff may differ. */
static struct kui_update_handoff_context check_boot_payload(const struct kui_runtime_image *image,
    const char *host_path,bool bank) {
    FILE *host=fopen(host_path,"rb");CHECK(host && !fseek(host,0,SEEK_END));
    size_t size=(size_t)ftell(host);CHECK(size==KUI_RUNTIME_HEADER_BYTES+image->info.payload_bytes);
    CHECK(!fseek(host,0,SEEK_SET));uint8_t *file=malloc(size);CHECK(file);
    CHECK(fread(file,1u,size,host)==size && !fclose(host));
    uint8_t *payload=file+KUI_RUNTIME_HEADER_BYTES;struct kui_update_handoff_context context;
    CHECK(kui_update_handoff_empty(payload,image->info.payload_bytes,&context.payload_offset));
    memcpy(context.wire,(const uint8_t *)image->data+context.payload_offset,sizeof(context.wire));
    if(bank) {
        struct kui_update_handoff decoded;struct kui_sha256 hash;uint8_t digest[32];
        kui_sha256_init(&hash);kui_sha256_update(&hash,file,size);kui_sha256_digest(&hash,digest);
        CHECK(kui_update_handoff_decode(context.wire,&decoded) && !memcmp(decoded.runtime_sha256,digest,32));
        memcpy(payload+context.payload_offset,context.wire,sizeof(context.wire));
    } else CHECK(kui_update_handoff_empty(context.wire,sizeof(context.wire),NULL));
    CHECK(kui_storage_patch_boot(payload,image->info.payload_bytes,KUI_STORAGE_SCI));
    CHECK(!memcmp(payload,image->data,image->info.payload_bytes));free(file);
    return context;
}

static void cold_boot(enum kui_boot_mode mode,const char *bootstrap,const char *target,
    const char *bank,bool recovery,bool fallback) {
    unsigned before=writes;struct kui_runtime_image cd={0},next={0};struct kui_update_boot_view view;
    /* Power off destroys every software pin and RAM handoff. The original
     * boot disc reads one physical alias and patches only its transport. */
    kui_update_boot_reset();CHECK(!mounted);
    CHECK(kui_boot_image_read_mode(&media,KUI_STORAGE_SCI,mode,&cd,kui_log,kui_cancelled)==KUI_RUNTIME_OK);
    struct kui_update_handoff_context context=check_boot_payload(&cd,bootstrap,false);
    kui_media_set(&media);mount_volume();
    enum kui_update_boot_result result=kui_update_boot_startup(cd.info.build,KUI_STORAGE_SCI,
        &context,recovery,&next,&view,kui_log,kui_cancelled);
    char physical_build[13];memcpy(physical_build,cd.info.build,sizeof(physical_build));
    kui_runtime_free(&cd);
    if(fallback)CHECK(view.fallback);
    if(!bank && !recovery)CHECK(!view.fallback);
    if(recovery || !bank) {
        CHECK(result==KUI_UPDATE_BOOT_BASELINE && !next.data && !view.bank[0]);
        CHECK(!strcmp(view.build,physical_build));
    } else {
        CHECK(result==KUI_UPDATE_BOOT_CHAIN && !strcmp(view.bank,bank));
        context=check_boot_payload(&next,target,true);
        char build[13];memcpy(build,next.info.build,sizeof(build));kui_runtime_free(&next);
        release_volume();kui_update_boot_reset();mount_volume();
        CHECK(kui_update_boot_startup(build,KUI_STORAGE_SCI,&context,false,
            &next,&view,kui_log,kui_cancelled)==KUI_UPDATE_BOOT_PINNED);
        CHECK(!next.data && !strcmp(view.bank,bank));
    }
    update_boot_view=view;CHECK(writes==before);release_volume();
    /* Physical and selected GUIs can share a source build. The Updates
     * screen identifies the active bank independently of its GUI build. */
    struct kui_shell_update_request list=request(KUI_SHELL_UPDATE_LIST,NULL);
    struct kui_shell_update_view displayed;update_work(&list,&displayed);
    CHECK(displayed.running_bank_known && !strcmp(displayed.running_bank,view.bank));
    CHECK(!strcmp(displayed.boot_notice,view.notice));
    if(!recovery && !fallback)CHECK(displayed.selection_known && !strcmp(displayed.selected,view.bank));
}

static void cold_boot_tests(const char *bootstrap,const char *target,const char *bank) {
    kui_update_boot_reset();mount_volume();copy_host(bootstrap,"0:/KUI/boot.kui");
    copy_host(bootstrap,KUI_RUNTIME_PATH);release_volume();
    cold_boot(KUI_BOOT_MODE_AUTOBOOT,bootstrap,target,bank,false,false);
    cold_boot(KUI_BOOT_MODE_NORMAL,bootstrap,target,bank,true,false);
    /* Selecting the physical GUI for one boot leaves the new bank intact. */
    cold_boot(KUI_BOOT_MODE_NORMAL,bootstrap,target,bank,false,false);
    mount_volume();CHECK(!strcmp(activation().selected,bank));
    copy_host(target,KUI_RUNTIME_PATH);release_volume();
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
static void real_artifact_flow(const char *runtime_path,const char *package_path,const char *bootstrap) {
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
    review=check_package(FULL_PATH);view=install_review(&review);
    CHECK(strcmp(view.selected,package->build));char bank[13];strcpy(bank,view.selected);cold_boot_tests(bootstrap,runtime_path,bank);restart_selected();
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
    printf("PASS real artifact GUI flow: %u checks; GUI %.12s, exact %u-file KUP %.12s, both CD entries, same-build physical recovery, read-only cold selection, LIST/CHECK/install, stale review refusal, bank/root restart and restore, corrupt-selector recovery/reinstall, exact module/helper bytes and real shared presets/selection\n",
        checks,KUI_BUILD_ID,package->count,package->build);free(package);
}
int main(int argc,char **argv) {
    bool real=argc==6 && !strcmp(argv[1],"--real");CHECK(real || argc==9);
    FILE *host=fopen(argv[real?2:1],"rb");CHECK(host && !fseek(host,0,SEEK_END));long size=ftell(host);
    CHECK(size>0 && !fseek(host,0,SEEK_SET));disk_bytes=(size_t)size;disk=malloc(disk_bytes);CHECK(disk);
    CHECK(fread(disk,1u,disk_bytes,host)==disk_bytes && !fclose(host));kui_media_set(&media);mount_volume();
    check_exfat_storage_pin();
    const char *bootstrap=argv[real?5:8];
    if(!real) {
        /* Exercise the uninitialized resolver before simulating production
         * main's engine registration. Return the card to its pristine
         * selection state before testing its first physical boot. */
        copy_host(argv[4],FULL_PATH);copy_host(argv[5],A_PATH);copy_host(argv[6],B_PATH);
        corrupt_selection();release_volume();browse_without_selection();mount_volume();
        CHECK(f_unlink(KUI_UPDATE_ACTIVE0)==FR_OK && f_unlink(KUI_UPDATE_ACTIVE1)==FR_OK);
    }
    kui_update_boot_require_engine(KUI_UPDATE_ENGINE_CURRENT);
    copy_host(bootstrap,"0:/KUI/boot.kui");copy_host(bootstrap,KUI_RUNTIME_PATH);release_volume();
    cold_boot(KUI_BOOT_MODE_AUTOBOOT,bootstrap,NULL,NULL,false,false);
    mount_volume();
    if(real) {real_artifact_flow(argv[3],argv[4],argv[5]);kui_update_boot_reset();free(disk);return 0;}
    copy_host(argv[2],KUI_RUNTIME_PATH);copy_host(argv[3],"0:/KUI/old-gui.kui");
    file_write(HELPER,"root helper",11u);corrupt_selection();
    struct kui_game_launch_settings settings;kui_game_launch_settings_default(&settings);settings.reader=KUI_GAME_READER_BACKGROUND;
    CHECK(kui_game_launch_settings_save("/Games/Original/disc.gdi",&settings,kui_log));
    kui_games_selection_default(&last_selection);strcpy(last_selection.name,"Original");CHECK(kui_games_selection_save(&last_selection,kui_log));
    struct kui_runtime_image image={0};unsigned before=writes;
    CHECK(kui_update_boot_read(KUI_BUILD_ID,KUI_STORAGE_SCI,&image,&update_boot_view,kui_log,kui_cancelled)==KUI_UPDATE_BOOT_BASELINE);
    CHECK(!image.data && writes==before);release_volume();
    struct kui_shell_update_request input=request(KUI_SHELL_UPDATE_LIST,NULL);struct kui_shell_update_view view;
    before=writes;update_work(&input,&view);view_ok(&view);
    CHECK(view.stage==KUI_SHELL_UPDATE_PACKAGES && view.total==3u && writes==before);
    kui_update_boot_block();browse_without_selection();mount_volume();before=writes;
    CHECK(kui_update_boot_read(KUI_BUILD_ID,KUI_STORAGE_SCI,&image,&update_boot_view,kui_log,kui_cancelled)==KUI_UPDATE_BOOT_BASELINE);
    CHECK(!image.data && writes==before);release_volume();
    struct kui_shell_update_view review=check_package(FULL_PATH);CHECK(writes==before);
    mount_volume();copy_host(argv[4],NIGHTLY_PATH);release_volume();
    check_nightly_review();
    review=check_package(FULL_PATH);
    /* Change the real saved selection between review and confirmation. */
    mount_volume();struct kui_update_activation changed={.sequence=17u};activation_write(0u,&changed);release_volume();
    input=review.reviewed;input.generation=++generation;before=writes;update_work(&input,&view);
    CHECK(view.status.complete && !view.status.passed && !view.ready && strstr(view.status.message,"review changed") && writes==before);
    mount_volume();corrupt_selection();release_volume();
    input=request(KUI_SHELL_UPDATE_NIGHTLY,NULL);strcpy(input.directory,KUI_NIGHTLY_DIRECTORY);
    input.route=KUI_INTERNET_DREAMPI;input.keep=0;download_result=NIGHTLY_PATH;
    update_work(&input,&review);view_ok(&review);
    CHECK(review.ready && !strcmp(review.reviewed.path,NIGHTLY_PATH) && !strcmp(review.reviewed.directory,KUI_NIGHTLY_DIRECTORY));
    view=install_review(&review);
    CHECK(strcmp(view.selected,review.reviewed.package.build));char full_bank[13];strcpy(full_bank,view.selected);
    /* The official GUI in memory is still ROOT. A version check must be able
     * to offer the feed again while a different bank is selected for boot. */
    expect_running_nightly=false;
    update_work(&input,&review);view_ok(&review);
    CHECK(review.restart_required && review.ready && !strcmp(review.reviewed.path,NIGHTLY_PATH));
    expect_running_nightly=true;
    mount_volume();CHECK(!strcmp(activation().selected,view.selected));release_volume();cold_boot_tests(argv[8],argv[2],full_bank);restart_selected();
    mount_volume();expect_helper("full helper");release_volume();shared_records(1u);
    review=check_package(A_PATH);view=install_review(&review);char a_bank[13];strcpy(a_bank,view.selected);
    CHECK(strcmp(a_bank,full_bank));restart_selected();mount_volume();expect_helper("test A helper");release_volume();shared_records(2u);
    /* Damage only the newest bank after it was installed. A fresh boot must
     * validate and load the older bank's bytes without changing the record. */
    char newest_gui[KUI_UPDATE_BOOT_PATH_BYTES];CHECK(kui_update_bank_path(a_bank,"runtime.kui",newest_gui,sizeof(newest_gui)));
    mount_volume();file_write(newest_gui,"bad",3);release_volume();
    cold_boot(KUI_BOOT_MODE_AUTOBOOT,bootstrap,argv[2],full_bank,false,true);
    mount_volume();CHECK(!strcmp(activation().selected,a_bank));copy_host(argv[2],newest_gui);release_volume();
    restart_selected();
    /* Lose only the new selector after the installer finishes. A still-valid
     * older bank must not turn the GUI's final readback into a success. */
    review=check_package(B_PATH);input=review.reviewed;lose_latest_after_install=true;
    update_work(&input,&view);
    CHECK(!lose_latest_after_install && !view.status.passed && view.status.errors &&
        !view.selection_known && strstr(view.status.message,"Activation could not be confirmed"));
    mount_volume();CHECK(!strcmp(activation().selected,a_bank));release_volume();
    review=check_package(B_PATH);view=install_review(&review);CHECK(strcmp(view.selected,a_bank));restart_selected();
    mount_volume();expect_helper("test B helper");release_volume();shared_records(3u);
    cold_boot(KUI_BOOT_MODE_AUTOBOOT,bootstrap,NULL,view.selected,true,false);
    input=request(KUI_SHELL_UPDATE_PREVIOUS,NULL);update_work(&input,&review);view_ok(&review);
    CHECK(review.ready && review.restore && !strcmp(review.reviewed.target,a_bank));view=install_review(&review);
    CHECK(!strcmp(view.selected,a_bank));cold_boot(KUI_BOOT_MODE_AUTOBOOT,bootstrap,argv[2],a_bank,false,false);
    restart_selected();mount_volume();expect_helper("test A helper");release_volume();shared_records(4u);
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
    /* A confirmed ZIP installs the exact KUP shown in review, with one
     * extraction. Reject a substituted or missing cache before any writes. */
    const char *zip_path=UPDATES "/full.zip";
    mount_volume();copy_host(argv[7],zip_path);release_volume();
    review=check_package(zip_path);
    mount_volume();copy_host(argv[5],KUI_UPDATE_ZIP_STAGE_PATH);release_volume();
    input=review.reviewed;before=writes;update_work(&input,&view);
    CHECK(!view.status.passed && view.status.errors && writes==before);
    review=check_package(zip_path);
    mount_volume();CHECK(f_unlink(KUI_UPDATE_ZIP_STAGE_PATH)==FR_OK);release_volume();
    input=review.reviewed;before=writes;update_work(&input,&view);
    CHECK(!view.status.passed && view.status.errors && writes==before);
    review=check_package(zip_path);
    mount_volume();CHECK(f_unlink(zip_path)==FR_OK);release_volume();
    view=install_review(&review);restart_selected();
    mount_volume();expect_helper("full helper");release_volume();
    review=check_package(A_PATH);input=review.reviewed;lose_selectors_after_install=true;
    update_work(&input,&view);
    CHECK(!lose_selectors_after_install && !view.status.passed && !view.selection_known &&
        strstr(view.status.message,"Activation could not be confirmed"));
    CHECK(!mounted && !paused && pauses==resumes);kui_update_boot_reset();free(disk);
    printf("PASS actual GUI update flow: %u checks; mounted download preference, offline trim retry, nested nightly review/install, unknown/BLOCK read-only listing, corrupt-selector LIST/CHECK/FULL install, stale review rejection, actual snapshot IDs, partial A/B, shared real presets/selection, restart/adoption, GUI2 CHECK/Restore admission, corrupt FULL repair and ZIP review cache verification\n",checks);return 0;
}
