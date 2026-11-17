/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_WCE_PROBE_H
#define KUI_WCE_PROBE_H
/* The Windows CE data probe (docs/wince-data-probe.md): a test build of the
 * generic ce-boot.kui that records what each game asks of the disc and of
 * the controller ports, and on A+B+X+Y+Start (or a reader fault) saves those
 * counters and the console's 16 MiB of RAM into one session slot of
 * 0:/KUI/WCEDATA/WCEDATA.BIN, a file the owner copies onto the card blank.
 *
 * File layout, all little-endian, 512-byte sectors:
 *   sector 0                      file header (struct kui_wce_probe_file)
 *   sector first_slot+s*slot_sectors   slot s:
 *     +0                          slot header (struct kui_wce_probe_slot)
 *     +1 .. +STAGE_SECTORS        stage report, written before Windows CE starts
 *     +TERMINAL_SECTOR ..         terminal report, written at the end
 *     +DUMP_SECTOR ..             RAM dump: the non-zero 4 KiB pages of
 *                                 0x8c000000..0x8d000000 in address order,
 *                                 named by the terminal report's page map.
 * The blank file holds only the file header and every slot's EMPTY header;
 * everything else is zero. The stage writes only into a slot whose header it
 * has just read back as this file's EMPTY header for that slot. */

#define KUI_WCE_PROBE_VERSION 1u
#define KUI_WCE_PROBE_SECTOR 512u
#define KUI_WCE_PROBE_HEADER_BYTES 64u
#define KUI_WCE_PROBE_STAGE_SECTOR 1u
#define KUI_WCE_PROBE_STAGE_SECTORS 7u
#define KUI_WCE_PROBE_TERMINAL_SECTOR 8u
#define KUI_WCE_PROBE_TERMINAL_SECTORS 8u
#define KUI_WCE_PROBE_DUMP_SECTOR 16u
#define KUI_WCE_PROBE_RAM_BEGIN 0x8c000000u
#define KUI_WCE_PROBE_RAM_BYTES 0x01000000u
#define KUI_WCE_PROBE_PAGE_BYTES 4096u
#define KUI_WCE_PROBE_PAGES (KUI_WCE_PROBE_RAM_BYTES/KUI_WCE_PROBE_PAGE_BYTES)
#define KUI_WCE_PROBE_DUMP_SECTORS (KUI_WCE_PROBE_RAM_BYTES/KUI_WCE_PROBE_SECTOR)
#define KUI_WCE_PROBE_SLOT_SECTORS (KUI_WCE_PROBE_DUMP_SECTOR+KUI_WCE_PROBE_DUMP_SECTORS)
#define KUI_WCE_PROBE_FIRST_SLOT 2048u
/* The terminal report's page map: one bit per 4 KiB page, at this offset. */
#define KUI_WCE_PROBE_PAGE_MAP_OFFSET 3584u
#define KUI_WCE_PROBE_PAGE_MAP_BYTES (KUI_WCE_PROBE_PAGES/8u)
/* A slot is written in at most this many card runs; the stage refuses a
 * slot that the file's allocation splits further. */
#define KUI_WCE_PROBE_SLOT_RUNS 8u
/* The data file's allocation is mapped once, up to this many runs. */
#define KUI_WCE_PROBE_FILE_RUNS 128u

enum kui_wce_probe_state {
    KUI_WCE_PROBE_EMPTY = 0u,
    KUI_WCE_PROBE_CLAIMED = 1u,  /* claimed by the stage (written before its report) */
    KUI_WCE_PROBE_DUMPING = 2u,  /* unused: a cut-off dump stays CLAIMED */
    KUI_WCE_PROBE_COMPLETE = 3u  /* dump finished */
};
enum kui_wce_probe_reason {
    KUI_WCE_PROBE_MENU = 1u,     /* the game's BIOS menu call: A+B+X+Y+Start */
    KUI_WCE_PROBE_FAULT = 2u     /* a reader fault that stops the game */
};

#ifndef __ASSEMBLER__
#include <stddef.h>
#include <stdint.h>

/* Sector 0 of WCEDATA.BIN. */
struct kui_wce_probe_file {
    char magic[8];               /* "KUIWCED1" */
    uint32_t version,header_bytes,slot_count,slot_sectors,first_slot,dump_sectors;
    uint8_t file_id[16];
    uint32_t flags,reserved[2],crc32; /* CRC-32 of the 60 bytes before it */
};
_Static_assert(sizeof(struct kui_wce_probe_file)==KUI_WCE_PROBE_HEADER_BYTES,"WCE probe file header");

/* Sector +0 of every slot. */
struct kui_wce_probe_slot {
    char magic[8];               /* "KUIWCES1" */
    uint32_t version,header_bytes,slot,state,sequence,rtc;
    uint8_t file_id[16];
    uint32_t pages,reason,reserved,crc32; /* pages, reason: always 0 (see the terminal report) */
};
_Static_assert(sizeof(struct kui_wce_probe_slot)==KUI_WCE_PROBE_HEADER_BYTES,"WCE probe slot header");

