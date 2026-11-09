# Dominion: Storm Over Gift 3 — Remaster Plan

Phase 0 analysis of the original game files and feasibility of each phase.
Date: 2026-11-09. Source: `Dominion_Storm_Over_Gift_3_Win_ISO_EN/Game Files/DOMINION1.iso` and `DOMINION2.iso` (Sold Out re-release, English).

## 1. Summary

| Phase | Goal | Feasibility | Rough effort |
|---|---|---|---|
| 1a | Runs on modern Linux/Windows through compatibility layers (Wine / DirectDraw wrapper) | **High** | Days to a few weeks |
| 1b | Native build without Wine (own code over SDL3) | **Medium**: possible, but means rebuilding the engine | Many months, done step by step |
| 2 | Bug fixes, modern multiplayer, AI players in multiplayer | **Medium**. Easy once 1b is far along; hard as binary patches | Months |
| 3 | "Definitive Edition" graphics and UI | **Medium-low**, mostly limited by art production | Open-ended |

The main finding is that the game is unusually well suited to a step-by-step rebuild:

* The game code is split into **separate DLLs with named exports**. These are the seams where our own code can replace the original, one module at a time.
* The main game DLL `ISO.R32` is a **debug build**: debug C runtime, frame pointers in every function, source-file paths, and **325 named exports**. That makes it much easier to decompile than a typical optimized retail binary.
* Game balance lives in **plain CSV tables** (`unittype.tbl`, `weapon.tbl`, `economy.tbl`, ...).
* The game already **runs under Wine 11**. It initializes DirectDraw, DirectSound and DirectPlay, then stops at the CD check.

## 2. What is on the discs

### Disc layout
* **CD1** (526 MB): installer (`SETUP/DATA1.CAB`, InstallShield 5), 46 Smacker cutscenes (`CINE/*.SMK`, Human "H_" + Darken "D_"), `Manual.pdf`, a bundled DirectX 5/6 redistributable, IE5, Acrobat, and the "Sold Out" launcher (VB5 + Flash). Only the installer, the cutscenes and the manual matter.
* **CD2** (355 MB): cutscenes for the other two factions (Merc "M_", Scorpion "S_"), plus a Daikatana demo trailer.
* **Small `.DAT` files on each disc** (`ASWN.DAT`, `BFHJ.DAT`, `NBBV.DAT`, `UIQW.DAT`, 3 bytes each): disc-identity markers for the CD check.
* **Music is missing.** The game played music from **CD audio tracks** (`xStartCDJukebox`, `AudioPlayCD`, `DSPlayCDTrack`). `.iso` files cannot hold audio tracks, so these images have no music. We need the original discs or a BIN/CUE rip, or else new music.

### Installed game (`DATA1.CAB`, 895 files, 160 MB), unpacked with `tools/is5x.py`

