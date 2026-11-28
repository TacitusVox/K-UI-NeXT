# Dreamcast VGA patches by release

For US **The King of Fighters Dream Match 1999** v1.004 and **The King of Fighters Evolution** v1.000, connect VGA and launch the original normally. K-UI patches each pressing in memory. For other titles, check the product ID, region and version. Use the author’s full-disc patch when you also need changes to movies, sound or files the game loads later. [Automatic boot catalog](automatic-vga.md)

## Preparing a VGA companion for Games on SD

1. On a computer, start with your original disc image. Match the game, region, language, revision and disc number to the release listed below.
2. For a **DCP**, open **Apply Patch** in Universal Dreamcast Patcher, select the original image and the matching patch, and write the resulting GDI into a separate directory. Current versions accept TOSEC GDI, Redump CUE and CHD as input. [Universal Dreamcast Patcher](https://github.com/DerekPascarella/UniversalDreamcastPatcher/)
3. For a **PPF**, follow the package readme to identify its input. An executable patch applies to the named file extracted from your disc, commonly `1ST_READ.BIN`. Rebuild a GDI with the patched file and the complete original set of data and audio tracks. A track-level patch instead applies to the exact track named in its instructions. A checksum error means the patch expects a different input. [GDIBuilder](https://projects.sappharad.com/tools/gdibuilder.html)
4. Install the patched GDI and **all tracks it references** beside the original under `.kui-vga/<original filename including extension>/disc.gdi`. For example, for `/games/Skies Disc 1.gdi`, the descriptor is `/games/.kui-vga/Skies Disc 1.gdi/disc.gdi`. Each disc of a set gets its own companion. Launch the original library entry normally. Keep the original and allow additional card space for the companion’s descriptor and tracks. The launcher checks the disc identity, region compatibility, member of the set, and that the patched game can be prepared. Missing or rejected companions fall back to the original. The companion applies only to the SD Games route on a VGA cable.

For an original GDI, the PC helper can copy the complete track set into the correct directory. Run these commands from the K-UI source directory. The first uses a GDI already patched by the author's software.

```sh
python3 tools/prepare_vga_companion.py "/card/Games/Game.gdi" --prepared "/patched/disc.gdi"
```

To apply one DCP directly, use `--dcp "/patches/Game.dcp"` in place of `--prepared "/patched/disc.gdi"`. To match a directory of unpacked VGA DCP files against a collection of original GDIs, preview the matches and then install them.

```sh
python3 tools/prepare_vga_companion.py "/card/Games" --patch-dir "/patches" --dry-run
python3 tools/prepare_vga_companion.py "/card/Games" --patch-dir "/patches"
```

Direct DCP application requires the `rom-weaver` CLI and, for secondary-compressed deltas, `xdelta3`. A DCP without a disc header requires explicit selection. The helper leaves existing companions in place. Convert a CHD to a supported GDI before using it as the original Games entry. ZSO entries do not use a companion. [rom-weaver releases](https://github.com/rom-weaver/rom-weaver/releases) · [xdelta3](https://github.com/jmacd/xdelta)

Each patch link describes work by its author or points to the specific archived patch. A listed package is an installation option. The automatic catalog identifies the exact releases K-UI patches without preparing an image. The [community index](https://consolemods.org/wiki/Dreamcast:List_of_VGA_Games_and_Patches) lists 45 patch entries as of November 24, 2026, including two Hydro Thunder revisions. The [index cross-reference](evidence/automatic-vga/expanded/community-index.md) identifies the matching built-in profile for each entry. The index omits some regional variants by design. The tables include those variants when the release author identifies them.

## North American releases and their regional counterparts

| Game and release | Patch or installation detail |
| --- | --- |
| Airforce Delta, US `T9501N` v1.000 | TapamN's DCP. The separately authored PAL Deadly Skies correction targets a different executable. [US patch](https://consolemods.org/wiki/File:Airforce_Delta_(VGA_Patch_-_TapamN).dcp) · [PAL corrections by pitito](https://dreamcast-talk.com/forum/viewtopic.php?start=230&t=16508) |
| Bangai-O, US v1.001 | TapamN's DCP enables VGA. An optional second patch restores the original brightness. The PAL version can lose video after its refresh-rate menu. [US VGA patch](https://consolemods.org/wiki/File:Bangai-O_(VGA_Patch_-_TapamN).dcp) · [brightness patch](https://segaxtreme.net/resources/bangai-o-brightness-restoration.459/) |
| ECW Anarchy Rulz, US v1.000 | Use the US Anarchy Rulz DCP. [TapamN patch](https://consolemods.org/wiki/File:ECW_Anarchy_Rulz_(VGA_Patch_-_TapamN).dcp) |
| ECW Hardcore Revolution, US v1.001 | Use the US Hardcore Revolution DCP. [TapamN patch](https://consolemods.org/wiki/File:ECW_Hardcore_Revolution_(VGA_Patch_-_TapamN).dcp) |
| Gunbird 2, US `T1214N` v1.000 | Derek Pascarella's v1.0 DCP fixes stage slowdown, the loading graphic and the Gallery. Owners of DCHDMI, DCDigital or RetroGEM should set their device to **Force VGA** as the author specifies. Separate GDI and CDI track deltas are available. [Author's release and checksums](https://github.com/DerekPascarella/Gunbird2-VGASlowdownFixDreamcast) |
| Hydro Thunder, US v1.020 and prototype v0.800 | Use the DCP for retail v1.020. For the May 20, 1999 alpha, apply Japanese Cake’s PPF to `track36.iso` or its executable PPF to `1ST_READ.BIN` in that track. [Retail DCP by TapamN](https://consolemods.org/wiki/File:Hydro_Thunder_(VGA_Patch_-_TapamN).dcp) · [prototype patch by Japanese Cake](https://consolemods.org/wiki/File:Jc-hydroa-rgb-vga.zip) |
| JoJo's Bizarre Adventure | Rolly's package has a DCP each for US `T1206N` v1.001, PAL `T7007D  50` v1.001, JP `T1205M` v1.001 and JP trial `6107357` v1.002. The Japanese Matching Service edition already supports VGA. The published US and PAL patches also alter disc checks. [Four-release package](https://segasky.itch.io/jojos-bizarre-adventure-vga-patch-for-sega-dreamcast) |
| Plasma Sword / Star Gladiator 2 | Rolly supplies four DCPs for US, PAL, JP retail and JP trial. Preserve the audio tracks. The K-UI PAL disc header reads `T7005D  50` even though the release page prints another number. [Four-release package](https://segasky.itch.io/plasma-sword-star-gladiator-2-vga-patch-for-sega-dreamcast) |
| Psychic Force 2012 | US v1.010 uses TapamN's DCP. Rolly supplies separate DCPs for JP `T1101M` v1.002 and PAL `T-8106D-50` v1.000. Preserve all audio tracks. [US patch](https://consolemods.org/wiki/File:Psychic_Force_2012_(VGA_Patch_-_TapamN).dcp) · [JP and PAL package](https://segasky.itch.io/psychic-force-2012-vga-patch-for-sega-dreamcast) |
| Soul Fighter, US v1.000 | Use the US DCP. The PAL pressing has a separate compatibility report. [TapamN patch](https://consolemods.org/wiki/File:Soul_Fighter_(VGA_Patch_-_TapamN).dcp) |
| The King of Fighters Dream Match 1999, US v1.004 | The automatic fix and TapamN's DCP target `T3101N`. Opening, character selection and an actual fight exercise different parts of the patch. [US DCP](https://consolemods.org/wiki/File:The_King_of_Fighters_-_Dream_Match_1999_(VGA_Patch_-_TapamN).dcp) |
| The King of Fighters Evolution, US v1.000 | The built-in fix and the DCP target `T44302N`. The early Japanese releases of both KOF games need separate fixes. [US DCP](https://consolemods.org/wiki/File:The_King_of_Fighters_-_Evolution_(VGA_Patch_-_TapamN).dcp) |
| The Last Blade 2 / Gekka no Kenshi Final Edition | Rolly supplies US `T44305N` v1.002 and JP `T3107M` v1.003 DCPs. For a black screen on JP, the author reports a controller soft reset at the menu can restore VGA. Preserve the US audio track. [Both releases](https://segasky.itch.io/the-last-blade-2-vga-patch-for-sega-dreamcast) |
| The Ring: Terror's Realm, US `T15122N` v1.002 | Use the archived US DCP. The original patch author's identity is unknown. [Patch file](https://consolemods.org/wiki/File:The_Ring_-_Terror's_Realm_(VGA_Patch).dcp) |

## PAL video and timing patches

| Game and release | Patch or installation detail |
| --- | --- |
| Dino Crisis, PAL v1.000 | Choose English, French, German, Italian or Spanish. Japanese Cake's five separate executable patches fix FMV and, where present, subtitle rendering and timing. Test an opening movie and the credits. [Author's five patches](https://blog.japanese-cake.io/index.php/2016/11/03/dear-capcom-i-think-ive-finished-your-work-what-no-money/) |
| MTV Sports: Skateboarding Featuring Andy Macdonald, PAL v1.002 | Apply Japanese Cake’s PPF to `1ST_READ.BIN` from this pressing. [Patch archive](https://consolemods.org/wiki/File:Jc-mtvam-60hz-vga.zip) |
| Resident Evil: Code Veronica, PAL v1.000 | Use Japanese Cake's v1.1 package for English, French, German or Spanish on both discs. Each language shares its executable between discs. For whole-track input, use the correct `track01.bin` and `track03.bin` PPFs for each disc. The revision fixes the later display change in the golden Lugers room. [Author's four patches](https://blog.japanese-cake.io/index.php/2020/04/15/catching-up-part-1-claire-in-the-dark/) |
| Skies of Arcadia, PAL, both discs | Japanese Cake's v2 PPF targets the executable. For the fuller Perfect 60Hz version, use Riggles' matching DCP on each disc. It also corrects cutscene audio, sound effects and walking speed. Install both resulting GDIs to get these asset changes. [VGA executable patch](https://blog.japanese-cake.io/index.php/2015/02/18/skies-of-arcadia-euro-60hz-vga-patch/) · [Perfect 60Hz release](https://www.dreamcast-talk.com/forum/viewtopic.php?t=18173) |
| Snow Surfers, PAL v1.001 | Apply Japanese Cake’s PPF to `1ST_READ.BIN`. [Patch archive](https://consolemods.org/wiki/File:Jc-snows-60hz-vga.zip) |
| WWF Attitude, PAL v1.010 | K-UI includes this executable fix. To prepare a companion, apply the PPF to `1WWFTUDE.BIN` from the edition in the readme. The PAL catalog includes separate English and German releases. [Patch archive](https://consolemods.org/wiki/File:Jc-wwfa-60hz-vga.zip) |

## Japanese releases

Rolly’s DCPs in this list each patch one boot executable and carry an IP.BIN header. His release pages also describe replacing the executable in a rebuilt GDI. Use the package for the specific trial, full release or disc number shown. “PAL console support” on a Japanese patch page describes the console on which that Japanese disc can run.

| Game and release | Patch or installation detail |
| --- | --- |
| Berserk Trial Version R, `6107370` v1.001 | Hold on this trial. The published change writes through the cable-detection result, which can be address zero on VGA. [Instruction check](evidence/automatic-vga/expanded/japan.json) · [Rolly's release](https://segasky.itch.io/berserk-trial-version-r-vga-patch) |
| Black/Matrix AD, `T-20101M` v1.002, two discs | Select the corresponding DCP for each disc. One user reports Disc 1 checksum errors with two sources while Disc 2 succeeded. For Disc 1 use the author's GDIBuilder procedure with a matching original. [Rolly's package and report](https://segasky.itch.io/black-matrix-ad-vga-patch-for-sega-dreamcast) |
| Chō-Hatsumei Boy Kanipan: Bōsō Robot no Nazo!?, `HDR-0021` v1.005 | Use the Kanipan DCP. [Rolly's patch](https://segasky.itch.io/cho-hatsumei-boy-kanipan-vga-patch-for-sega-dreamcast) |
| Dance Dance Revolution 2nd Mix, `T-9506M` v1.003, and Club Version, `T-9508M` v1.000 | Choose the DCP for the named game. [Rolly's two-game package](https://segasky.itch.io/dance-dance-revolution-vga-patch-for-sega-dreamcast) |
| D+Vine [Luv], `T-46501M` v1.002 | Match the product ID specified by the author. Sega also lists another product number for this title. [Rolly's patch](https://segasky.itch.io/dvine-luv-vga-patch-for-sega-dreamcast) |
| deSPIRIA, `T14402M` v1.001, and trial `6107925` v1.000 | Rolly published both versions. ConsoleMods reports incompatibility with several display and cable combinations and links a separate fix. The alternate DCP targets the retail product. Verify your display through gameplay. [Rolly's package](https://segasky.itch.io/despiria-vga-patch) · [retail correction listed by ConsoleMods](https://drive.google.com/uc?export=download&id=1d01vJSCD5SZXolb1lrGsw1xojmnuOJjv) |
| Es, `T45501M` v1.001, discs 1–3 | Apply the shared executable PPF to `1ST_READ.BIN` for each disc, or use each disc’s own `track03.bin` PPF. The readme supplies a SHA-256 for each original track. [Patch archive](https://consolemods.org/wiki/File:Jc-es-vga-patch-v1.0.zip) |
| Langrisser Millennium, `T2501M` v1.003 | Use TapamN's DCP. [Patch file](https://consolemods.org/wiki/File:Langrisser_Millennium_(VGA_Patch_-_TapamN).dcp) |
| L.O.L.: Lack of Love, `T2106M` v1.001 | Use the PPF for `101.BIN` from `track09.bin` or apply the separate whole-track PPF. Japanese Cake also documents compatibility with the SEGA-SKY translation. [Original patch](https://consolemods.org/wiki/File:Jc-lol-vga.zip) · [translation](https://www.romhacking.net/translations/5387/) |
| Mahjong Taikai II Special, `T7601M` v1.003 | Use the Mahjong DCP. [Rolly's patch](https://segasky.itch.io/mahjong-taikai-ii-special-vga-patch) |
| Nanatsu no Hikan / Seven Mansions: Ghastly Smile, `T7604M` | K-UI patches the English v1.3 startup executable from either publisher package and the verified CD rebuild. Its publisher-supplied header uses `T38801M` v1.002. From Japanese retail, the official v1.4 GDI DCP includes 77 entries for the translated program, movies, audio, text, graphics and bootstrap. Apply the whole DCP for that edition or use the separate compressed-cutscene CDI release. [Both accepted source hashes](evidence/automatic-vga/expanded/seven-mansions.json) · [Publisher on the VGA-only changes](https://dreamcast-talk.com/forum/viewtopic.php?start=160&t=16508) · [Team releases](https://github.com/Eight-Mansions/SevenMansionsDC/releases) |
| Nijyuei, `T46505M` v1.001 | Use the Nijyuei DCP. [Rolly's patch](https://segasky.itch.io/nijyuei-vga-patch-for-sega-dreamcast) |
| Nobunaga no Yabou: Reppuuden, `T7605M` v1.001, and Shouseiroku with Power-Up Kit, `T7602M` v1.000 | The Reppuuden DCP patches the boot file `0NOBU8.BIN`. Shouseiroku uses `1ST_READ.BIN`. [Rolly's package](https://segasky.itch.io/sangokushi-nobunga-vga) |
| Rune Caster, `T40001M` v1.007 | Use the Rune Caster DCP. [Rolly's patch](https://segasky.itch.io/rune-caster-vga-patch) |
| Sangokushi VI, `T7603M` v1.000, and Sangokushi VI with Power-Up Kit, `T7607M` v1.000 | Select the matching edition from the Koei package. [Rolly's package](https://segasky.itch.io/sangokushi-nobunga-vga) |
| Seireiki Rayblade, `T42201M` v1.003, and Video ROM Trial 1, `6107747` v1.002 | Select retail or trial and preserve the audio track when rebuilding. [Rolly's package](https://segasky.itch.io/seireiki-rayblade-vga-patch-for-sega-dreamcast) |
| Sunrise Eiyuutan, `T41101M` v1.000 | Use the Sunrise Eiyuutan DCP. [Rolly's patch](https://segasky.itch.io/sunrise-eiyuutan-vga-patch) |
| Tanaka Torahiko no Uru Toraryuu Shougi: Ibisha Anaguma-hen, `T22001M` v1.006 | Use the Shougi DCP. [Rolly's patch](https://segasky.itch.io/tanaka-torahiko-no-uru-toraryuu-shougi-ibisha-vga-patch) |
| The Shinri Game / The Psychological Game, `T43403M` v1.004 | Use the Shinri Game DCP. [Rolly's patch](https://segasky.itch.io/the-shinri-game-vga-patch) |
| Treasure Strike, `T19701M` v1.006 | Use the Treasure Strike DCP. [Rolly's patch](https://segasky.itch.io/treasure-strike-vga-patch-for-sega-dreamcast) |
| Winning Post 4 Program 2000, `T7606M` v1.001 | Use the Winning Post DCP. [Rolly's patch](https://segasky.itch.io/winning-post-4-vga-patch) |
| World Neverland 2 Plus: Pluto Kyouwakoku Monogatari, `T5303M` v1.001 | The DCP targets the sequel. [Rolly's patch](https://segasky.itch.io/world-neverland-2-plus-vga-patch-for-sega-dreamcast) |
| Yukyu Gensokyoku 3: Perpetual Blue, `T27801M` v1.003 | Use the Perpetual Blue DCP. [Rolly's patch](https://segasky.itch.io/yukyu-gensokyoku-3-perpetual-blue-vga) |

## Releases with a header or regional compatibility report

**AeroWings 2 PAL** has a later report from Rolly that setting the VGA bit in `IP.BIN` works. An earlier survey reported failure with Xploder. K-UI already bypasses the BIOS header gate on its own boot paths, so try the original first. For a loader that checks the flag, enable it on a copy in Universal Dreamcast Patcher’s IP.BIN Editor. PAL **Hidden & Dangerous** has conflicting reports. One owner got a picture by choosing 60 Hz. PAL **Capcom vs. SNK** and **Heavy Metal Geomatrix** can get as far as the refresh-rate selector and then lose the image. PAL **The Next Tetris** can show publisher logos and then go black. [K-UI boot path](evidence/automatic-vga/boot-admission.md) · [AeroWings 2 retest](https://dreamcast-talk.com/forum/viewtopic.php?start=300&t=16508) · [PAL hardware reports](https://thecodelair.com/assemblergames/threads/35330/)

The [regional survey data](evidence/automatic-vga/regional-surveys.csv) records 52 PAL titles for which streeker reported exceptions to normal VGA boot. Xploder booted 29 in that survey, including three with missing movies or incorrect scaling, and 23 were listed as incompatible at the time. Firmware, cable, game revision and later patches matter. Among the other reported Xploder successes were Aqua GT, Buggy Heat, Evolution: The World of Sacred Device and Tokyo Highway Challenge. K-UI launch and in-game results still require testing for those releases. [Original PAL survey](https://dreamcast-talk.com/forum/viewtopic.php?start=10&t=494)

The same data includes 109 Japanese release entries where Sega's own VGA-box column is unmarked or marked unsupported. These records use the representative product numbers in Sega's catalog. They include titles such as Two One, D+Vine [Luv] and Evolution. A catalog mark alone says nothing about whether forcing VGA works. [Sega-published releases](https://www.sega.jp/history/hard/dreamcast/software.html) · [Licensed releases](https://www.sega.jp/history/hard/dreamcast/software_l.html)

**Caesars Palace 2000** (US `T-12504N`, PAL `12502D-50`) and **World Neverland Plus: Olerud Oukoku Monogatari** (JP `T5301M`) are the two titles the community index lists without a working VGA patch. All three disc products use Windows CE. Use their supported TV output for now. [Community status](https://consolemods.org/wiki/Dreamcast:List_of_VGA_Games_and_Patches#Unsupported_Games) · [K-UI disc catalog](https://github.com/TacitusVox/K-UI-NeXT/blob/main/docs/library-compat-world.md)
