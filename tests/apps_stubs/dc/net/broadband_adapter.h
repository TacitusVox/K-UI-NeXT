/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_TEST_APPS_BBA_H
#define KUI_TEST_APPS_BBA_H
#include <stdint.h>
#define BBA_TX_NOWAIT 0
#define BBA_TX_OK 0
#define BBA_TX_AGAIN -2
#define RT_MII_BMSR 0x64u
#define RT_MII_LINK 0x0004u
int bba_init(void);
int bba_shutdown(void);
int bba_tx(const uint8_t *packet,int bytes,int wait);
#endif
