/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/theme.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void put32(uint8_t *p,uint32_t value) {for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(value>>(8*i));}
static void record_crc(uint8_t *record) {put32(record+252,kui_crc32(0,record,252));}
static void rejects_record(const void *data,size_t bytes) {
    struct kui_theme_settings out,before;kui_theme_settings_default(&before);out=before;
    uint64_t sequence=73;
    assert(!kui_theme_settings_decode(&out,&sequence,data,bytes));
    assert(!memcmp(&out,&before,sizeof(out)) && sequence==73);
}
static void rejects_manifest(const char *text) {
    struct kui_theme_definition out,before;kui_theme_definition_default(&before);out=before;
    assert(!kui_theme_parse(&out,"test",text,strlen(text)));
    assert(!memcmp(&out,&before,sizeof(out)));
}
int main(void) {
    const uint16_t old[]={0x0864,0x10c6,0x494b,0x318a,0xf7bf,0xadd9,0x675e,0xf3fb,0x8ef6,0xfda9,0xf7bf};
    struct kui_theme_palette palette;kui_theme_palette_default(&palette);
    for(unsigned i=0;i<KUI_THEME_COLOR_COUNT;i++)assert(kui_theme_rgb565(palette.colors[i])==old[i]);
    for(unsigned i=0;i<=65535u;i++)assert(kui_theme_rgb565(kui_theme_rgb888((uint16_t)i))==i);
    assert(kui_theme_rgb565(0xff0000)==0xf800 && kui_theme_rgb565(0x00ff00)==0x07e0 && kui_theme_rgb565(0x0000ff)==31);
    assert(kui_theme_id_valid("moonlit-muramasa") && kui_theme_id_valid("KUI_3"));
    assert(kui_theme_id_valid("default") && kui_theme_id_valid("none"));
    const char *badids[]={"",".","..","../other","a/b","a\\b","0:a"," hello","hello world","-bad","a.b",
        "Default","DEFAULT","dEfAuLt","NONE","None","nOnE","01234567890123456789012345678901xx"};
    for(unsigned i=0;i<sizeof(badids)/sizeof(badids[0]);i++)assert(!kui_theme_id_valid(badids[i]));

    struct kui_theme_settings settings;kui_theme_settings_default(&settings);
    assert(kui_theme_settings_valid(&settings));
    strcpy(settings.theme_id,"moonlit-muramasa");strcpy(settings.splash_id,"default");
    strcpy(settings.header_id,"none");strcpy(settings.background_id,"second-pack");
    settings.override_mask=(1u<<KUI_THEME_ACCENT)|(1u<<KUI_THEME_SELECTION_TEXT);
    settings.palette.colors[KUI_THEME_ACCENT]=0x123456;settings.palette.colors[KUI_THEME_SELECTION_TEXT]=0xfedcba;
    assert(!strcmp(kui_theme_component_id(&settings,KUI_THEME_SPLASH),"default"));
    assert(!strcmp(kui_theme_component_id(&settings,KUI_THEME_HEADER),"none"));
    assert(!strcmp(kui_theme_component_id(&settings,KUI_THEME_BACKDROP),"second-pack"));
    memset(settings.splash_id,0,sizeof(settings.splash_id));
    assert(!strcmp(kui_theme_component_id(&settings,KUI_THEME_SPLASH),settings.theme_id));
    uint8_t record[KUI_THEME_SETTINGS_RECORD_SIZE],damaged[KUI_THEME_SETTINGS_RECORD_SIZE+1];
    assert(kui_theme_settings_encode(record,&settings,UINT64_C(0x0807060504030201)));
    assert(!memcmp(record,"KUITHE01\1\0\0\0\0\1\0\0\1\2\3\4\5\6\7\10",24));
    struct kui_theme_settings decoded;uint64_t sequence=0;
    assert(kui_theme_settings_decode(&decoded,&sequence,record,sizeof(record)));
    assert(sequence==UINT64_C(0x0807060504030201) && !memcmp(&settings,&decoded,sizeof(settings)));
    for(unsigned bit=0;bit<sizeof(record)*8;bit++) {
        memcpy(damaged,record,sizeof(record));damaged[bit/8]^=(uint8_t)(1u<<(bit%8));rejects_record(damaged,sizeof(record));
    }
    for(size_t bytes=0;bytes<sizeof(record);bytes++)rejects_record(record,bytes);
    memcpy(damaged,record,sizeof(record));damaged[sizeof(record)]=0;rejects_record(damaged,sizeof(damaged));
    const unsigned corrupt[]={8,12,200,251};
    for(unsigned i=0;i<sizeof(corrupt)/sizeof(corrupt[0]);i++) {
        memcpy(damaged,record,sizeof(record));damaged[corrupt[i]]++;record_crc(damaged);rejects_record(damaged,sizeof(record));
    }
    memcpy(damaged,record,sizeof(record));memset(damaged+16,0,8);record_crc(damaged);rejects_record(damaged,sizeof(record));
    memcpy(damaged,record,sizeof(record));damaged[155]=0x80;record_crc(damaged);rejects_record(damaged,sizeof(record));
    memcpy(damaged,record,sizeof(record));damaged[159]=1;record_crc(damaged);rejects_record(damaged,sizeof(record));
    memcpy(damaged,record,sizeof(record));memset(damaged+24,'x',32);record_crc(damaged);rejects_record(damaged,sizeof(record));
    memcpy(damaged,record,sizeof(record));damaged[54]='x';record_crc(damaged);rejects_record(damaged,sizeof(record));
    struct kui_theme_settings invalid=settings;memset(invalid.header_id,'x',sizeof(invalid.header_id));
    assert(!kui_theme_settings_valid(&invalid));
    invalid=settings;strcpy(invalid.theme_id,"none");assert(!kui_theme_settings_valid(&invalid));
    invalid=settings;strcpy(invalid.theme_id,"Default");assert(!kui_theme_settings_valid(&invalid));
    invalid=settings;strcpy(invalid.header_id,"NONE");assert(!kui_theme_settings_valid(&invalid));
    invalid=settings;invalid.override_mask=1u<<31;assert(!kui_theme_settings_valid(&invalid));
    invalid=settings;invalid.palette.colors[0]=0x1000000;assert(!kui_theme_settings_valid(&invalid));
    memcpy(damaged,record,sizeof(record));assert(!kui_theme_settings_encode(damaged,&settings,0));
    assert(!memcmp(damaged,record,sizeof(record)));

    static const char manifest[]=
        "; comment\r\n[theme]\r\nversion = 1\r\nname = Moonlit Muramasa\r\nauthor = TacitusVox\r\n"
        "splash = splash.kuiimg\r\nheader=none\r\n[colors]\r\nbackground=#010203\r\nselection_text=#aBcDeF\r\n";
    struct kui_theme_definition definition;
    assert(kui_theme_parse(&definition,"moonlit-muramasa",manifest,sizeof(manifest)-1));
    assert(!strcmp(definition.id,"moonlit-muramasa") && !strcmp(definition.name,"Moonlit Muramasa"));
    assert(!strcmp(definition.author,"TacitusVox") && !strcmp(definition.assets[0],"splash.kuiimg"));
    assert(!strcmp(definition.assets[1],"none") && !definition.assets[2][0]);
    assert(definition.palette.colors[KUI_THEME_BACKGROUND]==0x010203);
    assert(definition.palette.colors[KUI_THEME_SELECTION_TEXT]==0xabcdef);
    kui_theme_effective_palette(&palette,&definition,&settings);
    assert(palette.colors[KUI_THEME_BACKGROUND]==0x010203);
    assert(palette.colors[KUI_THEME_ACCENT]==0x123456 && palette.colors[KUI_THEME_SELECTION_TEXT]==0xfedcba);
    assert(kui_theme_parse(&definition,"plain","[theme]\nversion=1",17));
    assert(!strcmp(definition.name,"plain") && !definition.author[0]);
    const char *bad[]={
        "[theme]\nname=x\n", "[theme]\nversion=2\n", "[theme]\nversion=1\nversion=1\n",
        "[theme]\nversion=1\nsplash=../secret\n", "[theme]\nversion=1\nsplash=dir/file\n",
        "[theme]\nversion=1\nsplash=a..b\n", "[theme]\nversion=1\nsplash=/bad\n",
        "[theme]\nversion=1\nsplash=0:bad\n", "[theme]\nversion=1\nsplash=a\\b\n",
        "[theme]\nversion=1\nunknown=1\n", "[unknown]\nversion=1\n", "version=1\n",
        "[theme]\nversion=1\nname=\n", "[theme]\nversion=1\nname=one\nname=two\n",
        "[theme]\nversion=1\n[colors]\ntext=#12345\n", "[theme]\nversion=1\n[colors]\ntext=#12gh56\n",
        "[theme]\nversion=1\n[colors]\ntext=#123456\ntext=#654321\n",
        "[theme]\nversion=1\n[colors]\ntext=0xff00ff\n", "[theme]\nversion=1\n[colors]\nother=#ffffff\n"
    };
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++)rejects_manifest(bad[i]);
    char longtext[KUI_THEME_MANIFEST_MAX+1];memset(longtext,'a',sizeof(longtext));
    assert(!kui_theme_parse(&definition,"plain",longtext,sizeof(longtext)));
    longtext[sizeof(longtext)-1]=0;rejects_manifest(longtext);
    assert(!kui_theme_parse(&definition,"plain","[theme]\nversion=1\0",18));

    uint8_t header[32]={0};memcpy(header,"KUITEX01",8);put32(header+8,1);
    header[12]=0x80;header[13]=2;header[14]=100;
    put32(header+16,128000);put32(header+20,0x12345678);uint32_t crc=0;
    assert(kui_theme_image_header(header,sizeof(header),KUI_THEME_HEADER,128032,&crc) && crc==0x12345678);
    assert(!kui_theme_image_header(header,sizeof(header),KUI_THEME_SPLASH,128032,&crc));
    assert(!kui_theme_image_header(header,sizeof(header),KUI_THEME_HEADER,128033,&crc));
    header[31]=1;assert(!kui_theme_image_header(header,sizeof(header),KUI_THEME_HEADER,128032,&crc));header[31]=0;
    header[14]=0xe0;header[15]=1;put32(header+16,614400);
    assert(kui_theme_image_header(header,sizeof(header),KUI_THEME_SPLASH,614432,&crc));
    assert(kui_theme_image_header(header,sizeof(header),KUI_THEME_BACKDROP,614432,&crc));
    header[12]=0xff;assert(!kui_theme_image_header(header,sizeof(header),KUI_THEME_SPLASH,614432,&crc));
    assert(!kui_theme_image_header(header,sizeof(header),(enum kui_theme_asset_kind)-1,614432,&crc));
    puts("PASS themes: original palette, selectors, persistent record corruption, strict manifests and bounded image headers");
    return 0;
}
