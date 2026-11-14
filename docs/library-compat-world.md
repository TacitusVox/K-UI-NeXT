# Worldwide library compatibility table

Every Dreamcast startup received from the owner, by region: the US runs, the
JPN run (`eee9cdc7`), and the full CHD collection run (`39dbe1a6`, bundler
v2.4, 33 parts, Redump and TOSEC CHDs, which adds PAL). The tables went
through the launcher's own admission checks with `tools/library_compat_table.py`,
against the compact Background stage end `0x8cebe748`. Column meanings are in
[the library compatibility table](library-compat-table.md); see also
[US](library-compat-us.md) and [JPN](library-compat-jpn.md). Static results
are necessary evidence, not console compatibility.

| Region | Background with virtual VMU | Background only | Neither | Windows CE |
| --- | ---: | ---: | ---: | ---: |
| US | 55 | 148 | 28 | 36 |
| JPN | 111 | 409 | 98 | 53 |
| PAL | 44 | 168 | 31 | 38 |
| Multi-region | 2 | 8 | 0 | 0 |
| **All** | **212** | **733** | **157** | **127** |

Two further US startups are loaders. Repeat listings across runs and
duplicate discs are not rows. The CHD run could not read 83 of its 1,484
images: 73 unlicensed or indie CD releases, and 10 MIL-CD, soundtrack or
system discs (09 Chairs, Checkicco no Miru CD, the D2 and Himitsu soundtracks,
Dreamcast System-Disc 2, Hang the DJ, Kita e. White Illumination, Newpowered
Generation 1, Space Channel 5 Theme Music, dps - Heartbreak Diary). None is a
licensed GD-ROM game.

## What blocks virtual VMU worldwide

Each game counts once, under the first check that refuses it.

| First refusal | Games | Meaning |
| --- | ---: | --- |
| `driver` | 260 | Maple driver family not resolved |
| `layout` | 185 | Maple layout not resolved |
| `exits` | 170 | Terminal exits not resolved |
| `heap` | 80 | SDK heap family not resolved |
| `placement_384k` | 60 | 384 KiB lease does not fit |
| `initializer_call` | 46 | Heap initializer call not proved |
| `extent` | 45 | Executable extent not proved |
| `initializer_slot` | 28 | No heap initializer slot (fixed RAM; needs a RAM map) |
| `guarded_exits` | 16 | Terminal exits overlap hook windows |
| `loader_program` | 2 | Boot file only loads another program |

## Table