| Component | Files | Notes |
|---|---|---|
| Launcher | `dominion.exe` (75 KB code) | Thin host. Imports only `Rtlib32.dll` |
| Engine runtime | `Rtlib32.dll` (443 KB code, 430 exports) | Ion Storm's in-house runtime: script interpreter, resource files, sprites, palettes, DirectDraw, audio (wave, MIDI, CD), compression. Sources under `D:\DEV\RUNTIME`, `D:\DEV\SPRITE` |
| Game logic | `ISO.R32` (1093 KB code, ~2400 functions, 325 exports) | Isometric engine, units, pathfinding, AI ("Arnold"), map editor, network game logic, fog of war, save/load. Source path `E:\dev\rtplugin\iso\src\` |
| Networking | `NETPLAY.R32` (64 exports) | DirectPlay (`DPLAYX`) + `WSOCK32`: TCP/IP, IPX, modem, serial |
| Audio | `WAVEPLAY.R32` (55 exports) | DirectSound |
| Video | `Smacker.r32` + `SMACKW32.DLL` | RAD Smacker 1 playback |
| Fonts | `BmpFont.r32` | Bitmap fonts |
| Game-flow script | `DOMINION.bin` (1.5 MB) | Compiled RtLib script: menus, campaign flow, CD checks, options, lobby UI |
| Graphics archive | `DOMINION.000` (54 MB) | RtLib resource file (compressed 8-bit bitmaps / sprites) |
| Sound archive | `audio.rdf` (45 MB) | "Resource Data File", Ion Storm 1997. Contains plain RIFF WAVs (easy to extract) |
| Tilesets | `ARCTIC/DESERT/GRASS/MOLTEN.TL2` + `.tlh`, `fogmask.tlx` | Tagged binary ("TL2", "TLX") |
| Palettes | `colormap.dat`, `clut40.dat`, `blut.dat` | 8-bit palette lookup tables (shading, team colors) |
| Campaign | 48 missions: `Human/Darken/Merc/Scorp 01–12` `.wxp` (map) + `.sdc` (scenario script) | 4 factions × 12 |
| Multiplayer maps | 58 maps, 2–8 players, `.wxm` + `.sdc` | |
| Rules tables | `unittype, weapon, economy, upgrade, depends, techlvl, ...tbl` | CSV / text |
| Mplayer.com client | `System/`, `Programs/`, `Art/`, `Commands/` | Lobby service shut down long ago. Drop it |
| Config tool | `DominCfg.dll` | Settings dialog |
| Bundled system DLLs | MFC40/42, msvcrt, DPlay, OLE... | Not needed on modern systems |

### Technical profile (circa 1998)
* Built with **MSVC (5.0 era)**, 32-bit x86, build dates June/July 1998. In-game version string `v1.1`.
* **Graphics:** DirectDraw (IDirectDraw2), **8-bit palettized**, software sprite blitting. `VideoMode` = 640 / 800 / 1024 / 1280. Palette effects (fades, shading tables, team-color remaps) are central to the look, so any modern renderer must reproduce them exactly. Use a palette-to-RGBA shader, not a naive conversion.
* **Audio:** DirectSound, MCI CD audio, MIDI.
* **Network:** DirectPlay lockstep-style sync (`SyncThreshold`, `NetworkPing`, `IsoInitCRCCheck`, `IsoSetNetworkTimer`). Multiplayer modes: TCP/IP (direct IP), IPX, modem, serial, Mplayer lobby.
* **AI:** Real skirmish-style AI exists (`AIAPIInitArnold`, `IsoAILoop`, base-defense / threat-group / build-placement logic with detailed debug logs). But computer players are only set up by scenario scripts (`SET_COMPUTER_PLAYER` in `.sdc`). There is no option to add AI to a multiplayer or custom game. **Adding AI players to multiplayer means reusing existing code, not writing an AI from scratch.**
* **Copy protection:** `IsoCopyProtection`, `IsoCheckForCD`, CD-swap prompts per faction. Mapping the disc as a Wine CD drive with the right volume label is **not** enough. The check needs patching or a better emulated drive.
* **Debug facilities shipped:** `IsoOpenDebugFiles`, `IsoDebugFile`, AI logs, profiler hooks, `IsoCheckCheatCodes`, built-in map editor (`IsoMapEditDisplayLoop`, `EDITPATH_*`). These help with reverse engineering and with diagnosing crashes.

### Wine test (Wine 11.20 Staging, Fedora 44)
In an isolated prefix with a virtual desktop, the game loads all modules (`Rtlib32`, `waveplay.r32`, `DPLAYX`, `DDRAW`, `DSOUND`) and shows its own dialog: *"Please insert the 'DOMINION1' CD in the CD-ROM drive"*. No crash or missing-DLL errors.

## 3. Phase plans and feasibility

### Phase 0 — Analysis (mostly done; remaining work)
1. ✅ Unpack discs and installer; inventory files; identify architecture and tech stack.
2. ⏳ Load `ISO.R32`, `Rtlib32.dll` and the `.R32` modules into **Ghidra**, apply export names and the debug-runtime signatures, and map the module interfaces (call graph between modules).
3. ⏳ Document file formats, in priority order: RtLib resource file (`DOMINION.000`, `audio.rdf`) → sprites and palettes → `TL2/TLH` tiles → `WXM/WXP` maps → `SDC` scenario scripts → `DOMINION.bin` script bytecode → saved games. Write a Python extractor and viewer for each (the start of a modding toolkit).
4. ⏳ Get a playable baseline under Wine (get past the CD check) and record reference footage and timings. These are the "feel" baseline that every later phase is compared against.
5. ⏳ Collect known crash reports and patches (the game reports `v1.1`; check whether a later patch existed).

### Phase 1 — Run on modern platforms
**1a. Compatibility route (quick win, low risk)**
* Linux: Wine with a virtual desktop or Gamescope. Windows 10/11: a DirectDraw wrapper (e.g. cnc-ddraw or dxwrapper) for 8-bit DirectDraw, plus enabling the optional "DirectPlay" Windows feature.
* Replace the CD check with a small, documented binary patch (or a hook DLL).
* Replace CD-audio music with OGG files through a `winmm` hook wrapper.
* Ship a launcher and installer that unpacks the user's own discs (`tools/is5x.py`). Output: a playable game on both OSes within weeks. **Not native**, but it gives everyone a test bed right away.

**1b. Native route (the actual goal): replace modules one at a time**
The DLL split makes a "ship of Theseus" approach possible: replace one original module at a time with our own source code, keeping the game playable after every step.
1. **Drop-in replacement DLLs (still 32-bit Windows binaries, tested under Wine and Windows):** `BmpFont.r32` (12 exports) → `Smacker.r32` (17, via FFmpeg or libsmacker) → `WAVEPLAY.R32` (55, via SDL3 audio) → `NETPLAY.R32` (64, via plain UDP/ENet instead of DirectPlay) → **`Rtlib32.dll`** (430 exports: DirectDraw → SDL3 with a palette shader, file I/O, script interpreter). After this step, everything below the game logic is ours and portable.
2. **Decompile `ISO.R32` into C/C++** function by function. The debug build and frame pointers make this unusually clean. Verify against the original by running both and comparing deterministic game state (same random seed, same inputs, same results).
3. Once no original binaries remain, build **native 64-bit Linux and Windows** executables (CMake + SDL3). The original game data, read from the user's discs, stays a runtime requirement.

Feasibility: **medium**. About 5,000 functions in total (ISO.R32 ~2,400, Rtlib32 ~1,800). Comparable community projects (Devilution/DevilutionX for Diablo, re3, OpenRCT2) took small teams 1–3 years. AI-assisted decompilation shortens that a lot, but this remains the bulk of the project. The step-by-step plan means each milestone produces something playable, so the project never stalls in a half-done state.

*Alternative considered:* a clean-room engine rewrite (like OpenRA) that only reads the data files. It is more flexible for Phase 3, but much more likely to lose the original gameplay "feel" (unit timings, pathfinding quirks, AI behavior). Not recommended as the main route. It can still happen later on top of decompiled, well-understood code.

### Phase 2 — Technical updates
* **Crash fixes:** the debug runtime's heap checks and asserts plus Wine's debug channels let us catch memory corruption and bad pointers in the original code early. Phase 0 and 1 crash logs build the bug list. Fixes are applied in decompiled source (best) or as patches (interim).
* **Multiplayer:** replace DirectPlay with our own UDP lockstep (ENet) and direct IP. Add an optional lobby or relay server for NAT traversal, and rejoin after disconnect (the hooks `IsoInitReconnectAndPauseWindow` already exist). Keep the deterministic lockstep model and add checksum verification (`IsoInitCRCCheck` exists).
* **AI players in multiplayer and skirmish:** let the game setup assign the existing "Arnold" AI to empty slots (the AI already runs on every machine in lockstep). Improving the AI comes later.
* **Quality of life:** widescreen and arbitrary resolutions (the engine already supports up to 1280 wide, so it is resolution-flexible), windowed and borderless modes, rebindable hotkeys, larger selection limits and control groups, faster game speeds, modern save files.

Feasibility: medium. Mostly depends on how far Phase 1b has progressed. Binary patching alone would make these changes painful.

### Phase 3 — "Definitive Edition" modernization
* **Renderer:** go from palette-indexed software blitting to GPU sprite batching, with zoom, smooth scrolling and high refresh rates. Keep the "classic graphics" toggle (like AoE DE).
* **Art:** the hard part is content, not code. Options, cheapest first:
  1. Upscale existing sprites and tiles (2×/4×, AI-assisted with manual cleanup) while keeping the palette tricks.
  2. Re-render units from new 3D models into high-resolution sprite sheets (the approach AoE DE used).
  3. Fully new art.
* **UI:** scalable UI at 1080p, 1440p and 4K, modern sidebar and minimap, tooltips.
* **Audio:** remastered or replacement music (needed anyway because the music is missing), higher-quality sound.
* **Cutscenes:** upscale the Smacker videos (AI video upscaling) and add subtitles.

Feasibility: medium-low as a whole. The tech is straightforward once 1b is done; art volume and quality decide the outcome.

## 4. Risks and open questions
* **Legal:** the game is copyrighted (Ion Storm / Eidos, now owned by other companies). The safe model is the one OpenRA, DevilutionX and OpenRCT2 use: publish only our own code and require players to provide their original discs. Never redistribute original binaries, data, videos or music. Check this before any public release.
* **Missing music:** needs the original discs or a BIN/CUE rip with audio tracks, or new music.
* **The game-flow script (`DOMINION.bin`):** its bytecode has to be understood (or the interpreter reused) to keep menus and campaign flow. Medium risk.
* **Determinism:** multiplayer and replay validation depend on bit-exact simulation. Keep integer math and random-number behavior identical while decompiling.
* **Tooling needed:** Ghidra (+ Java 21), CMake, SDL3, and permission to build open-source helpers (unshield, cnc-ddraw, libsmacker).

## 5. Proposed next steps
1. `git init` the project; keep the original discs **outside** the repo (add them to `.gitignore`).
2. Install Ghidra + Java and CMake; import all modules; produce an interface map of the module exports.
3. Get past the CD check under Wine → playable baseline + reference recordings.
4. Write extractors for `audio.rdf` and `DOMINION.000` (resource format) → asset viewer.
5. Start Phase 1b with the smallest modules (`BmpFont.r32`, `Smacker.r32`) to prove the drop-in-replacement approach.

## Appendix
* `tools/is5x.py`: InstallShield 5 cabinet extractor (pure Python). Usage: `python3 tools/is5x.py <DATA1.CAB> <outdir> [--list]`
* `docs/exports_ISO.R32.txt`, `docs/exports_Rtlib32.txt`: exported function names
* `docs/data1_cab_filelist.txt`: full installer file list with sizes
