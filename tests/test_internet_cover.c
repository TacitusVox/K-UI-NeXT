/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/games.h"
#include "kui/internet_cover.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static void check(const char *product, const char *region, const char *title, const char *box) {
    struct kui_games_disc_identity id = {0};
    snprintf(id.product, sizeof(id.product), "%s", product);
    snprintf(id.region, sizeof(id.region), "%s", region);
    snprintf(id.title, sizeof(id.title), "%s", title);
    const char *actual = kui_internet_cover_name(&id);
    if(box) assert(actual && !strcmp(actual, box));
    else assert(!actual);
}

int main(void) {
    check("MK-51035", "U", "CRAZY_TAXI", "Crazy Taxi (USA)");
    check("MK-51035", "E", "", "Crazy Taxi (Europe)");
    check("T-40301N", "JUE", "", "Armada (USA)");
    check("T3602M", "J", "DOA2", "Dead or Alive 2 (Japan)");
    check("T-3601M", "J", "DOA2", "Dead or Alive 2 (Japan) (Shokai Genteiban)");
    check("610-7913", "J", "Dead or Alive 2", "Dead or Alive 2 (Japan) (Taikenban)");
    check("MK-51052", "U", "", "Skies of Arcadia (USA)");
    check("51052-1", "U", "", "Skies of Arcadia (USA)");
    check("T-1204N", "U", "disc 2", "Resident Evil - Code - Veronica (USA)");
    check("MK-51184", "E", "", "Shenmue II (Europe) (En,Fr,De,Es)");
    /* PSO and its bundled Sonic Adventure 2 trial share a catalog serial. */
    check("MK-51100", "U", "PHANTASY STAR ONLINE", "Phantasy Star Online (USA)");
    check("MK-51100", "U", "", NULL);
    check("MK-51020", "U", "TOY COMMANDER", "Toy Commander (USA) (En,Fr,De,Es)");
    check("", "U", "THE HOUSE OF THE DEAD 2", "House of the Dead 2, The (USA)");
    check("", "U", "house of the dead 2, the", "House of the Dead 2, The (USA)");
    check("", "U", "ELEMENTAL_GIMMICK_GEAR", "Elemental Gimmick Gear (USA)");
    check("", "J", "Crazy Taxi", "Crazy Taxi (Japan)");
    check("", "", "Crazy Taxi", NULL);
    check("", "JUE", "Crazy Taxi", NULL);
    check("", "E", "Record of Lodoss War", NULL);
    check("", "J", "Crazy Taxi 2 Demo", NULL);
    check("T-40301N", "J", "", NULL);
    check("", "", "", NULL);
    check("", "U", "No such Dreamcast title", NULL);
    struct kui_games_disc_identity id = {.product="T-36805N",.region="U"};
    char url[768];
    assert(kui_internet_cover_url(&id, url, sizeof(url)));
    assert(strstr(url, "https://raw.githubusercontent.com/") == url);
    assert(strstr(url, "/Named_Boxarts/Sword%20of%20the%20Berserk%20-%20Guts%27%20Rage%20%28USA%29.png"));
    size_t length = strlen(url);
    assert(kui_internet_cover_url(&id, url, length+1));
    assert(!kui_internet_cover_url(&id, url, length) && !url[0]);
    assert(!kui_internet_cover_url(&id, url, 1) && !url[0]);
    assert(!kui_internet_cover_url(NULL, url, sizeof(url)) && !url[0]);
    assert(!kui_internet_cover_url(&id, NULL, 0));
    memset(id.product, 'X', sizeof(id.product));
    assert(!kui_internet_cover_name(&id));
    puts("Direct Libretro product, region, title and cover URL checks passed.");
}