| Game | Product | Boot KiB | Background X/Y + virtual VMU | Without VMU | Blocker | CD music | Console (recorded) |
| --- | --- | ---: | --- | --- | --- | --- | --- |
| AKIHABARA DENNOUGUMI PATAPIES | HDR-0019 | 3,228 | virtual VMU | — |  | — |  |
| ARABIAN NIGHTS - PRINCE OF PERSIA | T41405N | 741 | virtual VMU | — |  | no profile: CD music Off |  |
| BAKURETSUMUTEKI BANGAIOH | T38702M | 546 | virtual VMU | — |  | no profile: CD music Off |  |
| BANGAI-O | T40217N | 550 | virtual VMU | — |  | no profile: CD music Off |  |
| BANGAI-O | T7011D  50 | 550 | virtual VMU | — |  | no profile: CD music Off |  |
| BATTLE BEASTER | T43801M | 2,004 | virtual VMU | — |  | — |  |
| BERSERK | T2101M | 1,637 | virtual VMU | — |  | — |  |
| BIKKURIMAN 2000 VIVA FESTIVA | T41801M | 1,210 | virtual VMU | — |  | — |  |
| BIOHAZARD CODE VERONICA | 6107724 | 3,876 | virtual VMU | — |  | — |  |
| BIOHAZARD CODE VERONICA GENTEIBAN | T1210M | 3,881 | virtual VMU | — |  | — |  |
| BIOHAZARD2 VALUE PLUS | T1214M | 3,708 | virtual VMU | — |  | — |  |
| BLACK MATRIX AD | T20101M | 925 | virtual VMU | — |  | — |  |
| BOMBER HEHHE | T42903M | 748 | virtual VMU | — |  | — |  |
| CHOKOSENKI KIKAIOH | T1206M | 2,592 | virtual VMU | — |  | — |  |
| CHUCHU ROCKET | MK-51049 | 1,496 | virtual VMU | — |  | no profile: CD music Off |  |
| CHUCHU ROCKET | MK-5104950 | 1,550 | virtual VMU | — |  | no profile: CD music Off |  |
| CHUCHUROCKET | HDR-0039 | 3,127 | virtual VMU | — |  | no profile: CD music Off |  |
| CHUCHUTENTOU | 6107384 | 3,442 | virtual VMU | — |  | no profile: CD music Off |  |
| COASTER WORKS | T15128N | 1,004 | virtual VMU | — |  | no profile: CD music Off |  |
| COASTER WORKS | T44903D 50 | 1,108 | virtual VMU | — |  | no profile: CD music Off |  |
| CRAZY TAXI 2 | MK-51136 | 1,418 | virtual VMU | — |  | — | works on its own game card (virtual VMU, own game card) |
| D2 | MK-51036 | 2,819 | virtual VMU | — |  | — |  |
| D2 | T30006M | 2,808 | virtual VMU | — |  | — |  |
| D2 THE FIRST TOUCH | 6107164 05 | 2,713 | virtual VMU | — |  | — |  |
| DANCINGBLADE KATTENI MOMO TENSI | T9504M | 412 | virtual VMU | — |  | — |  |
| DE LA JET SET RADIO | HDR-0128 | 2,977 | virtual VMU | — |  | — |  |
| DEMOLITION RACER NO EXIT DC | T15112N | 1,508 | virtual VMU | — |  | — (only short layout tracks) |  |
| DINO CRISIS | T1217N | 1,695 | virtual VMU | — |  | — |  |
| DINO CRISIS | T1220M | 4,526 | virtual VMU | — |  | — |  |
| DINO CRISIS | T7019D  09 | 1,706 | virtual VMU | — |  | — |  |
| DINO CRISIS | T7019D  18 | 1,706 | virtual VMU | — |  | — |  |
| DINO CRISIS | T7019D  13 | 1,704 | virtual VMU | — |  | — |  |
| DINO CRISIS | T7019D  06 | 1,706 | virtual VMU | — |  | — |  |
| DISNEY S DONALD DUCK QUACK ATTACK | T17714D50 | 1,122 | virtual VMU | — |  | — |  |
| DISNEYS DINOSAUR | T-17717N | 1,195 | virtual VMU | — |  | — |  |
| DORICATCH SERIES - FUNCARGO | T39206M | 978 | virtual VMU | — |  | — |  |
| DREAM PREVIEW VOL6 | 610702706 | 457 | virtual VMU | — |  | — |  |
| DREAMCAST EXPRESS VOL2 | 610711002 | 504 | virtual VMU | — |  | — |  |
| DREAMCAST GENERATOR | 51034 | 158 | virtual VMU | — |  | no profile: CD music Off |  |
| DREAMCAST MAGAGINE GD VOL1 | 6107140 | 198 | virtual VMU | — |  | — (only short layout tracks) |  |
| DREAMCAST SEQUENCER OTOIRE | T32801M | 821 | virtual VMU | — |  | — |  |
| DREAMON 4 | 8320011 50 | 973 | virtual VMU | — |  | no profile: CD music Off |  |
| ELEMENTAL GIMMICK GEAR | T41601N | 670 | virtual VMU | — |  | — | saves and loads (Background, virtual VMU in A1) |
| ESPN INTERNATIONAL TRACK AND FIELD | T9509N | 2,064 | virtual VMU | — |  | — |  |
| ESPN INTERNATIONAL TRACK AND FIELD | T9504D | 2,064 | virtual VMU | — |  | — |  |
| ESPN NBA2NIGHT | T9505N | 163 | virtual VMU | — |  | — (only short layout tracks) |  |
| ETERNAL ARCADIA | HDR-0076 | 5,338 | virtual VMU | — |  | — |  |
| ETERNAL ARCADIA KUZOKUBAN | 6107810 | 3,929 | virtual VMU | — |  | — |  |
| EVOLUTION - THE WORLD OF SACRED DEVICE | T17706N | 2,093 | virtual VMU | — |  | — | save and load succeed; FMV slowdown reported (virtual VMU (A1 builds)) |
| EVOLUTION - THE WORLD OF SACRED DEVICE | T17705D 05 | 2,093 | virtual VMU | — |  | — |  |
| EVOLUTION2 | T1711N | 3,961 | virtual VMU | — |  | — | save and load succeed; FMV slowdown reported (virtual VMU (A1 builds)) |
| EVOLUTION2 | T38703M | 3,969 | virtual VMU | — |  | — |  |
| EVOLUTION2 FAR OFF PROMISE | T45005D 50 | 3,963 | virtual VMU | — |  | — |  |
| F1 WORLD GRAND PRIX | T3001N | 1,015 | virtual VMU | — |  | no profile: CD music Off |  |
| F1 WORLD GRAND PRIX II | T3002M  50 | 1,182 | virtual VMU | — |  | no profile: CD music Off |  |
| F1 WORLD GRAND PRIX II FOR DREAMCAST | T3002M | 1,180 | virtual VMU | — |  | no profile: CD music Off |  |
| F1 WORLD GRAND PRIX II FOR DREAMCAST | 6107922 | 1,178 | virtual VMU | — |  | no profile: CD music Off |  |
| FIGHTING FORCE 2 | 36801N | 3,936 | virtual VMU | — |  | no profile: CD music Off |  |
| FIGHTING FORCE 2 | 36801N | 3,935 | virtual VMU | — |  | no profile: CD music Off |  |
| FIGHTING FORCE 2 | 36802D 80 | 3,935 | virtual VMU | — |  | no profile: CD music Off |  |
| FIGHTING FORCE 2 | 36802D 18 | 3,935 | virtual VMU | — |  | no profile: CD music Off |  |
| FORMULA 1 WORLD GRAND PRIX FOR DREAMCAST | T3001D  50 | 1,001 | virtual VMU | — |  | no profile: CD music Off |  |
| FUR FIGHTERS AND DOA 2 DEMO DISK | D-0009-50 | 1,089 | virtual VMU | — |  | no profile: CD music Off |  |
| GAMBARE NIPPON OLYMPIC 2000 | T9511M | 2,046 | virtual VMU | — |  | — |  |
| GODZILLA GENERATIONS MAXIMUM IMPACT | HDR-0047 | 1,466 | virtual VMU | — |  | — |  |
| GRANDIA2 | T17716N | 1,249 | virtual VMU | — |  | — (only short layout tracks) | works with virtual VMU (virtual VMU) |
| GRANDIA2 | T4503M | 1,249 | virtual VMU | — |  | — |  |
| GRANDIA2 | T17715D | 1,249 | virtual VMU | — |  | — (only short layout tracks) |  |
| GRANDIA2 DEMO | 6107878 | 1,249 | virtual VMU | — |  | — |  |
| GUNDAM SIDE STORY 0079  +VS RX-78-2 GUNDAM | T13303M | 154 | virtual VMU | — |  | — |  |
| GUNDAM SIDE STORY 0079  PREMIUM DISC | 6107160 | 153 | virtual VMU | — |  | — |  |
| GUNDAM SIDE STORY 0079  RISE FROM THE ASHES | T13301N | 153 | virtual VMU | — |  | — |  |
| GUNDAM SIDE STORY 0079  RISE FROM THE ASHES | T13302M | 153 | virtual VMU | — |  | — |  |
| HELLO KITTY NO OTONARU MAIL | HDR-0065 | 2,695 | virtual VMU | — |  | — |  |
| ILLBLEED | T45601M | 1,358 | virtual VMU | — |  | — |  |
| JAH-MONG | T-43401M | 778 | virtual VMU | — |  | no profile: CD music Off |  |
| JET GRIND RADIO | MK-51058 | 3,003 | virtual VMU | — |  | — | virtual A2 works (virtual VMU (A2)) |
| JET SET RADIO | HDR-0078 | 2,857 | virtual VMU | — |  | — |  |
| JETCOASTER DREAM | T40902M | 1,010 | virtual VMU | — |  | no profile: CD music Off |  |
| JETCOASTER DREAM2 | T41201M | 2,111 | virtual VMU | — |  | no profile: CD music Off |  |
| JIKKYO PAWAFURU PUROYAKYU DREAMCAST EDITION | T9507M | 3,452 | virtual VMU | — |  | — |  |
| JINSEIGAME FOR DREAMCAST | T10301M | 1,764 | virtual VMU | — |  | — |  |
| JOJO S BIZARRE ADVENTURE | T1206N | 229 | virtual VMU | — |  | — |  |
| KIKAIOH FOR MATCHING | T1232M | 3,708 | virtual VMU | — |  | — |  |
| KITAHE PHOTO MEMORIES | T14306M | 354 | virtual VMU | — |  | no profile: CD music Off |  |
| KITERETSU BOYS | HDR-0064 | 1,083 | virtual VMU | — |  | — |  |
| KUONNOKIZUNA SAIRINSYO | T36701M | 796 | virtual VMU | — |  | — |  |
| KUONNOKIZUNA SAIRINSYO | T36701M | 796 | virtual VMU | — |  | — |  |
| LOVEHINA | HDR-0102 | 1,145 | virtual VMU | — |  | — |  |
| MACROSS M3 VER.EX | 6107961 | 1,728 | virtual VMU | — |  | — |  |
| MAKEN X | MK-51050 | 1,825 | virtual VMU | — |  | no profile: CD music Off |  |
| MAKEN X | T14401M | 1,830 | virtual VMU | — |  | no profile: CD music Off |  |
| MAKEN X | MK-5105050 | 1,840 | virtual VMU | — |  | no profile: CD music Off |  |
| MEMORIES OFF COMPLETE | T19702M | 800 | virtual VMU | — |  | no profile: CD music Off |  |
| MERCURIUS PRETTY END OF THE CENTURY | T20103M | 644 | virtual VMU | — |  | — |  |
| MR.DRILLER | T1402N | 917 | virtual VMU | — |  | no profile: CD music Off |  |
| MR.DRILLER | T1402M | 914 | virtual VMU | — |  | no profile: CD music Off |  |
| MR.DRILLER | T7020D | 975 | virtual VMU | — |  | no profile: CD music Off |  |
| NADESICO THE MISSION | T27901M | 1,616 | virtual VMU | — |  | — |  |
| NADESICO THE MISSION TAIKENBAN | 6107183 | 1,615 | virtual VMU | — |  | — |  |
| NASUBI NO HEYA | T14305M | 1,359 | virtual VMU | — |  | no profile: CD music Off |  |
| NBA SHOWTIME:NBA ON NBC | T-9706N | 3,100 | virtual VMU | — |  | no profile: CD music Off |  |
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
| NFL BLITZ 2000 | T-9703N | 2,465 | virtual VMU | — |  | no profile: CD music Off |  |
| NFL BLITZ 2000 | T-9703N | 2,451 | virtual VMU | — |  | no profile: CD music Off |  |
| NFL BLITZ 2000 | T-9703D-50 | 2,461 | virtual VMU | — |  | no profile: CD music Off |  |
| OUKAHOUSIN | T38704M | 2,282 | virtual VMU | — |  | — |  |
| PANZER FRONT | T2102M | 1,363 | virtual VMU | — |  | no profile: CD music Off |  |
| POCKEKANO | T19501M | 753 | virtual VMU | — |  | — |  |
| POD SPEEDZONE | T-17713N | 1,717 | virtual VMU | — |  | no profile: CD music Off |  |
| POWER STONE 2 | 6107808 | 353 | virtual VMU | — |  | — |  |
| POWER STONE 2 PAL | T36812D 50 | 3,025 | virtual VMU | — |  | — |  |
| POWER STONE 2 USA | T1211N | 3,024 | virtual VMU | — |  | — | saves and loads after a complete reboot (SD, card p0005); physical-disc virtual VMU also saves and reloads (SD virtual VMU and physical disc) |
| PRISMATICALLIZATION | T22002M | 409 | virtual VMU | — |  | no profile: CD music Off |  |
| PROYAKYU TEAM DE ASOBOU NET | HDR-0091 | 753 | virtual VMU | — |  | no profile: CD music Off |  |
| PSO | MK-51100 | 3,694 | virtual VMU | — |  | — | works; intro video slow (native) |
| PSO NET TRIAL | 6107978 | 3,480 | virtual VMU | — |  | — |  |
| PUROYAKYUU TIMU WO TUKUROU AND ASOBOU | HDR-0173 | 251 | virtual VMU | — |  | no profile: CD music Off |  |
| PUYOPUYODA FEATURING ELLENA SYSTEM | T6601M | 1,540 | virtual VMU | — |  | — |  |
| RAYBLADE | T42201M | 1,108 | virtual VMU | — |  | no profile: CD music Off |  |
| RAYMAN 2 THE GREAT ESCAPE | 17707N | 913 | virtual VMU | — |  | — | virtual VMU shown; saving and loading work; FMVs and gameplay flawless (New with virtual VMU) |
| RAYMAN 2 THE GREAT ESCAPE | 17707D | 913 | virtual VMU | — |  | — |  |
| RAYMAN THE GREAT ESCAPE | T17703M | 911 | virtual VMU | — |  | — |  |
| RECODE OF LODOSS WAR | T27903M | 3,401 | virtual VMU | — |  | no profile: CD music Off |  |
| RECODE OF LODOSS WAR | T7012D  18 | 3,392 | virtual VMU | — |  | no profile: CD music Off |  |
| RECORD OF LODOSS WAR | T40218N | 3,392 | virtual VMU | — |  | — (only short layout tracks) | virtual VMU shown; saving and loading work; FMVs and gameplay flawless (New with virtual VMU) |
| RECORD OF LODOSS WAR | T7012D  05 | 3,394 | virtual VMU | — |  | no profile: CD music Off |  |
| RECORD OF LODOSS WAR | T7012D  09 | 3,414 | virtual VMU | — |  | no profile: CD music Off |  |
| RED DOG SUPERIOR FIREPOWER | T40215N | 3,384 | virtual VMU | — |  | — |  |
| RESIDENT EVIL CODE VERONICA | T1204N | 3,823 | virtual VMU | — |  | — | saves, power-cycle reload and Disc 2 verified (Background New (X), virtual VMU) |
| RESIDENT EVIL CODE VERONICA | T36806D 05 | 3,824 | virtual VMU | — |  | — |  |
| RESIDENT EVIL CODE VERONICA | T36806D 09 | 3,824 | virtual VMU | — |  | — |  |
| RESIDENT EVIL CODE VERONICA | T36806D 18 | 3,824 | virtual VMU | — |  | — |  |
| RESIDENT EVIL CODE VERONICA | T36806D 06 | 3,824 | virtual VMU | — |  | — |  |
| RESIDENT EVIL CODE VERONICA DEMO | 8340007 50 | 3,824 | virtual VMU | — |  | — |  |
| RESIDENT EVIL3 | T7021D  72 | 151 | virtual VMU | — |  | — |  |
| REVIVE | T1301M | 1,372 | virtual VMU | — |  | — |  |
| ROOMMANIA203 | HDR-0044 | 2,124 | virtual VMU | — |  | — |  |
| ROYAL RUMBLE | T10005N | 1,323 | virtual VMU | — |  | — |  |
| ROYAL RUMBLE | T10003D 50 | 1,326 | virtual VMU | — |  | — |  |
| RUNE CASTER | T40001M | 952 | virtual VMU | — |  | — (only short layout tracks) |  |
| SEAMAN | T39401M | 2,974 | virtual VMU | — |  | — |  |
| SEAMAN | T39401M | 2,980 | virtual VMU | — |  | — |  |
| SEGA SMASH PACK VOLUME 1 | MK-51146 | 207 | virtual VMU | — |  | Smash collection route | hardware-accepted (Smash collection) |
| SEGAGT | MK-51053 | 1,721 | virtual VMU | — |  | no profile: CD music Off |  |
| SEGAGT HOMOLOGATION SPECIAL | HDR-0054 | 1,738 | virtual VMU | — |  | no profile: CD music Off |  |
| SEGAGT HOMOLOGATION SPECIAL TAIKENBAN | 6107727 | 1,727 | virtual VMU | — |  | no profile: CD music Off |  |
| SENGOKUTURB FID | T38805M | 381 | virtual VMU | — |  | no profile: CD music Off |  |
| SENTIMENTAL GRAFFITI 2 | T20104M | 845 | virtual VMU | — |  | — |  |
| SENTIMENTAL GRAFFITI2 3RDWINDOW | 6107749 | 382 | virtual VMU | — |  | — |  |
| SHADOWMAN | T-8106N | 1,574 | virtual VMU | — |  | — (only short layout tracks) |  |
| SHADOWMAN | T-8106N | 1,573 | virtual VMU | — |  | — (only short layout tracks) |  |
| SHADOWMAN | T8104D  58 | 1,575 | virtual VMU | — |  | no profile: CD music Off |  |
| SHADOWMAN | T8104D  18 | 1,573 | virtual VMU | — |  | no profile: CD music Off |  |
| SILVER | T15108N | 2,886 | virtual VMU | — |  | — | works: saves survive power-off, reboot and reload (tested before the prefetch work) (virtual VMU) |
| SILVER | T15109D | 2,885 | virtual VMU | — |  | — |  |
| SILVER | T15109D | 2,885 | virtual VMU | — |  | — |  |
| SKIES OF ARCADIA DISC 1 | MK-51052 | 5,332 | virtual VMU | — |  | — | launches every boot and reloads saves; see docs/native-skies-read-fix.md for the opening Cupil hang (virtual VMU) |
| SKIES OF ARCADIA DISC1 | MK-51052 | 4,741 | virtual VMU | — |  | — | launches every boot and reloads saves; see docs/native-skies-read-fix.md for the opening Cupil hang (virtual VMU) |
| SONIC ADVENTURE | MK-51000 | 6,592 | virtual VMU | — |  | — | works with the Background reader and virtual VMU (Background reader, virtual VMU) |
| SONIC ADVENTURE | MK-51000 | 6,577 | virtual VMU | — |  | — | works with the Background reader and virtual VMU (Background reader, virtual VMU) |
| SONIC ADVENTURE 2 | MK-51117 | 1,541 | virtual VMU | — |  | — | saves and loads; gameplay was choppy with input lag on that build (virtual VMU) |
| SONIC ADVENTURE 2 | HDR-0165 | 1,541 | virtual VMU | — |  | — |  |
| SONIC ADVENTURE 2 | MK-5111750 | 1,540 | virtual VMU | — |  | — |  |
| SONIC ADVENTURE 2 | MK-5111750 | 1,541 | virtual VMU | — |  | — |  |
| SONIC ADVENTURE TRIAL VERSION FOR E3 | MK-5100950 | 6,593 | virtual VMU | — |  | — |  |
| SPECOPS OMEGA SQUAD | T41704N | 827 | virtual VMU | — |  | no profile: CD music Off |  |
| STREET FIGHTER3 DOUBLE IMPACT | T7006D  50 | 1,183 | virtual VMU | — |  | — |  |
| STRIKER PRO 2000 | T15111N | 183 | virtual VMU | — |  | no profile: CD music Off |  |
| SUNRISE EIYUUTAN | T41101M | 1,390 | virtual VMU | — |  | — |  |
| SUPER PUZZLE FIGHTER2X FOR MATCHING SERVICE | T1250M | 4,398 | virtual VMU | — |  | — |  |
| SUPER STREET FIGHTER 2X FOR MATCHING SERVICE | T1236M | 5,062 | virtual VMU | — |  | — |  |
| SUPERMAGNETICNIUNIU | 6107386 | 2,985 | virtual VMU | — |  | — |  |
| SWORD OF THE BERSERK GUTS RAGE | T-36805N | 1,641 | virtual VMU | — |  | — | works; the game only uses a card in A1 and ignores a virtual card in A2 (virtual VMU in A1) |
| SWORD OF THE BERSERK GUTS RAGE | T36807D 05 | 1,649 | virtual VMU | — |  | — |  |
| TANTEISHINSHIDASH | T44401M | 650 | virtual VMU | — |  | — (only short layout tracks) |  |
| TECH ROMANCER | T1208N | 2,585 | virtual VMU | — |  | — |  |
| TECH ROMANCER | T7009D50 | 2,593 | virtual VMU | — |  | — |  |
| TEST DRIVE 6 | T15102N | 1,056 | virtual VMU | — |  | — (only short layout tracks) |  |
| TETSUMAN MENKYOKAIDEN | T18701M | 382 | virtual VMU | — |  | no profile: CD music Off |  |
| THE HOUSE OF THE DEAD 2 | MK-51002 | 793 | virtual VMU (fixed arena) | — |  | — | saves and reloads after power-off; one freeze at the first boss (Original mode, VMU On) and minor lag; VMU Off did not crash (Background, virtual VMU (fixed arena)); known limit: residual lag; bounded scheduling work (integration roadmap section 3; not owner-confirmed) |
| THE KING OF FIGHTERS 99 EVOLUTION | T3102M | 3,954 | virtual VMU | — |  | — |  |
| THE KING OF FIGHTERS DREAM MATCH 1999 | T3101N | 2,314 | virtual VMU | — |  | no profile: CD music Off |  |
| THE KING OF FIGHTERS DREAM MATCH 1999 | T3101M | 2,312 | virtual VMU | — |  | no profile: CD music Off |  |
| THE KING OF FIGHTERS EVOLUTION | T44302N | 3,952 | virtual VMU | — |  | — |  |
| THE RING | T15122N | 1,713 | virtual VMU | — |  | — |  |
| THE RING | T27902M | 1,586 | virtual VMU | — |  | — |  |
| THIS IS A SAMPLE DISK FOR USE IN THE OFFICE ONLY  DO NOT SELL   DO NOT CARRY OUT FROM THE OFFICE | 6107158 | 1,023 | virtual VMU | — |  | — |  |
| TIME STALKERS | MK-51011 | 645 | virtual VMU | — |  | — | boots, saves and reloads (Background New (X), virtual VMU in A1) |
| TIME STALKERS | MK-5101153 | 653 | virtual VMU | — |  | — |  |
| TOY COMMANDER | MK-57020 | 730 | virtual VMU | — |  | native profile (Toy Commander) | sees imported saves, creates new saves and loads them after reboot; CD music works; intro movies still stall (virtual VMU; native CDDA) |
| TOY COMMANDER | MK-5102050 | 730 | virtual VMU | — |  | no profile: CD music Off |  |
| TOY RANGER | HDR-0052 | 754 | virtual VMU | — |  | no profile: CD music Off |  |
| TRICOLORE CRISE ART PALETTE | 6107933 | 754 | virtual VMU | — |  | — |  |
| WINNING POST 4 PROGRAM2000 FOR DREAMCAST | T7606M | 1,946 | virtual VMU | — |  | no profile: CD music Off |  |
| WWF ROYAL RUMBLE | T45101M | 1,328 | virtual VMU | — |  | — |  |
| ZUSAR VASAR | T43301M | 1,677 | virtual VMU | — |  | no profile: CD music Off |  |
| ZUSAR VASAR | 6107846 | 1,454 | virtual VMU | — |  | no profile: CD music Off |  |
| 102 DALMATIANS | T36803N | 711 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| 102 DALMATIANS | T36813D 05 | 716 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| 102 DALMATIANS | T36813D 50 | 718 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| 21-TWOONE- | T46503M | 869 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| 4WHEELTHUNDER | T-9708N | 3,576 | refused | New, no virtual VMU | executable extent not proved | no profile: CD music Off |  |
| 4WHEELTHUNDER | T-9706D-50 | 3,576 | refused | New, no virtual VMU | executable extent not proved | no profile: CD music Off |  |
| 90MINUTES -SEGA CHAMPIONSHIP FOOTBALL- | MK-5119050 | 727 | refused | New, no virtual VMU | executable extent not proved | — |  |
| ADVANCED DAISENRYAKU | HDR-0066 | 2,622 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| ADVANCED DAISENRYAKU 2001 | HDR-0158 | 3,541 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| AERO DANCING I | T6807M | 7,023 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| AERO DANCING I/JIKAI-SAKU MADE MATEMASEN | T6809M | 7,084 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| AERO TO JET DE DANCING | 6108017 | 608 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| AERODANCING F | T6804M | 2,959 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| AERODANCING F | T6804M | 2,960 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| AERODANCING F TRIAL | 6107740 | 2,955 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| AERODANCING F/TODOROKI TSUBASA NO HATSU-HIKO | T6805M | 7,035 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| AEROWINGS | T40201N | 4,078 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| AEROWINGS | T40202D 50 | 4,079 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| AEROWINGS 2 AIR STRIKE | T40210N | 2,887 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| AEROWINGS 2 AIR STRIKE | T40209D 50 | 2,888 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| AFTER | T18706M | 860 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| AIKAGI | T20130M | 922 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| AIR | T20112M | 727 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| ALIENFRONT ONLINE | MK-51171 | 2,253 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | no profile: CD music Off |  |
| ALONE IN THE DARK - THE NEW NIGHTMARE | T15117N | 801 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| ALONE IN THE DARK 4 - THE NEW NIGHTMARE | T15117N | 801 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| ALONE IN THE DARK 4 - THE NEW NIGHTMARE | T15117N | 801 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| ANGEL WISH | T18712M | 883 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| ANGELPRESENT | T20107M | 1,049 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| ANIMASTAR | T34101M | 1,251 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| AQUA GT | T40509D 50 | 503 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| ARMY MEN SARGES HEROES | T-9715N | 1,977 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| ARMY MEN SARGES HEROES | T9708D  50 | 1,571 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| ARMY MEN SARGES HEROES | T-9715N | 1,988 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| ATSUMARE GURU GURU ONSEN | HDR-0015 | 4,836 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| ATSUMARE GURU GURU ONSEN BB | HDR-0121 | 4,817 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| ATSUMARE GURU GURU ONSEN BB | HDR-0121 | 4,818 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| ATSUMARE GURU GURU ONSEN TAIKENBAN | 6107187 | 4,701 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| ATSUMARE GURU GURU ONSEN TAIKENBAN | 6107187 | 4,697 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| BALDR FORCE EXE | T47110M | 1,394 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| BASS RUSH DREAM | T5801M | 1,868 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| BERSERK TRIAL VERSION R | 6107370 | 1,581 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| BIOHAZARD CODE VERONICA PLUS | T1240M | 3,934 | refused | New, no virtual VMU | executable extent not proved | — |  |
| BIOHAZARD3 | T1222M | 2,035 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| BITTERSWEET FOOLS | T47401M | 655 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| BLUE6 -TIME AND TIDE- | HDR-0075 | 1,561 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| BOKOMU NO TATSUJIN | T42904M | 1,369 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| BOKU DORAEMON | T41802M | 955 | refused | New, no virtual VMU | terminal exits not resolved | — (only short layout tracks) |  |
| BOKU NO TENNIS JINSEI | T41202M | 1,457 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| BOKU TO BOKURA NO NATU | T19717M | 1,281 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| BOMBERMAN ONLINE | MK-51065 | 2,483 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| BORDER DOWN | T46703M | 1,556 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| BOUNTY HUNTER SARA SCENARIO1 | T1243M | 618 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| BROADBAND PASSPORT | 6107883 | 3,578 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| BUGGYHEAT | MK-5104050 | 4,499 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| BUGGYHEAT | T6801M | 4,341 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| BUSNAVI | T35404M | 465 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| BUZZ 2000 | T13007N | 962 | refused | New, no virtual VMU | 284 KiB lease does not fit | — (only short layout tracks) |  |
| BUZZ 2000 | T13005D 05 | 969 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| BUZZ 2000 | T13005D 18 | 968 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| BUZZ 2000 | T13005D 09 | 970 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| CAFE LITTLEWISH | T46513M | 497 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| CANARY | T20111M | 712 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| CANDY STRIPE | HDR-0172 | 3,246 | refused | New, no virtual VMU | executable extent not proved | — |  |
| CANNON SPIKE | T1215N | 619 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| CANNON SPIKE | T46601D 05 | 623 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| CANVAS | T20108M | 797 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| CAPCOM TAISEN FANDISC | 6108086 | 482 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| CAPCOM VS SNK | T1218N | 2,347 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| CAPCOM VS SNK | T1217M | 2,332 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| CAPCOM VS SNK | T1217M | 2,334 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| CAPCOM VS SNK | T7017D  50 | 2,348 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| CAPCOM VS SNK  DEMO VERSION | 6107917 | 2,324 | refused | New, no virtual VMU | terminal exits overlap hook windows | — |  |
| CAPCOM VS SNK PRO | T1247M | 2,304 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| CAPCOM VS SNK2 | T1249M | 2,331 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| CARDCAPTOR SAKURA TOMOYO NO VIDEO DAISAKUSEN | HDR-0115 | 1,003 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| CARDOFDESTINY | T44406M | 817 | refused | New, no virtual VMU | Maple driver family not resolved | — (only short layout tracks) |  |
| CARRIER | T5701N | 11,611 | refused | New, no virtual VMU | executable extent not proved | — |  |
| CARRIER | T5701M | 11,775 | refused | New, no virtual VMU | executable extent not proved | — |  |
| CARRIER | T44901D 50 | 11,625 | refused | New, no virtual VMU | executable extent not proved | — |  |
| CENTIPEDE | T40602N | 3,254 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| CHAOS FIELD | T47801M | 1,125 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| CHARGEN BLAST | T15127N | 2,478 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| CHARGEN BLAST | T4402M | 2,472 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| CHARGEN BLAST | T44902D 50 | 2,477 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| CHICKEN RUN | T36811N | 922 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| CHICKEN RUN | T36814D 87 | 923 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| CHICKEN RUN | T36814D 05 | 923 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| CHOCOLAT - MAID CAFE CURIO - | T47107M | 2,218 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| CHOCOLAT - MAID CAFE CURIO - | 6108395 | 1,328 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| CHOUHATSUMEIBOY KANIPAN BOUSOUROBOTO NO NAZO | HDR-0021 | 1,386 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| CHRISTMAS SEAMAN MESSAGEKIT | T39403M | 2,112 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| CLEOPATRA FORTUNE | T16603M | 545 | refused | New, no virtual VMU | Maple driver family not resolved | no profile: CD music Off |  |
| CLIMAX LANDERS | HDR-0009 | 643 | refused | New, no virtual VMU | terminal exits overlap hook windows | — |  |
| CLOSE TO | T19704M | 1,045 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| COJI COJI | T40802M | 1,272 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| COMIC PARTY | T38301M | 1,129 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| COMIC PARTY | T38301M | 1,129 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| COMIC PARTY | T38301M | 1,155 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| CONFIDENTIAL MISSION | MK-51160 | 1,744 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| CONFIDENTIAL MISSION | HDR-0160 | 1,745 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| CONFIDENTIAL MISSION | MK-5116050 | 1,745 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| CONFLICT ZONE | T17721N | 2,400 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| CONFLICT ZONE | T46604D | 2,413 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| COOL BOARDERS BURRRN | T36901M | 1,328 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| COOL BOARDERS BURRRN | 6107174 | 1,203 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| COOL COOL TOON | T3106M | 2,801 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| COOL COOL TOON DEMO | 6107877 | 2,799 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| COSMIC SMASH | HDR-0176 | 912 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| CRAZY TAXI | MK-51035 | 1,433 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | — | works without virtual VMU (standalone New); known limit: needs a reviewed retained arena (integration roadmap section 3; not owner-confirmed) |
| CRAZY TAXI | HDR-0053 | 1,433 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | — |  |
| CRAZY TAXI | MK-51035 | 1,442 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | — | works without virtual VMU (standalone New); known limit: needs a reviewed retained arena (integration roadmap section 3; not owner-confirmed) |
| CRAZY TAXI 2 | HDR-0159 | 1,418 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | — |  |
| CRAZY TAXI 2 | MK-5113650 | 1,425 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | — |  |
| CRAZY TAXI 2 DEMO | 6108041 | 1,375 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | — |  |
| CULDCEPT SECOND | T43903M | 3,188 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| CYBER TROOPERS VIRTUAL-ON ORATORIO TANGRAM | T13004N | 1,595 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| CYBER TROOPERS VIRTUAL-ON ORATORIO TANGRAM | HDR-0040 | 2,291 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| DABITSUKU | HDR-0084 | 3,943 | refused | New, no virtual VMU | executable extent not proved | — |  |
| DABITSUKU TKBFT | 6107875 | 3,768 | refused | New, no virtual VMU | executable extent not proved | — |  |
| DABITSUKU2 | HDR-0167 | 4,605 | refused | New, no virtual VMU | executable extent not proved | — |  |
| DANCE DANCE REVOLUTION 2NDMIX DREAMCAST EDITION | T9506M | 1,373 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| DAVE MIRRA FREESTYLE BMX | T-8120N | 3,795 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| DAVE MIRRA FREESTYLE BMX | T8120D  50 | 3,753 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| DAYTONA USA 2001 | HDR-0106 | 1,955 | refused | New, no virtual VMU | executable extent not proved | — |  |
| DAYTONA USA 2001 TRIAL | 6107967 | 1,658 | refused | New, no virtual VMU | executable extent not proved | — |  |
| DAYTONAUSA | MK-51037 | 1,963 | refused | New, no virtual VMU | executable extent not proved | — |  |
| DAYTONAUSA2001 | MK-5103750 | 1,970 | refused | New, no virtual VMU | executable extent not proved | — |  |
| DDR CLUB VER 1000 | T9508M | 1,421 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| DEAD OR ALIVE 2 | T3601N | 1,773 | refused | New, no virtual VMU | SDK heap family not resolved | — | works with virtual VMU; Background loader plus prefetch did not fit in RAM, so it runs with the Background loader only (Background loader with virtual VMU, no prefetch) |
| DEAD OR ALIVE 2 | T3601M | 1,884 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | — |  |
| DEAD OR ALIVE 2 | T3602M | 1,883 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | — |  |
| DEAD OR ALIVE 2 | T8116D  50 | 1,789 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| DEAD OR ALIVE 2(DEMONSTRATION VER.) | T3601N | 1,673 | refused | New, no virtual VMU | SDK heap family not resolved | — | works with virtual VMU; Background loader plus prefetch did not fit in RAM, so it runs with the Background loader only (Background loader with virtual VMU, no prefetch) |
| DEAD OR ALIVE2 TENTO DEMO | 6107913 | 660 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| DEATH CRIMSON OX | T2401N | 9,151 | refused | New, no virtual VMU | executable extent not proved | — |  |
| DEATH CRIMSON OX | T23202M | 9,141 | refused | New, no virtual VMU | executable extent not proved | — |  |
| DEATHCRIMSON2 | T23201M | 1,310 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| DEEP FIGHTER | T17705N | 1,812 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | no profile: CD music Off |  |
| DEEP FIGHTER | T17704D 05 | 1,812 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | no profile: CD music Off |  |
| DEEP FIGHTER | T17704D 09 | 1,813 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | no profile: CD music Off |  |
| DEEP FIGHTER | T17704D 18 | 1,812 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | no profile: CD music Off |  |
| DESPIRIA | T14402M | 1,449 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| DIGI CHARAT FANTASY | T46302M | 674 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| DINO CRISIS | T7019D  05 | 1,696 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| DISNEY S DONALD DUCK GOIN QUACKER | T17714D50 | 1,122 | refused | New, no virtual VMU | terminal exits overlap hook windows | no profile: CD music Off |  |
| DOGU SENKI -HAOH- | T9101M | 1,457 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| DOKIDOKI IDOL STARSEEKER REMIX | T46701M | 786 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| DOUSOUKAI2 | T20113M | 551 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| DP3SAKURA | 6107996 | 3,428 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| DRACONUS CULT OF THE WYRM | T40203N | 2,284 | refused | New, no virtual VMU | terminal exits not resolved | — (only short layout tracks) |  |
| DRAGONRIDERS - THE CHRONICLES OF PERN | T17720N | 1,113 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| DRAGONRIDERS - THE CHRONICLES OF PERN | T17716D 50 | 1,112 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| DRAGONS_BLOOD | T12503D-50 | 2,281 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| DREAM PASSPORT2 | 6107180 | 3,515 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| DREAM PASSPORT2 | 6107180 | 3,569 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| DREAM PASSPORT3 | 6107819 | 3,408 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| DREAM PREVIEW VOL7 | 610702707 | 531 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| DREAMCAST EXPRESS EXTRA | 6107709 | 551 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| DREAMCAST EXPRESS VOL3 | 610711003 | 533 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| DREAMCAST EXPRESS VOL5 | 6107110 05 | 551 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| DREAMCAST EXPRESS VOL5 | 6107110 05 | 549 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| DREAMCAST EXPRESS VOL6 | 6107110 06 | 607 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| DREAMCAST EXPRESS VOL7 | 6107110 07 | 625 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| DREAMCAST VM DATA ISSUE | 6108094 | 641 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| DREAMCASTMAGAZINE GD VOL.2-2 | 6107164 03 | 3,220 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| DREAMCASTMAGAZINE GD VOL11 | 6107164 12 | 608 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| DREAMCASTMAGAZINE GD VOL2 1 | 610716402 | 495 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| DREAMCASTMAGAZINE GD VOL3 | 6107164 03 | 552 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| DREAMCASTMAGAZINE GD VOL5 | 6107164 06 | 607 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| DREAMCASTMAGAZINE GD VOL7 | 6107164 08 | 719 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| DREAMFLYER | HDR-0042 | 1,106 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| DREAMKEY | 832-000205 | 2,779 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| DREAMKEY | 832-000205 | 2,779 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| DREAMKEY 1.5 ENGLISH V.1.003 | 8320022 05 | 2,780 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| DREAMKEY INDIA | 6107940 | 2,449 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| DREAMKEY OZ | 610-7719 | 2,780 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| DREAMKEY2 C | 832004646 | 2,767 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| DREAMKEY3 | 8320068 50 | 2,762 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| DREAMON 1 | 8320002 50 | 974 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| DREAMON 11 | 8320032 50 | 1,023 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| DREAMON 12 | 8320035 50 | 1,025 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| DREAMON 2 | 8320001 50 | 974 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| DREAMON 5 | 8320012 50 | 1,021 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| DREAMON 7&8 | 8320025 50 | 1,022 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| DREAMON 9 | 8320023 50 | 1,023 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| DREAMON HW BUNDLE 3 | 8320026 50 | 1,025 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| DREAMON HW2 | 8320021 50 | 1,022 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| DREAMON10 | 8320028 50 | 1,023 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| DREAMPASSPORT PREMIER | 6108061 | 3,250 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| DREAMPASSPORT3 URBAN STYLE | 6107894 | 3,428 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| DREAMSTUDIO | HDR-0114 | 1,802 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| DVINE LUV | T46501M | 1,031 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| ECCO THE DOLPHIN DEFENDER OF THE FUTURE | MK-51033 | 2,906 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | no profile: CD music Off |  |
| ECCO THE DOLPHIN DEFENDER OF THE FUTURE | HDR-0103 | 2,925 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | no profile: CD music Off |  |
| ECCO THE DOLPHIN DEFENDER OF THE FUTURE | MK-5103350 | 2,899 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | no profile: CD music Off |  |
| ECW ANARCHY RULZ | T-8114N | 4,366 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| ECW ANARCHY RULZ | T8119D  50 | 4,367 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| ECW HARDCORE REVOLUTION | T-8112N | 2,663 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| ECW HARDCORE REVOLUTION | T-8111D-50 | 2,663 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| ECW HARDCORE REVOLUTION | T-8111D-58 | 2,661 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| ELDORADO GATE VOL1 | T1223M | 1,230 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| ELDORADO GATE VOL2 | T1224M | 1,275 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| ELDORADO GATE VOL3 | T1225M | 1,337 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| ELDORADO GATE VOL4 | T1226M | 1,411 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| ELDORADO GATE VOL5 | T1227M | 1,880 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| ELDORADO GATE VOL6 | T1228M | 2,005 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| ELDORADO GATE VOL7 | T1229M | 1,662 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| ELYSION | T20116M | 855 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| EOS | T22903D 50 | 1,005 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| ERDE | T19720M | 1,051 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| ES | T45501M | 1,053 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| ESL | T7015D  50 | 3,737 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| ESPION-AGE-NTS | T38804M | 1,285 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| ESPION-AGE-NTS | 6107185 | 392 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| EVANGELION AYANAMI IKUSEI KEIKAKU | T46303M | 2,243 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| EVE ZERO | T39803M | 1,641 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| EVER17 -THE OUT OF INFINITY- | T19716M | 1,329 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| EVER17PE | T19723M | 1,327 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| EVER17PE | T19723M | 683 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| EVIL DEAD HAIL TO THE KING | T10003N | 8,450 | refused | New, no virtual VMU | 284 KiB lease does not fit | — (only short layout tracks) |  |
| EVILDEAD HAIL TO THE KING | T10005D 05 | 8,450 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| EVILTWIN | T46605D 71 | 1,977 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| EVILTWIN | T46605D 80 | 1,977 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| EXODUSGUILTYNEOS | T44404M | 834 | refused | New, no virtual VMU | Maple driver family not resolved | — (only short layout tracks) |  |
| EXPENDABLE | T15104N | 4,820 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| EXPENDABLE | T15003M | 4,816 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| F1 RACING CHAMPIONSHIP | 17706D-50 | 2,016 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| F1 WORLD GRAND PRIX FOR DREAMCAST | T3001M | 1,008 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| F1 WORLD GRAND PRIX FOR DREAMCAST | T3001M | 1,014 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| FATAL FURY MARK OF THE WOLVES | T44306N | 1,491 | refused | New, no virtual VMU | Maple driver family not resolved | no profile: CD music Off |  |
| FIGHTER OF ZERO | T43501M | 946 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| FIGHTING VIPERS2 | HDR-0133 | 1,333 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| FIGHTING VIPERS2 | MK-5115450 | 1,333 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| FIRE PROWRESTLING D | T18805M | 1,614 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| FIRST KISS STORY | T46304M | 638 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| FIRST KISS STORY 2 | T46305M | 1,362 | refused | New, no virtual VMU | executable extent not proved | — |  |
| FISHEYESWILD | T9105M | 1,774 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| FRAME GRIDE | T34201M | 1,727 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| FRAME GRIDE | 6107123 | 1,101 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| FROGGER 2 | T40604N | 818 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| FUR FIGHTERS | T-8107N | 1,506 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| FUR FIGHTERS | 8D-0008-50 | 1,424 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| FUR FIGHTERS | T-8113D-50 | 1,506 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| FUREAI | T47402M | 660 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| FUSIGINODANZYON FUURAINOSIRENGAIDEN ONNAKENSIASUKAKENZAN | HDR-0187 | 1,896 | refused | New, no virtual VMU | heap initializer call not proved | — (only short layout tracks) |  |
| GAIA MASTER | T1212M | 1,680 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| GAROU MARK OF THE WOLVES | T3108M | 1,491 | refused | New, no virtual VMU | Maple driver family not resolved | no profile: CD music Off |  |
| GAUNTLET LEGENDS | T9710N | 3,673 | refused | New, no virtual VMU | 284 KiB lease does not fit | — (only short layout tracks) |  |
| GAUNTLET LEGENDS | T9707D  50 | 3,553 | refused | New, no virtual VMU | 284 KiB lease does not fit | — (only short layout tracks) |  |
| GEKKA NO KENSHI2 FINAL EDITION | T3107M | 1,421 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| GENERATOR 2 | MK-51057 | 159 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off | known limit: demo disc: its menu starts several separate game programs (native and Windows CE); the checks cover the menu only |
| GETBASS2 | MK-51166 | 2,248 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| GETBASS2 | HDR-0169 | 2,256 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| GIANTGRAM2000 | HDR-0101 | 4,635 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| GIGA WING | T7008D  50 | 1,514 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| GIGA WING2 | T1222N | 1,401 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| GIGA WING2 | T1239M | 1,402 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| GIGA WING2 TAIKEN BAN | 6107992 | 1,399 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| GIGAWING | T1209N | 1,503 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| GIGAWING | T1208M | 1,501 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| GIGAWING | 6107358 | 1,500 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| GOLF SHIYOUYO | T40901M | 1,291 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| GOLF SHIYOUYO - COURSES DATA - ADVENTURE EDITION | T40904M | 1,314 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| GOLF SHIYOUYO 2 | T44501M | 2,265 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| GOLF_SHIYOUYO | T40901M | 1,290 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| GRAND THEFT AUTO 2 | T42102N | 4,394 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| GRAND THEFT AUTO 2 | T40502D 93 | 4,446 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| GRAND THEFT AUTO 2 | T40502D 09 | 4,447 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| GRAUEN VOL.1 | HDR-0033 | 506 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| GRAUEN VOL.2 | HDR-0034 | 510 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| GRAUEN VOL.6 | HDR-0038 | 606 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| GUILTY GEAR X | T-2402M | 1,445 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| GUILTY GEAR X SAMPLE | 6107937 | 1,386 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| GUNBIRD2 | T1214N | 1,122 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| GUNBIRD2 | T1213M | 1,090 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| GUNBIRD2 | T7018D  50 | 1,090 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| GUNDAM BATTLE ONLINE | T13304M | 1,009 | refused | New, no virtual VMU | terminal exits not resolved | — (only short layout tracks) |  |
| GUNDAM BATTLE ONLINE TRIAL | 6108039 | 982 | refused | New, no virtual VMU | terminal exits not resolved | — (only short layout tracks) |  |
| GUNSPIKE | T1219M | 625 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| GUNSPIKE | 6107982 | 623 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| GURU GURU ONSEN 3 | HDR-0195 | 2,755 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| GURUGURU ONSEN 2 | HDR-0168 | 2,695 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| HANAGUMI TAISEN COLUMNS 2 | HDR-0046 | 2,245 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| HANIHANI | T47109M | 728 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| HAPPY BREEDING | T46512M | 499 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| HARUSAME YOUBI | T20106M | 747 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| HEADHUNTER - SEGA - AMUZE - 2001 | MK-5104150 | 1,508 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| HEAVY METAL GEOMATRIX | T1223N | 1,199 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | — |  |
| HEAVY METAL GEOMATRIX | T1246M | 1,200 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | — |  |
| HEAVY METAL GEOMATRIX | T46602D 50 | 1,201 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | — |  |
| HEISEI MAJANSOU | T2202M | 1,764 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| HELLO KITTY DREAM PASSPORT2 | 6107349 | 3,568 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| HIMITSU YUIGAITANATSU | T46101M | 568 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| HYDRO THUNDER | T-9702N | 1,844 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| I - SPY | T41302N | 1,289 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| IDOL JANSHI WO TSUKUTTYAOU | T5703M | 971 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| IKARUGA | T38706M | 1,003 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| ILLBLEED | T46001N | 1,357 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| ILLBLEED | T45601M 11 | 1,358 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| INCOMING | T12503N | 11,185 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| INCOMING | T40701D-50 | 11,180 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| INTERLUDE | T20117M | 619 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| IRIS | T19722M | 1,115 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| IRON ACES | T15129N | 988 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| IZUMO | T46902M | 1,634 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| J-LEAGUE SPECTACLE SOCCER | HDR-0154 | 735 | refused | New, no virtual VMU | executable extent not proved | — |  |
| JEREMY MCGRATH SUPERCROSS 2000 | T-8104N | 1,297 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| JEREMY MCGRATH SUPERCROSS 2000 | T8114D  50 | 1,335 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| JET SET RADIO | MK-5105850 | 333 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| JISSEN PACHISLOT HISSYOUHOU VPACHI KONGDOM | T18802M | 736 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| JLEAGUE PRO SOCCER CLUB MAKER | HDR-0028 | 1,401 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| JLEAGUE PRO SOCCER CLUB MAKER | HDR-0028 | 1,401 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| JOJO | T1231M | 443 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| JOJO | T1205M | 234 | refused | New, no virtual VMU | terminal exits overlap hook windows | — |  |
| JOJO | 6107357 | 226 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| JRA PAT FOR DREAMCAST | T42501M | 1,409 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| JRAPAT FOR DREAMCAST | T42501M | 1,383 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| JRAPAT FOR DREAMCAST | T42501M | 1,386 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| JRAPAT FOR DREAMCAST V50 | HDR-0204 | 1,534 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| KAENSEIBO | T42802M | 910 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| KANON | T20105M | 818 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| KAO THE KANGAROO | T22903N | 820 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| KAO THE KANGAROO | T22902D 50 | 821 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
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
| LOONEY TUNES SPACE RACE | T15116N | 105 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| LOVEHINA2 | HDR-0139 | 1,136 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| MACROSS M3 | T21501M | 2,087 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| MAGFORCE RACING | T40208N | 523 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| MAGFORCE RACING | T40207D 50 | 524 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| MAGIC THE GATHERING | HDR-0116 | 2,501 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| MAJO NO OCHAKAI | T20124M | 940 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| MAPLE | T46510M | 537 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| MARGE | T46514M | 500 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| MARIONETTE HANDLER | T2201M | 1,166 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| MARIONETTE HANDLER2 | T2204M | 1,611 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| MARS MATRIX | T1221N | 2,336 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| MARS MATRIX | T1238M | 2,338 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| MARVEL VS CAPCOM 2 | T1212N | 1,767 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| MARVEL VS CAPCOM 2 | T7010D  50 | 1,769 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| MARVEL VS. CAPCOM 2 | T1215M | 1,923 | refused | New, no virtual VMU | terminal exits overlap hook windows | — |  |
| MARVEL VS. CAPCOM 2 TENTOU | 6107807 | 2,540 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| MAT HOFFMANS PRO BMX | T13005N | 6,812 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| MAX STEEL COV | T41402N | 2,542 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| MDK2 | T12502N | 733 | refused | New, no virtual VMU | Maple driver family not resolved | native profile (MDK2) |  |
| MDK2 | T12501D 50 | 733 | refused | New, no virtual VMU | Maple driver family not resolved | no profile: CD music Off |  |
| MEMORIES OFF 2ND | T19707M | 1,186 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| MEMORIES OFF 2ND -MAKING DISC- | T19708M | 439 | refused | New, no virtual VMU | executable extent not proved | no profile: CD music Off |  |
| MEMORIES OFF 2ND -PROMOTION DISC- | T19705M | 1,125 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| METALWOLF | T46507M | 1,014 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| METROPOLIS STREET RACER | MK-51012 | 488 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| MIDDLEWARE CONFERENCE2 | 610SYKN002 | 206 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| MILKY SEASON | T19713M | 1,232 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| MILLENNIUM SOLDIER | T15101D-50 | 4,820 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| MISSING PARTS 2 THE TANTEI STORIES | T36703M | 627 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| MISSING PARTS 3 THE TANTEI STORIES | T36704M | 631 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| MISSING PARTS THE TANTEI STORIES | T36702M | 615 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| MIZUIRO | T20114M | 656 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| MOBILE SUIT GUNDAM THE EARTH FEDERATION VS. THE PRINCIPALITY OF ZEON AND DX | T13306M | 2,432 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| MOBILE SUIT GUNDAM THE EARTH FEDERATION VS. THE PRINCIPALITY OF ZEON AND DX | T13306M | 3,053 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| MOEKAN | T46515M | 541 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| MOERO JUSTICE GAKUEN | T1221M | 3,853 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| MOERO JUSTICE GAKUEN TRIAL VER. | 6107970 | 3,827 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| MOHO | T40508D 50 | 1,117 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| MOMOTENSHI2 DISC1 | T9505M | 427 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| MONACO GRAND PRIX | 17701N | 1,366 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| MONACO GRAND PRIX RACING SIMULATION 2 | 17701D | 1,367 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| MONACO GRAND PRIX RACING SIMULATION 2 ON-LINE | T-45006D50 | 2,043 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| MORTAL KOMBAT GOLD | T-9701N | 6,142 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| MORTAL KOMBAT GOLD | T-9701N | 6,178 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| MORTAL KOMBAT GOLD | T9701D | 6,179 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| MOTTO PROYAKYU TEAM WO TUKUROU | HDR-0092 | 736 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| MS. PAC-MAN MAZE MADNESS | T1404N | 1,343 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| MTV SPORTS SKATEBOARDING | T10004N | 901 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| MTV SPORTS SKATEBOARDING | T10001D 50 | 900 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| MUSAPEY NO CHOCOMARKER | T23203M | 9,529 | refused | New, no virtual VMU | executable extent not proved | — |  |
| MY BROWSER DI GI CHARAT | T44601M | 871 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| MY MERRY MAY | T19714M | 1,334 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| MY MERRY MAYBE | T19719M | 1,040 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| NAKORURU | T44702M | 808 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| NAMCO MUSEUM | T1403N | 1,657 | refused | New, no virtual VMU | 284 KiB lease does not fit | — (only short layout tracks) |  |
| NANATSU NO HIKAN SENRITSU NO BISYOU | T7604M | 1,463 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| NAPPLETALE ARSIA IN DAYDREAM | HDR-0079 | 1,763 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | — |  |
| NATSUIROCELEBRATION | T47404M | 666 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| NBA 2K | MK-51004 | 3,231 | refused | New, no virtual VMU | 284 KiB lease does not fit | — (only short layout tracks) |  |
| NBA 2K | MK-5100450 | 3,232 | refused | New, no virtual VMU | 284 KiB lease does not fit | — (only short layout tracks) |  |
| NBA 2K1 | HDR-0150 | 3,365 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| NBA HOOPZ | T9709N | 11,361 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| NBA HOOPZ | T9713D | 11,409 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| NBA2K2 | MK-51178 | 3,231 | refused | New, no virtual VMU | 284 KiB lease does not fit | — (only short layout tracks) |  |
| NCAA COLLEGE FOOTBALL 2K2 | MK-51176 | 3,231 | refused | New, no virtual VMU | 284 KiB lease does not fit | — (only short layout tracks) |  |
| NEO GOLDEN LOGRES | T9904M | 493 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| NET DE TENNIS | T1234M | 1,677 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| NET GIMMICK | T1248M | 1,649 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| NEVER7 THE END OF INFINITY | T19703M | 1,035 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| NFL 2K | MK-51003 | 3,231 | refused | New, no virtual VMU | 284 KiB lease does not fit | — (only short layout tracks) |  |
| NFL 2K | HDR-0058 | 3,231 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| NFL 2K1 | HDR-0144 | 3,365 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| NFL BLITZ 2001 | T9712N | 1,909 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| NHL 2K | MK-51025 | 1,419 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| NHL 2K | MK-5102550 | 1,424 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| NHL2K2 | MK-51182 | 2,631 | refused | New, no virtual VMU | heap initializer call not proved | — (only short layout tracks) |  |
| NHL2K2 | MK-51182 | 2,631 | refused | New, no virtual VMU | heap initializer call not proved | — (only short layout tracks) |  |
| NIJYUEI | T46505M | 835 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| NOBUNAGA NO YABOU REPPUUDEN | T7605M | 2,748 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| OFFICIAL SEGA DREAMCAST MAGAZINE JANUARY 2000 | IGN2 | 159 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| OHGAMI ICHIRO FUNTOUKI | HDR-0057 | 1,110 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | — |  |
| OKAERI | T47403M | 696 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| OMIKRON THE NOMAD SOUL | T36807N | 1,038 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| OMOIDE NI KAWARU KIMI | T19718M | 1,362 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| ORANGEPOCKET | T18708M | 803 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| PACHI-SLOT TEIO DREAM SLOT HEIWA SP | T20501M | 1,033 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| PACHI-SLOT TEIO DREAM SLOT OLYMPIA SP | T20502M | 1,032 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| PACHINKO NO DENDOU CR NANASY | T5205M | 722 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| PACHITTECHONMAGE | T45801M | 1,548 | refused | New, no virtual VMU | Maple driver family not resolved | — (only short layout tracks) |  |
| PANDORA NO YUME | T20120M | 1,174 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| PATISSIERNANYANKO | T18710M | 1,301 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| PENPEN | T15103D 50 | 2,233 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| PENPEN TRIICELON | T15105N | 2,234 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| PERPETUAL BLUE | T27801M | 926 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| PHANTASY STAR ONLINE TENTOU DEMO | 6107990 | 379 | refused | New, no virtual VMU | Maple driver family not resolved | — (only short layout tracks) |  |
| PIZZICATO POLKA | T19724M | 1,755 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| PLANET RING | MK-5112550 | 2,808 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| PLANETWEB INTERNET BROWSER V3.0 | T31901N | 4,328 | refused | New, no virtual VMU | executable extent not proved | — |  |
| POD 2 | T-17710D50 | 1,682 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| POPN MUSIC 2 | T9590M | 648 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| POPN MUSIC 3 APPEND DISC | T9591M | 154 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| POPN MUSIC 4 APPEND DISC | T9592M | 155 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| POWER JET RACING 2001 | T6806M | 2,454 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| POWER SMASH | HDR-0113 | 2,515 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| POWER SMASH (SHOP TRIAL VERSION) | 6107958 | 2,505 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| POWER SMASH 2 | HDR-0177 | 2,700 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| POWER SMASH 2 TRIAL VERSION | 610-8092 | 2,489 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| POWER SMASH DDIRECT VERSION | 6107973 | 2,508 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| POWER STONE | T1201N | 3,334 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| POWER STONE | T36801D 50 | 3,335 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| POWER STONE | T1201N | 3,334 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| POWER STONE 2 | T1218M | 2,901 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| PRINCESS HOLIDAY | T47106M | 976 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| PRISM HEART | T19711M | 852 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| PRISM HEART | T19712M | 849 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| PRO MAHJONG KIWAME D | T16801M | 515 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| PRO PINBALL TRILOGY | T30701D 50 | 395 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| PROJECT JUSTICE | T1219N | 2,941 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| PROJECT JUSTICE | T7022D  50 | 2,943 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| PSO | HDR-0129 | 3,882 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| PSO | MK-5110050 | 3,694 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| PSO VER.2 | MK-51193 | 153 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| PSO VER.2 | HDR-0163 | 153 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| PSYVARIAR2 THE WILL TO FABRICATE | T9907M | 2,031 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| PUYOPUYO FEVER | HDR-0216 | 1,302 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| QUIZ AH MY GODDESS | HDR-0105 | 1,109 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| QUIZ AH MY GODDESS SPECIAL EXTRA GD-ROM | HDR-0123 | 432 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| R7 | 6107726 | 3,260 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| RADIRGY | T47802M | 1,208 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| RAINBOW COTTON | T9901M | 564 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| RAYBLADE TRIAL1 | 6107747 | 922 | refused | New, no virtual VMU | terminal exits not resolved | — (only short layout tracks) |  |
| RAYMAN 2 - THE GREAT ESCAPE | 17707D | 3,031 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| RAZOR FREESTYLE SCOOTER | T40219N | 829 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| RE-VOLT | T-8109N | 2,868 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| RE-VOLT | T-8101M | 2,871 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| RE-VOLT DEMO | 6100003 50 | 3,415 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| READY 2 RUMBLE BOXING | T9704N | 2,002 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| READY 2 RUMBLE BOXING | T9704N | 2,012 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| READY 2 RUMBLE BOXING | HDR-0051 | 2,034 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| READY 2 RUMBLE BOXING | T9704D  50 | 2,012 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| READY 2 RUMBLE BOXING | T9704D  05 | 2,011 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| READY 2 RUMBLE BOXING -- (C) 1999 MIDWAY, INC. | T9704N | 1,890 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| READY 2 RUMBLE BOXING ROUND 2 | T9717N | 1,379 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| READY 2 RUMBLE BOXING ROUND 2 | T9711D  50 | 1,379 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| READY 2 RUMBLE BOXING ROUND 2 | T9711D  50 | 1,379 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| RECODE OF LODOSS WAR | T27903M | 3,401 | refused | New, no virtual VMU | terminal exits overlap hook windows | no profile: CD music Off |  |
| RECODE OF LODOSS WAR | 6107816 | 2,870 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| RED DOG SUPERIOR FIREPOWER | MK-5102151 | 3,375 | refused | New, no virtual VMU | terminal exits overlap hook windows | — |  |
| REELFISHINGWILD | T44303N | 1,776 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| RENAICHU | T47602M | 558 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| RENT A HERO NO.1 | HDR-0074 | 1,720 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| RESIDENT EVIL3 | T1220N | 2,046 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| RESIDENT EVIL3 | T7021D  05 | 2,051 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| RESIDENT EVIL3 | T7021D  18 | 2,053 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| REVOLT | T8107D  50 | 2,871 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| REZ | HDR-0178 | 1,087 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| RIPPIN' RIDERS | MK-51010 | 1,362 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| ROADSTERS | T22901D 50 | 820 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| ROADSTERS RC7 | T22901N | 796 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| ROGUE SPEAR | T45002D-05 | 165 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| ROOMMATE ASAMI - OKUSAMA HA JYOSIKOUSEI - | T19508M | 534 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| ROOMMATENOVEL - YUKA SATO - | T19502M | 570 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| RUNDIM AS BLACK SOUL | T46201M | 2,313 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| RUSH 2049 | T-9707N | 1,097 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| RUSH 2049 | 9D-0800-50 | 1,104 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| RYOKO INOUE - ROOMMATE - | T19505M | 520 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| RYOKO INOUE LAST SCENE | T19506M | 667 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SAKATSUKU FILE DOWNLOAD | 6107383 | 1,401 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| SAKATUKUTOKUDAIGO | HDR-0126 | 2,880 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| SAKATUKUTOKUDAIGO2 | HDR-0183 | 2,880 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| SAKURA TAISEN ONLINE PARI NO YUUGANA HIBI | HDR-0185 | 2,800 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SAKURA TAISEN ONLINE TEITO NO NAGAI HIBI | HDR-0184 | 2,799 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SAKURA WARS | HDR-0072 | 1,624 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| SAKURA WARS | HDR-0072 | 1,624 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| SAKURA WARS | HDR-0072 | 1,622 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| SAKURA WARS 2 | HDR-0082 | 687 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| SAKURA WARS 2 | HDR-0082 | 686 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| SAKURA WARS 3 DRAMA DOWNLOAD DISC | HDR-0191 | 1,663 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SAKURA WARS 4 | HDR-0191 | 2,180 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SAKURA WARS 4 PREVIEW DISC | HDR-0184 | 1,692 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SAKURA WARS2 / SAKURA WARS3 PREVIEWDISC | HDR-0082 | 1,200 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SAKURA WARS3 | HDR-0152 | 2,215 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SAKURATAISEN KINEMATRON HANAGUMI | HDR-0136 | 2,791 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SAKURATAISEN3 MOVIE DISC | 6108008 | 692 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SAMBA DE AMIGO | MK-51092 | 2,115 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| SAMBA DE AMIGO | HDR-0063 | 2,110 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| SAMBA DE AMIGO | MK-5109250 | 2,117 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| SAMBADEAMIGO V2 | HDR-0131 | 2,724 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| SAN FRANCISCO RUSH 2049 | T-9709D-50 | 1,096 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| SANGOKUSHI6WITHPK | T7607M | 3,210 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| SEAMAN | MK-51048 | 2,465 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| SEAMAN ESA DISK | 6107804 | 536 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| SEAMAN THE FORBIDEN PEST 2001 | T39406M | 2,473 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| SEGA DREAMCAST BROWSER 2.0 | MK-51056 | 3,520 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| SEGA DREAMCAST BROWSER 2.62 | MK-51142 | 3,520 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| SEGA DREAMCAST WEB BROWSER 2.0 WITH SEGANET | MK-51128 | 3,520 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| SEGA EXTREME SPORTS | HDR-0146 | 1,945 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| SEGA EXTREME SPORTS | MK-5108150 | 1,907 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| SEGA MARINE FISHING | MK-51096 | 1,409 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| SEGA MARINE FISHING | HDR-0104 | 1,409 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| SEGA TETRIS | HDR-0073 | 4,254 | refused | New, no virtual VMU | executable extent not proved | — |  |
| SEGA TETRIS | HDR-0073 | 4,254 | refused | New, no virtual VMU | executable extent not proved | — |  |
| SEGA WORLD WIDE SOCCER 2000 | 51031-50 | 1,064 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| SEGA WORLD WIDE SOCCER 2000 | 51031-50 | 1,064 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| SEGAGT | MK-5105350 | 1,910 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| SEGAKARA FOR DREAMCAST | 6107762 | 1,703 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SEIMATAISEN | T46901M | 916 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| SENTIMENTAL GRAFFITI YAKUSOKU | T20128M | 913 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SEVENTH CROSS EVOLUTION | T41301N | 1,669 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| SGGG | HDR-0151 | 2,088 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SGGG | HDR-0151 | 2,080 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SHANGHAI DYNASTY | T9902M | 860 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SHANGLI-RA | 6107369 | 2,143 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| SHANGRI-LA | T40801M | 2,145 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| SHIKIGAMI NO SHIRO 2 | T47702M | 1,632 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SHIN HONKAKU HANAFUDA | T16601M | 1,002 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| SHINRI GAME DC EDITION | T43403M | 1,279 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SHUTOKOU BATTLE | T30801M | 1,046 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| SHUTOKOU BATTLE 2 | T30803M | 1,382 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SILENT SCOPE | T9507N | 1,353 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SILENT SCOPE | T9513M | 1,353 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SILENT SCOPE | T9505D | 1,353 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SILVER | T15109D | 2,883 | refused | New, no virtual VMU | terminal exits overlap hook windows | — |  |
| SIROTSUMESOUWA | T20123M | 1,304 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SISTER PRINCESS PREMIUM EDITION | T27802M | 601 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SLAVE ZERO | T15106N | 1,295 | refused | New, no virtual VMU | Maple driver family not resolved | — (only short layout tracks) |  |
| SLAVE ZERO | T15106N | 1,294 | refused | New, no virtual VMU | Maple driver family not resolved | — (only short layout tracks) |  |
| SLAVE ZERO | T15104D 50 | 1,296 | refused | New, no virtual VMU | Maple driver family not resolved | no profile: CD music Off |  |
| SNOW | T20132M | 1,525 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SNOW SURFERS | MK-5101050 | 1,363 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| SOLDIER OF FORTUNE | T40212N | 754 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| SONIC ADVENTURE | MK-5100050 | 6,577 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SONIC ADVENTURE  2 TRIAL VERSION | 6108057 | 1,517 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SONIC ADVENTURE 2  THE TRIAL | 6107994 | 1,266 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SONIC ADVENTURE INTERNATIONAL | HDR-0043 | 6,584 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SONIC ADVENTURE LIMITED EDITION | MK-51014 | 6,584 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| SONIC SHUFFLE | MK-51060 | 998 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SONIC SHUFFLE | HDR-0125 | 1,004 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SONIC SHUFFLE | MK-5106050 | 990 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SORAWOMAUTSUBASA | T23205M | 11,528 | refused | New, no virtual VMU | executable extent not proved | — |  |
| SORCERIAN-DC | T9103M | 1,545 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| SOUL FIGHTER | T41401N | 659 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| SOUL FIGHTER | T41401D 50 | 661 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| SOULCALIBUR | T1401N | 3,593 | refused | New, no virtual VMU | terminal exits not resolved | — | known limit: incomplete dump (its data track ends before the boot file); set aside for now |
| SOULCALIBUR | T1401M | 3,494 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SOULCALIBUR | T1401D  50 | 4,038 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SOULCALIBUR | 6107173 | 3,221 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SOULCALIBUR | 6107175 | 3,220 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SOUTH PARK RALLY | T-8116N | 1,154 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| SOUTH PARK RALLY | T-8112D-50 | 1,157 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| SPACE CHANNEL 5 PART 2 | HDR-0190 | 1,268 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SPACECHANNEL5 | MK-51051 | 2,432 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| SPACECHANNEL5 | HDR-0029 | 2,432 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| SPACECHANNEL5 | MK-5105150 | 2,432 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| SPACECHANNEL5 | MK-5105150 | 2,432 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| SPACECHANNEL5 | 6107368 | 2,496 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| SPAWN IN THE DEMONS HAND | T1216N | 1,191 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | — | boots; intro problems; audio distortion observed (native) |
| SPAWN IN THE DEMONS HAND | T1216M | 1,191 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | — |  |
| SPAWN IN THE DEMONS HAND | T36816D 50 | 1,191 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | — |  |
| SPEC OPS2: OMEGA SQUAD | T45004D 50 | 980 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| SPEED DEVILS | T17702N | 3,480 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| SPEED DEVILS | T17702M | 3,481 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| SPEED DEVILS | T17702D 50 | 3,481 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| SPEED DEVILS ONLINE | T17718N | 495 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| SPIDER-MAN | T13008N | 3,761 | refused | New, no virtual VMU | terminal exits overlap hook windows | — (only short layout tracks) |  |
| SPIDER-MAN | T13008N | 3,763 | refused | New, no virtual VMU | terminal exits overlap hook windows | no profile: CD music Off |  |
| STAR WARS DEMOLITION | T23003N | 3,723 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| STAR WARS EPISODE 1 RACER | T23001N | 993 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| STAR WARS EPISODE ONE JEDI POWER BATTLES | T23002N | 1,550 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| STAR WARS EPISODE ONE JEDI POWER BATTLES | T23002D | 1,549 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| STAR WARS: EPISODE 1 RACER | T23001D 50 | 996 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| STAR WARS:DEMOLITION | T13010D 05 | 3,722 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| STREET FIGHTER ALPHA3 SAIKYO-DOJO | T1203N | 7,589 | refused | New, no virtual VMU | executable extent not proved | — |  |
| STREET FIGHTER ALPHA3 SAIKYO-DOJO | T7005D  50 | 7,591 | refused | New, no virtual VMU | executable extent not proved | — |  |
| STREET FIGHTER ZERO3 SAIKYO-RYU DOJO | T1203M | 7,648 | refused | New, no virtual VMU | executable extent not proved | — |  |
| STREET FIGHTER ZERO3 SAIKYO-RYU DOJO | 6107161 | 7,648 | refused | New, no virtual VMU | executable extent not proved | — |  |
| STREET FIGHTER ZERO3 SAIKYO-RYU DOJO FOR MATCHING SERVICE | T1230M | 1,736 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| STREET FIGHTER3 DOUBLE IMPACT | T1210N | 803 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| STREET FIGHTER3 W IMPACT | T1211M | 803 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| STUNT GP | T22904D 50 | 2,236 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| STUPID INVADERS | T17708N | 721 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| STUPID INVADERS | T17711D | 721 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| SUIGETSU | T19725M | 1,256 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SUIKA | T46509M | 790 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| SUPER EURO SOCCER 2000 | T15006M | 2,582 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| SUPER MAGNETIC NEO | T40206N | 3,297 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| SUPER MAGNETIC NEO | T40206D 50 | 3,336 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| SUPER MAGNETIC NEO STAND ALONE DEMO | T40206D | 3,296 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| SUPERMAGNETICNIUNIU | T30802M | 3,244 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| SURF ROCKET RACERS | T40216N | 2,030 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| SURF ROCKET RACERS | T17721D 50 | 2,030 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| SUZUKI ALSTARE EXTREME RACING | T17703N | 1,008 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| SUZUKI ALSTARE EXTREME RACING | T17703D 50 | 1,008 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| SWWS 2000 EE | MK51083 50 | 1,353 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| TAKO NO MARINE | T5204M | 613 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| TAMACUE | T20133M | 962 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| TANAKATORAHIKO NO URUTORARYUUSYOUGI IBISYAANAGUMAHEN | T22001M | 887 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| TANTEISHINSHIDASH | T44401M | 650 | refused | New, no virtual VMU | terminal exits overlap hook windows | — (only short layout tracks) |  |
| TEE OFF | T8108N | 1,290 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| TEE OFF | T8108D  50 | 1,290 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| TENNIS 2K2 | MK-51186 | 2,703 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| TENNIS 2K2 | MK-51186 | 2,699 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| TENOHIRA | T46516M | 536 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| TENTAMA -1ST SUNNY SIDE- | T19710M | 1,311 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| TEST DRIVE V-RALLY | T15110N | 2,344 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | no profile: CD music Off |  |
| THE GRINCH | T9512N | 801 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| THE GRINCH | T9503D | 1,603 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| THE HOUSE OF THE DEAD 2 | HDR-0007 | 794 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| THE HOUSE OF THE DEAD 2 | MK-5100250 | 794 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| THE HOUSE OF THE DEAD 2 | MK51002-50 | 793 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| THE KING OF FIGHTERS 2000 | T47303M | 2,779 | refused | New, no virtual VMU | Maple driver family not resolved | — (only short layout tracks) |  |
| THE KING OF FIGHTERS 2001 | T47304M | 3,113 | refused | New, no virtual VMU | Maple driver family not resolved | — (only short layout tracks) |  |
| THE KING OF FIGHTERS 2002 | T47305M | 3,360 | refused | New, no virtual VMU | Maple driver family not resolved | — (only short layout tracks) |  |
| THE LAST BLADE 2 | T44305N | 1,286 | refused | New, no virtual VMU | Maple driver family not resolved | no profile: CD music Off |  |
| THE LOST GOLEM | T41501M | 1,896 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| THE TYPING OF THE DEAD | MK-51144 | 1,078 | refused | New, no virtual VMU | SDK heap family not resolved | — (only short layout tracks) |  |
| THE TYPING OF THE DEAD | HDR-0067 | 1,072 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| THIS IS A SAMPLE DISK FOR USE IN THE OFFICE ONLY  DO NOT SELL   DO NOT CARRY OUT FROM THE OFFICE | MK0001 | 1,025 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| THIS IS A SAMPLE DISK FOR USE INTHE OFFICE ONLY / DO NOT SELL / DO NOT CARRY OUT FROM THE OFFICE | T0000M | 2,005 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| TNN MOTORSPORTS HARDCORE HEAT | T0000M | 4,499 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| TOKUSATSUBOUKENKATSUGEKI SUPERHERO RETSUDEN | T20601M | 865 | refused | New, no virtual VMU | 284 KiB lease does not fit | — |  |
| TOKYO BUS GUIDE | T35402M | 947 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| TOKYO BUS GUIDE | T35402M | 947 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| TOKYO HIGHWAY CHALLENGE | T40201D 50 | 1,041 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| TOKYO HIGHWAY CHALLENGE 2 | T40210D 50 | 1,371 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| TOKYO XTREME RACER | T40202N | 1,041 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| TOKYO XTREME RACER 2 | T40211N | 1,370 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| TONY HAWK'S PRO SKATER | T40205N | 2,800 | refused | New, no virtual VMU | terminal exits overlap hook windows | — (only short layout tracks) |  |
| TONY HAWKS PRO SKATER 2 | T13006N | 3,428 | refused | New, no virtual VMU | 284 KiB lease does not fit | — (only short layout tracks) |  |
| TONY HAWKS PRO SKATER 2 | T13008D 05 | 3,429 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| TONY HAWKS PRO SKATER 2 | T13008D 18 | 3,431 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| TONY HAWKS SKATEBOARDING | T40204D 50 | 2,800 | refused | New, no virtual VMU | terminal exits overlap hook windows | no profile: CD music Off |  |
| TORAUMAN | T35405M | 1,017 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| TORAUMAN | T35405M | 523 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| TOUKONRETSUDEN4 | T28202M | 1,763 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| TOY RACER | MK-5114950 | 949 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| TOY STORY 2 / BUZZ LIGHTYEAR TO THE RESCUE | T13003D 18 | 8,164 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| TOY STORY 2 / BUZZ LIGHTYEAR TO THE RESCUE | T13003D 05 | 8,276 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| TOY STORY 2 / BUZZ LIGHTYEAR TO THE RESCUE | T13003D 09 | 8,277 | refused | New, no virtual VMU | SDK heap family not resolved | — |  |
| TREASURE STRIKE | T19701M | 2,126 | refused | New, no virtual VMU | heap initializer call not proved | — |  |
| TRICKSTYLE | T-8102N | 1,070 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | no profile: CD music Off |  |
| TRICKSTYLE | T-8101D-50 | 1,074 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | no profile: CD music Off |  |
| TRICOLORE CRISE | T9104M | 1,491 | refused | New, no virtual VMU | terminal exits overlap hook windows | — |  |
| TRIGGERHEART EXELICA | T29102M | 1,973 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| TRIZEAL | T47901M | 1,047 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| TWINKLESTAR SPRITES | T3103M | 710 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| TYPING OF THE DATE | T14307M | 808 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| UEFA DREAM SOCCER | MK-5109505 | 1,631 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| ULTIMATE FIGHTING CHAMPIONSHIP | T40204N | 1,009 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| ULTIMATE FIGHTING CHAMPIONSHIP | T1241M | 1,015 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| ULTIMATE FIGHTING CHAMPIONSHIP | T40203D 50 | 1,013 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| ULTIMATE FIGHTING CHAMPIONSHIP | T40204D | 702 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| UNDER DEFEAT | T46705M | 1,014 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| UNREAL TOURNAMENT | T15125N | 166 | refused | New, no virtual VMU | terminal exits overlap hook windows | no profile: CD music Off |  |
| UNREAL TOURNAMENT | T15113D 50 | 2,550 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| UTAU TANBURINGU DAISU | T23207M | 11,290 | refused | New, no virtual VMU | executable extent not proved | — |  |
| V-RALLY 2 EXPERT EDITION | T15105D 50 | 2,343 | refused | New, no virtual VMU | no heap initializer slot (fixed RAM; needs a RAM map) | no profile: CD music Off |  |
| VAMPIRE CHRONICLE FOR MATCHINGSERVICE | T1235M | 5,558 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| VANISHING POINT | T-8110N | 1,476 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| VANISHING POINT | T-8110D-50 | 1,478 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| VERMILION DESERT | T5302M | 1,264 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| VIGILANTE 8 SECOND OFFENSE | T13002N | 3,447 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| VIGILANTE 8 SECOND OFFENSE | T13002D | 3,448 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| VIGILANTE8 SECOND BATTLE | T36501M | 3,472 | refused | New, no virtual VMU | heap initializer call not proved | no profile: CD music Off |  |
| VIRTUA TENNIS | MK-51054 | 2,035 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| VIRTUA TENNIS | MK-51054 | 2,039 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| VIRTUA TENNIS | 5105450 | 2,062 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| VIRTUA TENNIS | MK-5105450 | 2,062 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| VIRTUA TENNIS 2 | MK-5118650 | 2,703 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| VIRTUAATHLETE2000 | T44301N | 1,699 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| VIRTUAATHLETE2K | HDR-0081 | 1,678 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| VIRTUAATHLETE2K | MK-5109450 | 1,699 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| VISUALPARK | 6107801 | 2,221 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| VIVA ROCK VEGAS | T-45003D50 | 1,544 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| WACKY RACES | T15113N | 158 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| WACKY RACES | T15106D | 158 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| WALT DISNEY WORLD QUEST MAGICAL RACING TOUR | T36804N | 2,451 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| WALT DISNEY WORLD QUEST MAGICAL RACING TOUR | T36809D 50 | 2,451 | refused | New, no virtual VMU | 284 KiB lease does not fit | no profile: CD music Off |  |
| WELCOME TO PIACARROT 3 | T20122M | 1,344 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| WELCOME TO PIACARROT2.5 | T20109M | 598 | refused | New, no virtual VMU | Maple driver family not resolved | no profile: CD music Off |  |
| WIND -A BREATH OF HEART- | T47104M | 1,225 | refused | New, no virtual VMU | Maple driver family not resolved | — |  |
| WORLD NEVERLAND2 THE WAKTIC REPUBLIC OF PLUTO STORIES | T5303M | 2,195 | refused | New, no virtual VMU | terminal exits not resolved | — |  |
| WORLD SERIES BASEBALL 2K1 | MK-51055 | 4,285 | refused | New, no virtual VMU | executable extent not proved | — |  |
| WORLD SERIES BASEBALL 2K1 | HDR-0157 | 4,285 | refused | New, no virtual VMU | executable extent not proved | — |  |
| WWF: ATTITUDE | T-8103N | 2,962 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| WWF: ATTITUDE | T-8103N-50 | 2,959 | refused | New, no virtual VMU | Maple layout not resolved | — (only short layout tracks) |  |
| WWTBAM | T36811D 05 | 496 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| WWTBAM | T36811D 05 | 506 | refused | New, no virtual VMU | Maple layout not resolved | no profile: CD music Off |  |
| XTREME SPORTS | T15126N | 1,906 | refused | New, no virtual VMU | SDK heap family not resolved | no profile: CD music Off |  |
| YAMAZAKI | HDR-0050 | 889 | refused | New, no virtual VMU | terminal exits overlap hook windows | no profile: CD music Off |  |
| YUME NO TSUBASA | T19706M | 1,149 | refused | New, no virtual VMU | terminal exits not resolved | no profile: CD music Off |  |
| YUMEBAKEN99INTERNET | T40101M | 808 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| ZERO GUNNER2 | T20401M | 705 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| ZOMBIE REVENGE | MK-51038 | 10,057 | refused | New, no virtual VMU | Maple layout not resolved | — | works without virtual VMU (standalone New); known limit: six-input Maple layout and allocator/reservation (integration roadmap section 3; not owner-confirmed) |
| ZOMBIE REVENGE | HDR-0026 | 10,059 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| ZOMBIE REVENGE | MK-5103850 | 10,058 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| ZOMBIE REVENGE | HDR-0026 | 10,059 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| ZOMBIE REVENGE SAMPLE | 6107353 | 1,763 | refused | New, no virtual VMU | Maple layout not resolved | — |  |
| 18WHEELER | MK-51064 | 2,112 | refused | refused | Maple driver family not resolved | — |  |
| 18WHEELER | MK-5106450 | 2,112 | refused | refused | Maple driver family not resolved | — |  |
| 18WHEELER AMERICAN-PRO-TRUCKER | HDR-0080 | 2,112 | refused | refused | Maple layout not resolved | — |  |
| AERO DANCING FEATURING BLUE IMPULSE | T6802M | 2,365 | refused | refused | Maple driver family not resolved | — |  |
| AERO DANCING TODOROKI TAICHOU NO HIMITSU DISK | T6803M | 2,425 | refused | refused | Maple driver family not resolved | — |  |
| AERODANCING FEATURING BLUEIMPULSE TAIKENBAN | 6107106 | 2,358 | refused | refused | Maple driver family not resolved | — |  |
| AIRFORCE DELTA | T9501N | 1,600 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| AIRFORCE DELTA | T9502M | 1,602 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| AM2 OF CRI 2001 NEW LINEUP MOVIE COLLECTION | 6108018 | 1,259 | refused | refused | executable extent not proved | — |  |
| ATELIER MARIE AND ELIE | T44701M | 22 | refused | refused | Maple driver family not resolved | — |  |
| ATELIER MARIE AND ELIE OMAKE | T44701M | 22 | refused | refused | Maple driver family not resolved | — |  |
| BLUE STINGER | T13001N | 1,929 | refused | refused | Maple driver family not resolved | — | known limit: earlier interrupt-table initializer and its ownership (integration roadmap section 3; not owner-confirmed) |
| BLUE STINGER | HDR-0003 | 1,917 | refused | refused | Maple driver family not resolved | — |  |
| BLUE STINGER | T13001D-05 | 1,929 | refused | refused | Maple driver family not resolved | — |  |
| BLUE STINGER | T13001D 09 | 1,932 | refused | refused | Maple driver family not resolved | — |  |
| BLUE STINGER | T13001D 18 | 1,933 | refused | refused | Maple driver family not resolved | — |  |
| CART RACING | MK-51007 | 1,191 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| DEADLY SKIES | T9501N-50 | 1,821 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| DEMOLITION RACER - NO EXIT INFOGRAMES FREE DEMO | T15112N | 158 | refused | refused | Maple driver family not resolved | — (only short layout tracks) |  |
| Dream Passport | 6107000 | 2,413 | refused | refused | Maple driver family not resolved | — |  |
| Dream Passport | 6107055 | 2,412 | refused | refused | Maple driver family not resolved | — (only short layout tracks) |  |
| Dream Passport for TOYOTA | 6107057 | 3,801 | refused | refused | Maple driver family not resolved | — |  |
| DREAM PREVIEW VOL3 | 610702703 | 326 | refused | refused | Maple driver family not resolved | — |  |
| DREAM PREVIEW VOL5 | 610702705 | 487 | refused | refused | Maple driver family not resolved | — |  |
| DREAMCAST PROMOTION DISK | 6107124 | 471 | refused | refused | Maple driver family not resolved | — |  |
| DREAMCAST_EXPRESS_VOL1 | 610711001 | 467 | refused | refused | Maple driver family not resolved | — |  |
| DREAM_PREVIEW_VOL0 | 610702700 | 306 | refused | refused | Maple driver family not resolved | — |  |
| DREAM_PREVIEW_VOL2 | 610702702 | 304 | refused | refused | Maple driver family not resolved | — |  |
| DYNAMITE COP | MK-51013 | 1,873 | refused | refused | Maple driver family not resolved | — |  |
| DYNAMITE COP | MK-5101350 | 1,873 | refused | refused | Maple driver family not resolved | — |  |
| DYNAMITE DEKA 2 | HDR-0020 | 1,864 | refused | refused | Maple driver family not resolved | — |  |
| DYNAMITE DEKA 2 IN STORE TRIAL VERSION | 6107150 | 2,004 | refused | refused | executable extent not proved | — |  |
| DYNAMITE DEKA 2 TRIAL VERSION | 6107146 | 2,004 | refused | refused | executable extent not proved | — |  |
| ELEMENTAL GIMMICK GEAR          BY HUDSON | T14302M | 654 | refused | refused | Maple driver family not resolved | — |  |
| ETERNAL ARCADIA KEY VERSION | HDR-0119 | 14,271 | refused | refused | executable extent not proved | — |  |
| EVOLUTION | T38701M | 3,395 | refused | refused | executable extent not proved | — |  |
| EVOLUTION | 6107039 | 3,417 | refused | refused | executable extent not proved | — |  |
| F355 CHALLENGE | MK-0100 | 1,563 | refused | refused | Maple driver family not resolved | — |  |
| F355 CHALLENGE | HDR-0100 | 1,569 | refused | refused | Maple driver family not resolved | — |  |
| F355 CHALLENGE | T8118D  50 | 1,778 | refused | refused | Maple driver family not resolved | — |  |
| F355 TRIAL | 6107899 | 1,419 | refused | refused | Maple driver family not resolved | — |  |
| FLOIGAN BROS. EPISODE 1  MOIGLE'S SECTRET PROJECT | MK-51114 | 848 | refused | refused | Maple driver family not resolved | — (only short layout tracks) |  |
| FLOIGAN BROS. EPISODE 1: MOIGLE'S SECRET PROJECT | MK-51114 | 848 | refused | refused | Maple driver family not resolved | — (only short layout tracks) |  |
| GETBASS | HDR-0023 | 1,116 | refused | refused | Maple driver family not resolved | — |  |
| GETBASS | 6107119 | 859 | refused | refused | Maple driver family not resolved | — |  |
| GIANTGRAM ALL JAPAN PRO.WRESTLING2 IN NIPPON BUDOUKAN | HDR-0005 | 2,213 | refused | refused | Maple driver family not resolved | — |  |
| GODZILLA GENERATIONS | HDR-0004 | 1,322 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| HYDRO THUNDER | T-9702N | 1,877 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| INCOMING | T15001M | 722 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| INCOMING | T15001M | 722 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| July | T35401M | 1,262 | refused | refused | Maple driver family not resolved | — |  |
| July | T35401M | 1,260 | refused | refused | Maple driver family not resolved | — |  |
| MABOROSI TUKIYO | T4401M | 766 | refused | refused | Maple driver family not resolved | — |  |
| MABOROSI TUKIYO | 6107186 | 753 | refused | refused | Maple driver family not resolved | — |  |
| MAHJONG TAIKAI 2 SPECIAL | T7601M | 773 | refused | refused | Maple driver family not resolved | — |  |
| MARVEL VS.CAPCOM | T1202N | 4,815 | refused | refused | Maple driver family not resolved | — |  |
| MARVEL VS.CAPCOM | T1202M | 4,809 | refused | refused | Maple driver family not resolved | — |  |
| MARVEL VS.CAPCOM | T7002D 50 | 4,817 | refused | refused | Maple driver family not resolved | — |  |
| MARVEL VS.CAPCOM | 6107121 | 4,805 | refused | refused | Maple driver family not resolved | — |  |
| MONACO GRAND PRIX RACING SIMULATION 2 | T17701M | 1,639 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| Mr.Yukawa's Search The Treasure | HDR-0024 | 645 | refused | refused | Maple driver family not resolved | — |  |
| MY TRACKMAN | T37001M | 4,329 | refused | refused | Maple driver family not resolved | — (only short layout tracks) |  |
| NOBUNAGA NO YABOU SHOUSEIROKU with PowerUpKit | T7602M | 1,830 | refused | refused | Maple driver family not resolved | — |  |
| OOGA BOOGA | MK-51140 | 1,356 | refused | refused | Maple driver family not resolved | — (only short layout tracks) |  |
| OUTTRIGGER | MK-51102 | 2,815 | refused | refused | Maple driver family not resolved | — |  |
| OUTTRIGGER | HDR-0118 | 2,825 | refused | refused | Maple driver family not resolved | — |  |
| OUTTRIGGER | MK-5110250 | 2,794 | refused | refused | Maple driver family not resolved | — |  |
| PENPEN TRIAL VERSION | 6107102 | 1,966 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| PENPEN TRIICELON | T17001M | 1,974 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| PENPEN TRIICELON | T17001M | 1,970 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| PENPEN TRIICELON | T17001M | 1,923 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| PLASMA SWORD | T1207N | 9,291 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| PLASMA SWORD | T7005D  50 | 9,295 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| POPNMUSIC | T9501M | 559 | refused | refused | Maple driver family not resolved | — |  |
| POWERSTONE | T1201M | 3,357 | refused | refused | Maple driver family not resolved | — |  |
| POWERSTONE SAMPLE | 6107115 | 3,357 | refused | refused | Maple driver family not resolved | — |  |
| PROPELLER ARENA | MK-51162 | 2,511 | refused | refused | Maple driver family not resolved | — |  |
| PROYAKYUU TEAM DE ASOBOU | HDR-0027 | 711 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| PROYAKYUU TEAM DE ASOBOU TAIKENBAN | 6107385 | 711 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| PSYCHIC FORCE 2012 | T-31101N | 4,605 | refused | refused | Maple driver family not resolved | — |  |
| PSYCHIC FORCE 2012 | T1101M | 4,595 | refused | refused | Maple driver family not resolved | — |  |
| PSYCHIC FORCE 2012 | T-8106D-50 | 4,610 | refused | refused | Maple driver family not resolved | — |  |
| PSYCHICFORCE2012 | T1101M | 4,588 | refused | refused | Maple driver family not resolved | — |  |
| PUROYAKYUU      TEAM DE         ASOBOU          TOKUBETSUBAN | 6107350 | 1,047 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| PUROYAKYUU      TEAM WO         TAMESOU         FAMITUU         TOKUBETUBAN | 6107159 | 1,047 | refused | refused | Maple driver family not resolved | — |  |
| PUROYAKYUU TEAM WO TSUKUROU | HDR-0022 | 933 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| PUROYAKYUU TEAM WO TSUKUROU | HDR-0022 | 933 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| PUYO4 | 6107109 | 1,311 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| PUYOPUYO4 | HDR-0014 | 1,769 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| REALSOUND | T30001M | 675 | refused | refused | Maple driver family not resolved | — |  |
| REALSOUND | T30001M | 676 | refused | refused | Maple driver family not resolved | — |  |
| REALSOUND | T30001M | 674 | refused | refused | Maple driver family not resolved | — |  |
| REALSOUND(D2-SHOCK!-) | T30001M | 2,844 | refused | refused | Maple driver family not resolved | — |  |
| REDLINE RACER | T15002M | 880 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| SANGOKUSHI 6 | T7603M | 5,295 | refused | refused | Maple driver family not resolved | — |  |
| SEGA BASS FISHING | MK-51006 | 1,117 | refused | refused | Maple driver family not resolved | — |  |
| SEGA BASS FISHING | MK-5100650 | 1,063 | refused | refused | Maple driver family not resolved | — |  |
| SEGA DREAMCAST BROWSER | MK-51027A | 1,455 | refused | refused | Maple driver family not resolved | — (only short layout tracks) |  |
| SEGA DREAMCAST BROWSER | MK-51027 | 1,455 | refused | refused | Maple driver family not resolved | — (only short layout tracks) |  |
| SENGOKU TURB | T38801M | 4,714 | refused | refused | Maple driver family not resolved | — |  |
| SENGOKU TURB Trial Version | 6107103 | 4,708 | refused | refused | Maple driver family not resolved | — |  |
| SEVENTH CROSS | T38802M | 1,684 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| SHENMUE | MK-51059 | 2,632 | refused | refused | Maple driver family not resolved | — (only short layout tracks) |  |
| SHENMUE | MK-51059 | 4,936 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| SHENMUE | HDR-0016 | 2,646 | refused | refused | Maple driver family not resolved | — (only short layout tracks) |  |
| SHENMUE | HDR-0016 | 4,936 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| SHENMUE | HDR-0016 | 4,936 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| SHENMUE | MK-5105950 | 2,690 | refused | refused | Maple driver family not resolved | — (only short layout tracks) |  |
| SHENMUE | MK-5105950 | 4,936 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| SHENMUE2 | HDR-0164 | 3,105 | refused | refused | Maple driver family not resolved | — (only short layout tracks) |  |
| SHENMUE2 | HDR-0164 | 3,105 | refused | refused | Maple driver family not resolved | — |  |
| SHENMUE2 EU | MK-5118450 | 3,121 | refused | refused | Maple driver family not resolved | — (only short layout tracks) |  |
| SHUTOKOU BATTLE | 6107134 | 1,063 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| SONIC ADVENTURE | HDR-0001 | 6,624 | refused | refused | Maple driver family not resolved | — |  |
| SOUL REAVER | T36802N | 3,374 | refused | refused | 284 KiB lease does not fit | — (only short layout tracks) |  |
| SOUL REAVER | T36802N | 3,374 | refused | refused | 284 KiB lease does not fit | — (only short layout tracks) |  |
| SOUL REAVER | T36802N | 3,374 | refused | refused | 284 KiB lease does not fit | — (only short layout tracks) |  |
| SOUL REAVER | T36802N | 3,374 | refused | refused | 284 KiB lease does not fit | — (only short layout tracks) |  |
| SOUL REAVER | T36802N | 3,374 | refused | refused | 284 KiB lease does not fit | — (only short layout tracks) |  |
| SPORTS JAM | T44304N | 13,164 | refused | refused | executable extent not proved | — |  |
| SPORTS JAM | HDR-0153 | 13,161 | refused | refused | executable extent not proved | — |  |
| STAR GLADIATOR 2 | T1204M | 9,291 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| STAR GLADIATOR 2 | 6107181 | 9,290 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| STREET FIGHTER 3 3RD STRIKE | T1213N | 13,993 | refused | refused | executable extent not proved | — |  |
| STREET FIGHTER 3 3RD STRIKE | T1209M | 13,962 | refused | refused | executable extent not proved | — |  |
| STREET FIGHTER 3 3RD STRIKE | T7013D-50 | 13,994 | refused | refused | executable extent not proved | — |  |
| SUPER SPEED RACING | HDR-0013 | 1,190 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| SYDNEY 2000 | T36808N | 1,608 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| SYDNEY 2000 THE OFFICIAL VIDEO GAME OF THE OLYMPIC GAMES | T36808D 03 | 1,609 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| SYDNEY 2000 THE OFFICIAL VIDEO GAME OF THE OLYMPIC GAMES | T36808D 05 | 1,609 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| SYDNEY 2000 THE OFFICIAL VIDEO GAME OF THE OLYMPIC GAMES | T36808D 09 | 1,609 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| SYDNEY 2000 THE OFFICIAL VIDEO GAME OF THE OLYMPIC GAMES | T36808D 18 | 1,609 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| SYDNEY 2000 THE OFFICIAL VIDEO GAME OF THE OLYMPIC GAMES | T36808D 13 | 1,609 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| SYDNEY 2000 THE OFFICIAL VIDEO GAME OF THE OLYMPIC GAMES | T36808D 06 | 1,609 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| SYDNEY2000 | T1237M | 1,622 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| TETRIS 4D | T20801M | 1,020 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| THE HOUSE OF THE DEAD 2 | HDR-0007 | 846 | refused | refused | Maple driver family not resolved | — |  |
| THE HOUSE OF THE DEAD 2 | HDR-0007 | 846 | refused | refused | Maple driver family not resolved | — |  |
| TOY STORY 2 / BUZZ LIGHTYEAR TO THE RESCUE | T13003N | 13,311 | refused | refused | executable extent not proved | — |  |
| US SHENMUE | HDR-0156 | 2,653 | refused | refused | Maple driver family not resolved | — (only short layout tracks) |  |
| US SHENMUE | HDR-0156 | 4,936 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| VF3TB | MK-51001 | 1,152 | refused | refused | Maple driver family not resolved | — |  |
| VF3tb | HDR-0002 | 3,392 | refused | refused | Maple driver family not resolved | — |  |
| VF3tb | HDR-0002 | 1,152 | refused | refused | Maple driver family not resolved | — |  |
| VF3TB | MK-5100150 | 1,152 | refused | refused | Maple driver family not resolved | — |  |
| VF3tb+1 | HDR-0002 | 384 | refused | refused | Maple driver family not resolved | — |  |
| VF4PASSPORT | 6108088 | 2,052 | refused | refused | Maple driver family not resolved | — |  |
| VIRTUA FIGHTER HISTORY | 6108087 | 569 | refused | refused | executable extent not proved | — |  |
| VIRTUA STRIKER 2 VER.2000.1 | MK-51028 | 2,112 | refused | refused | Maple driver family not resolved | — |  |
| VIRTUA STRIKER 2 VER.2000.1 | HDR-0045 | 2,112 | refused | refused | Maple driver family not resolved | — |  |
| VIRTUA STRIKER 2 VER.2000.1 | MK-5102850 | 2,112 | refused | refused | Maple driver family not resolved | — |  |
| VIRTUA STRIKER 2 VER.2000.1 | MK-5102850 | 2,112 | refused | refused | Maple driver family not resolved | — |  |
| WEB MYSTERY | T39501M | 808 | refused | refused | Maple driver family not resolved | — |  |
| WETRIXPLUS | T8111N | 1,441 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| WETRIXPLUS | T40504D 50 | 1,441 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| WHATS SHENMUE | 6107179 | 2,433 | refused | refused | Maple driver family not resolved | no profile: CD music Off |  |
| YS GAME WORKS VOL.1 | 6108099 | 3,093 | refused | refused | Maple driver family not resolved | — (only short layout tracks) |  |
| QUAKE III ARENA | MK-51061 | 96 | not established (loader) | not established (loader) | boot file is only a loader; the stage binds the loader, not the program it starts. Its programs: Q3_USA.BIN (1,285 KiB) terminal exits not resolved; INTRO.BIN (346 KiB) terminal exits not resolved | no profile: CD music Off |  |
| TEST DRIVE LE MANS | T15123N | 105 | not established (loader) | not established (loader) | boot file is only a loader; the stage binds the loader, not the program it starts. Its programs: LEMANS24.BIN (2,229 KiB) Maple layout not resolved; FMV.BIN (487 KiB) SDK heap family not resolved | — (only short layout tracks) |  |
| 4X4 EVOLUTION | T41903N | 1,666 | Windows CE reader | — |  | no profile: CD music Off |  |
| APRICOT | T37910M | 1,984 | Windows CE reader | — |  | — |  |
| ARMADA | T40301N 00 | 1,224 | Windows CE reader | — |  | CE profile (Armada) | boots; intro movie playback is poor (under investigation) (Windows CE) |
| ATARI ANNIVERSARY EDITION | T15130N | 1,312 | Windows CE reader | — |  | — (only short layout tracks) |  |
| BANG! GUNSHIP ELITE | T44102N | 1,324 | Windows CE reader | — |  | no profile: CD music Off |  |
| BIOHAZARD 2 VALUE PLUS | T1214M | 1,158 | Windows CE reader | — |  | no profile: CD music Off |  |
| BIOHAZARD 2 VALUE PLUS | T1214M | 1,158 | Windows CE reader | — |  | no profile: CD music Off |  |
| BUST A MOVE 4 | T-8117N | 1,788 | Windows CE reader | — |  | CE profile (Bust-A-Move 4) |  |
| BUST-A-MOVE4 | T-8109D-50 | 1,788 | Windows CE reader | — |  | CE profile (Bust-A-Move 4) |  |
| CAESARS PALACE 2000 | T-12504N | 1,164 | Windows CE reader | — |  | no profile: CD music Off |  |
| CAESARS PALACE 2000 | 12502D-50 | 1,164 | Windows CE reader | — |  | no profile: CD music Off |  |
| CHAMPIONSHIP SURFER | T41403N | 1,854 | Windows CE reader | — |  | no profile: CD music Off |  |
| CHAMPIONSHIP SURFER | T41402D | 1,854 | Windows CE reader | — |  | no profile: CD music Off |  |
| CHAMPIONSHIP SURFER | T41402D 50 | 1,854 | Windows CE reader | — |  | no profile: CD music Off |  |
| CHERRY BLOSSOM | T37912M | 1,984 | Windows CE reader | — |  | — |  |
| DAISESSEN | T35403M | 2,536 | Windows CE reader | — |  | — |  |
| DENSHA DE GO 2 3K FOR DREAMCAST | T1102M | 1,142 | Windows CE reader | — |  | — |  |
| DUCATI WORLD | T-8113N | 1,258 | Windows CE reader | — |  | no profile: CD music Off |  |
| DUCATI WORLD | T-8121D-50 | 1,258 | Windows CE reader | — |  | no profile: CD music Off |  |
| EISEIMEIJIN3 GAME CREATOR YOSIMURA NOBUHIRO NO ZUNOU | T9503M | 1,378 | Windows CE reader | — |  | no profile: CD music Off |  |
| EVANGELION TYPE C | T35102M | 2,070 | Windows CE reader | — |  | — (only short layout tracks) |  |
| EVANGELION TYPE E | T35101M | 2,070 | Windows CE reader | — |  | — (only short layout tracks) |  |
| FOR SYMPHONY | T37908M | 1,984 | Windows CE reader | — |  | — |  |
| FRAGRANCE TAIL | T37905M | 1,984 | Windows CE reader | — |  | — |  |
| GET COLONY | HDR-0071 | 1,252 | Windows CE reader | — |  | no profile: CD music Off |  |
| GIANT KILLERS | T45401D 50 | 1,746 | Windows CE reader | — |  | no profile: CD music Off |  |
| GIANT KILLERS | T45401D 50 | 1,746 | Windows CE reader | — |  | no profile: CD music Off |  |
| GREATEST ARCADE HITS 1 | T-9713N | 1,940 | Windows CE reader | — |  | — (only short layout tracks) |  |
| GREATEST ARCADE HITS 2 | T-9714N | 1,320 | Windows CE reader | — |  | no profile: CD music Off |  |
| HAPPY LESSON | T19503M | 2,772 | Windows CE reader | — |  | — |  |
| HAPPY LESSON -FIRST LESSON- | T19504M | 1,238 | Windows CE reader | — |  | — |  |
| HELLO KITTY LOVELY FRUIT PARK | HDR-0055 | 1,232 | Windows CE reader | — |  | no profile: CD music Off |  |
| HELLO KITTY MAGICAL BLOCK | HDR-0070 | 2,192 | Windows CE reader | — |  | no profile: CD music Off |  |
| HELLO KITTY WAKUWAKU COOKIES | HDR-0090 | 1,258 | Windows CE reader | — |  | no profile: CD music Off |  |
| HIDDEN AND DANGEROUS | T-40502-N | 2,258 | Windows CE reader | — |  | — (only short layout tracks) |  |
| HIDDEN AND DANGEROUS | T40503D 50 | 2,258 | Windows CE reader | — |  | no profile: CD music Off |  |
| HOYLE CASINO | T11008N 00 | 1,118 | Windows CE reader | — |  | — (only short layout tracks) |  |
| HUNDRED SWORDS | HDR-0124 | 2,220 | Windows CE reader | — |  | no profile: CD music Off |  |
| HUNDRED SWORDS KEY VERSION | HDR-0127 | 2,220 | Windows CE reader | — |  | no profile: CD music Off |  |
| JIMMY WHITE'S 2 CUEBALL | T7001D  50 | 1,266 | Windows CE reader | — |  | — |  |
| KISS PSYCHO CIRCUS THE NIGHTMARE CHILD | T41901N | 1,174 | Windows CE reader | — |  | no profile: CD music Off |  |
| KISS PSYCHO CIRCUS THE NIGHTMARE CHILD | T40506D 50 | 1,174 | Windows CE reader | — |  | no profile: CD music Off |  |
| KITAE WHITE ILLUMINATION | T14301M | 1,340 | Windows CE reader | — |  | — |  |
| KITAHEI GOLD | T39801M | 4,246 | Windows CE reader | — |  | no profile: CD music Off |  |
| KITTY GP | 6107381 | 1,070 | Windows CE reader | — |  | — |  |
| MARIONETTE COMPANY | T5202M | 1,382 | Windows CE reader | — |  | — |  |
| MARIONETTE COMPANY 2 | T5203M | 2,550 | Windows CE reader | — |  | no profile: CD music Off |  |
| MIDWAY'S GREATEST ARCADE HITS VOLUME 1 | T-9710D-50 | 1,940 | Windows CE reader | — |  | — (only short layout tracks) |  |
| MISS MOONLIGHT | T18702M | 2,908 | Windows CE reader | — |  | no profile: CD music Off |  |
| MORITA NO SAIKYO REVERSI | T39102M | 1,378 | Windows CE reader | — |  | — |  |
| MORITA NO SAIKYO REVERSI TAIKENBAN | 6107132 | 1,378 | Windows CE reader | — |  | — |  |
| MORITA NO SAIKYO SHOUGI | T39101M | 1,378 | Windows CE reader | — |  | — |  |
| NET DE PARA | T37903M | 2,180 | Windows CE reader | — |  | no profile: CD music Off |  |
| NFL QUARTERBACK CLUB 2001 | T-8115N | 1,302 | Windows CE reader | — |  | no profile: CD music Off |  |
| NIGHTMARE CREATURES II | T9504M | 2,368 | Windows CE reader | — |  | no profile: CD music Off |  |
| NIGHTMARE CREATURES II | T-9502D-50 | 2,380 | Windows CE reader | — |  | no profile: CD music Off |  |
| NISHIKAZE | T44502M | 1,138 | Windows CE reader | — |  | — |  |
| PLUS PLUMB | T37901M | 1,214 | Windows CE reader | — |  | — |  |
| PRINCESS MAKER COLLECTION | T44801M | 1,874 | Windows CE reader | — |  | — (only short layout tracks) |  |
| PUZZLE BOBBLE4 | T42301M | 1,788 | Windows CE reader | — |  | no profile: CD music Off |  |
| QBERT | T40403N | 1,258 | Windows CE reader | — |  | no profile: CD music Off |  |
| QUARTERBACK CLUB 2000 | T-8101N | 1,046 | Windows CE reader | — |  | no profile: CD music Off |  |
| QUARTERBACK CLUB 2000 | T8102D  50 | 1,046 | Windows CE reader | — |  | no profile: CD music Off |  |
| RAILROAD TYCOON II DREAMCAST EDITION | T41902N | 1,090 | Windows CE reader | — |  | no profile: CD music Off |  |
| RAILROAD TYCOON II DREAMCAST EDITION | T40505D 50 | 1,090 | Windows CE reader | — |  | no profile: CD music Off |  |
| RAINBOW SIX | T40401N | 1,162 | Windows CE reader | — |  | no profile: CD music Off |  |
| RESIDENT EVIL 2 | T1205N | 1,178 | Windows CE reader | — |  | no profile: CD music Off |  |
| RESIDENT EVIL 2 | T1205N | 1,178 | Windows CE reader | — |  | no profile: CD music Off |  |
| RESIDENT EVIL 2 | T7004D  50 | 1,178 | Windows CE reader | — |  | no profile: CD music Off |  |
| RESIDENT EVIL 2 | T7004D  50 | 1,178 | Windows CE reader | — |  | no profile: CD music Off |  |
| RUNE JADE | T14304M | 1,596 | Windows CE reader | — |  | — |  |
| SEGA RALLY 2 | MK-51019 | 1,144 | Windows CE reader | — |  | no profile: CD music Off |  |
| SEGA RALLY 2 | MK-5101950 | 1,228 | Windows CE reader | — |  | no profile: CD music Off |  |
| SEGA RALLY 2 | MK-5101950 | 1,228 | Windows CE reader | — |  | no profile: CD music Off |  |
| SEGA RALLY2 | HDR0010 | 1,414 | Windows CE reader | — |  | no profile: CD music Off |  |
| SEKAI FUSHIGI HAKKEN TROIA | T39701M | 1,166 | Windows CE reader | — |  | — |  |
| SIERRA SPORTS MAXIMUM POOL | T11010N | 1,658 | Windows CE reader | — |  | — (only short layout tracks) |  |
| SNO-CROSS | T40207N | 1,292 | Windows CE reader | — |  | no profile: CD music Off |  |
| SNO-CROSS | T17722D 50 | 1,156 | Windows CE reader | — |  | no profile: CD music Off |  |
| SOUTH PARK CHEFS | T-8105N | 1,002 | Windows CE reader | — |  | no profile: CD music Off |  |
| SOUTH PARK CHEFS LUV SHACK | T-8105D-50 | 1,002 | Windows CE reader | — |  | no profile: CD music Off |  |
| SPACE GRIFFON VF-9 | T39601M | 2,744 | Windows CE reader | — |  | no profile: CD music Off |  |
| SPIRIT OF SPEED 1937 | T-8118N | 1,258 | Windows CE reader | — |  | no profile: CD music Off |  |
| SPIRIT OF SPEED 1937 | T-8102M | 1,258 | Windows CE reader | — |  | no profile: CD music Off |  |
| SPIRIT OF SPEED 1937 | T-8117D-50 | 1,258 | Windows CE reader | — |  | no profile: CD music Off |  |
| STARLANCER | T40209N | 1,908 | Windows CE reader | — |  | — (only short layout tracks) |  |
| STARLANCER | T17723D 05 | 1,908 | Windows CE reader | — |  | no profile: CD music Off |  |
| SUPER PRODUCERS | T14303M | 802 | Windows CE reader | — |  | no profile: CD music Off |  |
| SUPER ROBOT WARS ALPHA | T20602M | 1,246 | Windows CE reader | — |  | — |  |
| SUPER RUNABOUT | T12509N | 1,042 | Windows CE reader | — |  | no profile: CD music Off |  |
| SUPER RUNABOUT | T22101M | 1,042 | Windows CE reader | — |  | no profile: CD music Off |  |
| SUPER RUNABOUT | T22101M | 1,042 | Windows CE reader | — |  | no profile: CD music Off |  |
| SUPER RUNABOUT | T7014D  50 | 1,240 | Windows CE reader | — |  | no profile: CD music Off |  |
| SUPER RUNABOUT SAN FRANCISCO EDITION | T22102M | 1,042 | Windows CE reader | — |  | no profile: CD music Off |  |
| SUPERRUNABOUT | 6107803 | 1,782 | Windows CE reader | — |  | no profile: CD music Off |  |
| SWEET SEASON | T37911M | 1,984 | Windows CE reader | — |  | — |  |
| TAXI2 | TAXI2 | 1,742 | Windows CE reader | — |  | no profile: CD music Off |  |
| THE NEXT TETRIS | T17717D 50 | 2,952 | Windows CE reader | — |  | no profile: CD music Off |  |
| THE NEXT TETRIS ONLINE EDITION | T40214N | 1,932 | Windows CE reader | — |  | no profile: CD music Off |  |
| TOM CLANCYS RAINBOW SIX | T-45001D05 | 1,162 | Windows CE reader | — |  | no profile: CD music Off |  |
| TOM CLANCYS RAINBOW SIX | T-45001D09 | 1,162 | Windows CE reader | — |  | no profile: CD music Off |  |
| TOM CLANCYS RAINBOW SIX | T-45001D18 | 1,162 | Windows CE reader | — |  | no profile: CD music Off |  |
| TOMB RAIDER CHRONICLES | T36812N | 1,122 | Windows CE reader | — |  | no profile: CD music Off |  |
| TOMB RAIDER CHRONICLES | T36815D18 | 1,122 | Windows CE reader | — |  | — |  |
| TOMB RAIDER CHRONICLES | T36815D09 | 1,122 | Windows CE reader | — |  | — |  |
| TOMB RAIDER CHRONICLES | T36815D05 | 1,122 | Windows CE reader | — |  | — |  |
| TOMB RAIDER CHRONICLES | T36815D06 | 1,122 | Windows CE reader | — |  | — |  |
| TOMB RAIDER CHRONICLES | T36815D13 | 1,122 | Windows CE reader | — |  | — |  |
| TOMB RAIDER IV | T1233M | 1,252 | Windows CE reader | — |  | — |  |
| TOMB RAIDER THE LAST REVELATION | T-36806N | 1,252 | Windows CE reader | — |  | — |  |
| TOMB RAIDER THE LAST REVELATION | T36804D05 | 1,252 | Windows CE reader | — |  | — |  |
| UNDERCOVER A.D.2025 KEI | T39001M | 1,026 | Windows CE reader | — |  | no profile: CD music Off |  |
| URBAN CHAOS | T36810N | 1,132 | Windows CE reader | — |  | no profile: CD music Off |  |
| URBAN CHAOS | T36810D 50 | 1,132 | Windows CE reader | — |  | no profile: CD music Off |  |
| VIRTUACOP2 | HDR-0061 | 1,594 | Windows CE reader | — |  | no profile: CD music Off |  |
| WEBTV CONNECTION KIT FOR DREAMCAST | 6107117 | 3,560 | Windows CE reader | — |  | — |  |
| WEBTV CONNECTION KIT FOR DREAMCAST 128BIT SSL | 610-7390 | 3,570 | Windows CE reader | — |  | — |  |
| WHO WANTS TO BEAT UP A MILLIONAIRE | T11011N | 1,132 | Windows CE reader | — |  | — (only short layout tracks) |  |
| WILD METAL | T42101N 00 | 3,276 | Windows CE reader | — |  | no profile: CD music Off |  |
| WILD METAL | T40501D-50 | 3,276 | Windows CE reader | — |  | no profile: CD music Off |  |
| WORLD NEVERLAND PLUS - OLERUD KINGDOM STORY - | T5301M | 3,910 | Windows CE reader | — |  | — |  |
| WORMS ARMAGEDDON | T40601N | 1,084 | Windows CE reader | — |  | CE profile (Worms Armageddon) |  |
| WORMS ARMAGEDDON | T40601D 50 | 1,084 | Windows CE reader | — |  | CE profile (Worms Armageddon) |  |
| WORMS WORLD PARTY | T22904N | 1,786 | Windows CE reader | — |  | CE profile (Worms World Party) |  |
| WORMS WORLD PARTY | T7016D  50 | 1,786 | Windows CE reader | — |  | CE profile (Worms World Party) |  |
| YOSHIANOOKADE NEKORONDE | T18704M | 2,916 | Windows CE reader | — |  | no profile: CD music Off |  |
| YUKIGATARI | T37909M | 2,340 | Windows CE reader | — |  | — (only short layout tracks) |  |

