/* SPDX-License-Identifier: GPL-3.0-only */
#define _POSIX_C_SOURCE 200809L
#include "kui/theme.h"
#include "kui/media.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Real FatFs image tests. Fault injection occurs below the production file
 * adapter so verification and the retained slot exercise the real I/O path. */
static struct {
    FILE *image;
    uint64_t blocks;
    unsigned writes;
    const char *fault;
    bool payload_seen,injected;
} test;
static FATFS fs;
static const char *const slots[]={KUI_THEME_SETTINGS_PATH_A,KUI_THEME_SETTINGS_PATH_B};
static const char *const kept[]={"0:/KUI/settings-a.bin","0:/KUI/apps/system/settings-a.bin"};
static const char sentinel[]="Existing capture/system settings remain unchanged.\n";
static struct kui_theme_settings defaults,previous,changed,third;
static unsigned cancel_calls,cancel_at;
static bool cancelled(void) {return ++cancel_calls>=cancel_at;}
static bool fault(const char *name) {return test.fault && !strcmp(test.fault,name);}
static void put32(uint8_t *p,uint32_t value) {for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(value>>(i*8));}
static void log_line(const char *format,...) {
    va_list args;va_start(args,format);vprintf(format,args);va_end(args);puts("");
}
static uint64_t blocks(void *ctx) {(void)ctx;return test.blocks;}
static bool contains_record(const uint8_t *data,size_t count) {
    for(size_t i=0;i<count;i++)if(!memcmp(data+i*512,"KUITHE01",8))return true;
    return false;
}
static int read_image(void *ctx,uint32_t block,size_t count,uint8_t *data) {
    (void)ctx;
    if(fault("read-fail")){test.injected=true;return -1;}
    if(fseeko(test.image,(off_t)block*512,SEEK_SET) || fread(data,512,count,test.image)!=count)return -1;
    if(test.payload_seen && contains_record(data,count)) {
        if(fault("readback-fail")){test.injected=true;return -1;}
        if(fault("readback-corrupt")){data[24]^=1;test.injected=true;}
        if(fault("readback-substitute"))for(size_t i=0;i<count;i++)if(!memcmp(data+i*512,"KUITHE01",8)) {
            uint8_t *record=data+i*512;record[156]^=1;
            put32(record+252,kui_crc32(0,record,252));test.injected=true;
        }
    }
    return 0;
}
static int write_image(void *ctx,uint32_t block,size_t count,const uint8_t *data) {
    (void)ctx;++test.writes;
    if(contains_record(data,count)) {
        test.payload_seen=true;
        if(fault("write-fail")){test.injected=true;return -1;}
    }
    return fseeko(test.image,(off_t)block*512,SEEK_SET) || fwrite(data,512,count,test.image)!=count?-1:0;
}
static int sync_image(void *ctx) {
    (void)ctx;
    if(fault("sync-fail") && test.payload_seen){test.injected=true;return -1;}
    return fflush(test.image) || fsync(fileno(test.image))?-1:0;
}
static const struct kui_media_ops media={NULL,blocks,read_image,write_image,sync_image};
static void remount(void) {
    assert(f_mount(NULL,"0:",0)==FR_OK);kui_media_set(&media);assert(kui_mount(&fs,log_line));
}
static void equals(const struct kui_theme_settings *actual,const struct kui_theme_settings *expected) {
    assert(!strcmp(actual->theme_id,expected->theme_id));
    assert(!strcmp(actual->splash_id,expected->splash_id));
    assert(!strcmp(actual->header_id,expected->header_id));
    assert(!strcmp(actual->background_id,expected->background_id));
    assert(actual->override_mask==expected->override_mask);
    assert(!memcmp(&actual->palette,&expected->palette,sizeof(actual->palette)));
}
static void write_file(const char *path,const void *data,size_t bytes) {
    FIL file;UINT written;
    assert(f_open(&file,path,FA_WRITE|FA_CREATE_ALWAYS)==FR_OK);
    assert(f_write(&file,data,(UINT)bytes,&written)==FR_OK && written==bytes);
    assert(f_sync(&file)==FR_OK && f_close(&file)==FR_OK);
}
static void check_file(const char *path,const void *expected,size_t bytes) {
    FIL file;UINT got;uint8_t data[KUI_THEME_SETTINGS_RECORD_SIZE];assert(bytes<=sizeof(data));
    assert(f_open(&file,path,FA_READ)==FR_OK && f_size(&file)==bytes);
    assert(f_read(&file,data,(UINT)bytes,&got)==FR_OK && got==bytes && !memcmp(data,expected,bytes));
    assert(f_close(&file)==FR_OK);
}
static void check_slot(unsigned slot,const struct kui_theme_settings *expected,uint64_t sequence) {
    uint8_t record[KUI_THEME_SETTINGS_RECORD_SIZE];assert(kui_theme_settings_encode(record,expected,sequence));
    check_file(slots[slot],record,sizeof(record));
}
static void check_loaded(const struct kui_theme_settings *expected) {
    struct kui_theme_settings actual;assert(kui_theme_settings_load(&actual,log_line));equals(&actual,expected);
}
static void damage_newest(const char *name) {
    uint8_t record[KUI_THEME_SETTINGS_RECORD_SIZE+1]={0};
    assert(kui_theme_settings_encode(record,&changed,2));size_t size=KUI_THEME_SETTINGS_RECORD_SIZE;
    if(!strcmp(name,"corrupt"))record[24]^=1;
    else if(!strcmp(name,"truncated"))size--;
    else if(!strcmp(name,"oversize"))size++;
    else {
        if(!strcmp(name,"version"))put32(record+8,99);
        else if(!strcmp(name,"reserved"))record[210]=1;
        else if(!strcmp(name,"unsafe-id"))memcpy(record+24,"../other",9);
        else assert(false);
        put32(record+252,kui_crc32(0,record,252));
    }
    write_file(slots[1],record,size);
}
static void make_manifest(const char *id,const char *name,const char *extra) {
    char path[160],manifest[512];
    int length=snprintf(path,sizeof(path),KUI_THEME_ROOT "/%s",id);
    assert(length>0 && (size_t)length<sizeof(path) && f_mkdir(path)==FR_OK);
    length=snprintf(path,sizeof(path),KUI_THEME_ROOT "/%s/theme.ini",id);
    assert(length>0 && (size_t)length<sizeof(path));
    length=snprintf(manifest,sizeof(manifest),"[theme]\nversion=1\nname=%s\nauthor=TacitusVox\n%s",name,extra);
    assert(length>0 && (size_t)length<sizeof(manifest));write_file(path,manifest,(size_t)length);
}
static void check_catalog(void) {
    assert(f_mkdir(KUI_THEME_ROOT)==FR_OK);
    make_manifest("zebra","zebra","");make_manifest("alpha","Alpha","");
    make_manifest("broken","Broken","splash=../escape\n");
    make_manifest("none","Not an installable theme","");make_manifest("default","Not the builtin","");
    struct kui_theme_catalog catalog;
    unsigned before=test.writes;assert(kui_theme_catalog_scan(&catalog,log_line,NULL));
    assert(test.writes==before && catalog.count==3 && !catalog.truncated);
    assert(!strcmp(catalog.themes[0].id,"default") && !strcmp(catalog.themes[0].name,"K-UI Original"));
    assert(!strcmp(catalog.themes[1].id,"alpha") && !strcmp(catalog.themes[2].id,"zebra"));
    for(int i=29;i>=0;i--) {
        char id[32];int length=snprintf(id,sizeof(id),"item-%02d",i);assert(length>0);
        make_manifest(id,id,"");
    }
    before=test.writes;assert(kui_theme_catalog_scan(&catalog,log_line,NULL));
    assert(test.writes==before && catalog.count==KUI_THEME_CATALOG_MAX && catalog.truncated);
    assert(!strcmp(catalog.themes[1].id,"alpha") && !strcmp(catalog.themes[23].id,"item-21"));
    for(unsigned i=2;i<catalog.count;i++) {
        char id[32];assert(snprintf(id,sizeof(id),"item-%02u",i-2)>0);assert(!strcmp(catalog.themes[i].id,id));
    }
    cancel_calls=0;cancel_at=3;assert(!kui_theme_catalog_scan(&catalog,log_line,cancelled));
    assert(cancel_calls==cancel_at && test.writes==before);
    struct kui_theme_definition definition,unchanged;kui_theme_definition_default(&unchanged);definition=unchanged;
    assert(!kui_theme_definition_load(&definition,"../bad",log_line));assert(!memcmp(&definition,&unchanged,sizeof(definition)));
    assert(!kui_theme_definition_load(&definition,"absent",log_line));assert(!memcmp(&definition,&unchanged,sizeof(definition)));
    assert(!kui_theme_definition_load(&definition,"broken",log_line));assert(!memcmp(&definition,&unchanged,sizeof(definition)));
    assert(kui_theme_definition_load(&definition,"alpha",log_line) && !strcmp(definition.name,"Alpha"));
}
static void check_assets(void) {
    assert(f_mkdir(KUI_THEME_ROOT)==FR_OK);
    make_manifest("assets","Assets","splash=full.kuiimg\nheader=header.kuiimg\nbackground=none\n");
    const size_t full_bytes=640u*480u*2u+32u,header_bytes=640u*100u*2u+32u;
    uint8_t *data=calloc(1,full_bytes+1);assert(data);memcpy(data,"KUITEX01",8);put32(data+8,1);
    data[12]=0x80;data[13]=2;data[14]=0xe0;data[15]=1;put32(data+16,(uint32_t)full_bytes-32);
    for(size_t i=32;i<full_bytes;i+=2){
        unsigned sample=(unsigned)(i-32)/2u;
        uint16_t color=(uint16_t)(0x1234u+sample*31u);
        data[i]=(uint8_t)color;data[i+1]=(uint8_t)(color>>8);
    }
    put32(data+20,kui_crc32(0,data+32,full_bytes-32));
    write_file(KUI_THEME_ROOT "/assets/full.kuiimg",data,full_bytes);
    data[14]=100;data[15]=0;put32(data+16,(uint32_t)header_bytes-32);
    put32(data+20,kui_crc32(0,data+32,header_bytes-32));
    write_file(KUI_THEME_ROOT "/assets/header.kuiimg",data,header_bytes);
    struct kui_theme_definition definition;assert(kui_theme_definition_load(&definition,"assets",log_line));
    struct kui_theme_image image={0};unsigned before=test.writes;
    assert(kui_theme_image_load(&image,&definition,KUI_THEME_SPLASH,log_line,NULL)==KUI_THEME_ASSET_READY);
    assert(image.width==640 && image.height==480 && image.pixels[0]==0x1234 &&
        image.pixels[640*480-1]==(uint16_t)(0x1234u+(640u*480u-1u)*31u));
    kui_theme_image_release(&image);assert(!image.pixels && !image.width && !image.height);
    assert(kui_theme_image_load(&image,&definition,KUI_THEME_HEADER,log_line,NULL)==KUI_THEME_ASSET_READY);
    assert(image.width==640 && image.height==100 &&
        image.pixels[640*100-1]==(uint16_t)(0x1234u+(640u*100u-1u)*31u));kui_theme_image_release(&image);
    for(unsigned kind=KUI_THEME_SPLASH;kind<=KUI_THEME_HEADER;kind++) {
        assert(kui_theme_thumbnail_load(&image,&definition,kind,log_line,NULL)==KUI_THEME_ASSET_READY);
        assert(image.width==256 && image.height==(kind==KUI_THEME_HEADER?40u:192u));
        for(unsigned y=0;y<image.height;y++)for(unsigned x=0;x<image.width;x++) {
            unsigned source_index=y*5u/2u*640u+x*5u/2u;
            assert(image.pixels[y*image.width+x]==(uint16_t)(0x1234u+source_index*31u));
        }
        kui_theme_image_release(&image);
    }
    assert(kui_theme_image_load(&image,&definition,KUI_THEME_BACKDROP,log_line,NULL)==KUI_THEME_ASSET_DISABLED);
    assert(kui_theme_thumbnail_load(&image,&definition,KUI_THEME_BACKDROP,log_line,NULL)==KUI_THEME_ASSET_DISABLED);
    definition.assets[2][0]=0;assert(kui_theme_image_load(&image,&definition,KUI_THEME_BACKDROP,log_line,NULL)==KUI_THEME_ASSET_DEFAULT);
    assert(kui_theme_thumbnail_load(&image,&definition,KUI_THEME_BACKDROP,log_line,NULL)==KUI_THEME_ASSET_DEFAULT);
    cancel_calls=0;cancel_at=1;
    assert(kui_theme_image_load(&image,&definition,KUI_THEME_SPLASH,log_line,cancelled)==KUI_THEME_ASSET_CANCELLED);
    cancel_calls=0;cancel_at=4;
    assert(kui_theme_image_load(&image,&definition,KUI_THEME_SPLASH,log_line,cancelled)==KUI_THEME_ASSET_CANCELLED);
    assert(!image.pixels && test.writes==before);
    cancel_calls=0;cancel_at=4;
    assert(kui_theme_thumbnail_load(&image,&definition,KUI_THEME_SPLASH,log_line,cancelled)==KUI_THEME_ASSET_CANCELLED);
    assert(!image.pixels && !image.width && !image.height && test.writes==before);
    strcpy(definition.assets[1],"../escape");assert(kui_theme_image_load(&image,&definition,KUI_THEME_HEADER,log_line,NULL)==KUI_THEME_ASSET_FAILED);
    memset(definition.assets[1],'x',sizeof(definition.assets[1]));
    assert(kui_theme_image_load(&image,&definition,KUI_THEME_HEADER,log_line,NULL)==KUI_THEME_ASSET_FAILED);
    strcpy(definition.assets[1],"absent.kuiimg");assert(kui_theme_image_load(&image,&definition,KUI_THEME_HEADER,log_line,NULL)==KUI_THEME_ASSET_FAILED);
    strcpy(definition.assets[1],"header.kuiimg");
    for(unsigned error=0;error<6;error++) {
        data[8]=1;data[24]=0;data[12]=0x80;data[40]=(uint8_t)(0x1234u+4u*31u);
        size_t bytes=header_bytes;
        if(error==0)data[40]^=1;
        if(error==1)bytes--;
        if(error==2)bytes++;
        if(error==3)data[8]=2;
        if(error==4)data[24]=1;
        if(error==5)data[12]=0x81;
        write_file(KUI_THEME_ROOT "/assets/header.kuiimg",data,bytes);remount();before=test.writes;
        assert(kui_theme_image_load(&image,&definition,KUI_THEME_HEADER,log_line,NULL)==KUI_THEME_ASSET_FAILED);
        assert(kui_theme_thumbnail_load(&image,&definition,KUI_THEME_HEADER,log_line,NULL)==KUI_THEME_ASSET_FAILED);
        assert(!image.pixels && !image.width && !image.height && test.writes==before);
    }
    free(data);
}
bool theme_install_test(int argc,char **argv);