/* What the stage hands the reader: where the slot's terminal report and dump
 * go (runs[] covers the slot from its terminal sector to its end, in order),
 * and the slot header to write when the dump is complete, prepared by the
 * stage so the reader needs no checksum code. */
struct kui_wce_probe_run {uint32_t lba,sectors;};
struct kui_wce_probe_seed {
    uint32_t magic,version,slot,run_count;          /* magic 'WCES' */
    uint32_t slot_header_lba,sequence,stage_rtc,flags;
    uint8_t file_id[16];
    struct kui_wce_probe_run runs[KUI_WCE_PROBE_SLOT_RUNS];
    struct kui_wce_probe_slot complete;
    uint32_t reserved,crc32;                        /* CRC-32 of the words before */
};
#define KUI_WCE_PROBE_SEED_MAGIC 0x53454357u
_Static_assert(sizeof(struct kui_wce_probe_seed)==184u,"WCE probe seed");

/* The reader's counters, saved in the terminal report and present in the
 * dump. Counters saturate instead of wrapping. */
#define KUI_WCE_PROBE_FUNCTIONS 16u
#define KUI_WCE_PROBE_COMMAND_FIRST 16u /* GD commands 16..47 */
#define KUI_WCE_PROBE_COMMANDS 32u
#define KUI_WCE_PROBE_AUDIO_FIRST 4u
#define KUI_WCE_PROBE_AUDIO_RING 8u
#define KUI_WCE_PROBE_MAPLE_UNITS 12u /* ports A..D x {main device, slot 1, slot 2} */
#define KUI_WCE_PROBE_MAPLE_COMMANDS 8u
struct kui_wce_probe_audio {
    uint8_t command,result;uint16_t call; /* call: low 16 bits of the GD call count */
    uint32_t first,last,repeat;
};
struct kui_wce_probe_counters {
    uint32_t magic,version,bytes,calls;              /* magic 'WCEC' */
    uint32_t rtc_first,rtc_first_read,rtc_first_audio,rtc_last;
    uint16_t functions[KUI_WCE_PROBE_FUNCTIONS];     /* GD calls by function */
    uint16_t commands[KUI_WCE_PROBE_COMMANDS];       /* accepted REQUESTs by command-16 */
    uint32_t refused,refused_last,refused_mask[2];   /* refused REQUESTs: count, last, commands 0..63 */
    uint32_t read_requests,read_sectors,largest_read,unknown_commands;
    uint32_t audio_count,maple_samples,maple_lists,maple_frames;
    uint32_t maple_bad,maple_last_list;
    struct kui_wce_probe_audio audio_first[KUI_WCE_PROBE_AUDIO_FIRST];
    struct kui_wce_probe_audio audio_ring[KUI_WCE_PROBE_AUDIO_RING];
    /* Frames seen in the Maple DMA lists, by recipient and command (saturating
     * at 255): commands 1 device info, 9 condition, 10 media info, 11 block
     * read, 12 block write, 13 write complete (these three for storage,
     * function 0x02000000, only), 14 set condition, 0 any other. */
    uint8_t maple[KUI_WCE_PROBE_MAPLE_UNITS][KUI_WCE_PROBE_MAPLE_COMMANDS];
    uint32_t maple_functions[KUI_WCE_PROBE_MAPLE_UNITS]; /* function codes seen, OR'd */
};
#define KUI_WCE_PROBE_COUNTERS_MAGIC 0x43454357u
_Static_assert(sizeof(struct kui_wce_probe_counters)==520u,"WCE probe counters");

/* The stage report, slot sectors +1..+7. */
struct kui_wce_probe_stage {
    char magic[8];               /* "KUIWCEST" */
    uint32_t version,bytes;
    char build[16];              /* the module's build ID */
    uint32_t rtc,transport,card_high_capacity,card_blocks;
    uint32_t partition_start,partition_end,track_count,extent_count;
    uint32_t map_slots,kernel_bytes,kernel_crc32,kernel_head_crc32;
    uint32_t rom_header,rom_header_method,ram_start,ram_free;
    uint32_t ram_end,zero_begin,zero_end,file_runs;
    uint32_t slot,slot_runs,sequence,flags;
    uint8_t ip[256];             /* IP.BIN's first 256 bytes */
    uint8_t rom_header_bytes[84];
    uint32_t zero_high_begin,zero_high_end; /* the second cleared range, above the stage */
    uint32_t reserved[11];
};
_Static_assert(sizeof(struct kui_wce_probe_stage)<=KUI_WCE_PROBE_STAGE_SECTORS*KUI_WCE_PROBE_SECTOR,
    "WCE probe stage report");

