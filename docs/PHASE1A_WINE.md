# Phase 1a — Running under Wine (Linux)

Status (2026-11-09): the game installs from the disc images, passes its CD check, plays the intro videos and reaches the main menu without crashing under Wine 11.20 Staging. Gameplay is not tested yet.

## Quick start
```sh
tools/setup_wine.sh "Dominion_Storm_Over_Gift_3_Win_ISO_EN/Game Files" 640   # or 800 / 1024 / 1280
tools/run_wine.sh
```
Installs to `~/.local/share/dominion-remastered` (override with `DOMINION_HOME`).

## Problems found and fixed

| # | Symptom | Cause | Fix |
|---|---|---|---|
| 1 | "Please insert the 'DOMINION1' CD" | `DSCheckCDIdentity` (WAVEPLAY.R32) asks MCI for the audio-CD identity (calculated from the audio track layout) or looks for a CD-ROM with the exact volume label. A mounted `.iso` has no audio TOC. | `patch_game.py` → `nocd`: function returns 1 |
| 2 | Crash in the sprite blitter (`Rtlib32+0x62a3e`) right when the main menu appears | `RtSetDisplayMode` checks `GetDeviceCaps(RASTERCAPS) & RC_PALETTE`. In 1998 this was always true (8-bit mode). Modern GDI reports true color, so the engine takes a rarely used true-color path whose zoom transition (`SpriteCopyRect`) reads a run-length-compressed background as if it were raw → pointer = data + 0xBEBEBEBE. | `patch_game.py` → `palette`: always use the palette path |
| 3 | Same crash when the real desktop is bigger than the game mode | Wine on Wayland doesn't switch resolution. The engine re-reads the screen size (2560×1440) and rebuilds its buffers at that size. | Run inside a Wine virtual desktop sized to `VideoMode` (`run_wine.sh`) |
| 4 | Missing videos and data | `Dominion.ini` is written by the original installer. The game parses backslash escapes in its `*Dir` paths, so `.\` must be written as `.\\`. | `setup_wine.sh` writes the INI from the template embedded in `ISO.R32` |
| 5 | Faction cutscenes split across two discs | Merc and Scorpion videos are on disc 2 | Both `CINE` folders are merged into `game/CINE` |

Not a problem: 3 GB of reported video memory (tested with Wine's `VideoMemorySize=64`, same result).

## Still open for Phase 1a
* **Music:** the `.iso` files contain no audio tracks (each file ends exactly at the end of its data track). We need a rip of the original discs, then a `winmm`/MCI replacement that plays the tracks from OGG files.
* **Play-testing:** campaign missions for all 4 factions, skirmish, save/load, every video mode.
* **Automated testing:** needs Xvfb + xdotool, so the game can run off-screen with screenshots and scripted clicks.
* **Windows 10/11:** test the same patches with a DirectDraw wrapper (cnc-ddraw). The palette patch should fix the same crash there.
* **Multiplayer:** DirectPlay over TCP/IP under Wine (Wine has its own `dplayx`).

## Tools
* `tools/is5x.py` — InstallShield 5 cabinet extractor
* `tools/patch_game.py` — byte patches; checks the original bytes first, keeps `.orig` backups
* `tools/setup_wine.sh`, `tools/run_wine.sh` — Wine install and launcher
* `tools/ghidra/DecompileAll.java` — headless Ghidra script that decompiles every function to one C file (output in `work/decomp/`, not for publishing)
