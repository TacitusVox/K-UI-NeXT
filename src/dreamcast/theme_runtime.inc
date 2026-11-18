/* SPDX-License-Identifier: GPL-3.0-only */
/* Included only by the SD runtime. Storage worker owns the unpublished result;
 * main owns every displayed bundle and all frees of displayed image memory. */
#include "kui/media_theme_jobs.h"
static struct kui_theme_bundle theme_saved_bundle,theme_preview_bundle,theme_result_bundle;
static struct kui_theme_settings theme_pending,theme_startup_settings;
static struct kui_theme_catalog theme_catalog,theme_catalog_result;
static unsigned theme_request_pending,theme_result_request,theme_result_action;
static bool theme_result_ready,theme_result_ok,theme_result_has_bundle;
static bool theme_preview_showing,theme_restore_needed;
static char theme_result_note[128];

static struct kui_theme_bundle *theme_visible_bundle(void) {
    return theme_preview_showing?&theme_preview_bundle:&theme_saved_bundle;
}
static void theme_init(void) {
    kui_theme_bundle_default(&theme_saved_bundle);
    kui_theme_bundle_default(&theme_preview_bundle);
    kui_theme_bundle_default(&theme_result_bundle);
    kui_theme_definition_default(&theme_catalog.themes[0]);theme_catalog.count=1;
}
static void theme_publish_result(unsigned action,unsigned request,bool ok,bool bundle,const char *note) {
    mutex_lock(&lock);
    theme_result_action=action;theme_result_request=request;
    theme_result_ok=ok;theme_result_has_bundle=bundle;
    snprintf(theme_result_note,sizeof(theme_result_note),"%s",note);
    theme_result_ready=true;
    mutex_unlock(&lock);
}
/* Main is the only SD owner until the first splash is on screen. Keep its
 * settings snapshot for the worker; opening Appearance later cannot change it. */
static void theme_first_frame(bool mounted) {
    if(mounted) kui_theme_startup_splash_prepare(&theme_saved_bundle,kui_log);
    theme_startup_settings=theme_saved_bundle.settings;
    kui_shell_set_theme_preferences(&shell,&theme_startup_settings);
    draw_shell();
}
static void theme_startup_load(void) {
    bool prepared=kui_theme_bundle_prepare(&theme_result_bundle,&theme_startup_settings,
        KUI_THEME_LOAD_MENU,false,kui_log,kui_cancelled);
    theme_publish_result(0u,0u,prepared,prepared,prepared?
        "Saved menu appearance loaded.":"Menu appearance unavailable; built-in fallback used.");
}
static void theme_operation(unsigned action) {
    unsigned request;
    struct kui_theme_settings settings;
    mutex_lock(&lock);settings=theme_pending;request=theme_request_pending;mutex_unlock(&lock);
    if(action==THEME_CANCEL_WORK) {
        theme_publish_result(action,request,true,false,"Preview discarded; saved appearance restored.");
        return;
    }
    bool ok=false,prepared=false;
    const char *note="Theme storage unavailable; current appearance retained.";
    uint64_t began=timer_ms_gettime64();
    kui_sd_set_params(KUI_STORAGE_AUTO,true);
    if(kui_sd_connect()) {
        FATFS fs;
        if(kui_mount(&fs,kui_log)) {
            if(action==THEME_SCAN_WORK) {
                ok=kui_theme_catalog_scan(&theme_catalog_result,kui_log,kui_cancelled);
                note=ok?(theme_catalog_result.truncated?"Theme list full; first 24 themes shown.":
                    "Choose a theme or mix its images and colors."):"Theme list unavailable; try Refresh.";
            } else {
                unsigned mask=action==THEME_RESTORE_WORK?KUI_THEME_LOAD_MENU:KUI_THEME_LOAD_ALL;
                prepared=kui_theme_bundle_prepare(&theme_result_bundle,&settings,mask,
                    action!=THEME_RESTORE_WORK,kui_log,kui_cancelled);
                if(prepared && action==THEME_PREVIEW_WORK &&
                   theme_result_bundle.status[KUI_THEME_SPLASH]==KUI_THEME_ASSET_DEFAULT) {
                    struct kui_theme_image *splash=&theme_result_bundle.images[KUI_THEME_SPLASH];
                    const char *id=kui_theme_component_id(&settings,KUI_THEME_SPLASH);
                    splash->pixels=!strcmp(id,"default")?kui_splash_preview_decode():
                        kui_splash_preview_factory_decode();
                    splash->width=640u;splash->height=480u;
                    if(!splash->pixels) {kui_theme_bundle_release(&theme_result_bundle);prepared=false;}
                }
                ok=prepared;
                if(prepared && action==THEME_APPLY_WORK) {
                    /* Validate everything before committing. Once the durable
                     * write starts, finish its readback even if B is pressed. */
                    ok=!kui_cancelled() && kui_theme_settings_save(&settings,kui_log);
                    note=ok?"Appearance saved. Splash selection applies on startup.":
                        "Appearance not confirmed saved; previous appearance retained.";
                } else note=prepared?(action==THEME_RESTORE_WORK?
                    "Saved appearance restored.":"Preview only. Apply to save, B to discard."):
                    "Selected theme/image is missing, damaged or stopped; current appearance retained.";
            }
            f_mount(NULL,"0:",0);
        }
        kui_sd_disconnect();
    }
    if(!ok && prepared) {kui_theme_bundle_release(&theme_result_bundle);prepared=false;}
    kui_log("Theme operation %u: %s, %llu ms",action,ok?"complete":"failed/stopped",
        (unsigned long long)(timer_ms_gettime64()-began));
    theme_publish_result(action,request,ok,prepared,note);
}
/* Main, lock held, between frames. Successful publication transfers ownership
 * rather than copying pixels or letting a worker free what a frame is reading. */
