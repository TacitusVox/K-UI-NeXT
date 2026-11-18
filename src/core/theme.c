/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/theme.h"
#include <string.h>

static const char *const color_keys[KUI_THEME_COLOR_COUNT]={
    "background","panel","selection","edge","text","muted","accent",
    "accent2","success","warning","selection_text"
};
static const char *const color_names[KUI_THEME_COLOR_COUNT]={
    "Background","Panels","Selection","Borders","Text","Muted text","Accent",
    "Second accent","Success","Warning","Selected text"
};
static const uint16_t original_palette[KUI_THEME_COLOR_COUNT]={
    0x0864,0x10c6,0x494b,0x318a,0xf7bf,0xadd9,0x675e,0xf3fb,0x8ef6,0xfda9,0xf7bf
};
/* The same colors as the shipped factory pack; also available before SD is mounted. */
static const struct kui_theme_palette factory_palette = {{
    0x080f23,0x101f35,0x20455e,0x395369,0xe7f1ff,0xa5bbd1,
    0x6edded,0xff7f86,0x91dec0,0xffd18a,0xf6faff
}};
static uint16_t get16(const uint8_t *p) {return (uint16_t)(p[0]|(uint16_t)p[1]<<8);}
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static uint64_t get64(const uint8_t *p) {return get32(p)|((uint64_t)get32(p+4)<<32);}
static void put32(uint8_t *p,uint32_t value) {for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(value>>(i*8));}
static void put64(uint8_t *p,uint64_t value) {put32(p,(uint32_t)value);put32(p+4,(uint32_t)(value>>32));}
static bool alnum_ascii(unsigned char c) {
    return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9');
}
static bool token(const char *text,size_t limit,bool filename) {
    if(!text || !alnum_ascii((unsigned char)text[0]))return false;
    for(size_t i=0;i<limit;i++) {
        unsigned char c=(unsigned char)text[i];
        if(!c)return true;
        if(!alnum_ascii(c) && c!='-' && c!='_' && !(filename&&c=='.'))return false;
        if(c=='.' && i && text[i-1]=='.')return false;
    }
    return false;
}
bool kui_theme_id_valid(const char *id) {
    if(!token(id,KUI_THEME_ID_BYTES,false))return false;
    /* FAT names are case insensitive. Reserve every case spelling so direct
     * selection cannot turn a built-in selector into an on-disk theme. */
    static const char *const reserved[]={"default","none"};
    for(unsigned n=0;n<sizeof(reserved)/sizeof(reserved[0]);n++) {
        unsigned i=0;
        while(id[i] && reserved[n][i]) {
            unsigned char c=(unsigned char)id[i];
            if(c>='A' && c<='Z')c=(unsigned char)(c+'a'-'A');
            if(c!=(unsigned char)reserved[n][i])break;
            i++;
        }
        if(!id[i] && !reserved[n][i] && strcmp(id,reserved[n]))return false;
    }
    return true;
}
uint16_t kui_theme_rgb565(uint32_t rgb) {
    return (uint16_t)(((rgb>>8)&0xf800u)|((rgb>>5)&0x07e0u)|((rgb>>3)&0x001fu));
}
uint32_t kui_theme_rgb888(uint16_t color) {
    unsigned r=(color>>11)&31u,g=(color>>5)&63u,b=color&31u;
    return ((r<<3|r>>2)<<16)|((g<<2|g>>4)<<8)|(b<<3|b>>2);
}
const char *kui_theme_color_key(unsigned role) {return role<KUI_THEME_COLOR_COUNT?color_keys[role]:"invalid";}
const char *kui_theme_color_name(unsigned role) {return role<KUI_THEME_COLOR_COUNT?color_names[role]:"Invalid";}
void kui_theme_palette_default(struct kui_theme_palette *out) {
    if(out)for(unsigned i=0;i<KUI_THEME_COLOR_COUNT;i++)out->colors[i]=kui_theme_rgb888(original_palette[i]);
}
void kui_theme_definition_default(struct kui_theme_definition *out) {
    if(!out)return;
    memset(out,0,sizeof(*out));
    memcpy(out->id,"default",8);strcpy(out->name,"K-UI Original");memcpy(out->author,"TacitusVox",11);
    kui_theme_palette_default(&out->palette);
}
void kui_theme_settings_default(struct kui_theme_settings *out) {
    if(!out)return;
    memset(out,0,sizeof(*out));strcpy(out->theme_id,KUI_THEME_FACTORY_ID);
    out->palette=factory_palette;
}
static bool selector(const char *id) {return id && (!id[0] || kui_theme_id_valid(id));}
bool kui_theme_settings_valid(const struct kui_theme_settings *s) {
    if(!s || !kui_theme_id_valid(s->theme_id) || !strcmp(s->theme_id,"none") ||
       !selector(s->splash_id) || !selector(s->header_id) || !selector(s->background_id) ||
       s->override_mask&~((UINT32_C(1)<<KUI_THEME_COLOR_COUNT)-1u))return false;
    for(unsigned i=0;i<KUI_THEME_COLOR_COUNT;i++)if(s->palette.colors[i]>0xffffffu)return false;
    return true;
}
const char *kui_theme_component_id(const struct kui_theme_settings *s,enum kui_theme_asset_kind kind) {
    if(!s || (unsigned)kind>=KUI_THEME_ASSET_COUNT)return "default";
    const char *id=kind==KUI_THEME_SPLASH?s->splash_id:kind==KUI_THEME_HEADER?s->header_id:s->background_id;
    return id[0]?id:s->theme_id;
}
void kui_theme_effective_palette(struct kui_theme_palette *out,const struct kui_theme_definition *d,
                                 const struct kui_theme_settings *s) {
    if(!out)return;
    if(d)*out=d->palette;
    else if(s && !strcmp(s->theme_id,KUI_THEME_FACTORY_ID))*out=factory_palette;
    else kui_theme_palette_default(out);
    if(s)for(unsigned i=0;i<KUI_THEME_COLOR_COUNT;i++)
        if(s->override_mask&(UINT32_C(1)<<i))out->colors[i]=s->palette.colors[i];
}
static char *trim(char *text) {
    while(*text==' ' || *text=='\t' || *text=='\r')text++;
    size_t n=strlen(text);
    while(n && (text[n-1]==' ' || text[n-1]=='\t' || text[n-1]=='\r'))text[--n]=0;
    return text;
}
static bool copy_text(char *out,size_t capacity,const char *text) {
    size_t n=strlen(text);
    if(!n || n>=capacity)return false;
    for(size_t i=0;i<n;i++)if((unsigned char)text[i]<32u || (unsigned char)text[i]>126u)return false;
    memset(out,0,capacity);memcpy(out,text,n+1);return true;
}
static int hex(unsigned char c) {
    if(c>='0'&&c<='9')return c-'0';
    if(c>='a'&&c<='f')return c-'a'+10;
    if(c>='A'&&c<='F')return c-'A'+10;
    return -1;
}
bool kui_theme_parse(struct kui_theme_definition *out,const char *id,const void *text,size_t bytes) {
    if(!out || !kui_theme_id_valid(id) || !text || !bytes || bytes>KUI_THEME_MANIFEST_MAX)return false;
    const unsigned char *source=text;
    for(size_t i=0;i<bytes;i++)
        if((source[i]<32u && source[i]!='\r' && source[i]!='\n' && source[i]!='\t') || source[i]>126u)return false;
    struct kui_theme_definition parsed;kui_theme_definition_default(&parsed);
    memset(parsed.id,0,sizeof(parsed.id));memcpy(parsed.id,id,strlen(id)+1);
    memset(parsed.name,0,sizeof(parsed.name));memcpy(parsed.name,id,strlen(id)+1);
    memset(parsed.author,0,sizeof(parsed.author));
    unsigned section=0;uint32_t seen=0;
    for(size_t pos=0;pos<bytes;) {
        char line[192];size_t length=0;
        while(pos<bytes && source[pos]!='\n') {
            if(length+1>=sizeof(line))return false;
            line[length++]=(char)source[pos++];
        }
        if(pos<bytes)pos++;
        line[length]=0;char *key=trim(line);
        if(!*key || *key==';' || *key=='#')continue;
        if(*key=='[') {
            if(!strcmp(key,"[theme]"))section=1;
            else if(!strcmp(key,"[colors]"))section=2;
            else return false;
            continue;
        }
        char *equals=strchr(key,'=');if(!equals || !section)return false;
        *equals=0;char *value=trim(equals+1);key=trim(key);
        unsigned field=32;
        if(section==1) {
            static const char *const keys[]={"version","name","author","splash","header","background","type"};
            for(unsigned i=0;i<sizeof(keys)/sizeof(keys[0]);i++)if(!strcmp(key,keys[i])){field=i;break;}
            if(field==32 || (seen&(UINT32_C(1)<<field)))return false;
            if(field==0) {if(strcmp(value,"1"))return false;}
            else if(field==6) {
                if(strcmp(value,"banner"))return false;
                parsed.banner_only=true;
            } else if(field==1 || field==2) {
                char *target=field==1?parsed.name:parsed.author;
                if(!copy_text(target,KUI_THEME_NAME_BYTES,value))return false;
            } else {
                if(!token(value,KUI_THEME_FILE_BYTES,true))return false;
                memcpy(parsed.assets[field-3],value,strlen(value)+1);
            }
        } else {
            for(unsigned i=0;i<KUI_THEME_COLOR_COUNT;i++)if(!strcmp(key,color_keys[i])){field=i+7;break;}
            if(field==32 || (seen&(UINT32_C(1)<<field)) || strlen(value)!=7 || value[0]!='#')return false;
            uint32_t rgb=0;
            for(unsigned i=1;i<7;i++){int digit=hex((unsigned char)value[i]);if(digit<0)return false;rgb=(rgb<<4)|(unsigned)digit;}
            parsed.palette.colors[field-7]=rgb;
        }
        seen|=UINT32_C(1)<<field;
    }
    if(!(seen&1u))return false;
    if(parsed.banner_only && (!(seen&(1u<<4)) || !strcmp(parsed.assets[KUI_THEME_HEADER],"none") ||
       (seen&((1u<<3)|(1u<<5)))))return false;
    *out=parsed;return true;
}
bool kui_theme_settings_encode(uint8_t out[KUI_THEME_SETTINGS_RECORD_SIZE],
                               const struct kui_theme_settings *s,uint64_t sequence) {
    if(!out || !sequence || !kui_theme_settings_valid(s))return false;
    memset(out,0,KUI_THEME_SETTINGS_RECORD_SIZE);memcpy(out,"KUITHE01",8);
    put32(out+8,1);put32(out+12,KUI_THEME_SETTINGS_RECORD_SIZE);put64(out+16,sequence);
    memcpy(out+24,s->theme_id,strlen(s->theme_id));memcpy(out+56,s->splash_id,strlen(s->splash_id));
    memcpy(out+88,s->header_id,strlen(s->header_id));memcpy(out+120,s->background_id,strlen(s->background_id));
    put32(out+152,s->override_mask);
    for(unsigned i=0;i<KUI_THEME_COLOR_COUNT;i++)put32(out+156+i*4,s->palette.colors[i]);
    put32(out+252,kui_crc32(0,out,252));return true;
}
bool kui_theme_settings_decode(struct kui_theme_settings *out,uint64_t *sequence,const void *record,size_t bytes) {
    if(!out || !sequence || !record || bytes!=KUI_THEME_SETTINGS_RECORD_SIZE)return false;
    const uint8_t *p=record;
    if(memcmp(p,"KUITHE01",8) || get32(p+8)!=1 || get32(p+12)!=bytes || !get64(p+16) ||
       get32(p+252)!=kui_crc32(0,p,252))return false;
    for(unsigned i=200;i<252;i++)if(p[i])return false;
    for(unsigned slot=0;slot<4;slot++) {
        const uint8_t *id=p+24+slot*32;unsigned end=0;
        while(end<32 && id[end])end++;
        if(end==32)return false;
        for(;end<32;end++)if(id[end])return false;
    }
    struct kui_theme_settings decoded;memset(&decoded,0,sizeof(decoded));
    memcpy(decoded.theme_id,p+24,32);memcpy(decoded.splash_id,p+56,32);
    memcpy(decoded.header_id,p+88,32);memcpy(decoded.background_id,p+120,32);
    decoded.override_mask=get32(p+152);
    for(unsigned i=0;i<KUI_THEME_COLOR_COUNT;i++)decoded.palette.colors[i]=get32(p+156+i*4);
    if(!kui_theme_settings_valid(&decoded))return false;
    *out=decoded;*sequence=get64(p+16);return true;
}
bool kui_theme_image_header(const void *header,size_t bytes,enum kui_theme_asset_kind kind,
                            size_t file_bytes,uint32_t *crc) {
    if(!header || !crc || bytes!=KUI_THEME_IMAGE_HEADER_SIZE || (unsigned)kind>=KUI_THEME_ASSET_COUNT)return false;
    const uint8_t *p=header;
    unsigned height=kind==KUI_THEME_HEADER?KUI_THEME_HEADER_HEIGHT:KUI_THEME_HEIGHT;
    size_t payload=KUI_THEME_WIDTH*height*2u;
    if(memcmp(p,"KUITEX01",8) || get32(p+8)!=1 || get16(p+12)!=KUI_THEME_WIDTH ||
       get16(p+14)!=height || get32(p+16)!=payload || file_bytes!=KUI_THEME_IMAGE_HEADER_SIZE+payload)return false;
    for(unsigned i=24;i<32;i++)if(p[i])return false;
    *crc=get32(p+20);return true;
}
