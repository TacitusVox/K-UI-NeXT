/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_DREAMPI_H
#define KUI_DREAMPI_H
#include "kui/apps.h"

/* An explicitly requested call to a locally attached DreamPi. */
int kui_dreampi_open(kui_log_fn log, kui_cancel_fn cancel, char problem[128]);
bool kui_dreampi_online(void);
void kui_dreampi_close(void);
#endif
