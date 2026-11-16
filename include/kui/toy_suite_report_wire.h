/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_TOY_SUITE_REPORT_WIRE_H
#define KUI_TOY_SUITE_REPORT_WIRE_H

#define KUI_TOY_REPORT_MAGIC0 0x5449554b
#define KUI_TOY_REPORT_MAGIC1 0x31305352
#define KUI_TOY_REPORT_VERSION 1
#define KUI_TOY_REPORT_WIRE_BYTES 64
#define KUI_TOY_REPORT_TEST_OFFSET 16
#define KUI_TOY_REPORT_LBA_OFFSET 20
#define KUI_TOY_REPORT_PLACEHOLDER_OFFSET 28
#define KUI_TOY_REPORT_CID_OFFSET 36
#define KUI_TOY_REPORT_HC_OFFSET 52
#define KUI_TOY_REPORT_RESERVED_OFFSET 56
#define KUI_TOY_REPORT_CRC_OFFSET 60
#define KUI_TOY_REPORT_SECTOR_BYTES 512
#define KUI_TOY_REPORT_FILE_BYTES 1024

#ifndef __ASSEMBLER__
#include <stddef.h>
#include <stdint.h>

/* Little-endian ABI. The first four words identify the all-zero template. */
struct kui_toy_report_wire {
    uint32_t magic[2],version,bytes,test,lba[2],placeholder_crc[2];
    uint8_t cid[16];
    uint32_t high_capacity,reserved,crc32;
};
_Static_assert(sizeof(struct kui_toy_report_wire)==KUI_TOY_REPORT_WIRE_BYTES,"Toy report wire size");
_Static_assert(offsetof(struct kui_toy_report_wire,test)==KUI_TOY_REPORT_TEST_OFFSET,"Toy report test offset");
_Static_assert(offsetof(struct kui_toy_report_wire,lba)==KUI_TOY_REPORT_LBA_OFFSET,"Toy report LBA offset");
_Static_assert(offsetof(struct kui_toy_report_wire,placeholder_crc)==KUI_TOY_REPORT_PLACEHOLDER_OFFSET,
    "Toy report placeholder CRC offset");
_Static_assert(offsetof(struct kui_toy_report_wire,cid)==KUI_TOY_REPORT_CID_OFFSET,"Toy report CID offset");
_Static_assert(offsetof(struct kui_toy_report_wire,high_capacity)==KUI_TOY_REPORT_HC_OFFSET,"Toy report HC offset");
_Static_assert(offsetof(struct kui_toy_report_wire,reserved)==KUI_TOY_REPORT_RESERVED_OFFSET,"Toy report reserved offset");
_Static_assert(offsetof(struct kui_toy_report_wire,crc32)==KUI_TOY_REPORT_CRC_OFFSET,"Toy report CRC offset");
#endif
#endif
