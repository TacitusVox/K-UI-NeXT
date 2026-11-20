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
    check("MK-51100", "U", "PSO", "Phantasy Star Online (USA)");
    check("51100", "U", "PSO", "Phantasy Star Online (USA)");
    check("MK-51100", "U", "Sonic Adventure 2 - The Trial", "Sonic Adventure 2 - The Trial (USA, Japan) (En,Ja)");
    check("MK-51100", "U", "PHANTASY STAR ONLINE", "Phantasy Star Online (USA)");
    check("MK-51100", "U", "", NULL);
    check("MK-51100", "U", "PSO VER. 2", NULL);
    check("MK-51193", "U", "PSO VER. 2", "Phantasy Star Online Ver. 2 (USA)");
    check("MK-5110050", "E", "PSO", "Phantasy Star Online (Europe) (En,Ja,Fr,De,Es) (Rev 1)");
    check("MK-51100", "E", "PSO", "Phantasy Star Online (Europe) (En,Ja,Fr,De,Es) (Rev 1)");
    check("MK-51100", "J", "PSO", NULL);
    check("MK-51100", "JUE", "PSO", NULL);
    check("51100", "E", "PSO", NULL);
    check("51100", "JUE", "PSO", NULL);
    check("51100", "", "PSO", NULL);
    check("", "U", "PSO", NULL);
    check("UNKNOWN", "U", "PSO", NULL);
    /* Real US retail IP fields from the scan. Printed product numbers differ. */
    check("T1711N", "U", "EVOLUTION2", "Evolution 2 - Far Off Promise (USA)");
    check("T11010N", "U", "SIERRA SPORTS MAXIMUM POOL", "Maximum Pool (USA)");
    check("T12509N", "U", "SUPER RUNABOUT", "Super Runabout - San Francisco Edition (USA)");
    check("T12509N", "J", "SUPER RUNABOUT", NULL);
    check("T11010N", "JUE", "SIERRA SPORTS MAXIMUM POOL", NULL);
    check("T1711N", "U", "EVOLUTION", NULL);
    struct kui_games_disc_identity shenmue = {
        .title="SHENMUE", .product="MK-51059", .version="V1.003", .region="U", .total=4
    };
    for(shenmue.number=1; shenmue.number<=3; ++shenmue.number)
        assert(!strcmp(kui_internet_cover_name(&shenmue), "Shenmue (USA)"));
    assert(!kui_internet_cover_name(&shenmue)); /* Disc four with the game version. */
    strcpy(shenmue.version,"V1.004");
    assert(!strcmp(kui_internet_cover_name(&shenmue), "Shenmue (USA) (Shenmue Passport)"));
    shenmue.number=1;
    assert(!kui_internet_cover_name(&shenmue)); /* Game disc with the Passport version. */
    shenmue.number=shenmue.total=0;
    assert(!kui_internet_cover_name(&shenmue)); /* A filename or an unnumbered rip is insufficient. */
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
    id = (struct kui_games_disc_identity){.product="MK-51035", .region="U"};
    assert(kui_internet_cover_small_url(&id, url, sizeof(url)));
    assert(!strcmp(url, "https://archive.org/download/CrazyTaxi-CompleteScans/"
                        "Crazy%20Taxi%20-%20Cover%20Front_thumb.jpg"));
    length = strlen(url);
    assert(kui_internet_cover_small_url(&id, url, length+1));
    assert(!kui_internet_cover_small_url(&id, url, length) && !url[0]);
    assert(kui_internet_cover_small_matches(&id, 23900, 0));
    assert(!kui_internet_cover_small_matches(&id, 0, 0));
    assert(!kui_internet_cover_small_matches(&id, 131073, 0));
    snprintf(id.region, sizeof(id.region), "E");
    assert(!kui_internet_cover_small_url(&id, url, sizeof(url)) && !url[0]);
    assert(kui_internet_cover_url(&id, url, sizeof(url)) && strstr(url, "%28Europe%29"));
    id = (struct kui_games_disc_identity){.title="THE HOUSE OF THE DEAD 2", .region="U"};
    assert(kui_internet_cover_small_url(&id, url, sizeof(url)) && strstr(url, "/__ia_thumb.jpg"));
    uint32_t bytes,crc;
    assert(kui_internet_cover_small_pin(&id,&bytes,&crc) && bytes==21946 && crc==UINT32_C(0x98908bea));
    assert(kui_internet_cover_small_matches(&id, 21946, UINT32_C(0x98908bea)));
    assert(!kui_internet_cover_small_matches(&id, 21946, UINT32_C(0x98908beb)));
    assert(!kui_internet_cover_small_matches(&id, 21945, UINT32_C(0x98908bea)));
    assert(!kui_internet_cover_small_url(NULL, url, sizeof(url)) && !url[0]);
    assert(!kui_internet_cover_small_url(&id, NULL, 0));
    memset(id.product, 'X', sizeof(id.product));
    assert(!kui_internet_cover_name(&id));
    puts("Direct covers: regional matching, smaller source selection, fallback and tile identity passed.");
}
