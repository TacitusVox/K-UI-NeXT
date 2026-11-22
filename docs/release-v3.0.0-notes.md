# K-UI 3.0 RC1 release notes

This testing release integrates the regular-games and Windows CE loader lines. The GUI includes the larger native Background maps and the Windows CE routes for the Worms World Party and Bust-A-Move 4 kernel groups. The CD music work includes Ducati World’s repeat-count adjustment and the separate Japanese Virtua Cop 2 video-memory fix.

RC1 ships the promoted everyday generic WinCE reader `91b0540103f0`, with video-memory transfers and the adaptive slow window enabled. RAM pooling and read-ahead remain off. The five original WinCE CD-music readers keep the accepted `1ba2fa7e7040` configuration. The GUI accepts the `9e3295bcfaec` Standard VMU reader and includes the four `cc81e63ca4d2` Smash readers. The complete update also installs those four readers and both helpers as individual files.

Kernel support and console coverage differ by game. Puzzle Bobble 4 and Virtua Cop 2 (Japan) have reported console results with Windows CE CD music enabled. Five other kernels in the Bust-A-Move 4 group still await console tests. The regular-games background CD audio investigation remains in progress.

The manual SD ZIP and full testing KUP share a software inventory. Each managed module keeps its own build ID, including previously accepted readers. See the [loader acceptance record](https://github.com/TacitusVox/K-UI-NeXT/blob/main/docs/accepted-progress.md) for component-level results.