int main(int argc,char **argv) {
    if(argc<3)return 2;
    struct stat st;if(lstat(argv[1],&st) || !S_ISREG(st.st_mode) || st.st_size%512)return 2;
    test.image=fopen(argv[1],"r+b");if(!test.image)return 2;
    test.blocks=(uint64_t)st.st_size/512;kui_media_set(&media);assert(kui_mount(&fs,log_line));
    if(!strcmp(argv[2],"installed")) {
        bool writable=theme_install_test(argc-3,argv+3);
        assert(writable || !test.writes);
        assert(f_mount(NULL,"0:",0)==FR_OK && fclose(test.image)==0);
        return 0;
    }
    if(argc!=3)return 2;
    kui_theme_settings_default(&defaults);previous=changed=third=defaults;
    strcpy(previous.theme_id,"default");strcpy(previous.header_id,"none");
    strcpy(changed.theme_id,"blue-theme");strcpy(changed.splash_id,"moonlit-muramasa");
    changed.override_mask=1u<<KUI_THEME_ACCENT;changed.palette.colors[KUI_THEME_ACCENT]=0x123456;
    strcpy(third.theme_id,"third");strcpy(third.background_id,"default");
    const char *name=argv[2];
    if(!strcmp(name,"missing")) {
        assert(!strcmp(defaults.theme_id,KUI_THEME_FACTORY_ID));
        check_loaded(&defaults);struct kui_theme_catalog catalog;assert(kui_theme_catalog_scan(&catalog,log_line,NULL));
        assert(catalog.count==1 && !strcmp(catalog.themes[0].id,"default") && !test.writes);
    } else if(!strcmp(name,"seed")) {
        assert(kui_theme_settings_save(&previous,log_line));
        for(unsigned i=0;i<2;i++)write_file(kept[i],sentinel,sizeof(sentinel));
        remount();check_loaded(&previous);check_slot(0,&previous,1);
        FILINFO info;assert(f_stat(slots[1],&info)==FR_NO_FILE);
    } else if(!strcmp(name,"catalog"))check_catalog();
    else if(!strcmp(name,"assets"))check_assets();
    else if(!strcmp(name,"alternate")) {
        assert(kui_theme_settings_save(&changed,log_line));remount();check_loaded(&changed);
        check_slot(0,&previous,1);check_slot(1,&changed,2);
        assert(kui_theme_settings_save(&third,log_line));remount();check_loaded(&third);
        check_slot(0,&third,3);check_slot(1,&changed,2);
    } else if(!strcmp(name,"both-invalid")) {
        write_file(slots[0],"x",1);write_file(slots[1],"x",1);remount();unsigned before=test.writes;
        check_loaded(&defaults);assert(test.writes==before);assert(kui_theme_settings_save(&third,log_line));
        remount();check_slot(0,&third,1);check_loaded(&third);
    } else if(!strcmp(name,"invalid-save")) {
        struct kui_theme_settings invalid=changed;strcpy(invalid.theme_id,"../secret");unsigned before=test.writes;
        assert(!kui_theme_settings_save(&invalid,log_line) && test.writes==before);check_loaded(&previous);
    } else if(!strcmp(name,"overflow")) {
        uint8_t record[256];assert(kui_theme_settings_encode(record,&changed,UINT64_MAX));write_file(slots[1],record,sizeof(record));
        remount();check_loaded(&changed);unsigned before=test.writes;
        assert(!kui_theme_settings_save(&third,log_line) && test.writes==before);check_slot(0,&previous,1);check_slot(1,&changed,UINT64_MAX);
    } else if(!strcmp(name,"read-fail") || !strcmp(name,"save-read-fail")) {
        remount();test.fault="read-fail";unsigned before=test.writes;
        if(!strcmp(name,"read-fail")) {
            struct kui_theme_settings actual=changed;assert(!kui_theme_settings_load(&actual,log_line));equals(&actual,&defaults);
        } else assert(!kui_theme_settings_save(&changed,log_line));
        assert(test.injected && test.writes==before);test.fault=NULL;remount();check_loaded(&previous);check_slot(0,&previous,1);
    } else if(!strcmp(name,"write-fail") || !strcmp(name,"sync-fail") ||
              !strcmp(name,"readback-fail") || !strcmp(name,"readback-corrupt") || !strcmp(name,"readback-substitute")) {
        test.fault=name;test.payload_seen=false;assert(!kui_theme_settings_save(&changed,log_line));assert(test.injected);
        test.fault=NULL;remount();check_slot(0,&previous,1);
        struct kui_theme_settings actual;assert(kui_theme_settings_load(&actual,log_line));
        if(!strcmp(name,"write-fail"))equals(&actual,&previous);
        equals(&actual,!strcmp(actual.theme_id,previous.theme_id)?&previous:&changed);
    } else {
        assert(kui_theme_settings_save(&changed,log_line));damage_newest(name);remount();check_loaded(&previous);check_slot(0,&previous,1);
        assert(kui_theme_settings_save(&third,log_line));remount();check_loaded(&third);check_slot(0,&previous,1);check_slot(1,&third,2);
    }
    if(strcmp(name,"missing"))for(unsigned i=0;i<2;i++)check_file(kept[i],sentinel,sizeof(sentinel));
    assert(f_mount(NULL,"0:",0)==FR_OK && fclose(test.image)==0);
    printf("PASS themes %s: capture/system settings preserved\n",name);return 0;
}
