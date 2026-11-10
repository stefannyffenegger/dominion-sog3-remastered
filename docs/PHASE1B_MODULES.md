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
| 3 | `WAVEPLAY.R32` | Sound effects (DirectSound) and CD music | **Done**: music from ripped files, own sound-effect engine |
| 4 | `NETPLAY.R32` | Multiplayer (was DirectPlay) | **Done**: own network code, DirectPlay no longer needed |
| 5 | `Rtlib32.dll` | Engine runtime: script interpreter, resources, sprites, DirectDraw | **In progress**: our DLL wraps the original; 407 of ~1,450 engine functions rewritten |

Hidden, unused and unfinished things found on the way are collected in `HIDDEN_AND_UNUSED.md`.

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
   * With `--call` (for modules whose functions are all cdecl) it also logs each return value and the values behind pointer arguments, so output parameters show up. It works on our own modules too, which makes side-by-side comparison with the original easy.

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

## 3. WAVEPLAY.R32 (done)
Sources: `src/waveplay/waveplay.c` (sound effects and all exports), `cdmusic.c` (music).

**Music** (`cdmusic.c`). The original played the CD's audio tracks through MCI; ours plays the files ripped with `tools/rip_music.sh`.
* Folder layout: `music/DOMINION1/track02.wav` … `track06.wav`, and the same under `DOMINION2`. `trackNN.cdda.wav` is also accepted.
* "The disc in the drive" is whichever disc the game last asked for with `DSCheckCDIdentity`. Human and Darken missions ask for disc 1, Merc and Scorpion missions for disc 2. No disc is needed, and the no-CD byte patch isn't either.
* Music streams from a background thread, so it keeps playing during loading.
* When a track ends, the game's "track finished" message is sent and the script picks the next track.
* Checked in the game:
  * A Human mission played disc 1 track 4. A recording of the output matched the ripped track.
  * At the end of the track, the game moved on to track 5.
  * A Merc mission played disc 2 track 4.

**Sound effects** (`waveplay.c`).
* Where sounds come from:
  * `audio.rdf`, an archive of 725 WAV files with an index sorted by name; names are looked up ignoring case;
  * loose WAV files, which take precedence over the archive;
  * resources in the engine's `DOMINION.000`, PCM or IMA ADPCM, decoded with the engine's own `ima2pcm`.
* Keys: a sound is identified by its archive index, the CRC32 of its file name (same table as the original, no final inversion), or its resource id.
* Channels: 8–20, set by `Dominion.ini` `SoundBuffers`. Each has its own DirectSound buffer holding the whole sound. The original streamed sounds longer than ~2 s in pieces; the result sounds the same.
* Stop messages: when a sound stops (it ended, the game stopped it, or its channel was taken), the message given at play time is run at once, before the channel is freed. The script may call back into the module from that message.
* Volume and pan:
  * Volume is 0–10,000; the DirectSound volume is volume − 10,000. Pan is −10,000…10,000. Unit sounds are positioned by distance and side.
  * The master volume (`DSSetVolume(…, 0, v)`, which the game sets to 9600) goes on the primary buffer, as in the original. It therefore also covers the cutscene sound and the music.
* Unloading: sounds played with "auto-unload" are unloaded after 10 s without use.
* Checked: the same scripted session (logos, menu, Human mission with orders) was recorded with the original module and with ours. Loudness per 2 s matches (ratio 1.00), except for two sections:
  * The stereo ambient loop `AMB0001.WAV` is 3 dB louder in ours. The original created its buffers with DirectSound's 3D flag, which mixes stereo down to mono in the center (at least under Wine). Ours plays it in stereo.
  * The mission has music with ours. The original had none without a CD.
* Kept from the original on purpose (marked `original:` in the source):
  * A channel's priority is never stored, so when all channels are busy, priority-0 sounds are dropped and higher ones always take channel 0.
  * `rdfUnloadAll` only unloads archive file 0.
  * `DSSetMaxBuffers` also stops channel n−1.
* Fixed:
  * The unload queue removed the wrong entries after postponing one.
  * The primary output format the game asks for (22 kHz 8-bit) is no longer applied; modern systems mix at their own rate.
  * Music no longer "ends" early after a pause.
  * Music at the highest volume no longer goes silent.
* Music files are WAV for now: our modules don't use a C runtime, and an Opus/Vorbis decoder needs one. Compressed music comes with the SDL3 audio layer.

