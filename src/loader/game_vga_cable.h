/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_GAME_VGA_CABLE_H
#define KUI_GAME_VGA_CABLE_H

#include <stdbool.h>
#include <stdint.h>

/* KOS configures SH-4 PA8 and PA9 as inputs when it checks the retail
 * Dreamcast A/V cable. The freestanding stage only samples those pins.
 * Leave the other port A pins, including SCI chip select, untouched. */
static inline bool kui_game_vga_connected(void) {
    const volatile uint32_t *control = (const volatile uint32_t *)(uintptr_t)0xff80002cu;
    const volatile uint16_t *data = (const volatile uint16_t *)(uintptr_t)0xff800030u;
    return (*control & 0x00050000u) == 0 && (*data & 0x0300u) == 0;
}

#endif
