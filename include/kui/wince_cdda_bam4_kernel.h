/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_WINCE_CDDA_BAM4_KERNEL_H
#define KUI_WINCE_CDDA_BAM4_KERNEL_H
#include "kui/wince_cdda_bam4_kernels.h"
#include <stdint.h>

/* The exact supplied Bust-A-Move 4 US kernel (SHA256 2a8d24738bb5ba0687fa56ad33e814fe
 * 798158f9cb8cacfd4a8a20dea6bbb35d) and its CD music module's kernel contract.
 * Whole-kernel admission must precede use.
 *
 * Another kernel of Bust-A-Move 4's group (kui/wince_cdda_bam4_kernels.h,
 * selected by KUI_WINCE_CDDA_BAM4_KERNEL) carries this nk.exe build: its NK
 * globals below move by its data block's shift; its ROM header, RAM start and
 * free and the sound DLLs' ROM table entries are its own. Bust-A-Move 4's own
 * record has shift 0 and exactly the reviewed values. Only a full-stream CD
 * music module is built for another kernel (Makefile.armada-stream). */
#if KUI_WINCE_CDDA_BAM4_KERNEL != KUI_WINCE_CDDA_BAM4_KERNEL_BUST_A_MOVE_4 && !KUI_ARMADA_CE_STREAM
#error "Another kernel of Bust-A-Move 4's group has a full-stream CD music module only"
#endif
#define KUI_BAM4_CE_SHIFT ((uint32_t)(KUI_WINCE_CDDA_BAM4_DATA_SHIFT))
#define KUI_BAM4_CE_ROMHDR ((uint32_t)KUI_WINCE_CDDA_BAM4_ROMHDR)
#define KUI_BAM4_CE_ROMHDR_CRC ((uint32_t)KUI_WINCE_CDDA_BAM4_ROMHDR_CRC)
#define KUI_BAM4_CE_RAM_START ((uint32_t)KUI_WINCE_CDDA_BAM4_RAM_START)
#define KUI_BAM4_CE_RAM_FREE ((uint32_t)KUI_WINCE_CDDA_BAM4_RAM_FREE)

#define KUI_BAM4_CE_CURRENT_PROCESS (UINT32_C(0x8c1d1890)+KUI_BAM4_CE_SHIFT)
#define KUI_BAM4_CE_PROCESS_BASE (UINT32_C(0x8c1da6e0)+KUI_BAM4_CE_SHIFT)
#define KUI_BAM4_CE_MODULE_HEAD (UINT32_C(0x8c1d1b24)+KUI_BAM4_CE_SHIFT)
#define KUI_BAM4_CE_HEAP_LOW (UINT32_C(0x8c1d9430)+KUI_BAM4_CE_SHIFT)
#define KUI_BAM4_CE_HEAP_HIGH (UINT32_C(0x8c1d9434)+KUI_BAM4_CE_SHIFT)
#define KUI_BAM4_CE_APISETS (UINT32_C(0x8c1dba80)+KUI_BAM4_CE_SHIFT)
#define KUI_BAM4_CE_IRQ_PENDING (UINT32_C(0x8c1d1b40)+KUI_BAM4_CE_SHIFT)
#define KUI_BAM4_CE_IRQ_RING (UINT32_C(0x8c1d1aa8)+KUI_BAM4_CE_SHIFT)
#define KUI_BAM4_CE_IRQ_RESCHEDULE (UINT32_C(0x8c1d1884)+KUI_BAM4_CE_SHIFT)
#define KUI_BAM4_CE_IRQ_TABLE (UINT32_C(0x8c1d1c04)+KUI_BAM4_CE_SHIFT)
#define KUI_BAM4_CE_COREDLL_TOC ((uint32_t)KUI_WINCE_CDDA_BAM4_COREDLL_TOC)
#define KUI_BAM4_CE_SNDCORE_TOC ((uint32_t)KUI_WINCE_CDDA_BAM4_SNDCORE_TOC)
#define KUI_BAM4_CE_DSOUND_TOC ((uint32_t)KUI_WINCE_CDDA_BAM4_DSOUND_TOC)

_Static_assert(KUI_BAM4_CE_IRQ_TABLE==KUI_BAM4_CE_IRQ_PENDING+0xc4u,
    "reviewed Bust-A-Move 4 interrupt table geometry");
#if KUI_WINCE_CDDA_BAM4_KERNEL == KUI_WINCE_CDDA_BAM4_KERNEL_BUST_A_MOVE_4
_Static_assert(KUI_BAM4_CE_ROMHDR==0x8c1ce19cu && KUI_BAM4_CE_ROMHDR_CRC==0xe3b81ab3u &&
    KUI_BAM4_CE_RAM_START==0x8c1cf000u && KUI_BAM4_CE_RAM_FREE==0x8c1dc000u &&
    KUI_BAM4_CE_SHIFT==0u && KUI_BAM4_CE_COREDLL_TOC==0x8c1ce210u &&
    KUI_BAM4_CE_SNDCORE_TOC==0x8c1ce410u && KUI_BAM4_CE_DSOUND_TOC==0x8c1ce430u &&
    KUI_WINCE_CDDA_BAM4_KERNEL_BYTES==1830912 && KUI_WINCE_CDDA_BAM4_PREFIX_CRC==0x70513a3cu &&
    KUI_WINCE_CDDA_BAM4_BODY_CRC==0xf6902800u,
    "Bust-A-Move 4's record is its reviewed kernel contract");
#endif
#endif
