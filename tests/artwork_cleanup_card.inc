/* SPDX-License-Identifier: GPL-3.0-only */
/* A separately formatted card exercises the installed 7194 layout without
 * a Games scan adding new downloads or unrelated marker files. */
#include "kui/artwork_cleanup.h"
#include "kui/games_metadata.h"

#define CLEAN_NAME (sizeof(KUI_DOWNLOADED_COVERS)+38u)
#define CLEAN_PEER ART_DIR "11111111111111111111111111111111.kct"
#define CLEAN_ORIGINAL ART_DIR "22222222222222222222222222222222.kct"
#define CLEAN_BROKEN ART_DIR "33333333333333333333333333333333.kct"
#define CLEAN_FOREIGN ART_DIR "hand-painted.png"

static uint64_t free_card_bytes(void) {
    DWORD free_clusters=0; FATFS *fs=NULL;
    assert(f_getfree("0:",&free_clusters,&fs)==FR_OK && fs && fs->csize);
    return (uint64_t)free_clusters*fs->csize*512u;
}
static void create_generation(uint16_t color,char path[CLEAN_NAME]) {
    uint8_t digest[32];char hex[33];
    art_compact(color);
    card_digest(ART_STAGE,digest);
    kui_hex(digest,16,hex);
    assert(snprintf(path,CLEAN_NAME,"%s/%s.kct",KUI_DOWNLOADED_COVERS,hex)<(int)CLEAN_NAME);
    assert(f_rename(ART_STAGE,path)==FR_OK);
}
static void check_live(const char *path) {
    FIL f; assert(f_open(&f,path,FA_READ)==FR_OK);
    assert(kui_compact_cover_verify(&f,cancelled)==KUI_COMPACT_OK && f_close(&f)==FR_OK);
}
static void check_gone(const char *path) {
    FILINFO info;assert(f_stat(path,&info)==FR_NO_FILE);
}
static void make_overlay(const char *path,const char *target) {
    art_link(target);
    assert(kui_games_publish_cover(ART_STAGE,path,cancelled));
}
static void protect_assigned(const char *native,const char *legacy) {
    struct kui_games_metadata *table=calloc(1,sizeof(*table));
    assert(table);
    struct kui_games_metadata_entry edit={.path="/games/other/disc.gdi",.anchor="/games/other/disc.gdi"};
    const char *leaf=strrchr(native,'/');
    assert(leaf && snprintf(edit.edit.cover_path,sizeof(edit.edit.cover_path),"/kui/covers/downloads%s",leaf) <
        (int)sizeof(edit.edit.cover_path));
    assert(kui_games_metadata_upsert(table,&edit));
    /* Earlier K-UI builds also allowed assigning the old .png path, which
     * resolves to an existing compact cover after conversion. */
    strcpy(edit.path,"/games/another/disc.gdi");
    strcpy(edit.anchor,edit.path);
    leaf=strrchr(legacy,'/');
    assert(leaf && snprintf(edit.edit.cover_path,sizeof(edit.edit.cover_path),"/kui/covers/downloads%s",leaf) <
        (int)sizeof(edit.edit.cover_path));
    strcpy(strrchr(edit.edit.cover_path,'.'),".png");
    assert(kui_games_metadata_upsert(table,&edit) && kui_games_metadata_save(table,log_line));
    free(table);
}

static void artwork_cleanup_card(void) {
    mount_art();
    assert(f_mkdir("0:/KUI")==FR_OK && f_mkdir("0:/KUI/covers")==FR_OK && f_mkdir(KUI_DOWNLOADED_COVERS)==FR_OK);
    char base[CLEAN_NAME],shared[CLEAN_NAME],assigned[CLEAN_NAME],legacy[CLEAN_NAME],unused[CLEAN_NAME],overlay[CLEAN_NAME];
    create_generation(11,base); create_generation(12,shared); create_generation(13,assigned);
    create_generation(14,unused); create_generation(16,legacy);
    strcpy(overlay,base);overlay[strlen(overlay)-1u]='u';
    make_overlay(overlay,shared);
    art_link(shared);assert(kui_games_publish_cover(ART_STAGE,CLEAN_PEER,cancelled));
    protect_assigned(assigned,legacy);
    art_compact(15);assert(f_rename(ART_STAGE,CLEAN_ORIGINAL)==FR_OK);
    art_write(CLEAN_FOREIGN,"hand-painted cover",18);
    char junk[36];memset(junk,'z',sizeof(junk));memcpy(junk,"KCL1",4);
    art_write(CLEAN_BROKEN,junk,sizeof(junk));
    unsigned removed=0;
    assert(!kui_artwork_cleanup_mounted(&removed,log_line,cancelled) && !removed);
    check_live(unused);
    assert(f_unlink(CLEAN_BROKEN)==FR_OK);
    assert(!kui_artwork_cleanup_mounted(&removed,log_line,art_cancel) && !removed);
    check_live(unused);
    art_remount();mount_art();
    uint64_t before=free_card_bytes();
    assert(kui_artwork_cleanup_mounted(&removed,log_line,cancelled) && removed==1);
    uint64_t after=free_card_bytes();
    /* First upkeep can allocate the media directory and its stamp. exFAT
     * charges one cluster to each; the card must still gain actual space. */
    assert(after>before);
    check_gone(unused);
    char versions[8][CLEAN_NAME];
    uint64_t plateau=0;
    for(unsigned i=0;i<8;i++) {
        create_generation((uint16_t)(100+i),versions[i]);
        make_overlay(overlay,versions[i]);
        art_remount();mount_art();
        assert(kui_artwork_cleanup_mounted(&removed,log_line,cancelled));
        assert(removed==(i<2?0u:1u));
        check_live(versions[i]);
        if(i) check_live(versions[i-1u]);
        if(i>=2) check_gone(versions[i-2u]);
        check_live(base);check_live(shared);check_live(assigned);check_live(legacy);check_live(CLEAN_ORIGINAL);
        FIL linked;
        char target[CLEAN_NAME];assert(kui_games_open_compact_target(&linked,base,target,sizeof(target))==FR_OK &&
            !strcmp(target,versions[i]) && f_close(&linked)==FR_OK);
        assert(kui_games_open_compact_target(&linked,CLEAN_PEER,target,sizeof(target))==FR_OK &&
            !strcmp(target,shared) &&
            kui_compact_cover_verify(&linked,cancelled)==KUI_COMPACT_OK && f_close(&linked)==FR_OK);
        FILINFO info;assert(f_stat(CLEAN_FOREIGN,&info)==FR_OK && info.fsize==18);
        uint64_t free_now=free_card_bytes();
        if(i==2) plateau=free_now;
        if(i>2) assert(plateau==free_now);
    }
    /* The changed pass already saved the post-sweep inventory. */
    count_cleanup_reads=true;cleanup_sectors_read=0;
    assert(kui_artwork_cleanup_mounted(&removed,log_line,cancelled) && !removed);
    count_cleanup_reads=false;
    /* A clean repeat can read directory metadata, but never rehash a native cover. */
    assert(cleanup_sectors_read<KUI_COMPACT_COVER_BYTES/512 && free_card_bytes()==plateau);
    printf("PASS artwork cleanup: initial free %llu -> %llu bytes, steady %llu across five more replacements; "
        "both generations, peer, base, assignment and foreign art preserved; %u sectors on a clean repeat\n",
        (unsigned long long)before,(unsigned long long)after,(unsigned long long)plateau,cleanup_sectors_read);
    assert(kui_games_storage_release());
}
