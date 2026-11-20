/* SPDX-License-Identifier: GPL-3.0-only */
/* FatFs publisher failure injection on the FAT32/exFAT multidisc fixture.
 * Crash children discard the application's FIL and FatFs caches. Only writes
 * already sent to the emulated block device survive its fresh host open. */
#include <sys/wait.h>
enum art_fault {
    ART_OK, ART_RENAME_ERROR, ART_CUT_AFTER_RENAME, ART_SLOT_WRITE_ERROR,
    ART_CUT_BEFORE_SLOT, ART_CUT_TORN_SLOT, ART_SLOT_SYNC_ERROR,
    ART_CUT_AFTER_SYNC, ART_READBACK_ERROR, ART_CUT_CATALOG_PENDING
};
static const char *fault_card_path;
static struct {
    enum art_fault kind;
    const char *path;
    FIL *file;
    bool touched, wrote_slot, synced_slot;
} art_fault;

FRESULT __real_f_open(FIL *,const TCHAR *,BYTE);
FRESULT __real_f_read(FIL *,void *,UINT,UINT *);
FRESULT __real_f_write(FIL *,const void *,UINT,UINT *);
FRESULT __real_f_sync(FIL *);
FRESULT __real_f_rename(const TCHAR *,const TCHAR *);

static void art_power_cut(void) {
    assert(!fflush(card)); /* Controller media, not FatFs's pending buffers. */
    _exit(71);
}
FRESULT __wrap_f_open(FIL *f,const TCHAR *path,BYTE mode) {
    FRESULT r=__real_f_open(f,path,mode);
    if(art_fault.path && !strcmp(path,art_fault.path) && r==FR_OK) art_fault.file=f;
    else if(art_fault.file==f) art_fault.file=NULL;
    return r;
}
FRESULT __wrap_f_read(FIL *f,void *data,UINT n,UINT *got) {
    if(f==art_fault.file && art_fault.kind==ART_READBACK_ERROR && art_fault.synced_slot && !art_fault.touched) {
        art_fault.touched=true;*got=0;return FR_DISK_ERR;
    }
    return __real_f_read(f,data,n,got);
}
FRESULT __wrap_f_write(FIL *f,const void *data,UINT n,UINT *sent) {
    if(f==art_fault.file && art_fault.kind==ART_CUT_CATALOG_PENDING &&
       f_tell(f)==0 && n==KUI_COVER_HEADER_BYTES && !memcmp(data,"\0\0\0\0",4))
        art_fault.wrote_slot=true;
    if(f==art_fault.file && n==48 && !memcmp(data,"KCU1",4)) {
        if(art_fault.kind==ART_CUT_BEFORE_SLOT) art_power_cut();
        if(art_fault.kind==ART_SLOT_WRITE_ERROR || art_fault.kind==ART_CUT_TORN_SLOT) {
            assert(__real_f_write(f,data,19,sent)==FR_OK && *sent==19 && __real_f_sync(f)==FR_OK);
            if(art_fault.kind==ART_CUT_TORN_SLOT) art_power_cut();
            art_fault.touched=true;return FR_DISK_ERR;
        }
        art_fault.wrote_slot=true;
    }
    return __real_f_write(f,data,n,sent);
}
FRESULT __wrap_f_sync(FIL *f) {
    if(f==art_fault.file && art_fault.wrote_slot && art_fault.kind==ART_SLOT_SYNC_ERROR && !art_fault.touched) {
        art_fault.touched=true;return FR_DISK_ERR;
    }
    FRESULT r=__real_f_sync(f);
    if(r==FR_OK && f==art_fault.file && art_fault.wrote_slot) {
        art_fault.synced_slot=true;
        if(art_fault.kind==ART_CUT_AFTER_SYNC || art_fault.kind==ART_CUT_CATALOG_PENDING) art_power_cut();
    }
    return r;
}
FRESULT __wrap_f_rename(const TCHAR *from,const TCHAR *to) {
    bool target=art_fault.path && !strcmp(art_fault.path,to);
    if(target && art_fault.kind==ART_RENAME_ERROR) {art_fault.touched=true;return FR_DISK_ERR;}
    FRESULT r=__real_f_rename(from,to);
    if(target && r==FR_OK && art_fault.kind==ART_CUT_AFTER_RENAME) art_power_cut();
    return r;
}

#define ART_DIR KUI_DOWNLOADED_COVERS "/"
#define ART_NEW ART_DIR "aaaaaaaabbbbbbbbccccccccdddddddd.kct"
#define ART_NEXT ART_DIR "eeeeeeee111111112222222233333333.kct"
#define ART_STAGE ART_DIR "faulttemp.tmp"
#define ART_ASSIGNED "0:/kui/assigned-fault-test.png"