/* The terminal report, slot sectors +8..+15. */
#define KUI_WCE_PROBE_REGISTERS 32u
#define KUI_WCE_PROBE_REGISTERS_READ 16u /* the rest are zero */
struct kui_wce_probe_terminal {
    char magic[8];               /* "KUIWCETR" */
    /* Menu: the BIOS menu command, caller and stack. Fault: the GD function,
     * and in caller the fault message's length + 1 (it names the message:
     * src/loader/retail_resident.c, tools/wce_probe_data.py FAULT_REASONS). */
    uint32_t version,bytes,reason,menu_command,caller,stack;
    uint32_t rtc,hook_fault,pages,dump_sectors;
    /* SR (in the hook: BL and IMASK forced), VBR, CCR MMUCR PTEH TTB, Maple
     * MDSTAR, ASIC ISTNRM, video SPG_CONTROL SPG_LOAD VO_CONTROL FB_R_CTRL
     * FB_R_SOF1, Holly IML2NRM IML4NRM IML6NRM; then 16 zero words. Every one
     * is a 32-bit register read without side effects. */
    uint32_t registers[KUI_WCE_PROBE_REGISTERS];
    struct kui_wce_probe_seed seed;
    struct kui_wce_probe_counters counters;
};
_Static_assert(offsetof(struct kui_wce_probe_terminal,counters)+sizeof(struct kui_wce_probe_counters)<=
    KUI_WCE_PROBE_PAGE_MAP_OFFSET,"WCE probe terminal report");

/* FAT32/exFAT lookup of the data file (src/core/wce_probe_fat.c), shared by
 * the stage and its host test. read() fills one 512-byte sector and returns
 * zero on success. */
typedef int (*kui_wce_probe_read)(void *context,uint32_t lba,uint8_t sector[512]);
struct kui_wce_probe_volume {
    uint32_t partition_start,partition_end;
    kui_wce_probe_read read;void *context;
};
struct kui_wce_probe_layout {
    uint32_t bytes,run_count;
    struct kui_wce_probe_run runs[KUI_WCE_PROBE_FILE_RUNS];
};
enum kui_wce_probe_fat_result {
    KUI_WCE_PROBE_FAT_OK = 0, KUI_WCE_PROBE_FAT_IO, KUI_WCE_PROBE_FAT_VOLUME,
    KUI_WCE_PROBE_FAT_MISSING, KUI_WCE_PROBE_FAT_CHAIN, KUI_WCE_PROBE_FAT_FRAGMENTED
};
/* Find 0:/KUI/WCEDATA/WCEDATA.BIN and map its allocation to card runs.
 * work must hold 512 bytes. */
enum kui_wce_probe_fat_result kui_wce_probe_fat_find(const struct kui_wce_probe_volume *volume,
    struct kui_wce_probe_layout *layout,uint8_t work[512]);
/* The card sector of file sector `sector`, or 0 when it is past the file. */
uint32_t kui_wce_probe_lba(const struct kui_wce_probe_layout *layout,uint32_t sector);
/* Runs covering file sectors [first, first+count); 0 if they need more than
 * `capacity` runs or fall outside the file. */
uint32_t kui_wce_probe_runs(const struct kui_wce_probe_layout *layout,uint32_t first,uint32_t count,
    struct kui_wce_probe_run *out,uint32_t capacity);
uint32_t kui_wce_probe_crc32(uint32_t crc,const void *data,size_t bytes);

/* Claiming a slot (the stage, and its host test): the file header, then the
 * first EMPTY slot whose stage report and terminal/dump area fit their runs,
 * each run's first sector read back blank. Read-only. */
enum kui_wce_probe_claim_result {
    KUI_WCE_PROBE_CLAIM_OK = 0, KUI_WCE_PROBE_CLAIM_IO, KUI_WCE_PROBE_CLAIM_NOT_FILE,
    KUI_WCE_PROBE_CLAIM_FULL, KUI_WCE_PROBE_CLAIM_SLOT_HEADER, KUI_WCE_PROBE_CLAIM_NOT_BLANK
};
struct kui_wce_probe_claim {
    struct kui_wce_probe_file file;
    uint32_t slot,used,header_lba,stage_count,run_count;
    struct kui_wce_probe_run stage_runs[KUI_WCE_PROBE_STAGE_SECTORS];
    struct kui_wce_probe_run runs[KUI_WCE_PROBE_SLOT_RUNS];
};
enum kui_wce_probe_claim_result kui_wce_probe_claim_find(const struct kui_wce_probe_volume *volume,
    const struct kui_wce_probe_layout *layout,uint8_t work[512],struct kui_wce_probe_claim *out);
void kui_wce_probe_slot_header(struct kui_wce_probe_slot *h,uint32_t slot,uint32_t state,
    uint32_t sequence,uint32_t rtc,const uint8_t file_id[16]);
/* The reader's seed for a claimed slot, with its COMPLETE header ready. */
void kui_wce_probe_seed_build(const struct kui_wce_probe_claim *claim,uint32_t rtc,uint32_t flags,
    struct kui_wce_probe_seed *seed);

#if KUI_WCE_PROBE && defined(KUI_RETAIL_CE) && defined(KUI_RETAIL_ASYNC)
/* The reader side (src/loader/wce_probe_reader.inc). */
struct retail_display_state;
extern struct kui_wce_probe_seed kui_wce_probe_seed;
void kui_wce_probe_record(uint32_t function,uint32_t command,int32_t result);
void kui_wce_probe_terminal(const struct retail_display_state *display,uint32_t reason,
    uint32_t command,uint32_t caller,uint32_t stack) __attribute__((noreturn));
#endif
#endif
#endif
