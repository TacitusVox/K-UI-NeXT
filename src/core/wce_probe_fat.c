/* SPDX-License-Identifier: GPL-3.0-only */
/* Finds 0:/KUI/WCEDATA/WCEDATA.BIN on a FAT32 or exFAT volume and maps its
 * allocation to card runs, for the Windows CE data probe's stage
 * (include/kui/wce_probe.h). Read-only: it never writes the card. */
#include "kui/wce_probe.h"
#include <string.h>

struct volume {
    const struct kui_wce_probe_volume *v;
    uint8_t *work;
    int exfat;
    uint32_t fat_lba,fat_sectors,data_lba,shift,clusters,root;
    uint32_t cached;      /* the FAT sector now in work, or 0 */
};

static uint32_t le16(const uint8_t *p) {return (uint32_t)p[0]|(uint32_t)p[1]<<8;}
static uint32_t le32(const uint8_t *p) {return le16(p)|le16(p+2)<<16;}

uint32_t kui_wce_probe_crc32(uint32_t crc,const void *data,size_t bytes) {
    const uint8_t *p=data;
    crc=~crc;
    while(bytes--) {
        crc^=*p++;
        for(unsigned bit=0;bit<8u;bit++) crc=(crc>>1)^(UINT32_C(0xedb88320)&(0u-(crc&1u)));
    }
    return ~crc;
}

static int read_sector(struct volume *f,uint32_t lba) {
    if(lba<f->v->partition_start || lba>=f->v->partition_end) return -1;
    f->cached=0u;
    return f->v->read(f->v->context,lba,f->work)?-1:0;
}

static int mount(struct volume *f) {
    const struct kui_wce_probe_volume *v=f->v;
    if(v->partition_end<=v->partition_start || read_sector(f,v->partition_start)) return -1;
    const uint8_t *b=f->work;
    if(b[510]!=0x55u || b[511]!=0xaau) return -1;
    uint32_t total=v->partition_end-v->partition_start;
    if(!memcmp(b+3u,"EXFAT   ",8u)) {
        /* 64-bit fields above 4 GiB sectors cannot occur on these cards. */
        if(le32(b+0x44u) || le32(b+0x4cu) || b[0x6cu]!=9u || b[0x6du]>15u || !b[0x6eu]) return -1;
        uint32_t length=le32(b+0x48u);
        f->exfat=1;f->shift=b[0x6du];
        f->fat_lba=v->partition_start+le32(b+0x50u);f->fat_sectors=le32(b+0x54u);
        f->data_lba=v->partition_start+le32(b+0x58u);f->clusters=le32(b+0x5cu);
        f->root=le32(b+0x60u);
        /* VolumeFlags ActiveFat: only the first FAT is read. */
        if(length>total || le32(b+0x50u)<24u || !f->fat_sectors || (le16(b+0x6au)&1u) ||
           le32(b+0x58u)<le32(b+0x50u)+f->fat_sectors || le32(b+0x58u)>=length) return -1;
    } else {
        uint32_t bytes=le16(b+11u),per=b[13u],reserved=le16(b+14u),fats=b[16u];
        uint32_t roots=le16(b+17u),small=le16(b+19u),fat16=le16(b+22u);
        uint32_t sectors=small?small:le32(b+32u),fat=le32(b+36u),ext=le16(b+40u);
        /* FAT32 only. With mirroring off (ExtFlags bit 7) only FAT 0 is read. */
        if(bytes!=512u || !per || (per&(per-1u)) || !reserved || !fats || fats>2u || roots || small ||
           fat16 || !fat || fat>total || sectors>total || ((ext&0x80u) && (ext&0x0fu))) return -1;
        f->exfat=0;f->shift=0u;
        while((1u<<f->shift)<per) ++f->shift;
        f->fat_lba=v->partition_start+reserved;f->fat_sectors=fat;
        uint64_t data64=(uint64_t)reserved+(uint64_t)fats*fat;
        if(data64>=sectors) return -1;
        uint32_t data=(uint32_t)data64;
        f->data_lba=v->partition_start+data;
        f->clusters=(sectors-data)>>f->shift;f->root=le32(b+44u);
    }
    if(f->clusters<1u || f->root<2u || f->root>f->clusters+1u ||
       (uint64_t)f->fat_sectors*128u<(uint64_t)f->clusters+2u) return -1;
    uint64_t end=(uint64_t)f->data_lba+((uint64_t)f->clusters<<f->shift);
    return end>v->partition_end?-1:0;
}