| Not checked yet | Product | Why | Console (recorded) |
| --- | --- | --- | --- |
| 09 Chairs (Japan).chd |  | not read: Truncated or non-Mode1 raw data sector |  |
| Action Replay CDX - Demo Version (UK) (Unl).chd |  | not read: No GD high-density data session found |  |
| BIOHAZARD CODE VERONICA | T1207M | same startup as 027-T1210M-BIOHAZARD-CODE-VERONICA-GENTEIBAN |  |
| BIOHAZARD CODE VERONICA | T1207M | same startup as 074-T1210M-BIOHAZARD-CODE-VERONICA-GENTEIBAN |  |
| CHRISTMAS SEAMAN PRESENTDISK | T39404M | same startup as 150-T39403M-CHRISTMAS-SEAMAN-MESSAGEKIT |  |
| DISNEY'S DINOSAUR | T17718D | same startup as None |  |
| DREAM PREVIEW VOL8 | 610702708 | same startup as 299-610702707-DREAM-PREVIEW-VOL7 |  |
| DREAMKEY | 832-000409 | same startup as 319-832-000205-DREAMKEY |  |
| DREAMKEY | 832-000409 | same startup as 320-832-000205-DREAMKEY |  |
| DREAMKEY2 B | 832004550 | same startup as 327-832004646-DREAMKEY2-C |  |
| DREAMKEY2 D | 832005350 | same startup as 327-832004646-DREAMKEY2-C |  |
| DREAMKEY31PORTUGAL | 832006946 | same startup as 324-8320068-50-DREAMKEY3 |  |
| DREAMKEY31SPAIN | 832006906 | same startup as 324-8320068-50-DREAMKEY3 |  |
| DREAMON 14 | 8320043 | same startup as 334-8320026-50-DREAMON-HW-BUNDLE-3 |  |
| DREAMON 15 | 8320047 | same startup as 334-8320026-50-DREAMON-HW-BUNDLE-3 |  |
| DREAMON 16 | 8320051 | same startup as 334-8320026-50-DREAMON-HW-BUNDLE-3 |  |
| DREAMON 17 | 8320054 | same startup as 334-8320026-50-DREAMON-HW-BUNDLE-3 |  |
| DREAMON 18 | 8320056 | same startup as 334-8320026-50-DREAMON-HW-BUNDLE-3 |  |
| Dreamon 19 | 8320060 | same startup as 334-8320026-50-DREAMON-HW-BUNDLE-3 |  |
| DREAMON 20 | 8320062 | same startup as 334-8320026-50-DREAMON-HW-BUNDLE-3 |  |
| Dreamon 22 | 8320066 | same startup as 334-8320026-50-DREAMON-HW-BUNDLE-3 |  |
| DREAMON 3 | 8320006 | same startup as 349-8320001-50-DREAMON-2 |  |
| DREAMON 6 | 8320015 | same startup as 333-8320021-50-DREAMON-HW2 |  |
| DREAMON 7 | 8320017 | same startup as 333-8320021-50-DREAMON-HW2 |  |
| DREAMON 8 | 8320019 | same startup as 357-8320025-50-DREAMON-7-8 |  |
| DREAMON HW BUNDLE 4 | 8320040 | same startup as 334-8320026-50-DREAMON-HW-BUNDLE-3 |  |
| DREAMON10 | 8320031 | same startup as 337-8320028-50-DREAMON10 |  |
| DREAMON10 | 8320027 | same startup as 337-8320028-50-DREAMON10 |  |
| DREAMON21 | 8320064 | same startup as 334-8320026-50-DREAMON-HW-BUNDLE-3 |  |
| DREAMON_13 | 8320038 | same startup as 334-8320026-50-DREAMON-HW-BUNDLE-3 |  |
| ETERNAL ARCADIA TENTOU TAIKENBAN | 6107918 | same startup as 276-6107164-06-DREAMCASTMAGAZINE-GD-VOL5 |  |
| FREESTYLE SCOOTER | T40214D | same startup as None |  |
| GRAUEN VOL.3 | HDR-0035 | same startup as 517-HDR-0034-GRAUEN-VOL-2 |  |
| GRAUEN VOL.4 | HDR-0036 | same startup as 517-HDR-0034-GRAUEN-VOL-2 |  |
| GRAUEN VOL.5 | HDR-0037 | same startup as 517-HDR-0034-GRAUEN-VOL-2 |  |
| HYDRO THUNDER | T-9702D-50 | same startup as None |  |
| IMAKOSO DREAMCAST WITH SPECIAL GD | 6107993 | same startup as 024-6108017-AERO-TO-JET-DE-DANCING |  |
| IRON ACES | T44904D | same startup as None |  |
| JOJO S BIZARRE ADVENTURE | T7007D | same startup as None |  |
| LE MANS 24 HOURS | HDR-0149 | same startup as None |  |
| LE MANS 24 HOURS | T15111D | same startup as None |  |
| LE MANS 24 HOURS | HDR-0149 | same startup as Le Mans 24 Hours (Europe) (En,Fr,De,Es,It).chd |  |
| LOONEY TUNES SPACE RACE | T15108D | same startup as None |  |
| METROPOLIS STREET RACER | MK-5102250 | same startup as None |  |
| METROPOLIS STREET RACER | MK-5102250 | same startup as MSR - Metropolis Street Racer (Europe) (En,Fr,De,Es) (Rev A).chd |  |
| NBA 2K | HDR-0069 | same startup as None |  |
| NBA2K1 | MK-51063 | same startup as 129-MK-51004-NBA-2K |  |
| NBA2K1 | MK-51063 | same startup as NBA 2K v1.008 (1999)(Sega)(US)[!][1S]/NBA 2K v1.008 (1999)(Sega)(US)[!][1S].gdi |  |
| NBA2K1 | MK-51063 | same startup as NBA 2K (Japan).chd |  |
| NBA2K2 | HDR-0197 | same startup as NBA 2K (Japan).chd |  |
| NFL2K1 | MK-51062 | same startup as 138-MK-51176-NCAA-COLLEGE-FOOTBALL-2K2 |  |
| NFL2K1 | MK-51062 | same startup as NCAA College Football 2K2 - Road to the Rose Bowl v1.003 (2001)(Sega)(US)[!][2S]/NCAA College Football 2K2 - Road to the Rose Bowl v1.003 (2001)(Sega)(US)[!][2S].gdi |  |
| NFL2K1 | MK-51062 | same startup as NCAA College Football 2K2 - Road to the Rose Bowl (USA).chd |  |
| NFL2K2 | MK-51168 | same startup as 129-MK-51004-NBA-2K |  |
| NFL2K2 | MK-51168 | same startup as NBA 2K v1.008 (1999)(Sega)(US)[!][1S]/NBA 2K v1.008 (1999)(Sega)(US)[!][1S].gdi |  |
| NFL2K2 | HDR-0196 | same startup as NBA 2K v1.000 (2000)(Sega)(JP)(en)[!]/NBA 2K v1.000 (2000)(Sega)(JP)(en)[!].gdi |  |
| NFL2K2 | HDR-0196 | same startup as NBA 2K (Japan).chd |  |
| NFL2K2 | MK-51168 | same startup as NBA 2K (Japan).chd |  |
| OFFICIAL SEGA DREAMCAST MAGAZINE DECEMBER 2000 | IGN9 | same startup as Generator Vol. 1 (Japan).chd |  |
| OFFICIAL SEGA DREAMCAST MAGAZINE FEBRUARY 2001 | IGN11 | same startup as Generator Vol. 1 (Japan).chd |  |
| OFFICIAL SEGA DREAMCAST MAGAZINE JANUARY 2001 | IGN10 | same startup as Generator Vol. 1 (Japan).chd |  |
| OFFICIAL SEGA DREAMCAST MAGAZINE JULY 2000 | IGN6 | same startup as Generator Vol. 1 (Japan).chd |  |
| OFFICIAL SEGA DREAMCAST MAGAZINE MARCH 2000 | IGN3 | same startup as Generator Vol. 1 (Japan).chd |  |
| OFFICIAL SEGA DREAMCAST MAGAZINE MAY 2000 | IGN4 | same startup as Generator Vol. 1 (Japan).chd |  |
| OFFICIAL SEGA DREAMCAST MAGAZINE NOVEMBER 2000 | IGN8 | same startup as Generator Vol. 1 (Japan).chd |  |
| OFFICIAL SEGA DREAMCAST MAGAZINE SEPTEMBER 2000 | IGN7 | same startup as Generator Vol. 1 (Japan).chd |  |
| OFFICIAL SEGA@DREAMCAST MAGAZINE NOVEMBER '99 | IGN1 | same startup as 484-51034-DREAMCAST-GENERATOR |  |
| PSO VER.2 | MK-5119350 | same startup as None |  |
| QUAKE 3 ARENA | MK-5106150 | same startup as None |  |
| REZ | MK-5119250 | same startup as None |  |
| ROGUE SPEAR | T-45002D09 | same startup as 1326-T45002D-05-ROGUE-SPEAR |  |
| ROGUE SPEAR | T-45002D18 | same startup as 1326-T45002D-05-ROGUE-SPEAR |  |
| ROGUESPEAR | T40402N | same startup as 060-T9505N-ESPN-NBA2NIGHT |  |
| ROGUESPEAR | T40402N | same startup as ESPN NBA 2night v1.005 (2000)(Konami)(US)[!][1S]/ESPN NBA 2night v1.005 (2000)(Konami)(US)[!][1S].gdi |  |
| ROGUESPEAR | T40402N | same startup as ESPN NBA 2Night (USA).chd |  |
| SAKURA WARS 4 DEMO MOVIE | 6108333 | same startup as 270-6107164-12-DREAMCASTMAGAZINE-GD-VOL11 |  |
| SAKURATAISEN3 TENTOUDEMOBAN | 6108022 | same startup as 875-6107990-PHANTASY-STAR-ONLINE-TENTOU-DEMO |  |
| SAN FRANCISCO RUSH 2049 RETAILER DEMO VERSION | 9707ND | same startup as ESPN NBA 2Night (USA).chd |  |
| SOLDIER OF FORTUNE | T17726D | same startup as None |  |
| SOULCALIBUR | T1401N | not read: Startup extent leaves the data tracks at LBA 547203 (tracks 0-300, 450-976 audio, 45000-171916) | known limit: incomplete dump (its data track ends before the boot file); set aside for now |
| SPEED DEVILS ONLINE | T17713D | same startup as None |  |
| SYSTEM-DISC | HKT-0900 | not read: Dreamcast GD IP header not found |  |
| THE NOMAD SOUL | T36805D | same startup as None |  |
| THE NOMAD SOUL | T36805D | same startup as Nomad Soul, The (Europe) (En,Fr,De,Es,It).chd |  |
| UEFA DREAM SOCCER | MK-5109509 | same startup as 1370-MK-5109505-UEFA-DREAM-SOCCER |  |
| UEFA DREAM SOCCER | MK-5109518 | same startup as 1370-MK-5109505-UEFA-DREAM-SOCCER |  |
| UEFA DREAM SOCCER | MK-5109506 | same startup as 1370-MK-5109505-UEFA-DREAM-SOCCER |  |
| UEFA STRIKER | T15102D | same startup as Striker Pro 2000 (USA).chd |  |
| WORLD SERIES BASEBALL 2K2 | MK-51152 | same startup as 287-T15125N-UNREAL-TOURNAMENT |  |
| WORLD SERIES BASEBALL 2K2 | MK-51152 | same startup as Unreal Tournament v1.000 (2001)(Infogrames)(US)(M5)[!]/Unreal Tournament v1.000 (2001)(Infogrames)(US)(M5)[!].gdi |  |
| WORLD SERIES BASEBALL 2K2 | HDR-0198 | same startup as None |  |
| WORLD SERIES BASEBALL 2K2 | HDR-0198 | same startup as Unreal Tournament (USA) (En,Fr,De,Es,It).chd |  |
| WORLD SERIES BASEBALL 2K2 | MK-51152 | same startup as Unreal Tournament (USA) (En,Fr,De,Es,It).chd |  |
| WWF: ATTITUDE | T-8103N-18 | same startup as 1424-T-8103N-50-WWF-ATTITUDE |  |

