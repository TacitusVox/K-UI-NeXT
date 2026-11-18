/* SPDX-License-Identifier: GPL-3.0-only */
/* Consume the actual producer outputs with the production manifest/image
 * header parsers and the production streaming CRC implementation. */
#include "kui/theme.h"
#include <stdio.h>
#include <string.h>

int main(int argc,char **argv) {
    if(argc!=2)return 2;
    char path[1024];
    int n=snprintf(path,sizeof(path),"%s/theme.ini",argv[1]);
    if(n<0 || (size_t)n>=sizeof(path))return 2;
    FILE *file=fopen(path,"rb");
    if(!file)return 2;
    unsigned char manifest[KUI_THEME_MANIFEST_MAX+1];
    size_t bytes=fread(manifest,1,sizeof(manifest),file);
    bool failed=ferror(file)!=0;
    if(fclose(file) || failed)return 2;
    struct kui_theme_definition definition;
    if(!kui_theme_parse(&definition,"producer-test",manifest,bytes))return 2;
    unsigned checked=0;
    for(unsigned kind=0;kind<KUI_THEME_ASSET_COUNT;kind++) {
        const char *name=definition.assets[kind];
        if(!name[0] || !strcmp(name,"none"))continue;
        n=snprintf(path,sizeof(path),"%s/%s",argv[1],name);
        if(n<0 || (size_t)n>=sizeof(path))return 2;
        file=fopen(path,"rb");if(!file)return 2;
        if(fseek(file,0,SEEK_END)) {fclose(file);return 2;}
        long size=ftell(file);if(size<0 || fseek(file,0,SEEK_SET)) {fclose(file);return 2;}
        uint8_t header[KUI_THEME_IMAGE_HEADER_SIZE];
        size_t got=fread(header,1,sizeof(header),file);uint32_t expected;
        if(!kui_theme_image_header(header,got,(enum kui_theme_asset_kind)kind,(size_t)size,&expected)) {
            fclose(file);return 2;
        }
        uint8_t buffer[4096];uint32_t crc=0;
        while((got=fread(buffer,1,sizeof(buffer),file))!=0)crc=kui_crc32(crc,buffer,got);
        failed=ferror(file)!=0;
        if(fclose(file) || failed || crc!=expected)return 2;
        checked++;
    }
    printf("Production C reader accepted %u prepared assets for %s\n",checked,definition.name);
    return 0;
}
