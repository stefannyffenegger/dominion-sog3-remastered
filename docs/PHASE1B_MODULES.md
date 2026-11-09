# Phase 1b — Replacing the game's modules with our own code

Goal: replace the original DLLs one at a time with our own source code, while the
rest of the game keeps running unchanged. Each replacement must look and behave
exactly like the original. Once all of `Rtlib32.dll` is ours, the game logic
(`ISO.R32`) is next, and then a native 64-bit build without Wine.

Order (smallest and most self-contained first):

| # | Module | What it does | Status |
|---|---|---|---|
| 1 | `BmpFont.r32` | Draws text with the game's bitmap fonts | **Done**: pixel-identical |
| 2 | `Smacker.r32` | Plays the cutscenes (Smacker video) | **Done**: own decoder, bit-exact |
| 3 | `WAVEPLAY.R32` | Sound effects (DirectSound) and CD music | **Step 1 done**: music from files. Sound effects still go to the original |
| 4 | `NETPLAY.R32` | Multiplayer (DirectPlay) | |
| 5 | `Rtlib32.dll` | Engine runtime: script interpreter, resources, sprites, DirectDraw | |

## Building
Needs `cmake` and the 32-bit MinGW compiler (`dnf install mingw32-gcc`).
```sh
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw32.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build
tools/install_modules.sh               # copy into $DOMINION_HOME/game, originals kept as *.orig
tools/install_modules.sh --restore     # back to the originals
```

How the modules are built:
* They are 32-bit Windows DLLs with the same file names, export names and ordinals as the originals.
* They call back into the original `Rtlib32.dll` through an import library made from `src/engine/rtlib32.def`. `src/engine/rtlib32.h` declares those engine functions.
  * Most engine functions are `cdecl`.
  * The `Sprite*` drawing functions are `stdcall`: the called function removes its own arguments. Getting this wrong corrupts the stack.
* They don't use the C runtime. The game folder ships a 1998 `msvcrt.dll`, which Windows would load instead of the system one. `src/common/minicrt.c` has the few functions we need (`memset`, `memcpy`, …).

## How each module is checked
1. Screenshots of a fixed tour of menu screens with the original and with our module. They must be pixel-identical; only the animated mouse cursor may differ.
2. The same tour with a test build that draws nothing proves which screens actually use the module's code.
3. In-game use is checked during a match (`tools/mp_test.sh`).
4. `tools/make_proxy.py` builds a logging stand-in for an original module. It records every call with its arguments in the Wine log, then passes the call on to the original. This is how we learn which functions the game really calls, and with what.

## 1. BmpFont.r32 (done)
* Source: `src/bmpfont/bmpfont.c`, about 450 lines replacing the original 1998 code.
* 12 exports:
  * The menu script (`DOMINION.bin`) calls `BFDrawText`, `BFDrawTextShadow`, `BFBrightWrite`, `BFClearRect`, `BFRepaintRect` and `BFStringPixelWidth`.
  * The game logic calls `BF_BlastText`, `BF_DrawTextShadow`, `BFDrawTextShadow` and `BFStringPixelWidth` (unit names, for example).
* Fonts are runs of glyph sprites starting at the space character. Text is drawn into an 8-bit bitmap, recolored, then the screen area is repainted.
* Original quirks are kept on purpose and marked `original:` in the source, so the screen looks the same. Bug fixes come in Phase 2. The quirks:
  * `BFBrightWrite` only marks the shadow of the first letter of each word.
  * `BF_DrawText` only knows the characters `' '` to `'Z'`.
  * Characters ≥ 0x80 are drawn as spaces, except in `BF_BlastText`.
* Changes compared to the original:
  * A missing string or resource now returns an error instead of crashing.
  * The animation delay in `BFBrightWrite` gives up its CPU time while waiting instead of spinning.

## 2. Smacker.r32 (done)
* The original wrapped RAD's `SMACKW32.DLL` (a closed library). The replacement doesn't need it.
* `src/smacker/smk.c` is our own decoder (about 500 lines, portable C, no Windows code).
  * Video: Huffman-coded 4×4 blocks with a palette that changes per frame.
  * Sound: Huffman-coded delta (DPCM) audio.
  * Tested natively on Linux: `tests/smk_compare.sh CINE/*.SMK` decodes every video and compares picture and sound byte for byte with ffmpeg. All 77 videos on the two discs are identical.
* `src/smacker/smacker.c` is the module the game loads:
  * It switches to a 640×480 16-bit screen, as the original did.
  * It draws each frame centered on the DirectDraw surface.
  * It streams the sound through the game's own DirectSound object.
  * Playback is driven by the engine's main loop through `SmackerUpdateHook`.
  * At the end, or on Escape, it posts the message the menu script waits for.
* Playback method: `Dominion.ini` → `[Dominion]` → `CinePlaybackMethod`.
  * 0 = original size
  * 1 = doubled
  * 2 = doubled and smoothed (default)
  * 3 = doubled and interlaced
  * Our smoothing is a simple average of neighboring pixels; RAD's exact filter isn't known.
* Checked in the game:
  * the logo videos at startup, and skipping each one with Escape;
  * the intro from the Cinematics menu;
  * the Merc campaign opening and briefing (videos from disc 2), followed by mission 1.
  * Recorded sound output matches the original module's loudness second by second.
* Not carried over:
  * The debug keys (+/− to change method, F for a frame counter) of an optional "VideoOutput" window that the game never creates.
  * The windowed (non-DirectDraw) mode, which the game never uses.

## 3. WAVEPLAY.R32 (step 1 of 2: music)
* The original plays sound effects through DirectSound and the music from the CD's audio tracks (MCI).
* **Step 1 (done):** our module implements the 8 CD functions and plays the music from the files ripped with `tools/rip_music.sh`. The other 45 exports are passed on unchanged to the original, which is installed next to it as `WAVEPLAY_orig.R32`.
  * Folder layout: `music/DOMINION1/track02.wav` … `track06.wav`, and the same under `DOMINION2`. `trackNN.cdda.wav` is also accepted.
  * "The disc in the drive" is whichever disc the game last asked for with `DSCheckCDIdentity`. Human and Darken missions ask for disc 1, Merc and Scorpion missions for disc 2. No disc is needed, and the no-CD byte patch isn't either.
  * Music streams from a background thread into a DirectSound buffer, so it keeps playing during loading.
  * When a track ends, the "track finished" message goes to the script from the engine's audio service call. The game then picks the next track itself.
  * Volume: the game sends 0–10,000. This is mapped to DirectSound decibels.
  * Checked in the game:
    * A Human mission played disc 1 track 4. A recording of the output matched the ripped track.
    * At the end of the track, the game moved on to track 5.
    * A Merc mission played disc 2 track 4.
  * Behavior changes:
    * The original timed the end of a track from when it started, so a pause made the track "end" early. Ours ends when the music really ends.
    * At the highest setting (10,000) the original's volume wrapped around to silence. Ours plays at full volume.
* **Step 2 (next):** sound effects. These are WAV files in `audio.rdf`, looked up by a CRC32 of the file name, played on 8–20 channels with volume, pan and frequency control. The `DSAudioService` call streams the longer sounds.
* Music files are WAV for now: our modules don't use a C runtime, and an Opus/Vorbis decoder needs one. Compressed music comes with the SDL3 audio layer.