Also listed: 89 files in other image formats.

## Lease size and the placement refusals (14 Nov 2026)

The 60 `placement_384k` refusals and the 212 admitted startups were rerun
through the production placement probe at six lease sizes. A scratch copy
changed only the compact lease size and its fixed-size checks; at 284 KiB
it reproduces this table exactly. Record:
[lease-size survey](evidence/native-lease-size-survey-2026-11-14.json).

| Lease | What it holds | Admitted | Still refused |
| --- | --- | ---: | ---: |
| 284 KiB | Today: card worker, read-ahead engine, 192 KiB cache | 212 | 60 |
| 208 KiB | 116 KiB cache | 214 | 58 |
| 156 KiB | 64 KiB cache | 218 | 54 |
| 128 KiB | 36 KiB cache | 218 | 54 |
| 92 KiB | Engine, no cache | 218 | 54 |
| 72 KiB | Card worker only (the VMU-lite layout) | 219 | 53 |

Only seven startups depend on the size: Buzz 2000 (US and three PAL), the
Kanipan game and Pop'n Music 4 Append (JPN) and JoJo (JPN, card only). The
other 53 are refused by where the game's memory pool lies, at any size:

- **36 startups, 21 titles:** the pool begins before the end of the program
  file, by under 1 KiB to 127 KiB, and runs to the top of RAM. The lease
  would sit at the top, megabytes from the program, but the placement rule
  refuses any overlap of pool and program. The titles include NFL 2K and
  2K1, NBA 2K to 2K2, NCAA College Football 2K2, Tony Hawk's Pro Skater 2,
  Soul Reaver, Gauntlet Legends, Ready 2 Rumble, Namco Museum, Centipede,
  Evil Dead, Incoming, Expendable and Millennium Soldier.
- **Capcom fighters** (Capcom vs. SNK and Pro, Capcom vs. SNK 2, Marvel vs.
  Capcom 2, the Capcom Taisen fan disc): the SDK pool is smaller than any
  lease. These games manage their own memory, as DOA2 does, and need a RAM map.
- **Tokyo Xtreme Racer and Shutokou Battle:** the pool ends inside K-UI's
  stage, so a lease at its top would cover the stage.
- **Coji Coji and Superhero Retsuden:** the pool starts inside the stage.
- **Gekka no Kenshi 2 and Pop'n Music 2:** allocator state.
- **Buggy Heat:** its pool lies inside the program image.

The executable-overlap rule was added with the installer-stack protection
(`c6b7bccd`); why it must cover a pool's start inside the program tail is not
recorded. Reviewing it is a check change, not a memory change.

Fixed-RAM games (`heap`, `initializer_slot`, `initializer_call`) were not
rerun: their gates come first. For them a smaller lease matters differently,
because a RAM map must find a free region of that size.
