# US library compatibility table

Every US Dreamcast startup from the owner's full US library run
(`US-startup-01` to `08`, bundler v2.2, run `c2f5d6564a7a`), together with the
earlier library runs that already held 41 of them. Each row went through the
launcher's own admission checks with `tools/library_compat_table.py`, against
the compact Background stage end `0x8cebe748`. Column meanings and the gate
model are in [the library compatibility table](library-compat-table.md).
Static results are necessary evidence, not console compatibility.

| Result | Games |
| --- | ---: |
| Background reader with virtual VMU admitted | 54 |
| Background reader only (virtual VMU blocked) | 138 |
| Neither (Standard reader only) | 22 |
| Boot file is only a loader | 2 |
| Windows CE (separate reader; see [driver families](wince-vmu-driver-families.md)) | 36 |

Three discs could not be read; duplicate discs and repeat listings are not
rows.

## What blocks virtual VMU, by games affected

Each game counts once, under the first check that refuses it. A game may have
further blockers behind that one.

| First refusal | Games | Meaning |
| --- | ---: | --- |
| `layout` | 39 | Maple (controller) layout not resolved |
| `driver` | 27 | Maple driver family not resolved |
| `heap` | 25 | SDK heap family not resolved |
| `exits` | 19 | Terminal exits not resolved |
| `placement_384k` | 17 | 384 KiB lease does not fit |
| `initializer_call` | 15 | Heap initializer call not proved |
| `initializer_slot` | 8 | No heap initializer slot (fixed RAM; needs a RAM map) |
| `extent` | 6 | Executable extent not proved |
| `guarded_exits` | 4 | Terminal exits overlap hook windows |
| `loader_program` | 2 | Boot file only loads another program (rebinding needed) |

The Maple layout, Maple driver and SDK heap families together are the first
blocker for 91 games, so they are the highest-value fixes for universal virtual
VMU.

## Table

