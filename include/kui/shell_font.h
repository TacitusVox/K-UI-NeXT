/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_SHELL_FONT_H
#define KUI_SHELL_FONT_H
#include <stdbool.h>
#include <stdint.h>

#define KUI_SHELL_FONT_SMALL_HEIGHT 18u
#define KUI_SHELL_FONT_LARGE_HEIGHT 24u
#define KUI_SHELL_FONT_LETTER_SPACING 1u
/* In the GUI, input is UTF-8 with Japanese glyphs. Bootstrap and standalone
 * helpers use the original printable ASCII font. Unsupported characters use
 * '?'. The letter gap is 1px. NULL/empty strings have width zero. Width
 * saturates at UINT_MAX. Glyphs fit inside their advance, with no kerning. */
unsigned kui_shell_font_width(const char *text,bool large);
/* GUI glyph advance without letter spacing. Zero denotes an unsupported scalar. */
unsigned kui_shell_font_advance(uint32_t scalar,bool large);
/* Draw into a 640x480 RGB565 framebuffer. x/y are the line box's top-left;
 * clipping includes negative/extreme coordinates. No allocation, I/O or KOS
 * calls. Four-bit coverage blends with existing pixels; transparency skips
 * writes and full coverage writes the exact requested color. */
void kui_shell_font_draw(uint16_t *frame,int x,int y,uint16_t rgb565,
                         const char *text,bool large);
/* Same glyphs and coverage, restricted to framebuffer rows in
 * [clip_top,clip_bottom). Bounds are clamped to 0..480; an empty or reversed
 * range draws nothing. Pixels outside the range are left untouched. */
void kui_shell_font_draw_clipped(uint16_t *frame,int x,int y,uint16_t rgb565,
                                 const char *text,bool large,int clip_top,int clip_bottom);
/* Compact, opaque glyphs for the console header: no letter gap, coverage
 * below half skips writes and the remaining pixels use the exact color. */
unsigned kui_shell_font_width_crisp(const char *text,bool large);
void kui_shell_font_draw_crisp(uint16_t *frame,int x,int y,uint16_t rgb565,
                               const char *text,bool large);
#endif
