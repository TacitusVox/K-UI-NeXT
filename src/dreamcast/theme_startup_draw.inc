/* SPDX-License-Identifier: GPL-3.0-only */
static void theme_draw_startup_splash(const struct kui_theme_bundle *theme,uint16_t *target) {
    if(theme->status[KUI_THEME_SPLASH]==KUI_THEME_ASSET_DISABLED) {
        uint16_t color=kui_theme_rgb565(theme->palette.colors[KUI_THEME_BACKGROUND]);
        for(unsigned i=0;i<640u*480u;i++) target[i]=color;
    } else if(theme->images[KUI_THEME_SPLASH].pixels) {
        memcpy(target,theme->images[KUI_THEME_SPLASH].pixels,KUI_SPLASH_FRAME_BYTES);
    } else if(!strcmp(kui_theme_component_id(&theme->settings,KUI_THEME_SPLASH),"default")) {
        kui_splash_draw(target);
    } else {
        kui_splash_draw_factory(target);
    }
}