| Game | Product | Boot KiB | Background X/Y + virtual VMU | Without VMU | Blocker | CD music | Console (recorded) |
| --- | --- | ---: | --- | --- | --- | --- | --- |
| ARABIAN NIGHTS - PRINCE OF PERSIA | T41405N | 741 | virtual VMU | — |  | no profile: CD music Off |  |
| BANGAI-O | T40217N | 550 | virtual VMU | — |  | no profile: CD music Off |  |
| CHUCHU ROCKET | MK-51049 | 1,496 | virtual VMU | — |  | no profile: CD music Off |  |
| COASTER WORKS | T15128N | 1,004 | virtual VMU | — |  | no profile: CD music Off |  |
| CRAZY TAXI 2 | MK-51136 | 1,418 | virtual VMU | — |  | — | works on its own game card (virtual VMU, own game card) |
| D2 | MK-51036 | 2,819 | virtual VMU | — |  | — |  |
| DEMOLITION RACER NO EXIT DC | T15112N | 1,508 | virtual VMU | — |  | — (only short layout tracks) |  |
| DINO CRISIS | T1217N | 1,695 | virtual VMU | — |  | — |  |
| DISNEYS DINOSAUR | T-17717N | 1,195 | virtual VMU | — |  | — |  |
| ELEMENTAL GIMMICK GEAR | T41601N | 670 | virtual VMU | — |  | — | saves and loads (Background, virtual VMU in A1) |
| ESPN INTERNATIONAL TRACK AND FIELD | T9509N | 2,064 | virtual VMU | — |  | — |  |
| ESPN NBA2NIGHT | T9505N | 163 | virtual VMU | — |  | — (only short layout tracks) |  |
| EVOLUTION - THE WORLD OF SACRED DEVICE | T17706N | 2,093 | virtual VMU | — |  | — | save and load succeed; FMV slowdown reported (virtual VMU (A1 builds)) |
| EVOLUTION2 | T1711N | 3,961 | virtual VMU | — |  | — | save and load succeed; FMV slowdown reported (virtual VMU (A1 builds)) |
| F1 WORLD GRAND PRIX | T3001N | 1,015 | virtual VMU | — |  | no profile: CD music Off |  |
| FIGHTING FORCE 2 | 36801N | 3,936 | virtual VMU | — |  | no profile: CD music Off |  |
| FIGHTING FORCE 2 | 36801N | 3,935 | virtual VMU | — |  | no profile: CD music Off |  |
| GRANDIA2 | T17716N | 1,249 | virtual VMU | — |  | — (only short layout tracks) | works with virtual VMU (virtual VMU) |
| GUNDAM SIDE STORY 0079  RISE FROM THE ASHES | T13301N | 153 | virtual VMU | — |  | — |  |
| JET GRIND RADIO | MK-51058 | 3,003 | virtual VMU | — |  | — | virtual A2 works (virtual VMU (A2)) |
| JOJO S BIZARRE ADVENTURE | T1206N | 229 | virtual VMU | — |  | — |  |
| MAKEN X | MK-51050 | 1,825 | virtual VMU | — |  | no profile: CD music Off |  |
| MR.DRILLER | T1402N | 917 | virtual VMU | — |  | no profile: CD music Off |  |
| NBA SHOWTIME:NBA ON NBC | T-9706N | 3,100 | virtual VMU | — |  | no profile: CD music Off |  |
| NFL BLITZ 2000 | T-9703N | 2,465 | virtual VMU | — |  | no profile: CD music Off |  |
| NFL BLITZ 2000 | T-9703N | 2,451 | virtual VMU | — |  | no profile: CD music Off |  |
| POD SPEEDZONE | T-17713N | 1,717 | virtual VMU | — |  | no profile: CD music Off |  |
| POWER STONE 2 USA | T1211N | 3,024 | virtual VMU | — |  | — | saves and loads after a complete reboot (SD, card p0005); physical-disc virtual VMU also saves and reloads (SD virtual VMU and physical disc) |
| PSO | MK-51100 | 3,694 | virtual VMU | — |  | — | works; intro video slow (native) |
| RAYMAN 2 THE GREAT ESCAPE | 17707N | 913 | virtual VMU | — |  | — | virtual VMU shown; saving and loading work; FMVs and gameplay flawless (New with virtual VMU) |
| RECORD OF LODOSS WAR | T40218N | 3,392 | virtual VMU | — |  | — (only short layout tracks) | virtual VMU shown; saving and loading work; FMVs and gameplay flawless (New with virtual VMU) |
| RED DOG SUPERIOR FIREPOWER | T40215N | 3,384 | virtual VMU | — |  | — |  |
| RESIDENT EVIL CODE VERONICA | T1204N | 3,823 | virtual VMU | — |  | — | saves, power-cycle reload and Disc 2 verified (Background New (X), virtual VMU) |
| ROYAL RUMBLE | T10005N | 1,323 | virtual VMU | — |  | — |  |
| SEGA SMASH PACK VOLUME 1 | MK-51146 | 207 | virtual VMU | — |  | Smash collection route | hardware-accepted (Smash collection) |
| SEGAGT | MK-51053 | 1,721 | virtual VMU | — |  | no profile: CD music Off |  |
| SHADOWMAN | T-8106N | 1,574 | virtual VMU | — |  | — (only short layout tracks) |  |
| SHADOWMAN | T-8106N | 1,573 | virtual VMU | — |  | — (only short layout tracks) |  |
| SILVER | T15108N | 2,886 | virtual VMU | — |  | — | works: saves survive power-off, reboot and reload (tested before the prefetch work) (virtual VMU) |
| SKIES OF ARCADIA DISC 1 | MK-51052 | 5,332 | virtual VMU | — |  | — | launches every boot and reloads saves; see docs/native-skies-read-fix.md for the opening Cupil hang (virtual VMU) |
| SONIC ADVENTURE | MK-51000 | 6,592 | virtual VMU | — |  | — | works with the Background reader and virtual VMU (Background reader, virtual VMU) |
| SONIC ADVENTURE | MK-51000 | 6,577 | virtual VMU | — |  | — | works with the Background reader and virtual VMU (Background reader, virtual VMU) |
| SONIC ADVENTURE 2 | MK-51117 | 1,541 | virtual VMU | — |  | — | saves and loads; gameplay was choppy with input lag on that build (virtual VMU) |
| SPECOPS OMEGA SQUAD | T41704N | 827 | virtual VMU | — |  | no profile: CD music Off |  |
| STRIKER PRO 2000 | T15111N | 183 | virtual VMU | — |  | no profile: CD music Off |  |
| SWORD OF THE BERSERK GUTS RAGE | T-36805N | 1,641 | virtual VMU | — |  | — | works; the game only uses a card in A1 and ignores a virtual card in A2 (virtual VMU in A1) |
| TECH ROMANCER | T1208N | 2,585 | virtual VMU | — |  | — |  |
| TEST DRIVE 6 | T15102N | 1,056 | virtual VMU | — |  | — (only short layout tracks) |  |
| THE HOUSE OF THE DEAD 2 | MK-51002 | 793 | virtual VMU (fixed arena) | — |  | — | saves and reloads after power-off; one freeze at the first boss (Original mode, VMU On) and minor lag; VMU Off did not crash (Background, virtual VMU (fixed arena)); known limit: residual lag; bounded scheduling work (integration roadmap section 3; not owner-confirmed) |
| THE KING OF FIGHTERS DREAM MATCH 1999 | T3101N | 2,314 | virtual VMU | — |  | no profile: CD music Off |  |
| THE KING OF FIGHTERS EVOLUTION | T44302N | 3,952 | virtual VMU | — |  | — |  |
| THE RING | T15122N | 1,713 | virtual VMU | — |  | — |  |
| TIME STALKERS | MK-51011 | 645 | virtual VMU | — |  | — | boots, saves and reloads (Background New (X), virtual VMU in A1) |
| TOY COMMANDER | MK-57020 | 730 | virtual VMU | — |  | native profile (Toy Commander) | sees imported saves, creates new saves and loads them after reboot; CD music works; intro movies still stall (virtual VMU; native CDDA) |
| 102 DALMATIANS | T36803N | 711 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| 4WHEELTHUNDER | T-9708N | 3,576 | refused | New, no virtual VMU | executable extent not proved | no profile: CD music Off |  |
| AEROWINGS | T40201N | 4,078 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| AEROWINGS 2 AIR STRIKE | T40210N | 2,887 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| ALIENFRONT ONLINE | MK-51171 | 2,253 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | no profile: CD music Off |  |
| ALONE IN THE DARK - THE NEW NIGHTMARE | T15117N | 801 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| ARMY MEN SARGES HEROES | T-9715N | 1,977 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| BOMBERMAN ONLINE | MK-51065 | 2,483 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| BUZZ 2000 | T13007N | 962 | refused | New, no virtual VMU | 284 KiB lease does not fit | — (only short layout tracks) |  |
| CANNON SPIKE | T1215N | 619 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| CAPCOM VS SNK | T1218N | 2,347 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| CARRIER | T5701N | 11,611 | refused | New, no virtual VMU | executable extent not proved | — |  |
| CENTIPEDE | T40602N | 3,254 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| CHARGEN BLAST | T15127N | 2,478 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| CHICKEN RUN | T36811N | 922 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| CONFIDENTIAL MISSION | MK-51160 | 1,744 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| CONFLICT ZONE | T17721N | 2,400 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| CRAZY TAXI | MK-51035 | 1,433 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | — | works without virtual VMU (standalone New); known limit: needs a reviewed retained arena (integration roadmap section 3; not owner-confirmed) |
| CYBER TROOPERS VIRTUAL-ON ORATORIO TANGRAM | T13004N | 1,595 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| DAVE MIRRA FREESTYLE BMX | T-8120N | 3,795 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| DAYTONAUSA | MK-51037 | 1,963 | refused | New, no virtual VMU | executable extent not proved | — |  |
| DEAD OR ALIVE 2 | T3601N | 1,773 | refused | New, no virtual VMU | SDK heap family not resolved | — | works with virtual VMU; Background loader plus prefetch did not fit in RAM, so it runs with the Background loader only (Background loader with virtual VMU, no prefetch) |
| DEATH CRIMSON OX | T2401N | 9,151 | refused | New, no virtual VMU | executable extent not proved | — |  |
| DEEP FIGHTER | T17705N | 1,812 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | no profile: CD music Off |  |
| DISNEY S DONALD DUCK GOIN QUACKER | T17714D50 | 1,122 | refused | New, no virtual VMU | terminal exits overlap hook windows | no profile: CD music Off |  |
| DRACONUS CULT OF THE WYRM | T40203N | 2,284 | refused | New, no virtual VMU | terminal exits not resolved | — (only short layout tracks) |  |
| DRAGONRIDERS - THE CHRONICLES OF PERN | T17720N | 1,113 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| ECCO THE DOLPHIN DEFENDER OF THE FUTURE | MK-51033 | 2,906 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | no profile: CD music Off |  |
| ECW ANARCHY RULZ | T-8114N | 4,366 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| ECW HARDCORE REVOLUTION | T-8112N | 2,663 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| EVIL DEAD HAIL TO THE KING | T10003N | 8,450 | refused | New, no virtual VMU | 284 KiB lease does not fit | — (only short layout tracks) |  |
| EXPENDABLE | T15104N | 4,820 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| FATAL FURY MARK OF THE WOLVES | T44306N | 1,491 | refused | New, no virtual VMU | Maple driver family not resolved | no profile: CD music Off |  |
| FROGGER 2 | T40604N | 818 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| FUR FIGHTERS | T-8107N | 1,506 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| GAUNTLET LEGENDS | T9710N | 3,673 | refused | New, no virtual VMU | 284 KiB lease does not fit | — (only short layout tracks) |  |
| GENERATOR 2 | MK-51057 | 159 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off | known limit: demo disc: its menu starts several separate game programs (native and Windows CE); the checks cover the menu only |
| GETBASS2 | MK-51166 | 2,248 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| GIGA WING2 | T1222N | 1,401 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| GIGAWING | T1209N | 1,503 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| GRAND THEFT AUTO 2 | T42102N | 4,394 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| GUNBIRD2 | T1214N | 1,122 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| HEAVY METAL GEOMATRIX | T1223N | 1,199 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | — |  |
| HYDRO THUNDER | T-9702N | 1,844 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| I - SPY | T41302N | 1,289 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| ILLBLEED | T46001N | 1,357 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| INCOMING | T12503N | 11,185 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| IRON ACES | T15129N | 988 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| JEREMY MCGRATH SUPERCROSS 2000 | T-8104N | 1,297 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| KAO THE KANGAROO | T22903N | 820 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| LOONEY TUNES SPACE RACE | T15116N | 105 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| MAGFORCE RACING | T40208N | 523 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| MARS MATRIX | T1221N | 2,336 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| MARVEL VS CAPCOM 2 | T1212N | 1,767 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| MAT HOFFMANS PRO BMX | T13005N | 6,812 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| MAX STEEL COV | T41402N | 2,542 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| MDK2 | T12502N | 733 | refused | New, no virtual VMU | Maple driver family not resolved | native profile (MDK2) |  |
| METROPOLIS STREET RACER | MK-51012 | 488 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| MONACO GRAND PRIX | 17701N | 1,366 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| MORTAL KOMBAT GOLD | T-9701N | 6,142 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| MORTAL KOMBAT GOLD | T-9701N | 6,178 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| MS. PAC-MAN MAZE MADNESS | T1404N | 1,343 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| MTV SPORTS SKATEBOARDING | T10004N | 901 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| NAMCO MUSEUM | T1403N | 1,657 | refused | New, no virtual VMU | 284 KiB lease does not fit | — (only short layout tracks) |  |
| NBA 2K | MK-51004 | 3,231 | refused | New, no virtual VMU | 284 KiB lease does not fit | — (only short layout tracks) |  |
| NBA HOOPZ | T9709N | 11,361 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| NCAA COLLEGE FOOTBALL 2K2 | MK-51176 | 3,231 | refused | New, no virtual VMU | 284 KiB lease does not fit | — (only short layout tracks) |  |
| NFL 2K | MK-51003 | 3,231 | refused | New, no virtual VMU | 284 KiB lease does not fit | — (only short layout tracks) |  |
| NFL BLITZ 2001 | T9712N | 1,909 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| NHL 2K | MK-51025 | 1,419 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| NHL2K2 | MK-51182 | 2,631 | refused | New, no virtual VMU | heap initializer call not proved | — (only short layout tracks) |  |
| OMIKRON THE NOMAD SOUL | T36807N | 1,038 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| PENPEN TRIICELON | T15105N | 2,234 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| POWER STONE | T1201N | 3,334 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| PROJECT JUSTICE | T1219N | 2,941 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| PSO VER.2 | MK-51193 | 153 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| RAZOR FREESTYLE SCOOTER | T40219N | 829 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| RE-VOLT | T-8109N | 2,868 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| READY 2 RUMBLE BOXING | T9704N | 2,002 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| READY 2 RUMBLE BOXING | T9704N | 2,012 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| READY 2 RUMBLE BOXING ROUND 2 | T9717N | 1,379 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| REELFISHINGWILD | T44303N | 1,776 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| RESIDENT EVIL3 | T1220N | 2,046 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| RIPPIN' RIDERS | MK-51010 | 1,362 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| ROADSTERS RC7 | T22901N | 796 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| RUSH 2049 | T-9707N | 1,097 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| SAMBA DE AMIGO | MK-51092 | 2,115 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| SEAMAN | MK-51048 | 2,465 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| SEGA MARINE FISHING | MK-51096 | 1,409 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| SEVENTH CROSS EVOLUTION | T41301N | 1,669 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| SILENT SCOPE | T9507N | 1,353 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SLAVE ZERO | T15106N | 1,295 | refused | New, no virtual VMU | Maple driver family not resolved | — (only short layout tracks) |  |
| SLAVE ZERO | T15106N | 1,294 | refused | New, no virtual VMU | Maple driver family not resolved | — (only short layout tracks) |  |
| SOLDIER OF FORTUNE | T40212N | 754 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| SONIC ADVENTURE LIMITED EDITION | MK-51014 | 6,584 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| SONIC SHUFFLE | MK-51060 | 998 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SOUL FIGHTER | T41401N | 659 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| SOULCALIBUR | T1401N | 3,593 | refused | New, no virtual VMU | terminal exits not resolved | — | known limit: incomplete dump (its data track ends before the boot file); set aside for now |
| SOUTH PARK RALLY | T-8116N | 1,154 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| SPACECHANNEL5 | MK-51051 | 2,432 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| SPAWN IN THE DEMONS HAND | T1216N | 1,191 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | — | boots; intro problems; audio distortion observed (native) |
| SPEED DEVILS | T17702N | 3,480 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| SPEED DEVILS ONLINE | T17718N | 495 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| SPIDER-MAN | T13008N | 3,761 | refused | New, no virtual VMU | terminal exits overlap hook windows | — (only short layout tracks) |  |
| STAR WARS DEMOLITION | T23003N | 3,723 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| STAR WARS EPISODE 1 RACER | T23001N | 993 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| STAR WARS EPISODE ONE JEDI POWER BATTLES | T23002N | 1,550 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| STREET FIGHTER ALPHA3 SAIKYO-DOJO | T1203N | 7,589 | refused | New, no virtual VMU | executable extent not proved | — |  |
| STREET FIGHTER3 DOUBLE IMPACT | T1210N | 803 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| STUPID INVADERS | T17708N | 721 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| SUPER MAGNETIC NEO | T40206N | 3,297 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| SURF ROCKET RACERS | T40216N | 2,030 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| SUZUKI ALSTARE EXTREME RACING | T17703N | 1,008 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| TEE OFF | T8108N | 1,290 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| TENNIS 2K2 | MK-51186 | 2,703 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| TENNIS 2K2 | MK-51186 | 2,699 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| TEST DRIVE V-RALLY | T15110N | 2,344 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | no profile: CD music Off |  |
| THE GRINCH | T9512N | 801 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| THE LAST BLADE 2 | T44305N | 1,286 | refused | New, no virtual VMU | Maple driver family not resolved | no profile: CD music Off |  |
| THE TYPING OF THE DEAD | MK-51144 | 1,078 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| TNN MOTORSPORTS HARDCORE HEAT | T0000M | 4,499 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| TOKYO XTREME RACER | T40202N | 1,041 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| TOKYO XTREME RACER 2 | T40211N | 1,370 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| TONY HAWK'S PRO SKATER | T40205N | 2,800 | refused | New, no virtual VMU | terminal exits overlap hook windows | — (only short layout tracks) |  |
| TONY HAWKS PRO SKATER 2 | T13006N | 3,428 | refused | New, no virtual VMU | 284 KiB lease does not fit | — (only short layout tracks) |  |
| TRICKSTYLE | T-8102N | 1,070 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | no profile: CD music Off |  |
| ULTIMATE FIGHTING CHAMPIONSHIP | T40204N | 1,009 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| UNREAL TOURNAMENT | T15125N | 166 | refused | New, no virtual VMU | terminal exits overlap hook windows | no profile: CD music Off |  |
| VANISHING POINT | T-8110N | 1,476 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| VIGILANTE 8 SECOND OFFENSE | T13002N | 3,447 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| VIRTUA TENNIS | MK-51054 | 2,035 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| VIRTUAATHLETE2000 | T44301N | 1,699 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| WACKY RACES | T15113N | 158 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| WALT DISNEY WORLD QUEST MAGICAL RACING TOUR | T36804N | 2,451 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| WORLD SERIES BASEBALL 2K1 | MK-51055 | 4,285 | refused | New, no virtual VMU | executable extent not proved | — |  |
| WWF: ATTITUDE | T-8103N | 2,962 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| XTREME SPORTS | T15126N | 1,906 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| ZOMBIE REVENGE | MK-51038 | 10,057 | refused | New, no virtual VMU | Maple layout not resolved | — | works without virtual VMU (standalone New); known limit: six-input Maple layout and allocator/reservation (integration roadmap section 3; not owner-confirmed) |
| 18WHEELER | MK-51064 | 2,112 | refused | refused | Maple driver family not resolved | — |  |
| AIRFORCE DELTA | T9501N | 1,600 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| BLUE STINGER | T13001N | 1,929 | refused | refused | Maple driver family not resolved | — | known limit: earlier interrupt-table initializer and its ownership (integration roadmap section 3; not owner-confirmed) |
| CART RACING | MK-51007 | 1,191 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| DYNAMITE COP | MK-51013 | 1,873 | refused | refused | Maple driver family not resolved | — |  |
| F355 CHALLENGE | MK-0100 | 1,563 | refused | refused | Maple driver family not resolved | — |  |
| FLOIGAN BROS. EPISODE 1  MOIGLE'S SECTRET PROJECT | MK-51114 | 848 | refused | refused | Maple driver family not resolved | — (only short layout tracks) |  |
| HYDRO THUNDER | T-9702N | 1,877 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| MARVEL VS.CAPCOM | T1202N | 4,815 | refused | refused | Maple driver family not resolved | — |  |
| OOGA BOOGA | MK-51140 | 1,356 | refused | refused | Maple driver family not resolved | — (only short layout tracks) |  |
| OUTTRIGGER | MK-51102 | 2,815 | refused | refused | Maple driver family not resolved | — |  |
| PLASMA SWORD | T1207N | 9,291 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| PSYCHIC FORCE 2012 | T-31101N | 4,605 | refused | refused | Maple driver family not resolved | — |  |
| SEGA BASS FISHING | MK-51006 | 1,117 | refused | refused | Maple driver family not resolved | — |  |
| SHENMUE | MK-51059 | 2,632 | refused | refused | Maple driver family not resolved | — (only short layout tracks) |  |
| SHENMUE | MK-51059 | 4,936 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| SHENMUE2 EU | MK-5118450 | 3,121 | refused | refused | Maple driver family not resolved | — (only short layout tracks) |  |
| SOUL REAVER | T36802N | 3,374 | refused | refused | 284 KiB lease does not fit | — (only short layout tracks) |  |
| SYDNEY 2000 | T36808N | 1,608 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| VF3TB | MK-51001 | 1,152 | refused | refused | Maple driver family not resolved | — |  |
| VIRTUA STRIKER 2 VER.2000.1 | MK-51028 | 2,112 | refused | refused | Maple driver family not resolved | — |  |
| WETRIXPLUS | T8111N | 1,441 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| QUAKE III ARENA | MK-51061 | 96 | not established (loader) | not established (loader) | boot file is only a loader; the stage binds the loader, not the program it starts. Its programs: Q3_USA.BIN (1,285 KiB) terminal exits not resolved; INTRO.BIN (346 KiB) terminal exits not resolved | no profile: CD music Off |  |
| TEST DRIVE LE MANS | T15123N | 105 | not established (loader) | not established (loader) | boot file is only a loader; the stage binds the loader, not the program it starts. Its programs: LEMANS24.BIN (2,229 KiB) Maple layout not resolved; FMV.BIN (487 KiB) SDK heap family not resolved | — (only short layout tracks) |  |
| 4X4 EVOLUTION | T41903N | 1,666 | Windows CE reader | — |  | no profile: CD music Off |  |
| ARMADA | T40301N 00 | 1,224 | Windows CE reader | — |  | CE profile (Armada) | boots; intro movie playback is poor (under investigation) (Windows CE) |
| ATARI ANNIVERSARY EDITION | T15130N | 1,312 | Windows CE reader | — |  | — (only short layout tracks) |  |
| BANG! GUNSHIP ELITE | T44102N | 1,324 | Windows CE reader | — |  | no profile: CD music Off |  |
| BUST A MOVE 4 | T-8117N | 1,788 | Windows CE reader | — |  | CE profile (Bust-A-Move 4) |  |
| CAESARS PALACE 2000 | T-12504N | 1,164 | Windows CE reader | — |  | no profile: CD music Off |  |
| CHAMPIONSHIP SURFER | T41403N | 1,854 | Windows CE reader | — |  | no profile: CD music Off |  |
| DUCATI WORLD | T-8113N | 1,258 | Windows CE reader | — |  | no profile: CD music Off |  |
| GREATEST ARCADE HITS 1 | T-9713N | 1,940 | Windows CE reader | — |  | — (only short layout tracks) |  |
| GREATEST ARCADE HITS 2 | T-9714N | 1,320 | Windows CE reader | — |  | no profile: CD music Off |  |
| HIDDEN AND DANGEROUS | T-40502-N | 2,258 | Windows CE reader | — |  | — (only short layout tracks) |  |
| HOYLE CASINO | T11008N 00 | 1,118 | Windows CE reader | — |  | — (only short layout tracks) |  |
| KISS PSYCHO CIRCUS THE NIGHTMARE CHILD | T41901N | 1,174 | Windows CE reader | — |  | no profile: CD music Off |  |
| NFL QUARTERBACK CLUB 2001 | T-8115N | 1,302 | Windows CE reader | — |  | no profile: CD music Off |  |
| NIGHTMARE CREATURES II | T9504M | 2,368 | Windows CE reader | — |  | no profile: CD music Off |  |
| QBERT | T40403N | 1,258 | Windows CE reader | — |  | no profile: CD music Off |  |
| QUARTERBACK CLUB 2000 | T-8101N | 1,046 | Windows CE reader | — |  | no profile: CD music Off |  |
| RAILROAD TYCOON II DREAMCAST EDITION | T41902N | 1,090 | Windows CE reader | — |  | no profile: CD music Off |  |
| RAINBOW SIX | T40401N | 1,162 | Windows CE reader | — |  | no profile: CD music Off |  |
| RESIDENT EVIL 2 | T1205N | 1,178 | Windows CE reader | — |  | no profile: CD music Off |  |
| RESIDENT EVIL 2 | T1205N | 1,178 | Windows CE reader | — |  | no profile: CD music Off |  |
| SEGA RALLY 2 | MK-51019 | 1,144 | Windows CE reader | — |  | no profile: CD music Off |  |
| SIERRA SPORTS MAXIMUM POOL | T11010N | 1,658 | Windows CE reader | — |  | — (only short layout tracks) |  |
| SNO-CROSS | T40207N | 1,292 | Windows CE reader | — |  | no profile: CD music Off |  |
| SOUTH PARK CHEFS | T-8105N | 1,002 | Windows CE reader | — |  | no profile: CD music Off |  |
| SPIRIT OF SPEED 1937 | T-8118N | 1,258 | Windows CE reader | — |  | no profile: CD music Off |  |
| STARLANCER | T40209N | 1,908 | Windows CE reader | — |  | — (only short layout tracks) |  |
| SUPER RUNABOUT | T12509N | 1,042 | Windows CE reader | — |  | no profile: CD music Off |  |
| THE NEXT TETRIS ONLINE EDITION | T40214N | 1,932 | Windows CE reader | — |  | no profile: CD music Off |  |
| TOMB RAIDER CHRONICLES | T36812N | 1,122 | Windows CE reader | — |  | no profile: CD music Off |  |
| TOMB RAIDER THE LAST REVELATION | T-36806N | 1,252 | Windows CE reader | — |  | — |  |
| URBAN CHAOS | T36810N | 1,132 | Windows CE reader | — |  | no profile: CD music Off |  |
| WHO WANTS TO BEAT UP A MILLIONAIRE | T11011N | 1,132 | Windows CE reader | — |  | — (only short layout tracks) |  |
| WILD METAL | T42101N 00 | 3,276 | Windows CE reader | — |  | no profile: CD music Off |  |
| WORMS ARMAGEDDON | T40601N | 1,084 | Windows CE reader | — |  | CE profile (Worms Armageddon) |  |
| WORMS WORLD PARTY | T22904N | 1,786 | Windows CE reader | — |  | CE profile (Worms World Party) |  |

