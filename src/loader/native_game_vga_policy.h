/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_NATIVE_GAME_VGA_POLICY_H
#define KUI_NATIVE_GAME_VGA_POLICY_H
/* The same selection reaches both the SH-4 assembly handoff and its C stage.
 * The collection owns its own native startup and its exact source proofs. */
#if (KUI_NATIVE_GAME_VGA_ORDINARY || KUI_RETAIL_ZSO || KUI_NATIVE_VMU_LITE || \
     KUI_NATIVE_STANDALONE_READER || KUI_VMU_NATIVE_BACKGROUND) && \
    !defined(KUI_RETAIL_CE) && !defined(KUI_RETAIL_SD_BENCH) && !KUI_SMASH_NATIVE_MENU && \
    !KUI_SMASH_NATIVE_CHILD && !KUI_SMASH_MENU_LOADER && !KUI_RETAIL_MDK2_CDDA && !KUI_RETAIL_TOY_PILOT
#define KUI_NATIVE_GAME_VGA_STAGE 1
#else
#define KUI_NATIVE_GAME_VGA_STAGE 0
#endif
#endif
