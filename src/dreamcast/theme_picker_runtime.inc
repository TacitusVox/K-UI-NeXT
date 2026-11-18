/* SPDX-License-Identifier: GPL-3.0-only */
/* Frame-visible buffers belong to main. The idle storage worker holds its
 * own candidate until the UI can adopt it between frames. */
#define THEME_PICKER_CACHE_SIZE 3u
#define THEME_PICKER_SETTLE_MS 110u
struct theme_picker_entry {
    char id[KUI_THEME_ID_BYTES];
    enum kui_theme_asset_kind kind;
    struct kui_theme_image image;
};
static struct theme_picker_entry picker_cache[THEME_PICKER_CACHE_SIZE];
static unsigned picker_next_slot,picker_focus,picker_worker_focus,picker_ready_focus;
static uint64_t picker_due;
static struct theme_picker_entry picker_ready;
static const struct kui_theme_image *picker_visible;
static enum kui_theme_asset_status picker_status,picker_ready_status;
static const struct kui_theme_definition *picker_definition;
static enum kui_theme_asset_kind picker_kind;

static bool picker_current_locked(void) {
    return picker_focus && shell.page==KUI_SHELL_THEME_PICKER &&
        picker_focus==shell.theme_picker_generation;
}
static bool picker_worker_allowed_locked(void) {
    return picker_current_locked() && !games_idle_blocked_locked() && !screenshot_frame_pending &&
        !update_restart_ready;
}
static bool picker_cancelled(void) {
    mutex_lock(&lock);
    bool stop=picker_worker_focus!=picker_focus || !picker_worker_allowed_locked();
    mutex_unlock(&lock);
    return stop;
}
static void picker_drop_ready(void) {
    kui_theme_image_release(&picker_ready.image);
    picker_ready_focus=0;
}
/* Main, under the publication lock, after input. No storage or decode here. */
static bool theme_picker_update_locked(uint64_t now) {
    if(shell.page!=KUI_SHELL_THEME_PICKER) {
        if(!picker_focus)return false;
        picker_focus=0;picker_visible=NULL;picker_definition=NULL;
        picker_drop_ready();
        for(unsigned i=0;i<THEME_PICKER_CACHE_SIZE;i++) {
            kui_theme_image_release(&picker_cache[i].image);
            picker_cache[i].id[0]=0;
        }
        picker_next_slot=0;
        return true;
    }
    if(picker_focus!=shell.theme_picker_generation) {
        picker_focus=shell.theme_picker_generation;picker_visible=NULL;picker_definition=NULL;
        picker_drop_ready();
        picker_due=now+THEME_PICKER_SETTLE_MS;
        picker_kind=shell.theme_selected==2u?KUI_THEME_HEADER:
            shell.theme_selected==3u?KUI_THEME_BACKDROP:KUI_THEME_SPLASH;
        const char *id=kui_shell_theme_picker_asset_id(&shell);
        picker_status=KUI_THEME_ASSET_FAILED;
        if(!id)return true;
        if(!strcmp(id,"none")) {picker_status=KUI_THEME_ASSET_DISABLED;return true;}
        for(unsigned i=0;i<THEME_PICKER_CACHE_SIZE;i++) {
            const struct theme_picker_entry *entry=&picker_cache[i];
            if(entry->image.pixels && entry->kind==picker_kind && !strcmp(entry->id,id)) {
                picker_status=KUI_THEME_ASSET_READY;picker_visible=&entry->image;
                return true;
            }
        }
        const struct kui_theme_catalog *catalog=shell.theme_catalog;
        if(catalog)for(unsigned i=0;i<catalog->count && i<KUI_THEME_CATALOG_MAX;i++) {
            if(!strcmp(catalog->themes[i].id,id)) {picker_definition=&catalog->themes[i];break;}
        }
        if(!strcmp(id,"default") && picker_kind!=KUI_THEME_SPLASH) {
            picker_status=KUI_THEME_ASSET_DEFAULT;return true;
        }
        if(picker_definition) {
            const char *asset=picker_definition->assets[picker_kind];
            if(!strcmp(asset,"none")) {picker_status=KUI_THEME_ASSET_DISABLED;return true;}
            if(!asset[0] && picker_kind!=KUI_THEME_SPLASH) {
                picker_status=KUI_THEME_ASSET_DEFAULT;return true;
            }
        }
        picker_status=KUI_THEME_ASSET_CANCELLED; /* Worker has yet to publish. */
        return true;
    }
    if(picker_ready_focus && picker_ready_focus==picker_focus) {
        picker_status=picker_ready_status;
        if(picker_status==KUI_THEME_ASSET_READY) {
            struct theme_picker_entry *slot=&picker_cache[picker_next_slot];
            kui_theme_image_release(&slot->image);
            *slot=picker_ready;
            picker_ready.image=(struct kui_theme_image){0};
            picker_next_slot=(picker_next_slot+1u)%THEME_PICKER_CACHE_SIZE;
            picker_visible=&slot->image;
        }
        picker_drop_ready();
        return true;
    }
    return false;
}
/* The sole storage worker services foreground requests before returning here.
 * A new highlight or queued job cancels the read between bounded chunks. */
