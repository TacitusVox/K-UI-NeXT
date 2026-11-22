/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_WINCE_CDDA_WWP_KERNEL_H
#define KUI_WINCE_CDDA_WWP_KERNEL_H
#include "kui/wince_cdda_wwp_kernels.h"
#include <stdint.h>

/* Independently reviewed metadata for the exact supplied WWP US kernel
 * SHA256 189719d8d3763284f74c5e66704710b9f9968118cb58aeb0a0844bd121a629ab.
 * Whole-kernel admission must precede use. These are distinct ROM, shared
 * kernel and live-module address domains, never an inferred title match.
 * Review ranges and hashes: docs/evidence/world-party-kernel-contracts-2026-10-15.md.
 *
 * Another kernel of World Party's group (kui/wince_cdda_wwp_kernels.h,
 * selected by KUI_WINCE_CDDA_WWP_KERNEL) carries this nk.exe build: its NK
 * globals below move by its data block's shift; its ROM header, RAM start and
 * free and the sound DLLs' ROM table entries are its own. World Party's own
 * record has shift 0 and exactly the values reviewed here.
 */
#define KUI_WWP_CE_SHIFT ((uint32_t)(KUI_WINCE_CDDA_WWP_DATA_SHIFT))
#define KUI_WWP_CE_ROMHDR ((uint32_t)KUI_WINCE_CDDA_WWP_ROMHDR)
#define KUI_WWP_CE_ROMHDR_CRC ((uint32_t)KUI_WINCE_CDDA_WWP_ROMHDR_CRC)
#define KUI_WWP_CE_RAM_START ((uint32_t)KUI_WINCE_CDDA_WWP_RAM_START)
#define KUI_WWP_CE_RAM_FREE ((uint32_t)KUI_WINCE_CDDA_WWP_RAM_FREE)
#define KUI_WWP_CE_RAM_END UINT32_C(0x8cfd0000)
#define KUI_WWP_CE_OEM_BYTES UINT32_C(0x00030000)
#define KUI_WWP_CE_POOL_BEGIN UINT32_C(0x8cf50000)
#define KUI_WWP_CE_POOL_END KUI_WWP_CE_RAM_END

#define KUI_WWP_CE_KDATA (UINT32_C(0x8c1d1800)+KUI_WWP_CE_SHIFT)
#define KUI_WWP_CE_CURRENT_PROCESS (UINT32_C(0x8c1d1890)+KUI_WWP_CE_SHIFT)
#define KUI_WWP_CE_PROCESS_BASE (UINT32_C(0x8c1da6e0)+KUI_WWP_CE_SHIFT)
#define KUI_WWP_CE_PROCESS_STRIDE UINT32_C(0x9c)
#define KUI_WWP_CE_PROCESS_COUNT 32u
#define KUI_WWP_CE_MODULE_HEAD (UINT32_C(0x8c1d1b24)+KUI_WWP_CE_SHIFT)
#define KUI_WWP_CE_HEAP_LOW (UINT32_C(0x8c1d9430)+KUI_WWP_CE_SHIFT)
#define KUI_WWP_CE_HEAP_HIGH (UINT32_C(0x8c1d9434)+KUI_WWP_CE_SHIFT)
#define KUI_WWP_CE_APISETS (UINT32_C(0x8c1dba80)+KUI_WWP_CE_SHIFT)
#define KUI_WWP_CE_IRQ_PENDING (UINT32_C(0x8c1d1b40)+KUI_WWP_CE_SHIFT)
#define KUI_WWP_CE_IRQ_RING (UINT32_C(0x8c1d1aa8)+KUI_WWP_CE_SHIFT)
#define KUI_WWP_CE_IRQ_RESCHEDULE (UINT32_C(0x8c1d1884)+KUI_WWP_CE_SHIFT)
#define KUI_WWP_CE_IRQ_TABLE (UINT32_C(0x8c1d1c04)+KUI_WWP_CE_SHIFT)
#define KUI_WWP_CE_WIN32_RECORD UINT32_C(0x8c011b20)
#define KUI_WWP_CE_WIN32_METHODS UINT32_C(0x8c0118e0)
#define KUI_WWP_CE_CALLBACK_SENTINEL UINT32_C(0x8c011aa4)
#define KUI_WWP_CE_COREDLL_TOC ((uint32_t)KUI_WINCE_CDDA_WWP_COREDLL_TOC)
#define KUI_WWP_CE_SNDCORE_TOC ((uint32_t)KUI_WINCE_CDDA_WWP_SNDCORE_TOC)
#define KUI_WWP_CE_DSOUND_TOC ((uint32_t)KUI_WINCE_CDDA_WWP_DSOUND_TOC)

/* Same geometry as the existing software walker and module binder, proved
 * against this kernel's own instructions rather than inherited by rebase. */
#define KUI_WWP_CE_SLOT_SHIFT 25u
#define KUI_WWP_CE_BLOCK_SHIFT 16u
#define KUI_WWP_CE_BLOCK_MASK 0x1ffu
#define KUI_WWP_CE_PAGE_SHIFT 12u
#define KUI_WWP_CE_PAGE_MASK 15u
#define KUI_WWP_CE_MEMBLOCK_PAGES 12u
#define KUI_WWP_CE_MODULE_SELF 0u
#define KUI_WWP_CE_MODULE_NEXT 4u
#define KUI_WWP_CE_MODULE_ATTACH 12u
#define KUI_WWP_CE_MODULE_REFERENCES 16u
#define KUI_WWP_CE_MODULE_BASE 80u
#define KUI_WWP_CE_MODULE_FLAGS 84u
#define KUI_WWP_CE_MODULE_READER 96u
#define KUI_WWP_CE_MODULE_KIND 100u
#define KUI_WWP_CE_MODULE_SECTION_COUNT 112u
#define KUI_WWP_CE_MODULE_SPAN 124u
#define KUI_WWP_CE_MODULE_SECTIONS 176u
#define KUI_WWP_CE_SECTION_STRIDE 28u

_Static_assert(KUI_WWP_CE_IRQ_TABLE==KUI_WWP_CE_IRQ_PENDING+0xc4u,
    "reviewed WWP interrupt table geometry");
_Static_assert(KUI_WWP_CE_POOL_END-KUI_WWP_CE_POOL_BEGIN==0x80000u &&
    KUI_WWP_CE_RAM_END+KUI_WWP_CE_OEM_BYTES==0x8d000000u,
    "reviewed WWP RAM and OEM reservation");
#if KUI_WINCE_CDDA_WWP_KERNEL == KUI_WINCE_CDDA_WWP_KERNEL_WORMS_WORLD_PARTY
_Static_assert(KUI_WWP_CE_ROMHDR==0x8c1cd614u && KUI_WWP_CE_ROMHDR_CRC==0x935ae972u &&
    KUI_WWP_CE_RAM_START==0x8c1ce000u && KUI_WWP_CE_RAM_FREE==0x8c1dc000u &&
    KUI_WWP_CE_SHIFT==0u && KUI_WWP_CE_COREDLL_TOC==0x8c1cd688u &&
    KUI_WWP_CE_SNDCORE_TOC==0x8c1cd888u && KUI_WWP_CE_DSOUND_TOC==0x8c1cd8a8u,
    "World Party's record is its reviewed kernel contract");
#endif
#endif
