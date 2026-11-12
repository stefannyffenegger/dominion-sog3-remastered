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
| 5 | `Rtlib32.dll` | Engine runtime: script interpreter, resources, sprites, DirectDraw | **In progress**: our DLL wraps the original; 1,384 of ~1,450 engine functions rewritten |

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
The engine is 7th Level's "Runtime Player 1.2" (1996), which Dominion inherited when Ion Storm took the game over from 7th Level; see `HIDDEN_AND_UNUSED.md`. It is far bigger than the other modules: 430 exports and about 1,800 functions (73,000 lines of decompiled C, part of it a statically linked C runtime). Only 112 exports are used from outside; the menu script, the interpreter and most of the engine run inside it. So it is replaced function by function from the inside, the way re3 (GTA III) and DevilutionX started:

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
* One of the Phase 1a `palette` byte patches sat in this code (`display_set_size`). The rewrite has the fix built in: the display is always treated as a palette device. Two more were in the engine-state code (built in there too); the last two are in `RtSetDisplayMode` (application core, not rewritten yet).
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
* **Movie frames** (script command 0x17, `SpriteSetMovieProc`) and the blit-effect settings are ours too.
* **Messages** (27 more functions): `SpritePostMessage` builds a queue command from C and appends it (starting the queue if idle); `SpriteSendMessage` runs it at once. The queue grows in 128-byte steps.
* Checked: menu tour and Human mission 1 match the all-original run. The edge-scrolling screenshot lands on one of two positions, in the original's runs too.

### Engine state and sprite display (`spritelib.c`, 22 functions)
Creating, resetting and freeing the engine state, the screen modes of the drawing bitmap, and showing, hiding, moving and redrawing sprites, plus the cel-animation command.
* The engine state is 0xff4 bytes. The first one is a static block inside the DLL; further ones are allocated.
* `SpriteInitLibrary` reads hidden `dominion.ini` settings (see `HIDDEN_AND_UNUSED.md`) and remembers Windows' system colors.
* Redrawing a sprite: if its new bounds overlap the old ones, one pass redraws both; otherwise the old place is redrawn without the sprite, then the new one.
* Two more spots of the Phase 1a `palette` patch were in this code (`SpriteInitLibrary` and the desktop-size function); the rewrite has the fix built in. Without it the screen stays black.
* Checked: menu tour and Human mission 1 match the all-original runs.

### Drawing pass (`draw.c`, 7 functions)
Redrawing a rectangle: the background (bitmap, tile map or fill color) into the screen bitmap, then every visible sprite over it in level order, then the result to the window (GDI) or the end of the DirectDraw frame.
* Sprites above level 24,500 are drawn in a second pass, after the tile overlay, so they stay on top of it.
* With the interface panel on, the panel's sprites go into the panel's own bitmap, shifted up by the play area's height.
* A sprite can have its own draw procedure, a clip box, a color map and an effect (which picks a different blitter).
* The blitters themselves are still the original code: the run-length merge (0x10059e74) and the effect merge (0x1005adb3) are compiled C; the copy and fill routines are hand-written assembly called through the register-safe wrapper.
* Checked: menu tour and Human mission 1 match the all-original runs. Dominion draws through DirectDraw, so the GDI paths (copying to the window, stretching, 16-bit modes) are ported but not exercised by these tests.

