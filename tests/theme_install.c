/* SPDX-License-Identifier: GPL-3.0-only */
/* Mounted by theme_image.c. Each "boot" runs in a fresh process. */
#include "kui/theme_runtime.h"
#include "kui/splash.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/dreamcast/theme_startup_draw.inc"

static void mkdir_if_missing(const char *path) {
    FRESULT result=f_mkdir(path);
    assert(result==FR_OK || result==FR_EXIST);
}

static void install_file(const char *directory,const char *id,const char *name) {
    char host[1024],card[256];
    int n=snprintf(host,sizeof(host),"%s/%s",directory,name);
    assert(n>0 && (size_t)n<sizeof(host));
    n=snprintf(card,sizeof(card),"%s/%s/%s",KUI_THEME_ROOT,id,name);
    assert(n>0 && (size_t)n<sizeof(card));
    FILE *source=fopen(host,"rb");
    FIL target;
    assert(source && f_open(&target,card,FA_WRITE|FA_CREATE_NEW)==FR_OK);
    uint8_t buffer[8192];
    size_t bytes;
    while((bytes=fread(buffer,1,sizeof(buffer),source))) {
        UINT written;
        assert(f_write(&target,buffer,(UINT)bytes,&written)==FR_OK && written==bytes);
    }
    assert(!ferror(source) && !fclose(source) && f_close(&target)==FR_OK);
}

static void install_pack(int argc,char **argv) {
    const char *id=argv[2];
    assert(kui_theme_id_valid(id));
    mkdir_if_missing("0:/KUI");
    mkdir_if_missing(KUI_THEME_ROOT);
    char folder[128];
    int n=snprintf(folder,sizeof(folder),"%s/%s",KUI_THEME_ROOT,id);
    assert(n>0 && (size_t)n<sizeof(folder));
    mkdir_if_missing(folder);
    for(int i=3;i<argc;i++) install_file(argv[1],id,argv[i]);
}

static void set_id(char id[KUI_THEME_ID_BYTES],const char *value) {
    assert(strlen(value)<KUI_THEME_ID_BYTES);
    strcpy(id,!strcmp(value,"-")?"":value);
}

static void print_image(const char *label,const struct kui_theme_bundle *bundle,unsigned kind) {
    const struct kui_theme_image *image=&bundle->images[kind];
    if(bundle->status[kind]==KUI_THEME_ASSET_READY) {
        assert(image->pixels && image->width==640 && image->height==(kind==KUI_THEME_HEADER?100u:480u));
        printf("%s pixels-%08" PRIx32 "\n",label,kui_crc32(0,image->pixels,image->width*image->height*2u));
    } else {
        assert(!image->pixels);
        assert(bundle->status[kind]==KUI_THEME_ASSET_DEFAULT || bundle->status[kind]==KUI_THEME_ASSET_DISABLED);
        printf("%s %s\n",label,bundle->status[kind]==KUI_THEME_ASSET_DISABLED?"none":"built-in");
    }
}

static void boot(void) {
    struct kui_theme_bundle startup={0},menu={0};
    kui_theme_startup_splash_prepare(&startup,NULL);
    static uint16_t frame[KUI_THEME_WIDTH*KUI_THEME_HEIGHT];
    theme_draw_startup_splash(&startup,frame);
    printf("FRAME pixels-%08" PRIx32 "\n",kui_crc32(0,frame,sizeof(frame)));
    kui_splash_release();
    printf("THEME %s\n",startup.settings.theme_id);
    print_image("SPLASH",&startup,KUI_THEME_SPLASH);
    assert(!startup.images[KUI_THEME_HEADER].pixels && !startup.images[KUI_THEME_BACKDROP].pixels);
    assert(kui_theme_bundle_prepare(&menu,&startup.settings,KUI_THEME_LOAD_MENU,false,NULL,NULL));
    print_image("BANNER",&menu,KUI_THEME_HEADER);
    kui_theme_bundle_release(&startup);
    kui_theme_bundle_release(&menu);
}

bool theme_install_test(int argc,char **argv) {
    assert(argc>0);
    if(!strcmp(argv[0],"boot")) {
        assert(argc==1);
        boot();
        return false;
    }
    if(!strcmp(argv[0],"catalog")) {
        struct kui_theme_catalog catalog;
        assert(kui_theme_catalog_scan(&catalog,NULL,NULL) && !catalog.truncated);
        for(unsigned i=0;i<catalog.count;i++) printf("PACK %s\n",catalog.themes[i].id);
        return false;
    }
    if(!strcmp(argv[0],"install")) {
        assert(argc>=4);
        install_pack(argc,argv);
    } else if(!strcmp(argv[0],"save")) {
        assert(argc==4);
        struct kui_theme_settings settings;
        kui_theme_settings_default(&settings);
        set_id(settings.theme_id,argv[1]);
        set_id(settings.splash_id,argv[2]);
        set_id(settings.header_id,argv[3]);
        assert(kui_theme_settings_save(&settings,NULL));
    } else if(!strcmp(argv[0],"remove")) {
        assert(argc==2 && !strncmp(argv[1],KUI_THEME_ROOT "/",sizeof(KUI_THEME_ROOT)));
        assert(f_unlink(argv[1])==FR_OK);
    } else {
        assert(!strcmp(argv[0],"damage") && argc==2);
        assert(!strcmp(argv[1],"a") || !strcmp(argv[1],"b"));
        FIL file;
        UINT written;
        const char *path=!strcmp(argv[1],"a")?KUI_THEME_SETTINGS_PATH_A:KUI_THEME_SETTINGS_PATH_B;
        assert(f_open(&file,path,FA_WRITE)==FR_OK);
        assert(f_write(&file,"BAD!",4,&written)==FR_OK && written==4 && f_close(&file)==FR_OK);
    }
    return true;
}
