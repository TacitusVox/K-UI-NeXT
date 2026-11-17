/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_SPLASH_H
#define KUI_SPLASH_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define KUI_SPLASH_FRAME_BYTES (640u*480u*2u)
/* Copy the 640x480 RGB565 startup art to an off-screen framebuffer. */
void kui_splash_draw(uint16_t *frame);
/* Free the startup art's RAM copy once the splash has ended (UI thread). */
void kui_splash_release(void);
/* The CD boot menu has separate artwork; dim in RAM before writing pixels. */
void kui_boot_splash_draw(uint16_t *frame);
void kui_boot_splash_draw_dimmed(uint16_t *frame);
/* Inflate a tools/build_splash.py zlib stream into a 640x480 RAM frame. */
bool kui_splash_art_inflate(const uint8_t *data, size_t bytes, uint16_t *frame);
/* Allocate and inflate one; NULL without RAM or on any damage. */
uint16_t *kui_splash_art_decode(const uint8_t *data, size_t bytes);
#endif
