/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_ARMADA_CE_VMU_LAYOUT_H
#define KUI_ARMADA_CE_VMU_LAYOUT_H

/* C/assembler/linker-preprocessor scalars. The default is the accepted
 * 24 KiB readonly reservation. The separate experimental 48 KiB profile
 * reserves space only: it grants no WRITE, storage or worker capability.
 * The separate VC2 persistent profile adds16KiB lower working BSS, retaining
 * the48KiB code image/entry. All are installed before NK; OEM globals still
 * begin at0x8cfd0000. */
#ifndef KUI_ARMADA_CE_MAPLE_WRITE_LAYOUT
#define KUI_ARMADA_CE_MAPLE_WRITE_LAYOUT 0
#endif
#if KUI_ARMADA_CE_MAPLE_WRITE_LAYOUT != 0 && KUI_ARMADA_CE_MAPLE_WRITE_LAYOUT != 1
#error "CE VMU WRITE layout must be explicitly disabled or enabled"
#endif
#if KUI_SMASH_SWIRL_VMU
/* Includes Swirl's separately copied Maple text. The existing working BSS,
 * initialized helper and fixed stacks retain their original addresses. */
#define KUI_ARMADA_CE_VMU_RESERVE_BEGIN 0x8cfb8000
#elif KUI_VC2_VMU_PERSIST
#define KUI_ARMADA_CE_VMU_RESERVE_BEGIN 0x8cfc0000
#elif KUI_ARMADA_CE_MAPLE_WRITE_LAYOUT && KUI_ARMADA_CE_CDDA_VMU && KUI_ARMADA_CE_MAPLE_STEADY
/* Combined CD music and writable A2 with the steady-state controller poll:
 * the helper outgrew the 48 KiB image. Below it, the CD music work area holds
 * only the 2 KiB data spill at 0x8cfc0000 in this layout (the controller's
 * transport admits only its source ring), so the image starts 8 KiB lower.
 * The fixed P2 handoff, services, stacks and logs keep their addresses. */
#define KUI_ARMADA_CE_VMU_RESERVE_BEGIN 0x8cfc2000
#elif KUI_ARMADA_CE_MAPLE_WRITE_LAYOUT
#define KUI_ARMADA_CE_VMU_RESERVE_BEGIN 0x8cfc4000
#else
#define KUI_ARMADA_CE_VMU_RESERVE_BEGIN 0x8cfca000
#endif
#define KUI_ARMADA_CE_VMU_RESERVE_END 0x8cfd0000
#define KUI_ARMADA_CE_VMU_RESERVE_BYTES (KUI_ARMADA_CE_VMU_RESERVE_END-KUI_ARMADA_CE_VMU_RESERVE_BEGIN)
#define KUI_ARMADA_CE_VMU_HOOK_P2 (KUI_ARMADA_CE_VMU_RESERVE_BEGIN+0x20000000)
#define KUI_ARMADA_CE_VMU_STACK_BOTTOM 0xacfcf800
#define KUI_ARMADA_CE_VMU_STACK_TOP 0xacfcfe00
#define KUI_ARMADA_CE_VMU_HANDOFF_P2 0xacfce200
#define KUI_ARMADA_CE_VMU_HANDOFF_BYTES 1272
#define KUI_ARMADA_CE_VMU_SERVICES_ADDRESS 0xacfce700
#define KUI_ARMADA_CE_VMU_SERVICES_SIZE 32
#define KUI_ARMADA_CE_VMU_WORKER_STACK_BOTTOM 0xacfce800
#define KUI_ARMADA_CE_VMU_WORKER_STACK_TOP 0xacfcf800
/* Diagnostic hook timing (KUI_ARMADA_CE_HOOK_TIMING builds only): after the
 * A2 log, inside the zeroed reservation. Words: TMU0 ticks spent in the
 * helper's PRE/POST phases (interrupts masked), phases, the longest phase,
 * and steady-state controller polls (KUI_ARMADA_CE_MAPLE_STEADY).
 * The stage zeroes the reservation before NK; nothing else uses these. */
#define KUI_ARMADA_CE_HOOK_TIMING_P2 0xacfcffc0
#define KUI_ARMADA_CE_HOOK_TIMING_WORDS 4
#endif
