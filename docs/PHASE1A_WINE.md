# Phase 1a — Running under Wine (Linux)

Status (2026-11-09): **playable under Wine 11.20 Staging, including LAN multiplayer.** The game installs from the disc images, plays the intro videos, shows the main menu, and loads Human campaign mission 1. Unit selection and move orders work. A two-player TCP/IP match (host + join on one machine) starts and stays in sync. Longer play sessions, other factions and save/load are not tested yet. Screenshots are in `work/screens/` (kept out of git).

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
| 2b | Main menu completely black (the game runs and flips pages, but every color is black) | The sprite engine keeps its own copy of the palette-display flag (`sprite+0x9fc`, 3 more `GetDeviceCaps` checks). With it unset, no GDI or DirectDraw palette is ever created, so the 8-bit screen has no colors. | `palette` patch also forces these 3 checks (now 2 `CreatePalette` + `SetEntries` calls as expected) |
| 3 | Same crash when the real desktop is bigger than the game mode | Wine on Wayland doesn't switch resolution. The engine re-reads the screen size (2560×1440) and rebuilds its buffers at that size. | Run inside a Wine virtual desktop sized to `VideoMode` (`run_wine.sh`) |
| 4 | Missing videos and data | `Dominion.ini` is written by the original installer. The game parses backslash escapes in its `*Dir` paths, so `.\` must be written as `.\\`. | `setup_wine.sh` writes the INI from the template embedded in `ISO.R32` |
| 5 | Faction cutscenes split across two discs | Merc and Scorpion videos are on disc 2 | Both `CINE` folders are merged into `game/CINE` |
| 6 | Multiplayer: "Create" silently returns to the session list | The game folder ships the 1998 DirectPlay files. Wine used its own `dplayx` with the old TCP/IP provider (`dpwsockx`) from the game folder, and the two don't work together (`DPERR_GENERIC`). Wine's own TCP/IP provider can't host yet (`DPERR_UNSUPPORTED`). | `setup_wine.sh` sets `dplay`, `dplayx`, `dpwsock`, `dpwsockx` to native, so all of DirectPlay comes from the game folder |

Not problems:
* 3 GB of reported video memory (tested with Wine's `VideoMemorySize=64`, same result).
* Single Player → Mission shows an empty list. It lists custom missions made with the map editor; the shipped `.wxp` maps are all campaign maps.
* The first start in a new Wine prefix can show only the empty Wine desktop while Wine finishes setting up. Start again.
* DirectPlay shows its own "Locate Session" box when searching. Leave it blank (searches the LAN) or enter the host's address.

## Still open for Phase 1a
* **Music:** the `.iso` files contain no audio tracks (each file ends exactly at the end of its data track). We need a rip of the original discs, then a `winmm`/MCI replacement that plays the tracks from OGG files.
* **Play-testing:** campaign missions for all 4 factions, skirmish, save/load, every video mode.
* **Other `GetDeviceCaps` checks:** bit depth (`BITSPIXEL`), `SIZEPALETTE` and `NUMCOLORS` still report the true-color desktop. Nothing has broken yet. If something does, the cleaner fix is one wrapper for all `GetDeviceCaps` calls that answers like Windows 98 in 8-bit mode.
* **Windows 10/11:** test the same patches with a DirectDraw wrapper (cnc-ddraw). The palette patch should fix the same crash there.
* **Multiplayer over a real network:** only tested with two games on one machine (127.0.0.1). DirectPlay also needs TCP/UDP port 47624 plus a dynamic range open in a firewall.

## Tools
* `tools/is5x.py` — InstallShield 5 cabinet extractor
* `tools/patch_game.py` — byte patches; checks the original bytes first, keeps `.orig` backups
* `tools/setup_wine.sh`, `tools/run_wine.sh` — Wine install and launcher
* `tools/mp_test.sh` — two-player test on one machine: two installs on two hidden displays, host + join + start a match (`setup`, `start`, `shot`, `stop`). It switches both games to Wine's GDI renderer: with OpenGL on Xvfb (software rendering) the second game froze in a screen flip, and in lockstep multiplayer the host then waits forever. The GDI renderer sometimes shows a garbled frame, so take several screenshots
* `tools/xvfb_session.sh` — runs the game on a hidden Xvfb display for automated tests (`start`, `shot`, `click`, `key`, `stop`). Escape skips the logo videos; the intro starts on a black screen for a while, which looks like a hang but isn't
* `tools/ghidra/DecompileAll.java` — headless Ghidra script that decompiles every function to one C file (output in `work/decomp/`, not for publishing)
