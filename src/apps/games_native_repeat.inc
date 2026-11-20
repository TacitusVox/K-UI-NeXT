/* SPDX-License-Identifier: GPL-3.0-only */
/* A completed ordinary reader preflight can preserve its *positive* result.
 * This record is local configuration, like the VMU mapping configuration.
 * Bind the complete source (including reclaimed suffixes), detector and
 * module to the checksummed record. Cached length/extent is meaningful only
 * after that binding matches; the current prefix bytes still have to replay.
 * The detached stage independently checks lifetime, image and live ownership.
 * No failed discovery or choice of standalone reader is ever persisted. */
struct native_repeat_record {
    uint8_t key[32],seal[32],wire[KUI_VMU_NATIVE_RESOLUTION_CACHE_BYTES];
};
static void native_repeat_key(uint8_t key[32],const uint8_t image_hash[32],
    const struct kui_retail_manifest *map,const struct kui_runtime_image *package,bool dual) {
    /* Bump the format domain for incompatible lifetime/receipt semantics,
     * even if the underlying NVR detector and module did not change. */
    static const char domain[]="K-UI complete native reader proof v1";
    const uint32_t identity[]={KUI_VMU_NATIVE_RESOLUTION_DETECTOR_EPOCH,map->boot_bytes,
        KUI_RETAIL_EXEC_ADDRESS,map->storage_transport,map->reader,dual?native_cache_dual_lease(package):0u,
        package->info.payload_bytes,package->info.memory_bytes,package->info.crc32};
    struct kui_sha256 hash;kui_sha256_init(&hash);
    kui_sha256_update(&hash,domain,sizeof(domain));
    kui_sha256_update(&hash,image_hash,32u);
    kui_sha256_update(&hash,package->info.build,sizeof(package->info.build));
    for(unsigned i=0u;i<sizeof(identity)/sizeof(identity[0]);++i) {
        uint8_t word[4];
        for(unsigned j=0u;j<4u;++j)word[j]=(uint8_t)(identity[i]>>(8u*j));
        kui_sha256_update(&hash,word,sizeof(word));
    }
    kui_sha256_digest(&hash,key);
}
static void native_repeat_seal(const struct native_repeat_record *record,uint8_t digest[32]) {
    struct kui_sha256 hash;kui_sha256_init(&hash);
    kui_sha256_update(&hash,record->key,sizeof(record->key));
    kui_sha256_update(&hash,record->wire,sizeof(record->wire));
    kui_sha256_digest(&hash,digest);
}
static void native_repeat_path(char path[120],const uint8_t key[32]) {
    char hex[65];kui_hex(key,32u,hex);
    snprintf(path,120,"0:/KUI/GMCACHE/%s.nvr",hex);
}
/* image_hash and marks were both computed from this private boot allocation
 * in this invocation. For a reclaimed suffix, hash exactly the program
 * prefix, resuming from its final 64 KiB checkpoint where available. */
static bool native_cache_replay_program(const uint8_t *boot,size_t source_bytes,size_t program_bytes,
    const struct kui_sha256_prefix *marks,const uint8_t image_hash[32],struct native_cache_workspace *work) {
    uint8_t prefix[32];const uint8_t *digest=image_hash;
    if(program_bytes!=source_bytes) {
        size_t rehashed=0u;
        KUI_SMASH_TIME_SPAN(prefix_sha,KUI_SMASH_UI_EXEC_SHA);
        bool valid=kui_sha256_prefix_digest(marks,boot,program_bytes,prefix,&rehashed);
        KUI_SMASH_TIME_END(prefix_sha);
        if(!valid)return false;
        kui_smash_launch_timing_add(KUI_SMASH_UI_EXEC_SHA_BYTES,rehashed);
        digest=prefix;
    }
    return kui_vmu_native_resolution_cache_replay_hashed(boot,program_bytes,KUI_RETAIL_EXEC_ADDRESS,
        work->wire,sizeof(work->wire),work->digest,digest,&work->resolution);
}
static bool native_repeat_read(struct native_repeat_record *record,const uint8_t key[32],
    const uint8_t *boot,const struct kui_retail_manifest *map,const struct kui_runtime_image *package,
    bool dual,const struct kui_sha256_prefix *marks,const uint8_t image_hash[32],
    struct native_cache_workspace *work) {
    char path[120];native_repeat_path(path,key);
    if(!native_cache_read_bytes(path,record,sizeof(*record)) || memcmp(record->key,key,32u))return false;
    uint8_t seal[32];native_repeat_seal(record,seal);
    if(memcmp(record->seal,seal,32u) ||
       !kui_vmu_native_resolution_cache_inspect_config(record->wire,sizeof(record->wire),&work->resolution) ||
       !kui_vmu_native_resolution_cache_record_digest(record->wire,sizeof(record->wire),work->digest))return false;
    const size_t bytes=work->resolution.driver.image_bytes;
    const bool arena=work->resolution.heap.family==KUI_VMU_NATIVE_HEAP_ARENA;
    if(!bytes || bytes>map->boot_bytes || work->resolution.driver.load_address!=KUI_RETAIL_EXEC_ADDRESS ||
       (!dual && (arena || !work->resolution.heap.initializer_slot)) ||
       (arena && native_cache_dual_lease(package)!=work->resolution.call.entry_bytes))return false;
    memcpy(work->wire,record->wire,sizeof(work->wire));
    return native_cache_replay_program(boot,map->boot_bytes,bytes,marks,image_hash,work);
}
/* Called only after the current launch has consumed its fresh CRC and its
 * source files are closed. Failure is a cold repeat, never a launch failure. */
static inline void native_repeat_publish(const struct native_repeat_record *record,kui_log_fn log,kui_cancel_fn cancel) {
    if(!record || native_cache_cancelled(cancel))return;
    struct native_repeat_record *check=malloc(sizeof(*check));
    if(!check)return;
    char path[120];native_repeat_path(path,record->key);
    bool valid=native_cache_ensure_directory("0:/KUI") && native_cache_ensure_directory("0:/KUI/GMCACHE") &&
        native_cache_publish_bytes(path,record,check,sizeof(*record),cancel,record->key);
    if(!valid && log)log("Native reader mapping could not be saved; next launch will check it again");
    free(check);
}