static bool theme_picker_idle_work(void) {
    struct theme_picker_entry entry={0};
    struct kui_theme_definition definition;
    mutex_lock(&lock);
    if(!picker_worker_allowed_locked() || picker_ready_focus ||
       picker_status!=KUI_THEME_ASSET_CANCELLED) {
        mutex_unlock(&lock);return false;
    }
    /* Defer speculative music/game reads during the brief settle interval. */
    if(timer_ms_gettime64()<picker_due) {mutex_unlock(&lock);return true;}
    picker_worker_focus=picker_focus;entry.kind=picker_kind;
    snprintf(entry.id,sizeof(entry.id),"%s",kui_shell_theme_picker_asset_id(&shell));
    bool known=picker_definition!=NULL;
    if(known)definition=*picker_definition;
    mutex_unlock(&lock);

    enum kui_theme_asset_status status=KUI_THEME_ASSET_FAILED;
    bool custom=known && definition.assets[entry.kind][0];
    if(!strcmp(entry.id,"default") || (known && !custom))status=KUI_THEME_ASSET_DEFAULT;
    else if(!salvage_session.parked && !picker_cancelled()) {
        kui_sd_set_params(KUI_STORAGE_AUTO,true);
        if(kui_games_storage_open(kui_log)==KUI_GAMES_STORAGE_OK) {
            if(known || kui_theme_definition_load(&definition,entry.id,kui_log))
                status=kui_theme_thumbnail_load(&entry.image,&definition,entry.kind,kui_log,picker_cancelled);
            if(!kui_games_storage_done(status!=KUI_THEME_ASSET_FAILED))status=KUI_THEME_ASSET_FAILED;
        }
    }
    if(entry.kind==KUI_THEME_SPLASH && status==KUI_THEME_ASSET_DEFAULT && !picker_cancelled()) {
        entry.image.pixels=kui_splash_thumbnail_decode(strcmp(entry.id,"default")!=0);
        entry.image.width=KUI_THEME_THUMB_WIDTH;entry.image.height=KUI_THEME_THUMB_HEIGHT;
        status=entry.image.pixels?KUI_THEME_ASSET_READY:KUI_THEME_ASSET_FAILED;
    }
    mutex_lock(&lock);
    if(picker_worker_allowed_locked() && picker_worker_focus==picker_focus &&
       status!=KUI_THEME_ASSET_CANCELLED) {
        picker_ready=entry;entry.image=(struct kui_theme_image){0};
        picker_ready_status=status;picker_ready_focus=picker_focus;
    }
    mutex_unlock(&lock);
    kui_theme_image_release(&entry.image);
    return true;
}
