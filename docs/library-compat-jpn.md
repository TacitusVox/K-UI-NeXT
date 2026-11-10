# JPN library compatibility table

Every JPN Dreamcast startup from the owner's JPN run (bundler v2.3, run
`eee9cdc7c257`, 16 parts named `US-startup-NN` because the run kept the default
name; every disc is region `J`). Each row went through the launcher's own
admission checks with `tools/library_compat_table.py`, against the compact
Background stage end `0x8cebe748`. Column meanings are in
[the library compatibility table](library-compat-table.md); the US results are
in [US compatibility](library-compat-us.md). Static results are necessary
evidence, not console compatibility.

| Result | Games |
| --- | ---: |
| Background reader with virtual VMU admitted | 87 |
| Background reader only (virtual VMU blocked) | 309 |
| Neither (Standard reader only) | 59 |
| Windows CE (see [driver families](wince-vmu-driver-families.md)) | 48 |

The run listed 615 discs: 503 startups copied, 109 duplicate discs and 3
already sent in earlier runs. Duplicates and repeat listings are not rows.

## What blocks virtual VMU, by games affected

Each game counts once, under the first check that refuses it.

| First refusal | Games | Meaning |
| --- | ---: | --- |
| `driver` | 157 | Maple driver family not resolved |
| `exits` | 75 | Terminal exits not resolved |
| `layout` | 62 | Maple layout not resolved |
| `extent` | 20 | Executable extent not proved |
| `placement_384k` | 16 | 384 KiB lease does not fit |
| `initializer_call` | 14 | Heap initializer call not proved |
| `heap` | 9 | SDK heap family not resolved |
| `initializer_slot` | 9 | No heap initializer slot (fixed RAM; needs a RAM map) |
| `guarded_exits` | 6 | Terminal exits overlap hook windows |

Unlike the US library, where Maple layout, Maple driver and SDK heap lead,
the Maple driver family is the first blocker for most JPN games. Exits and
Maple layout follow.

## Table