## 4. NETPLAY.R32 (done)
Sources: `src/netplay/netplay.c` (the 64 exports), `net.c` (our network code, replacing DirectPlay).

**What the original was.** A thin layer over DirectPlay with some logic of its own:
* Players have slot numbers 1–8. The host hands them out; slot 1 is the host.
* Its own small messages: slot assignment (types 3 and 6), "leave" (2), "player gone" (7), latency pings (4), and map transfer in chunks (1).
* It turns DirectPlay sessions and players into messages for the menu script.

Who uses it:
* The menu script calls most exports: connection types, session list, create/join, and the lobby's chat and settings (`DPSendMsg`/`DPReceiveMsg`).
* The game logic (`ISO.R32`) sends and receives in-game messages from its network thread (`DPReceiveMsgDirect`). It reacts to only two system messages: 5 (a player left) and 0x31 (connection lost).

**What ours keeps.** That logic is ported function by function. The menu script and the game see the same messages, in the same order, with the same values as with the original. This was checked by logging both modules' calls with `make_proxy.py --call` through the same lobby and match.

Details that matter:
* The script reads the player type field (always 1) of DirectPlay's "player created" message and uses it as a slot number. With 0 there, the host greeted every player twice, and the match start failed with "Internal program error: Invalid resource."
* DirectPlay sends "player data changed" and "session changed" to the local player as well. The script expects them.

**What replaces DirectPlay** (`net.c`). The model is DirectPlay's peer-to-peer session:
* The host listens on TCP and UDP port 47624 (DirectPlay's port; `[Dominion] NetPort` in Dominion.ini changes it).
* A joining game asks the host, then connects directly to every other game. Every game is connected to every other one. Messages go straight from sender to receiver, so messages between two players stay in order.
* Changes to a player are announced by the game that owns it. Player ids are unique without asking the host.
* If the host leaves, the others keep playing, as with DirectPlay. Nobody can join any more, because these sessions never moved the host role.
* LAN search: a UDP broadcast (plus 127.0.0.1, for a host on the same machine). An address entered in the game's address box is asked directly. "Host" or "host:port" both work.
* Firewall: the host needs TCP and UDP 47624 open. The other players accept connections on a random TCP port, or on `[Dominion] NetPeerPort`.

**Visible differences:**
* The menu now shows the game's own "Enter host address:" box (under SEARCH), and the game's own address on the session screen.
  * The original module asked DirectPlay what kind of address its TCP/IP connection uses. With the DirectPlay 6 files in the game folder the answer was "other" (4), so the script left addressing to DirectPlay's "Locate Session" dialog.
  * Ours answers "internet address" (1), the case the script was written for. It joins as soon as the search finds the game at that address. A blank address searches the LAN and lists what it finds.
* DirectPlay is no longer used. The `dplay*` overrides from Phase 1a only matter for the original module.

**Kept from the original** (marked `original:` in the source):
* `DPSetMaxPlayers`/`DPLockGame` pass a 0-based index where a slot number is expected when they drop players above a lowered maximum.
* `DPDisconnectPlayer` may pick the leaving slot itself as the new host slot.
* `DPReceiveMsgDirect` hands file-transfer chunks to code that reads the module's own buffer instead of the caller's.

**Fixed:**
* Slot 0, slot numbers outside 1–8 and missing size pointers no longer write outside the tables.
* The event that signals new messages is no longer named. Two games on one Windows machine shared it before.
* A file that can't be opened for sending is reported instead of being used.
* The address shown for this machine skips loopback addresses.
* `DPGetPlayerAddress` writes "a.b.c.d" instead of DirectPlay's binary address block.

**Not carried over:** MPlayer.com and DirectPlay lobby launching (`DPCheckForConnect` reports none, `DPIsMPlayer` is false), and modem and serial connections.

**Tested** (two or three games on one machine, `tools/mp_test.sh`):
* Host and join, with an address and by LAN search; lobby; start of 2_2Prong. Messages flow in the match.
* Three players in one lobby and in a match on 3_Death Valley. The second and third game connected directly.
* The joiner quits during a match: the host gets "player 2 left" and wins.
* The host quits during a 3-player match: both others see "HOST HAS BEEN DEFEATED!" and keep playing.
* Not yet tested: separate machines, a full match to the end, map transfer.

## 5. Rtlib32.dll (in progress)
The engine is far bigger than the other modules: 430 exports and about 1,800 functions (73,000 lines of decompiled C, part of it a statically linked C runtime). Only 112 exports are used from outside; the menu script, the interpreter and most of the engine run inside it. So it is replaced function by function from the inside, the way re3 (GTA III) and DevilutionX started:

* **The shell** (`src/rtlib/`): our `Rtlib32.dll` loads the original as `Rtlib32_orig.dll` and forwards all 430 exports to it (`rtlib32.def`).
* **Rewrites** are listed in `ported.h` with the original's address. At load time `hooks.c` patches the first bytes of each original function into a jump to ours. Outside callers and the engine's own internal calls then both run our code.
* Our functions keep the original's calling convention (mostly stdcall, read from each function's `ret` instruction) and its data layouts. The rest of the original still reads the same structures.
* Original globals and not-yet-rewritten functions are reached through `orig.h`: `G(type, address)` and `ORIG(type, address)`. Addresses are the original's own, as in the decompiled code.
* **A/B testing:** `DOMINION_ORIGINAL=ResOpenFile,ResLoadResource` (or `all`) leaves those functions original. The log says how many were switched in ("RTLIB: 86 functions rewritten").
* **Finding the code that runs:** `DOMINION_TRACE=<file>` (a Windows path, e.g. `Z:\home\...`) takes a list of original addresses, one per line with an optional name. Each listed function logs `RTLIB: first call <address> <name>` the first time it runs, then runs normally. The lists come from the Ghidra function list (in `work/`, not published).
* **Install:** `install_modules.sh` copies the unpatched original to `Rtlib32_orig.dll` and applies the `palette` byte patches to that copy (`patch_game.py` patches `Rtlib32_orig.dll` when it exists). `--restore` puts the original back.
* When every function is rewritten, the exports point to ours and the original is no longer loaded.

