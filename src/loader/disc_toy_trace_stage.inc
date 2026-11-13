/* SPDX-License-Identifier: GPL-3.0-only */
#include "disc_toy_trace.h"
#include "kui/gd_service.h"
#include "kui/toy_pilot_boot.h"
#include "kui/vmu_launch_package.h"

/* Bound only by the explicit physical Toy diagnostic GUI action. Keep blank
 * module markers distinct from all literal code and verify before use. */
__asm__(".section .rodata.disc_toy_trace_seeds,\"a\"\n.balign 4\n"
        ".global __disc_toy_report_seed\n__disc_toy_report_seed:\n"
        ".ascii \"KUITRS01\"\n.long 1,64\n.zero 48\n"
        ".global __disc_toy_range_seed\n__disc_toy_range_seed:\n"
        ".ascii \"KUITFR01\"\n.zero 32\n.previous\n"
        ".section .rodata.disc_toy_trace_worker,\"a\"\n.balign 4\n"
        ".global __disc_toy_worker_begin\n__disc_toy_worker_begin:\n"
        ".incbin \"" KUI_DISC_TOY_TRACE_BIN "\"\n"
        ".global __disc_toy_worker_end\n__disc_toy_worker_end:\n.previous\n");
extern const uint8_t disc_toy_worker_begin[] __asm__("__disc_toy_worker_begin");
extern const uint8_t disc_toy_worker_end[] __asm__("__disc_toy_worker_end");
extern const struct kui_toy_report_wire disc_toy_report_seed __asm__("__disc_toy_report_seed");
struct disc_toy_range_seed {
    uint32_t magic[2];
    struct kui_toy_movie_range movie[4];
};
extern const struct disc_toy_range_seed disc_toy_ranges __asm__("__disc_toy_range_seed");
_Static_assert(sizeof(struct disc_toy_range_seed)==40u,"physical movie ranges ABI");

void kui_disc_toy_heap_hook(void);
static uint32_t disc_toy_firmware;
uint32_t kui_disc_toy_heap_call[4];

static uint32_t toy_word(uint32_t address) {
    return *(volatile const uint32_t *)(uintptr_t)address;
}

static void toy_publish(uint32_t address, uint32_t value) {
    *(volatile uint32_t *)(uintptr_t)address=value;
    __asm__ volatile("ocbp @%0"::"r"(address&~31u):"memory");
}

static bool toy_ranges_valid(void) {
    if(disc_toy_ranges.magic[0]!=0x5449554bu || disc_toy_ranges.magic[1]!=0x31305246u)
        return false;
    for(unsigned i=0;i<4u;i++) {
        struct kui_toy_movie_range current=disc_toy_ranges.movie[i];
        if(current.lba<45000u || current.lba>=719850u ||
           !current.count || current.count>719850u-current.lba) return false;
        for(unsigned j=0;j<i;j++) {
            struct kui_toy_movie_range other=disc_toy_ranges.movie[j];
            if(current.lba<other.lba+other.count && other.lba<current.lba+current.count &&
               (i/2u!=j/2u || current.lba!=other.lba || current.count!=other.count)) return false;
        }
    }
    return true;
}

static bool toy_report_valid(void) {
    const struct kui_toy_report_wire *seed=&disc_toy_report_seed;
    return seed->magic[0]==KUI_TOY_REPORT_MAGIC0 && seed->magic[1]==KUI_TOY_REPORT_MAGIC1 &&
        seed->version==KUI_TOY_REPORT_VERSION && seed->bytes==sizeof(*seed) &&
        seed->test==KUI_DISC_TOY_TRACE_TEST && !seed->reserved && seed->high_capacity<=1u &&
        seed->lba[0] && seed->lba[1] && seed->lba[0]!=seed->lba[1] &&
        kui_stage_crc32(0u,seed,KUI_TOY_REPORT_CRC_OFFSET)==seed->crc32;
}

static void toy_trace_prepare(uint32_t firmware) {
    static const uint8_t toy_sha[32]={
        0xee,0x68,0xa3,0x26,0xda,0x96,0x4f,0xec,0x58,0x92,0xb7,0x22,0x63,0x11,0xf2,0x29,
        0x3f,0xd3,0xa0,0xbb,0xf5,0xba,0xc4,0x96,0xec,0x82,0x99,0x19,0x64,0xd4,0x9b,0xbd};
    if(plan.ip_crc32!=KUI_VMU_LAUNCH_TOY_IP_CRC ||
       plan.exec_crc32!=KUI_VMU_LAUNCH_TOY_EXEC_CRC ||
       plan.exec_bytes!=KUI_VMU_LAUNCH_TOY_EXEC_BYTES || memcmp(plan.exec_sha256,toy_sha,sizeof(toy_sha)) ||
       !toy_ranges_valid() || !toy_report_valid()) stopped("TOY RELEASE OR REPORT REFUSED",5u);
    if(toy_word(KUI_TOY_BOOT_HEAP_POOL)!=KUI_TOY_BOOT_HEAP_INIT ||
       toy_word(KUI_TOY_BOOT_RESET_CALLBACK_POOL)!=KUI_TOY_BOOT_RESET_CALLBACK_NATIVE ||
       *(volatile const uint16_t *)KUI_TOY_BOOT_RESET_REGISTER_SITE!=KUI_TOY_BOOT_RESET_REGISTER_WORD ||
       *(volatile const uint16_t *)KUI_TOY_BOOT_RESET_INVOKE_SITE!=KUI_TOY_BOOT_RESET_INVOKE_WORD)
        stopped("TOY HEAP OR RESET HOOK REFUSED",6u);
    disc_toy_firmware=firmware;
    toy_publish(KUI_TOY_BOOT_HEAP_POOL,(uint32_t)(uintptr_t)kui_disc_toy_heap_hook);
}

