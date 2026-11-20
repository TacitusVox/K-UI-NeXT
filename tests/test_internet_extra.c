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
    game = (struct kui_games_disc_identity){.product="MK-51019", .title="SEGA RALLY 2", .region="U"};
    assert(kui_internet_manual_pages(&game) == 14);
    assert(kui_internet_manual_page_url(&game, 13, url, sizeof(url)));
    assert(!kui_internet_manual_page_url(&game, 14, url, sizeof(url)));
    assert(strstr(url, "/Sega_Rally_2_Championship_1999_U/page/n13_w512.jpg"));
    assert(kui_internet_extra_url(&game, KUI_MEDIA_MANUAL, url, sizeof(url)));
    assert(strstr(url, "sega_rally_2.pdf"));
    game = (struct kui_games_disc_identity){.product="T41902N", .region="U", .title="RAILROAD TYCOON II DREAMCAST EDITION"};
    assert(kui_internet_manual_pages(&game) == 16);
    assert(kui_internet_manual_page_url(&game, 15, url, sizeof(url)));
    assert(strstr(url, "/Railroad_Tycoon_II_Gold_Edition_2000_U/page/n15_w512.jpg"));
    assert(!kui_internet_manual_page_url(&game, 16, url, sizeof(url)));
    game = (struct kui_games_disc_identity){.product="MK-51059", .region="U", .title="SHENMUE",
                                            .version="V1.004", .number=4, .total=4};
    assert(kui_internet_manual_pages(&game) == 10);
    assert(kui_internet_manual_page_url(&game, 9, url, sizeof(url)));
    assert(strstr(url, "/Shenmue_Passport_2000_U/page/n9_w512.jpg"));
    assert(!kui_internet_manual_page_url(&game, 10, url, sizeof(url)));
    game.number = 1;
    assert(!kui_internet_manual_pages(&game));
    strcpy(game.version, "V1.003");
    assert(kui_internet_manual_page_url(&game, 0, url, sizeof(url)));
    assert(strstr(url, "/Shenmue_2000_U_color/"));
    /* Separate Japanese SKUs use the same base title. Only the matched
     * limited edition can fetch the collected 70-page colour manual. */
    game = (struct kui_games_disc_identity){.product="T-3601M", .title="DEAD OR ALIVE 2", .region="J"};
    assert(kui_internet_manual_pages(&game) == 70);
    assert(kui_internet_manual_page_url(&game, 69, url, sizeof(url)));
    assert(strstr(url, "Dead_Or_Alive_2_Limited_Edition"));
    strcpy(game.product,"T-3602M");
    assert(!kui_internet_manual_pages(&game));
    game = (struct kui_games_disc_identity){.product="T-14302M", .region="J"};
    assert(kui_internet_manual_pages(&game) == 28);
    assert(!kui_internet_manual_pages(NULL));
    assert(!kui_internet_extra_url(NULL, KUI_MEDIA_PREVIEW, url, sizeof(url)));
    game = (struct kui_games_disc_identity){.product="T41601N", .title="ELEMENTAL GIMMICK GEAR", .region="U"};
    assert(kui_internet_extra_url(&game, KUI_MEDIA_PREVIEW, url, sizeof(url)));
    assert(strstr(url, "/EGG%20-%20Elemental%20Gimmick%20Gear%20%28USA%29.ogv"));
    assert(kui_internet_manual_pages(&game) == 16);
    game = (struct kui_games_disc_identity){.product="T41902N", .title="RAILROAD TYCOON II DREAMCAST EDITION", .region="U"};
    assert(kui_internet_extra_url(&game, KUI_MEDIA_PREVIEW, url, sizeof(url)));
    assert(strstr(url, "/Railroad%20Tycoon%20II%20-%20Gold%20Edition%20%28USA%29.ogv"));
    game = (struct kui_games_disc_identity){.title="ELEMENTAL GIMMICK GEAR", .region="J"};
    assert(!kui_internet_extra_url(&game, KUI_MEDIA_PREVIEW, url, sizeof(url)));
    puts("Internet manual and preview release selection passed");
    return 0;
}
