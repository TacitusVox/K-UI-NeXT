# Testing K-UI 3.0 RC1 on Dreamcast

The maintainer reported that the repaired full update installed directly from `64ae07435de5`, that Diagnostics showed `93f13e84d989`, and that the RC1 hardware checklist passed. CD music in **4x4 Evolution** was confirmed separately. The following checks can help other owners compare their setups and report regressions.

| Feature | Check |
| --- | --- |
| Full update | Install the full KUP, select **Restart K-UI**, and reopen the package. The review should show **Already installed**. Check the Games list, favorites, settings and an existing virtual card. |
| Physical startup | On a spare card, copy the SD ZIP’s `KUI/` folder to the card root. Start with the independent K-UI boot disc and confirm the same build under Diagnostics. |
| Native reader and saves | Save and reload in Power Stone 2 using Standard and Virtual VMU On. Launch MDK2 on Background with virtual VMU and CD music Off and reach gameplay. |
| WinCE data and music | Skip the intro in 4x4 Evolution with CD music Off and enter a race. Enable CD music and listen in gameplay. Try Armada with CD music and virtual VMU On, then save, relaunch and reload. |
| WinCE game families | With CD music enabled, start a second race in Ducati World. Play a stage in Puzzle Bobble 4 or the standalone Japanese Virtua Cop 2 and listen across a transition. |
| Sega Smash Pack | In one session switch through Virtua Cop 2, Shining Force, Phantasy Star II and Sega Swirl, then return to Virtua Cop 2. Reload a disposable RPG save and check VC2 textures. |

For an issue, include the GUI build, game and region, image format, storage interface, reader and VMU settings, and the first repeatable failure point. For sound reports, distinguish a movie soundtrack from CD music and note where playback starts or stops. **Diagnostics → START → Create problem report** can capture shell information.
