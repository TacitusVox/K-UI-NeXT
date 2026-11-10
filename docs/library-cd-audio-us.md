# US library: CD audio games and their sound drivers

From the owner's US startup runs, which hold the track layout and small
sound-driver files for every disc. The record is
[the US CD audio survey](evidence/us-cd-audio-survey-2026-11-10.json).

| US startups | Games |
| --- | ---: |
| Real CD audio (an audio track of 30 s or more) | 99: 73 native, 26 Windows CE |
| Only short layout tracks between data tracks | 55 |
| No high-density audio tracks | 90 |

K-UI's native CD music adapts to each game's own sound driver: the MDK2
adapter is built around `AUDIO64.DRV`. The native CD-audio games ship a few
standard drivers. Builds are grouped here by exact bytes, which is
conservative.

| Driver | Native CD-audio games | Distinct builds | Largest builds cover |
| --- | ---: | ---: | --- |
| `MANATEE.DRV` | 46 | 16 | 8, 7, 6, 6, 6 games |
| `AUDIO64.DRV` | 33 | 11 | 10, 6, 5 games |
| `SOUND.BIN` | 12 | 12 | one each (game-specific) |
| `MANATEE2.DRV` | 9 | 1 | all 9 |
| `AICADRV.BIN` | 4 | 3 | 2 games |
| `MIDIDA.DRV` | 3 | 2 | 2 games |
| `ARMCOM.DRV` | 2 | 1 | both |

A game can ship more than one driver.

Next, when native CD music work resumes: compare the driver builds the way the
[WinCE controller drivers](wince-vmu-driver-families.md) were compared, modulo
relocation and data. Then find which games actually play their tracks (the
GD PLAY path in each executable), since audio tracks alone do not prove that.
PAL and JPN runs extend the same survey.
