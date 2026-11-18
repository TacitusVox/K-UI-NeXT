/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/theme.h"
#include "theme_thumbnail.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const settings_paths[]={KUI_THEME_SETTINGS_PATH_A,KUI_THEME_SETTINGS_PATH_B};
struct theme_slot {
    struct kui_theme_settings settings;
    uint64_t sequence;
    bool valid;
    uint8_t record[KUI_THEME_SETTINGS_RECORD_SIZE];
};
static bool read_slot(unsigned index,struct theme_slot *slot,kui_log_fn log) {
    memset(slot,0,sizeof(*slot));FIL file;
    FRESULT r=f_open(&file,settings_paths[index],FA_READ);
    if(r==FR_NO_FILE || r==FR_NO_PATH)return true;
    if(r!=FR_OK) {if(log)log("Theme preferences unavailable: FatFs=%u",(unsigned)r);return false;}
    bool sized=f_size(&file)==sizeof(slot->record);UINT got=0;
    if(sized)r=f_read(&file,slot->record,sizeof(slot->record),&got);
    FRESULT closed=f_close(&file);
    if(r!=FR_OK || closed!=FR_OK)return false;
    if(sized && got==sizeof(slot->record))
        slot->valid=kui_theme_settings_decode(&slot->settings,&slot->sequence,slot->record,sizeof(slot->record));
    if(!slot->valid && log)log("Ignoring invalid theme preferences: %s",settings_paths[index]+2);
    return true;
}
static int newest(const struct theme_slot slots[2]) {
    if(!slots[0].valid)return slots[1].valid?1:-1;
    if(!slots[1].valid)return 0;
    return slots[1].sequence>slots[0].sequence?1:0;
}
bool kui_theme_settings_load(struct kui_theme_settings *out,kui_log_fn log) {
    if(!out)return false;
    kui_theme_settings_default(out);struct theme_slot slots[2];
    if(!read_slot(0,&slots[0],log) || !read_slot(1,&slots[1],log))return false;
    int chosen=newest(slots);
    if(chosen>=0)*out=slots[chosen].settings;
    return true;
}
static bool make_parents(kui_log_fn log) {
    static const char *const parents[]={"0:/KUI","0:/KUI/apps","0:/KUI/apps/system"};
    for(unsigned i=0;i<sizeof(parents)/sizeof(parents[0]);i++) {
        FILINFO info;FRESULT r=f_stat(parents[i],&info);
        if(r==FR_OK && (info.fattrib&AM_DIR))continue;
        if(r==FR_NO_FILE || r==FR_NO_PATH)r=f_mkdir(parents[i]);else if(r==FR_OK)r=FR_DENIED;
        if(r!=FR_OK) {if(log)log("Cannot create theme preferences folder: FatFs=%u",(unsigned)r);return false;}
    }
    return true;
}
bool kui_theme_settings_save(const struct kui_theme_settings *settings,kui_log_fn log) {
    if(!kui_theme_settings_valid(settings))return false;
    struct theme_slot slots[2];
    if(!read_slot(0,&slots[0],log) || !read_slot(1,&slots[1],log))return false;
    int current=newest(slots);
    if(current>=0 && slots[current].sequence==UINT64_MAX) {
        if(log)log("Theme preference sequence exhausted; save refused");
        return false;
    }
    uint64_t sequence=current<0?1:slots[current].sequence+1;
    unsigned target=current<0?0:(unsigned)(1-current);
    uint8_t record[KUI_THEME_SETTINGS_RECORD_SIZE];
    if(!kui_theme_settings_encode(record,settings,sequence) || !make_parents(log))return false;
    FIL file;FRESULT r=f_open(&file,settings_paths[target],FA_WRITE|FA_CREATE_ALWAYS);
    if(r!=FR_OK)return false;
    UINT written=0;r=f_write(&file,record,sizeof(record),&written);
    bool ok=r==FR_OK && written==sizeof(record);
    if(ok){r=f_sync(&file);ok=r==FR_OK;}
    FRESULT closed=f_close(&file);
    if(!ok || closed!=FR_OK) {
        if(log)log("Theme preference save failed; previous slot retained");
        return false;
    }
    struct theme_slot saved;
    if(!read_slot(target,&saved,log) || !saved.valid || memcmp(saved.record,record,sizeof(record))) {
        if(log)log("Theme preference verification failed; previous slot retained");
        return false;
    }
    if(log)log("Saved theme preferences, sequence=%" PRIu64,sequence);
    return true;
}
bool kui_theme_definition_load(struct kui_theme_definition *out,const char *id,kui_log_fn log) {
    if(!out || !kui_theme_id_valid(id) || !strcmp(id,"none"))return false;
    if(!strcmp(id,"default")){kui_theme_definition_default(out);return true;}
    char path[128];int length=snprintf(path,sizeof(path),KUI_THEME_ROOT "/%s/theme.ini",id);
    if(length<0 || (size_t)length>=sizeof(path))return false;
    FIL file;FRESULT r=f_open(&file,path,FA_READ);
    if(r!=FR_OK){if(log)log("Theme %s unavailable: FatFs=%u",id,(unsigned)r);return false;}
    FSIZE_t bytes=f_size(&file);UINT got=0;
    char text[KUI_THEME_MANIFEST_MAX];
    bool sized=bytes>0 && bytes<=sizeof(text);
    if(sized)r=f_read(&file,text,(UINT)bytes,&got);
    FRESULT closed=f_close(&file);
    bool ok=sized && r==FR_OK && closed==FR_OK && got==bytes && kui_theme_parse(out,id,text,got);
    if(!ok && log)log("Theme %s has an invalid/unreadable manifest",id);
    return ok;
}
static int compare_ascii(const char *a,const char *b) {
    while(*a && *b) {
        unsigned x=(unsigned char)*a++,y=(unsigned char)*b++;
        if(x>='A'&&x<='Z')x+='a'-'A';
        if(y>='A'&&y<='Z')y+='a'-'A';
        if(x!=y)return x<y?-1:1;
    }
    return *a?1:*b?-1:0;
}
static int compare_theme(const struct kui_theme_definition *a,const struct kui_theme_definition *b) {
    int order=compare_ascii(a->name,b->name);
    if(!order)order=compare_ascii(a->id,b->id);
    return order?order:strcmp(a->id,b->id);
}
static void catalog_insert(struct kui_theme_catalog *out,const struct kui_theme_definition *definition) {
    for(unsigned i=0;i<out->count;i++)if(!compare_ascii(out->themes[i].id,definition->id))return;
    unsigned at=1;
    while(at<out->count && compare_theme(&out->themes[at],definition)<0)at++;
    if(out->count==KUI_THEME_CATALOG_MAX) {
        out->truncated=true;if(at>=KUI_THEME_CATALOG_MAX)return;
    } else out->count++;
    for(unsigned i=out->count-1;i>at;i--)out->themes[i]=out->themes[i-1];
    out->themes[at]=*definition;
}
bool kui_theme_catalog_scan(struct kui_theme_catalog *out,kui_log_fn log,kui_cancel_fn cancelled) {
    if(!out)return false;
    memset(out,0,sizeof(*out));kui_theme_definition_default(&out->themes[0]);out->count=1;
    if(cancelled && cancelled())return false;
    DIR directory;FRESULT r=f_opendir(&directory,KUI_THEME_ROOT);
    if(r==FR_NO_PATH || r==FR_NO_FILE)return true;
    if(r!=FR_OK)return false;
    bool ok=true;
    for(;;) {
        if(cancelled && cancelled()){ok=false;break;}
        FILINFO info;r=f_readdir(&directory,&info);
        if(r!=FR_OK){ok=false;break;}
        if(!info.fname[0])break;
        if(!(info.fattrib&AM_DIR) || !kui_theme_id_valid(info.fname) ||
           !compare_ascii(info.fname,"default") || !compare_ascii(info.fname,"none"))continue;
        struct kui_theme_definition definition;
        if(kui_theme_definition_load(&definition,info.fname,log))catalog_insert(out,&definition);
    }
    FRESULT closed=f_closedir(&directory);
    return ok && closed==FR_OK;
}
void kui_theme_image_release(struct kui_theme_image *image) {
    if(!image)return;
    free(image->pixels);memset(image,0,sizeof(*image));
}
static void pixels_from_le(uint16_t *pixels,size_t count) {
    const uint16_t endian=1;
    if(!*(const uint8_t *)&endian)for(size_t i=0;i<count;i++) {
        const uint8_t *p=(const uint8_t *)pixels+i*2;
        pixels[i]=(uint16_t)(p[0]|(uint16_t)p[1]<<8);
    }
}
/* The full image is already open at its pixel payload, so a stale, damaged
 * or absent preview can fall back without re-opening or rewinding it. */
