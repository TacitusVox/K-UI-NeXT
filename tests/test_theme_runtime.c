/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/theme_runtime.h"
#include "kui/shell.h"
#include "kui/storage.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Model only the mounted storage boundary. The production bundle coordinator,
 * settings selectors and palette resolution run unchanged. Every allocated
 * image has an independent owner, so partial failures must release candidates
 * without touching the currently displayed bundle. */
static const char *const ids[]={"default","moon","ember","mono","broken",KUI_THEME_FACTORY_ID};
enum { THEME_FIXTURES=sizeof(ids)/sizeof(ids[0]) };
static unsigned definition_reads[THEME_FIXTURES],image_reads[THEME_FIXTURES][KUI_THEME_ASSET_COUNT];
static unsigned live_images,allocated_images,released_images;
static unsigned cancel_at,cancel_calls;
static int failed_kind=-1;
static const char *failed_theme;
static bool fail_allocation,empty_splash;

static unsigned index_of(const char *id) {
    for(unsigned i=0;i<THEME_FIXTURES;i++)
        if(!strcmp(id,ids[i]))return i;
    return 99;
}
bool kui_theme_definition_load(struct kui_theme_definition *out,const char *id,kui_log_fn log) {
    (void)log;
    unsigned index=index_of(id);
    if(index>=THEME_FIXTURES)return false;
    ++definition_reads[index];
    if(index==4)return false;
    kui_theme_definition_default(out);
    snprintf(out->id,sizeof(out->id),"%s",id);
    if(index) {
        out->palette.colors[KUI_THEME_ACCENT]=0x102030u*index;
        for(unsigned kind=0;kind<KUI_THEME_ASSET_COUNT;kind++)
            snprintf(out->assets[kind],sizeof(out->assets[kind]),"asset%u.kuiimg",kind);
    }
    return true;
}
enum kui_theme_asset_status kui_theme_image_load(struct kui_theme_image *out,
    const struct kui_theme_definition *definition,enum kui_theme_asset_kind kind,
    kui_log_fn log,kui_cancel_fn cancelled) {
    (void)log;(void)cancelled;
    assert(out && !out->pixels && !out->width && !out->height);
    unsigned index=index_of(definition->id);assert(index<THEME_FIXTURES);
    ++image_reads[index][kind];
    if(!index || (empty_splash && kind==KUI_THEME_SPLASH))return KUI_THEME_ASSET_DEFAULT;
    if(fail_allocation || ((int)kind==failed_kind && failed_theme &&
       !strcmp(failed_theme,definition->id)))return KUI_THEME_ASSET_FAILED;
    /* The coordinator never dereferences an image. A unique one-pixel marker
     * makes ownership visible while keeping cancellation tests inexpensive. */
    out->pixels=malloc(sizeof(*out->pixels));assert(out->pixels);
    *out->pixels=(uint16_t)(index*100u+kind);
    out->width=KUI_THEME_WIDTH;
    out->height=kind==KUI_THEME_HEADER?KUI_THEME_HEADER_HEIGHT:KUI_THEME_HEIGHT;
    ++live_images;++allocated_images;
    return KUI_THEME_ASSET_READY;
}
void kui_theme_image_release(struct kui_theme_image *image) {
    if(!image)return;
    if(image->pixels) {
        assert(live_images);--live_images;++released_images;free(image->pixels);
    }
    memset(image,0,sizeof(*image));
}
static bool cancelled(void) {return cancel_at && ++cancel_calls>=cancel_at;}
static void reset_storage(void) {
    assert(!live_images);
    memset(definition_reads,0,sizeof(definition_reads));
    memset(image_reads,0,sizeof(image_reads));
    allocated_images=released_images=cancel_at=cancel_calls=0;
    failed_kind=-1;failed_theme=NULL;fail_allocation=false;
}
static void check_empty(const struct kui_theme_bundle *bundle) {
    for(unsigned k=0;k<KUI_THEME_ASSET_COUNT;k++) {
        assert(!bundle->images[k].pixels);
        assert(!bundle->images[k].width && !bundle->images[k].height);
        assert(bundle->status[k]==KUI_THEME_ASSET_DEFAULT);
    }
}
static void test_default(void) {
    reset_storage();
    struct kui_theme_settings settings;kui_theme_settings_default(&settings);
    struct kui_theme_bundle bundle;kui_theme_bundle_default(&bundle);
    assert(!strcmp(settings.theme_id,KUI_THEME_FACTORY_ID));
    assert(kui_theme_bundle_prepare(&bundle,&settings,KUI_THEME_LOAD_ALL,true,NULL,NULL));
    assert(live_images==3 && !bundle.fallback_count && definition_reads[index_of(KUI_THEME_FACTORY_ID)]==1);
    kui_theme_bundle_release(&bundle);check_empty(&bundle);
    strcpy(settings.theme_id,"default");
    assert(kui_theme_bundle_prepare(&bundle,&settings,KUI_THEME_LOAD_ALL,true,NULL,NULL));
    check_empty(&bundle);assert(!bundle.fallback_count && !live_images);
    assert(definition_reads[0]==1);
    for(unsigned k=0;k<KUI_THEME_ASSET_COUNT;k++)assert(image_reads[0][k]==1);
    struct kui_theme_palette defaults;kui_theme_palette_default(&defaults);
    assert(!memcmp(&bundle.palette,&defaults,sizeof(defaults)));
    kui_theme_bundle_release(&bundle);check_empty(&bundle);
}
static void test_mixed_components_and_palette(void) {
    reset_storage();
    struct kui_theme_settings settings;kui_theme_settings_default(&settings);
    strcpy(settings.theme_id,"moon");strcpy(settings.splash_id,"ember");
    strcpy(settings.header_id,"mono");strcpy(settings.background_id,"none");
    settings.override_mask=1u<<KUI_THEME_TEXT;settings.palette.colors[KUI_THEME_TEXT]=0xabcdef;
    struct kui_theme_bundle bundle;kui_theme_bundle_default(&bundle);
    assert(kui_theme_bundle_prepare(&bundle,&settings,KUI_THEME_LOAD_ALL,true,NULL,NULL));
    assert(live_images==2 && allocated_images==2);
    assert(*bundle.images[KUI_THEME_SPLASH].pixels==200);
    assert(*bundle.images[KUI_THEME_HEADER].pixels==301);
    assert(bundle.status[KUI_THEME_BACKDROP]==KUI_THEME_ASSET_DISABLED);
    assert(!bundle.images[KUI_THEME_BACKDROP].pixels);
    assert(bundle.palette.colors[KUI_THEME_ACCENT]==0x102030);
    assert(bundle.palette.colors[KUI_THEME_TEXT]==0xabcdef);
    assert(definition_reads[0]==0 && definition_reads[1]==1 &&
        definition_reads[2]==1 && definition_reads[3]==1);
    kui_theme_bundle_release_asset(&bundle,KUI_THEME_SPLASH);
    assert(live_images==1 && !bundle.images[KUI_THEME_SPLASH].pixels);
    assert(bundle.images[KUI_THEME_HEADER].pixels);
    kui_theme_bundle_release(&bundle);check_empty(&bundle);
    assert(!live_images && allocated_images==released_images);
    kui_theme_bundle_release(&bundle);assert(allocated_images==released_images);
}
static void test_definition_cache_and_mask(void) {
    reset_storage();
    struct kui_theme_settings settings;kui_theme_settings_default(&settings);
    strcpy(settings.theme_id,"moon");strcpy(settings.header_id,"moon");
    strcpy(settings.background_id,"moon");strcpy(settings.splash_id,"broken");
    struct kui_theme_bundle bundle;kui_theme_bundle_default(&bundle);
    assert(kui_theme_bundle_prepare(&bundle,&settings,KUI_THEME_LOAD_MENU,true,NULL,NULL));
    assert(definition_reads[1]==1 && definition_reads[4]==0);
    assert(!bundle.images[KUI_THEME_SPLASH].pixels && image_reads[1][KUI_THEME_SPLASH]==0);
    assert(live_images==2);
    kui_theme_bundle_release(&bundle);assert(!live_images);
    assert(!kui_theme_bundle_prepare(&bundle,&settings,1u<<KUI_THEME_ASSET_COUNT,true,NULL,NULL));
    check_empty(&bundle);
}
static void test_strict_failure_keeps_displayed_owner(void) {
    reset_storage();
    struct kui_theme_settings saved,draft;kui_theme_settings_default(&saved);
    strcpy(saved.theme_id,"moon");
    struct kui_theme_bundle displayed,candidate;
    kui_theme_bundle_default(&displayed);kui_theme_bundle_default(&candidate);
    assert(kui_theme_bundle_prepare(&displayed,&saved,KUI_THEME_LOAD_ALL,true,NULL,NULL));
    struct kui_theme_bundle before=displayed; /* Compare only: not a second owner. */
    draft=saved;strcpy(draft.theme_id,"ember");
    failed_theme="ember";failed_kind=KUI_THEME_BACKDROP;
    assert(!kui_theme_bundle_prepare(&candidate,&draft,KUI_THEME_LOAD_ALL,true,NULL,NULL));
    check_empty(&candidate);assert(live_images==3 && released_images==2);
    assert(!memcmp(&displayed,&before,sizeof(before)));
    for(unsigned k=0;k<KUI_THEME_ASSET_COUNT;k++)assert(*displayed.images[k].pixels==100u+k);
    kui_theme_bundle_release(&displayed);assert(!live_images && allocated_images==released_images);
}
static void test_tolerant_fallbacks(void) {
    reset_storage();
    struct kui_theme_settings settings;kui_theme_settings_default(&settings);
    strcpy(settings.theme_id,"broken");strcpy(settings.header_id,"moon");
    strcpy(settings.splash_id,"none");
    settings.override_mask=1u<<KUI_THEME_ACCENT;settings.palette.colors[KUI_THEME_ACCENT]=0x123456;
    struct kui_theme_bundle bundle;kui_theme_bundle_default(&bundle);
    assert(kui_theme_bundle_prepare(&bundle,&settings,KUI_THEME_LOAD_ALL,false,NULL,NULL));
    assert(!strcmp(bundle.settings.theme_id,"broken"));
    assert(bundle.fallback_count==2 && definition_reads[4]==1);
    assert(bundle.palette.colors[KUI_THEME_ACCENT]==0x123456);
    assert(bundle.status[KUI_THEME_SPLASH]==KUI_THEME_ASSET_DISABLED);
    assert(bundle.status[KUI_THEME_HEADER]==KUI_THEME_ASSET_READY);
    assert(bundle.status[KUI_THEME_BACKDROP]==KUI_THEME_ASSET_DEFAULT);
    kui_theme_bundle_release(&bundle);assert(!live_images);
    reset_storage();strcpy(settings.theme_id,"moon");settings.splash_id[0]=0;
    failed_theme="moon";failed_kind=KUI_THEME_HEADER;
    assert(kui_theme_bundle_prepare(&bundle,&settings,KUI_THEME_LOAD_ALL,false,NULL,NULL));
    assert(bundle.fallback_count==1 && live_images==2);
    assert(bundle.status[KUI_THEME_HEADER]==KUI_THEME_ASSET_DEFAULT);
    assert(!bundle.images[KUI_THEME_HEADER].pixels);
    kui_theme_bundle_release(&bundle);assert(!live_images);
}
static void test_allocation_failure_and_cancellation(void) {
    struct kui_theme_settings settings;kui_theme_settings_default(&settings);
    strcpy(settings.theme_id,"moon");
    struct kui_theme_bundle bundle;
    reset_storage();kui_theme_bundle_default(&bundle);fail_allocation=true;
    assert(!kui_theme_bundle_prepare(&bundle,&settings,KUI_THEME_LOAD_ALL,true,NULL,NULL));
    check_empty(&bundle);assert(!live_images);
    /* Before base read, each component, and after all reads: every possible
     * coordinator cancellation point must relinquish its partial candidate. */
    for(unsigned at=1;at<=5;at++) {
        reset_storage();kui_theme_bundle_default(&bundle);cancel_at=at;
        assert(!kui_theme_bundle_prepare(&bundle,&settings,KUI_THEME_LOAD_ALL,true,NULL,cancelled));
        check_empty(&bundle);assert(!live_images && allocated_images==released_images);
    }
    reset_storage();kui_theme_bundle_default(&bundle);cancel_at=6;
    assert(kui_theme_bundle_prepare(&bundle,&settings,KUI_THEME_LOAD_ALL,true,NULL,cancelled));
    assert(live_images==3);kui_theme_bundle_release(&bundle);
    assert(!live_images && allocated_images==released_images);
}