| Game | Product | Boot KiB | Background X/Y + virtual VMU | Without VMU | Blocker | CD music | Console (recorded) |
| --- | --- | ---: | --- | --- | --- | --- | --- |
| AKIHABARA DENNOUGUMI PATAPIES | HDR-0019 | 3,228 | virtual VMU | — |  | — |  |
| BAKURETSUMUTEKI BANGAIOH | T38702M | 546 | virtual VMU | — |  | no profile: CD music Off |  |
| BATTLE BEASTER | T43801M | 2,004 | virtual VMU | — |  | — |  |
| BIKKURIMAN 2000 VIVA FESTIVA | T41801M | 1,210 | virtual VMU | — |  | — |  |
| BIOHAZARD CODE VERONICA GENTEIBAN | T1210M | 3,881 | virtual VMU | — |  | — |  |
| BLACK MATRIX AD | T20101M | 925 | virtual VMU | — |  | — |  |
| BOMBER HEHHE | T42903M | 748 | virtual VMU | — |  | — |  |
| CHOKOSENKI KIKAIOH | T1206M | 2,592 | virtual VMU | — |  | — |  |
| CHUCHUROCKET | HDR-0039 | 3,127 | virtual VMU | — |  | no profile: CD music Off |  |
| D2 | T30006M | 2,808 | virtual VMU | — |  | — |  |
| DANCINGBLADE KATTENI MOMO TENSI | T9504M | 412 | virtual VMU | — |  | — |  |
| DE LA JET SET RADIO | HDR-0128 | 2,977 | virtual VMU | — |  | — |  |
| DINO CRISIS | T1220M | 4,526 | virtual VMU | — |  | — |  |
| ETERNAL ARCADIA | HDR-0076 | 5,338 | virtual VMU | — |  | — |  |
| EVOLUTION2 | T38703M | 3,969 | virtual VMU | — |  | — |  |
| F1 WORLD GRAND PRIX II FOR DREAMCAST | T3002M | 1,180 | virtual VMU | — |  | no profile: CD music Off |  |
| GAMBARE NIPPON OLYMPIC 2000 | T9511M | 2,046 | virtual VMU | — |  | — |  |
| GODZILLA GENERATIONS MAXIMUM IMPACT | HDR-0047 | 1,466 | virtual VMU | — |  | — |  |
| GRANDIA2 | T4503M | 1,249 | virtual VMU | — |  | — |  |
| GUNDAM SIDE STORY 0079  +VS RX-78-2 GUNDAM | T13303M | 154 | virtual VMU | — |  | — |  |
| GUNDAM SIDE STORY 0079  PREMIUM DISC | 6107160 | 153 | virtual VMU | — |  | — |  |
| GUNDAM SIDE STORY 0079  RISE FROM THE ASHES | T13302M | 153 | virtual VMU | — |  | — |  |
| ILLBLEED | T45601M | 1,358 | virtual VMU | — |  | — |  |
| JAH-MONG | T-43401M | 778 | virtual VMU | — |  | no profile: CD music Off |  |
| JET SET RADIO | HDR-0078 | 2,857 | virtual VMU | — |  | — |  |
| JETCOASTER DREAM | T40902M | 1,010 | virtual VMU | — |  | no profile: CD music Off |  |
| JETCOASTER DREAM2 | T41201M | 2,111 | virtual VMU | — |  | no profile: CD music Off |  |
| JIKKYO PAWAFURU PUROYAKYU DREAMCAST EDITION | T9507M | 3,452 | virtual VMU | — |  | — |  |
| JINSEIGAME FOR DREAMCAST | T10301M | 1,764 | virtual VMU | — |  | — |  |
| KIKAIOH FOR MATCHING | T1232M | 3,708 | virtual VMU | — |  | — |  |
| KITAHE PHOTO MEMORIES | T14306M | 354 | virtual VMU | — |  | no profile: CD music Off |  |
| KITERETSU BOYS | HDR-0064 | 1,083 | virtual VMU | — |  | — |  |
| KUONNOKIZUNA SAIRINSYO | T36701M | 796 | virtual VMU | — |  | — |  |
| KUONNOKIZUNA SAIRINSYO | T36701M | 796 | virtual VMU | — |  | — |  |
| LOVEHINA | HDR-0102 | 1,145 | virtual VMU | — |  | — |  |
| MAKEN X | T14401M | 1,830 | virtual VMU | — |  | no profile: CD music Off |  |
| MEMORIES OFF COMPLETE | T19702M | 800 | virtual VMU | — |  | no profile: CD music Off |  |
| MERCURIUS PRETTY END OF THE CENTURY | T20103M | 644 | virtual VMU | — |  | — |  |
| MR.DRILLER | T1402M | 914 | virtual VMU | — |  | no profile: CD music Off |  |
| NADESICO THE MISSION | T27901M | 1,616 | virtual VMU | — |  | — |  |
| NASUBI NO HEYA | T14305M | 1,359 | virtual VMU | — |  | no profile: CD music Off |  |
| NEPPACHI | T41002M | 2,074 | virtual VMU | — |  | — |  |
| NEPPACHI | T41002M | 2,074 | virtual VMU | — |  | — |  |
| NEPPACHI2 VPACHI | T41003M | 1,493 | virtual VMU | — |  | — |  |
| NEPPACHI3 VPACHI | T41004M | 1,602 | virtual VMU | — |  | — |  |
| NEPPACHI4 | T41005M | 1,540 | virtual VMU | — |  | — |  |
| NEPPACHI5 | T41006M | 1,387 | virtual VMU | — |  | — |  |
| NEPPACHI6 | T41007M | 1,662 | virtual VMU | — |  | — |  |
| NET GOLF | HDR-0006 | 2,359 | virtual VMU | — |  | — |  |
| NET VERSUS CHESS | T45706M | 1,474 | virtual VMU | — |  | — |  |
| NET VERSUS GOMOKUNARABE RENJU | T45705M | 1,474 | virtual VMU | — |  | — |  |
| NET VERSUS HANAFUDA | T45704M | 1,474 | virtual VMU | — |  | — |  |
| NET VERSUS IGO | T45703M | 1,474 | virtual VMU | — |  | — |  |
| NET VERSUS MAJAN | T45701M | 1,474 | virtual VMU | — |  | — |  |
| NET VERSUS REVERSI | T45707M | 1,474 | virtual VMU | — |  | — |  |
| NET VERSUS SHOUGI | T45702M | 1,474 | virtual VMU | — |  | — |  |
| OUKAHOUSIN | T38704M | 2,282 | virtual VMU | — |  | — |  |
| PANZER FRONT | T2102M | 1,363 | virtual VMU | — |  | no profile: CD music Off |  |
| POCKEKANO | T19501M | 753 | virtual VMU | — |  | — |  |
| PRISMATICALLIZATION | T22002M | 409 | virtual VMU | — |  | no profile: CD music Off |  |
| PROYAKYU TEAM DE ASOBOU NET | HDR-0091 | 753 | virtual VMU | — |  | no profile: CD music Off |  |
| PUROYAKYUU TIMU WO TUKUROU AND ASOBOU | HDR-0173 | 251 | virtual VMU | — |  | no profile: CD music Off |  |
| PUYOPUYODA FEATURING ELLENA SYSTEM | T6601M | 1,540 | virtual VMU | — |  | — |  |
| RAYBLADE | T42201M | 1,108 | virtual VMU | — |  | no profile: CD music Off |  |
| RAYMAN THE GREAT ESCAPE | T17703M | 911 | virtual VMU | — |  | — |  |
| RECODE OF LODOSS WAR | T27903M | 3,401 | virtual VMU | — |  | no profile: CD music Off |  |
| REVIVE | T1301M | 1,372 | virtual VMU | — |  | — |  |
| ROOMMANIA203 | HDR-0044 | 2,124 | virtual VMU | — |  | — |  |
| RUNE CASTER | T40001M | 952 | virtual VMU | — |  | — (only short layout tracks) |  |
| SEAMAN | T39401M | 2,974 | virtual VMU | — |  | — |  |
| SEAMAN | T39401M | 2,980 | virtual VMU | — |  | — |  |
| SEGAGT HOMOLOGATION SPECIAL | HDR-0054 | 1,738 | virtual VMU | — |  | no profile: CD music Off |  |
| SENGOKUTURB FID | T38805M | 381 | virtual VMU | — |  | no profile: CD music Off |  |
| SENTIMENTAL GRAFFITI 2 | T20104M | 845 | virtual VMU | — |  | — |  |
| SONIC ADVENTURE 2 | HDR-0165 | 1,541 | virtual VMU | — |  | — |  |
| SUNRISE EIYUUTAN | T41101M | 1,390 | virtual VMU | — |  | — |  |
| SUPER PUZZLE FIGHTER2X FOR MATCHING SERVICE | T1250M | 4,398 | virtual VMU | — |  | — |  |
| SUPER STREET FIGHTER 2X FOR MATCHING SERVICE | T1236M | 5,062 | virtual VMU | — |  | — |  |
| TANTEISHINSHIDASH | T44401M | 650 | virtual VMU | — |  | — (only short layout tracks) |  |
| TETSUMAN MENKYOKAIDEN | T18701M | 382 | virtual VMU | — |  | no profile: CD music Off |  |
| THE KING OF FIGHTERS 99 EVOLUTION | T3102M | 3,954 | virtual VMU | — |  | — |  |
| THE KING OF FIGHTERS DREAM MATCH 1999 | T3101M | 2,312 | virtual VMU | — |  | no profile: CD music Off |  |
| THE RING | T27902M | 1,586 | virtual VMU | — |  | — |  |
| TOY RANGER | HDR-0052 | 754 | virtual VMU | — |  | no profile: CD music Off |  |
| WINNING POST 4 PROGRAM2000 FOR DREAMCAST | T7606M | 1,946 | virtual VMU | — |  | no profile: CD music Off |  |
| WWF ROYAL RUMBLE | T45101M | 1,328 | virtual VMU | — |  | — |  |
| ZUSAR VASAR | T43301M | 1,677 | virtual VMU | — |  | no profile: CD music Off |  |
| 21-TWOONE- | T46503M | 869 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| ADVANCED DAISENRYAKU 2001 | HDR-0158 | 3,541 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| AERO DANCING I | T6807M | 7,023 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| AERO DANCING I/JIKAI-SAKU MADE MATEMASEN | T6809M | 7,084 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| AERODANCING F | T6804M | 2,959 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| AERODANCING F | T6804M | 2,960 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| AERODANCING F/TODOROKI TSUBASA NO HATSU-HIKO | T6805M | 7,035 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| AFTER | T18706M | 860 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| AIR | T20112M | 727 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| ANGEL WISH | T18712M | 883 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| ANGELPRESENT | T20107M | 1,049 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| ANIMASTAR | T34101M | 1,251 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| ATSUMARE GURU GURU ONSEN BB | HDR-0121 | 4,817 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| ATSUMARE GURU GURU ONSEN BB | HDR-0121 | 4,818 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| BALDR FORCE EXE | T47110M | 1,394 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| BIOHAZARD CODE VERONICA PLUS | T1240M | 3,934 | refused | New, no virtual VMU | executable extent not proved | — |  |
| BIOHAZARD3 | T1222M | 2,035 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| BITTERSWEET FOOLS | T47401M | 655 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| BLUE6 -TIME AND TIDE- | HDR-0075 | 1,561 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| BOKOMU NO TATSUJIN | T42904M | 1,369 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| BOKU DORAEMON | T41802M | 955 | refused | New, no virtual VMU | terminal exits not resolved | — (only short layout tracks) |  |
| BOKU TO BOKURA NO NATU | T19717M | 1,281 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| BORDER DOWN | T46703M | 1,556 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| BOUNTY HUNTER SARA SCENARIO1 | T1243M | 618 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| CAFE LITTLEWISH | T46513M | 497 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| CANARY | T20111M | 712 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| CANDY STRIPE | HDR-0172 | 3,246 | refused | New, no virtual VMU | executable extent not proved | — |  |
| CANVAS | T20108M | 797 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| CAPCOM VS SNK | T1217M | 2,332 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| CAPCOM VS SNK | T1217M | 2,334 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| CAPCOM VS SNK PRO | T1247M | 2,304 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| CAPCOM VS SNK2 | T1249M | 2,331 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| CARDCAPTOR SAKURA TOMOYO NO VIDEO DAISAKUSEN | HDR-0115 | 1,003 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| CARDOFDESTINY | T44406M | 817 | refused | New, no virtual VMU | Maple driver family not resolved | — (only short layout tracks) |  |
| CARRIER | T5701M | 11,775 | refused | New, no virtual VMU | executable extent not proved | — |  |
| CHAOS FIELD | T47801M | 1,125 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| CHARGEN BLAST | T4402M | 2,472 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| CHOCOLAT - MAID CAFE CURIO - | T47107M | 2,218 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| CHOUHATSUMEIBOY KANIPAN BOUSOUROBOTO NO NAZO | HDR-0021 | 1,386 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| CLEOPATRA FORTUNE | T16603M | 545 | refused | New, no virtual VMU | Maple driver family not resolved | no profile: CD music Off |  |
| CLIMAX LANDERS | HDR-0009 | 643 | refused | New, no virtual VMU | terminal exits overlap hook windows | — |  |
| CLOSE TO | T19704M | 1,045 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| COJI COJI | T40802M | 1,272 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| COMIC PARTY | T38301M | 1,129 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| COMIC PARTY | T38301M | 1,129 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| COMIC PARTY | T38301M | 1,155 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| CONFIDENTIAL MISSION | HDR-0160 | 1,745 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| COOL BOARDERS BURRRN | T36901M | 1,328 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| COOL COOL TOON | T3106M | 2,801 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| COSMIC SMASH | HDR-0176 | 912 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| CRAZY TAXI | HDR-0053 | 1,433 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | — |  |
| CRAZY TAXI 2 | HDR-0159 | 1,418 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | — |  |
| CULDCEPT SECOND | T43903M | 3,188 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| CYBER TROOPERS VIRTUAL-ON ORATORIO TANGRAM | HDR-0040 | 2,291 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| DABITSUKU | HDR-0084 | 3,943 | refused | New, no virtual VMU | executable extent not proved | — |  |
| DABITSUKU2 | HDR-0167 | 4,605 | refused | New, no virtual VMU | executable extent not proved | — |  |
| DANCE DANCE REVOLUTION 2NDMIX DREAMCAST EDITION | T9506M | 1,373 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| DAYTONA USA 2001 | HDR-0106 | 1,955 | refused | New, no virtual VMU | executable extent not proved | — |  |
| DDR CLUB VER 1000 | T9508M | 1,421 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| DEAD OR ALIVE 2 | T3601M | 1,884 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | — |  |
| DEAD OR ALIVE 2 | T3602M | 1,883 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | — |  |
| DEATH CRIMSON OX | T23202M | 9,141 | refused | New, no virtual VMU | executable extent not proved | — |  |
| DEATHCRIMSON2 | T23201M | 1,310 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| DESPIRIA | T14402M | 1,449 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| DIGI CHARAT FANTASY | T46302M | 674 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| DOGU SENKI -HAOH- | T9101M | 1,457 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| DOKIDOKI IDOL STARSEEKER REMIX | T46701M | 786 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| DOUSOUKAI2 | T20113M | 551 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| DVINE LUV | T46501M | 1,031 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| ECCO THE DOLPHIN DEFENDER OF THE FUTURE | HDR-0103 | 2,925 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | no profile: CD music Off |  |
| ELDORADO GATE VOL1 | T1223M | 1,230 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| ELDORADO GATE VOL2 | T1224M | 1,275 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| ELDORADO GATE VOL3 | T1225M | 1,337 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| ELDORADO GATE VOL4 | T1226M | 1,411 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| ELDORADO GATE VOL5 | T1227M | 1,880 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| ELDORADO GATE VOL6 | T1228M | 2,005 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| ELDORADO GATE VOL7 | T1229M | 1,662 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| ELYSION | T20116M | 855 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| ERDE | T19720M | 1,051 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| ES | T45501M | 1,053 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| ESPION-AGE-NTS | T38804M | 1,285 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| EVANGELION AYANAMI IKUSEI KEIKAKU | T46303M | 2,243 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| EVE ZERO | T39803M | 1,641 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| EVER17 -THE OUT OF INFINITY- | T19716M | 1,329 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| EVER17PE | T19723M | 1,327 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| EXODUSGUILTYNEOS | T44404M | 834 | refused | New, no virtual VMU | Maple driver family not resolved | — (only short layout tracks) |  |
| F1 WORLD GRAND PRIX FOR DREAMCAST | T3001M | 1,008 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| F1 WORLD GRAND PRIX FOR DREAMCAST | T3001M | 1,014 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| FIGHTER OF ZERO | T43501M | 946 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| FIGHTING VIPERS2 | HDR-0133 | 1,333 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| FIRE PROWRESTLING D | T18805M | 1,614 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| FIRST KISS STORY | T46304M | 638 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| FIRST KISS STORY 2 | T46305M | 1,362 | refused | New, no virtual VMU | executable extent not proved | — |  |
| FISHEYESWILD | T9105M | 1,774 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| FRAME GRIDE | T34201M | 1,727 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| FUREAI | T47402M | 660 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| GAIA MASTER | T1212M | 1,680 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| GAROU MARK OF THE WOLVES | T3108M | 1,491 | refused | New, no virtual VMU | Maple driver family not resolved | no profile: CD music Off |  |
| GETBASS2 | HDR-0169 | 2,256 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| GIANTGRAM2000 | HDR-0101 | 4,635 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| GIGA WING2 | T1239M | 1,402 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| GIGAWING | T1208M | 1,501 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| GOLF SHIYOUYO | T40901M | 1,291 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| GOLF SHIYOUYO - COURSES DATA - ADVENTURE EDITION | T40904M | 1,314 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| GOLF SHIYOUYO 2 | T44501M | 2,265 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| GOLF_SHIYOUYO | T40901M | 1,290 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| GUILTY GEAR X | T-2402M | 1,445 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| GUNBIRD2 | T1213M | 1,090 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| GUNDAM BATTLE ONLINE | T13304M | 1,009 | refused | New, no virtual VMU | terminal exits not resolved | — (only short layout tracks) |  |
| GUNSPIKE | T1219M | 625 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| GURU GURU ONSEN 3 | HDR-0195 | 2,755 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| GURUGURU ONSEN 2 | HDR-0168 | 2,695 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| HANAGUMI TAISEN COLUMNS 2 | HDR-0046 | 2,245 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| HANIHANI | T47109M | 728 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| HAPPY BREEDING | T46512M | 499 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| HARUSAME YOUBI | T20106M | 747 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| HEAVY METAL GEOMATRIX | T1246M | 1,200 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | — |  |
| HEISEI MAJANSOU | T2202M | 1,764 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| HIMITSU YUIGAITANATSU | T46101M | 568 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| IDOL JANSHI WO TSUKUTTYAOU | T5703M | 971 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| IKARUGA | T38706M | 1,003 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| ILLBLEED | T45601M 11 | 1,358 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| INTERLUDE | T20117M | 619 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| IRIS | T19722M | 1,115 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| IZUMO | T46902M | 1,634 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| J-LEAGUE SPECTACLE SOCCER | HDR-0154 | 735 | refused | New, no virtual VMU | executable extent not proved | — |  |
| JISSEN PACHISLOT HISSYOUHOU VPACHI KONGDOM | T18802M | 736 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| JLEAGUE PRO SOCCER CLUB MAKER | HDR-0028 | 1,401 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| JLEAGUE PRO SOCCER CLUB MAKER | HDR-0028 | 1,401 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| JOJO | T1231M | 443 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| JOJO | T1205M | 234 | refused | New, no virtual VMU | terminal exits overlap hook windows | — |  |
| KAENSEIBO | T42802M | 910 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| KANON | T20105M | 818 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| KAROUS | T47803M | 951 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| KAZE NO UTA | T47501M | 1,596 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| KIDOUSENSHI GUNDAM GIHREN NO YABO ZEON NO KEIFU | T13305M | 1,714 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| KIDOUSENSHI GUNDAM GIHREN NO YABO ZEON NO KEIFU | T13305M | 1,714 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| KIMIGA NOZOMU EIEN | T47101M | 1,051 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| KONOHANA TRUEREPORT | T9905M | 541 | refused | New, no virtual VMU | Maple driver family not resolved | no profile: CD music Off |  |
| KONOHANA2 | T9906M | 572 | refused | New, no virtual VMU | Maple driver family not resolved | no profile: CD music Off |  |
| L.O.L. | T2106M | 5,373 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| LAKEMASTERSPRO DREAMCAST PLUS | T42401M | 1,087 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| LANGRISSER MILLENNIUM | T2501M | 3,554 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| LOVEHINA2 | HDR-0139 | 1,136 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| MACROSS M3 | T21501M | 2,087 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| MAGIC THE GATHERING | HDR-0116 | 2,501 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| MAJO NO OCHAKAI | T20124M | 940 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| MAPLE | T46510M | 537 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| MARGE | T46514M | 500 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| MARIONETTE HANDLER | T2201M | 1,166 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| MARIONETTE HANDLER2 | T2204M | 1,611 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| MARS MATRIX | T1238M | 2,338 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| MARVEL VS. CAPCOM 2 | T1215M | 1,923 | refused | New, no virtual VMU | terminal exits overlap hook windows | — |  |
| MEMORIES OFF 2ND | T19707M | 1,186 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| METALWOLF | T46507M | 1,014 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| MILKY SEASON | T19713M | 1,232 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| MISSING PARTS 2 THE TANTEI STORIES | T36703M | 627 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| MISSING PARTS 3 THE TANTEI STORIES | T36704M | 631 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| MISSING PARTS THE TANTEI STORIES | T36702M | 615 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| MIZUIRO | T20114M | 656 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| MOBILE SUIT GUNDAM THE EARTH FEDERATION VS. THE PRINCIPALITY OF ZEON AND DX | T13306M | 2,432 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| MOBILE SUIT GUNDAM THE EARTH FEDERATION VS. THE PRINCIPALITY OF ZEON AND DX | T13306M | 3,053 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| MOEKAN | T46515M | 541 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| MOERO JUSTICE GAKUEN | T1221M | 3,853 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| MOMOTENSHI2 DISC1 | T9505M | 427 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| MOTTO PROYAKYU TEAM WO TUKUROU | HDR-0092 | 736 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| MUSAPEY NO CHOCOMARKER | T23203M | 9,529 | refused | New, no virtual VMU | executable extent not proved | — |  |
| MY MERRY MAY | T19714M | 1,334 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| MY MERRY MAYBE | T19719M | 1,040 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| NAKORURU | T44702M | 808 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| NANATSU NO HIKAN SENRITSU NO BISYOU | T7604M | 1,463 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| NAPPLETALE ARSIA IN DAYDREAM | HDR-0079 | 1,763 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | — |  |
| NATSUIROCELEBRATION | T47404M | 666 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| NBA 2K1 | HDR-0150 | 3,365 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| NEO GOLDEN LOGRES | T9904M | 493 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| NET DE TENNIS | T1234M | 1,677 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| NET GIMMICK | T1248M | 1,649 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| NEVER7 THE END OF INFINITY | T19703M | 1,035 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| NFL 2K | HDR-0058 | 3,231 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| NFL 2K1 | HDR-0144 | 3,365 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| NHL2K2 | MK-51182 | 2,631 | refused | New, no virtual VMU | heap initializer call not proved | — (only short layout tracks) |  |
| NIJYUEI | T46505M | 835 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| NOBUNAGA NO YABOU REPPUUDEN | T7605M | 2,748 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| OHGAMI ICHIRO FUNTOUKI | HDR-0057 | 1,110 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | — |  |
| OKAERI | T47403M | 696 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| OMOIDE NI KAWARU KIMI | T19718M | 1,362 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| ORANGEPOCKET | T18708M | 803 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| PACHI-SLOT TEIO DREAM SLOT HEIWA SP | T20501M | 1,033 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| PACHI-SLOT TEIO DREAM SLOT OLYMPIA SP | T20502M | 1,032 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| PACHINKO NO DENDOU CR NANASY | T5205M | 722 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| PACHITTECHONMAGE | T45801M | 1,548 | refused | New, no virtual VMU | Maple driver family not resolved | — (only short layout tracks) |  |
| PANDORA NO YUME | T20120M | 1,174 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| PATISSIERNANYANKO | T18710M | 1,301 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| PERPETUAL BLUE | T27801M | 926 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| PIZZICATO POLKA | T19724M | 1,755 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| POPN MUSIC 2 | T9590M | 648 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| POPN MUSIC 3 APPEND DISC | T9591M | 154 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| POPN MUSIC 4 APPEND DISC | T9592M | 155 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| POWER JET RACING 2001 | T6806M | 2,454 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| POWER SMASH | HDR-0113 | 2,515 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| POWER SMASH 2 | HDR-0177 | 2,700 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| POWER STONE 2 | T1218M | 2,901 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| PRINCESS HOLIDAY | T47106M | 976 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| PRISM HEART | T19711M | 852 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| PRISM HEART | T19712M | 849 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| PRO MAHJONG KIWAME D | T16801M | 515 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| PSO | HDR-0129 | 3,882 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| PSO VER.2 | HDR-0163 | 153 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| PSYVARIAR2 THE WILL TO FABRICATE | T9907M | 2,031 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| PUYOPUYO FEVER | HDR-0216 | 1,302 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| QUIZ AH MY GODDESS | HDR-0105 | 1,109 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| RADIRGY | T47802M | 1,208 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| RAINBOW COTTON | T9901M | 564 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| RE-VOLT | T-8101M | 2,871 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| READY 2 RUMBLE BOXING | HDR-0051 | 2,034 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| RENAICHU | T47602M | 558 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| RENT A HERO NO.1 | HDR-0074 | 1,720 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| REZ | HDR-0178 | 1,087 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| ROOMMATE ASAMI - OKUSAMA HA JYOSIKOUSEI - | T19508M | 534 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| ROOMMATENOVEL - YUKA SATO - | T19502M | 570 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| RUNDIM AS BLACK SOUL | T46201M | 2,313 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| RYOKO INOUE - ROOMMATE - | T19505M | 520 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| RYOKO INOUE LAST SCENE | T19506M | 667 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SAKATUKUTOKUDAIGO | HDR-0126 | 2,880 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| SAKATUKUTOKUDAIGO2 | HDR-0183 | 2,880 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| SAKURA TAISEN ONLINE PARI NO YUUGANA HIBI | HDR-0185 | 2,800 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SAKURA TAISEN ONLINE TEITO NO NAGAI HIBI | HDR-0184 | 2,799 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SAKURA WARS | HDR-0072 | 1,624 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| SAKURA WARS | HDR-0072 | 1,624 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| SAKURA WARS 2 | HDR-0082 | 687 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| SAKURA WARS 2 | HDR-0082 | 686 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| SAKURA WARS 4 | HDR-0191 | 2,180 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SAKURA WARS3 | HDR-0152 | 2,215 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SAMBA DE AMIGO | HDR-0063 | 2,110 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| SAMBADEAMIGO V2 | HDR-0131 | 2,724 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| SANGOKUSHI6WITHPK | T7607M | 3,210 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| SEGA EXTREME SPORTS | HDR-0146 | 1,945 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| SEGA MARINE FISHING | HDR-0104 | 1,409 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| SEGA TETRIS | HDR-0073 | 4,254 | refused | New, no virtual VMU | executable extent not proved | — |  |
| SEGA TETRIS | HDR-0073 | 4,254 | refused | New, no virtual VMU | executable extent not proved | — |  |
| SEIMATAISEN | T46901M | 916 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| SENTIMENTAL GRAFFITI YAKUSOKU | T20128M | 913 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SGGG | HDR-0151 | 2,088 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SGGG | HDR-0151 | 2,080 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SHANGHAI DYNASTY | T9902M | 860 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SHANGRI-LA | T40801M | 2,145 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| SHIKIGAMI NO SHIRO 2 | T47702M | 1,632 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SHIN HONKAKU HANAFUDA | T16601M | 1,002 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| SHINRI GAME DC EDITION | T43403M | 1,279 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SHUTOKOU BATTLE | T30801M | 1,046 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| SHUTOKOU BATTLE 2 | T30803M | 1,382 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SILENT SCOPE | T9513M | 1,353 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SIROTSUMESOUWA | T20123M | 1,304 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SISTER PRINCESS PREMIUM EDITION | T27802M | 601 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SNOW | T20132M | 1,525 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SONIC ADVENTURE INTERNATIONAL | HDR-0043 | 6,584 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SONIC SHUFFLE | HDR-0125 | 1,004 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SORAWOMAUTSUBASA | T23205M | 11,528 | refused | New, no virtual VMU | executable extent not proved | — |  |
| SORCERIAN-DC | T9103M | 1,545 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| SOULCALIBUR | T1401M | 3,494 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SPACE CHANNEL 5 PART 2 | HDR-0190 | 1,268 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SPACECHANNEL5 | HDR-0029 | 2,432 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| SPAWN IN THE DEMONS HAND | T1216M | 1,191 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | — |  |
| SPEED DEVILS | T17702M | 3,481 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| STREET FIGHTER ZERO3 SAIKYO-RYU DOJO | T1203M | 7,648 | refused | New, no virtual VMU | executable extent not proved | — |  |
| STREET FIGHTER ZERO3 SAIKYO-RYU DOJO FOR MATCHING SERVICE | T1230M | 1,736 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| STREET FIGHTER3 W IMPACT | T1211M | 803 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SUIGETSU | T19725M | 1,256 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SUIKA | T46509M | 790 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SUPER EURO SOCCER 2000 | T15006M | 2,582 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| SUPERMAGNETICNIUNIU | T30802M | 3,244 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| TAKO NO MARINE | T5204M | 613 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| TAMACUE | T20133M | 962 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| TANAKATORAHIKO NO URUTORARYUUSYOUGI IBISYAANAGUMAHEN | T22001M | 887 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| TANTEISHINSHIDASH | T44401M | 650 | refused | New, no virtual VMU | terminal exits overlap hook windows | — (only short layout tracks) |  |
| TENOHIRA | T46516M | 536 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| TENTAMA -1ST SUNNY SIDE- | T19710M | 1,311 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| THE HOUSE OF THE DEAD 2 | HDR-0007 | 794 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| THE KING OF FIGHTERS 2000 | T47303M | 2,779 | refused | New, no virtual VMU | Maple driver family not resolved | — (only short layout tracks) |  |
| THE KING OF FIGHTERS 2001 | T47304M | 3,113 | refused | New, no virtual VMU | Maple driver family not resolved | — (only short layout tracks) |  |
| THE KING OF FIGHTERS 2002 | T47305M | 3,360 | refused | New, no virtual VMU | Maple driver family not resolved | — (only short layout tracks) |  |
| THE LOST GOLEM | T41501M | 1,896 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| THE TYPING OF THE DEAD | HDR-0067 | 1,072 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| TOKUSATSUBOUKENKATSUGEKI SUPERHERO RETSUDEN | T20601M | 865 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| TOKYO BUS GUIDE | T35402M | 947 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| TORAUMAN | T35405M | 1,017 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| TOUKONRETSUDEN4 | T28202M | 1,763 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| TREASURE STRIKE | T19701M | 2,126 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| TRICOLORE CRISE | T9104M | 1,491 | refused | New, no virtual VMU | terminal exits overlap hook windows | — |  |
| TRIGGERHEART EXELICA | T29102M | 1,973 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| TRIZEAL | T47901M | 1,047 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| TWINKLESTAR SPRITES | T3103M | 710 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| TYPING OF THE DATE | T14307M | 808 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| ULTIMATE FIGHTING CHAMPIONSHIP | T1241M | 1,015 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| UNDER DEFEAT | T46705M | 1,014 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| UTAU TANBURINGU DAISU | T23207M | 11,290 | refused | New, no virtual VMU | executable extent not proved | — |  |
| VAMPIRE CHRONICLE FOR MATCHINGSERVICE | T1235M | 5,558 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| VERMILION DESERT | T5302M | 1,264 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| VIGILANTE8 SECOND BATTLE | T36501M | 3,472 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| VIRTUAATHLETE2K | HDR-0081 | 1,678 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| WELCOME TO PIACARROT 3 | T20122M | 1,344 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| WELCOME TO PIACARROT2.5 | T20109M | 598 | refused | New, no virtual VMU | Maple driver family not resolved | no profile: CD music Off |  |
| WIND -A BREATH OF HEART- | T47104M | 1,225 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| WORLD NEVERLAND2 THE WAKTIC REPUBLIC OF PLUTO STORIES | T5303M | 2,195 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| WORLD SERIES BASEBALL 2K1 | HDR-0157 | 4,285 | refused | New, no virtual VMU | executable extent not proved | — |  |
| YAMAZAKI | HDR-0050 | 889 | refused | New, no virtual VMU | terminal exits overlap hook windows | no profile: CD music Off |  |
| YUME NO TSUBASA | T19706M | 1,149 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| ZERO GUNNER2 | T20401M | 705 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| ZOMBIE REVENGE | HDR-0026 | 10,059 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| AERO DANCING FEATURING BLUE IMPULSE | T6802M | 2,365 | refused | refused | Maple driver family not resolved | — |  |
| AERO DANCING TODOROKI TAICHOU NO HIMITSU DISK | T6803M | 2,425 | refused | refused | Maple driver family not resolved | — |  |
| AIRFORCE DELTA | T9502M | 1,602 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| ATELIER MARIE AND ELIE | T44701M | 22 | refused | refused | Maple driver family not resolved | — |  |
| ATELIER MARIE AND ELIE OMAKE | T44701M | 22 | refused | refused | Maple driver family not resolved | — |  |
| BLUE STINGER | HDR-0003 | 1,917 | refused | refused | Maple driver family not resolved | — |  |
| DYNAMITE DEKA 2 | HDR-0020 | 1,864 | refused | refused | Maple driver family not resolved | — |  |
| ELEMENTAL GIMMICK GEAR          BY HUDSON | T14302M | 654 | refused | refused | Maple driver family not resolved | — |  |
| ETERNAL ARCADIA KEY VERSION | HDR-0119 | 14,271 | refused | refused | executable extent not proved | — |  |
| EVOLUTION | T38701M | 3,395 | refused | refused | executable extent not proved | — |  |
| F355 CHALLENGE | HDR-0100 | 1,569 | refused | refused | Maple driver family not resolved | — |  |
| GETBASS | HDR-0023 | 1,116 | refused | refused | Maple driver family not resolved | — |  |
| GIANTGRAM ALL JAPAN PRO.WRESTLING2 IN NIPPON BUDOUKAN | HDR-0005 | 2,213 | refused | refused | Maple driver family not resolved | — |  |
| GODZILLA GENERATIONS | HDR-0004 | 1,322 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| INCOMING | T15001M | 722 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| INCOMING | T15001M | 722 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| July | T35401M | 1,262 | refused | refused | Maple driver family not resolved | — |  |
| July | T35401M | 1,260 | refused | refused | Maple driver family not resolved | — |  |
| MABOROSI TUKIYO | T4401M | 766 | refused | refused | Maple driver family not resolved | — |  |
| MAHJONG TAIKAI 2 SPECIAL | T7601M | 773 | refused | refused | Maple driver family not resolved | — |  |
| MARVEL VS.CAPCOM | T1202M | 4,809 | refused | refused | Maple driver family not resolved | — |  |
| MONACO GRAND PRIX RACING SIMULATION 2 | T17701M | 1,639 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| Mr.Yukawa's Search The Treasure | HDR-0024 | 645 | refused | refused | Maple driver family not resolved | — |  |
| MY TRACKMAN | T37001M | 4,329 | refused | refused | Maple driver family not resolved | — (only short layout tracks) |  |
| NOBUNAGA NO YABOU SHOUSEIROKU with PowerUpKit | T7602M | 1,830 | refused | refused | Maple driver family not resolved | — |  |
| OUTTRIGGER | HDR-0118 | 2,825 | refused | refused | Maple driver family not resolved | — |  |
| PENPEN TRIICELON | T17001M | 1,974 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| PENPEN TRIICELON | T17001M | 1,970 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| POPNMUSIC | T9501M | 559 | refused | refused | Maple driver family not resolved | — |  |
| POWERSTONE | T1201M | 3,357 | refused | refused | Maple driver family not resolved | — |  |
| PROYAKYUU TEAM DE ASOBOU | HDR-0027 | 711 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| PSYCHIC FORCE 2012 | T1101M | 4,595 | refused | refused | Maple driver family not resolved | — |  |
| PUROYAKYUU TEAM WO TSUKUROU | HDR-0022 | 933 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| PUROYAKYUU TEAM WO TSUKUROU | HDR-0022 | 933 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| PUYOPUYO4 | HDR-0014 | 1,769 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| REDLINE RACER | T15002M | 880 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| SANGOKUSHI 6 | T7603M | 5,295 | refused | refused | Maple driver family not resolved | — |  |
| SENGOKU TURB | T38801M | 4,714 | refused | refused | Maple driver family not resolved | — |  |
| SEVENTH CROSS | T38802M | 1,684 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| SHENMUE | HDR-0016 | 2,646 | refused | refused | Maple driver family not resolved | — (only short layout tracks) |  |
| SHENMUE | HDR-0016 | 4,936 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| SHENMUE | HDR-0016 | 4,936 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| SHENMUE2 | HDR-0164 | 3,105 | refused | refused | Maple driver family not resolved | — (only short layout tracks) |  |
| SONIC ADVENTURE | HDR-0001 | 6,624 | refused | refused | Maple driver family not resolved | — |  |
| SPORTS JAM | HDR-0153 | 13,161 | refused | refused | executable extent not proved | — |  |
| STAR GLADIATOR 2 | T1204M | 9,291 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| STREET FIGHTER 3 3RD STRIKE | T1209M | 13,962 | refused | refused | executable extent not proved | — |  |
| SUPER SPEED RACING | HDR-0013 | 1,190 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| SYDNEY2000 | T1237M | 1,622 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| TETRIS 4D | T20801M | 1,020 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| THE HOUSE OF THE DEAD 2 | HDR-0007 | 846 | refused | refused | Maple driver family not resolved | — |  |
| THE HOUSE OF THE DEAD 2 | HDR-0007 | 846 | refused | refused | Maple driver family not resolved | — |  |
| US SHENMUE | HDR-0156 | 2,653 | refused | refused | Maple driver family not resolved | — (only short layout tracks) |  |
| US SHENMUE | HDR-0156 | 4,936 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| VF3tb | HDR-0002 | 3,392 | refused | refused | Maple driver family not resolved | — |  |
| VF3tb | HDR-0002 | 1,152 | refused | refused | Maple driver family not resolved | — |  |
| VIRTUA STRIKER 2 VER.2000.1 | HDR-0045 | 2,112 | refused | refused | Maple driver family not resolved | — |  |
| WEB MYSTERY | T39501M | 808 | refused | refused | Maple driver family not resolved | — |  |
| YS GAME WORKS VOL.1 | 6108099 | 3,093 | refused | refused | Maple driver family not resolved | — (only short layout tracks) |  |
| APRICOT | T37910M | 1,984 | Windows CE reader | — |  | — |  |
| BIOHAZARD 2 VALUE PLUS | T1214M | 1,158 | Windows CE reader | — |  | no profile: CD music Off |  |
| BIOHAZARD 2 VALUE PLUS | T1214M | 1,158 | Windows CE reader | — |  | no profile: CD music Off |  |
| CHERRY BLOSSOM | T37912M | 1,984 | Windows CE reader | — |  | — |  |
| DAISESSEN | T35403M | 2,536 | Windows CE reader | — |  | — |  |
| DENSHA DE GO 2 3K FOR DREAMCAST | T1102M | 1,142 | Windows CE reader | — |  | — |  |
| EISEIMEIJIN3 GAME CREATOR YOSIMURA NOBUHIRO NO ZUNOU | T9503M | 1,378 | Windows CE reader | — |  | no profile: CD music Off |  |
| EVANGELION TYPE C | T35102M | 2,070 | Windows CE reader | — |  | — (only short layout tracks) |  |
| EVANGELION TYPE E | T35101M | 2,070 | Windows CE reader | — |  | — (only short layout tracks) |  |
| FOR SYMPHONY | T37908M | 1,984 | Windows CE reader | — |  | — |  |
| FRAGRANCE TAIL | T37905M | 1,984 | Windows CE reader | — |  | — |  |
| GET COLONY | HDR-0071 | 1,252 | Windows CE reader | — |  | no profile: CD music Off |  |
| HAPPY LESSON | T19503M | 2,772 | Windows CE reader | — |  | — |  |
| HELLO KITTY LOVELY FRUIT PARK | HDR-0055 | 1,232 | Windows CE reader | — |  | no profile: CD music Off |  |
| HELLO KITTY MAGICAL BLOCK | HDR-0070 | 2,192 | Windows CE reader | — |  | no profile: CD music Off |  |
| HELLO KITTY WAKUWAKU COOKIES | HDR-0090 | 1,258 | Windows CE reader | — |  | no profile: CD music Off |  |
| HUNDRED SWORDS | HDR-0124 | 2,220 | Windows CE reader | — |  | no profile: CD music Off |  |
| HUNDRED SWORDS KEY VERSION | HDR-0127 | 2,220 | Windows CE reader | — |  | no profile: CD music Off |  |
| KITAE WHITE ILLUMINATION | T14301M | 1,340 | Windows CE reader | — |  | — |  |
| KITAHEI GOLD | T39801M | 4,246 | Windows CE reader | — |  | no profile: CD music Off |  |
| KITTY GP | 6107381 | 1,070 | Windows CE reader | — |  | — |  |
| MARIONETTE COMPANY | T5202M | 1,382 | Windows CE reader | — |  | — |  |
| MARIONETTE COMPANY 2 | T5203M | 2,550 | Windows CE reader | — |  | no profile: CD music Off |  |
| MISS MOONLIGHT | T18702M | 2,908 | Windows CE reader | — |  | no profile: CD music Off |  |
| MORITA NO SAIKYO REVERSI | T39102M | 1,378 | Windows CE reader | — |  | — |  |
| MORITA NO SAIKYO SHOUGI | T39101M | 1,378 | Windows CE reader | — |  | — |  |
| NET DE PARA | T37903M | 2,180 | Windows CE reader | — |  | no profile: CD music Off |  |
| NISHIKAZE | T44502M | 1,138 | Windows CE reader | — |  | — |  |
| PLUS PLUMB | T37901M | 1,214 | Windows CE reader | — |  | — |  |
| PRINCESS MAKER COLLECTION | T44801M | 1,874 | Windows CE reader | — |  | — (only short layout tracks) |  |
| PUZZLE BOBBLE4 | T42301M | 1,788 | Windows CE reader | — |  | no profile: CD music Off |  |
| RUNE JADE | T14304M | 1,596 | Windows CE reader | — |  | — |  |
| SEGA RALLY2 | HDR0010 | 1,414 | Windows CE reader | — |  | no profile: CD music Off |  |
| SEKAI FUSHIGI HAKKEN TROIA | T39701M | 1,166 | Windows CE reader | — |  | — |  |
| SPACE GRIFFON VF-9 | T39601M | 2,744 | Windows CE reader | — |  | no profile: CD music Off |  |
| SPIRIT OF SPEED 1937 | T-8102M | 1,258 | Windows CE reader | — |  | no profile: CD music Off |  |
| SUPER PRODUCERS | T14303M | 802 | Windows CE reader | — |  | no profile: CD music Off |  |
| SUPER ROBOT WARS ALPHA | T20602M | 1,246 | Windows CE reader | — |  | — |  |
| SUPER RUNABOUT | T22101M | 1,042 | Windows CE reader | — |  | no profile: CD music Off |  |
| SUPER RUNABOUT | T22101M | 1,042 | Windows CE reader | — |  | no profile: CD music Off |  |
| SUPER RUNABOUT SAN FRANCISCO EDITION | T22102M | 1,042 | Windows CE reader | — |  | no profile: CD music Off |  |
| SWEET SEASON | T37911M | 1,984 | Windows CE reader | — |  | — |  |
| TOMB RAIDER IV | T1233M | 1,252 | Windows CE reader | — |  | — |  |
| UNDERCOVER A.D.2025 KEI | T39001M | 1,026 | Windows CE reader | — |  | no profile: CD music Off |  |
| VIRTUACOP2 | HDR-0061 | 1,594 | Windows CE reader | — |  | no profile: CD music Off |  |
| WORLD NEVERLAND PLUS - OLERUD KINGDOM STORY - | T5301M | 3,910 | Windows CE reader | — |  | — |  |
| YOSHIANOOKADE NEKORONDE | T18704M | 2,916 | Windows CE reader | — |  | no profile: CD music Off |  |
| YUKIGATARI | T37909M | 2,340 | Windows CE reader | — |  | — (only short layout tracks) |  |

| Not checked yet | Product | Why | Console (recorded) |
| --- | --- | --- | --- |
| BIOHAZARD CODE VERONICA | T1207M | same startup as 027-T1210M-BIOHAZARD-CODE-VERONICA-GENTEIBAN |  |
| LE MANS 24 HOURS | HDR-0149 | startup not received yet |  |
| NBA 2K | HDR-0069 | startup not received yet |  |
| NFL2K2 | HDR-0196 | same startup as NBA 2K v1.000 (2000)(Sega)(JP)(en)[!]/NBA 2K v1.000 (2000)(Sega)(JP)(en)[!].gdi |  |
| WORLD SERIES BASEBALL 2K2 | HDR-0198 | startup not received yet |  |
