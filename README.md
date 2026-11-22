# Dominion: Storm Over Gift 3 — Remastered

A source port of *Dominion: Storm Over Gift 3* (Ion Storm, 1998), a real-time strategy game. The goal is a game that runs natively on today's Windows and Linux, plays exactly like the original, and later gets optional extras (higher resolutions, upscaled cutscenes).

**You need your own copy of the game.** This repository contains no game data: no graphics, sounds, videos, music, maps or original program files. The tools here only work with the files from your own discs.

## Status

Work in progress; the game is not yet playable without the original program files.

The original game consists of `dominion.exe`, an engine (`Rtlib32.dll`, the "7th Level Runtime Player") and plugin modules. They are replaced one by one with our own code, each tested against the original:

| Module | What it does | Status |
|---|---|---|
| `BmpFont.r32` | bitmap fonts | **ours** (pixel-identical) |
| `Smacker.r32` | cutscene playback | **ours** (own Smacker decoder) |
| `WAVEPLAY.R32` | sound effects and music | **ours** (plays the CD music from ripped files) |
| `NETPLAY.R32` | multiplayer | **ours** (TCP/IP, no DirectPlay needed) |
| `Rtlib32.dll` | the engine: scripts, sprites, display, input, sound | **ours**, completely |
| `ISO.R32` | the game logic: units, AI, combat, interface | **in progress**: 819 of about 1,986 functions are ours |
| `dominion.exe` | the launcher | original |

Until everything is ported, the game runs under Wine (Linux) with our modules dropped into an installed copy. Next steps: finish `ISO.R32`, then our own executable on SDL3 (native on Windows and Linux, no Wine), then 64-bit. See `docs/PHASE0_ANALYSIS.md` for the full plan.

Along the way we document hidden settings, cheat codes, debug features and unused content found in the game: [`docs/HIDDEN_AND_UNUSED.md`](docs/HIDDEN_AND_UNUSED.md).

## Requirements

* The original game: the English Windows release, version 1.1, as disc images (`DOMINION1.iso`, `DOMINION2.iso`) or discs.
* Linux with Wine (tested with Wine 11 Staging), `7z` (p7zip), Python 3.
* To build the modules: CMake 3.20+ and the MinGW-w64 32-bit cross compiler (`i686-w64-mingw32-gcc`; on Fedora `mingw32-gcc`).
* Optional, for the music: `cd-paranoia` and the original discs (the music is CD audio, which disc images usually lack).

## Install and run (Linux)

```sh
# 1. Install the original game into its own Wine prefix (default ~/.local/share/dominion-remastered,
#    override with DOMINION_HOME). Applies the compatibility patches and writes Dominion.ini.
tools/setup_wine.sh /path/to/folder/with/the/iso/files 640      # or 800, 1024, 1280

# 2. Build our modules
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw32.cmake
cmake --build build

# 3. Install them over the game's own (the originals are kept as *.orig)
tools/install_modules.sh

# 4. Optional: rip the music from each original disc
tools/rip_music.sh /dev/sr0

# 5. Play
tools/run_wine.sh
```

`tools/install_modules.sh --restore` puts the original modules back. Step 1 alone (without our modules) gives the original game, patched to run under Wine; see [`docs/PHASE1A_WINE.md`](docs/PHASE1A_WINE.md) for what the patches fix.

Windows is not supported yet: our modules are Windows DLLs and should work there too, but this is untested. The native build will come with our own executable.

## For developers

* `src/`: our modules, one folder each. The engine (`src/rtlib`) and the game logic (`src/iso`) are replaced function by function: our DLL forwards to the original and patches each rewritten function into a jump to ours, so every part can be tested on its own (`DOMINION_ISO_ORIGINAL=name,...` switches parts back to the original for comparison).
* `tests/`: comparison tests against the original (byte-exact codec and blitter tests, scripted game sessions with screenshots, and a tick-by-tick log of every unit's state). See [`docs/TESTING.md`](docs/TESTING.md).
* `tools/`: setup, install, patch and analysis scripts.
* `docs/PHASE1B_MODULES.md`: how each module works, part by part, with the open items.

Reverse-engineering material (disassemblies, decompiler output) is kept out of the repository.

## License

The code in this repository is licensed under the GNU General Public License, version 3 ([`LICENSE`](LICENSE)). This covers our code only, not the game.

## Legal

*Dominion: Storm Over Gift 3* and its content are the property of their respective owners. This project is not affiliated with or endorsed by them. It contains no part of the original game; you need a legally obtained copy to use it. The code here was written for this project, reimplementing the original's behaviour from analysis of the program for compatibility and preservation.