static enum kui_theme_asset_status read_prepared_thumbnail(const char *asset_path,
        enum kui_theme_asset_kind kind,bool banner_only,uint32_t source_crc,uint16_t *pixels,size_t bytes,
        kui_cancel_fn cancelled) {
    char path[176];
    int length=snprintf(path,sizeof(path),"%s.thumb",asset_path);
    if(length<0 || (size_t)length>=sizeof(path))return KUI_THEME_ASSET_FAILED;
    FIL file;
    if(f_open(&file,path,FA_READ)!=FR_OK)return KUI_THEME_ASSET_FAILED;
    uint8_t header[KUI_THEME_IMAGE_HEADER_SIZE];UINT got=0;
    uint32_t expected=0,actual=0;
    bool ok=f_size(&file)==sizeof(header)+bytes &&
        f_read(&file,header,sizeof(header),&got)==FR_OK && got==sizeof(header) &&
        kui_theme_thumbnail_header(header,sizeof(header),kind,banner_only,sizeof(header)+bytes,source_crc,&expected);
    bool stopped=false;
    for(size_t offset=0;ok && offset<bytes;) {
        if(cancelled && cancelled()){stopped=true;break;}
        UINT count=(UINT)(bytes-offset<16384u?bytes-offset:16384u);
        uint8_t *target=(uint8_t *)pixels+offset;
        ok=f_read(&file,target,count,&got)==FR_OK && got==count;
        if(ok) {actual=kui_crc32(actual,target,count);offset+=count;}
    }
    FRESULT closed=f_close(&file);
    if(stopped)return KUI_THEME_ASSET_CANCELLED;
    if(!ok || closed!=FR_OK || actual!=expected)return KUI_THEME_ASSET_FAILED;
    pixels_from_le(pixels,bytes/2u);
    return KUI_THEME_ASSET_READY;
}
/* Standalone banners put the fully branded graphic in the left 320 columns.
 * Preserve those RGB565 bytes at native scale; the CRC still covers both halves. */