/* Run the exact runtime publication/worker adapter too. Only platform I/O,
 * time, mutexes and reducer callbacks are recording stubs; the production
 * .inc is compiled here unchanged. This catches an automatic menu reload
 * discarding a newer preview even when the bundle preparation itself is sound. */
static int lock;
static bool locked,splash_active,shell_frame_ready;
static struct kui_shell shell;
static bool storage_ok=true,settings_read_ok=true,settings_write_ok=true;
static struct kui_theme_settings stored_settings;
static unsigned preferences_calls,result_calls,catalog_calls,saves,first_frames;
static void draw_shell(void) {assert(!locked && preferences_calls==1 && !first_frames);++first_frames;}
static enum kui_shell_action reported_action;
static bool reported_ok;
static void mutex_lock(int *value) {assert(value==&lock && !locked);locked=true;}
static void mutex_unlock(int *value) {assert(value==&lock && locked);locked=false;}
static void kui_log(const char *format,...) {(void)format;}
static bool kui_cancelled(void) {return cancelled();}
static uint64_t timer_ms_gettime64(void) {return 100u;}
static void kui_sd_set_params(unsigned target,bool checked) {assert(target==KUI_STORAGE_AUTO && checked);}
static bool kui_sd_connect(void) {return storage_ok;}
static void kui_sd_disconnect(void) {}
bool kui_mount(FATFS *fs,kui_log_fn log) {(void)log;assert(fs);return storage_ok;}
FRESULT f_mount(FATFS *fs,const TCHAR *path,BYTE immediate) {
    assert(!fs && !strcmp(path,"0:") && !immediate);return FR_OK;
}
bool kui_theme_settings_load(struct kui_theme_settings *out,kui_log_fn log) {
    (void)log;if(settings_read_ok)*out=stored_settings;return settings_read_ok;
}
bool kui_theme_settings_save(const struct kui_theme_settings *settings,kui_log_fn log) {
    (void)log;++saves;if(settings_write_ok)stored_settings=*settings;return settings_write_ok;
}
bool kui_theme_catalog_scan(struct kui_theme_catalog *out,kui_log_fn log,kui_cancel_fn cancel) {
    (void)log;if(cancel && cancel())return false;
    memset(out,0,sizeof(*out));kui_theme_definition_default(&out->themes[0]);out->count=1;
    return true;
}
static uint16_t *kui_splash_preview_decode(void) {
    if(fail_allocation)return NULL;
    uint16_t *pixels=malloc(sizeof(*pixels));assert(pixels);*pixels=999u;
    ++live_images;++allocated_images;return pixels;
}
static uint16_t *kui_splash_preview_factory_decode(void) {
    uint16_t *pixels=kui_splash_preview_decode();
    if(pixels)*pixels=777u;
    return pixels;
}
void kui_shell_set_theme_preferences(struct kui_shell *s,const struct kui_theme_settings *settings) {
    assert(!locked && s==&shell);s->theme_saved=*settings;++preferences_calls;
}
void kui_shell_set_theme_catalog(struct kui_shell *s,unsigned generation,
        const struct kui_theme_catalog *catalog,bool ok,const char *notice) {
    assert(locked && s==&shell && generation==shell.theme_generation && catalog && notice);
    ++catalog_calls;reported_ok=ok;
}
void kui_shell_theme_result(struct kui_shell *s,unsigned generation,
        enum kui_shell_action action,bool ok,const char *notice) {
    assert(locked && s==&shell && generation==shell.theme_generation && notice);
    ++result_calls;reported_action=action;reported_ok=ok;
}
#include "../src/dreamcast/theme_runtime.inc"