### Assembly routines in C (`blit.c`)
The sprite engine's hand-written assembly, rewritten in portable C one routine at a time: rectangle intersect and union, palette fade, 8-bit fill, and the copy routine (raw or run-length source, with flips).
* `tests/blit_compare.sh <unmodified Rtlib32.dll>` builds a Windows program that runs each routine and the original side by side on random input under Wine and compares the results byte for byte. A deliberately broken version is caught.
* The C keeps the original's quirks where they change results: the copy routine advances one pointer with a 16-bit add (`inc si`), copies 512 bytes for a zero-length literal, reads run-length row offsets as 32 bits from a 16-bit table (so its multi-row run-length path can't have worked), and doesn't really mirror raw sources.
* Our code now calls these directly, without the register-safe wrapper.
* **The merge blitter** (0x10059e74), which draws every sprite: for run-length coded sprites it is now our C, with its helpers (copy, mirrored copy, color map, shadow, dithered shadow and the fast path for unclipped sprites on a top-down screen). Plain bitmaps still go to the original assembly (0x10060c14, 5 KB). Byte-exact on 30,000 random cases, including the destination area *around* the bitmap.
* Sprite format: rows of 16-bit-aligned data, bottom row first; each row a segment count, then segments of 16 bits (type in the top 5, length in the low 11): 0 transparent, 1 shadow, 2 also transparent (an effect left unfinished), 3 pixels follow.
* The shadow and color-map routines look up 32-bit table entries and combine four at a time; that equals a per-pixel lookup because the game's tables only hold values below 256 (checked in `ISO.R32`), which our C relies on.
* Found while wiring it in: with no color map, the 5 KB routine jumps into a routine that removes 9 arguments instead of 10. The original only survives because its caller resets the stack from its frame. Our register-safe call wrapper now does the same.
* The game screenshots drift by an animation frame sometimes (the briefing background and idle units animate); a dense series of screenshots of the unmodified game contains our briefing frame exactly.
* **The plain merge** (0x1005fcec, and 0x10060c14 with a color map; 9 KB of assembly) is C too: plain bitmaps, and a second run-length format (the copy routine's, with special values 0 transparent, 1 shadow, 2 highlight, 3 spot color, and "effect pixel" runs). Byte-exact on 30,000 random cases with all clip variants, flips and color maps.
* Its quirks are kept: plain bitmaps are drawn in pixel pairs where a right pixel that transforms to 0 is sometimes written and sometimes not; mirrored, the odd last pixel takes its shadow from the pixel to its right; 8-bit loop counters make some zero counts mean 256.
* **The effect merge** (0x1005adb3, from `SpriteSetBlitFX`) is C too, with its helpers: effect 1 blends the sprite with the screen through a 256 x 256 table (or, with the screen-door setting, copies every second pixel); effects 2 and 3 look pixels up in an effect table. Its three assembly routines (blend, blend through a color map, screen door) are C as well. Byte-exact on 20,000 random merges and 6,000 helper runs.
* Two values the original passes are uninitialized stack memory: the shadow-dither row and the screen-door row of unclipped sprites. Each stays constant for the whole sprite; we use 0. Those cases are left out of the comparison.

### Sprite parts, paths and pixel tests (`spritelib.c` +7, `sprite.c` +4, math in `blit.c`)
* **Parts:** script command 2 shows several cels at once as one sprite (a unit's body and turret, say). Each part gets its own rectangle; if the first parts stay the same, only the changed ones are redrawn.
* **Paths:** commands 9 and 5 move a sprite along a line or a curve in a fixed number of steps while its cels animate; command 10 jumps to a position. The point math was assembly: the line rounds to nearest, the curve is a quadratic in 10-bit fixed point.
* **Pixel tests** for picking sprites with the mouse: whether a bitmap is solid at a point, for both run-length formats and plain bitmaps.
* The math and the pixel tests are byte-exact against the original (20,000 random cases each).

### Bitmap operations and hit detection (`draw.c` +12, `sprite.c` +6)
* The exported bitmap calls (`SpriteDibCopy`, `SpriteDibMerge`, `SpriteDibSet`, `SpriteDibColorMapMerge`, `SpriteDibFXMerge`, `SpriteDibDiff`) are thin wrappers around the blitters. `SpriteDibDiff` puts a source pixel back wherever the destination is 255; an unexported twin clears the destination wherever the source is non-zero.
* The engine's own assembly memcpy and memmove are replaced too.
* `SpriteHitDetect` / `SpriteHitDetectRect` test whether two sprites (or their parts) overlap in a non-zero pixel. Dominion never calls them. The overlap routine was 875 bytes of assembly; ours matches it on 30,000 random cases.

### 16-bit screens, wipes and leftovers (`blit.c`, `draw.c` +10, `view.c` +8, `display.c` +4)
* **16-bit screens:** a fill and copies that draw 8-bit bitmaps through the palette into 5-5-5 or 5-6-5 pixels, with keyed variants (skip source pixels that are 0; skip where the destination is 0 too). Dominion runs in 8 bits; of this group only the copy and the fill can be reached. Byte-exact against the original (20,000 cases).
* **Wipes:** two screen transitions the script interpreter can call: reveal a new background from the middle outwards, or blank it from the edges inwards, in steps with the sprites animating in between.
* **Leftovers nothing refers to:** a display-mode list callback, a helper that frees the DirectDraw state, and a function that reads the desktop's size and depth from the Windows 95 registry (`HKEY_CURRENT_CONFIG\Display\Settings`).
* With this, every function of the sprite module that can still run is ours. What remains there is assembly whose callers are all rewritten (the old merge fast paths, register-passing setup helpers); it goes away with the original DLL.

### Tile map (`tiles.c` 19 functions, `tileblit.c` 9 blitters)
The map's ground: isometric tiles, 64 x 32 pixel diamonds, drawn in half-tile rows (every second row shifted by half a tile), with the layers above them.
* Tile format: a 4-byte header, then the diamond's 32 rows without the transparent corners (2, 6, ..., 58, 64, 64, 58, ..., 2 pixels, each narrow row followed by 2 unused bytes).
* One blitter draws a whole tile; eight more draw tiles cut off by the screen edge (bottom, top, left, right and the four corners). Lookup tables built once per screen pitch tell them where each clipped row starts in the tile.
* A map cell has a tile number, an overlay number and two bit masks. The tile pass draws tiles, overlays numbered 0x37 and up (trees and other ground objects) and two masked overlays; the pass after the sprites draws overlays 2-0x36 and the shroud (overlay 1, filled with color 10 for whole tiles).
* Non-isometric maps are a plain grid of bitmaps drawn with the plain merge.
* The blitters are portable C, checked byte for byte with `tests/blit_compare.sh` (6,000 random cases, the tables for four pitches, and two deliberately broken versions caught).
* The DirectDraw full-tile blitter (0x10066470) is hand-written assembly, a long unrolled copy. Measured by drawing tiles whose bytes encode their own position: it draws the same pixels as the C one, except that when the tile's top corner is not on a 4-byte boundary, row 30 is copied 2 bytes too long (the 2 unused bytes after it land right of the row). Our C does the same.
* Checked: Human mission 1 matches the all-original runs (the scrolled view now pixel for pixel).

### Engine core, part 1: backgrounds (`rt_bg.c`, 43 functions)
The core ("Rt") keeps its whole state in one packed block that the game allocates and passes to every `Rt*` call. The menu script's commands arrive as packed records. First part, 0x10001000-0x10003039:
* **Background command:** a bitmap, a plain color, a tile map, or none; a bitmap larger than the screen makes the view scrollable. `RtSetBackground`, `RtSetTileBackground`, `RtSetScrollBox` and friends are the game's entry points to the same code.
* **Palette commands:** set colors from the script or load a palette resource (colors 10-245; the first and last 10 stay Windows' own).
* **Transitions** that bring a new background in: blinds, opening from the middle, closing from the edges, rolling down, and two slides.
* **Screen capture** to an 8-bit `.bmp`, growable arrays, and the `RtError` texts for plugins.
* Checked: menu tour and Human mission 1 against the all-original runs.

### Engine core, part 2: script expressions (`rt_script.c`, 6 functions)
Expressions in the menu scripts are compiled to a small stack machine: one opcode byte, some with 32-bit operands, 0 at the end.
* About 40 opcodes: constants, variables (globals in the state block, locals in a frame), arrays, assignment, arithmetic, comparisons, `&&` and `||` with short-circuit jumps, `++`/`--`, shifts, and three kinds of call.
* Calls go to a built-in function (270 of them in one 29 KB switch at 0x10003f6d, still original), to a function written in the script, or to a function in a plugin DLL (`ExternalCall`).
* The game can register a hook that sees every function call.
* Division by zero raises script error 0x6f (in debug mode) or fatal error 1000.

### Engine core, part 3: input (`rt_input.c`, 23 functions)
* **Mouse buttons:** the sprite under the pointer (or, failing that, the newest click rectangle there) gets its handler run. Sprites marked draggable are picked up instead and follow the pointer until the button is released.
* **Hover:** events when the pointer enters or leaves a sprite, and a list of hover regions (rectangles or sprites) with enter and leave handlers.
* **Keys:** a table of 256 keys with separate handlers for plain, Shift, Ctrl, Shift+Ctrl and release. The Pause key pauses the sound and the engine, or asks a script handler first.
* The script can install handlers that see every mouse event and every key and can swallow them; the game can install a key hook in front of all of it.
* **Collision pairs:** the script can ask for a handler to run when two sprites touch; the pairs are checked with the (unused-by-Dominion) hit detection.
* Hover region commands (up to 80 regions), queued events, and starting a drag from the script (+8 functions).
* Checked: menu tour and Human mission 1 (clicks, Escape, pause, drag-select, scrolling) against the all-original runs.

### Engine core, part 4: showing and hiding the scene (`rt_fade.c`, 6 functions)
* On a palette display the scene comes up by fading the palette in over 75 steps and goes down by fading it out; sprites keep animating during the fade. `RtSetNoFade` skips the steps.
* Without a palette (true color) the background is wiped in or out instead, with the sprite engine's wipes.

### Engine core, part 5: the object table (`rt_obj.c`, 48 functions; `load.cpp` in the original)
Every resource id has a 12-byte entry in the object table; what it holds depends on the resource type.
* **Types:** 1 bitmap, 0xf bitmap reference (its own header on another bitmap's pixels), 6 MIDI, 0xc wave, 0x10 group (a list of ids), 0x14 sprite, 0x16 tile map, 2/0xe/0x11-0x13 plain data (palettes, handlers), and 4, 5, 7-10 for the game's own types, which the game creates and frees through callbacks.
* **Loading** a resource loads what it refers to: a sprite record creates the sprite and loads its bitmaps and sounds; a tile map loads its tiles. Bitmaps are counted by use and freed when the last user goes.
* Before a group or sprite loads, the bitmaps it will need are collected and read in one pass over the file.
* **Copies:** sprites and the game's types can be duplicated at run time under new ids and deleted again.
* The game can hook every load and release. `RtGetLibraryState` returns a plugin's state by name.
* Checked: menu tour and Human mission 1 against the all-original runs.

### Engine core, part 6: plugins (`rt_plugin.c`, 12 functions)
The game's `.r32` files are plugins of the engine, and this is the code that loads them.
* The resource file names the plugins (key resource 10). Each `<name>.r32` is looked for in the game directory, then the data directory, and loaded; its `RtLibInit(rt, info)` returns the plugin's context, which the engine passes back as the first argument of every call.
* The functions scripts call in plugins are listed by name in the resource file (key 11) with their plugin's number (key 12) and bound with `GetProcAddress`.
* A plugin can register callbacks in its info block: every frame, on pause, on palette changes and two more.
* Unloading calls each plugin's `RtLibFree` but frees the DLLs later, from a window message, since a plugin may be on the call stack.
* A plugin that fails to initialize ends the game with "Error initializing Dominion. Please see the troubleshooting guide."
* Checked: the game starts, loads all its plugins through this code and plays as before.

### Engine core, part 7: command argument resolvers (`rt_cmdargs.c`, 62 functions)
A script command's fields can be constants or variables; a flag byte per field says which. Each command has a small wrapper that copies the packed record, replaces the flagged fields by the variables' values and calls the command's handler with the copy.
* The decompiler output is unreadable for these (unaligned fields), so the field and flag offsets were extracted from the disassembly by a script, and 35 of the wrappers are generated from that table. The 27 irregular ones (rectangles with a flag bit per corner, value lists with a count, results written back into a variable) are written out.
* Checked: menu tour and Human mission 1 against the all-original runs; every command the menus and the game run goes through these.

### Engine core, part 8: command handlers (`rt_cmds.c`)
The handlers behind the script commands, in address order. First batch (21): sprite bounds, position and level, the date and time, conditions (`if` with a script per branch, conditional jumps, expression tests), key states, MIDI, wave and CD audio, random numbers (never the same draw twice in a row) and starting a new scene. Second batch (18): random and sequential script choice, `switch` on a value or expression, arithmetic on variables (divide rounds to nearest), `sprintf` into string variables, string comparison, CD/MIDI/wave volume up, down or set, sprite script queues, and saving a resource as a file in the Windows directory.

### Engine core, part 9: settings and display mode (`rt_ini.c`, 32 functions)
* **Settings:** the script reads and writes `DOMINION.INI` (in the Windows directory) and the registry, as numbers or strings with `\n`-style escapes. Values can optionally be stored scrambled (see `HIDDEN_AND_UNUSED.md`).
* **Display mode:** switching the desktop to a resolution and depth (the exact depth first, then any deeper one), in a thread of its own; all zeros restores the user's mode.
* Zoom, CD stop, wave stop, and `RtCaptureFrame`.
* Checked: menu tour and Human mission 1; the game reads and writes its settings through this code.

### Engine core, part 10: the `Rt*` exports (`rt_exports.c`)
The functions the game calls in the engine. First batch (57, `RtCopy` to `RtSetComposeCallback`): mostly getters, setters and pass-throughs to the sprite and sound layers, plus `RtRun`/`RtRunMsg` (run a script handler), `RtGetCurrentScript`, `RtDynamicResource` and `RtDelete`. Each sets the current state block first; where the original returns the callee's result, ours does too. (`RtEnableDirectDraw` just returns 0 and stays original: too short to patch.) Second batch (28, `RtSetDisplayMode` to `RtGetPalette`): `RtSetDisplayMode` has Phase 1a's palette fix built in (the display is always treated as a palette device); the rest are setters and pass-throughs.

### Engine core, part 11: startup and shutdown (`rt_startup.c`, 12 functions)
`RtStartup` is the game's first call into the engine: it creates the state block (the first one is static), the full-screen window with its seven cursors, and the sound, sprite, resource and utility libraries; reads the engine's settings; checks for at least 256 colors; and opens the game's resource file. `RtFree` shuts it all down.
* `RtInit` holds the last of Phase 1a's palette patch spots; the fix is built in. **With this, the game runs on an unpatched original engine behind ours**: every place the patch changed is now our code. (The install still patches the original copy, which `DOMINION_ORIGINAL=all` comparison runs use.)
* Our DLL used to import the original's `RtStartup` only to make Windows load the original first; it now imports `RtEnableDirectDraw`, which stays original (it is too short to hook).

### Engine core, part 12: command line, service and small exports (`rt_service.c`, 39 functions)
* **Command line:** lowercased outside quotes; `/w`, `/k` and `/s` pick a run mode, `/t "name"` sets the disc title for "Please insert ..." messages, and any other word is taken as the resource file path. Dominion passes no switches (traced: none of the run-mode code runs).
* **Per frame:** `RtService` and Dominion's own `RtDominionService` (sprites, plugins, collisions, queued events).
* 29 small `Rt*` getters, setters and scene calls (`RtGetCopyId`, `RtSceneRestart`, `RtSetWindowBox`, ...); byte setters return what the original leaves in EAX.
* The screensaver window procedure for `/s` (see `HIDDEN_AND_UNUSED.md`); not reachable in Dominion, so it is an untested port.
* Checked: menu tour and Human mission 1.

### Engine core, part 13: joystick and controller keys (`rt_joystick.c`, 20 functions)
* Two script built-ins that pop their arguments off the script stack and play a MIDI or wave resource.
* Joystick capture at startup (up to `MaxJoySticks`, centers recorded); the only part Dominion runs.
* The input side of the unused two-player controller layer: joystick moves and buttons mapped to 17 slots per player (four- or eight-way directions, four buttons), each slot firing a script handler or a controller event; eight bound keys per player, with direction keys combined into eight directions. Untested ports: nothing in Dominion reaches them.
* Fixed on the way: our key-up code called the original's key filter as cdecl, though it is stdcall (a stack slip on a path Dominion never takes).
* Checked: menu tour and Human mission 1.

### Engine core, part 14: scenes (`rt_scene.c`)
A scene is one resource file with its script. Fourth batch (3): the run-state reset for a new first scene (with the info block handed to plugins), `RtSceneInCallback`'s entry, and leaving a scene (`RtSceneOut`: exit handler, then everything freed; an untested port). Third batch (2): entering the first scene (`RtSceneIn`) and finding its file, with the "Please insert ... into the CDROM drive" prompt on a CD drive. Second batch (3): opening the scene's resource file (its header sets the window title, color limits and start handler; its plugins load), the scene list entry with saved variables, and the resource name table. First batch (6): loading a scene (state reset, open, read, palette, start handler) and changing scenes (the current scene's variables saved, sprite, sound and object state reset, the next one loaded). Loading runs at every game start; changing and trimming the scene list are untested ports (Dominion moves between its scripts another way).
* Checked: menu tour and Human mission 1.

### Engine core, part 15: the interpreter's command loop (`rt_interp.c`, 8 functions)
A script handler is a list of commands: a 16-bit opcode, then fixed-size or self-sized arguments. The command dispatcher runs one command through its (already ported) handler and returns its size; handlers run until a command stops them or the scene changes, calling each other up to 30 deep with their locals on a stack in the state block. Also the command size table, handler records (arguments become the first locals), and the sprite engine's script message callback. Every script command of the game now runs through our code.
* Checked mechanically: the size table and every dispatcher case (size and handler called) against the decompile.
* Fixed on the way: four command wrappers ported earlier as `void` return their handler's result in the original, which the dispatcher uses to stop a handler; they now return it explicitly (before, it only worked because the compiler left the value in place).
* Checked: menu tour and Human mission 1.

### Engine core, part 16: one command, program launcher, kiosk mode (`rt_launch.c`, 9 functions)
Running a single command of a handler (live), and two features Dominion doesn't use: launching an external program while the engine waits hidden, and run mode 2's window procedure (see `HIDDEN_AND_UNUSED.md`). Untested ports apart from the first.
* Checked: menu tour and Human mission 1.

### Engine core, part 17: text objects (`rt_text.c`, 15 functions)
Resource type 0x15: text drawn with GDI (`DrawText`) into a bitmap that up to eight sprites show, with a 10-font cache, word wrap or a measured box, alignment, a color, and a password mode that shows `*` (optionally keeping the last character visible). Dominion draws its text with `BmpFont.r32` and never creates one, so this is an untested port.
* Checked: menu tour and Human mission 1 (no change).

### Engine core, part 18: text entry and timers (`rt_text.c`, `rt_timer.c`, 17 functions)
* Text entry (11): one text object at a time takes typed characters, with a `_` cursor, Backspace, Delete (clear), Esc (cancel, the old text returns) and Enter (accept: the text goes to a string, a handler is told), an optional length limit, digits-only mode and double-byte characters. The original tests an unset variable to decide whether to redraw after an ignored key; ours doesn't redraw.
* Timers (6): up to 20 script timers that run a handler after an interval, and up to 8 controller timers. With the table full, the original reports the error and then writes past the table anyway; kept.
* Checked: menu tour and Human mission 1 (timers run in both).

### Engine core, part 19: timer service and sprite transfer (`rt_timer.c`, `rt_transfer.c`, 11 functions)
* Timers (4): the per-frame services that run due script timers and controller-timer callbacks, the delayed event, and pausing (unpausing moves every timer on by the time paused).
* Sprite transfer (7, `RtSpriteTransfer`): one sprite object takes over another's place and images, with reference counts on the images; not reached in the tested paths, so an untested port.
* Checked: menu tour and Human mission 1.

### Engine core, part 20: cursors, strings, errors, events (`rt_msg.c`, 14 functions)
String lookup (script variables, or constants from the resource file), the cursor modes, the fatal-error and script-error message boxes (with the running script and file added), the 40-entry script event queue, extracting a resource to a file, and dropping pending messages. The string lookup and event queue run constantly; the error boxes only on errors (untested).
* Checked: menu tour and Human mission 1.

### Engine core, part 21: the window procedure (`rt_wndproc.c`, 10 functions)
The engine window's procedure: activation (switching away pauses the game unless `AltTab` is set), painting, mouse, keyboard and joystick routing (with the idle timer re-armed on input), the engine's own messages (deferred handlers, scene change, queued calls, the program launcher's poll), Alt+Tab and Alt+F4, and closing (exit handler, libraries shut down, cursors destroyed).
* Checked: menu tour and Human mission 1, and Alt+F4 at the main menu exits the game cleanly.

### Engine core, part 22: Living Wall Paper mode and screen captures (`rt_wallpaper.c`, 12 functions)
Run mode 1 (`/w`): the window procedure that keeps the engine behind all other windows, the rubber band for choosing a screen area, saving it as the Windows wallpaper, and screen captures to numbered `.bmp` files (see `HIDDEN_AND_UNUSED.md`). Dominion uses none of it: untested ports.
* Checked: menu tour and Human mission 1 (no change).
* With this, every engine-core function that runs in Dominion is ours except the 270 built-in script functions (one 29 KB switch).

### Engine core, part 23: bitmaps into sprites, icons, printing (`rt_print.c`, 18 functions)
A device bitmap becomes a sprite bitmap, its colors added to the palette (most used first, up to the free entries, then the nearest one). A file's icon (a `.ico`, or an icon of a program) can be drawn into a sprite, and a bitmap resource can be printed on the default printer (see `HIDDEN_AND_UNUSED.md`). Dominion uses none of it: untested ports.
* The original crashed when no default printer was set (it cut the `device` entry of `WIN.INI` at commas without checking); ours then reports failure.
* Checked: menu tour and Human mission 1 (no change).

### Engine core, part 24: geometry built-ins and mouse steering (`rt_geom.c`, 9 functions)
The script built-ins for angles, sine and cosine, where two lines meet, and where a ray from a point leaves a rectangle; and the unused controller layer's mouse steering (the mouse as a joystick: a dead zone of 40 pixels around the centre, held within 60, the direction from the angle). `GetAngle` and `GetIntersect`, which use x87 floating point, stay the original's.
* Checked: menu tour and Human mission 1 (no change).

### Engine core, part 25: the engine's movies (`rt_movie.c`, 28 functions)
Resource type 0xd: a movie is a command list (show the next frame N times, wait, start the sound, timing on or off, run a script command), a frame table and the frame data. Frames (full or changes) and sound are read from the resource file into ten 65,000-byte buffers ahead of playing, and the movie is shown as a sprite's image. A movie more than half a second behind drops change frames; `RtPauseMovies` stops the clocks. Up to four play at once. A trace during the tests showed Dominion never opens one (its videos are Smacker files), so this is an untested port.
* The command runner (0x10015e27) takes the sprite as an unused second argument, which the decompile had dropped.
* Checked: menu tour and Human mission 1 (no change).

### Engine core, part 26: built-ins on the script stack (`rt_builtin_misc.c`, 14 functions)
Built-in functions that take their arguments from the script stack: points on curves and lines, where two lines meet, where a ray leaves a rectangle, the local date and time (hundredths of a second too), collision tests, list entries, and random and sequence script picks whose state is kept per list in a table that grows by 100.
* Checked: menu tour and Human mission 1 (no change).

### Engine core, part 27: the controller layer (`rt_controller.c`, 17 functions)
Two players, each bound to a device slot (joysticks 1-2, keyboard sets 3-4, the mouse 5). Presses and releases fire the player's handler; directions pressed within a short time make a combination, matched against up to 50 entries per player (code under a mask, highest priority first) to fire the entry's handler, or kept for the script to read. Dominion never assigns a device, but the focus code runs on every window activation and pause.
* The original's second combination list, when recording, copies the entry from the first list at the same place; kept as it was.
* Checked: menu tour, Human mission 1 (no change), and Alt+F4 at the main menu.
* With this, every engine-core function is ours except the 270 built-in script functions (one switch) and two exports too short to patch.

### Engine core, part 28: the built-in script functions (`rt_builtins.c`)
The script's 204 built-in functions are one switch on the function number (0x10003f6d; 66 of its 270 numbers do nothing). Arguments come off the script stack, the result replaces the function number below them; many built-ins just pack their arguments into a script command record and run it. Ported from the assembly, since the decompile of a function this size is wrong in places (it has the absolute value falling through to nothing). They cover strings (compare, length, find, copy, characters, to number), arrays (read, write, dimensions), fixed-point arithmetic, min, max, random, angles, distances, sprite messages, positions, bounds, hit tests, levels and clip boxes, scrolling, background and palette commands, hover, click, key, drag and controller settings, timers, sound and CD, resource caching, scene changes, registry and INI settings, the user name, running programs, a message box, an assertion and a debug print. Ported in two steps, the second hooking the switch itself.
* A trace showed the game calls 15 built-ins in the menu tour and mission 1 (copies, arrays, random numbers, strings, sprite messages, visibility).
* The debug print formatted into 340 bytes (`wvsprintf` writes up to 1,024); ours has room, and passes the text to the debug output as text rather than as a format.
* The built-in for a sprite's part bitmaps wrote past its four result pointers for sprites of more parts; ours stops at four.
* `res_cache_file` (0x10069c2f, ported in part 1) takes a third argument it never uses, which the decompile had dropped. Our version took two, so the caching built-in (never used by Dominion) would have unbalanced the stack; fixed. A check of every port's return against the original found no other mismatch.
* Checked: menu tour, Human mission 1 (no change), and Alt+F4 at the main menu, after each step.

### Resource files, part 2: loading many bitmaps at once (`res_batch.c`, 6 functions)
When an object needs several bitmaps, the engine sorts them by data file and offset and reads neighbours with one disk read of up to 64,000 bytes, then hands each bitmap on. Also the run-length expander for coded bitmaps (0x1006a588, hand-written assembly), now in `blit.c` and checked byte for byte against the original on 20,000 random bitmaps by `tests/blit_compare.sh`.
* Checked: menu tour and Human mission 1 (no change).

### Sprites: the DirectDraw background (`display.c`, 1 function)
`set_background` (0x10049a02): in DirectDraw mode the background is a fill color or a bitmap copied (or run-length expanded) into an offscreen video-memory surface made on first use, centred on the screen. The menu tour sets it 7 times (**traced**).
* The original carried on after a failed surface lock and wrote through a null pointer; ours stops.
* Checked: menu tour, Human mission 1 (no change), and Alt+F4 at the main menu.

### Utilities: `LinearSearch` (`utils.c`, 1 function)
An export (hand-written assembly) that finds a string in a list of strings, with or without case; nothing in Dominion calls it. Checked against the original on 200,000 random searches. The original, ignoring case, ran past the end of the list when the key began with the whole last entry (it fell into its case-sensitive loop with the count used up) and crashed in the test; ours returns "not found". The original also changed EBX without saving it. `Collide`, `Distance`, `SquareRoot`, `GetAngle` and `GetIntersect` use x87 floating point and stay the original's.
* Checked: the comparison above and the menu tour (no change).

### Cutting loose from the original, step 1: direct calls
Goal: our `Rtlib32.dll` stands alone and `Rtlib32_orig.dll` goes. Steps: (1) calls to the original that now land in our code anyway become direct calls; (2) the C runtime functions still borrowed; (3) the x87 math, rewritten and checked bit for bit; (4) the two short exports; (5) the audio system; (6) the original's globals and tables become ours; (7) our DLL exports its own functions instead of forwarding, and the hooks go.
* Step 1 done: 70 calls that went through the original's address to our hook now call our functions. `ExternalCall` takes any number of arguments (the original's own version handled more than 64). `RtCaptureFrame`'s second argument is a name prefix, not an id.
* Exports whose function returns nothing (`RtShowGDISurface`, `RtPauseMovies`, `RtCursor`, ...) returned whatever was left in EAX; ours return 0. The game ignores these values.
* Checked: menu tour, Human mission 1 (no change), and Alt+F4 at the main menu.

### Cutting loose, step 2: the C runtime (`crt.c`)
The engine borrowed `rand`, `srand`, `atoi`, `strstr`, `strrchr`, `isalpha`, `abs`, `sscanf`, `_getdcwd` and the debug assertion report from the original's statically linked C library. Ours: the same random generator (seed × 214013 + 2531011, bits 16-30), kept per thread like the original's, so scripts see the same random numbers.
* The joystick calibration read six hex numbers with `sscanf` and sent whatever was on the stack for numbers it couldn't read (on modern Windows `SYSTEM.INI` has none); ours sends zeros.
* A failed engine assertion showed the debug C library's Abort/Retry/Ignore box; ours writes it to the debug output and goes on.
* Checked: menu tour, Human mission 1 (no change), and Alt+F4 at the main menu.

### Cutting loose, step 3: the floating-point math (`math87.c`, 5 functions)
`GetAngle`, `GetIntersect`, `Collide`, `Distance` and `SquareRoot` used x87 floating point through the original's C library. Ours run the same x87 operations in the same order (`long double` where the original keeps values on the FPU stack, `double` stores where it stores them, `atan2` and `sqrt` at full precision as the C library did, truncation through a 64-bit store as `__ftol`), so results match bit for bit. `tests/math_compare.sh` checks every vector from -1024 to 1023 for `GetAngle`, 3 million square roots and millions of random cases for the rest, at both 53- and 64-bit FPU precision: all identical.
* The engine converts radians to tenths of a degree with π = 3.1415 (1800 / 3.1415 = 572.97 instead of 572.96).
* Checked: the comparison above, menu tour, Human mission 1 (no change, rerun twice), and Alt+F4 at the main menu.

### Cutting loose, step 4: the two short exports
`RtSetScreenRes` and `RtEnableDirectDraw` are empty in the original (they return 0) and too short to hook. Ours are in `rt_exports.c`; our own code now calls ours, and they become the exports in step 7.

### Cutting loose, step 5: the audio library
Dominion never opens the engine's wave, MIDI or CD devices (it plays everything through `WAVEPLAY.R32`), but our engine code still calls into this library (script commands, wave and MIDI resources, movies), so it comes along: about 190 functions, ported in parts from the decompile, with the assembly read wherever the decompile drops arguments or garbles a field. None of it can be checked in the game; the game tests only show that nothing else changed. The context layout and the functions still borrowed are in `audio_int.h`.
* Part 1 (`audio_play.c`, 34 functions, 0x1003f036-0x10040555): playing, stopping and querying MIDI and wave sounds: priorities, fades, loops, random delays, notifications to the window. The original's MIDI stop clears the notification flag before checking it, so no end message is ever sent; kept.
* Part 2 (`audio_wave.c`, 19 functions, 0x10040555-0x10041514): the waveOut device: the format (the sound's own, the same in 8 bits, or the first of the twelve standard formats the device and the `WaveFormatMask` setting allow), opening, eight 8 KB buffers, volume, fades in 200 ms steps, mute, pause and reset. The original's "no format found" test compared a 16-bit value with 0xffffffff and so never fired (it would have read entry 65,535 of a 12-entry table); ours gives up. Its volume setter checks the previous call's error instead of its own; kept.
* Part 3 (`audio_objs.c`, 22 functions, 0x10041514-0x1004226a): making and freeing MIDI sounds (up to 20) and wave sounds (up to 100). A sound reads its data through a source of three callbacks (read, rewind, close); a wave longer than the buffers (8 × 8 KB) streams, a shorter one is read whole. Also a MIDI sound made from a `.mid` file, which nothing calls.
* Part 4 (`audio_files.c`, 11 functions, 0x1004226a-0x10042ac0): `.wav` files: a wave sound streamed from a file, loading one whole, saving one. Besides plain PCM it reads and writes "WAV7" files, 7th Level's own compressed waves (a 10-byte header, then 4-bit IMA ADPCM, through the codec of `compress.c`).
* Part 5 (`audio_cd.c`, 21 functions, 0x10042b13-0x100435e1): CD audio over MCI, its volume through the mixer's CD line. Positions are kept as track, minutes, seconds, frames from the high byte down (MCI's TMSF the other way round). The first play measures whether the drive reports the start position for a while; if it does, later plays wait up to a second for it to start. `AudioGetCDTrackLengths` closes the drive afterwards when it was open before rather than when it opened it itself, and doesn't set the context; kept.
* Part 6 (`audio_midi.c`, `midi_prepare.c`, 25 functions, 0x10043620-0x100445f8): the MIDI device (opening, volume, mute, pause, notes off) and turning a Standard MIDI File (format 0 or 1, not SMPTE timing) into the sequencer's event list: (time, message) pairs from all tracks, merged and sorted, then the times turned into waits in ms (at most 5 s each, else the file is refused). `tests/midi_compare.sh` checks the preparation against the original on 20,000 random files: all identical. Original quirks kept: meta and SysEx lengths are read as one byte rather than a variable-length number, and the delta times of meta events other than tempo changes are lost (tempo changes keep their own clock).
* Part 7 (`audio_mix.c`, 12 functions, 0x100447fb-0x1004504c): the mixer's main loop (take back finished buffers, mix every playing sound into the next free 8 KB buffer at 8 or 16 bits, hand it to waveOut), whole-device fades in 200 ms steps, end-of-sound messages, and the MIDI sequencer: events sent up to the next wait, the wait done by a one-shot multimedia timer of at least 3 ms, loops, channel 9 moved to 15 with `RemapMidiDrums`, notes kept off muted channels.
* Part 8 (`resample.c`, 17 functions of hand-written assembly, 0x1004504c-0x10045480 and 0x10045f08-0x10046250): converting PCM between 8 and 16 bits, mono and stereo, and sample rates. Rates change only by whole factors: down by averaging (the sum shifted by 1 for a factor of 2, else by 2, so a factor of 3 comes out at 3/4 of the level and of 5 at 5/4), up by linear steps (shifted the same way, so other factors than 2 and 4 under- or overshoot). Stereo to mono keeps the left channel. Only the two entry points are hooked; the helpers pass state in registers and have no other callers. `tests/resample_compare.sh` runs 20,000 random conversions through the original's `AudioResampleWave` and ours: all identical.
* Part 9 (`audio_stream.c`, 12 functions, 0x10045480-0x10045f08): a wave sound's data as the mixer takes it (moved to the front of its buffer and refilled from the source, looped, unpacked from IMA ADPCM, converted to the device format), the device volume steps, and mixing one sound into a buffer with its volume and fades (8-bit sums clipped to 0-255, 16-bit to ±32,767). Original quirk kept: 8-bit samples are scaled by the volume with their 128 offset still in, so a quieter 8-bit sound also shifts towards silence's low end.
* With part 9 the whole audio library is ours (the 20 functions at 0x100468d4-0x10048522 belong to the wave codec, whose callers are all ours in `codec.c`). Our engine code now calls it directly instead of through the original's addresses.
* Checked after each part: menu tour and Human mission 1 (no change) or the exit test, plus the byte-exact tests of parts 6 and 8.

### What's left
| Part | Functions | Status |
|---|---|---|
| Engine core, resources, utilities, sprites, display, audio | 1,384 | **ours** |
| Exports too short to hook (`RtEnableDirectDraw`, `RtSetScreenRes`) | 2 | original |
| Sprite assembly routines whose callers are all ours (C versions in `blit.c`) | 14 | dead in the original, no hook needed |
| Wave codec helpers whose callers are all ours (C versions in `codec.c`) | 20 | dead in the original, no hook needed |
| C runtime | ~400 | not ported; replaced by the system's |