static void copy_banner_region(struct theme_thumbnail *thumb,const uint8_t *data,unsigned bytes) {
    unsigned remaining=bytes/2u;
    while(remaining) {
        unsigned row=thumb->consumed/KUI_THEME_WIDTH,x=thumb->consumed%KUI_THEME_WIDTH;
        unsigned count=KUI_THEME_WIDTH-x;
        if(count>remaining)count=remaining;
        if(x<KUI_THEME_THUMB_WIDTH) {
            unsigned width=KUI_THEME_THUMB_WIDTH-x;
            if(width>count)width=count;
            memcpy(thumb->pixels+row*KUI_THEME_THUMB_WIDTH+x,data,width*sizeof(*thumb->pixels));
        }
        thumb->consumed+=count;
        data+=count*2u;
        remaining-=count;
    }
}
static enum kui_theme_asset_status load_image(struct kui_theme_image *out,
    const struct kui_theme_definition *definition,enum kui_theme_asset_kind kind,
    kui_log_fn log,kui_cancel_fn cancelled,bool thumbnail) {
    if(!out || out->pixels || !definition || (unsigned)kind>=KUI_THEME_ASSET_COUNT ||
       !kui_theme_id_valid(definition->id))return KUI_THEME_ASSET_FAILED;
    if(cancelled && cancelled())return KUI_THEME_ASSET_CANCELLED;
    const char *filename=definition->assets[kind];
    if(!filename[0] || !strcmp(definition->id,"default"))return KUI_THEME_ASSET_DEFAULT;
    /* Definitions normally come from the strict parser. Recheck the full
     * filename here so a constructed definition cannot escape its directory. */
    if(!((filename[0]>='a'&&filename[0]<='z')||(filename[0]>='A'&&filename[0]<='Z')||
         (filename[0]>='0'&&filename[0]<='9')))return KUI_THEME_ASSET_FAILED;
    bool terminated=false;
    for(unsigned i=0;i<KUI_THEME_FILE_BYTES;i++) {
        unsigned char c=(unsigned char)filename[i];
        if(!c){terminated=true;break;}
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.') ||
           (i && c=='.' && filename[i-1]=='.'))return KUI_THEME_ASSET_FAILED;
    }
    if(!terminated)return KUI_THEME_ASSET_FAILED;
    if(!strcmp(filename,"none"))return KUI_THEME_ASSET_DISABLED;
    char path[160];int length=snprintf(path,sizeof(path),KUI_THEME_ROOT "/%s/%s",definition->id,filename);
    if(length<0 || (size_t)length>=sizeof(path))return KUI_THEME_ASSET_FAILED;
    FIL file;FRESULT r=f_open(&file,path,FA_READ);
    if(r!=FR_OK){if(log)log("Theme asset unavailable: %s",path+2);return KUI_THEME_ASSET_FAILED;}
    uint8_t header[KUI_THEME_IMAGE_HEADER_SIZE];UINT got=0;
    r=f_read(&file,header,sizeof(header),&got);uint32_t expected=0;
    FSIZE_t file_bytes=f_size(&file);
    bool ok=r==FR_OK && got==sizeof(header) && file_bytes<=KUI_THEME_WIDTH*KUI_THEME_HEIGHT*2u+sizeof(header) &&
        kui_theme_image_header(header,sizeof(header),kind,(size_t)file_bytes,&expected);
    unsigned height=kind==KUI_THEME_HEADER?KUI_THEME_HEADER_HEIGHT:KUI_THEME_HEIGHT;
    size_t bytes=KUI_THEME_WIDTH*height*2u;
    bool native_banner=thumbnail && kind==KUI_THEME_HEADER && definition->banner_only;
    unsigned out_width=thumbnail?KUI_THEME_THUMB_WIDTH:KUI_THEME_WIDTH;
    unsigned out_height=thumbnail && !native_banner?height/2u:height;
    uint16_t *pixels=ok?malloc(out_width*out_height*sizeof(*pixels)):NULL;
    if(thumbnail && pixels) {
        enum kui_theme_asset_status ready=read_prepared_thumbnail(path,kind,definition->banner_only,expected,pixels,
            out_width*out_height*sizeof(*pixels),cancelled);
        if(ready!=KUI_THEME_ASSET_FAILED) {
            FRESULT closed=f_close(&file);
            if(ready==KUI_THEME_ASSET_READY && closed==FR_OK) {
                out->pixels=pixels;out->width=out_width;out->height=out_height;
                return ready;
            }
            free(pixels);
            return ready==KUI_THEME_ASSET_CANCELLED?ready:KUI_THEME_ASSET_FAILED;
        }
    }
    const unsigned chunk_size=16384u;
    struct theme_thumbnail *thumb=pixels && thumbnail?malloc(sizeof(*thumb)+chunk_size):NULL;
    uint8_t *chunk=NULL;
    if(thumb) {
        thumb->pixels=pixels;thumb->consumed=0;
        chunk=(uint8_t *)(thumb+1);
    }
    if(!pixels || (thumbnail && !thumb))ok=false;
    bool stopped=false;uint32_t actual=0;
    for(size_t offset=0;ok && offset<bytes;) {
        if(cancelled && cancelled()){stopped=true;ok=false;break;}
        UINT count=(UINT)(bytes-offset<chunk_size?bytes-offset:chunk_size);
        uint8_t *target=thumbnail?chunk:(uint8_t *)pixels+offset;
        r=f_read(&file,target,count,&got);
        if(r!=FR_OK || got!=count){ok=false;break;}
        actual=kui_crc32(actual,target,got);offset+=got;
        if(native_banner)copy_banner_region(thumb,target,got);
        else if(thumb)theme_thumbnail_sample(thumb,target,got);
    }
    free(thumb);
    FRESULT closed=f_close(&file);
    if(!ok || closed!=FR_OK || actual!=expected) {
        free(pixels);
        if(log && !stopped)log("Theme asset failed size/CRC/read checks: %s",path+2);
        return stopped?KUI_THEME_ASSET_CANCELLED:KUI_THEME_ASSET_FAILED;
    }
    /* The file is LE on every host; SH-4 and the production host are LE.
     * Explicit conversion also permits a big-endian host validator. */
    if(!thumbnail || native_banner)pixels_from_le(pixels,out_width*out_height);
    out->pixels=pixels;out->width=out_width;out->height=out_height;
    return KUI_THEME_ASSET_READY;
}

enum kui_theme_asset_status kui_theme_image_load(struct kui_theme_image *out,
    const struct kui_theme_definition *definition,enum kui_theme_asset_kind kind,
    kui_log_fn log,kui_cancel_fn cancelled) {
    return load_image(out,definition,kind,log,cancelled,false);
}

enum kui_theme_asset_status kui_theme_thumbnail_load(struct kui_theme_image *out,
    const struct kui_theme_definition *definition,enum kui_theme_asset_kind kind,
    kui_log_fn log,kui_cancel_fn cancelled) {
    return load_image(out,definition,kind,log,cancelled,true);
}