### Resource files (done, `res.c`, 86 functions)
`DOMINION.bin` is the main resource file; `DOMINION.000` holds its data. Layout (also in `res.h`):
* **Header** (0x1dc bytes): magic 0x4c37, version 0x102 (version 2 is an older, shorter layout that is converted on load), title, script buffer bounds, counts, a checksum (all header bytes add up to 0), and 15 "key resources": offset and size of the index, name tables, variables, constant strings, script area, palette and more.
* **Index**: 10-byte entries (type, data file number, offset, size) in blocks of 6,500. `DOMINION.bin` has 55,134 stored entries plus 4,000 free slots for resources the script creates at run time.
* **Types** 1-12 (bitmap, data, file, frame, ground, midi, model, mproto, obj3d, oproto, table, wave) are read from the data files `.000`, `.001`, ...; types 13-22 (movie, array, cell, group, palette, queue, script, sprite, text, tile) live in the script area, which is kept in memory.
* **Bitmaps** are stored as a 0x18-byte record (width, height, flags, ...) plus packed pixels, and unpacked into the engine's 0x24-byte DIB header + pixels. Run-length coded sprites (flag 0x80) stay coded.
* When playing from CD, data files could be copied to the hard disk; `dominion.ini [resource]` lists the copies so leftovers are deleted at the next start.

Checked: the menu tour is pixel-identical to the original, and Human mission 1 loads and plays with the same picture. Kept from the original: `ResExtractFile` reads a file's 6-byte head through the main file instead of the data file; `ResGetTitle` doesn't check for a missing file.

### Utilities (done except floating point, `utils.c`, 40 functions)
Memory (`winmalloc` & co.: Windows global memory plus a byte counter), strings, paths (`GetPathAndFile`, `MakeFullPath`), debug output (`DebugStr` goes to a debug window and to a hook the launcher sets), processor detection, and integer trigonometry: angles in tenths of a degree (0-3599), sine and tangent from tables scaled by 10,000.
* The sine table is close to `round(sin(x) * 10000)` but differs in 93 of 901 entries, all near .5, so it was computed with lower precision (probably single-precision floats). For now our code reads the table from the original.
* Left original for now: `GetAngle`, `Collide`, `Distance`, `SquareRoot` and `GetIntersect` use x87 floating point through the C runtime, and unit movement may depend on their exact results. They move together with the C runtime's math. Also `ExternalCall` and `LinearSearch` (hand-written assembly).
* Checked: menu tour identical to the all-original run (`DOMINION_ORIGINAL=all`), Human mission 1 identical.

