/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_MODE1_ERASURE_H
#define KUI_MODE1_ERASURE_H
#include <stdbool.h>
#include <stdint.h>

/* The most lost bytes one raw sector may carry and still be attempted. */
#define KUI_MODE1_ERASURE_MAX 32u

/* Fill bytes of one raw 2352-byte Mode 1 sector that are known to be lost,
 * at known positions, from what the sector itself still says about them:
 * the fixed sync pattern, the address expected at fad, the zero intermediate
 * field, and the P and Q parity, each of whose code words XORs to zero.
 * A code word with exactly one lost byte gives that byte back; filling it
 * may leave another code word with one, and so on until nothing moves.
 *
 * Returns true if every lost byte of the part the EDC covers and of the EDC
 * itself (positions below 2068) was filled. Lost parity bytes may stay.
 * This invents nothing and proves nothing: the caller must still pass the
 * sector through its address and EDC check before using it. Positions must
 * be below 2352; count above KUI_MODE1_ERASURE_MAX is refused. */
bool kui_mode1_fill_lost(uint8_t raw[2352], const uint16_t *lost, unsigned count, uint32_t fad);

/* Whether every P and Q code word of the sector XORs to zero: the one
 * relation the fill uses. An image whose raw sectors carry no real parity
 * fails this, and the fill cannot work on it. */
bool kui_mode1_parity_holds(const uint8_t raw[2352]);

#endif