static uint32_t cluster_lba(const struct volume *f,uint32_t cluster) {
    return f->data_lba+((cluster-2u)<<f->shift);
}
static int cluster_valid(const struct volume *f,uint32_t cluster) {
    return cluster>=2u && cluster<=f->clusters+1u;
}
/* The next cluster of a FAT chain: 0 at its end, UINT32_MAX on a fault. */
static uint32_t fat_next(struct volume *f,uint32_t cluster) {
    uint32_t sector=f->fat_lba+cluster/128u;
    if(f->cached!=sector) {
        if(read_sector(f,sector)) return UINT32_MAX;
        f->cached=sector;
    }
    uint32_t next=le32(f->work+(cluster%128u)*4u);
    if(!f->exfat) next&=UINT32_C(0x0fffffff);
    if((!f->exfat && next>=UINT32_C(0x0ffffff8)) || (f->exfat && next==UINT32_MAX)) return 0u;
    return cluster_valid(f,next)?next:UINT32_MAX;
}

/* One directory entry name to find. FAT32 compares its 8.3 name. */
struct wanted {const char *name;char short_name[11];};
static int upper(int c) {return c>='a' && c<='z'?c-32:c;}

/* Directory walk: returns the entry's first cluster and size and whether it
 * is a directory and contiguous (exFAT NoFatChain). */
struct found {uint32_t cluster,bytes,valid;int directory,contiguous;};

static int scan(struct volume *f,uint32_t start,int start_contiguous,uint32_t start_bytes,
    const struct wanted *w,struct found *out) {
    uint32_t cluster=start,sectors_per=1u<<f->shift;
    uint32_t limit=f->exfat?(start_contiguous?(start_bytes+511u)/512u:UINT32_MAX):UINT32_MAX;
    /* exFAT entry-set state */
    unsigned pending=0u,name_left=0u,name_at=0u,matched=1u;
    uint32_t set_attr=0u;struct found candidate={0};
    uint32_t walked=0u;
    for(uint32_t visits=0u;visits<65536u;visits++) {
        if(!cluster_valid(f,cluster)) return -1;
        for(uint32_t s=0u;s<sectors_per;s++,walked++) {
            if(walked>=limit || walked>=4096u) return 1; /* 65536 entries at most */
            if(read_sector(f,cluster_lba(f,cluster)+s)) return -1;
            for(unsigned e=0u;e<512u;e+=32u) {
                const uint8_t *d=f->work+e;
                if(!f->exfat) {
                    if(!d[0]) return 1;
                    if(d[0]==0xe5u || d[11]==0x0fu || (d[11]&0x08u)) continue;
                    if(memcmp(d,w->short_name,11u)) continue;
                    out->cluster=le16(d+20u)<<16|le16(d+26u);
                    out->bytes=le32(d+28u);out->directory=!!(d[11]&0x10u);out->contiguous=0;
                    return 0;
                }
                uint8_t type=d[0];
                if(!type) return 1;
                if(type==0x85u) {
                    pending=d[1];set_attr=le16(d+4u);name_left=0u;name_at=0u;matched=1u;
                    memset(&candidate,0,sizeof(candidate));
                    if(pending<2u) pending=0u;
                    continue;
                }
                if(!pending) continue;
                --pending;
                if(type==0xc0u) {
                    candidate.contiguous=!!(d[1]&0x02u);
                    name_left=d[3];
                    candidate.cluster=le32(d+20u);
                    if(le32(d+28u) || le32(d+12u)) matched=0u; /* above 4 GiB */
                    candidate.bytes=le32(d+24u);candidate.valid=le32(d+8u);
                    if(name_left!=strlen(w->name)) matched=0u;
                } else if(type==0xc1u) {
                    size_t wanted_length=strlen(w->name);
                    for(unsigned i=0u;i<15u && name_left;i++,name_left--,name_at++) {
                        uint32_t c=le16(d+2u+2u*i);
                        if(name_at>=wanted_length || c>127u ||
                           upper((int)c)!=upper(w->name[name_at])) matched=0u;
                    }
                }
                if(!pending && matched && !name_left && candidate.cluster) {
                    candidate.directory=!!(set_attr&0x10u);
                    *out=candidate;return 0;
                }
            }
        }
        if(f->exfat && start_contiguous) {++cluster;continue;}
        uint32_t next=fat_next(f,cluster);
        if(next==UINT32_MAX) return -1;
        if(!next) return 1;
        cluster=next;
    }
    return -1;
}