### Compression (done, `codec.c` + `compress.c`, 16 entry points)
`codec.c` is plain portable C, tested natively; `compress.c` connects it to the engine's exports.
* **Bitmaps** are a series of blocks, each a 2-byte header (method in the top 3 bits, data size below) and the data:
  * raw bytes;
  * LZW with 10, 11 or 12-bit codes, as sub-chunks of a 2-byte length plus codes. Each sub-chunk covers up to 8 KB of output and starts with an empty dictionary. Codes are packed MSB first; 256 and up are new strings, `2^bits - 1` ends a sub-chunk;
  * 0xE000 ends the stream.
  The original's packer can also write a run-length method (0x2000) that its own unpacker refuses. Its setting is never changed from 1 (LZW with 11-bit codes), so the game files only contain raw and 11-bit LZW blocks.
* **Sound** is standard IMA ADPCM (4 bits per sample) after a 10-byte header. The original keeps the step index times 8 and precomputes its tables; they match the standard ones exactly, so ours computes them.
* The original's decoder core is hand-written assembly with arguments in registers. It can't be redirected on its own, but nothing outside the codec uses its state, so the codec is replaced as a whole and the old internals are no longer called.
* **Checked byte for byte** (`tests/codec_compare.sh <unmodified Rtlib32.dll> <game folder> [wine prefix]`):
  * all 29,211 bitmaps of `DOMINION.bin` unpacked by ours (natively) and by the original engine (under Wine);
  * all 876 LZW blocks in the game files re-packed by our packer;
  * IMA unpacking and packing, mono and stereo.
  The menu tour and Human mission 1 are identical too.
* **Register rule found here:** some original functions are hand-written assembly that changes EBX (the IMA functions, the sprite run-length expander). Our compiled code expects EBX, ESI and EDI to be kept. Where our code still calls such an original function, it goes through `orig_call2_unsafe` (`hooks.c`), which saves them. The resource code's call to the sprite expander needed this.

