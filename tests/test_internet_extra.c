/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/internet_extra.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    struct kui_games_disc_identity game = {.product="MK-51035", .title="CRAZY_TAXI", .region="U"};
    char url[384];
    assert(kui_internet_manual_pages(&game) == 13);
    assert(kui_internet_manual_page_url(&game, 0, url, sizeof(url)));
    assert(!strcmp(url,"https://archive.org/download/Crazy_Taxi_2000_U/page/n0_w512.jpg"));
    assert(kui_internet_manual_page_url(&game, 12, url, sizeof(url)));
    assert(!kui_internet_manual_page_url(&game, 13, url, sizeof(url)));
    assert(!kui_internet_manual_page_url(&game, 0, url, 8));
    assert(kui_internet_extra_url(&game, KUI_MEDIA_MANUAL, url, sizeof(url)));
    assert(strstr(url, "/dreamcast/crazy_taxi.pdf"));
    assert(kui_internet_extra_url(&game, KUI_MEDIA_PREVIEW, url, sizeof(url)));
    assert(!strcmp(url,"https://archive.org/download/SegaDreamcastVideoSnaps/Crazy%20Taxi%20%28USA%29.ogv"));
    assert(!kui_internet_extra_url(&game, KUI_MEDIA_PREVIEW, url, 12));
    strcpy(game.region,"E");
    assert(kui_internet_manual_pages(&game) != 13);
    assert(!kui_internet_extra_url(&game, KUI_MEDIA_MANUAL, url, sizeof(url)));
    assert(!kui_internet_manual_pages(NULL));
    assert(!kui_internet_extra_url(NULL, KUI_MEDIA_PREVIEW, url, sizeof(url)));
    puts("Internet manual and preview release selection passed");
    return 0;
}