enum kui_wce_probe_fat_result kui_wce_probe_fat_find(const struct kui_wce_probe_volume *volume,
    struct kui_wce_probe_layout *layout,uint8_t work[512]) {
    if(!volume || !volume->read || !layout || !work) return KUI_WCE_PROBE_FAT_VOLUME;
    memset(layout,0,sizeof(*layout));
    struct volume f={.v=volume,.work=work};
    if(mount(&f)) return KUI_WCE_PROBE_FAT_VOLUME;
    static const struct wanted path[3]={
        {"KUI",{'K','U','I',' ',' ',' ',' ',' ',' ',' ',' '}},
        {"WCEDATA",{'W','C','E','D','A','T','A',' ',' ',' ',' '}},
        {"WCEDATA.BIN",{'W','C','E','D','A','T','A',' ','B','I','N'}}};
    struct found at={.cluster=f.root,.directory=1,.contiguous=0,.bytes=0};
    for(unsigned i=0u;i<3u;i++) {
        if(!at.directory) return KUI_WCE_PROBE_FAT_MISSING;
        struct found next={0};
        int r=scan(&f,at.cluster,at.contiguous,at.bytes,&path[i],&next);
        if(r<0) return KUI_WCE_PROBE_FAT_IO;
        if(r>0 || (i<2u)!=!!next.directory) return KUI_WCE_PROBE_FAT_MISSING;
        at=next;
    }
    /* exFAT: every byte of the file must be valid data (ValidDataLength). */
    if(!at.bytes || !cluster_valid(&f,at.cluster) || (f.exfat && at.valid!=at.bytes))
        return KUI_WCE_PROBE_FAT_CHAIN;
    uint32_t cluster_bytes=512u<<f.shift;
    uint32_t needed=(uint32_t)(((uint64_t)at.bytes+cluster_bytes-1u)/cluster_bytes);
    uint32_t cluster=at.cluster;
    for(uint32_t n=0u;n<needed;n++) {
        if(!cluster_valid(&f,cluster)) return KUI_WCE_PROBE_FAT_CHAIN;
        uint32_t lba=cluster_lba(&f,cluster),sectors=1u<<f.shift;
        struct kui_wce_probe_run *last=layout->run_count?&layout->runs[layout->run_count-1u]:NULL;
        if(last && last->lba+last->sectors==lba) last->sectors+=sectors;
        else {
            if(layout->run_count==KUI_WCE_PROBE_FILE_RUNS) return KUI_WCE_PROBE_FAT_FRAGMENTED;
            layout->runs[layout->run_count].lba=lba;
            layout->runs[layout->run_count++].sectors=sectors;
        }
        if(n+1u==needed) break;
        if(f.exfat && at.contiguous) {++cluster;continue;}
        uint32_t next=fat_next(&f,cluster);
        if(next==UINT32_MAX || !next) return KUI_WCE_PROBE_FAT_CHAIN;
        cluster=next;
    }
    /* A looping or cross-linked chain shows as runs that overlap. */
    for(uint32_t i=0u;i<layout->run_count;i++)
        for(uint32_t j=i+1u;j<layout->run_count;j++) {
            const struct kui_wce_probe_run *a=&layout->runs[i],*b=&layout->runs[j];
            if(a->lba<b->lba+b->sectors && b->lba<a->lba+a->sectors) return KUI_WCE_PROBE_FAT_CHAIN;
        }
    layout->bytes=at.bytes;
    return KUI_WCE_PROBE_FAT_OK;
}