### Audio (the part the game runs, `audio.c`, 27 functions)
The engine has its own sound system: a software wave mixer on `waveOut`, a MIDI sequencer and CD audio over MCI (about 200 functions). Traced through the menus and Human mission 1 (`DOMINION_TRACE`), only 11 of them ever run, and the game never opens a wave, MIDI or CD device. Dominion does all its sound and music through `WAVEPLAY.R32`, which hooks into `AudioService`.
* Rewritten: library setup (settings from `dominion.ini [audio]`, device checks, the CD mixer line), the audio context, `AudioService` (calls WAVEPLAY's routine every main-loop pass), `AudioSetServiceProc`, `AudioGetWaveDeviceInfo`, and focus changes, mute and pause (`AudioActivate`, `AudioMute`, `AudioPause`, `AudioPauseMidi`), `AudioExitLibrary`.
* Setup seeds the engine's own C runtime random generator (`srand(timeGetTime())`); ours calls that same `srand`, so the random sequence stays the same.
* The mixer, sequencer and CD player stay original. They only run once a device is open, which doesn't happen in Dominion. Whether to rewrite them or replace them when the original goes away is decided then.
* Checked: menu tour and Human mission 1 identical to the all-original run; the mission music starts through WAVEPLAY as before.

### Display layer (done, `display.c`, 62 functions)
The sprite engine's DirectDraw code: creating the DirectDraw object, setting the 8-bit display mode (and the "not supported" warning), the flip chain (double or triple buffering, decided by video memory), the palette, page flipping with timing, the back buffer locked as the "screen" bitmap the blitters draw into, the frame around a smaller view, and the interface panel. The panel is a separate surface that is only redrawn where its sprites changed (dirty rectangles) and copied below the play area every frame.
* The sprite engine's state (over 4 KB) is still shared with original code, so its fields are reached at the original's offsets. The DirectDraw objects sit in a 0xb4-byte block of their own (`DisplayDD`).
* The calls use DirectDraw's C interface instead of raw vtable offsets, so this file also documents exactly what the engine asks of DirectDraw. That is the list the SDL replacement has to cover.
* One of the Phase 1a `palette` byte patches sat in this code (`display_set_size`). The rewrite has the fix built in: the display is always treated as a palette device. The other four patched spots are in code not rewritten yet.
* Left original: the background setup at 0x10049a02, which calls hand-written blitters; and a debug assertion.
* Checked: menu tour and Human mission 1 identical to the all-original run.

### Sprite objects, part 1 (`sprite.c`, 54 functions)
Creating and destroying sprites, their cel lists and animation ranges, bounds, position and speed, pausing, the per-frame animation pass (`SpriteAnimate`) and the getters and setters.
* A sprite is a 0xf4-byte block with a 0x4a-byte info block. Sprites sit in two doubly linked lists between a head sentinel (level -90000) and a tail sentinel (level 90000): one sorted by drawing level, and the animation list that `SpriteAnimate` walks. At most 4,000 sprites.
* Sprites on the interface panel mark their old and new rectangles dirty around each animation step (see the display layer).
* Checked: menu tour and Human mission 1 identical to the all-original run.

### Sprite objects, part 2 (`sprite.c`, 50 functions)
Mouse picking, replacing and copying cels, drawing levels, flips, the remaining setters, moving a sprite between the animation list and out of it, screen shake and the scroll positions.
* Picking: `SpritePicked` searches the level list from the top for a sprite with a pixel under the mouse; a sprite can instead be picked anywhere in its bounds, and its pick mode can make it pickable while hidden, or never. The pixel test (0x10062d30) is ordinary compiled code and stays original for now. Ghidra decompiled the three picking functions as `fastcall` returning 64 bits; the disassembly shows plain `stdcall`.
* `SpriteSetLevel` moves a sprite in the level list (searching from its old place) and re-inserts it in the animation list.
* `SpriteSetFlip`, `SpriteSetProcessMsgs` and `SpriteSetRedraw` are `cdecl`, unlike the rest; `SpriteSetGraphicsQuality` takes two unused arguments.
* Three functions only log "UNIMPLEMENTED" (see `HIDDEN_AND_UNUSED.md`).
* Checked: menu tour and Human mission 1 identical to the all-original run (apart from the usual animation timing).

### View and palette (`view.c` 34 functions, `display.c` +19)
The background (a bitmap, a tile map or a fill color), the screen rectangle and clip boxes, scrolling, repainting, composing sprites into the background, bitmap helpers and screenshots; and the palette (GDI palette with Windows' static colors, palette animation, fades, the DirectDraw palette).
* Sprite coordinates are relative to the center of the background. Scrolling moves every scrollable sprite's rectangles; the horizontal scroll position is always kept odd.
* With GDI, scrolling moves the window contents with `ScrollDC` and repaints only the uncovered edges; a tile map background is scrolled in memory the same way.
* The fade itself (0x10062ed8) is hand-written assembly and stays original, called through the register-safe wrapper. So do the `SpriteDib*Merge`/`Copy`/`Diff`/`Set` wrappers, which only forward to blitters.
* Checked: menu tour and Human mission 1, now with scrolling right and back left, identical to the all-original run (apart from animation timing).

### Script queues (`queue.c` 14 functions, `view.c` +5)
Each sprite can run a list of commands: animate cels, move along a line or a curve, jump, flip, wait, loop, show or hide, draw itself into the background, change priority or speed, and send messages to the game. `sprite_step` runs the current command; commands that finish at once go straight on to the next.
* A command is a 16-bit type and packed data; the sizes are in a table filled at startup (type 2 and type 0x15 have variable sizes). Most values carry a flag byte that makes them an index into the game's variable table.
* Loops (type 6) hold a counter and a jump target; `SpriteBreakLoops` makes every loop fall through.
* Messages (types 0x14-0x16, and 0x13 when an animation ends) are passed to the game's message procedure. If the game changes the sprite meanwhile, the queue doesn't resume on its own.
* The commands that do the actual moving and animating (types 1, 2, 5, 9, 10, 0x17) are still original; the per-command helpers return a full register that is passed on unchanged.
* The screen bitmap (plain memory, a GDI DIB section, or the locked DirectDraw back buffer) moved to `view.c`.
* Checked: menu tour and Human mission 1 match the all-original run. The edge-scrolling screenshot lands on one of two positions, in the original's runs too.
* Next: showing, hiding and moving sprites, the drawing pass, then the hand-written blitters.

### What's left
| Part | Functions | Decompiled lines |
|---|---|---|
| Application core and script interpreter (`Rt*`) | ~600 | 21,400 |
| Sprites and blitters (`Sprite*`, partly hand-written assembly) | ~220 | 14,000 |
| Audio mixer, MIDI, CD (unused by Dominion, see above) | ~170 | 4,800 |
| C runtime (not ported; replaced by the system's) | ~430 | |

