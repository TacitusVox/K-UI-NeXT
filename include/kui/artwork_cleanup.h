/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_ARTWORK_CLEANUP_H
#define KUI_ARTWORK_CLEANUP_H
#include "kui/probe.h"
/* Exclusive storage worker, card already mounted. Returns false when cleanup
 * was deferred/interrupted. removed counts only successful unlinks. */
bool kui_artwork_cleanup_mounted(unsigned *removed,kui_log_fn log,kui_cancel_fn cancel);
#endif
