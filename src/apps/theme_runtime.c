/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/theme_runtime.h"
#include <string.h>

void kui_theme_bundle_default(struct kui_theme_bundle *out) {
    if(!out) return;
    memset(out,0,sizeof(*out));
    kui_theme_settings_default(&out->settings);
    kui_theme_palette_default(&out->palette);
}
void kui_theme_bundle_release_asset(struct kui_theme_bundle *bundle,unsigned kind) {
    if(!bundle || kind>=KUI_THEME_ASSET_COUNT) return;
    kui_theme_image_release(&bundle->images[kind]);
    bundle->status[kind]=KUI_THEME_ASSET_DEFAULT;
}
void kui_theme_bundle_release(struct kui_theme_bundle *bundle) {
    if(!bundle) return;
    for(unsigned i=0;i<KUI_THEME_ASSET_COUNT;i++)
        kui_theme_bundle_release_asset(bundle,i);
    kui_theme_bundle_default(bundle);
}
bool kui_theme_bundle_prepare(struct kui_theme_bundle *out,
    const struct kui_theme_settings *settings,unsigned mask,bool strict,
    kui_log_fn log,kui_cancel_fn cancelled) {
    if(!out || !kui_theme_settings_valid(settings) || (mask&~KUI_THEME_LOAD_ALL)) return false;
    kui_theme_bundle_default(out);
    out->settings=*settings;
    /* Cache at most one definition per selected component plus the palette.
     * Ordinary startup never enumerates the themes directory. */
    struct kui_theme_definition definitions[KUI_THEME_ASSET_COUNT+1u];
    char ids[KUI_THEME_ASSET_COUNT+1u][KUI_THEME_ID_BYTES];
    bool valid[KUI_THEME_ASSET_COUNT+1u]={false};
    unsigned count=1;
    strcpy(ids[0],settings->theme_id);
    if(cancelled && cancelled()) goto failed;
    valid[0]=kui_theme_definition_load(&definitions[0],ids[0],log);
    if(!valid[0]) {
        if(strict) goto failed;
        kui_theme_definition_default(&definitions[0]);
        ++out->fallback_count;
    }
    kui_theme_effective_palette(&out->palette,&definitions[0],settings);
    for(unsigned kind=0;kind<KUI_THEME_ASSET_COUNT;kind++) {
        if(!(mask&(1u<<kind))) continue;
        if(cancelled && cancelled()) goto failed;
        const char *id=kui_theme_component_id(settings,(enum kui_theme_asset_kind)kind);
        if(!strcmp(id,"none")) {
            out->status[kind]=KUI_THEME_ASSET_DISABLED;
            continue;
        }
        unsigned d=0;
        while(d<count && strcmp(ids[d],id)) ++d;
        if(d==count) {
            strcpy(ids[d],id);
            valid[d]=kui_theme_definition_load(&definitions[d],id,log);
            ++count;
        }
        if(!valid[d]) {
            if(strict) goto failed;
            ++out->fallback_count;
            continue;
        }
        out->status[kind]=kui_theme_image_load(&out->images[kind],&definitions[d],
            (enum kui_theme_asset_kind)kind,log,cancelled);
        if(out->status[kind]==KUI_THEME_ASSET_CANCELLED) goto failed;
        if(out->status[kind]==KUI_THEME_ASSET_FAILED) {
            if(strict) goto failed;
            out->status[kind]=KUI_THEME_ASSET_DEFAULT;
            ++out->fallback_count;
        }
    }
    if(cancelled && cancelled()) goto failed;
    return true;
failed:
    kui_theme_bundle_release(out);
    return false;
}
