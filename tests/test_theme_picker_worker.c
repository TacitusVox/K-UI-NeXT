/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/shell.h"
#include "kui/splash.h"
#include "kui/games_storage.h"
#include "kui/storage.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Run the production coordinator with a controllable storage boundary. The
 * read hook injects input/publication at the same boundary as a worker yield. */
static struct kui_shell shell;
static struct kui_theme_catalog catalog;
static struct { bool parked; } salvage_session;
static bool screenshot_frame_pending,update_restart_ready,foreground;
static bool lock,mounted,card_ok=true;
static uint64_t now;
static unsigned loads,connections,allocations,original_thumbs,factory_thumbs;
static const char *focus_id;
static void (*during_read)(void);
static void mutex_lock(bool *m) {assert(!*m);*m=true;}
static void mutex_unlock(bool *m) {assert(*m);*m=false;}
static uint64_t timer_ms_gettime64(void) {return now;}
static bool games_idle_blocked_locked(void) {assert(lock);return foreground;}
const char *kui_shell_theme_picker_asset_id(const struct kui_shell *s) {assert(s==&shell);return focus_id;}
static void kui_log(const char *fmt,...) {(void)fmt;}
static void kui_sd_set_params(unsigned transport,bool crc) {assert(!lock && transport==KUI_STORAGE_AUTO && crc);}
enum kui_games_storage_result kui_games_storage_open(kui_log_fn log) {
    assert(!lock && log==kui_log);
    if(!mounted)connections++;
    if(!card_ok)return KUI_GAMES_STORAGE_NO_CARD;
    mounted=true;return KUI_GAMES_STORAGE_OK;
}
bool kui_games_storage_done(bool keep) {assert(!lock && mounted);mounted=keep;return true;}
bool kui_theme_definition_load(struct kui_theme_definition *out,const char *id,kui_log_fn log) {
    (void)out;(void)id;assert(mounted && !lock && log==kui_log);return false;
}
void kui_theme_image_release(struct kui_theme_image *image) {
    if(image->pixels){assert(allocations);allocations--;free(image->pixels);}
    *image=(struct kui_theme_image){0};
}
static uint16_t *marker(uint16_t value) {
    assert(allocations<2u);
    uint16_t *pixel=malloc(sizeof(*pixel));assert(pixel);*pixel=value;++allocations;return pixel;
}
uint16_t *kui_splash_thumbnail_decode(bool factory,bool (*cancelled)(void)) {
    assert(!lock && cancelled && !cancelled());
    if(factory)factory_thumbs++;else original_thumbs++;
    return marker(factory?2u:1u);
}
enum kui_theme_asset_status kui_theme_thumbnail_load(struct kui_theme_image *out,const struct kui_theme_definition *d,
        enum kui_theme_asset_kind kind,kui_log_fn log,kui_cancel_fn cancelled) {
    assert(!lock && mounted && log==kui_log);loads++;
    assert(!cancelled());
    out->pixels=marker((uint16_t)d->id[0]);
    out->width=KUI_THEME_THUMB_WIDTH;
    out->height=kind==KUI_THEME_HEADER?KUI_THEME_THUMB_HEADER_HEIGHT:KUI_THEME_THUMB_HEIGHT;
    if(during_read){void (*operation)(void)=during_read;during_read=NULL;operation();}
    if(cancelled()) {kui_theme_image_release(out);return KUI_THEME_ASSET_CANCELLED;}
    return KUI_THEME_ASSET_READY;
}
#include "../src/dreamcast/theme_picker_runtime.inc"

static bool publish(void) {mutex_lock(&lock);bool changed=theme_picker_update_locked(now);mutex_unlock(&lock);return changed;}
static void highlight(const char *id,unsigned row) {
    focus_id=id;shell.page=KUI_SHELL_THEME_PICKER;shell.theme_selected=row;
    shell.theme_picker_generation++;assert(publish());
}
static void settle(void) {now+=THEME_PICKER_SETTLE_MS;assert(theme_picker_idle_work());}
static void leave(void) {
    shell.page=KUI_SHELL_APPEARANCE;shell.theme_picker_generation++;
    assert(publish() && !picker_visible);
}
static void move_during_read(void) {highlight("c",2);}
static void queue_foreground(void) {foreground=true;}
int main(void) {
    shell.theme_catalog=&catalog;
    const char *ids[]={"default","a","b","c","d","no-art"};
    for(unsigned i=0;i<sizeof(ids)/sizeof(ids[0]);i++) {
        struct kui_theme_definition *d=&catalog.themes[catalog.count++];
        strcpy(d->id,ids[i]);
        if(i>=1 && i<=4) {
            strcpy(d->assets[KUI_THEME_HEADER],"hdr.kuiimg");strcpy(d->assets[KUI_THEME_SPLASH],"splash.kuiimg");
        }
    }
    highlight("a",2);assert(theme_picker_idle_work() && !loads);
    highlight("b",2);assert(!picker_visible && !loads);
    settle();assert(!picker_visible && allocations==1 && !theme_picker_idle_work());
    assert(publish() && *picker_visible->pixels=='b' && loads==1 && connections==1);
    highlight("a",2);during_read=move_during_read;settle();
    assert(!publish() && !picker_visible && allocations==1);
    settle();assert(publish() && *picker_visible->pixels=='c' && loads==3 && connections==1);
    highlight("b",2);assert(*picker_visible->pixels=='b' && !theme_picker_idle_work());
    highlight("a",2);settle();assert(!picker_visible);
    highlight("d",2);assert(!publish() && allocations==1); /* awaiting an old main-thread adoption */
    during_read=queue_foreground;settle();assert(!publish() && !theme_picker_idle_work() && allocations==1);
    foreground=false;settle();assert(publish() && *picker_visible->pixels=='d' && allocations==2);
    highlight("b",2);assert(*picker_visible->pixels=='b' && !theme_picker_idle_work());
    highlight("a",2);settle();assert(publish() && allocations==2);
    highlight("no-art",3);assert(picker_status==KUI_THEME_ASSET_DEFAULT && !theme_picker_idle_work());
    highlight("none",2);assert(picker_status==KUI_THEME_ASSET_DISABLED && !theme_picker_idle_work());
    highlight("a",1);during_read=leave;settle();assert(!publish() && !allocations);
    shell.theme_catalog=NULL;mounted=false;card_ok=false;
    unsigned before=connections;
    highlight("default",1);settle();
    assert(publish() && original_thumbs==1 && *picker_visible->pixels==1 && connections==before);
    highlight("default",2);assert(picker_status==KUI_THEME_ASSET_DEFAULT && !theme_picker_idle_work());
    highlight("unlisted",2);settle();assert(publish() && picker_status==KUI_THEME_ASSET_FAILED && !picker_visible);
    assert(!theme_picker_idle_work());leave();assert(!allocations);
    shell.theme_catalog=&catalog;
    highlight("no-art",1);settle();assert(publish() && factory_thumbs==1 && *picker_visible->pixels==2);
    leave();assert(!allocations);
    puts("Theme picker worker: debounce, cache bounds, cross-focus and late completion, cancellation, original and missing media PASS");
}
