/* SPDX-License-Identifier: GPL-3.0-only */
/* Production menu/splash previews: render-theme MODE default|PACK OUT.ppm.
 * MODE is home, appearance, colors, splash (preview), or startup (full art).
 * PACK is a prepared folder; no settings or source assets are changed. */
#include "kui/shell.h"
#include "kui/splash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t frame[640u*480u];
static struct kui_shell shell;

static bool load_pack(const char *folder,struct kui_theme_definition *definition,
        struct kui_theme_image images[KUI_THEME_ASSET_COUNT],
        enum kui_theme_asset_status status[KUI_THEME_ASSET_COUNT]) {
    char path[1024];
    int n=snprintf(path,sizeof(path),"%s/theme.ini",folder);
    if(n<0 || (size_t)n>=sizeof(path))return false;
    FILE *file=fopen(path,"rb");if(!file)return false;
    uint8_t manifest[KUI_THEME_MANIFEST_MAX+1];
    size_t bytes=fread(manifest,1,sizeof(manifest),file);
    bool failed=ferror(file)!=0;
    if(fclose(file) || failed || !kui_theme_parse(definition,"preview-pack",manifest,bytes))return false;
    for(unsigned kind=0;kind<KUI_THEME_ASSET_COUNT;kind++) {
        const char *name=definition->assets[kind];
        if(!name[0])continue;
        if(!strcmp(name,"none")) {status[kind]=KUI_THEME_ASSET_DISABLED;continue;}
        n=snprintf(path,sizeof(path),"%s/%s",folder,name);
        if(n<0 || (size_t)n>=sizeof(path))return false;
        file=fopen(path,"rb");if(!file)return false;
        if(fseek(file,0,SEEK_END)) {fclose(file);return false;}
        long size=ftell(file);
        if(size<0 || fseek(file,0,SEEK_SET)) {fclose(file);return false;}
        uint8_t header[KUI_THEME_IMAGE_HEADER_SIZE];uint32_t expected;
        size_t got=fread(header,1,sizeof(header),file);
        if(!kui_theme_image_header(header,got,(enum kui_theme_asset_kind)kind,(size_t)size,&expected)) {
            fclose(file);return false;
        }
        size_t payload=(size_t)size-sizeof(header);
        uint8_t *pixels=malloc(payload);
        if(!pixels) {fclose(file);return false;}
        got=fread(pixels,1,payload,file);failed=ferror(file)!=0;
        if(fclose(file) || failed || got!=payload || kui_crc32(0,pixels,payload)!=expected) {
            free(pixels);return false;
        }
        /* Decode wire order explicitly, including on a big-endian host. */
        uint16_t *native=(uint16_t *)(void *)pixels;
        for(size_t i=0;i<payload/2;i++)native[i]=(uint16_t)(pixels[i*2]|((uint16_t)pixels[i*2+1]<<8));
        images[kind]=(struct kui_theme_image){native,640u,kind==KUI_THEME_HEADER?100u:480u};
        status[kind]=KUI_THEME_ASSET_READY;
    }
    return true;
}

static bool write_ppm(const char *path) {
    FILE *file=fopen(path,"wb");if(!file)return false;
    bool ok=fprintf(file,"P6\n640 480\n255\n")>0;
    for(unsigned i=0;ok && i<640u*480u;i++) {
        unsigned pixel=frame[i];
        uint8_t rgb[3]={(uint8_t)(((pixel>>11)&31u)*255u/31u),
            (uint8_t)(((pixel>>5)&63u)*255u/63u),(uint8_t)((pixel&31u)*255u/31u)};
        ok=fwrite(rgb,1,sizeof(rgb),file)==sizeof(rgb);
    }
    return fclose(file)==0 && ok;
}

int main(int argc,char **argv) {
    if(argc!=4) {
        fprintf(stderr,"Usage: %s home|appearance|colors|splash|startup default|PACK OUT.ppm\n",argv[0]);
        return 2;
    }
    bool startup=!strcmp(argv[1],"startup");
    enum kui_shell_page page=KUI_SHELL_HOME;
    if(!strcmp(argv[1],"appearance"))page=KUI_SHELL_APPEARANCE;
    else if(!strcmp(argv[1],"colors"))page=KUI_SHELL_THEME_COLORS;
    else if(!strcmp(argv[1],"splash"))page=KUI_SHELL_THEME_SPLASH_PREVIEW;
    else if(!startup && strcmp(argv[1],"home"))return 2;
    struct kui_theme_catalog catalog={.count=1};
    kui_theme_definition_default(&catalog.themes[0]);
    struct kui_theme_image images[KUI_THEME_ASSET_COUNT]={{0}};
    enum kui_theme_asset_status status[KUI_THEME_ASSET_COUNT]={KUI_THEME_ASSET_DEFAULT};
    struct kui_theme_settings settings;kui_theme_settings_default(&settings);
    struct kui_theme_definition *definition=&catalog.themes[0];
    int result=1;
    if(strcmp(argv[2],"default")) {
        definition=&catalog.themes[1];
        if(!load_pack(argv[2],definition,images,status)) {
            fprintf(stderr,"Invalid or unreadable prepared pack: %s\n",argv[2]);goto done;
        }
        catalog.count=2;
    }
    snprintf(settings.theme_id,sizeof(settings.theme_id),"%s",definition->id);
    if(!images[KUI_THEME_SPLASH].pixels && status[KUI_THEME_SPLASH]==KUI_THEME_ASSET_DEFAULT) {
        uint16_t *pixels=!strcmp(settings.theme_id,"default")?
            kui_splash_preview_decode():kui_splash_preview_factory_decode();
        images[KUI_THEME_SPLASH]=(struct kui_theme_image){pixels,640u,480u};
        if(!images[KUI_THEME_SPLASH].pixels) {fputs("Built-in splash decode failed\n",stderr);goto done;}
    }
    struct kui_settings preferences={true,false,true,KUI_CAPTURE_FORMAT_GDI,0,false};
    kui_shell_init(&shell,&preferences);
    kui_shell_set_theme_preferences(&shell,&settings);
    shell.page=page;shell.theme_catalog=&catalog;
    shell.theme_preview_active=true;shell.theme_selected=5;
    shell.theme_color_role=KUI_THEME_ACCENT;shell.theme_color_selected=1;
    struct kui_theme_palette palette;
    kui_theme_effective_palette(&palette,definition,&settings);
    struct kui_shell_view view={.theme_palette=&palette,.theme_images=images,.theme_asset_status=status,
        .branded_banner=definition->banner_only,
        .build="HOST PREVIEW",.music_title="Moonlit Circuit",.music_enabled=true,
        .music_playing=true,.music_volume=75};
    if(startup) {
        if(images[KUI_THEME_SPLASH].pixels)memcpy(frame,images[KUI_THEME_SPLASH].pixels,sizeof(frame));
    } else kui_shell_draw(frame,&shell,&view,NULL,NULL);
    result=write_ppm(argv[3])?0:1;
done:
    for(unsigned i=0;i<KUI_THEME_ASSET_COUNT;i++)free(images[i].pixels);
    return result;
}