static bool theme_accept_result(void) {
    if(!theme_result_ready) return false;
    unsigned action=theme_result_action;
    bool automatic=action==0u || action==THEME_RESTORE_WORK;
    bool current=automatic || theme_result_request==shell.theme_generation;
    if(current && theme_result_ok) {
        if(action==THEME_SCAN_WORK) {
            theme_catalog=theme_catalog_result;
        } else if(action==THEME_CANCEL_WORK) {
            kui_theme_bundle_release(&theme_preview_bundle);theme_preview_showing=false;
        } else if(theme_result_has_bundle) {
            struct kui_theme_bundle *destination=action==THEME_PREVIEW_WORK?
                &theme_preview_bundle:&theme_saved_bundle;
            if(action==0u && splash_active) {
                /* Menu publication must leave the first frame's image owned
                 * and visible until the normal splash deadline or B skip. */
                theme_result_bundle.images[KUI_THEME_SPLASH]=destination->images[KUI_THEME_SPLASH];
                theme_result_bundle.status[KUI_THEME_SPLASH]=destination->status[KUI_THEME_SPLASH];
                destination->images[KUI_THEME_SPLASH]=(struct kui_theme_image){0};
            }
            kui_theme_bundle_release(destination);
            *destination=theme_result_bundle;
            kui_theme_bundle_default(&theme_result_bundle);
            theme_result_has_bundle=false;
            if(action==THEME_PREVIEW_WORK) theme_preview_showing=true;
            else {
                /* A queued automatic reload may follow an explicit Preview.
                 * Restore the saved bundle without discarding that preview. */
                if(action!=THEME_RESTORE_WORK) {
                    kui_theme_bundle_release(&theme_preview_bundle);theme_preview_showing=false;
                }
                if(action==THEME_APPLY_WORK) theme_restore_needed=false;
                if(!splash_active && theme_saved_bundle.images[KUI_THEME_SPLASH].pixels)
                    kui_theme_bundle_release_asset(&theme_saved_bundle,KUI_THEME_SPLASH);
            }
        }
    }
    if(current) {
        if(action==THEME_SCAN_WORK) kui_shell_set_theme_catalog(&shell,theme_result_request,
            &theme_catalog,theme_result_ok,theme_result_note);
        else if(!automatic) {
            enum kui_shell_action result_action=action==THEME_PREVIEW_WORK?KUI_SHELL_THEME_PREVIEW:
                action==THEME_APPLY_WORK?KUI_SHELL_THEME_APPLY:KUI_SHELL_THEME_CANCEL;
            kui_shell_theme_result(&shell,theme_result_request,result_action,theme_result_ok,theme_result_note);
        }
    }
    if(theme_result_has_bundle) kui_theme_bundle_release(&theme_result_bundle);
    theme_result_has_bundle=false;theme_result_ready=false;
    shell_frame_ready=false;
    return true;
}
/* Main only, before starting a memory test or executable preparation. Keep
 * choices/palette, release image buffers; a failed/returning operation reloads
 * the menu assets through the same worker, never from drawing. */
static void theme_release_for_action(unsigned action) {
    if(action!=19u && !(action>=56u && action<=58u) && action!=62u && action!=79u &&
       action!=104u && action!=121u && action!=122u && action!=126u && action!=22u &&
       action!=25u && action!=44u && action!=KUI_GAMES_MEDIA_DOWNLOAD_JOB) return;
    kui_theme_bundle_release(&theme_preview_bundle);theme_preview_showing=false;
    for(unsigned kind=0;kind<KUI_THEME_ASSET_COUNT;kind++) {
        if(theme_saved_bundle.images[kind].pixels && kind!=KUI_THEME_SPLASH) theme_restore_needed=true;
        kui_theme_image_release(&theme_saved_bundle.images[kind]);
        if(theme_saved_bundle.status[kind]==KUI_THEME_ASSET_READY)
            theme_saved_bundle.status[kind]=KUI_THEME_ASSET_DEFAULT;
    }
    shell_frame_ready=false;
}
