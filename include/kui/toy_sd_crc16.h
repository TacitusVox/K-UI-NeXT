/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_TOY_SD_CRC16_H
#define KUI_TOY_SD_CRC16_H

#define KUI_TOY_SD_CRC16_SLOT 15
#define KUI_TOY_SD_CRC16_TABLE_BYTES 1024

#ifndef __ASSEMBLER__
#include <stdbool.h>
#include <stdint.h>
typedef bool (*kui_toy_sd_crc16_check_fn)(uint8_t *area,uint32_t tail,bool wire,uint32_t lost);
bool kui_toy_sd_crc16_check(uint8_t *area,uint32_t tail,bool wire,uint32_t lost);
#endif
#endif