| Not checked yet | Product | Why | Console (recorded) |
| --- | --- | --- | --- |
| NBA2K1 | MK-51063 | same startup as 129-MK-51004-NBA-2K |  |
| NBA2K2 | MK-51178 | same startup as 129-MK-51004-NBA-2K |  |
| NFL2K1 | MK-51062 | same startup as 138-MK-51176-NCAA-COLLEGE-FOOTBALL-2K2 |  |
| NFL2K2 | MK-51168 | same startup as 129-MK-51004-NBA-2K |  |
| ROGUESPEAR | T40402N | same startup as 060-T9505N-ESPN-NBA2NIGHT |  |
| SOULCALIBUR | T1401N | not read: Startup extent leaves the data tracks at LBA 547203 (tracks 0-300, 450-976 audio, 45000-171916) | known limit: incomplete dump (its data track ends before the boot file); set aside for now |
| SPORTS JAM | T44304N | not read: Required startup file 1ST_READ.BIN is 13,480,448 bytes; the per-file limit is 12,582,912 bytes |  |
| STREET FIGHTER 3 3RD STRIKE | T1213N | not read: Required startup file 1ST_READ.BIN is 14,329,504 bytes; the per-file limit is 12,582,912 bytes |  |
| TOY STORY 2 / BUZZ LIGHTYEAR TO THE RESCUE | T13003N | not read: Required startup file 1ST_READ.BIN is 13,631,086 bytes; the per-file limit is 12,582,912 bytes |  |
| WORLD SERIES BASEBALL 2K2 | MK-51152 | same startup as 287-T15125N-UNREAL-TOURNAMENT |  |

Also listed: 89 files in other image formats.