uint32_t kui_wce_probe_lba(const struct kui_wce_probe_layout *layout,uint32_t sector) {
    if(!layout || sector>=layout->bytes/512u) return 0u;
    for(uint32_t i=0u;i<layout->run_count;i++) {
        const struct kui_wce_probe_run *r=&layout->runs[i];
        if(sector<r->sectors) return r->lba+sector;
        sector-=r->sectors;
    }
    return 0u;
}

uint32_t kui_wce_probe_runs(const struct kui_wce_probe_layout *layout,uint32_t first,uint32_t count,
    struct kui_wce_probe_run *out,uint32_t capacity) {
    if(!layout || !out || !count || first>layout->bytes/512u || count>layout->bytes/512u-first) return 0u;
    uint32_t used=0u,skip=first;
    for(uint32_t i=0u;i<layout->run_count && count;i++) {
        const struct kui_wce_probe_run *r=&layout->runs[i];
        if(skip>=r->sectors) {skip-=r->sectors;continue;}
        uint32_t take=r->sectors-skip;
        if(take>count) take=count;
        if(used==capacity) return 0u;
        out[used].lba=r->lba+skip;out[used++].sectors=take;
        count-=take;skip=0u;
    }
    return count?0u:used;
}

void kui_wce_probe_slot_header(struct kui_wce_probe_slot *h,uint32_t slot,uint32_t state,
    uint32_t sequence,uint32_t rtc,const uint8_t file_id[16]) {
    memset(h,0,sizeof(*h));
    memcpy(h->magic,"KUIWCES1",8u);
    h->version=KUI_WCE_PROBE_VERSION;h->header_bytes=KUI_WCE_PROBE_HEADER_BYTES;
    h->slot=slot;h->state=state;h->sequence=sequence;h->rtc=rtc;
    memcpy(h->file_id,file_id,sizeof(h->file_id));
    h->crc32=kui_wce_probe_crc32(0u,h,offsetof(struct kui_wce_probe_slot,crc32));
}

static int claim_blank(const struct kui_wce_probe_volume *v,uint32_t lba,uint8_t work[512]) {
    if(lba<v->partition_start || lba>=v->partition_end || v->read(v->context,lba,work)) return -1;
    for(unsigned i=0;i<512u;i++) if(work[i]) return 1;
    return 0;
}

