# Phase 1b — Replacing the game's modules with our own code

Goal: replace the original DLLs one at a time with our own source code, while the
rest of the game keeps running unchanged. Each replacement must look and behave
exactly like the original. Once all of `Rtlib32.dll` is ours, the game logic
(`ISO.R32`) is next, and then a native 64-bit build without Wine.

Order (smallest and most self-contained first):

| # | Module | What it does | Status |
|---|---|---|---|
| 1 | `BmpFont.r32` | Draws text with the game's bitmap fonts | **Done**: pixel-identical |
| 2 | `Smacker.r32` | Plays the cutscenes (Smacker video) | next |
| 3 | `WAVEPLAY.R32` | Sound effects (DirectSound) and CD music | |
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
* They don't use the C runtime. The game folder ships a 1998 `msvcrt.dll`, which Windows would load instead of the system one.

## How each module is checked
1. Screenshots of a fixed tour of menu screens with the original and with our module. They must be pixel-identical; only the animated mouse cursor may differ.
2. The same tour with a test build that draws nothing proves which screens actually use the module's code.
3. In-game use is checked during a match (`tools/mp_test.sh`).

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