static void mount_art(void) {assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);}
static void art_remount(void) {
    assert(kui_games_storage_release() && !card_open && !fflush(card));
    assert((card=freopen(fault_card_path,"r+b",card)));
    kui_games_covers_cache_clear();
}
static void art_write(const char *path,const void *data,UINT n) {
    FIL f;UINT sent;
    assert(f_open(&f,path,FA_CREATE_ALWAYS|FA_WRITE)==FR_OK && f_write(&f,data,n,&sent)==FR_OK && sent==n);
    assert(f_sync(&f)==FR_OK && f_close(&f)==FR_OK);
}
static void art_link(const char *target) {
    char b[36]="KCL1";
    memcpy(b+4,strrchr(target,'/')+1,32);
    art_write(ART_STAGE,b,sizeof(b));
}
static void art_compact(uint16_t color) {
    static uint8_t bytes[KUI_COMPACT_COVER_BYTES];
    memcpy(bytes,"KCT1",4);
    unsigned at=KUI_COMPACT_COVER_HEADER;
    for(unsigned i=0;i<3;++i) {
        unsigned n=2*kui_cover_edge(i)*kui_cover_edge(i);
        for(unsigned j=0;j<n;j+=2) {bytes[at+j]=(uint8_t)color;bytes[at+j+1]=(uint8_t)(color>>8);}
        uint32_t crc=kui_crc32_capture(0,bytes+at,n);
        for(unsigned j=0;j<4;++j) bytes[4+4*i+j]=(uint8_t)(crc>>(8*j));
        at+=n;
    }
    assert(at==sizeof(bytes));art_write(ART_STAGE,bytes,sizeof(bytes));
}
static bool art_cancel(void) {return true;}
static void inject_art_fault(enum art_fault kind,const char *path,const struct kui_games_entry *activate) {
    assert(kui_games_storage_release() && !card_open && !fflush(card));
    pid_t pid=fork();assert(pid>=0);
    if(!pid) {
        assert((card=freopen(fault_card_path,"r+b",card)));
        mount_art();
        art_fault.kind=kind;art_fault.path=path;
        bool present=false;
        bool result=activate ? kui_games_prepare_media_cover(activate,&present,NULL,log_line,cancelled) :
                               kui_games_publish_cover(ART_STAGE,path,cancelled);
        assert(!result && art_fault.touched);
        assert(kui_games_storage_release() && !fflush(card));
        _exit(72);
    }
    int status=0;assert(waitpid(pid,&status,0)==pid && WIFEXITED(status));
    bool cut=kind==ART_CUT_AFTER_RENAME || kind==ART_CUT_BEFORE_SLOT ||
        kind==ART_CUT_TORN_SLOT || kind==ART_CUT_AFTER_SYNC || kind==ART_CUT_CATALOG_PENDING;
    assert(WEXITSTATUS(status)==(cut?71:72));
    art_remount();
}
static void remove_if_present(const char *path) {
    FRESULT r=f_unlink(path);assert(r==FR_OK || r==FR_NO_FILE);
}
static void art_activate(const struct kui_games_entry *one) {
    bool present=false;
    assert(kui_games_prepare_media_cover(one,&present,NULL,log_line,cancelled) && present);
}
static bool art_resolves(const char *master,const char *first,const char *second) {
    char target[KUI_GAMES_FILE_CAP];FIL f;
    assert(kui_games_open_compact_target(&f,master,target,sizeof(target))==FR_OK);
    assert(kui_compact_cover_verify(&f,cancelled)==KUI_COMPACT_OK && f_close(&f)==FR_OK);
    if(!strcmp(target,first)) return true;
    assert(!strcmp(target,second));return false;
}
static void artwork_publish_faults(const struct kui_games_entry *one,const struct kui_games_entry *two,
                                  const uint16_t *prior,const char *master) {
    static uint16_t actual[PIXEL_BYTES/2],assigned_pixels[PIXEL_BYTES/2],yellow[PIXEL_BYTES/2],blue[PIXEL_BYTES/2];
    for(unsigned i=0;i<PIXEL_BYTES/2;++i){yellow[i]=0xffe0;blue[i]=0x001f;}
    char overlay[KUI_GAMES_FILE_CAP];
    strcpy(overlay,master);overlay[strlen(overlay)-1]='u';
    struct kui_games_entry assigned=*one;strcpy(assigned.cover_path,ART_ASSIGNED+2);
    uint8_t old_master[32],old_assignment[32],after[32];
    mount_art();copy_fixture("custom.png",ART_ASSIGNED);
    card_digest(ART_ASSIGNED,old_assignment);card_digest(master,old_master);
    art_remount();visible_pixels(&assigned,assigned_pixels);

    for(enum art_fault kind=ART_RENAME_ERROR;kind<=ART_CUT_AFTER_RENAME;++kind) {
        mount_art();art_compact(0xffe0);inject_art_fault(kind,ART_NEW,NULL);
        visible_pixels(one,actual);assert(!memcmp(prior,actual,PIXEL_BYTES));
        visible_pixels(two,actual);assert(!memcmp(prior,actual,PIXEL_BYTES));
    }
    mount_art();remove_if_present(ART_NEW);art_compact(0xffe0);
    assert(kui_games_publish_cover(ART_STAGE,ART_NEW,cancelled));
    art_compact(0x001f);assert(kui_games_publish_cover(ART_STAGE,ART_NEXT,cancelled));
    art_link(ART_NEW);assert(!kui_games_publish_cover(ART_STAGE,overlay,art_cancel));
    art_link(ART_DIR "ffffffffffffffffffffffffffffffff.kct");
    assert(!kui_games_publish_cover(ART_STAGE,overlay,cancelled));
    art_write(ART_STAGE,"KCL1bogus",9);assert(!kui_games_publish_cover(ART_STAGE,overlay,cancelled));
    art_compact(0xffe0);change_card_byte(ART_STAGE,4096);
    assert(!kui_games_publish_cover(ART_STAGE,ART_DIR "aaaaaaaa000000000000000000000000.kct",cancelled));
    art_remount();visible_pixels(one,actual);assert(!memcmp(prior,actual,PIXEL_BYTES));

    for(enum art_fault kind=ART_SLOT_WRITE_ERROR;kind<=ART_READBACK_ERROR;++kind) {
        mount_art();remove_if_present(overlay);
        art_link(ART_NEW);assert(kui_games_publish_cover(ART_STAGE,overlay,cancelled));art_activate(one);
        art_remount();visible_pixels(one,actual);assert(!memcmp(yellow,actual,PIXEL_BYTES));
        mount_art();art_link(ART_NEXT);inject_art_fault(kind,overlay,NULL);
        visible_pixels(one,actual);
        /* The downloaded slot and the visible .kcv stamp commit separately.
         * A cut between them must leave the last accepted pixels displayable. */
        assert(!memcmp(yellow,actual,PIXEL_BYTES));
        mount_art();
        bool updated=art_resolves(master,ART_NEXT,ART_NEW);
        if(kind==ART_CUT_AFTER_SYNC) assert(updated);
        else if(kind!=ART_READBACK_ERROR && kind!=ART_SLOT_SYNC_ERROR) assert(!updated);
        art_activate(one);art_remount();
        visible_pixels(one,actual);assert(!memcmp(updated?blue:yellow,actual,PIXEL_BYTES));
        visible_pixels(two,actual);assert(!memcmp(prior,actual,PIXEL_BYTES));
        kui_games_covers_content_clear();visible_pixels(&assigned,actual);assert(!memcmp(assigned_pixels,actual,PIXEL_BYTES));
        mount_art();card_digest(ART_ASSIGNED,after);assert(!memcmp(old_assignment,after,32));
        card_digest(master,after);assert(!memcmp(old_master,after,32));
    }
    mount_art();remove_if_present(overlay);
    art_link(ART_NEW);assert(kui_games_publish_cover(ART_STAGE,overlay,cancelled));art_activate(one);
    art_link(ART_NEXT);assert(kui_games_publish_cover(ART_STAGE,overlay,cancelled));
    char key[KUI_COVER_PATH_CAP],record[KUI_GAMES_FILE_CAP];
    assert(kui_cover_key(one->name,key));
    assert(snprintf(record,sizeof(record),"%s/%s.kcv",KUI_GAMES_COVERS_FOLDER,key)<(int)sizeof(record));
    inject_art_fault(ART_CUT_CATALOG_PENDING,record,one);
    visible_pixels(one,actual);assert(!memcmp(yellow,actual,PIXEL_BYTES));
    visible_pixels(two,actual);assert(!memcmp(prior,actual,PIXEL_BYTES));
    mount_art();art_activate(one);
    art_remount();visible_pixels(one,actual);assert(!memcmp(blue,actual,PIXEL_BYTES));
    mount_art();change_card_byte(overlay,512+44);art_remount();
    visible_pixels(one,actual);assert(!memcmp(yellow,actual,PIXEL_BYTES));
    mount_art();
    FIL f;assert(f_open(&f,overlay,FA_WRITE)==FR_OK && f_lseek(&f,512+12)==FR_OK);
    assert(f_truncate(&f)==FR_OK && f_close(&f)==FR_OK);art_remount();
    visible_pixels(one,actual);assert(!memcmp(yellow,actual,PIXEL_BYTES));
    mount_art();remove_if_present(overlay);remove_if_present(ART_NEXT);remove_if_present(ART_NEW);
    remove_if_present(ART_STAGE);remove_if_present(ART_ASSIGNED);art_remount();
    puts("PASS card remount after failed master rename, cancelled or invalid stage, slot write/sync/readback errors, three slot power cuts and damaged newest selector; verified both discs and assigned cover pixels");
}