static void publication_reset(void) {
    kui_theme_bundle_release(&theme_saved_bundle);
    kui_theme_bundle_release(&theme_preview_bundle);
    kui_theme_bundle_release(&theme_result_bundle);
    reset_storage();theme_init();memset(&shell,0,sizeof(shell));
    theme_request_pending=theme_result_request=theme_result_action=0;
    theme_result_ready=theme_result_ok=theme_result_has_bundle=false;
    theme_preview_showing=theme_restore_needed=false;
    locked=splash_active=false;shell_frame_ready=true;
    storage_ok=settings_read_ok=settings_write_ok=true;
    preferences_calls=result_calls=catalog_calls=saves=first_frames=0;reported_ok=false;
    kui_theme_settings_default(&stored_settings);
}
static void accept(void) {
    mutex_lock(&lock);assert(theme_accept_result());mutex_unlock(&lock);
    assert(!theme_result_ready && !theme_result_has_bundle && !shell_frame_ready);
}
static void request(unsigned action,const char *id) {
    kui_theme_settings_default(&theme_pending);
    snprintf(theme_pending.theme_id,sizeof(theme_pending.theme_id),"%s",id);
    theme_request_pending=++shell.theme_generation;
    theme_operation(action);assert(theme_result_ready);accept();
}
static void test_publication_preview_restore_apply_cancel(void) {
    publication_reset();
    request(THEME_APPLY_WORK,"moon");
    assert(saves==1 && live_images==2 && reported_action==KUI_SHELL_THEME_APPLY && reported_ok);
    assert(!theme_preview_showing && !strcmp(theme_saved_bundle.settings.theme_id,"moon"));
    assert(theme_visible_bundle()==&theme_saved_bundle);
    theme_release_for_action(19u);
    assert(theme_restore_needed && !live_images);
    assert(!strcmp(theme_saved_bundle.settings.theme_id,"moon"));
    request(THEME_PREVIEW_WORK,"ember");
    assert(theme_preview_showing && live_images==3 && saves==1);
    uint16_t *preview=theme_preview_bundle.images[KUI_THEME_HEADER].pixels;
    unsigned callbacks=result_calls;
    theme_restore_needed=false; /* main's queue owns clearing this flag */
    request(THEME_RESTORE_WORK,"moon");
    assert(theme_preview_showing && live_images==5 && result_calls==callbacks);
    assert(theme_preview_bundle.images[KUI_THEME_HEADER].pixels==preview);
    assert(theme_visible_bundle()==&theme_preview_bundle);
    storage_ok=false; /* Cancel must restore without needing the SD card. */
    request(THEME_CANCEL_WORK,"default");
    assert(!theme_preview_showing && live_images==2 && reported_ok);
    assert(theme_visible_bundle()==&theme_saved_bundle);
    assert(!strcmp(theme_saved_bundle.settings.theme_id,"moon"));
    storage_ok=true;theme_restore_needed=true;
    request(THEME_APPLY_WORK,"mono");
    assert(!theme_restore_needed && live_images==2 && saves==2);
    assert(!strcmp(theme_saved_bundle.settings.theme_id,"mono"));
    publication_reset();assert(!live_images);
}
static void test_publication_failures_and_stale_result(void) {
    publication_reset();request(THEME_APPLY_WORK,"moon");
    uint16_t *saved=theme_saved_bundle.images[KUI_THEME_HEADER].pixels;
    settings_write_ok=false;request(THEME_APPLY_WORK,"ember");
    assert(!reported_ok && live_images==2 && theme_saved_bundle.images[KUI_THEME_HEADER].pixels==saved);
    assert(!strcmp(stored_settings.theme_id,"moon"));
    settings_write_ok=true;
    kui_theme_settings_default(&theme_pending);strcpy(theme_pending.theme_id,"ember");
    theme_request_pending=++shell.theme_generation;theme_operation(THEME_PREVIEW_WORK);
    assert(theme_result_ready && theme_result_has_bundle && live_images==5);
    unsigned callbacks=result_calls;++shell.theme_generation;accept();
    assert(!theme_preview_showing && live_images==2 && result_calls==callbacks);
    assert(theme_saved_bundle.images[KUI_THEME_HEADER].pixels==saved);
    storage_ok=false;request(THEME_SCAN_WORK,"default");
    assert(!reported_ok && catalog_calls==1 && live_images==2);
    publication_reset();assert(!live_images);
}
static void test_startup_and_builtin_preview(void) {
    publication_reset();strcpy(stored_settings.theme_id,"moon");
    splash_active=true;theme_first_frame(true);
    uint16_t *first=theme_saved_bundle.images[KUI_THEME_SPLASH].pixels;
    assert(first_frames==1 && first && live_images==1);
    assert(image_reads[1][KUI_THEME_SPLASH]==1 && !image_reads[1][KUI_THEME_HEADER]);
    theme_startup_load();accept();
    assert(first==theme_saved_bundle.images[KUI_THEME_SPLASH].pixels && image_reads[1][KUI_THEME_SPLASH]==1);
    assert(preferences_calls==1 && live_images==3 && !theme_preview_showing);
    assert(theme_saved_bundle.images[KUI_THEME_SPLASH].pixels);
    assert(!strcmp(shell.theme_saved.theme_id,"moon"));
    /* Theme jobs keep displayed images until publication. Download and launch jobs free them before dispatch. */
    theme_release_for_action(THEME_SCAN_WORK);assert(live_images==3);
    const unsigned heavy[]={19u,22u,25u,44u,56u,57u,58u,62u,79u,104u,121u,122u,126u,KUI_GAMES_MEDIA_DOWNLOAD_JOB};
    for(unsigned i=0;i<sizeof(heavy)/sizeof(heavy[0]);i++) {
        if(i) {theme_startup_load();accept();}
        theme_release_for_action(heavy[i]);assert(!live_images && theme_restore_needed);
    }
    publication_reset();request(THEME_PREVIEW_WORK,"default");
    assert(reported_ok && theme_preview_showing && live_images==1);
    assert(*theme_preview_bundle.images[KUI_THEME_SPLASH].pixels==999u);
    request(THEME_CANCEL_WORK,"default");assert(!live_images && !theme_preview_showing);
    empty_splash=true;request(THEME_PREVIEW_WORK,"ember");empty_splash=false;
    assert(reported_ok && *theme_preview_bundle.images[KUI_THEME_SPLASH].pixels==777u);
    request(THEME_CANCEL_WORK,"default");assert(!live_images);
    fail_allocation=true;request(THEME_PREVIEW_WORK,"default");
    assert(!reported_ok && !live_images && !theme_preview_showing);
    publication_reset();settings_read_ok=false;splash_active=true;
    theme_first_frame(true);theme_startup_load();accept();
    assert(preferences_calls==1 && live_images==3 && !strcmp(shell.theme_saved.theme_id,KUI_THEME_FACTORY_ID));
    publication_reset();theme_first_frame(false);
    assert(first_frames==1 && !live_images && !strcmp(shell.theme_saved.theme_id,KUI_THEME_FACTORY_ID));
    publication_reset();assert(!live_images);
}
int main(void) {
    test_default();test_mixed_components_and_palette();test_definition_cache_and_mask();
    test_strict_failure_keeps_displayed_owner();test_tolerant_fallbacks();
    test_allocation_failure_and_cancellation();
    test_publication_preview_restore_apply_cancel();test_publication_failures_and_stale_result();
    test_startup_and_builtin_preview();
    puts("theme runtime: bundles, exact worker publication, preview/restore race, rollback and memory release passed");
    return 0;
}
