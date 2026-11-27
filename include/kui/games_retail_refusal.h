/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_GAMES_RETAIL_REFUSAL_H
#define KUI_GAMES_RETAIL_REFUSAL_H
/* Why the last Games launch was not prepared, as it was logged after
 * "Retail boot: ", or an empty string when it was prepared. The launch's
 * timing lines follow that line in the log; the console's worker says it
 * again after them, so that the log page a refused launch opens ends on it. */
const char *kui_games_retail_last_refusal(void);
#endif