enum kui_wce_probe_claim_result kui_wce_probe_claim_find(const struct kui_wce_probe_volume *volume,
    const struct kui_wce_probe_layout *layout,uint8_t work[512],struct kui_wce_probe_claim *out) {
    memset(out,0,sizeof(*out));
    uint32_t lba=kui_wce_probe_lba(layout,0u);
    if(!lba || lba<volume->partition_start || lba>=volume->partition_end ||
       volume->read(volume->context,lba,work)) return KUI_WCE_PROBE_CLAIM_IO;
    struct kui_wce_probe_file *file=&out->file;
    memcpy(file,work,sizeof(*file));
    if(memcmp(file->magic,"KUIWCED1",8u) || file->version!=KUI_WCE_PROBE_VERSION ||
       file->header_bytes!=KUI_WCE_PROBE_HEADER_BYTES ||
       file->crc32!=kui_wce_probe_crc32(0u,file,offsetof(struct kui_wce_probe_file,crc32)) ||
       file->slot_sectors!=KUI_WCE_PROBE_SLOT_SECTORS || file->dump_sectors!=KUI_WCE_PROBE_DUMP_SECTORS ||
       file->first_slot<1u || !file->slot_count || file->slot_count>255u || /* < 4 GiB */
       (uint64_t)file->first_slot+(uint64_t)file->slot_count*file->slot_sectors>layout->bytes/512u)
        return KUI_WCE_PROBE_CLAIM_NOT_FILE;
    for(uint32_t slot=0;slot<file->slot_count;slot++) {
        uint32_t base=file->first_slot+slot*file->slot_sectors;
        uint32_t header=kui_wce_probe_lba(layout,base);
        struct kui_wce_probe_slot h,blank;
        if(!header || header<volume->partition_start || header>=volume->partition_end ||
           volume->read(volume->context,header,work)) return KUI_WCE_PROBE_CLAIM_IO;
        memcpy(&h,work,sizeof(h));
        kui_wce_probe_slot_header(&blank,slot,KUI_WCE_PROBE_EMPTY,0u,0u,file->file_id);
        if(memcmp(&h,&blank,sizeof(h))) {
            /* A used slot of this file. Anything else is not ours to touch. */
            if(memcmp(h.magic,"KUIWCES1",8u) || h.slot!=slot ||
               memcmp(h.file_id,file->file_id,sizeof(h.file_id)) ||
               h.crc32!=kui_wce_probe_crc32(0u,&h,offsetof(struct kui_wce_probe_slot,crc32)))
                return KUI_WCE_PROBE_CLAIM_SLOT_HEADER;
            ++out->used;continue;
        }
        out->stage_count=kui_wce_probe_runs(layout,base+KUI_WCE_PROBE_STAGE_SECTOR,
            KUI_WCE_PROBE_STAGE_SECTORS,out->stage_runs,KUI_WCE_PROBE_STAGE_SECTORS);
        out->run_count=kui_wce_probe_runs(layout,base+KUI_WCE_PROBE_TERMINAL_SECTOR,
            KUI_WCE_PROBE_SLOT_SECTORS-KUI_WCE_PROBE_TERMINAL_SECTOR,out->runs,KUI_WCE_PROBE_SLOT_RUNS);
        if(!out->stage_count || !out->run_count) {++out->used;continue;} /* too fragmented */
        /* Every run must lie in the volume and start blank. */
        for(uint32_t i=0;i<out->stage_count;i++) {
            const struct kui_wce_probe_run *r=&out->stage_runs[i];
            if(r->lba<volume->partition_start || r->lba+r->sectors>volume->partition_end)
                return KUI_WCE_PROBE_CLAIM_IO;
            int b=claim_blank(volume,r->lba,work);
            if(b) return b<0?KUI_WCE_PROBE_CLAIM_IO:KUI_WCE_PROBE_CLAIM_NOT_BLANK;
        }
        for(uint32_t i=0;i<out->run_count;i++) {
            const struct kui_wce_probe_run *r=&out->runs[i];
            if(r->lba<volume->partition_start || r->lba+r->sectors>volume->partition_end)
                return KUI_WCE_PROBE_CLAIM_IO;
            int b=claim_blank(volume,r->lba,work);
            if(b) return b<0?KUI_WCE_PROBE_CLAIM_IO:KUI_WCE_PROBE_CLAIM_NOT_BLANK;
        }
        out->slot=slot;out->header_lba=header;
        return KUI_WCE_PROBE_CLAIM_OK;
    }
    return KUI_WCE_PROBE_CLAIM_FULL;
}

void kui_wce_probe_seed_build(const struct kui_wce_probe_claim *c,uint32_t rtc,uint32_t flags,
    struct kui_wce_probe_seed *seed) {
    memset(seed,0,sizeof(*seed));
    seed->magic=KUI_WCE_PROBE_SEED_MAGIC;seed->version=KUI_WCE_PROBE_VERSION;
    seed->slot=c->slot;seed->run_count=c->run_count;seed->slot_header_lba=c->header_lba;
    seed->sequence=c->used+1u;seed->stage_rtc=rtc;seed->flags=flags;
    memcpy(seed->file_id,c->file.file_id,sizeof(seed->file_id));
    memcpy(seed->runs,c->runs,sizeof(seed->runs));
    kui_wce_probe_slot_header(&seed->complete,c->slot,KUI_WCE_PROBE_COMPLETE,c->used+1u,rtc,c->file.file_id);
    seed->crc32=kui_wce_probe_crc32(0u,seed,offsetof(struct kui_wce_probe_seed,crc32));
}
