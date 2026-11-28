# Coverage of the ConsoleMods Dreamcast VGA patch list

All 45 entries in the November 24, 2026 **VGA Patches** table have a built-in K-UI executable fix for a matching release. Those entries account for **52 of K-UI’s 64 profiles**. Dino Crisis uses five PAL language profiles and Resident Evil Code: Veronica uses four. The remaining 12 profiles cover additional releases listed below. These counts describe catalog coverage. Most profiles still need Dreamcast gameplay and display tests. [Index revision 98805](https://consolemods.org/wiki/index.php?title=Dreamcast:List_of_VGA_Games_and_Patches&oldid=98805) · [K-UI launch instructions](../../../automatic-vga.md)

The region column follows the wiki. The third column gives the internal product IDs and revisions accepted by K-UI. For the two entries identified by boot filename, it gives that filename and program size. K-UI also checks the executable identity before applying edits.

| Community entry | Listed region | K-UI release match | Scope |
| --- | --- | --- | --- |
| Airforce Delta | North America | `T9501N` · 1.000 | Uses TapamN’s corrected May 2023 build. |
| Bangai-O | North America | `T40217N` · 1.001 | The wiki also links an optional brightness patch. |
| Black/Matrix AD | Japan | `T20101M` · 1.002 | Both disc patches yield the same startup program. |
| Chō Hatsumei Boy Kanipan | Japan | `HDR-0021` · 1.005 |  |
| Dance Dance Revolution Club Version | Japan | `T9508M` · 1.000 |  |
| Dance Dance Revolution 2nd Mix | Japan | `T9506M` · 1.003 |  |
| Dino Crisis | PAL | `T7019D  05`, `T7019D  09`, `T7019D  18`, `T7019D  13`, `T7019D  06` · 1.000 | English, French, German, Italian and Spanish, respectively. |
| ECW Anarchy Rulz | North America | `T-8114N` · 1.000 |  |
| ECW Hardcore Revolution | North America | `T-8112N` · 1.001 |  |
| D+Vine [Luv] | Japan | `T46501M` · 1.002 |  |
| deSPIRIA | Japan | `T14402M` · 1.001 | Uses the October 2026 retail correction recommended by the wiki. |
| Es | Japan | `T45501M` · 1.001 | Discs 1–3 share an executable. |
| Hydro Thunder v1.020 | North America | `T-9702N` · 1.020 | Retail v1.020. |
| Hydro Thunder v0.800 prototype | North America | `1ST_READ.BIN` · 1,871,350 bytes | May 20, 1999 alpha. The publisher supplies the complete target. Original header fields are unknown. |
| JoJo’s Bizarre Adventure | North America | `T1206N` · 1.001 | The wiki row specifies US. |
| Langrisser Millennium | Japan | `T2501M` · 1.003 |  |
| L.O.L. – Lack of Love | Japan | `T2106M` · 1.001 |  |
| Seven Mansions – Ghastly Smile (English translation) | Japan | `T38801M` · 1.002 | English v1.3 startup, including the verified CD rebuild. See the retail-to-translation procedure below. |
| Mahjong Taikai II Special | Japan | `T7601M` · 1.003 |  |
| MTV Sports Skateboarding Featuring Andy Macdonald | PAL | `T10001D 50` · 1.002 |  |
| Nijyuei | Japan | `T46505M` · 1.001 |  |
| Nobunaga no Yabō – Reppūden | Japan | `T7605M` · 1.001 |  |
| Nobunaga no Yabō – Shōseiroku with Power-Up Kit | Japan | `T7602M` · 1.000 |  |
| Plasma Sword – Nightmare of Bilstein | North America | `T1207N` · 1.001 | The wiki row specifies US. |
| Psychic Force 2012 | North America | `T-31101N` · 1.010 | The wiki row specifies US. |
| Resident Evil Code: Veronica | PAL | `T36806D 05`, `T36806D 09`, `T36806D 18`, `T36806D 06` · 1.000 | English, French, German and Spanish, respectively. Both discs share each language’s startup. |
| Rune Caster | Japan | `T40001M` · 1.007 |  |
| Sangokushi VI | Japan | `T7603M` · 1.000 |  |
| Sangokushi VI with Power-Up Kit | Japan | `T7607M` · 1.000 |  |
| Seireiki Rayblade | Japan | `T42201M` · 1.003 |  |
| Skies of Arcadia | PAL | `MK-51052` · 1.000 | Both discs. The separate Perfect 60Hz companion also changes cutscene assets. |
| Snow Surfers | PAL | `MK-5101050` · 1.001 |  |
| Soul Fighter | North America | `T41401N` · 1.000 |  |
| Sunrise Eiyūtan | Japan | `T41101M` · 1.000 |  |
| Tanaka Torahiko no Urutoraryū Shōgi – Ibisha Anaguma-hen | Japan | `T22001M` · 1.006 |  |
| The King of Fighters Dream Match 1999 | North America | `T3101N` · 1.004 | Working video reported on a Dreamcast running an earlier K-UI build. |
| The King of Fighters Evolution | North America | `T44302N` · 1.000 |  |
| The Last Blade 2 – Heart of the Samurai | Japan | `T3107M` · 1.003 | The wiki row specifies Japan. The matching Japanese title is Gekka no Kenshi Final Edition. |
| The Ring – Terror’s Realm | North America | `T15122N` · 1.002 |  |
| The Shinri Game | Japan | `T43403M` · 1.004 |  |
| Treasure Strike | Japan | `T19701M` · 1.006 |  |
| Winning Post 4 Program 2000 | Japan | `T7606M` · 1.001 |  |
| World Neverland 2 Plus – Pluto Kyōwakoku Monogatari | Japan | `T5303M` · 1.001 |  |
| WWF Attitude | PAL | `1WWFTUDE.BIN` · 3,030,956 bytes · 1.010 | The known reference is PAL v1.010. The file and complete target identify the supported pressing. |
| Yūkyū Gensōkyoku 3 – Perpetual Blue | Japan | `T27801M` · 1.003 |  |

## Where the original media matters

- **Seven Mansions.** The wiki explicitly lists the English translation. The in-memory fix accepts two known English v1.3 program hashes. To start from the original Japanese retail disc, apply the translation team’s complete v1.4 GDI patch to a dump and use the result as an SD companion. The 77-entry release also supplies the translated movies, audio, text and other assets. [Verified executables](seven-mansions.json) · [Translation release](https://github.com/Eight-Mansions/SevenMansionsDC/releases/tag/release) · [Companion preparation](../../../vga-prescriptions.md)
- **Skies of Arcadia.** The built-in PAL profile applies the executable VGA fix. Riggles’ separate Perfect 60Hz release includes additional audio and timing changes. Prepare a companion for each disc to use its full asset changes. [Author’s release](https://www.dreamcast-talk.com/forum/viewtopic.php?t=18173)
- **Other regions and trials.** The 12 additional built-in profiles are PAL Deadly Skies, US Gunbird 2, Japanese JoJo retail and trial, PAL JoJo, PAL Plasma Sword, Japanese Star Gladiator 2 retail and trial, Japanese and PAL Psychic Force 2012, Japanese Seireiki Rayblade trial, and US The Last Blade 2. The wiki mentions Gunbird 2 in its introduction, outside its 45-row table.
- **Open cases.** The early Japanese KOF Dream Match and Evolution releases still need specific fixes. The deSPIRIA trial has no verified equivalent of the retail correction. Berserk Trial Version R’s publisher patch contains an unsafe store on its VGA path. The wiki’s separate unsupported section lists Caesars Palace 2000 and World Neverland Plus – Olerud Ōkoku Monogatari. World Neverland 2 Plus has its own supported entry above. [Trial verification](japan.json) · [Regional prescriptions](../../../vga-prescriptions.md)