static bool worker_entry(uint32_t address, uint32_t binary_end) {
    return !(address&1u) && address>=KUI_DISC_TOY_TRACE_LINK+sizeof(struct kui_disc_toy_trace_exports) &&
        address<binary_end;
}

void kui_disc_toy_install(const uint32_t call[4]) {
    uint32_t sr;
    __asm__ volatile("stc sr,%0":"=r"(sr));
    if((sr&0xf0u)!=0xf0u || call[0]!=0x8c04e924u ||
       call[1]!=KUI_TOY_BOOT_HEAP_BEGIN_VALUE || call[2]!=KUI_TOY_BOOT_HEAP_BYTES_VALUE ||
       (call[3]&3u) || call[3]<0x8c00c100u || call[3]>0x8c00f400u ||
       toy_word(KUI_DISC_TOY_TRACE_GD_VECTOR)!=disc_toy_firmware ||
       toy_word(KUI_TOY_BOOT_RESET_CALLBACK_POOL)!=KUI_TOY_BOOT_RESET_CALLBACK_NATIVE ||
       toy_word(0x8c117dbcu)!=0x8c0c5ac8u || toy_word(0x8c0c5ac8u)!=0x8c0b3ef2u ||
       toy_word(0x8c0c5accu)!=0x8c0b3fc0u || toy_word(0x8c117de0u)!=0x8c117dc0u ||
       toy_word(0x8c117dc0u)!=KUI_TOY_BOOT_HEAP_BEGIN_VALUE ||
       toy_word(KUI_TOY_BOOT_HEAP_BEGIN_VALUE)!=0x8c117dc0u ||
       toy_word(KUI_TOY_BOOT_HEAP_BEGIN_VALUE+4u)!=0x76de6u)
        stopped("TOY NATIVE HEAP STATE REFUSED",call[0]);

    uint32_t bytes=(uint32_t)(disc_toy_worker_end-disc_toy_worker_begin);
    struct kui_disc_toy_trace_exports exports;
    if(bytes<sizeof(exports) || bytes>0x8000u) stopped("TOY TRACE WORKER SIZE REFUSED",bytes);
    memcpy(&exports,disc_toy_worker_begin,sizeof(exports));
    if(exports.binary_end!=KUI_DISC_TOY_TRACE_LINK+((bytes+31u)&~31u) ||
       exports.bss_end<exports.binary_end || exports.bss_end>0xacfe0000u-4096u ||
       exports.config<exports.binary_end || exports.config>exports.bss_end-sizeof(struct kui_disc_toy_trace_config) ||
       !worker_entry(exports.proxy,exports.binary_end) || !worker_entry(exports.finish,exports.binary_end) ||
       exports.heap_guard!=exports.finish) stopped("TOY TRACE WORKER LAYOUT REFUSED",exports.config);

    void *(*allocate)(uint32_t)=(void *)(uintptr_t)0x8c0b3edeu;
    uint32_t lease=(uint32_t)(uintptr_t)allocate(KUI_DISC_TOY_TRACE_BYTES);
    if(lease!=KUI_DISC_TOY_TRACE_LEASE || toy_word(0x8cfcffe4u)!=0x1801u ||
       toy_word(KUI_TOY_BOOT_HEAP_BEGIN_VALUE+4u)!=0x755e5u)
        stopped("TOY TRACE LEASE REFUSED",lease);
    for(uint32_t at=lease;at<lease+exports.bss_end-KUI_DISC_TOY_TRACE_LINK+4096u;at+=32u)
        __asm__ volatile("ocbp @%0"::"r"(at):"memory");
    memset((void *)KUI_DISC_TOY_TRACE_LINK,0, exports.bss_end-KUI_DISC_TOY_TRACE_LINK);
    memcpy((void *)KUI_DISC_TOY_TRACE_LINK,disc_toy_worker_begin,bytes);
    if(memcmp((const void *)KUI_DISC_TOY_TRACE_LINK,disc_toy_worker_begin,bytes))
        stopped("TOY TRACE WORKER COPY FAILED",lease);
    struct kui_disc_toy_trace_config *config=(void *)(uintptr_t)exports.config;
    config->firmware=disc_toy_firmware;
    config->display=display;
    memcpy(config->movie,disc_toy_ranges.movie,sizeof(config->movie));
    config->report=disc_toy_report_seed;

    /* Game callback registration follows this allocator call. The first hook
     * also keeps a reinitialization from reclaiming the executing lease. */
    toy_publish(KUI_TOY_BOOT_HEAP_POOL,exports.heap_guard);
    toy_publish(KUI_TOY_BOOT_RESET_CALLBACK_POOL,exports.finish);
    toy_publish(KUI_GD_VECTOR_ADDRESS,exports.proxy);
    if(toy_word(KUI_DISC_TOY_TRACE_GD_VECTOR)!=exports.proxy)
        stopped("TOY GD TRACE VECTOR REFUSED",exports.proxy);
}
