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
| 5 | `Rtlib32.dll` | Engine runtime: script interpreter, resources, sprites, DirectDraw | **Done**: the whole engine is ours (1,384 functions); none of the original's code runs, it is read as data |
| 6 | `ISO.R32` | The game logic: maps, units, AI, path finding, the interface | **In progress**: our DLL wraps the original; 707 of about 1,986 game functions are ours |

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

## 5. Rtlib32.dll (done)
The engine is 7th Level's "Runtime Player 1.2" (1996), which Dominion inherited when Ion Storm took the game over from 7th Level; see `HIDDEN_AND_UNUSED.md`. It is far bigger than the other modules: 430 exports and about 1,800 functions (73,000 lines of decompiled C, part of it a statically linked C runtime). Only 112 exports are used from outside; the menu script, the interpreter and most of the engine run inside it. So it is replaced function by function from the inside, the way re3 (GTA III) and DevilutionX started:

How it was done (stage 1, until step 7 below):
* **The shell** (`src/rtlib/`): our `Rtlib32.dll` loaded the original as `Rtlib32_orig.dll` and forwarded all 430 exports to it.
* **Rewrites** are listed in `ported.h` with the original's address. At load time `hooks.c` patched the first bytes of each original function into a jump to ours, so outside callers and the engine's own internal calls both ran our code.
* Our functions keep the original's calling convention (mostly stdcall, read from each function's `ret` instruction) and its data layouts.
* Original globals and not-yet-rewritten functions were reached through `orig.h`: `G(type, address)` and `ORIG(type, address)`, with the original's own addresses as in the decompiled code.
* **A/B testing:** `DOMINION_ORIGINAL=ResOpenFile,ResLoadResource` (or `all`) left those functions original; `DOMINION_TRACE=<file>` logged the first call of listed original functions. Both went with the shell (they are in the history, `hooks.c`).

Now (step 7): our `Rtlib32.dll` exports its own functions under the original's names and ordinals (`rtlib32.def`, generated from the original's export table and `ported.h`) and doesn't import the original. `main.c` maps the original (`Rtlib32_orig.dll`) as data, without resolving its imports or calling its entry point; it provides the starting values of the engine's globals (copied into our memory, still named by the original's addresses through `G()`), the strings and tables they point to, and the engine's resources. `ported.h` remains the map from original addresses to our functions.
* **Install:** `install_modules.sh` copies the unpatched original to `Rtlib32_orig.dll` (and still applies the old `palette` byte patches to that copy, now without effect: they are in code that no longer runs). `--restore` puts the original back.

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
* Step 1 done: 70 calls that went through the original's address to our hook now call our functions, and (after the audio port) another 60 through small per-file helpers, rewritten as typed calls from the functions' prototypes; callbacks handed to Windows and to the sprite library are ours too. `ExternalCall` takes any number of arguments (the original's own version handled more than 64). `RtCaptureFrame`'s second argument is a name prefix, not an id.
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
### Cutting loose, step 6a: the engine's globals in our own memory
Our code still names the engine's globals by the original's addresses (`G()`, `ORIG_PTR()`), about 130 of them. They now resolve into our own copy of the original's data section, taken over at load time as the original left it after its entry point (which stores its module handle and sets up its C library). No original data enters the repository; the values come from the player's own copy. A test run with those globals overwritten in the original showed nothing of the original still uses them. With `DOMINION_ORIGINAL` (A/B runs with original code) both keep sharing the original's.
* Still from the original's memory at load time: the module handle, the pointers its data holds (to its strings and code), and its resources (strings, cursors, icons). Step 7 has to provide these once the original is no longer loaded: the tables and resources from the original file as data.
* Checked: menu tour, Human mission 1 (no change), Alt+F4 at the main menu, and the run with the original's globals overwritten.
### Cutting loose, step 7: our own exports, the original as data
Our `Rtlib32.dll` exports its own functions under the original's 430 names and ordinals (checked identical) and no longer imports the original; the patching shell (`hooks.c`) is gone. `main.c` maps `Rtlib32_orig.dll` with `DONT_RESOLVE_DLL_REFERENCES`: no imports, no entry point, none of its code runs. It is the source of the globals' starting values (our copy of its data section), of the strings and tables they point to, and of the engine's resources; our code sets the module handle the original's entry point used to set. The 61 pointers in its data section that lead to its code belong to its C runtime and the resampler's old table, none of which our code uses.
* The original's `RtEnableDirectDraw` and `RtSetScreenRes` (empty) are now ours too.
* `DOMINION_ORIGINAL` A/B runs and `DOMINION_TRACE` went with the shell; comparisons use the saved screenshots of all-original runs, and `install_modules.sh --restore` brings the original engine back.
* Checked: menu tour, Human mission 1 (no change; rerun twice), and Alt+F4 at the main menu.

### What's left
| Part | Functions | Status |
|---|---|---|
| Engine core, resources, utilities, sprites, display, audio | 1,384 | **ours** |
| Exports too short to hook (`RtEnableDirectDraw`, `RtSetScreenRes`) | 2 | original |
| Sprite assembly routines whose callers are all ours (C versions in `blit.c`) | 14 | dead in the original, no hook needed |
| Wave codec helpers whose callers are all ours (C versions in `codec.c`) | 20 | dead in the original, no hook needed |
| C runtime | ~400 | not ported; replaced by the system's |

## 6. ISO.R32 (in progress)
The game logic: Ion Storm's "Isometric Engine PlugIn", an engine plugin (`RtLibInit`) that the menu scripts call by name. A debug build: 325 named exports, about 2,560 functions (123,000 lines of decompiled C), 405 of them a statically linked C runtime. Only one source file name survives (`E:\dev\rtplugin\iso\src\PathTbl.cpp`, the path tables). It is replaced from the inside the same way as the engine:
* **The shell** (`src/iso/`): our `ISO.R32` forwards all 325 exports to the original, installed as `ISO_orig.dll` (a forwarded export names its module without an extension, which Windows completes as `.dll`; `ISO_orig.R32` was not found). `hooks.c` patches each function listed in `ported.h` into a jump to ours. The original's globals stay its own while its code still runs.
* `DOMINION_ISO_ORIGINAL` (names or `all`) and `DOMINION_ISO_TRACE` work as the engine's did.
* Checked: the empty shell, menu tour and Human mission 1 (no change).
* A first-call trace (`DOMINION_ISO_TRACE`, every non-library function) through the menu tour and mission 1: 720 of the 2,120 game functions run there (`work/tmp/iso/live.txt`, not published).
* **Calling conventions:** most functions are cdecl (a debug build's default), but the `Iso*` exports are not all: `IsoVTilePos`, `IsoResetTileSet` and `IsoRefreshScreen` are stdcall. Called as cdecl, the stack is off by the arguments' size and the game crashes when a mission starts. Check each callee's `ret` before calling it.
* Our code calls the engine through `src/engine` (an import library and `rtlib32.h`), extended with what the game logic needs.

### Open items (as of part 169)
* **The 949 split is older than thought:** a first run of part 159 split at 949-950, and its whole log (1,676 ticks) is identical to two logs from our builds of 2026-11-16 and 2026-11-18 (`sp_old.log`, `v847_1.log`), made before parts 126-159. So those parts did not introduce it. It has still not been seen in an all-original run, so an earlier part may cause it, or it may be a rare timing path of the original.
* **Next part:** 170, from 0x100d7b3d (list the live ones with `work/tmp/iso/live.txt`). Left original, not reached: 0x1008d1b1 ("Host migrated"), `IsoGetPause`, 0x1008de6e-0x1008f31e, 0x10091400, 0x1009167e, 0x100921f5-0x10092ade, 0x10092dfc-0x100933da, `IsoFreeColorLookUpTables`, the AI record file functions (0x1009c6bb-0x1009c755, 0x1009c84d), 0x1009d1ff, 0x100a2b0a, `IsoColonySpaceAvail`, 0x100a4161, `IsoSetMenMat`, `IsoInitBuildStart`, `IsoCancelBuild`, `IsoGetTabset`, `IsoInitIntfFXTable`, `IsoSetTechLevel`, `IsoBuildingOnTabset`, `IsoDisplayRefineryInfo`, 0x100a6e30, 0x100a6e59, 0x100b0837 (transport creation), 0x100b2715, 0x100b2953, 0x100b31e0 (reached only without a map), 0x100c8773 and 0x100c883f (the path finder's range check and its search for a free cell near a blocked goal), 0x100caf39 and 0x100cb194 (a vehicle backing out of a blocked cell), 0x100ce61d (a patrol meeting an enemy wall post), 0x100cf7cf (whether an enemy wall lies ahead), 0x100b380a, `IsoFindObject`, `IsoGetPlayerId`, `IsoGetScrollRate`, and everything between 0x1007eb13 and 0x1008c755. Parts go on in address order. Run the state checks when the machine is idle, with an all-original run alongside. Watch for a split at tick 949 (seen twice, parts 139 and 141, both the first run after an install; the two runs were identical to the end, about 1 run in 15 then; not reproduced in 14 targeted runs; never in 7 all-original runs): if it comes back more often, bisect it with `DOMINION_ISO_ORIGINAL`.
* **A second path at tick 847 in the unit state log (explained):** since parts 63-67, three of thirteen state checks of ours left the originals at tick 847, all the same way. A run logged with the unit fields for ticks 840-850 shows why. A structure (unit 4, type 0x74) takes a 16-point hit at tick 847 that the original's run shows at tick 848; everything after follows from that. Shots are stepped by `IsoWeaponMovementLoop` once per displayed frame, as many steps as game ticks have passed, so a hit can land a tick earlier or later depending on frame timing. This is the frame-driven variation that also limits the originals' agreement to about tick 950, not a logic difference. It may show up more often with our display code than with the original's.
* **Not reached, left original in 0x1006a268-0x1006ab64:** seven effect functions (0x1006a268-0x1006aaad).
* **Not reached, left original in the AI:** the task handlers and their creators from 0x10062b12 to 0x10066fac (types 7, 8, 0xd, 0xf, 0x10-0x15), and 0x100616d4-0x10062630 (type 0xd and type 7). Probably reached later in a mission than the tests run, or in other missions.
* **Not reached, left original (developer info, tools, network):** `IsoParseSessionName`, `IsoDisplayMapInfo`, `IsoDisplaySpriteUsage`, 0x1005878b, 0x100589a9, 0x10058bf5, `IsoDetonateSelection`, `IsoSendUnitCommand`, 0x1005a56d, `IsoPreloadUnits`, `IsoAddReinforcement`, 0x1005ae71-0x1005b314, `IsoGetUnitStringAndBuildResults`, `IsoSetSpriteColorMap`, `IsoDisplayObstructInfo`, 0x1005bbe7, `IsoIsAllied`, `IsoFillProgressBar`, 0x1005c073-0x1005c1a8, `IsoCheckForCD`, 0x1005c515 (a message box).
* **Not reached, left original (developer info and tools):** 0x10054b89, `IsoShowSelectedUnitInfo`, `IsoShowComputerPlayerInfo`, `IsoShowAllMenMaterial`, `IsoShowAllPlayerInfo`, `IsoShiftChars`, `IsoSetFollowUnit`, `IsoEnableCaptureMode`, `IsoCaptureScreen`, 0x100562e2 (the game's end, reached through the timer callback), `IsoGetPlayerInfo`, `IsoSetPlayerName`, `IsoShowSelectedSectorInfo`; the alliance toggles `IsoToggleAllied`, 0x1005797d, `IsoSetAlliance` (network games).
* **Not reached, left original:** `IsoCheckCheatCodes` (0x10052df4, 3 KB: no test enters a code) and 0x10053a65 (the fog-of-war command it and others use); `IsoHostSetStartLocations` and `IsoPlaceEggStartsRemote` (network).
* **The radar's drawing is not checked on screen:** the tests' radar stays empty (mission 1 has none, the two-player match has no radar station yet). A test in a mission or match with a radar is needed; the radar code ported so far (parts 52-54) is checked by the state log and the trace only.
* **Not reached, left original:** the tile tools `IsoBuildSuperTile` and `IsoBuildTileHistogramFile` (0x1004f45a-0x1004f90d), the developer cheat 0x1004fbc4 (see `HIDDEN_AND_UNUSED.md`) with its helper 0x1004f90d, 0x1004fbaf, 0x100508bc, 0x10050d3e, 0x10050e74 (a debug display, flag iso+300), 0x100516f9 (a second radar background builder) and 0x10051f0d.
* **The AI's terrain analysis, 0x10048270-0x1004baf0** (`ta_run` and about 45 helpers: corridors, key terrain, areas of approach, base locations): left original except the parts every game runs (part 47). The shipped missions give their areas of approach and key terrain in the scenario, and in the tests `ta_run` is not called at all. 0x1004b905 (`pickSafeRendezvous`) is also called by the AI, but not reached in the tests either. A skirmish or random-map game might reach this code; check with a trace before porting it. The `dominion.ini` setting `EnableTerrainAnalysis` does not switch it on: it is read and handed to the network module, but nothing reads it back (part 64).
* **Not reached, left original in the AI:** 0x1004c1bf (finishing a task, 1.4 KB); 0x1004d392-0x1004e360 (8 functions, the base integrity checks: "Check base integrity compromised/perimeter").
* **The network code, from 0x100313e0 on** (about 130 functions up to `IsoReceiveMsg` 0x10043435, then more mixed with other code up to about 0x10048000: building, sending (`DPSendMsg`) and receiving the network messages, `IsoReceiveSysMsg`, the `IsoSend*`/`IsoRequest*` exports, the net process and windows): the functions only network games reach are left original; the ones every game runs are ported in address order (part 45 on). Port the rest once a two-player state log exists (see the next item).
* **The map editor:** its code is in the game and the script imports it (see `HIDDEN_AND_UNUSED.md`), but whether it can still be started is not known. To find out: locate the script code that calls `IsoMapEditDisplayLoop` and see whether a menu, key or command line switch leads there.
* **Left original until the editor question is answered** (no test can reach them): the editor's saving, 0x10020bea `IsoSaveScenario`, 0x10026aac-0x10028603 (writing the description as text) and 0x1002cbfa with `IsoSaveSDCFile` (writing compiled `.sdc` files).
* **The two-player chat test** (`tests/game/chat.sh`): either player's screenshot often comes out as garbage, also with the original. A more reliable check is needed, for example a two-player state log or reading the chat lines from the game's state. In part 45 the first shots were garbage in three runs in a row, including one with the changed functions kept original; the later shots showed both players in the match.
* **Coverage:** the code no test reaches is listed in `TESTING.md` section 4.

### Checking the game logic: the unit state log
Screenshots can't tell a logic change from a frame of timing (idle animations differ between runs), so the game logic is checked on its state instead. `DOMINION_ISO_STATELOG=<Windows path>` (`statelog.c`) writes a hash of every unit's state (position, facing, health, action, flags; not the fields the display changes every frame) once per game tick, from the call that runs the tick's logic in `IsoMovementLoop` (0x1008d375). `DOMINION_ISO_STATELOG_UNITS=1` (or `=from-to` for a few ticks, which disturbs the timing less) adds each unit's fields.
* Input: with `DOMINION_ISO_STATELOG_ORDERS=1` the local player's units get move orders at fixed ticks (Human mission 1: into the enemy at tick 40, on at tick 500) and stances (kneel at 60, crawl at 120, stand at 200, through `IsoSetSelectedUnitState`). Clicks would land a tick early or late.
* Time: the game paces its ticks by the real clock (`FUN_100af179`) and also reads it elsewhere (99 calls), so a faster or slower build changes when things happen. `DOMINION_ISO_STATELOG_CLOCK=15` gives ISO (the original and ours) a virtual `timeGetTime` that advances 15 ms per displayed frame from the mission's first frame; our display loop drives it, so original runs use `DOMINION_ISO_ORIGINAL=all,-IsoDisplayLoop`.
* With both, original runs agree up to about tick 950-1000 (the engine's own timers still run on real time; where exactly varies from run to run), by which time units have moved and fought. `tests/game/statecheck.sh` runs ours and compares it with four saved original runs; ours must agree at least as far as they agree with each other. `DOMINION_ISO_ORIGINAL=all,-name` keeps everything original except the named functions.

### Part 1: power grids (`power.c`, 10 functions)
Structures that give power (an energy well, a relay) mark the map cells within their radius with their player's bit (bits 16-23 of a cell, the radius per unit type from a table, the shape from a mask per radius); each player keeps a list of up to 128 power grids with an icon sprite each. Placing or removing a structure marks or clears its cells and remarks the neighbours within 1,400 pixels.
* Checked: menu tour, Human mission 1 (no change), and Alt+F4 at the main menu.
### Part 2: power use and the "no power" icons (`power.c`, 16 functions, 0x10001f74-0x10003fd4)
A grid is rebuilt by a flood fill from its energy well over the cells marked with the player's bit (the original's own 24,000-entry stack; when it is full, the fill just stops going deeper), then what its structures give and use is summed (power plants give half when damaged, less at levels 2 and 3; upgraded structures use a quarter more). A structure has power when its grid gives at least what it uses; below 50% left it gets a warning icon (up to 128, shown for the local player only), without a grid the "no power" icon, with the spoken hints once each. Also the search whether a cell reaches a grid with power to spare, allied grids in reach, and `IsoDisplayPowerInfo`, a debug box with the selected structure's figures.
* Original quirk: with the "all powered" cheat on, the icon choice reads an uninitialized variable; ours shows no icon.
* Checked: menu tour, Human mission 1 (no change), and Alt+F4 at the main menu.
### Part 3: the power bar, scrolling and selections (`view.c`, 11 functions, 0x10003fd4-0x10005bb1)
The power bar (its frame and the "<n>% total power" text), the scroll box for the screen size, scrolling the map: with the mouse outside the scroll box, with the keys when `KeyboardScrolling` is on (three times as fast with the fast-scroll flag), gliding to a stop over five frames, and the screen shake; the radar's view box; centring on a unit; clearing a player's selection or taking one unit out of it.
* Original quirks kept: the radar check compares the box's new y with the stored one before negating it, so it reports a move nearly every time; the scroll box is left unset for screen widths other than 640, 800, 1024 and 1280 (ours takes 640's).
* The radar box divides with the x87 at the precision the game sets, as the original did (`long double` in our code), and truncates the way `__ftol` does.
* Checked: menu tour, Human mission 1 (no change), Alt+F4 at the main menu, and a key-scrolling run with `KeyboardScrolling=1` (identical to the original).
### Part 4: debug sprites, straight lines, chat lines, display timing (`display.c`, 10 functions, 0x10005bb1-0x100065c3)
The debug sprites (show, hide; set by `IsoSetDebugSprite`), the direction from one unit to another when it lies on one of the eight straight lines, and whether the way along it is clear (it gives up after 20 empty cells). The multiplayer chat lines: up to five of the newest at the top left, drawn with BmpFont in the sender's colours, dropped when they expire. A lookup in the game's 2D tables, and the display process's frame delay.
* `BF_BlastText` is still called through the original's import (which reaches our BmpFont); the text is copied into the engine's buffer of a string resource first, as the original does.
* Checked: menu tour, Human mission 1 (no change), Alt+F4 at the main menu, and a two-player match with two chat lines (`tests/game/chat.sh`): the lines look the same as the original's and disappear.
### Part 5: the display loop and groups (`loop.c`, 5 functions, 0x100065c3-0x10008772)
`IsoDisplayLoop`, one frame of the game: on the first frame the briefing makes way for the map; then every unit is drawn (through its type's handler) or hidden depending on the view, with its shading, the unit under the mouse found and the cursor set, its name and figures drawn into the popup; the effects list drawn or hidden the same way; the radar, frame capture, following a unit, chat lines, the network and objectives windows. Also the map editor's loop, `IsoSetDebugSprite`, `IsoFindNextUnit` (the player's next unit of a kind, e.g. damaged or an energy well) and `IsoSelectGroup` (select all, those in view, by type, by the last one's class; numbered groups).
* Original quirks kept: the unit popup's extra step depends on the last unit of the list rather than the one under the mouse; `IsoFindNextUnit` keeps the previous unit's answer for unknown kinds; making a group counts the given player's selection but takes the local player's.
* Checked: menu tour, Human mission 1 (no change), Alt+F4 at the main menu, and a group run (`tests/game/groups.sh`: drag-select, Ctrl+1, deselect, 1): identical to the original. `IsoFindNextUnit` has not been run by a test.
### Part 6: screen refresh, radar clicks, popups, countdown (`screen.c`, 9 functions, 0x10008772-0x100090e0)
`IsoGetProfilerValue`, `IsoRefreshScreen`, `IsoRefreshRect`, `IsoRadarClickPos` (a click on the radar moves the view there, or sends the selected units), `IsoSetDirectDrawMode`, `IsoHideUnitNamePopup`, switching the radar on and off, a unit's screen bounds, and the countdown clock some missions show.
* The radar click's scaling uses the x87 and `__ftol` truncation like the radar box (part 3).
* `IsoGetProfilerValue` takes two arguments (it is stdcall and removes 8 bytes); the second is unused.
* Checked: menu tour, Human mission 1 (no change), Alt+F4 at the main menu. The radar is off in mission 1 (no radar building yet), so `tests/game/radar.sh` could not exercise radar clicks; the countdown has not been seen in a test either.
### Part 7: placing a unit (`unit.c`, 3 functions, 0x100090e0-0x1000a86b)
Setting up a unit when it is put on the map (about 120 fields: its cell and one of four places on it, facing, health and weapon from its type's record, the player's upgrade, being built or ready, its sprite cells, depth, bounds, a turret on top for the turret types), the player's upgrades for its kind, and keeping track of its square in the fog grid (32 pixels) and its sector (256 pixels).
* `IsoVTilePos` returns the cell index (now in its declaration).
* Original quirk kept: an upgrade halves the 0xd6 field of non-infantry and adds one.
* Checked: menu tour, Human mission 1 (no change), Alt+F4 at the main menu, the group run (unit positions and selections the same; idle animation poses differ by a frame from run to run, also between two runs of ours).
### Part 8: a unit's action each tick (`action.c`, 1 function of 8 KB, 0x1000a86b-0x1000c898)
Once per game tick for every unit: when its action's steps are used up it is removed if it was dying, waits for its order or delay, finishes arriving (its place on the cell, boarding a vehicle, entering a building), takes the next waypoint, and starts the next action (81 actions in a two-level jump table: standing, walking, turning, attacking, building, dying, the unit kind's own); then one step of the action is taken (movement from the action's animation record by facing, its sprite cell, mirrored for three facings) and its sector updated.
* Original quirks kept: two calls pass on more than the byte they mean (the whole register of a callee's byte result; a byte with the upper bytes of the unit's address), and ids are passed with the upper half of the unit's address. Our code works on the same unit memory, so it passes exactly the same values.
* Checked: the unit state log (ours equal to an original run over the whole run of 1,690 ticks, and to the others as far as they agree with each other), menu tour, Human mission 1, Alt+F4 at the main menu.
### Part 9: sprites, script values, damage (`unit.c`, `items.c`, `damage.c`, 10 functions, 0x1000c898-0x1000d96c)
A unit's sprite following its state every frame (depth, mirroring, cell, position, the selection marker's colour and health bar) and making room where a structure is put down; `IsoSetUnitItem`, `IsoGetUnitItem`, `IsoSetObjectItem`, `IsoGetObjectItem` (single values for the engine's scripts); the damage an effect does at a distance from its centre (x87 like the original, `__ftol` truncation), taking damage, and whether a weapon gets through armour (also for a unit inside a vehicle).
* Original quirks kept: `IsoSetUnitItem` items 0x3ef and 0x3f0 do their work but report failure (a missing `break`).
* Two more calls Ghidra marks fastcall are plain cdecl with one argument (`FUN_1000e7e2`); the "register" arguments it shows don't exist.
* Checked: the unit state log (as far as the original runs agree), menu tour, Human mission 1, Alt+F4. In the test fight the sprite update and armour checks run; effect damage, taking damage and the item functions are not called there.
### Part 10: hits and kills (`combat.c`, 5 functions, 0x1000d96c-0x1000f85e)
A unit hit by a weapon or an effect: the damage (direct, or falling off with distance; weapons reach twice their radius here), its armour, the "under attack" announcement, death with its score, the team and the revealed area, the host's network message when a unit falls below half or a quarter of its health (multiplayer); then fighting back or, for an enemy's unarmed unit that can't reach the attacker, running. Also the hit's reaction (ducking; vehicles and structures turning to their hurt states) and sprite, calling the units of the sector for help (at most once a second per unit), and killing a unit (structures are wrecked).
* The decompile left out part of the weapon damage: the attacker's health counts. A unit above half its health does half again as much damage, one at a quarter or less half as much (`HIDDEN_AND_UNUSED.md`).
* Ghidra's fastcall functions with 64-bit results here (`FUN_100a2550`, `FUN_100a24d0`, `FUN_1006b6d3`) are plain cdecl with a 32-bit result.
* Our earlier parts now call `unit_kill` and `unit_hit` directly.
* Checked: the unit state log (as far as the original runs agree; all five functions run in the test fight), menu tour, Human mission 1, Alt+F4.
### Part 11: commands, destroyed structures, sectors (`commands.c`, `combat.c`, `sector.c`, 15 functions, 0x1000ec9e-0x10010ca9)
Stances for the selected units (`IsoSetSelectedUnitState`: stand, kneel, crawl, through the stance between when needed; a vehicle's behaviour), a unit's fire button; a destroyed structure's dying and the units inside and on top of it killed or sent out one after another (scheduled 8 or 16 ticks apart), the units under a destroyed bridge or a new building killed and the map objects under a building removed; the sectors' unit lists (leave, join, which players have units and structures there); small tests and a player's slots.
* Original quirk: `IsoSetSelectedUnitState` passes a value whose upper 24 bits are left over on the stack; ours passes the low byte only (the vehicle behaviour 0-7, all the callee is given).
* Our earlier parts call these directly now.
* Checked: the unit state log, with the stances added to the scripted input (new baselines), as far as the original runs agree (ticks 949-981 in three runs, the same spread as the originals); menu tour, Human mission 1, Alt+F4. Destroyed structures and bridges are not reached in the test.
### Part 12: power relays (`relay.c`, 10 functions, 0x10010ca9-0x100118ad)
Putting a relay ("umbilical") on the map and taking it off (a slot of its player's, its cell marked), the ring of eight sprites circling a relay to show its reach (growing to the relay's radius, then staying five seconds or for ever), the nearest relay to a point (`IsoShowClosestUmbilical`).
* Original quirk: the ring's size comes from an 11-entry table indexed by the relay's power radius without a check; ours takes 0 outside it.
* `power.c` calls `relay_add` and `relay_remove` directly now.
* Checked: the unit state log (equal to one original run over all 1,681 ticks), menu tour, Human mission 1, Alt+F4. Mission 1 has no relays, so the relay functions themselves have not run in a test.
### Part 13: idle infantry, the mouse cursor (`idle.c`, `cursor.c`, 13 functions, 0x100118ad-0x10012852)
Infantry at ease play one of two idle animations every 21-35 periods of 16 ticks; a group's leader stirs its group (their own idle animation; the most hurt member sets how long until the next time). The random choices come from the original's own C library `rand()`, reseeded from the unit's facing and id each time, so ours calls the original's `srand`/`rand` (0x100f4d60, 0x100f4d80) to get the same numbers. Also the in-game mouse cursor (loading its animation table and sprite, enabling it, its state and operation) and `IsoInitInGameFont`.
* Original quirk: the idle animation number is passed with leftover stack bytes above it; ours passes the byte.
* Checked: the unit state log (as far as the original runs agree; all five idle functions and the cursor run in it), menu tour, Human mission 1, Alt+F4.
### Part 14: the mouse over the map (`mouse.c`, 3 functions, 0x10012852-0x10014160)
The mouse each frame (anything selected, over the map, the cursor for the current operation, whether the operation stands alone), whether a cell can take a move or a structure (free, and seen by the local player), and the cursor over a unit: move, attack, enter, load, repair, capture, the waypoint and building cursors, about 30 cases from the unit's owner, kind and state and the option keys. After the mouse rests on a unit for 30 ticks, its name popup: own units with their figures (a carrier's load in percent, a power plant's output), allies and enemies by player name.
* The names I gave `mouse_state`'s outputs in part 5 were guesses; they are now named for what they are (`selected_any`, `over_map`, `fixed`), and `loop.c` calls these functions directly.
* Checked: a hover run (`tests/game/hover.sh`: a unit's name popup, the move cursor over ground with units selected, a vehicle's popup): ours pixel-identical or within two pixels of the original's; the unit state log; menu tour, Human mission 1, Alt+F4.
### Part 15: selection abilities, popup and scroll cursor, scouting (`mouse.c`, `recon.c`, 5 functions, 0x10014160-0x10014d9e)
What the local player's selection can do, as a set of bits worked out from the selected units' classes each frame (the cursors of part 14 and the command buttons depend on it); placing the unit name popup under a unit; the scroll cursor's arrow for the scroll direction; the cursor sprite following the mouse. And where a scouting unit goes next: the corner of the map, of the three other quarters, its player has seen least of, by a sector on the way it can reach.
* The word at 0xe099 is this ability set, not "option keys" as part 14 called it; renamed in `mouse.c`.
* The scouting function has a second mode that was never finished (it returns -1 in every case).
* Checked: the hover run (pixel-identical), the unit state log, menu tour, Human mission 1, Alt+F4. The scouting function is not reached in the tests.
### Part 16: the Recon unit's double click (`recon.c`, 9 functions, 0x10014d9e-0x10015efc)
A double click on a Recon unit standing still sends it to a sector still in fog: a random neighbouring sector its side has not seen and it can reach in a straight line (a line of cells drawn Bresenham-style), else the nearest such sector in its area, else (once) anywhere on the map. Its goal is the sector's middle or a free spot near it; a sector it cannot reach is marked so its player's scouts skip it. Also whether a player or its allies have seen a sector, and the share of sectors a player has not seen.
* The random choice uses the original's C library `rand()`, seeded with `time(NULL)`, the wall clock in seconds: ours calls the original's `time`, `srand` and `rand`, so it draws the same numbers in the same second. Runs differ from each other, the original's too.
* Distances are the original C library's `sqrt` (called, kept in extended precision as the original compares them against a stored `float`).
* Original quirks kept: a range check on the sector number never fires (it tests a 16-bit value for being negative); one check reads the order at +0x11b after testing +0x119.
* Ghidra lost `FUN_10015b7e`'s body (the two tile positions and its result); written from the assembly.
* Checked: `tests/game/recon.sh` (double click on the Recon unit in mission 1: in two runs each of the original and ours, the unit once drives off and once stays, as the seed decides), the unit state log, menu tour, Human mission 1, Alt+F4.
### Part 17: scouting areas, a record table, unit categories (`recon.c`, `records.c`, 14 functions, 0x10015efc-0x10016aa0)
The share of a quarter of the map a player has not seen (what `recon_next_sector` of part 15 compares), a scout ordered to the sectors around one (its area for later double clicks), and the local player's scouts forgetting their marked sectors. A table of 200 records of 0x4c bytes (allocate, add, remove, look up, search with a test callback, and save and load it whole in saved games). And a unit's category for the interface (infantry, special infantry, the vehicle kinds, armed and unarmed structures).
* Original quirks: the quarter share gives 0 for a quarter number over 3; the category function leaves other bits in the register for three categories, but every caller uses the low byte only, so ours returns a byte.
* Checked: the unit state log, the hover run, menu tour, Human mission 1, Alt+F4. Saving and loading the record table (saved games) and the scout orders are not reached in the tests.

### Part 18: the action buttons (`buttons.c`, 3 functions, 0x10016aa0-0x1001782a)
`IsoProcessAction`, what the engine calls for a button of the interface: actions 0-0x27 for the local player's selection (stances, detonate, the selection modes for move, attack and the like, the rendezvous and withdrawal points, automatic retreat, repair, sell, each unit type's special action, and eight actions for the unit the interface shows), and 0x30-0x37 the toggle buttons, whose check state the interface also asks for without toggling (automatic repair, the commander's thumper, two per-player unit choices, two unit flags, player colours). And the fire button of one unit: a relay shows its ring, another unit its weapon's range.
* The original writes two lines to its debug log when the automatic repair button is toggled; the log function returns at once in the shipped build, so ours leaves the calls out.
* The decompile drops the second argument (1) of the sell and the 0x1005b314 calls; the assembly has it.
* Checked: the unit state log (its scripted stances now go through `IsoProcessAction`, actions 9, 8, 10, which call the same function as before), menu tour, Human mission 1, Alt+F4. The other actions and the toggle buttons are not reached in the tests.

### Part 19: the popup menu, the mouse over a unit, click or drag (`popup.c`, 5 functions, 0x1001782a-0x10018711)
The right-click popup menu (on by default, `PopupEnabled` in `Dominion.ini`): which items it has and which are available, for one unit or for the whole selection, from a table of two 64-bit item sets per unit category (one unit, several units) adjusted per unit (allies to give a structure to, entering, repair, the toggle buttons' states), then shown by the original's `IsoShowPopupMenu`. Also the test whether the mouse is over a unit (every frame, from the display loop: the box for infantry and relays, the sprite's pixels otherwise) and whether a mouse press was a click or a drag (10 pixels).
* The decompile gets the 64-bit item sets wrong in places (it puts the per-player bits 32-39 in the low word, and shows two 64-bit locals as four mixed ones); ours follows the assembly.
* Original quirk: more than 15 items are not shown, only reported through `DebugStr`.
* Checked: new `tests/game/popup.sh` (the menu without a unit, "Command Points", which needs a long right click on the ground: set, locate and remove the rendezvous and withdrawal points, the last two greyed while none is set; the Commander's menu; the selection's menu; all equal to the original's), the hover run, the unit state log, menu tour, Human mission 1, Alt+F4.

### Part 20: a click or drag on the map (`click.c`, 7 functions, 0x10018711-0x10018edb and 0x10019b52-0x1001a434)
What a left click or drag on the map does when the mouse button comes up: a drag selects the local player's units in the box (added to the selection with the add modifier); a click on the ground moves the selection there (with the marker effect; the order modifier places a structure instead where the selection can build); a click on a unit sells, repairs or upgrades it when the cursor is in that mode, adds it to or removes it from the selection, uses a single selected unit's special action on itself, links two structures of the same owner, has the selection guard it or detonate at it (order modifier), or hands it to each selected unit's order for that unit (part 21), and otherwise selects it alone (another player's unit goes into its owner's selection list).
* A debug option (bit 2 of byte 0xeea) paints the clicked tile with a random ground tile; ours keeps it, with the original's `rand`.
* The decompile drops the second argument (0) of the sell, repair and 0x1005b314 calls, and invents register arguments for two calls; ours follows the assembly.
* Checked: the unit state log, Human mission 1 (its move click), the groups run (drag select; the poses of idle units differ from the original runs by about 250 pixels, as much as with this part kept original), the popup and hover runs, menu tour, Alt+F4. Not reached: clicks on structures, other players' units, the modifiers, structure links.

### Part 21: a selected unit's order for a clicked unit (`orders.c`, 5 functions, 0x10018edb-0x10019b52)
What each selected unit does with the unit the player clicked (called from `map_click` of part 20): board a vehicle or enter a structure with room (where the vehicle waits moves to the boarders' middle), go into a building of class 100 or make it the owner's choice (0xd6f9), the type 0x21 unit's order, an engineer's repair or capture of a structure, and attacking (also an own unit with Ctrl). Units ordered together start their action 2 ticks apart.
* The decompile shows the attack order as fastcall with two invented register arguments, and leaves out an argument of the attack call (the callee reads only four, so ours passes four).
* Checked: new `tests/game/orders.sh` (Ctrl+click: the infantry shoot the Recon to pieces; Alt+click on a soldier: "Guarding specified unit/structure"; equal to two original runs apart from the animated pool), the unit state log, menu tour, Human mission 1, Alt+F4. Not reached: boarding (no transport at the start of mission 1), entering buildings, engineers, the type 0x21 unit.

### Part 22: the modifier keys, the random map generator (`mapgen.c`, 13 functions, 0x1001a434-0x1001b380)
`IsoGetKeyModifiers` (the script asks which of Shift, Ctrl, Alt and the right button are down) and the unfinished random map generator `IsoGenerateMap` (see `HIDDEN_AND_UNUSED.md`): a new map, a ring of numbered object areas on a grid of 9x9-cell blocks, clusters of objects from per-terrain tables on the free cells, with the original's `srand`/`rand`.
* Most of its functions are stdcall with more arguments than the decompile shows (it drops the seed, the block grid and the area count); ours follows the assembly. Four steps of the original that only return 1 or loop over nothing (0x1001aa18, 0x1001adbd, 0x1001ae15, 0x1001b050) and the debug print of the block grid are not hooked: all their callers are ours.
* Checked: the unit state log, menu tour, Human mission 1, the orders run (Ctrl, Alt), Alt+F4. The generator itself is not reached (nothing in the tests calls `IsoGenerateMap`).

### Part 23: sounds on the map, unit voices (`sound.c`, 7 functions, 0x1001b380-0x1001bd92)
A sound placed on the map: full volume in the middle of the view, 3 per pixel quieter beyond half the view's width or height, panned by 6 per pixel from the view's left edge, and not played at all below 7500 (9000 in multiplayer); it goes to `WAVEPLAY.R32`'s `rdfPlayWave` through the original's import thunk. On top of it: a unit's voice (attack, selected, dying, moving and two more; a wounded soldier speaks its next recording; the file number comes from the unit type's table), general sounds (`gen800xx.wav`), and a weapon's firing and hit sounds by projectile or by weapon number. And `IsoGetSectorCoordinates`.
* The function earlier parts called `tutorial_event` is the unit voice (`unit_voice`): an order makes only the first unit speak. Renamed in `action.c`, `combat.c`, `click.c` and `orders.c`.
* The original reports missing waves through its debug printf, which returns at once in the shipped build; left out.
* Checked: the orders run with the new `DOMINION_WAVE_LOG`: the same 59 waves with the same volume and pan as the original (voices, 24 weapon sounds with panning, interface sounds); the unit state log, menu tour, Human mission 1, Alt+F4.

### Part 24: sector paths (`paths.c`, 7 functions, 0x1001bd92-0x1001c7c0)
Helpers on top of the path finder (0x100c70bc) between sectors' middles: releasing a found path's nodes (reference counted, back to a free list), its length (the sum of the straight pieces, with the original's `hypot` and truncation), whether a path exists from a sector or a point to a sector, how far a straight run of sectors goes in one direction (recursive), and a flag bit of the map cell in a sector's middle.
* The run in one direction also works out the slope between the two sectors as a float and never uses it; left out.
* Checked: the unit state log, the Recon run, menu tour, Human mission 1, Alt+F4. None of the seven ran in those (a trace of the Recon run, and the earlier trace of the menus and mission 1): probably the AI of later missions.

### Part 25: tile background and tile library, map editor helpers (`mapedit.c`, 7 functions, 0x1001c7c0-0x1001d92f)
Making the engine's tile background from the map's cells, freeing the map arrays, loading a tile library (`.TL2`: an 18-byte "TLX" header with the tile count, then the tiles; all of it or only listed tiles) and freeing it, `IsoUnloadMaskSubSystem`, and two map editor functions: `IsoCompileTileLibrary` (writes a tile list resource's tiles into a library file) and `IsoAutofillMap` (random tiles from a range, none equal to its upper or left neighbours, seeded from the clock; it uses the map width as the row stride for the neighbours, also where the map's rows are wider).
* `IsoClearScreen` (it only returns 0) is too short to hook and stays original.
* Checked: menu tour, Human mission 1, Alt+F4, the unit state log. A trace of the mission run shows none of the seven running (the scenario loader of part 26 loads tiles its own way).

### Part 26: a new map (`createmap.c`, 1 function, 0x1001d92f-0x1001e25a)
`IsoCreateMap`, run for every mission: the view's size in cells for the screen width (tables for 640, 800, 1024 and 1280) and the map size (32-256 by 64-512 cells), the row shifts, the cell size (64 x 32 pixels) and its logarithms, the map's pixel size and middle, clearing objects, units and the old map, the map and object arrays, the scenario's tiles (or the editor's autofill), the path finder and the AI.
* The decompile drops the fourth argument of the tile read (the tile count, 0xcda2); ours follows the assembly.
* Checked: the unit state log (one run equal to an original run to its end, tick 1681), menu tour, Human mission 1, Alt+F4. Only the 640-pixel table and mission 1's map size are reached.

### Part 27: loading a scenario (`scenario.c`, 2 functions, 0x1001e25a-0x10020b81)
`IsoLoadScenario` and the 10 KB loader behind it, run for every mission. A `.wxm` or `.wxp` file: a 0x80-byte header ("WXM"/"WXP", version, map size, tile set, offsets and sizes of the parts), the map's cells, the tiles (from the tile set's library, or embedded in the file), the objects (record layouts for versions 0, 1 and 2-5), and the AI's choke points (versions 3-5: points with their cells, the links between them, and a grid). Afterwards the cells' temporary bits are cleared, and in the editor (the engine started without sprites) tiles over 22000 are repaired ("Repairing corrupted map...").
* All 105 shipped maps are version 1.5. Versions 3 and 4 differ only in how points and links are stored (version 3 points have 6-byte cells; versions 3 and 4 links have 10 points, version 5 links 20).
* Original quirks kept: version 1 objects are counted as 12-byte records but stepped through 48 bytes apart (reading past the buffer); version 3 recomputes the points' cells when the previous map's grid was 0x8000 bytes or more; two arguments carry the header's address in their high halves.
* The loader calls several helpers of the original that are still original: the tile set's library (0x10023657, 0x10023b04, 0x10023559), the choke point set-up (0x10098542, 0x10098970, 0x1008de00, 0x10098dd4).
* Checked: the unit state log (equal to an all-original run made at the same time, both 847 on a busy machine), Human mission 1 (4_moved pixel-identical), the orders run, menu tour, Alt+F4.

### Part 28: map cells and tiles for the scripts (`maptiles.c`, 7 functions, 0x10020b81-0x10020bea, 0x10021667-0x10021c65)
`IsoRescanMap` (run by the scenario loader: clears the cells' top byte and marks which cells block, the edge and cells of blocking tiles), `IsoGetTile`, `IsoSetTile` and `IsoSetTileSet` (only in the editor or without sprites; otherwise they return 1 and do nothing), `IsoSetTileLib`, clearing the object map's marks, and `IsoMessageBox`.
* Original quirk kept: the cell checks allow x equal to the map width and y equal to its height (one past the end).
* Checked: the unit state log (980/990), Human mission 1, Alt+F4.

### Part 29: the tile background's view, obstructions (`maptiles.c`, 3 functions, 0x10021c65-0x100221d9)
`IsoResetTileSet` (in the game it only sets the tile background's flags: show blocking cells, or the local player's view; in the editor it fills the background with the cells' tiles and, as debug colours, their marks), `IsoGetObstruction` and `IsoSetObstruction` (a cell's object-map word; whether it blocks).
* Checked: the unit state log (950), Human mission 1, Alt+F4.

### Part 30: the map file list (`filelist.c`, 3 functions, 0x100221d9-0x100229d0)
`IsoBuildFileList` (all files matching a pattern) and `IsoBuildFileListIni` (only those an ini file lists as "<key>1".."<key>n"), for the menus' map and mission lists: 0x104-byte names, sorted with the original's `qsort` and comparison (0x1004e9d2), so the order stays the same. A multiplayer client keeps the list it has.
* Checked: menu tour, the unit state log (970), Human mission 1, Alt+F4.

### Part 31: map file info (`filelist.c`, 4 functions, 0x100229d0-0x10023185)
`IsoGetWXPInfo` (for the map lists: a map's header, its title, tile set, number of players and size, whether it is a .wxm; a multiplayer client compares the host's map with the header it got, and returns 0 only when it lacks the file), `IsoGetSDLFilenameIndex` (the current scenario's place in the list), `IsoFreeFileList`, and a map file's info by path.
* Checked: menu tour, Human mission 1, the two-player chat run (both players reach the match; the joining player's screenshot is garbage in that test also with the original), Alt+F4.

### Part 32: the mask and tile libraries (`tilelib.c`, 4 functions, 0x10023185-0x10023ee6)
`IsoLoadMaskLibrary` (the fog masks, a "TLX" file; a mask of odd size is followed by a padding byte, moved in with the original's `memmove`), loading the scenario's tile library (a "TL2" file of 0x444-byte tiles: all, or the ones the map uses), freeing it, and the tile background the engine draws (tile numbers, mask list, map and object map). All run for every mission; the scenario loader and `IsoCreateMap` now call ours.
* Original quirk kept: when the mask list cannot be allocated, the file stays open.
* Checked: the unit state log (970), Human mission 1 (fog and tiles), menu tour, Alt+F4.

### Part 33: script file commands, small AI callbacks (`filelist.c`, `aiglue.c`, 7 functions, 0x10023ee6-0x100241a0)
`IsoRenameFile`, `IsoDeleteFile` (optionally in the save folder) and `IsoGetMapFilename` for the scripts (the first two take the state from the global, not from their first argument, as the original does), and four functions of the AI's callback tables: calling a callback with 0-4 more arguments, a debug log line (silent in the shipped build), setting an AI player's value at 0x6a, and passing a unit order on.
* Checked: the unit state log (970), Human mission 1, Alt+F4.

### Part 34: AI script commands (`aiglue.c`, 10 functions, 0x100241a0-0x10024acc)
Ten commands of the AI's scripts: they find an AI player's group or task (tasks are 0x74-byte records at 0x1018b600, players 0x143-byte records at 0x1018b604), add tasks, hand groups orders, and mark tasks done. They only glue the AI's own functions together, which stay original; their names are by address until the AI is worked out. The debug log lines between them are silent in the shipped build; the name lookups for those lines are kept.
* Checked: the unit state log (970, with an all-original run at the same time 980), Human mission 1, Alt+F4. None of the ten ran in the earlier trace.

### Part 35: more AI script commands (`aiglue.c`, 10 functions, 0x10024acc-0x1002511b)
Ten more: marking an AI unit record, setting values of an AI player's first task of a type, orders with a value to an AI player's squads (0x130-byte records at 0x1018b608: the main one, the first, all, or those of a type), and setting the squads' mode (1-4 as 3, 4, 5, 2). 0x10024b57 runs in mission 1.
* Checked: the unit state log (ours 847 with an all-original run at the same time at 656: a busy machine), Human mission 1, Alt+F4.

### Part 36: AI script commands, the editor's AI temp files (`aiglue.c`, 10 functions, 0x1002511b-0x10025747)
Six more AI script commands (an AI player's group leader of type 0x14 or 0x19, an idle unit of a type, and orders passed on), the map editor's AI temp files ("_AITemp<n>.txt") and `IsoInitMapEditAITempFiles` (the editor's AI defaults per player), and sorting the AI's table of 201 names (with the original's `qsort`).
* Checked: the unit state log (952, one run equal to an original to its end), Human mission 1, Alt+F4.

### Part 37: the scenario description in text form (`sdl.c`, 2 functions, 0x10025747-0x10026aac)
`IsoLoadSDLFile` and its parser: a scenario description as text (`.sdl`, the form the map editor writes). The shipped missions come as compiled `.sdc` files, which `IsoLoadSDCFile` (0x1002d4de, with 0x1002cf1c) reads instead, so this parser does not run in the game. The text file is read twice (the first pass counts the command lines and allocates 0x5a-byte records, the second parses them). A line loses its blanks (not inside quotes or text), quotes and comments (`/`, `;`); its first word is looked up among 64 commands (type, subtype, number of values, how many are required), the values are numbers, names from the AI's sorted table of 201 (binary search), or strings; commands of type 0x32 and 0x3c start a block of text lines (the briefing, collected at 0x1860). With `check` each record is checked as it is read (0x10028603) and the mission is then started (0x10048270). The map editor also copies the AI rule sets (`DEFRULESET`) to its temp files. A multiplayer client loads the host's description with another loader (0x1002cf1c, still original).
* All C runtime calls are the original's (the files are the original runtime's FILE objects, and `strtok` keeps its state there).
* Original quirks kept: the record before the first is read for a text line's type; an empty line takes no record; in the editor the AI temp file is closed again on every line of the second pass.
* Checked: the unit state log, Human mission 1, menu tour, the two-player chat run, Alt+F4, all unchanged; but none of them runs this parser (an earlier note here said otherwise: a trace shows only the `.sdc` loader running).

### Left for later: the map editor's saving (0x10020bea `IsoSaveScenario`, 0x10026aac-0x10028603)
Saving a map from the editor: the map file (`IsoSaveScenario`) and its scenario description, written as text (build info, briefing, players with race, colour, tech level and resources, terrain analysis, energy wells, the units grouped into bases and groups; 0x10026aac with a sort comparison 0x10028308 and 0x10028549). Nothing in the tests can reach the editor, so these stay original until it is known whether the editor can still be started (see `HIDDEN_AND_UNUSED.md`).

### Part 38, step 1: the scenario record executor, main records (`sdlexec.c`, 0x10028603)
The executor (16.7 KB) carries out one record of the scenario description; `IsoLoadSDCFile`'s loader runs it for every timer, hot spot and settings record of a mission. Ported a part at a time: our function handles the subtypes rewritten so far and hands the others to the original's code through a trampoline (its 9-byte prologue, then a jump past the hook). Step 1: the main records (subtype 0): timers (`TIMER_EVENT`), hot spots (sector flags per player), the briefing's line count, the cinematics, the briefing's wave, designer, version and date.
* Checked: the unit state log (970), Human mission 1, Alt+F4. Mission 1 has one timer-free set of main records (types 0x28-0x50).
* Step 2: `ADD_UNIT` (subtype 10, 45 of mission 1's records): player, type, direction, mode, damage (1000 a flag, 2000 half health, 3000 a quarter), position, count, difficulty (0-2: from that difficulty on, 3-5: only at difficulty 0-2) and two values for the unit; in multiplayer the host sends the units instead; the wall pieces placed one after another are linked; the editor keeps the difficulty and mode with the unit. Subtype 5 does nothing. Checked: the unit state log (970, as an all-original run at the same time), Human mission 1, Alt+F4.
* Step 3: subtypes 20-130: `ADD_APC_UNIT` (a unit with fewer settings), `ADD_OBJECT`, game won and game lost (a timer 0x30 ticks later calls the original's end-of-game handler), `LOAD_MAP` (our scenario loader), `SET_TECH_LEVEL` (21 and up mean 13), `PLAYWAVE` (one of the local player's announcements), showing a text, and three that only write to the debug log (transport link, load description, maximum game time). Mission 1 uses `LOAD_MAP` and `SET_TECH_LEVEL`. Checked: the unit state log (951), Human mission 1, Alt+F4.
* Step 4: subtypes 120-200: `SET_OBJECTIVE` (values for the unit at a place), `SET_COMPUTER_PLAYER` (with its posture), `SET_PLAYER_RACE` (also the player's name, "Human (AI)" and so on for computer players, and its alliance with itself), `SET_PLAYER_COLOR`, `SET_MAX_PLAYERS`, `SET_MEN_MATERIAL` (set, none or add; per difficulty when given; the editor keeps all six values), `SET_NEXT_SCENARIO`, `SET_DEBUG_LEVEL` (the debug bits at 0xeea). Mission 1 uses the player settings. Checked: the unit state log (950 and 857; an all-original run at the same time 970), Human mission 1, Alt+F4.
* Step 5: subtypes 210-310: the AI's records (`DEFRULE`, the rule's settings, its end, `DEFGROUP` with the player's next group number, the group's end; they go to the AI's set-up function 0x100e1073, which needs the AI started first), linking energy walls, enabling, disabling and destroying (timers, a unit), and two more records handed to 0x1004b03a and 0x1004b119. Mission 1 uses all but the timers and destroying. Checked: the unit state log (847 twice, with all-original runs at the same time at 491 and 656: a busy machine), Human mission 1, Alt+F4.
* Step 6, the last: subtypes 320-660: convoys and patrol paths (way points from the record), following a unit, `DEFBASE` with the player's next base number and the base's end, default objectives (164 pairs), changing a unit's owner or objective values (by objective number or place), uncovering fog (around a place or the units of an objective; the player argument's high half is the unit's address, as in the original), `ADD_ENERGYWELL` (a bad one only warns), alliances, entry points, the game's end condition, and the countdown display (shown for n seconds, hidden). An unknown subtype gives the original's "UNDEFINED SUBREC" message. The trampoline to the original is gone: the whole executor is ours. Checked: the unit state log (867, with an all-original run at the same time at 847), Human mission 1 (it uses the patrol path, energy wells and the end condition), the two-player chat run (the joining player in the match), Alt+F4.

### Part 39: the description's getters, energy wall links (`sdcget.c`, 5 functions, 0x1002c74e-0x1002cbfa)
For the scripts: a briefing line, the briefing's wave, a cinematic (appended to a variable) and the next scenario's name; and linking two energy wall towers by their places (the `ENERGYWALL_LINK` record: the towers must be aligned and the line free; both towers' power is checked first). The first three run in mission 1 (briefing).
* Checked: the unit state log (867), Human mission 1 (briefing and pause screens), Alt+F4.

### Part 40: loading the compiled scenario description (`sdcget.c`, 2 functions, 0x1002cf1c-0x1002d589)
`IsoLoadSDCFile` and its loader, run for every mission: the `.sdc` file (a 0x18-byte header with the record count, then 0x5a-byte records), read whole; the timers, hot spots and settings records are carried out by the executor of part 38, the lines after the briefing record are collected as the briefing, and the mission is started (0x10048270). A multiplayer client without the host's file reads its text form (part 37) instead. The editor's writing of `.sdc` files (0x1002cbfa, `IsoSaveSDCFile`) stays original with the rest of the editor's saving.
* Checked: the unit state log (one run equal to an original run to its end, tick 1677), Human mission 1 (4_moved pixel-identical), Alt+F4.

### Part 41: the scenario's timers and events (`events.c`, 11 functions, 0x1002d589-0x1002e6f7)
The mission's events: timers (32 slots of 0x1a bytes, kept sorted by time with the original's `qsort`; a due timer runs its callback, such as the end of the game or units leaving a destroyed building, or the records after its `TIMER_EVENT` record, with the AI players held meanwhile, and is set again when it repeats), hot spots (a unit entering a flagged sector runs the records after the first matching `HOTSPOT_EVENT` record, clearing its sectors when it fires once), event counters (type 0x1e records counting down, for example when a group's leader dies), enabling and disabling timers, destroying the units of an objective, and the map information of `.sdc` files for the menus (`IsoGetSDCInfo`). The records "after" an event are those up to the next record of the same type and subtype.
* Original quirks kept: a repeating callback timer is set again with its arguments read from its slot after the slots were cleared and sorted; `IsoGetSDCInfo` returns the stack's contents as the map information when a multiplayer client lacks the file.
* Checked: the unit state log (867, with an all-original run at the same time at 847; a trace shows the event counter and the timers running in its fight), Human mission 1, Alt+F4.

### Part 42: `IsoProcessSDCRecord` (`sdcget.c`, 1 function, 0x1002e6f7)
For the menus: looks through the description's records of a type for player 0's race, the next scenario or the cinematics.
* Checked: the unit state log (858, with an all-original run at the same time at 656: the machine had just started), menu tour, Human mission 1 (normal after the start-up load was over), Alt+F4.

### Part 43: `unit_action_tick2` (`step.c`, 1 function, 0x1002e8e0)
The second variant of a unit's action each tick, a sibling of `unit_action_tick` (part 5): it chooses the next action when the current one's steps are used up, then takes one step. Unlike the first variant, its units keep no place on their cell. It has timers at 0x1b3/0x1c9 and turns with the 0x13/0x14 turning animations, using three [to][from] direction tables (0x1015b5a0, 0x1015b5e0, 0x1015b620). Types 0x24 and 0x27 only move on even ticks. Added `SpriteHitDetect@20` to the engine imports.
* Checked: the trace shows the unit state log run reaches it. Unit state log at 847, with an all-original run at the same time at 656. Human mission 1 and Alt+F4 also checked.

### Part 44: `unit_path_next`, `unit_action_done` (`step.c`, 2 functions, 0x10030760-0x100313d0)
* **`unit_path_next`:** moves the unit on to its next path point, using the order records at I(0x1984). The next record is at +0xe; -1, -2, -3 and 0 end the path, and then the unit stops with the fitting animation. With `set`, it also takes the point's position (+4/+6), facing (+9) and waiting time (+0), turns towards the point and, in multiplayer, sends the new path for the local player's units.
* **`unit_action_done`:** chooses the action that follows a finished one in `unit_action_tick2`. When there is still turning to do, it picks a turn on the spot (0x13/0x14, or 0xd-0x10 when the animation record allows the long way round).
* Checked: the trace shows the unit state log run reaches both. Unit state log at 950 and 950, the full range two original runs share. Human mission 1 and Alt+F4 also checked.

### Part 45: the network flags (`netflags.c`, 5 functions)
From the network code (from 0x100313e0 on), only the parts every game runs: `net_delay` (0x1003f390: the two counters 0xd857 + 0xd86b, read under the network lock while the network thread runs; the due tick for a path point sent to the others) and `IsoSetServerFlag`, `IsoGetServerFlag`, `IsoSetNetworkPlay`, `IsoGetNetworkPlay` (stdcall). The rest of that range is left original for now (see Open items).
* Checked: unit state log (847), Human mission 1, Alt+F4, and a two-player match (`tests/game/chat.sh`: both players in the match; the chat shots came out as garbage, the same as with these functions original).

### Part 46: more of the network code every game runs (`netflags.c`, 6 functions, 0x10045c72-0x10047322)
* **`net_pump` (0x10045c72):** hands up to 256 received network messages a call from the ring buffer at I(0x1ac7) to the message handler 0x100443d3 (still original), under its lock. When starting or joining a game failed, it says so in a message box and leaves the game with an engine message. With the `IsoKaboom` flag (0xe020) set, it crashes on purpose by copying from address 0, as the original does.
* **`IsoInitCRCCheck`:** the CRC32 (WAVEPLAY's ordinal 48, `calc_crc32`) of five tile and mask libraries, which the players compare before a network game.
* **Small ones:** `IsoInitSDLShowText`, `IsoGetWXPInvalid`, `IsoDebugScriptMessageProc`, `IsoInitNetworkSprites`.
* Checked: unit state log (867), menu tour, Human mission 1, Alt+F4. Also a two-player match: the host shows both chat lines, the joiner's first shot came out as garbage (as often), and both later shots show the match.

### Part 47: the terrain analysis' set-up and the scenario's AOAs and key terrain (`terrain.c`, 10 functions)
The AI's terrain analysis (0x10048270-0x1004b181) finds corridors, key terrain ("KT"), areas of approach ("AOA") and base locations on the sector grid. The shipped missions give their AOAs and key terrain as scenario records instead (`sdl_exec` hands them to `sdl_aoa` 0x1004b03a and `sdl_kt` 0x1004b119). Ported: those two; the set-up and teardown (`ta_init`: the corridor width and nearness limits from the grid size; `ta_free`; and 0x10049720, which clears a 0x800-byte table and which Ghidra misnamed `AfxResetMsgCache`); AOAs (`aoa_new`, `aoa_add_sector`, `aoa_free`); and key terrain (`kt_add`, which does nothing when one of the same type lies within `ta_near` sectors, and `kt_find`). The analysis' state stays in the original's data (0x10139e60-0x10139e84), and its lists use the original's list library (0x100c9cf0-0x100ca30a).
* Checked: the trace shows the unit state log run reaches all of them except `ta_run`. Unit state log at 952/953, the full range. Human mission 1 and Alt+F4 also checked.

### Part 48: the AI's task pool (`aitasks.c`, 12 functions, 0x1004baf0-0x1004c950)
Tasks are 0x74-byte records (G(0x1018b600)). Each AI player (G(0x1018b604), 0x143 bytes each) keeps its tasks in a doubly linked list (list 7, through the original's player-list helpers 0x100e2e62-0x100e2f15); the free tasks are linked from the players' block +4. `ai_run_tasks` (0x1004be3b) is one AI turn: each of the player's tasks takes a step (`task_step`), and when a task's steps are used up its handler runs (`task_run`, by type: 16 handlers, still original). A task without the repeat flag (+0x20) then finishes (0x1004c1bf, still original). The turn's time goes to the AI's timing log. Also `task_new` and `task_link`, and the getters and setters.
* Checked: the trace shows the unit state log run reaches `ai_run_tasks`, `task_new`, `task_link`, `task_step` and `task_run`. Unit state log at 858 and 951. Human mission 1 and Alt+F4 also checked.

### Part 49: the AI's targets and squads (`aisquads.c`, 10 functions, 0x1004c950-0x1004d392)
Squads are 0x130-byte records (G(0x1018b608)), targets 0x120-byte records (G(0x1018b610): +0x1c the owner, +0x20 the unit, +0x90 + player * 4 each player's squad that attacks it). Each squad keeps its targets in a list (+0x24 first, +0x28 last), through the original's per-player list helpers 0x1007225e-0x10072505.
* **The functions:** `ai_assign_targets` gives each target the player knows a squad (`target_assign`). It skips targets of allies and dead units, and, with the AI's fog flag (+0xa2c), unspotted ones. It takes the first squad of list 4 (units) or 5 (structures) that can hit the target (`squad_find_for`, `squad_can_attack`), or a new squad. Also `squad_add_target`, `squad_drop_target`, `target_tell_players`, `unit_attacked_by`, and `unit_tile_value`.
* **`ai_set_lethality`:** the summed squad strength scaled by the difficulty: none, x1.2 on the x87 truncated as `__ftol` does, x2, or x3.
* Checked: the trace shows the unit state log run reaches all of them except `unit_attacked_by`. Unit state log at 952-1002, the full range. Human mission 1 and Alt+F4 also checked.

### Part 50: neighbour offsets, external tables, cel offsets (`tables.c`, 6 functions, 0x1004e360-0x1004ea01)
* **`map_neighbour_offsets`** (0x1004e360, formerly `map_reset_objects` in `createmap.c`): the cell offsets of the eight neighbours for the map's row length, in the original's tables at 0x1017c4f8-0x1017c574.
* **`IsoLoadExternalTable`** (stdcall): loads a text file of comma-separated numbers into an engine resource's array (layers x rows x columns of ints). Spaces are dropped, `;` or `/` starts a comment, empty lines are skipped, and values starting with a letter leave the entry as it was. It reads line by line with `mmioSeek`/`mmioRead` and needs CR LF line ends.
* **Small ones:** `IsoSetCelOffset` (stdcall, moves two cels' offsets), `IsoFileExists`, `IsoGetShortUnitTypeString` (15-character names per race from a table at 0x1013b548), and the file list's comparator 0x1004e9d2 (`strcmpi`).
* Checked: unit state log (952-970), menu tour, Human mission 1, Alt+F4. Coverage from `live.txt` (map offsets, the external tables and the comparator run; `IsoFileExists`, `IsoGetShortUnitTypeString` and `IsoSetCelOffset` are not reached).

### Part 51: health bars (`healthbar.c`, 3 functions, 0x1004ea01-0x1004f45a)
Each unit has two small 8-bit DIBs (+0x90/+0x94) appended to its sprite as cels (+0x8c/+0x8e; for types 0x30-0x37 to the turret's sprite). They hold the health bar and, for the local player's units that have them, a row of pips above it (6x8 pictures, full or empty: the counts come from the type at +0xe0 and +0xf3, or for class 0x30 from the type's +0x28 and +0xdc divided by the tile record's +0x1c).
* **`unit_health_bar` (0x1004ebe6):** draws the bar. Network games use the palette's last four entries; single player uses 10, 0x4f, 0x77 and 0xdd, with the missing part in the high colour above half health and the low one below. With `hit` set, a unit at half health or less reports the damage, once every 500 ticks (+0x1b3). It was named `unit_selected` in five files and has been renamed.
* **`unit_health_bar_init` (0x1004ea01):** creates the two DIBs. It was named `unit_set_frame` in `unit.c` and has been renamed.
* **`unit_set_report_kind` (0x1004f34e):** sets the report kind +0x1bd (0x46 for structures, 0x40 for types 0x28-0x2f). It was named `unit_clear_path` in `unit.c` and has been renamed.
* Added `SpriteDibCreate`, `SpriteAppendBitmap` and `SpriteGetNumCels` to the engine imports.
* Checked: unit state log (952-990), Human mission 1, Alt+F4, and the orders test (`orders.sh`), whose shots came out pixel-identical to an original run.

### Parts 52-54: the radar, and small script exports (`radarmap.c`, `exports.c`, 15 functions, 0x1005077d-0x10052df4)
The radar has a sprite (I(0xbf1e)), a background DIB (I(0xbefe)), a fog DIB (I(0xbf02)) and a third DIB (I(0xbf06)) whose size sets the centre. The pitch is I(0xbf22); the scale is I(0xd70d)/I(0xd732), map pixels per radar pixel; the aspect ratio I(0xd733) is a float.
* **Dots:** `radar_plot` (2x2, formerly `unit_sound_stop` in `loop.c`) and `radar_plot_object` (4x4 framed: own player 0xdd, allied 0xbe, enemy 0x77; network games use each player's colour).
* **Scale:** `radar_set_scale` (formerly `map_reset_view` in `createmap.c` and `scenario.c`). An aspect ratio between -1.15 and 1.15 counts as 1. The original tests the aspect with the x87's "equal" flag, which a NaN also sets, and ours keeps that.
* **The background:** `IsoCreateRadarBgndBitmap`. Each radar pixel takes the colour of its map cell's tile from the scenario's `.TLH` file (the `.TL2` name with H), and the fog DIB keeps the cells already cleared. Also `IsoSetRadarBitmap`, and `IsoUpdateRadarFogBitmap`, which clears the fog where the map cells carry a player's seen bit.
* **The view box:** `IsoCreateRadarScreenArea`. Its size comes from the screen and the map on the x87, truncated as `__ftol` does, and divided by `fabs` of the aspect. Its corner colour is 0x5b for scenarios 0-3; the original leaves it unset for others.
* **Exports** (`exports.c`, all stdcall): `IsoGetUnitString`, `IsoCalcSliderPosition` and `IsoCalcSliderStartItem` (x87, 64-bit loads of the unsigned values), `IsoGetDibDimensions`, `IsoSetRace`, `IsoGetRace`, `IsoCampaignInfo`.
* Checked: the trace shows the unit state log run reaches all of them except `IsoGetUnitString` and the sliders. Unit state log 847-990 (an all-original run at the same time: 952-990), menu tour, Human mission 1, Alt+F4. In the two-player match the radar looks like the original's, but empty (see Open items).

### Part 55: the status line, scrolling the view, small exports (`status.c`, 6 functions, 0x10053e3b-0x10054b89)
* **`status_text` (0x10053ef5):** the status line at the bottom of the screen. It is drawn into the status DIB with BmpFont's `BFDrawTextShadow` (through the original's import, as in part 4) from a script variable, then the status sprite gets the redraw message (0x101572c0). The text is shown for `ms` milliseconds; with NULL the DIB is cleared. The callers in four files used it through `ORIG`.
* **`view_scroll_to` (0x100549b0, formerly `view_follow` in `loop.c`):** sets the engine's scroll position for a map point, offset by half the view, with an odd x made even. It also sets the view's edges and updates the radar's view box.
* **Small ones:** `IsoMouseInScrollRegion`, `IsoDebugStrIntf`, `IsoMemUsed` (`MemMgrSize`, added to the engine imports), `IsoSetGameState`.
* Checked: unit state log (952-990), Human mission 1, Alt+F4, and the orders test, whose status line ("Guarding specified unit/structure") matches the original. The only difference is the crater's animation, which varies between runs. The trace shows the state log run reaches `IsoMemUsed`, `IsoSetGameState` and `view_scroll_to`.

### Part 56: players, alliances, ground marks (`players.c`, 10 functions, 0x10055def-0x10056feb)
* **`ground_mark` (0x10056837):** puts a crater on a free map cell, picked with `srand(seed)` and `rand() % 5`: 0x3d-0x41 for big ones, 0x37-0x3b for small ones. Cells whose tile takes no marks (0x100f34a5) are skipped. Afterwards the screen is redrawn there.
* **Exports:** `IsoSetSpriteMerge` (`SpriteSetMergeDefault`, added to the engine imports), `IsoSetMissionEndCallback`, `IsoSetDifficulty` (0-2, otherwise 1), `IsoSetNumPlayers`, `IsoGetNumPlayers`, `IsoEnableAlliances` (when off in a network game, every player is allied with itself only).
* **Callbacks:** the two unit-kill callbacks 0x10056276 and 0x1005629c, and the game-over timer callback 0x100562c2.
* Checked: the trace shows the unit state log run reaches six of them, including the game-over timer and the ground marks. Unit state log (952-990), menu tour, Human mission 1, Alt+F4, orders test (only the crater animation differs).

### Part 57: the interface's spoken messages (`intfwave.c`, 5 functions, 0x10057deb-0x1005833c)
The interface speaks waves "int_%04d.wav" (0-99), one at a time from a queue of four kept in the original's data (0x1017c518: the wave, and the wave playing; -1 when empty).
* **`player_announce` (0x1005810f):** speaks a message to the local player, at once or queued. A message already waiting is not queued twice. Six files called it through `ORIG`.
* **`IsoIntfWaveQueueDone`:** when a message is done, the queue moves up and its next message starts.
* **Also:** `IsoAddIntfWaveToQueue`, `IsoJustPlayTheDamnWaveFile`, and the queue's reset 0x10057f48.
* Checked: unit state log (952-981), Human mission 1, Alt+F4. The orders test with `DOMINION_WAVE_LOG` played 59 waves, as did an all-original run, and the 20 interface messages are identical, volume and pan included. One weapon wave differed: the orders test runs on real time, so its fight varies a little between runs.

### Part 58: text sprites, preloading, special units, random numbers (`misc.c`, 8 functions, 0x10058a73-0x1005c073)
* **`IsoHideTextSprites`:** hides the scenario's "show text" sprites and, in network games, the five chat sprites.
* **`player_preload_units` (0x1005a67b, formerly `player_tech_changed` in `sdlexec.c`):** loads the resources of the units the player's race can build at its tech level.
* **`player_replace_d6cb` and `player_replace_d6f9` (formerly `player_unset_*` in `buttons.c`):** a player's special unit (types 0x50-0x53, or types 0x38-0x3a in action 'E') is replaced by another that fits, or cleared, when it goes.
* **`path_unescape` (0x1005bfc2, formerly `path_fix` in `filelist.c`):** turns "\\" into "\".
* **Small exports:** `IsoRand` (lo + `rand()` mod the range), `IsoBriefingScreen` (`RtSetBriefingScreenStatus`, added to the engine imports), `IsoInBriefingScreen`.
* Checked: the trace shows the unit state log run reaches all of them except `IsoRand`. Unit state log (952-990), menu tour, Human mission 1, Alt+F4, and the orders test, identical to one of the original runs in three of four shots.

### Part 59: what comes out of a destroyed unit (`eject.c`, 1 function, 0x10059be0)
`unit_eject`, called from `IsoDeleteUnit`.
* **A vehicle with seats** (type +0xe0, passengers at +0xf3, linked through +0xf6): its passengers are put out around it in turning directions and attack the unit that killed it (+0x18b) unless allied. A passenger dies if the vehicle's cell is taken, and passengers in a turret (types 0x30-0x37) die with it.
* **A structure** (types 0x40-0x83, not class 0x5c): leaves survivors from its type's list of up to four kinds and counts (a nibble each, in +0x2c or +0xc of the type by its level). They are added through `IsoAddUnit`, or `IsoRequestAddUnit` in network games, and face away from the killer or in a random direction.
* **Faithfully kept:** the original tests a second argument without effect. Unit ids passed on carry the upper half of a unit's address, as the original's registers did.
* Checked: the trace shows the unit state log run reaches it (which branch is not known). Unit state log 952-1001, Human mission 1, Alt+F4, and the orders test (the Recon destroyed): identical to one of the original runs in two shots, and in the other two only the crater animation differs.

### Part 60: races, tab sets, graphics quality, the copy protection (`prefs.c`, 8 functions, 0x1005c5b7-0x1005d140)
* **`IsoResetPlayerRace`:** changes all of a player's units to another race's types (type = class + offset).
* **`IsoGetTabsetParms` and `IsoSetTabsetParms`:** two banks of 0x1c-byte records at iso+0x8ea, each a value and five parameters; value -0x29a clears both banks.
* **`IsoSetGraphicsQuality`:** levels 1-3, giving the engine's two switches 1/1, 1/0 or 0/0 (`SpriteSetGraphicsQuality`, added to the engine imports).
* **`IsoCopyProtection`:** switched off; see `HIDDEN_AND_UNUSED.md`. The `rand()` call it makes is kept, since it moves the random numbers.
* **Also:** `IsoSetTextEntryFlag`, and the two `fabs` helpers 0x1005d110/0x1005d0f0, which return on the x87 stack (ours return `long double`).
* Checked: the trace shows the unit state log run reaches the tab sets, graphics quality, copy protection and `fabs`. Unit state log (952-990), menu tour, Human mission 1, Alt+F4.

### Part 61: the AI's building turn (`aibuild.c`, 2 functions, 0x1005d7c1-0x1005e5e0)
* **`ai_build_turn` (0x1005d7c1):** a computer player's building turn. Its four build slots (+0x13f, +0x13b, +0x143, +0x147, for tab sets 3, 0, 1, 2) are freed (the task finished) when the tab set no longer has a building. Then its build tasks are tried by priority: 10, then 7-9, then 3-6; any that succeeds ends the turn. If tasks were found but none could start, it waits; otherwise it tries priorities 0-3.
* **`ai_can_afford` (0x1005e4b6):** whether the player has the men (+0x10a) and materials (+0x10e) the next build asks for.
* Left original: the task's own try 0x1005db67 (reached only through ours).
* Checked: the trace shows the unit state log run reaches both. Unit state log (twice: 952-990), menu tour, Human mission 1, Alt+F4.

### Part 62: the game window's message handler and the drag box (`wndproc.c`, 5 functions, 0x1005e5e0-0x100600b8)
The plugin subclasses the engine's window; `iso_window_proc` (0x1005e5e0) sees every message first and passes on what it does not take.
* **Keys:** Ctrl, Shift and Alt are read on every key and button message (0xd797, 0xd7af, 0xd79b). The script's keyboard hook (0xa1e) gets key presses as messages (kind 1); its text hook (0xa1a) gets typed characters; Alt+F4 goes to its exit hook (0xe094); F10 and Pause are swallowed. In a network game the host's Escape asks every player still in to leave.
* **Mouse moves:** queued moves are merged into the last one. While Shift or Ctrl is held, waiting key presses other than digits, letters and 0x1b-0x2f are dropped. The cursor sprite follows the mouse, and an open popup menu follows it too.
* **The right button:** a press notes the time. A release over the map selects the unit under the mouse and opens the popup menu for it (`popup_menu_open`), unless the click was shorter than `PopupClickLength` with nothing under the mouse, or the unit is another player's, or a selection mode is on. A release over an open popup chooses its item.
* **The drag box** (0x1005f8c3, 0x1005fab2, 0x1005fb7f, 0x1005fe4b): starts on a left press inside the map's part of the screen, follows the mouse (kept inside that part), and moves as a whole while the right button is held. It is drawn with GDI by XOR (or by the engine under DirectDraw). On release it is ordered and handed to `map_click` and to the script's drag handler (a message of kind 6, with the box and Ctrl and Shift); Escape cancels it.
* **Map editor:** with no sprites shown and a path being edited, mouse messages go to the editor's path handler.
* Checked: the trace (popup test) shows all five are reached. Unit state log (952-953), Human mission 1, the orders, groups (drag box) and popup tests, menu tour, Alt+F4. In one groups run the right click deselected instead of opening the popup, like a later run of ours and an all-original run did not: which one happens depends on whether a frame (which sets the unit under the mouse, 0xd781) runs between the mouse move and the click.

### Part 63: the drag box exports, paint hooks, keyboard hooks (`rubber.c`, 13 functions, 0x100600b8-0x10060700)
* **`RubberBand`, `IsoRubberBand`, `RubberSetActiveBox`:** the script's drag handler, whether the map takes the mouse, and the part of the screen a drag is kept to. Switching off ends a drag going on; switching on hides the beacon directions and the power ring.
* **The engine hooks:** 0x10060166 ends a drag when the engine switches the plugin off. 0x10060191 is the sprite paint hook: it redraws the drag box where the engine painted over it, then calls the hook that was there before. 0x1006025a is the map editor's paint hook.
* **`IsoEditTextHook`, `IsoKeyboardHook`, `IsoExitGameHook`:** set the script's handlers. `IsoCtrlPressed` and `IsoShiftPressed` read the key state. `IsoSetIgnoreInputFlag` makes the window handler drop typed characters.
* **0x10060521:** the map editor's path editing with the mouse: pick or add a point, drag it, drop it.
* Checked (with parts 64-67): the trace shows the unit state log run reaches `RubberSetActiveBox`, `IsoRubberBand`, the paint hook, the keyboard and exit hooks and `IsoSetIgnoreInputFlag`. Unit state log (952-970, twice; one run 847), menu tour, Human mission 1, the orders, groups and popup tests, Alt+F4.

### Part 64: the plugin's start and end (`plugin.c`, 4 functions, 0x10060700-0x10060d40)
* **`RtLibInit`** (0x10060804) allocates the state block (0xe12f bytes). It then hands the engine the game-info hook (the tick function 0x100af179) and the paint hook. **0x100608c3** subclasses the window and looks up the other plugins (waveplay, netplay, bmpfont). It hands the network module the build's checksum and three `dominion.ini` settings (`SyncThreshold`, `NetworkPing`, `EnableTerrainAnalysis`; see `HIDDEN_AND_UNUSED.md`), seeds `rand()` from the clock and sets the window title to "Dominion".
* **`RtLibFree`** (0x10060700 and 0x1006071d) undoes all of this.
* The callbacks are passed at the original's addresses, so `DOMINION_ISO_ORIGINAL` still switches them.
* Left original: the DLL entry's helper 0x10060cfd (it runs before the hooks are in place), and `WEP`.
* A first version declared `SpriteSetPaintHook` cdecl; it is stdcall (the engine's `@16`), and the stack went wrong: a black screen from the start. Every call's stack fix-up in the original was then checked.
* Checked: as part 63; the trace shows `RtLibInit` and the setup run.

### Part 65: the first AI task handlers (`aitask1.c`, 10 functions, 0x10060d40-0x100616d4)
Each handler is run by `task_run` for its task type; next to each is the function that creates that kind of task.
* **Recon (type 1, 0x10060e41):** takes the player's recon squad, or makes one. It asks for recon units while the squad has fewer than wanted and the player has more men than the task's minimum, then sends the squad's units of types 0x1e and 0x23 out.
* **Scanning for threats (type 0):** runs `ai_assign_targets`.
* **Building (type 2):** runs `ai_build_turn`.
* **Keeping up the base (type 0xb):** hands a target to the base squad and removes a unit of kind 0x10-0x13 there, then checks the base's buildings and repairs (0x10092c44, 0x10092ade).
* **Type 0x16:** runs 0x100e3f0d between two debug prints.
* Checked: as part 63; the trace shows all ten run in the unit state log run.

### Part 66: building reserves (`aitask1.c`, 2 functions, 0x10062630-0x10062b12)
* **Creating the task (0x10062630):** a reserves task (type 0xe) is made only if the player has none.
* **The handler (0x100626bb):**
  * At first it notes the player's men (at most 500) and takes the reserve squad.
  * Later, a target it was given goes to the squad and is spread around the base (0x10093150, in turn over six places).
  * When no building is under way and the player can afford one, it asks for the first unit kind from 0xe down to 0 (not 8) the player's race can build.
  * Every tenth turn it reviews the squad (0x100933da).
* Left original, not reached in the tests: the other task handlers from 0x10062b12 to 0x10066fac. These are types 7, 8, 0xd, 0xf and 0x10-0x15, with their creators (about 17 functions, 21 KB).
* Checked: as part 63; the trace shows both run.

### Part 67: the occupied-cells map, distances, unit boxes, effect frames (`threatmap.c`, 12 functions, 0x10066fac-0x10067abc)
* **The occupied-cells map (0xe117):** a byte per map cell, 0-6, made when a scenario is loaded. Structures raise the cells they stand on (not unit types 0-7 and 0xc-0x17) and lower them when they go. Single cells can be raised (not cells flagged 1) and lowered. Reading a cell returns the upper bits the original's registers held, as the original did.
* **Distances (0x10067620, 0x10067664):** `hypot` truncated, and the distance along the axes.
* **`unit_screen_box` (0x10067690):** a unit's box on the screen, from its sprite's frame. Turrets are raised by their height.
* **Effects:** a timer per effect (0x100677e6) runs its kind's step from a table at 0x10157154. 0x10067883 shows an effect's next frame, and its second sprite's.
* Checked: as part 63; the trace shows all run except raising a single cell, lowering a unit's cells and the two distances.

### Part 68: projectiles (`projectile.c`, 2 functions, 0x10067acc-0x10068ba2)
A weapon's shot is an entry of the effects list (0x44 bytes) with a sprite of its own.
* **Launching (0x10067acc):**
  * The shot starts at the muzzle of the firing unit's current frame: the muzzle table (0x1988) is indexed by the facing and the frame, and mirrored sprites use the right side's frames for the left. Turrets add their mount offset (0x1964).
  * It aims at a point or at a target unit.
  * It gets a box around the aim point that counts as arrived: half the firer's accuracy radius, or twice the weapon's speed.
  * Its flight time is three times the distance divided by the speed. Its heading comes from the firer's facing, through the tables at 0x10140578 and 0x10140588.
  * The firer's cooldown and ammunition fields are updated, and the weapon's sound plays.
* **Flying (0x10068286):**
  * **Arriving:** if the target unit is still within 16 pixels of its box, it is hit, through its type's handler (table 0x101545c8). A random spread is passed with the hit: from the firer's accuracy, or from the weapon's spread when the firer has none. The random numbers are seeded from the firer's or the shot's serial number. Otherwise the ground is marked.
  * **Running out:** when the shot's range or time runs out, the ground is marked as well.
  * **Ending:** either way, the effect kind's end handler (0x10157158) runs.
  * **On the way:** the shot speeds up to full speed, and guided kinds turn towards the aim point by at most their turn rate per step (3600 units to a circle). With trails on, every few steps a trail sprite is left.
* **Faithfully kept:** unit and effect ids passed on carry the upper half of a pointer, as the original's registers did.
* Left original: the empty 0x10067abc (16 bytes, too short to hook).
* Checked: the trace shows both run in the unit state log run. Unit state log (952-1002, twice), Human mission 1, the orders test (fights; identical to one of the original runs in all four shots), Alt+F4.

### Part 69: beams and explosions (`projectile.c`, 4 functions, 0x10068ba2-0x1006a268)
* **Launching a beam (0x100692fe):**
  * Like `projectile_launch`, the start is at the firing unit's muzzle; this code is shared as `muzzle_point`.
  * There is no flight: the beam runs in a straight line from the muzzle to the aim point.
  * Weapons without frames have no sprite.
  * A firer hidden from view (unit +0x3e, bit 16) fires invisibly to players who are not its allies.
* **The beam's step (0x10069be4):** the beam moves along its line with `SpriteLinePoint` (added to the engine imports), and a hidden firer moves with it. At the end, for kind 2, the target is hit if it is still within the weapon's spread of the aim point (measured along the axes). The kind's end handler then runs.
* **Explosions (0x10069010, and 0x10069f63, which first notes the target's report kind):** the explosion sprite starts where the shot is, flipped by the firer's facing. It is noted in the target's attachment slot when the target has a free one, and its sound plays. The shot's entry is then cleared and freed.
* Left original, not reached in the tests: 0x10068ba2, 0x10068c89, 0x10068e16 and 0x1006a268 (another shot end, with a callback).
* Checked: the trace shows all four run in the unit state log run. Unit state log (952-1002, twice), Human mission 1, the orders test (identical to one of the original runs in all four shots), Alt+F4.

### Part 70: instant hits, area damage, attack checks, effect sprites (`weapons.c`, 6 functions, 0x1006ab64-0x1006be2a)
* **`effect_slot_take` (0x1006bbee):** takes a free effect sprite, one not in use or not visible. The search starts where the last one ended (0x1ac1) and wraps around once. The sprite is hidden, unflipped and emptied, and gets its owner's id.
* **`weapon_hit_instant` (0x1006ab64):** a bullet, with no flight.
  * The impact animation plays; on a turret it is at a random point of its box (0x1006bb01).
  * The sound plays.
  * The target is hit through its type's handler, with a spread from the firer's accuracy.
* **`effect_area_hit` (0x1006b00a):** area damage. Every unit in the effect's sector and its eight neighbours that is within the weapon's spread is hit. Excluded are the firer, optionally the target, turrets raised above a height (float 0x101130b8; a NaN counts as low, as the x87 test does) and units flagged 0x10e bit 15. The hit goes through a given function or the unit type's handler. The random numbers are seeded from the firer's id AND the victim's.
* **`unit_can_attack` (0x1006b6d3):** whether a unit may attack another.
  * The medic (type 0x21) may only treat friendly men.
  * Otherwise: not a unit under repair (+0x84 = 1), not a flagged or hidden target, not structures or units of types 0x40-0x43 as attackers, and allies only when asked. Units of kind 0x10-0x13 may only attack structures.
  * Weapon kind 5 hits only enemy ground units that are not turrets; kind 4 hits ground units that are not turrets and carry nothing, or units of type 0x40-0x4f.
  * **`selection_can_attack` (0x1006b633)** asks this for each of the local player's selected units.
* **Faithfully kept, with one exception:** the original passed unit ids with upper bits left over on its stack; ours pass 0 there.
* Checked: the trace shows all six run in the unit state log run. Unit state log (970 and 847, see Open items), Human mission 1, the orders, popup and hover tests (identical to one of the original runs in every shot), Alt+F4.

### Part 71: the weapon table and firing (`weapons.c`, 8 functions, 0x1006be2a-0x1006c6f6)
* **`IsoInitWeaponTable` and `IsoFreeWeaponTable`:** load the weapons' sprite resource and make the requested number of effect sprites, copies of the template at 0xc33b. Each table entry is 0xe bytes: an owner id, the sprite and its flags.
* **Firing (0x1006c18a at a unit, 0x1006c375 at a point; stdcall with the state first):**
  * The firing animation starts.
  * One effect is made for each of the weapon's shots (+0x54) and launched by its kind's launcher (table 0x10157148). The launchers are `projectile_launch`, `projectile_launch_beam` and `weapon_hit_instant` from parts 68-70.
  * Outside network games, a player that cannot see the attacker is warned (0x10090223).
  * The upper bits of the weapon id passed on are the attacker pointer's upper half, as the original's registers did.
* **`IsoWeaponMovementLoop`:**
  * Unless the game is paused, every effect in play takes as many steps (table 0x1015714c) as game ticks have passed; trails are left only when at most 2 have.
  * The `IsoKaboom` flag (0xe020, part 46) makes it crash on purpose, as in the original.
* **Also:** `IsoInitWeaponProcess` (trail sprites last 30 frames), `IsoStopWeaponProcess`, and `IsoSetWeaponDelay`, which does nothing.
* Checked: the trace shows all except firing at a point and `IsoSetWeaponDelay` run. Unit state log (951-990, twice), Human mission 1, the orders, popup and hover tests (identical to one of the original runs in every shot), Alt+F4.

### Part 72: attachment slots, clearing effects, starting an animation (`weapons.c`, 4 functions, 0x1006c6f6-0x1006dbc6)
* **Attachment slots (0x1006cc29, 0x1006cdc0):** a unit has two slots (+0x1bf) for explosions on it and a third (+0x1c3). A slot is free when it is empty, or when its effect sprite has stopped playing or now belongs to another unit; such a stale slot is cleared.
* **`unit_effects_clear` (0x1006d516):** stops a unit's weapon effects (+0x1b5, one per frame of its weapon) and notes its report kind.
* **`effect_start` (0x1006d9bd):** starts an effect animation from a cell range of the weapons' sprite resource. It sets the level, position, flip, blit procedure and delay, then posts the play message (its speed halved in network games).
  * It is skipped while the game is minimised (0x181).
  * It also depends on the detail setting (0xbeda): below 5 there are no effects, and below 10 half of them are left out (`rand()`).
  * The messages go through the original's global buffers (0x10157324, 0x10157348, 0x101572c0), as the original did, since the flip value stays in them for later effects.
* Left original, not reached in the tests: 0x1006c6f6-0x1006cc29, 0x1006ceec, 0x1006cff3, 0x1006d672.
* Checked: the trace shows all four run. Unit state log (980-990; a second run took the known tick-847 path, see Open items), Human mission 1, the orders, popup and hover tests (identical to one of the original runs in every shot), Alt+F4.

### Part 73: the AI scripts' condition helpers (`aiconds.c`, 6 functions, 0x1006f4d0-0x1006fd69)
* **`ai_compare` (0x1006f4d0):** a OP b for the operators 0-5 (<, <=, >, >=, ==, !=).
* **Conditions (0x1006f601, 0x1006f781):** a condition record holds:
  * how it is evaluated (every time, once, or never: a stored value);
  * its value;
  * the number of arguments (0-4) and the function.
* **`ai_squad_attacked` (0x1006f7e5):** whether a unit of the player's squad of a kind, or of any squad (-10), is under attack.
* **Also:** "difficulty OP value" (0x1006fd43) and a condition that is always true (0x1006f7be).
* Left original: 0x1006f7d3 (18 bytes, "always false": too short to hook) and the unreached condition functions between 0x1006f7a4 and 0x10070380.
* Checked: the trace shows all six run. Unit state log (980-990, twice), Human mission 1, the orders test, Alt+F4.

### Part 74: angles, sine and cosine, the colour map (`trig.c`, 7 functions, 0x1006fd69-0x10070c3b)
Angles are in tenths of a degree (0-3599).
* **`iso_angle` (0x1007042c, stdcall):** the direction of a vector, as the sprites face.
  * It starts from the C library's `atan2` (called in the original) times 1800/π (double 0x101130c0), truncated as `__ftol` does.
  * The result is turned by 90 degrees and mirrored.
  * It is then bent in eight pieces with steps of 1.5 and 0.75, so that the eight main directions fall on the isometric view's. These products are exact in double precision.
* **`iso_sin` and `iso_cos` (0x10070757, 0x10070380):** sine and cosine times 10000, from a quarter-wave table (0x10140760) after the engine's `NormalizeAngle` (added to the engine imports).
* **`colormap_load` (0x10070b60):** reads `COLORMAP.DAT` (0xc6000 bytes of player colour tables) and the hidden setting `[Dominion] NetColorMode` (see `HIDDEN_AND_UNUSED.md`).
* **Also:** three bit-field setters (0x100708b6, 0x10070a36, 0x10070a64).
* Left original: about 40 other bit-field helpers in 0x10070800-0x10070b44, many of them under 20 bytes and not reached.
* Checked: the trace shows all seven run. Unit state log (980-990, twice), Human mission 1, the orders test, Alt+F4.

### Part 75: the AI's target record fields (`targets.c`, 18 functions, 0x100721b0-0x10072550)
Getters and setters for the AI's target records (0x120 bytes each, G(0x1018b610)). Each record has its own links (+0x28 next, +0x2c previous) and per-player arrays of 8 (+0x30, +0x50, +0x70, +0xb0, +0xd0, +0xf0). The free list's head is the first int of the AI player block (G(0x1018b604)). The setters return the value they stored.
* Checked: the trace shows all 18 run. Unit state log (980-990, twice), Human mission 1, the orders test, Alt+F4.

### Part 76: making targets (`targets.c`, 2 functions, 0x10072550-0x10072aa5)
* **`target_new` (0x10072550):** a unit's target record, taken from the free list. It records the unit, its owner and its kind (0x1009dd57), and adds it to the owner's list.
  * The per-player links are reset for every active AI player (0x100e2db2), then linked into the owner's own list or the others' (0x10072b35, 0x10072c45).
  * The unit is told its record (0x1009f0a3), and the highest record in use (AI block +0xa40) is kept up to date.
* **`target_new_transport` (0x100728c9):** the same for transports: unit types 0x90-0x93 and 0x9c-0x9f, and with 0xdaae set also 0x8c-0x8f and 0x98-0x9b. The record has kind 2 and also notes the vehicle the unit is loaded in, or, if it is the topmost, the last of its cargo.
* Checked: the trace shows `target_new` run (transports: not in the tests). Unit state log (980-990, twice), Human mission 1, the orders test (the first shot differs from the originals by 85 pixels, all in the beacon's and the water's animation), Alt+F4.

### Part 77: linking, unlinking and freeing targets (`targets.c`, 10 functions, 0x10072aa5-0x10073750)
* **The two lists per AI player:** its own targets (list 0, links +0x30 and +0xb0) and the others' (list 1, links +0x50 and +0xd0). A target is appended with 0x10072b35 or 0x10072c45 and taken out with 0x1007307b or 0x10073175. The list heads and tails are kept by 0x100e2e62-0x100e2f15.
* **`target_free` (0x10072d31):** a target leaves the squads, tasks and lists, and is counted out of its owner's strength. Optionally the owner hears of the loss (AI event 5). The record then goes back to the free list. `unit_target_free` (0x10072aa5) does this for a unit's record.
* **`target_unlink_all` (0x10072eea):** clears a target from every active AI player's tasks (+0x44, +0x48) and squads (+0x2c), optionally not its owner's.
* **`player_targets_add` (0x1007326f):** counts a unit into or out of an AI player's strength: its value, its count per kind, and the value and count per class (men, light units, transports, the rest).
* **Also:** a squad's count of units (0x10073598) and whether two units are in the same or neighbouring sectors (0x1007364c).
* Left original, not reached: 0x100734df, and the drone exports `IsoCreateDrone`, `IsoDestroyDrone` and `IsoSetDroneType` (a unit sent along one of eight fixed paths; probably a developer feature).
* Checked: the trace shows all ten run. Unit state log (980-990, twice), Human mission 1, the orders test (the same animation-only differences as in part 76), Alt+F4.

### Part 78: selection modes, moving to a point, patrols, build-up, structure animation (`selmode.c`, `orders2.c`, 8 functions, 0x10073acc-0x10076f60)
* **`IsoActivateSelectionMode`:** a selection mode installs a handler for the next map click (0xc303, with the mode's two arguments). The window handler calls it on a left click (part 62).
  * Modes 1000-1100 (in steps of 10) select the handlers 0x10073f47-0x100758b9, through a two-level jump table checked case by case.
  * A handler gets message 2000 when it is installed (not for 1000 and 1050) and 3000 when it is replaced.
  * Any other mode switches the modes off and gives the map back its normal clicks.
  * The handlers themselves were not reached in the tests and stay original.
* **`unit_move_to` (0x1007618a):** sends a unit to a point: action 0x1f, the snapped target tile, a new path if it is not already there, and the point as a waypoint. Turrets, structures and units flagged +0x3e bit 25 are left alone.
* **`unit_patrol` (0x100764b2):** a patrol over up to 6 points with a delay at each. Mode 2000 ("LINEAR") comes back the same way, otherwise ("CIRCULAR") it starts over. The owner's AI hears of it (event 0xd).
  * **Original quirk kept:** the debug line prints the point count where the point's number was meant.
* **Build-up (0x10076af2, 0x10076ca3, 0x10076d00):** a unit's counter +0x88 under the state +0x84 (0 none, 1 done, 2 counting down, 3 counting up). It counts every other tick, up to 5 for the local player's allies and 9 for others; at the top the unit counts as built (0x1005c899).
* **Structure animation (0x10076de9, 0x10076e53):** the state +0x3a of types 0x40-0x87, moved on every other tick through a table (0x10146ac8) when the sprite has nothing queued.
  * The original returned an uninitialised value when there was nothing to do; ours returns 0.
* Checked: the trace shows all eight run. Unit state log (980-999, twice), Human mission 1, the orders, popup and groups tests (within their known noise), Alt+F4.

### Part 79: clear lines of fire (`lineofsight.c`, 8 functions, 0x100772d9-0x10078453)
* **`unit_fire_line_clear` (0x10077c54):** whether a unit has a clear straight line to its target, walking map cells in one of the eight directions (step table 0x1013b468: dx, dy for even and odd rows) for at most 20 cells. It returns 1 only if the target is reached after at least one step.
  * **Along an axis:** no cell may be blocked (cell bits 14 and 0), nor have a structure on either side (0x1007818a). A line that squeezes between two blocked cells counts as closed (0x100781e6, 0x1007831e).
  * **Diagonally:** raised cells (bit 15) are stepped over. A unit that is not a wall skips its first cell.
  * A unit whose route record in that direction is of kind 1 has no line at all.
* **Also:** route record kinds (0x100778fb, 0x10077aa1), walls (types 0x40-0x43, 0x10077303) and whether a cell is on the map (0x10077784).
* **`line_visit` (0x100772d9):** an empty hook called with every cell visited, left from debugging.
* Left original: 0x1007818a (19 bytes, too short to hook) and the unreached functions in 0x1007735c-0x10077b2b.
* Checked: the trace shows all eight run. Unit state log (970-990, twice), Human mission 1, the orders test, Alt+F4.

### Part 80: wall segments (`walls.c`, 5 functions, 0x100787e3-0x10079568)
A wall runs between two of a player's wall posts. Its segment record is 0x86 bytes, in a table at 0xe123 with 0xe11f entries. It holds:
* the two posts and the direction;
* up to 20 cells with their wall pieces;
* each post's route record per direction (+0x372) pointing back to it.
* **`walls_connect` (0x10078dd5):** connects two posts of the same player. Both need walls (+0x42) and must not be flagged +0x3e bit 12; with the check flag, they must not be too close (0x100d1e0f). The units standing in between take 50% damage (0x10078f1e). In mission state 4 the player hears message 0x26.
* **`wall_segment_build` (0x10078fa5):** builds a segment along a clear straight line (part 79).
  * It first splits the posts' existing segments in that direction, and reuses a matching segment or takes a free record.
  * It then fills in the cells with their wall pieces (0x10079669) until the second post.
  * An empty segment is cleared again.
* **`wall_segment_alloc` (0x10079430):** a free record, or the table grows by 50 (`winrealloc`, added to the engine imports).
* **Also:** whether a unit is a post (types 0x40-0x4f, 0x100787e3).
* **Faithfully kept:** the owner passed to the wall pieces carries the upper half of the post's pointer, as the original's registers did. One argument the original passed with leftover upper bits (to 0x10079c6c) is passed zero-extended.
* Checked: the trace shows all five run. Unit state log (980-1001, twice), Human mission 1, the orders test, Alt+F4.

### Part 81: wall pieces (`walls.c`, 5 functions, 0x10079669-0x10079c6c)
A wall piece is a sprite record (0xe bytes, at 0x1b13, 0xe11b of them; flags at +10: bit 0 in use, bits 1-8 the player, bits 9-11 the direction).
* **`wall_piece_at` (0x10079669):** places a piece at a cell.
  * The cell is walled (0x1007999f: blocked, bits 0 and 14).
  * The sprite is placed on the map and mirrored for directions 1 and 5, and its frame is set (0x1007c1d5).
  * For even directions, the free cells beside it are blocked and raised (0x10079a13, 0x10079bd6).
  * **Original quirk kept:** when no piece is free it returns 0, a valid piece number, rather than -1.
  * The original also walked the segment table in an empty loop here, with no effect; ours leaves it out.
* **`wall_piece_alloc` (0x100798c3):** a free piece, or 50 more (0x1007735c).
* Checked: the trace shows all five run. Unit state log (980-1001, twice), Human mission 1, the orders test, Alt+F4.

### Part 82: new wall posts, wall piece frames, `IsoInitEnergyWalls` (`walls.c`, 5 functions, 0x1007aa2b-0x1007cbde)
* **`walls_connect_new` (0x1007aa2b):** like `walls_connect`, for posts being built.
  * No new wall is made where a wall already runs that way (route kind 1).
  * When the posts are too close, a post standing between them splits the old segment instead (0x10079c6c), and nothing is connected.
* **Wall piece frames (0x1007c1d5, 0x1007c35e):** a piece shows the still frame for its direction (cel dir*9+4). With 0x11c set it plays its animation instead (cels dir*10 to +9, through the original's message buffers). 0x1007c35e redraws every piece in use.
* **`IsoInitEnergyWalls` (stdcall, 5 arguments):** makes the sprites, as copies of the given resources:
  * 100 wall pieces and 100 segment records;
  * the 16 beacon direction arrows;
  * 8 "umbDiamond" and 128 "umbNop" sprites.
  * It reports the ones that failed to load, as the original did. It does nothing in the map editor (no sprites shown).
* **`IsoHideBeaconDirections`:** hides the 16 arrows.
* Left original, not reached: 0x1007abf1-0x1007bd4c and the free function 0x1007cbde.
* Checked: the trace shows all five run. Unit state log (980-990, twice), menu tour, Human mission 1, the orders test, Alt+F4.

### Part 83: a new post's walls (`walls.c`, 4 functions, 0x1007d144-0x1007d592)
* **`post_new` (0x1007d144):** a placed post updates its walls, then 0x100f223a runs.
* **`post_update_walls` (0x1007d16c):** when a post's walls are due (+0x3e bit 5), its power state (`power_check`, part 2) decides what happens, and the flag is cleared:
  * powered (1) or state -0x7ffe: it is connected to its neighbours (0x1007d228);
  * state -0x7ffd: its walls are dropped (0x1007a8ab).
* **`post_connect_all` (0x1007d228):** connects the post to the posts its route records point at (ids 1-1999). In network games each new wall is sent to the other players (0x1003527c).
* **`post_find_walls` (0x1007d323):** looks in each direction, from the second cell and for up to 19 cells on the map, for the first post. It builds a wall to it if that post belongs to the same player, has walls and is not too close.
* Checked: the trace shows all four run. Unit state log (970-1001, twice), Human mission 1, the orders test, Alt+F4.

### Part 84: float vectors and score counters (`vecscore.c`, 9 functions, 0x1007e720-0x1007eb13)
* **Vectors (0x1007e720-0x1007e830):** a difference, the C library's `sqrt` of a float, and normalising (the unit vector from one point to another). The normaliser keeps the original's x87 rounding:
  * the squared length is summed in extended precision and stored as a float;
  * x is divided by the length at full precision, y by the length rounded to float.
  * The generated code was checked to round through memory where the original did (the build uses `-std=gnu99`).
* **Score counters (per player, at 0x1461 bytes):**
  * a destroyed unit counts as a loss for its owner and a kill for the attacker, per type (+0x2e59, +0x2ead) and in total (0x1007e870);
  * a built unit counts per type and in total (0x1007e943);
  * men and materials totals (0x1007e9ff, 0x1007eadf).
  * The per-type slot is the byte offset `(type & ~2) >> 1`, as in the original.
* Left original, not reached: the score functions 0x1007e9a1 and 0x1007ea33, `IsoUpdateScoreTable` and 0x1007f45e-0x100812c9 (the score screen).
* Checked: the trace shows all nine run. Unit state log (952-990, twice), Human mission 1, the orders test, Alt+F4.

### Part 85: turret aiming math (`aim.c`, 17 functions, 0x1008c755-0x1008cdf0)
* **`vec_angle` (0x1008c990):** the angle from one vector to another in degrees (0-360, counterclockwise): `acos` of the normalised dot product, mirrored when the cross product is negative. It is ported from the asm, since the decompiler's float casts were unreliable:
  * the lengths are rounded to float, the dot product is divided by their product in extended precision;
  * a quotient of -1 or below (or NaN) gives 180, and 1 or above gives 0;
  * the constants 180, π (as a float) and 360 come from the original's data.
* **`aim_round` (0x1008cc80):** rounds half up, with `floor` below .5 and `ceil` above.
* **`vec_dot` and `vec_cross` (0x1008caf0, 0x1008cb20).**
* **Wrappers:** each takes a float and calls the C library's `acos`, `sin`, `cos`, `floor`, `ceil` or `fabs`, returning on the x87 stack.
* **`aim_turrets` (0x1008c755):** points every turret (types 0x30-0x37) at the nearest unit of types 0x60-0x63 (0x1008c879, by `sqrt` distance) and resets its aiming state.
* Left original, not reached: the vector rotation 0x1008cb50 and the `fabs` wrapper pair 0x1008cd80/0x1008cda0.
* Checked: the trace shows all except the nearest-target search run (the mission has no such units). Unit state log (952-1001; one run agreed with an original up to tick 1749), Human mission 1, the orders test, Alt+F4.

### Part 86: the movement process and the game tick (`movement.c`, 5 functions, 0x1008ceac-0x1008dc1b)
* **`IsoMovementLoop`:** runs the game ticks that are due (0x17e5 - 0x17e1), unless the game is paused. Each tick:
  * the network and the tick's game logic (`net_pump`, part 46) run, with their time averaged (0xbee2);
  * the network status is checked every 5th tick, and the due timers fire;
  * the tick counter moves on, then the AI runs (0x100cc1be, unless switched off);
  * every unit gets its turn:
    * **its type's tick handler** (table 0x101545f0). Before it, a unit at rest looks for targets (0x100da072, nearer or farther by its weapon's range).
    * **its reload step** (0x1008dd2b, part 87; turrets 0x1008c643). Powered units (kind 0x48, 0x4c) reload more slowly as their power drops: not at all below 24%, every other tick up to 49%, three ticks in four up to 74%.
    * the power ring, and its type's after-move handler (0x101545bc);
  * then power and structures are updated, and in network games the host sends its sync messages.
  * The unit's type is read again after its tick handler, as the original does, since the handler can change it.
  * The `IsoKaboom` crash is kept.
* **The state log:** it used to redirect the tick call inside the original `IsoMovementLoop` (0x1008d375). Ours calls it through `statelog_tick` when the log is on; the call-site patch still serves runs that keep the original loop.
* **Also:**
  * `IsoSetMoveDelay`: milliseconds per tick. In network games only the host may set it, to at least 40 ms, and all players are told; a check for less than 6 below that test is dead code in the original.
  * `IsoInitMoveProcess`: 60 ms per tick, the tick counters reset.
  * `IsoStopMoveProcess`, and `IsoPauseGame` (single player, after the first frames).
* **Original quirk dropped:** the original read the unit's sprite before checking the unit for null.
* Checked: the trace shows all five run. Unit state log (980-990, twice, through the new hook), menu tour, Human mission 1, the orders and groups tests (the groups shots differ only in idle poses and the water), Alt+F4.

### Part 87: the reload step (`movement.c`, 2 functions, 0x1008dd2b-0x1008de6e)
* **`unit_reload_step` (0x1008dd2b):** a unit's weapon timer (+0xdc) rises by its rate (+0xe0) up to its type's full time (+0x28), and the type's pause (+0xfc) starts (+0xef). The game tick (part 86) calls it while the timer is below the full time.
* **`move_buffer_alloc` (0x1008de00):** allocates the 4000-byte buffer at 0x1014946c, once.
* Left original, not reached: `IsoGetPause` and 0x1008de6e-0x1008f31e.
* Checked: the trace shows both run. Unit state log (980-990, twice), Human mission 1, the orders test, Alt+F4.

### Part 88: path smoothing (`pathsmooth.c`, 1 function, 0x1008f31e-0x1008fa3f)
* **`path_smooth` (0x1008f31e):** finds a path across sectors (0x1008deb1), then moves each corner. The path record holds the number of points (+0), the sector of each point (+2 + k*2) and the points (+0x64 + k*4).
  * **Inside one sector:** the path is just the end point.
  * **Sharp bend** (the turn between the incoming and outgoing directions is 25 or less, 0x101131b0): the corner moves onto the sector's edge path, at its point for the corner's map cell or the nearest one.
  * **Almost straight:** the corner moves to where the line from the previous point to the next crosses the sector's gate (0x10090030), snapped to the edge path.
  * **The line misses the gate:** the corner goes to the gate's start or end, depending on which side it misses.
  * Either way, the edge path is walked forwards and, if that fails, backwards (0x1008fa3f).
* It is ported from the asm, because the decompiler lost the arguments of its twelve `__ftol` calls. The float steps keep the original's rounding, and a NaN takes the same branches as the x87 tests.
* Checked: the trace shows it run during the moves. Unit state log (980-990, twice), Human mission 1, the orders test (identical to one of the original runs in all four shots), Alt+F4.

### Part 89: building a unit's path, the turn angle, the gate crossing (`pathsmooth.c`, 3 functions, 0x1008fc3d-0x10090120)
* **`unit_path_build` (0x1008fc3d):** gives a unit a smoothed path (part 88) in its path cells (0x1984, 0x12 bytes each). Each point gets a cell (0x100e98de) with the point and flags:
  * bit 1: the caller's "last" flag, on every cell, or with unit +0x193 bit 2 only on the last;
  * bits 6 and 8: the caller's other two flags;
  * bit 0: set on every cell but the last.
  * The cells are linked in order (0x100d4450, which takes three arguments where the decompiler showed two), and the first is returned.
  * Infantry types (0-7, 0xc-0x17) path without the edge flag.
* **`vec_turn_angle` (0x1008ff50):** the angle between two directions, 0-180 degrees (like `vec_angle` without the mirroring, with its own operand order).
* **`segment_cross` (0x10090030):** where two lines cross, as fractions along each. It returns 0 when they are nearly parallel: |det| below 0.001, a double at 0x101131a8 that reads as a nonsense float.
* Checked: the trace shows the path building and the crossing run (the turn angle is not reached: no path in the tests has more than two points). Human mission 1, the orders test (identical to the originals within the usual noise), Alt+F4.
  * The unit state log was run seven times: 980 once, and splits at 159, 656 and 847 in the others.
  * Three runs with these three functions kept original split even earlier (368-491), and an all-original run made at the same time split at 656. The machine was busy (load about 4), and that alone moves the split points, as noted under "Checking the game logic".

### Part 90: a unit's sight and the fog (`vision.c`, 1 function, 0x10090223-0x10091400)
* **`iso_reveal` (0x10090223):** stamps a view shape onto the map around a point: round (by radius, table 0x10149634), or a cone facing one of eight directions (0x101494d8). The shape is walked row by row with the step table (0x1013b468).
  * **The radius:** for a unit it comes from its type's sight (`/ 64 + 2`, limited to 2-11). Turrets (types 0x30-0x37) see nothing of their own.
  * **Seen marks:** every cell in the shape is marked seen by the player (bits 24-31 of the map cell), and also by the local player when it shares its allies' sight (0xd9d2).
  * **Fog** (a byte per cell, 0x1960 +0x10): for the local player, its allies with shared sight, or when forced:
    * cells inside the shape (value 1) lose their fog;
    * cells on its edge (2) are collected, and get the fog tile (table 0x1014e9f0) that fits which of their eight neighbours are clear or lightly fogged.
  * **Redraw:** the screen area of the shape (by its direction) is redrawn, and the radar's fog updated.
* **Original quirks kept:** a neighbour off the map counts by the last value read, as in the original; and the debug lines for an unexpected tile (0x1e), through the debug log (0x10095f64).
* **Original limits not copied:**
  * the original kept up to 128 edge cells on its stack without a check, and counted them with a byte (more than 255 would never end); ours has room for 1024;
  * the original's first argument unit check reads the unit without a null check, as ours does.
* Left original, not reached: 0x10091400 ("Uncovering Allied Fog").
* Checked: the trace shows it run. Unit state log (980-1001 twice; an all-original run at the same time 980-990), menu tour, Human mission 1, the orders test (all identical to the original runs within the usual noise), Alt+F4.
  * The first two state checks after a reboot left empty logs: Wine's first starts after a reboot stall.

### Part 91: shade levels, the default ini, directions (`shade.c`, 4 functions, 0x10091550-0x10091a00)
* **`IsoSetShadeLevel` (stdcall):** loads one of 14 shade levels into the engine's shadow table (as ints, tables at 0x1014eb78) or its highlight table (as bytes, 0x1014f978).
* **0x10091790:** writes the game's default `Dominion.ini` into the Windows folder when there is none. The template's comments are in `HIDDEN_AND_UNUSED.md`.
* **0x1009188a:** the direction (0-7) from one point to another, by the engine's `GetAngle` (added to the engine imports, with `RtGetShadowTable` and `RtGetHiliteTable`), in 45-degree sectors.
* **Also:** an 8-byte table lookup (0x10091860).
* Left original, not reached: 0x1009167e.
* Checked: the trace shows all four run. Unit state log (980-990 twice, the same as an all-original run at the same time), menu tour, Human mission 1, the orders test, Alt+F4.

### Part 92: AI squads and the base's building list (`aibase.c`, 5 functions, 0x10092060-0x1009350e)
* **Squads:** the player's squad of a kind (0x10092060: -0x1e its base squad, -0x14 its first squad), and a new squad of a kind with a second squad of its own (0x10092102).
* **`ai_build_list_step` (0x10092c44):** steps through the base's building list, 18 steps of {kind, count} per race (0x10152418), one step per call.
  * If the player can afford it, has fewer of that kind than the count, and the task is not the one already building, it asks for one (0x1005d140).
  * It first reserves the costs plus 10 (0x1005e1c0) when they are at least its minimums.
  * A first power plant (kind 0x17) sets +0x14b.
  * The decompiler's offsets here were 0xc too low: the code addresses the AI block plus 0xc.
* **`ai_rebuild_lost` (0x10092ade):** while the player has lost buildings, gives a rebuild task (0x100616d4) for each of its squad's targets that its race can rebuild, is not being built and is not flagged.
* **0x100933da**, the squad review, has nothing left but its timing call.
* Left original, not reached: 0x100921f5-0x10092ade, 0x10092dfc-0x100933da (base repairs, sending units back to base).
* Checked: the trace shows all five run. Unit state log (980-990 twice, the same as an all-original run at the same time), Human mission 1, the orders test, Alt+F4.

### Part 93: colour lookup tables and numbered groups (`clut.c`, `groups.c`, 7 functions, 0x100945da-0x10095d80)
* **`IsoLoadClut` and `IsoLoadBlut`** (C++ names `?IsoLoadClut@@YAHPAXI@Z`, `?IsoLoadBlut@@YAHPAX@Z`): load the blitter's lookup tables through MMIO and hand them to the engine (`RtSetBlitLookupTable`, added to the engine imports).
  * `clutNN.dat`: 64 KB, table 2.
  * `blut.dat`: 1280 bytes, widened to ints, table 1. As in the original, when only one of its two allocations fails, the other is not freed.
* **Numbered groups:** a unit's group is at +0x15; the members of player p's groups are at 0x1017c8a0 + p*0x28.
  * The unit's group entry (0x100950c0), taking it out of the group slots (0x1009528d), and its slot (0x1009530a, units of kind 0xc).
  * **`group_attack` (0x10095c3b):** the decompiler took it for fastcall; it is cdecl (unit, target). The group's members (same player, kind 0-7, not flagged +0x10e bits 23, 17 or 3) that can attack the target are ordered to.
* **Also:** a record table of 0x26 bytes each (0x10094d90).
* Left original, not reached: `IsoFreeColorLookUpTables`.
* Checked: the trace shows all except the group slot run. Unit state log (980-990, and once identical to an original run up to tick 1677; an all-original run at the same time 980-989), menu tour, Human mission 1, the orders and groups tests, Alt+F4.

### Part 94: debug output and AI timing (`debugtime.c`, 5 functions, 0x10095e98-0x100962f0)
* **The debug print (0x10095e98), the debug log (0x10095f64) and `IsoDebugFile`:** switched off before release. Each returns 0 at once; their bodies are still in the original but never run. They printed by level through the flags at iso+0xe09b and wrote debug files.
* **AI timing:** the start of an AI step is noted (0x10096250, at 0x1017c9e4). At its end (0x1009626b) its time is printed when timing output is on (0x1017c9e0), and the slowest is kept (0x10152b58).
* Checked: the trace shows all five run. Unit state log (980-1002; an all-original run at the same time 980-989), menu tour, Human mission 1, the orders test, Alt+F4.

### Part 95: the path table (`pathtable.c`, 5 functions, 0x100983ee-0x100989da)
* **The path table** (iso+0xe113, 0x20 bytes): +0x14 the number of cells, +0x18 rows, +0x1a columns, +0x1c a byte per cell (0xff blocked, 0xfe open); +4/+8 the sectors, +0x10 a second list.
  * `path_table_fill` (0x100983ee): blocked cells (map bit 0) count as open unless what stands there is of kind 0x88-0x9f.
  * `path_table_init` (0x10098542): (re)makes and fills it at the map's size; in the map editor also the editor's path buffer.
* **Lookups:** the table's value for the cell under a point, or 0xff (0x100987c8); whether a point is on the map (0x10098865).
* **0x10098970** walks a path's 20 sector slots without any effect; ours does nothing.
* Checked: the trace shows all five run. Unit state log (948 once, then 980-989 and two more runs 980-1677; an all-original run at the same time 980-981), menu tour, Human mission 1, the orders and hover tests, Alt+F4.

### Part 96: the path sector overlay and the AI's records (`pathtable.c`, `aislots.c`, 6 functions, 0x1009a526-0x1009c8a9)
* **`path_sectors_draw` (0x1009a526):** a debug overlay of the path sectors, drawn when iso+0xeeb bit 2 is set. For each sector it draws its box's diagonal, lines from its centre to its up to 20 linked sectors' centres, and its number.
* **`EDITPATH_GetEditMode`:** the path editor's mode (0x1017c9e8).
* **The AI's global conditions:** 29 of 0x18 bytes at 0x101534f0. `ai_records_init` (0x1009c5c0) updates those of type 2.
* **The AI's records:** 60 records of 400 bytes (0x1017df44).
  * `ai_records_init` (0x1009c5c0) makes the empty table.
  * `ai_record_add` (0x1009c76a) copies a record into the next slot, keeping the slot's number. It counts the record for all players (AI +0xa26) and for its own player (+0x6e).
  * Lookups: 0x1009c86a and 0x1009c887.
* Checked: the trace shows all six run. Unit state log (970 once, 980-1002; an all-original run at the same time 980-1001), menu tour, Human mission 1, the orders test, Alt+F4.

### Part 97: a unit's order, state and action (`unitorder.c`, 4 functions, 0x1009cc60-0x1009d1ff)
* **Orders** (+0x148, a character code) with a lock (+0x147, set by 0x1009cc60): 'L' and ' ' block new orders, '!' lets only 'L' through. A busy unit (+0x3e bit 22) takes none.
* **`unit_order` (0x1009cc91):** in a network game, for the local player's units, with a handler (0x10136610, 8 bytes per order) and `send` set:
  * the order's time (+0x149) is set if none yet: now + the network delay + `delay`;
  * a changed order is passed to its handler.
* **`unit_set_state` (0x1009ce9f):** leaving state 0x1e calls the unit type's leave handler (0x10118d90), except for kinds 0x40-0x9f.
* **`unit_set_action` (0x1009cf5b):** works the same way with its own handlers (0x101368a8, 9 arguments).
  * In a network game, actions 0xb and 0x18 that are still waiting for their time (+0x199) are not replaced.
  * A unit leaving state '#' (being built) counts as built for its player.
* Checked: the trace shows all four run. Unit state log (970 once, 980-981; an all-original run at the same time 980-1002), Human mission 1, the orders, groups and hover tests, Alt+F4.

### Part 98: unit lookups and the AI's debug names (`unitinfo.c`, 13 functions, 0x1009da08-0x1009f8ec)
* **Lookups by unit id** (0 for none): the type's first table value (0x1009da08); one of four type record values (0x1009dbc2); the kind divided by 4 (0x1009dd57); whether it is being built (state '#', 0x1009ebb9); whether a player may order it (0x1009efda: its mask +0x2f, not busy, +0x84 not 1). Also a setter for +0x1b1 (0x1009f0a3), and a stop (0x1009f861: a move to where it stands).
* **`unit_valid_target` (0x1009e5d8):** the unit's attack target (+0x18b) if it can still attack it. Only the low 16 bits count; the upper ones come from the check's result, as in the original.
* **Names**, copied out, each returning the destination (callers use the result):
  * the unit type names (0x1009ddd0, 0x1013bee8);
  * the AI's debug names for task types, task states, attitudes and squad kinds (0x1009de5b, 0x1009e0e3, 0x1009e26a, 0x1009e31a). The jump tables were decoded to check each case.
* Checked: the trace shows all 13 run. Unit state log (970 and 998; the 970 fork is shared with all-original runs, see TESTING.md), Human mission 1, the orders, hover and popup tests, Alt+F4.

### Part 99: player counts, unit health and `IsoBlastText` (`playerinfo.c`, 8 functions, 0x100a0285-0x100a2550)
* **Lookups:** a player's men (+0x1b97, 0x100a02b6) and materials (+0x1b9b, 0x100a0285); the percentage of the map it has not seen (0x100a04d3); the difficulty (0x100a04f4).
* **0x100a2200:** sets a unit's +0x2a flag. Clearing it calls its type's leave handler, as `unit_set_state` does for state 0x1e.
* **`IsoBlastText`:** draws a script string with a font resource through BmpFont's `BF_BlastText`, in colour set 1 or 2. Not seen in the in-game trace.
* **A unit's maximum health** at an upgrade level (0x100a23e0): the type's base, plus a bonus from level 2. Only the low 16 bits count at levels 0-1; the upper ones are the type pointer's, as in the original.
  * The type's percentage of it (0x100a24d0) is computed in x87 order.
* Checked: the trace shows all except `IsoBlastText` run. Unit state log (980-1001; an all-original run at the same time 980-981), menu tour, Human mission 1, the orders and hover tests, Alt+F4.

### Part 100: the counters, build levels and men production (`buildinfo.c`, 4 functions, 0x100a25d0-0x100a3036)
* **`counters_update` (0x100a25d0):** writes the local player's men and materials into the interface's counter strings, draws them with BmpFont's `BFDrawTextShadow`, and has both counter sprites redrawn.
* **`build_level_update` (0x100a27f1):** a player's build level per unit kind (iso+0x182 + player*0x29 + kind/4), the best upgrade level of that kind it has. Afterwards `IsoUpdateIntfOptions` updates the interface.
  * Mode 0 sets the level, mode 1 raises it, and mode 2 recounts it from the player's units.
  * Kind 0x7c is also counted at +0x2e01, which raises the men limit.
  * For the local player, losing the last unit of kind 0x54 hides the radar.
* **0x100a2ab0:** sends script message 4 to the interface.
* **`building_produce_men` (0x100a2d71):** when a building's time has come, it adds its output (0x100a36d6; four times as much from upgrade level 2) to its player's men, up to the player's limit and at most 9999, and scores it.
* Checked: the trace shows all four run. Unit state log (951 once, the timing branch first seen in part 71's run, then 980-1002 in three runs; an all-original run at the same time 980-981), menu tour, Human mission 1, the orders and hover tests, Alt+F4.

### Part 101: mining (`mining.c`, 3 functions, 0x100a3036-0x100a37fa)
* **Energy wells** (iso+0x1097, 0x2b bytes each): +0 the sprite, +0xc what is left, +0x10 what it had, +0x14 the state (2 being mined, 3 empty), +0x16 the fill frame, +0x1f/+0x23 an extra sprite and its object, +0x27 the owner.
* **`building_output` (0x100a36d6):** a building's output is its maximum scaled by its health (rounded to a double, as in the original) and by its grid's spare power, plus 1, at most the maximum. It is 0 while the building is not working.
* **`refinery_mine` (0x100a3036):** when its time has come, a refinery adds its output to its player's materials (at most 99999), or half each (at least 1) to its player and the partner at +0x2deb.
  * The well loses what was taken; `well_fill_frame` (0x100a35ab, 4 frames by what is left) updates its sprite.
  * An empty well is marked, announced to its owner (wave 0x50), stopped, plotted on the radar and reported to the AI. Its refinery is set idle.
* Checked: the trace shows all three run. The x87 sequences were compared with our build's disassembly. Unit state log (980-990 twice; an all-original run at the same time 980-1002), menu tour, Human mission 1, the orders test, Alt+F4.

### Part 102: the build buttons (`buildinfo.c`, `IsoUpdateIntfOptions`)
* **`IsoUpdateIntfOptions`** (stdcall) decides which of the 40 build buttons a player may use.
  * A button (table iso+0x18dc: -0x29a for none, +8 its unit kind) is on when:
    * the kind is allowed for the player's side (table 0x1968);
    * the player has fewer than 250 units;
    * every technology it needs (table 0x18e4, up to 10; kind + 1000 for upgrade level 2) is at the needed build level.
  * Exceptions:
    * kind index 0x14 is on without its needs while the player has none;
    * kinds 0x30-0x37 are off unless 0x1008bd1f finds something for the player;
    * transports are off while +0x2dd6 is set.
  * For the local player, each button's state (5 off, 10 on) is set and newly available buildings (wave 40) and units (41) are announced. At 250 units it says so.
  * Buttons turned off cancel their build slot (0x100a3ef9).
  * As in the original, the unit count is read through the previous `g_iso` before the passed one is set.
* **Fix to part 100:** 0x100a2ab0 returns `RtRunMsg`'s result, which `IsoUpdateIntfOptions` passes on.
* Checked: the trace shows both run. Unit state log (980-1715, one run identical to an original to the end; an all-original run at the same time 980-981), menu tour, Human mission 1, the orders, popup and hover tests, Alt+F4.

### Part 103: cancelling a build slot and the counters' setup (`buildqueue.c`, 3 functions, 0x100a3ef9-0x100a43cd)
* **Build slots:** four per player at iso+0x2ca + player*0xc4 + slot*0x31. +0 holds the unit kind (-1 for empty); see part 104 for the rest.
* **`build_slot_cancel` (0x100a3ef9):** cancels the player's slot building a unit kind index (slots 2-3 only once started). The interface (local player) or the AI is told, then the slot is cleared.
* **`IsoUpdateMMString` and `IsoInitMenMatStrings`** (both stdcall) redraw the men and materials counters and set up their script variables, sprites and positions.
* Checked: the trace shows all three run. Unit state log (847 once, the known frame-timing split, and 1677, identical to an original to the end; an all-original run at the same time 980-981), menu tour, Human mission 1, the orders and popup tests, Alt+F4.

### Part 104: build progress and the interface's build tables (`buildtick.c`, 7 functions, 0x100a4a14-0x100a5c00)
* **The rest of a build slot:**
  * +4 its total time (0 when idle), +8 its progress;
  * +0xc/+0x10 the men and materials it costs, +0x14/+0x18 what has been paid so far;
  * +0x1c set when the interface finishes it;
  * +0x20/+0x24 the tab and bar light sprites, +0x28 the light frame.
* **`build_slots_tick` (0x100a4a14):** each started slot pays this tick's share of its cost (cost * progress / time, less what was paid) and moves on.
  * A player short of men or materials stops all building for this tick, and the whole step ends there (returning 0). The local player is told once (waves 0x24 and 0x23).
  * For the local player the lights show the progress in 32 frames. When the tab light is missing, the bar light gets the previous message, as in the original.
  * A finished slot places its unit (0x1009f6e0) or starts its building (0x10092379), or is left to the interface. The step ends at the first finished slot, so later players wait a tick.
* **Exports:** `IsoSetTabset`, `IsoInitIntfOptions`, `IsoRecalcBuildInfo` (recounts the build levels from the map, then `IsoUpdateIntfOptions`), `IsoInitEconomyTable`, `IsoInitTechDependTables` (all stdcall) and `IsoUnitIsBuilt` (cdecl).
* Checked: the trace shows all seven run. Unit state log (980-1001; an all-original run at the same time 980-1001), menu tour, Human mission 1, the orders, popup and hover tests, Alt+F4.

### Part 105: energy wells appearing and finding a well (`wells.c`, 2 functions, 0x100a621e-0x100a655f)
* **`wells_appear_tick` (0x100a621e):** a well that is appearing (state 1) steps through frames 1-5, one each time its timer (+0x17, interval +0x1b) runs out.
  * After frame 5 it shows its fill frames and becomes active (state 2).
  * It is plotted on the radar (except in the map editor) and loses its extra sprite.
* **`IsoFindActiveEnergyWell`** (cdecl): the well at a position and its state. In the map editor any well counts, otherwise only placed ones.
* Checked: the trace shows both run. Unit state log (952 once, the branch reference run 4 takes, then 980-990; an all-original run at the same time 980-1001), menu tour, Human mission 1, the orders test, Alt+F4.

### Part 106: the flip timer and the sector tables (`sectors.c`, 2 functions, 0x100a6df0-0x100a73ce)
* **`IsoInitFlipProcess`** (stdcall): starts the frame flip timer (iso+0xd83f/+0xd843).
* **`sectors_init` (0x100a6e82):** builds two tables for the map.
  * The sector table (iso+0xc2eb, 0x1e bytes per sector, one sector per 16 rows by 4 columns of cells): each sector's eight neighbours (-1 off the map, checked edge by edge from the decoded jump table), and a flag when its corner cell is blocked.
  * A byte table at iso+0x17a7, one byte per block of 2 rows by 1/2 column.
* Checked: the trace shows both run. Unit state log (980-1002; an all-original run at the same time 980-990), menu tour, Human mission 1, the orders and groups tests, Alt+F4.

### Part 107: free ids and the unit and object lists (`unitlists.c`, 7 functions, 0x100a7447-0x100a78fe)
* **`unit_id_free` (0x100a7447):** a free unit id (1-2000). The search starts 250 past the last id used (iso+0xd67e), so freed ids are not reused at once.
* **`object_slot_free` (0x100a751c):** the first free object slot. Slot 0 and "none" both return 0, as in the original.
* **The lists:** units on the map (iso+0xd67a, at most 2000) and active objects (iso+0xd6e3, at most 256).
  * Adding and removing: 0x100a7587/0x100a75e6 and 0x100a76e3/0x100a7742.
  * A removal moves the rest down through the original's copy routine, one entry past the end, as the original does.
* **0x100a783d:** scrolls the view to the player's first start marker.
* Checked: the trace shows all seven run. Unit state log (980-1000; an all-original run at the same time 980-981), menu tour, Human mission 1, the orders and groups tests, Alt+F4.

### Part 108: log2 and the sprite setup (`spriteinit.c`, 3 functions, 0x100a78fe-0x100a7f17)
* **0x100a78fe:** the integer log2 (stdcall).
* **`IsoInitSprites`** (stdcall, 10 arguments): sets up the cursor, the main sprite and its first five frames' data. With sprites shown, it also sets four interface values and loads two resources.
* **`IsoInitRendezvousSprites`** (stdcall, 11 arguments): the two command point sprites and eight colour bytes. The local player's command points that are already set are placed again.
* Checked: the trace shows all three run. Unit state log (980-1001; an all-original run at the same time 980-989), menu tour, Human mission 1, the orders test, Alt+F4.

### Part 109: `IsoInitEngine` (`engineinit.c`, 0x100a7f17-0x100a9195)
* **`IsoInitEngine`** (stdcall, 12 arguments) starts a game:
  * reads `DebuggingOn`, `RadarUpdateTime`, `AutoSaveTime`, `KeyboardScrolling` and (with sprites) `BrightnessEffects` from dominion.ini;
  * clears the game and player state;
  * gets the engine's shadow and highlight tables;
  * seeds the random numbers with 1. This goes through the original's C library (`_srand`), whose `rand()` the game logic still uses.
* **Tables it allocates:** units (8000 bytes), objects, active objects, the unit list, markers (0x800), timers (0x20), vectors (0x1000, chained), the per-player selections and the 0x200 slots (chained both ways), and the build slots. It also loads the colour tables and the unit type, object and other resources.
* **Return value:** 1, or a negative code naming the table that could not be allocated, as in the original.
* **Engine imports added:** `SpriteSetBkgTransparent`, `SpriteSetSpotColor`.
* Checked: the trace shows it run. Unit state log (980-1002; an all-original run at the same time 980-981), menu tour, Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 110: small engine exports (`engineexp.c`, 6 functions, 0x100a9c54-0x100aa238)
* **Exports (all stdcall):** `IsoInitReconnectAndPauseWindow`; `IsoFindUnit` (the first unit whose sprite bounds, kept at +0xc6, hold a point); `IsoSetPlayerId`; `IsoEnableAmbients`.
* **`IsoSetDebugMode`** sets the debug mode, except for the value 666: that toggles a flag that shows or hides every building's map-marker sprite (see HIDDEN_AND_UNUSED.md).
* **`IsoAddUnit`** takes the next unit id (iso+0x1ad1), counts the unit as built for its player (a transport under construction as kind 0x8c) and creates it (0x100aa238).
  * The original passes the id with a stack leftover in its upper half, which the callee masks. Ours passes 0 there.
* Checked: the trace shows all six run. Unit state log (970 twice: one run identical to part 98's 970-branch run for the whole game; an all-original run at the same time 980-990), menu tour, Human mission 1, the orders and hover tests, Alt+F4.

### Part 111: creating a unit (`unitcreate.c`, 0x100aa238-0x100ab4aa)
* **Transports** (kinds 0x88-0x9f) are made by 0x100b0837. Built ones get their time, and the kind is adjusted for parts.
  * When loading a game (a10 set), the two halves of a two-part transport are linked (+0xf6/+0xf8, through iso+0xd9de).
* **Other units:**
  * Units built by the player (flags bit 0) come out of the player's main plant (iso+0xd6cb, replaced first if it has been sold).
    * Without one, the message "Main Plant destroyed" is shown and nothing is built.
    * A plant of kind 0x50 puts its units halfway down its sprite.
  * The record (0x3a9 bytes) is allocated or cleared, the next free id chosen, and `IsoAddUnitType` sets the unit up.
  * In a network game, while the game is running, the unit is sent first (0x100332af) and made when the message comes back. A building whose footprint is blocked is dropped.
* **After setup:**
  * the wall cell under a wall post is marked;
  * a local refinery shows its well;
  * the AI is told, and the counts and build levels go up;
  * power is marked for buildings;
  * the map editor gets its extra record.
  * Building in a damaged plant (+0x3e bit 12) costs the plant a third of its health plus 1, and can destroy it.
* **Upper bits:** a few calls pass values whose upper bits were stack leftovers or pointer bits. The callees read only the low bits; ours passes 0 there.
* **Coverage:** the all-original trace shows the main plant branches run in the state check (0x1005ab95, 0x100eb80a), and our state logs match through them. Transports are not created in the tests.
* Checked: the trace shows it run. Unit state log (970 and 980-1001; an all-original run at the same time took the 970 fork too), menu tour, Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 112: `IsoDeleteUnit` (`unitdelete.c`, 0x100ab4aa-0x100ac51d)
* **`IsoDeleteUnit`** (stdcall: iso, id, mark) removes a unit. In order:
  * Its passengers are ejected, and its player is noted as having lost a building of a kind it had (0x1017df88).
  * A damaged-plant flag is handled, and the AI is told.
  * Its links are cleared: the paired unit, units targeting or following it, groups, main plant, and power icons.
* **Effects and cleanup:**
  * Units of types with a death blast make their sound and blast (a turret's raised by its height), plus a second brightness blast for kinds 0x18-0x3f.
  * Its refinery's well is released.
  * Its sprites, turret, extra offsets and map marker are freed. The marker's screen area is redrawn afterwards.
  * Its sector is updated, and its footprint or ground mark left.
  * Its kind's two handlers (0x101545e0, 0x101545ec) are called, and it is taken out of the lists, selection and radar.
  * Transports are unpaired, the build levels recounted, and its records freed.
* **Counts:** the events are counted, the player's counts updated, and the player checked for defeat.
* As before, a byte the original passes with a stack leftover above it (0x100b1e2c) is passed zero-extended.
* Checked: the trace shows it run, with the ejection, blasts, ground marks and footprints (0x10059be0, 0x1006d9bd, 0x10056837, 0x100ef91a). Unit state log (980-1749, one run identical to an original to the end; an all-original run at the same time 980-990), menu tour, Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 113: `IsoDeleteAllUnits` (`unitdelete.c`)
* **`IsoDeleteAllUnits`** (stdcall) frees every unit's sprites, turret, offsets and memory, without IsoDeleteUnit's effects and counts.
  * It clears the selections and the unit list.
  * Every map marker is sent its last message and deleted.
* Checked: the trace shows it run (at the start of a game). Unit state log (970 and 980-1002; an all-original run at the same time 980-1001), menu tour, Human mission 1, the orders test, Alt+F4.

### Part 114: `IsoAddObject` (`objects.c`, 0x100aca6e-0x100ad1d9)
* **Map objects** (iso+0x192c, 0x800 slots of 0x12 bytes) hold a sprite object, a position, type, flags and slot number. The object types table is at iso+0x1930 (resource, frames, frame delay).
* **`IsoAddObject`** (stdcall, 7 arguments) copies the type's resource (objects 0xb4-0x103 in the map's variant) and sets its level and position.
  * Objects 0xaa-0xb1 are offset by a building kind's footprint.
  * Each object is shown in a way that fits its type: last frame, or a message when the editor's flag 8 is set. Animated ones (flags bit 4) loop.
  * The object marks its map cells (0x100ad5f2) and goes on the radar.
* **Beacons:** a beacon (0x122/0x123) at a place that already has one returns that one; a 0x123 replaces a 0x122.
* **Linked objects:** object 0x10b gets a 0x118 above it.
* **Network:** in a network game, objects are shown only once the game runs.
* Checked: the trace shows it run. Unit state log (980-1000; an all-original run at the same time 980-981), menu tour, Human mission 1, the orders, popup and hover tests, Alt+F4.

### Part 115: energy wells and the object cells (`objects.c`, 2 functions, 0x100ad1d9-0x100ad985)
* **0x100ad5f2:** marks the map cells an object covers (objects with flags bit 0 only). The cell is blocked; with footprint 1 (type table +0x10) its four neighbours are too.
  * Energy wells mark their cells as wells: bit 29 for the centre, bit 30 for the others, and bit 1 cleared.
  * With the editor's grid on, the cells are coloured.
* **`IsoAddEnergyWell`** (stdcall, 8 arguments) places a well's object and fills its record: materials (at least 1 at the start) and an appearing interval (200 by default).
  * In the map editor, or with an interval below 2, the well is active at once.
  * Otherwise it appears frame by frame (part 105), with a glow sprite unless `quiet` is set.
* Checked: the trace shows both run. Unit state log (980-1001; an all-original run at the same time 980-1001), menu tour, Human mission 1, the orders and hover tests, Alt+F4.

### Part 116: showing the map objects (`objshow.c`, 2 functions)
* **`IsoInitObjectDestructionInfo`** (stdcall): the objects' destruction table (iso+0xd9e2).
* **`IsoShowObjects`** (stdcall): hides all map objects, or shows them as `IsoAddObject` does.
  * When showing, animated objects restart, and appearing wells and their glows are shown again.
* Checked: the trace shows both run. Unit state log (970 and 980-981; an all-original run at the same time 980-989), menu tour, Human mission 1, the orders and popup tests, Alt+F4.

### Part 117: scrolling, scenario and display exports (`dispexp.c`, 6 functions, 0x100ae68b-0x100aed6a)
* **Exports (all stdcall):** `IsoSetScrollRate`, `IsoGetScenarioInfo` (the map's size in cells and pixels, its variant, and the numbers of units and objects), `IsoGetDisplayMode`, `IsoGetScreenPos`.
* **`IsoSetScreenRes` and `IsoSetDisplayMode`** set the view's size (the screen less its offsets) and the display mode.
  * `IsoSetDisplayMode` takes nine arguments; the last is unused.
  * After setting the mode, `IsoSetScreenRes` reads the DirectDraw stats (`RtGetDirectDrawStats`, added to the engine imports). In the original, the rest only builds a debug description of the buffers.
* Checked: the trace shows all six run. Unit state log (970 and 980-981; an all-original run at the same time 980-981), menu tour, Human mission 1, the orders test, Alt+F4.

### Part 118: screen points to map cells (`tilepos.c`, 3 functions, 0x100aed6a-0x100af179)
* **The map is isometric:** cells are 64x32 pixels and odd rows are shifted by 32.
* **The three exports** (stdcall) share one formula:
  * `IsoVTilePos` works on a point on the view;
  * `IsoSnapTilePos` works on a cell corner;
  * `IsoTilePos` works on a screen point (the view position added), and gives the cell's centre.
* Each puts the cell's column and row back into its inputs, writes the cell's corner on the screen, and returns the cell's index. The outputs are written in the original's order, which matters when callers pass the same variables for input and output.
* Checked: the trace shows all three run. Unit state log (980-990; an all-original run at the same time took the 970 fork), menu tour, Human mission 1 (one shot showed a hover label a moment earlier; two reruns were normal), the orders, popup, groups and hover tests, Alt+F4.

### Part 119: the main loop (`mainloop.c`, 0x100af179-0x100b0837)
* **The main loop** (stdcall) is the engine's game callback, registered by `plugin_setup`. On each call it:
  * services the engine;
  * sends a script message at an interval (iso+0xd936);
  * advances the game clock in 5 ms steps (at most 200 at a time).
* **While the game runs**, it counts the movement and weapon ticks due (by iso+0xd863/0xd897) and runs the network pump, the movement, weapon and AI loops, and the display and page flips, each at its rate. It also autosaves (`AutoSaveTime`).
* **In a network game** it also counts down the time-outs of players who have stopped answering, and pings and syncs.
* **As in the original:** when time owed (iso+0xd8c3) is less than a step, it is cleared before being subtracted, so the whole step counts.
* **Debug FPS display:** with iso+0xeeb bit 5 set, it is shown once a second (see HIDDEN_AND_UNUSED.md).
* **Engine imports added:** `RtDominionService`, `RtFlipPages`, `SpriteAnimate`.
* Checked: the trace shows it run. Unit state log (980-1001; an all-original run at the same time 980-1002), menu tour, Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 120: unit counts, defeat and objects in the sector table (`playerstate.c`, 5 functions, 0x100b1dcb-0x100b2953)
* **A player's unit count** (+0x1b93) leaves out kinds 0x84-0x87 and transports. 0x100b1dcb adds to it, 0x100b1e2c takes away.
* **`player_check_lost` (0x100b2340):** a player without units (or, with iso+0x158, without any at all) is defeated.
  * In a single player game, the game ends: lost for the local player, won for the others.
  * In a network game, the host shows "has been defeated", frees the player's units and tells the others.
* **Objects in the sector table:** 0x100b2623 finds an object's sector and links it in (0x100b2883, at the front) or out (0x100b2715).
* Checked: the trace shows all five run. Unit state log (980-1738, one run identical to an original to the end; an all-original run at the same time 980-1000), menu tour, Human mission 1, the orders test, Alt+F4.

### Part 121: `IsoAddUnitType` (`unittype.c`, 0x100b2d8c-0x100b31e0)
* **`IsoAddUnitType`** (stdcall, 13 arguments) sets a unit record up as its type:
  * its type record (+0x32, from the types table);
  * its sprite: a copy of the unit sprite at +0, its object at +4, and the type's animation at +8;
  * the network owner (0x100b1a78);
  * the unit list and per-player count;
  * its type's setup handler (0x101545b8, 8 arguments);
  * for buildings, the footprint (0x100f3c43).
* **Re-applying a type:** with its second argument 0, it re-applies the unit's type and keeps its place, look and level.
* **Main plant:** a unit built in the main plant is paired with it (+0x9a) while it comes out.
* **Return value:** 0 on success; on failure 0x20 (no record) or a negative code, with the reason in 0x1017df58, as in the original.
* Checked: the trace shows it run. Unit state log (980-989; an all-original run at the same time 980-1738), menu tour, Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 122: unit events for the AI and sorting by distance (`unitai.c`, 2 functions, 0x100b3760-0x100b39aa)
* **0x100b3760:** for a computer player's unit (+0x1b69 == 2) it returns 1, and passes event 0xb (attacked) on to the AI.
* **0x100b38d4:** a qsort comparison of two unit ids by their distance (+0xc2). Used by the group move, which sorts with the original's `qsort`.
* Checked: the trace shows both run. Unit state log (998-999; an all-original run at the same time 980-981), Human mission 1, the orders test, Alt+F4.

### Part 123: the group move (`groupmove.c`, 0x100b39aa-0x100b4683)
* **The group move** (cdecl, 7 arguments) sends the selection to a point. Both the mouse orders and the state log's scripted orders use it.
* **Placement:**
  * The units keep their places relative to a leader, the first suitable unit, at most 240 pixels each way.
  * They are sorted by distance with the original's `qsort`, so equal distances keep the original's order.
  * Each unit gets its move (0x100b4ba4) with a spread from the original's `rand()`, clamped to the map.
* **Special cases:** units that were following or carrying something drop it first; turrets turn to the point instead (0x1008a000, sent over the network when local).
* **Which units:** only units that can move go: not busy or held, turrets only onto open seen ground, kind 0x22 only where it can stand.
* **As in the original:** the units are read from the local player's selection, while the count and the sorting use the player passed.
* Checked: the trace shows it run. Unit state log with its scripted move orders (970 and 980-1002; an all-original run at the same time took the 970 fork too), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 124: suspending a unit's waypoint path (`waypoints.c`, 0x100b4683-0x100b4ba4)
* **Waypoints** are vectors at iso+0x1984. A unit's path is kept at +0x119 (first waypoint), +0x11b (current) and +0x11d/+0x11f (saved), with +0x10e bit 1 meaning suspended.
* **Suspending (0x100b4683)** saves the path and sends the unit along a one-point path to its current waypoint.
* **Resuming** restores the saved path and moves on to the current waypoint.
* In a network game this only happens for the local player's units, unless forced.
* Checked: the trace shows it run. Unit state log (980-981 and 970; an all-original run at the same time 980-999), Human mission 1, the orders and groups tests, Alt+F4.

### Part 125: a unit's move order (`unitmove.c`, 0x100b4ba4-0x100b54c7)
* **The move order** (cdecl, 10 arguments) makes the target point a new waypoint and links it into the unit's path. `how` decides where:
  * 0 replaces the path;
  * 1 adds the point after the current waypoint, copying its flags;
  * 2 starts a patrol;
  * 3 queues the point at the end of the path.
* **Patrols:** units with a patrol get the order through 0x100b7a66 instead.
* **Starting:** when the path is new and there is no spread, the unit starts along it (0x100b5897).
* **Who may order:** turrets and busy or held units take no orders. In a network game only the local player's units do, unless forced.
* **Arguments:** `how` and the spread are compared as 16-bit values but passed on whole, as the original does.
* Checked: the trace shows it run, with the path insertion, start and patrol branches. Unit state log with scripted moves (980-981 and 970; an all-original run at the same time took the 970 fork too), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 126: new waypoints in a path, dropping a saved path (`waypath.c`, 2 functions, 0x100b54c7-0x100b5897)
* **`path_insert` (0x100b54c7):** builds the waypoints from a point to another waypoint's point (0x1008fc3d) and puts them into the unit's path. `how` 1 puts them after a given waypoint (0x100d51bb); otherwise they go at the path's end (0x100d4e05), and `how` 0 finds that end itself. `how` 3 marks every new waypoint "last". The first new waypoint takes the spread. It returns the first new waypoint, 0 when none was made, or -1 without a map.
* **A walk with no effect:** the original walks to the new stretch's last waypoint and does nothing with it; we keep the walk.
* **`saved_path_drop` (0x100b56aa):** drops the waypoints of a unit's saved path (+0x11f) up to the first one without a count. Kept waypoints (bit 1) go only when `all` is set. Each dropped waypoint gives back its map cell's mark, unless it is flagged bit 9. With nothing left, +0x11d becomes -1.
* Checked: the trace shows both run. Unit state log with scripted moves (970, and 952 with an all-original run at the same time also at 952), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 127: moving a unit on to its next waypoint (`pathstart.c`, 0x100b5897-0x100b6e66)
* **`path_start`** (cdecl, 4 arguments) moves a unit on to its next waypoint and sets it going there. The original's debug strings name the fields: +0x119 `m_firstWayPoint`, +0x11b `m_curWayPoint`, +0x177 `pCrashCount`, a waypoint's +0 `dwTimeStamp`, and iso+0x17dd `GameClock`.
* **Stepping on:**
  * The waypoint reached is dropped and its map cell's mark given back, unless it is kept (flag bit 1), for example on a patrol.
  * When the last one is reached, the path ends.
* **Arrival:** a unit already within 16 x 8 of the waypoint counts as there.
* **Waiting:** a waypoint with a wait (+0xc) holds the unit until the clock reaches it.
* **Going there:** a turn-on-the-spot waypoint (flag bit 7) becomes a one-point vector with the direction to face. Otherwise the path finder runs (0x100b72e6).
* **No way found:**
  * A computer unit (+0x1cd bit 5) gives up at once and tells the AI (0x100daa82).
  * Other units try again 50 ticks later, counting down from 4. Then they skip the waypoint once, or give up the path.
  * A unit on auto recon or guard that gives up shows "No Available Route. Terminating Auto Recon." / "... Guard Mode." to the player.
* **Retry timer:** retries are spaced by `timeGetTime` (at least 50 ms).
* **Debug output:** the debug lines ("Freeing Path %d", "Decrementing Crash Count ...") go to the compiled-out debug print and are left out.
* Checked: the trace shows it run. Unit state log with scripted moves: both runs took the known 847 path, which matches earlier runs up to tick 948. An all-original run at the same time went to 952. Two reruns: 970 and 952, the usual forks. Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 128: turning a unit, and finding its way (`unitface.c`, `unitfind.c`, 0x100b6e66-0x100b7a66)
* **`unit_face` (0x100b6e66):** the facing command. It turns a unit to a direction by putting a one-point vector with the direction in front of its path, then joining a waypoint where the unit stands.
  * Types that cannot turn on their own (+0xb4) turn only when the caller allows it (the command does for everything but vehicles).
  * In a network game, the other players are told (0x100331ab).
* **`unit_find_path` (0x100b72e6):** finds a unit's way to a point (0x100c70bc) and sets it going.
  * It can use a path found ahead of time (+0x115); otherwise that path is dropped.
  * A digging unit's own building footprint is lifted while the way is found.
  * Each waypoint is put in the middle of its map cell. Formation types (+0xb0) are moved to their slot in the cell.
  * With no way (-0x32), a formation type still turns to face the point.
  * Patrolling units hand the path to their patrol (0x100eaa98).
* Checked: the trace shows both run. Unit state log with scripted moves (980-1002; an all-original run at the same time 980-990), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 129: a unit's patrol order (`patrol.c`, 0x100b7a66-0x100b95dd)
* **`patrol_order`** (cdecl, 8 arguments) gives a unit its patrol order.
* **Patrol step:** a unit on patrol (+0x113) that is not set up yet (+0x10e bit 26) takes its next patrol step (0x100e663c), then counts as set up. The step gives where to go, how long it takes, and whether the unit only turns.
  * A step that only turns updates the unit's facing and animation.
  * Otherwise the unit's formation slot moves to the new map cell.
  * The patrol point moves to the step's end. The old point is kept as a path for later (+0x115) or dropped.
* **The waypoint:** it then goes into the unit's path:
  * as a new patrol starting where the unit stands;
  * joined after the current waypoint;
  * or at the path's start.
* **Network game:** the step is sent to the other players (0x10032cd4).
* **Stack leftovers:** the original reads two locals it never sets (see `HIDDEN_AND_UNUSED.md`):
  * the step's direction, which 0x100e663c never fills in;
  * when the patrol was already set up, the step's time.
* **The entry stub:** an assembly stub at the entry reads both from the same stack slots (`esp-0x3c` and `esp-0x58` on entry) before the C code runs, so the values are the ones the original would have used.
* Checked: the trace shows it run. Unit state log with scripted moves (both 980-990, the same as an all-original run at the same time), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 130: a free map cell near a point (`spot.c`, 0x100ba61f-0x100bb1cf)
* **`formation_spot`** (cdecl, 9 arguments) finds the map cell for a unit at or near a point. Move orders, group moves and units making room all use it.
* **Which cell:**
  * Its own cell, when it is free, passable and not taken.
  * Otherwise the first free one of its 8 neighbours, tried from a given direction on.
  * In "rings" mode it tries cells further out (table 0x10158290). Otherwise each direction goes on from the last cell tried.
* **Formation types (+0xb0):** a cell has four slots (bits 4-7 | 8-11). A formation type takes one; its slot moves to a free one when its own is in use.
* **The list of taken cells:** the cells taken in one order are listed in the original's globals (0x1017dfc0-0x1017e3f0), and `how` 2 frees them all again. `how` 3 also makes the cell the unit's own (+0x5a).
* Checked: the trace shows it run, tested together with part 131. Unit state log with scripted moves (both 980-990, the same as an all-original run at the same time), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 131: carried units and claimed destinations (`carry.c`, 3 functions, 0x100bdbeb-0x100beeea)
* **`carried_follow` (0x100bdbeb):** moves the units a carrier holds (the chain +0xf6) to its point. It counts the room each takes (type +0xe4) against what the carrier holds (+0xf3). A carried unit that no longer exists ends the chain.
* **Claimed destinations:** units claim the map cell they are heading for. These are 11-byte records at iso+0x18e8, in a doubly linked list (0x1066/0x1068, free ones from 0x1acb), and a unit's own record is +0x15d.
* **`dest_claim` (0x100be084):** claims a cell, or only tells whether it is free.
  * Only the ground units' type handler (0x1000a86b) claims cells.
  * Another unit's cell counts as free only for a formation type in another slot.
* **`dest_release` (0x100be4d2):** gives the claim up. On request, it also sends this side's idle units that claimed the same cell to a free cell nearby (0x100ba61f, then the move order).
* **An unused value:** the original also works out each moved unit's speed there and never uses it.
* Checked: the trace shows all three run, tested together with part 130. Unit state log with scripted moves (both 980-990, the same as an all-original run at the same time), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 132: the AI's squad list (`squadlist.c`, 7 functions, 0x100c0350-0x100c07c0)
* **The records:** squads are 0x130-byte records at G(0x1018b608), linked by +4 (next) and +8 (previous). The first free one is at +8 of the AI block G(0x1018b604).
* **Six accessors:** get and set the free list's head and a squad's links. 0x100c03f7 is not listed as live but sits in the same run.
* **`squad_new` (0x100c0462):** makes a squad of a given kind for a computer player, with a task and up to 16 unit types. It takes the squad from the free list (0x100c0d5a), counts it (+0x72 of the player's block) and hands it to the player (0x100e487c).
  * The debug line "CREATING GROUP: player %i group# %i, record %i, type %i, task %s" is compiled out, so the task name it would need is not looked up.
* Checked: the trace shows all seven run. Unit state log with scripted moves (951, a known fork, and 980-990, the same as an all-original run at the same time), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 133: a computer player's new targets and squads (`squadnote.c`, 2 functions, 0x100c07c0-0x100c0eab)
* **`target_note_new` (0x100c07c0):** a computer player takes note of a new unit of its own.
  * **A waiting task:** unless told not to (or the player's +0x106 is set), a task waiting for that kind of unit takes it, by type: soldiers +0x13b, vehicles +0x133, structures +0x137 or +0x12f.
  * **Otherwise a squad:**
    * structures go to the base squad, which is made when there is none;
    * the other units go to the squad for their type (+0x121, +0x125, +0x11d), else to a new one;
    * before the first tick, they go to the first one (+0x119).
  * **Kinds 0x10-0x13** are then taken off the map (0x10001f74, 0x1007d323).
  * The debug lines are compiled out.
* **`squad_link` (0x100c0d5a):** takes a squad off the free list and appends it to the player's list of squads of its kind.
  * When that list is empty, the original resets the links of the free list's first squad (the same squad, as `squad_new` calls it).
* Checked: the trace shows both run. Unit state log with scripted moves (970, and 980-990, the same as an all-original run at the same time), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 134: a squad's units and totals (`squadtarget.c`, 4 functions, 0x100c0eab-0x100c16c7)
* **The totals:** a squad keeps totals of the units it holds:
  * +0x34, the count;
  * +0x38, the count per kind;
  * +0x118, their health;
  * +0x114, their value (0x1004d14f; the player's own total is kept too, +0x76);
  * +0x11c and +0x120, two type values (0x1009da08, and 0x1009dbc2 by the player's difficulty).
* **`squad_target_changed` (0x100c0eab):** adds a unit to the totals or takes it off. The kind is used whole as an index, as in the original.
* **`squad_health_total` (0x100c11d6):** adds up the health anew.
  * Squads of kinds 2-3 walk their own units' list; kinds 4-5 walk the enemy list.
  * For any other kind the original never steps on and would loop forever. It is only called for kinds 2-5.
* **`squad_add_unit` (0x100c1344) and `squad_remove_target` (0x100c154a):** link a unit into the squad's list (+0x24/+0x28; the unit's squad at +0x110, for kind 3 also +0x114) or take it out, and update the totals.
* Checked: the first test of this part ran the previous build, because the name `squad_add_target` was already taken (`aisquads.c`, 0x1004c9b9) and the link failed. After the rename it was tested together with part 135: unit state log with scripted moves (both 980-990, the same as an all-original run at the same time), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 135: the AI's defence squads and detached squads (`squaddefend.c`, 3 functions)
* **`squad_setup_defense` (0x100c25d2):** a squad's guarding units (+0x196 0x1e) guard the squad's target (0x1009f71f), or stop when it has none. The squad's state becomes 6 (with a target) or 7.
* **The two reviews:** both walk the player's list 2 for squads without a task record:
  * `ai_player_review` (0x100c2d66) handles the detached attack squads ("Handle Detatched Attack Groups");
  * `ai_player_plan` (0x100c2f86) handles the detached defence squads ("Handle Detatched Defense Groups").
* **What they do with a squad:**
  * An empty squad is let go (0x100c197b).
  * A squad without an enemy squad looks for a target near its first unit (attack) or sets up its defence.
  * A squad with an enemy squad but no target picks one of its units (0x1004db61) and attacks it.
  * Otherwise it keeps attacking its target.
* **Timing log:** the time each takes goes to the AI's timing log, which is compiled out.
* Checked: the trace shows all three run, tested together with part 134. Unit state log with scripted moves (both 980-990, the same as an all-original run at the same time), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 136: a vehicle's tick (`unitthink.c`, 0x100c3240-0x100c561e)
* **`unit_think`** (9 KB) is the type handler of unit types 0x22, 0x28-0x2f and 0x38-0x3f (vehicles), called each tick through the type table at 0x101545bc. It has the same shape as the ground units' handler (0x1000a86b, `action.c`).
* **The order of a tick:**
  * deleting a unit marked for it;
  * reaching the end of the path (handler +0x3c), boarding or entering;
  * starting the next waypoint (0x100b5897);
  * waiting (+0x149);
  * turning;
  * choosing the next action;
  * then one step of the action.
* **Choosing the next action:** a 29-case switch on the action, decoded from its jump table: standing, the turns (tables 0x1015b5a0/5e0/620), patrol steps, dying (0x26/0x27), deploying and packing up (0x40-0x48, 0x51/0x52), and others.
* **One step:** the step moves by the animation's offsets (tables 0x1015b568-0x1015b598, or 0x101589d8 for turning on the spot). The vector is kept in the original's globals 0x10118b20/24. A turret (+0xb4) turns towards its target.
* **Debug output:** the debug lines (0xeea bit 0) go to the compiled-out debug print. The two that use the engine's `DebugStr` are kept.
* Checked: the trace shows it run. Unit state log with scripted moves (952, a known fork, and 980-990, the same as an all-original run at the same time), Human mission 1, the orders, popup, groups and hover tests (one vehicle), Alt+F4.

### Part 137: the CD check stub and the path finder's buffers (`pathinit.c`, 2 functions, 0x100c6d20-0x100c701f)
* **`cd_check` (0x100c6d20):** always passes (returns 1).
* **`pathfinder_init` (0x100c6d50):** allocates the path finder's buffers for a map, through the engine's `winmalloc`, and reports each on the debug output ("Allocated [%ld] bytes for Pathfinder aGrid data." and so on). The buffers are:
  * the grid (2 bytes a cell);
  * the open list (0x4e20 bytes) and its costs (0xfa0);
  * the vectors (0x8ca0).
* **A small leak:** only the grid and the vectors are freed before a new map. The other two leak each time a map is loaded again, as in the original.
* Checked: the trace shows both run. Unit state log with scripted moves (980-990, the same as an all-original run at the same time), Human mission 1, the orders test, the menu tour, Alt+F4.

### Part 138: the path finder (`pathfind.c`, 0x100c70bc-0x100c8773)
* **`path_find`** (cdecl, 8 arguments) finds a way between two points. All movement uses it.
* **The search:** it searches the map cells in a rotated grid, from the goal outwards, until it reaches the start.
  * Each step goes to one of the 8 neighbours; 0x100c9a00 steps round them.
  * Each step adds 1 (table 0x10158ce8) plus the cell's occupancy (0x100672b3) as the estimate's tie-breaker.
  * The open list is searched linearly for the lowest cost and holds at most 2000 cells.
* **Building the way:** it walks back down the costs from the start to the goal, notes each step's direction, and makes a waypoint vector at every change of direction.
* **The working data:** the original's globals, kept as they are: the grid 0x1017e414, the open list 0x1017e3f8 and its costs 0x1017e3fc, the step list 0x1017e400, the directions 0x1017e418, and the current cell 0x1017e408/40c.
* **Return value:** the number of waypoints, or an error code, also kept at iso+0x1b59:
  * -10, no buffers;
  * -0x32, both points on the same cell;
  * -0x1e, the goal is blocked or there is no way;
  * -0x14, the search or the way is too long, or no vector is free;
  * -0x28, no head pointer;
  * -0x3c, a cell out of range.
* **The debug lines:** the messages to the debug output ("++++ Maximum Path Reached #1 - Unit [%d] ++++" and so on) are kept, through `OutputDebugStringA` as in the original.
* **Without a map (+0xe113 == 0):** the original then also measures how far the unit gets along the way on its range (+0x1d3, a float, rounded through memory as in the original), and finds the first corner it cannot cut, into +0x370.
* **The goal pair:** 0x100c883f is handed the start and goal cells as {column, row} pairs, so they are kept as arrays.
* Checked: the trace shows it run. Unit state log with scripted moves (1002, and 980-990, the same as an all-original run at the same time), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 139: the path finder's helpers (`pfhelp.c`, 4 functions, 0x100c9a00-0x100c9cf0)
* **`pf_step` (0x100c9a00):** steps round the current cell's 8 neighbours. Tables 0x10158d08 hold the step and 0x10158d48 the next direction.
* **`pf_cost` (0x100c9a50):** the estimate, |dx| + |dy| in the rotated grid.
* **`pf_rotate` (0x100c9aa0):** a cell's column in the rotated grid.
* **`pf_blocked` (0x100c9ae0):** whether a cell is closed to a unit. A cell is closed when it is:
  * off the map;
  * blocked (bit 0, though hovercraft 0x30-0x37 may cross it; bit 1);
  * taken by another object, unless that is a wall piece (types 0x88-0x9c).
  * Cells with bit 12 are always open, and cells with bit 14 are open to all but computer units (+0x1cd bit 5).
* Checked: the trace shows all four run. Unit state log with scripted moves: 980-990, the same as an all-original run at the same time. A first run split at 949, a tick no earlier run had split at; two reruns gave 980-990. Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 140: the linked list library and the AI process exports (`dlist.c`, 19 functions, 0x100c9cf0-0x100ca41e)
* **The list library:** a doubly linked list used by the terrain analysis and the AI. A list is {count, first, last}; a node is {next, prev, free function} followed by its data, and the functions hand out pointers to the data (node + 0xc). Memory comes from `GlobalAlloc(GPTR)`.
* **The functions:** add (at the front, at the back, or after a node), link, unlink, remove, clear, visit, contains, first, last, next, prev, move, count, new and free.
* **Callbacks:** the original calls the free and visit functions without cleaning the stack after them, so they may be cdecl or stdcall. Ours calls them through a small assembly frame that restores the stack either way.
* **Left original:** two empty functions (0x100ca299, 0x100ca2a9).
* **The AI process exports:**
  * `IsoInitAIProcess` (the AI runs every 30 ticks, start time from `timeGetTime`);
  * `IsoStopAIProcess`;
  * `IsoSetAIDelay`;
  * `IsoAILoop` (one AI turn, 0x100e3077, unless the game is paused or minimised).
* Checked: the trace shows all but each, contains, last, move, prev and count run. Unit state log with scripted moves (both 980-990, the same as an all-original run at the same time), Human mission 1, the orders test, the menu tour, the popup, groups and hover tests, Alt+F4.

### Part 141: the cell a unit stands on, and running over soldiers (`unitcell.c`, 2 functions, 0x100ca4c6-0x100caa96)
* **`unit_cell_update` (0x100ca4c6):** notes the map cell a unit stands on (+0x56; the cell's bit 2) and its terrain byte (+0x2f, from 0x1904).
  * `how` 2 takes the unit off the map: it gives up its cell and formation slots and, in the editor, its claimed destination.
  * With the debug flag (0xeea bit 0) the cells are drawn in a debug map. On leaving the map, the original restores the debug map at the cell it has just cleared to 0.
* **`unit_run_over` (0x100ca80b):** a unit runs over the enemy soldiers on its cell. They must be of players it is not allied with and not lying down. They are killed and scored, and a unit that builds up counts up. In a network game this happens only on the host.
* Checked: the trace shows both run. Unit state log with scripted moves (980-990, the same as an all-original run at the same time; the first run split at 949, see Open items), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 142: the cell a vehicle stands on (`unitcell.c`, 0x100caa96-0x100caf39)
* **`unit_cell_update2`** does the same as `unit_cell_update` for the other units (vehicles).
* **Taking the cell:** a type with +0x74 == 3 (except while packing up, action 0x45) also takes its cell (bit 3) and runs over the soldiers on it (0x100ca80b).
* **The row:** +0x3e bit 19 notes an odd row.
* **Off the map:** in a network game, a unit off the map (except hovercraft) is deleted on the host, with a message box ("Unit off map.  Deleting!" / "Proximity Error").
* Checked: the trace shows it run, tested together with part 143. Unit state log with scripted moves (1001-1715 and 980-990, the same as an all-original run at the same time), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 143: a unit setting off, and its patrol legs (`patrolstep.c`, 2 functions, 0x100cb2b2-0x100cb9d8)
* **`unit_moved` (0x100cb2b2):** a unit has set off. A unit on patrol starts its patrol state over (+0x1cd, +0x360/362) and plans its legs.
  * The original indexes the unit table with the whole argument, not its low 16 bits.
* **`patrol_reset` (0x100cb37a):** plans the straight legs (up to 15 at +0x1e9, 25 bytes each) of the unit's patrol along its waypoints.
  * The plan reaches as far as the unit's range (0x100cb9d8, kept at +0x1d3) plus 80.
  * Each waypoint further than 0 away becomes a leg. The last leg ends part of the way to its waypoint, along the leg's unit vector (0x100cba36).
  * A leg ending off the map stops the plan.
  * With no leg, a unit on patrol gets one straight to its first waypoint.
* **Float precision:** all float values are kept as floats in memory between the steps (`volatile`). The comparisons are written so that an unordered (NaN) result goes the way the x87 compare goes.
* Checked: the trace shows both run, tested together with part 142. Unit state log with scripted moves (1001-1715 and 980-990, the same as an all-original run at the same time), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 144: the patrol legs' range, unit vectors and direction (`patrollegs.c`, 3 functions, 0x100cb9d8-0x100cbcea)
* **`patrol_range` (0x100cb9d8):** how far a unit patrols. It is the length of the vector 0x100e5a4d gives for its patrol (+0x1cb, passed with the record pointer's upper half above it), through the float square root 0x1007e780.
* **`leg_vector` (0x100cba36):** the unit vector of a direction: east (1, 0), then in eighths of a turn on the isometric map, the diagonals (0.894, 0.447). The values come from a jump table.
* **`leg_direction` (0x100cbb54):** the direction (1-7, else 0) from one point to another, by the angle against (1, 0) in the sectors of table 0x10113280.
  * The first test uses the angle as the x87 holds it; the others use it rounded to a float, as in the original.
* Checked: the trace shows all three run. Unit state log with scripted moves (both 980-990, the same as an all-original run at the same time), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 145: finishing a patrol plan (`legbox.c`, 2 functions, 0x100cbcea-0x100cc145)
* **`legs_start` (0x100cbcea):** trims a unit's patrol legs at both ends:
  * the first leg starts a quarter step back;
  * the last ends 5 further on (3 for soldiers).
* **Then:** it works out the lengths and the starts along the way again, and keeps the box around the legs (half its size at +0x1d7/+0x1db, its middle at +0x1df/+0x1e3).
* **Float precision:** all floats go through memory as floats between the steps, as in the original.
* **`leg_step` (0x100cc0b9):** how far one step goes in a direction: 64 east and west, 32 north and south, 36 on the diagonals.
* **Left original:** 0x100cc145 (a wait for a key) and 0x100cc17e (a busy wait for some milliseconds) are not reached.
* Checked: the trace shows both run. Unit state log with scripted moves (both 980-990, the same as an all-original run at the same time), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 146: the patrolling units' tick (`patroltick.c`, 0x100cc1be-0x100cc55c)
* **`units_patrol_tick`** runs every tick. It draws the sector debug view and calls 0x100d1afa. Every 3rd tick it also handles the patrols, after 0x100d601d and 0x100ce8a6.
* **Soldiers** (+0x28 == 1) plan again (0x100cc9ca) when their patrol is blocked (0x100ce277, unless +0x1cd bit 9) or 0x100d0e81 says so.
* **Vehicles** (+0x28 == 3, not hovercraft) plan again the same way, with 0x100cc55c.
  * A vehicle whose patrol is fine checks the vehicles after it in the list for crossing patrols (0x100ccb6d).
* **Which units take part:** only units on patrol (+0x1cd bit 0) that are not entering or leaving anything.
* **Its old name:** `movement.c` called it `ai_tick`.
* Checked: the trace shows it run. Unit state log with scripted moves (952, a known fork, and 1001-1002), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 147: whether a vehicle's patrol still ends somewhere it can go (`patrolend.c`, 2 functions, 0x100cc55c-0x100cc9ca)
* **`vehicle_patrol_check` (0x100cc55c):** whether a vehicle has to plan its patrol again.
  * Without legs (or with more than 15) it has to.
  * Otherwise it asks 0x100cc696 about the last leg's end, and if that says 0, tries one step back.
  * The floats become ints by truncation (the original's `_ftol`).
* **`patrol_end_free` (0x100cc696):** whether the cell at a point blocks the end of the patrol.
  * It never blocks when it is the vehicle's own cell or a cell with bits 12-14.
  * A cell with an impassable object (0x100d27d8) blocks unless the next cell in the leg's direction is open. A patrolling unit standing on that cell also sets the vehicle's +0x1cd bit 8.
  * A passable object blocks on a taken cell (bit 3).
  * A blocked cell without an object (bit 0, unless +0x10e bit 9) blocks unless the next cell is open.
* Checked: the trace shows both run. Unit state log with scripted moves (1001-1002, and 980-990, the same as an all-original run at the same time), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 148: walls that cut across patrols (`wallcross.c`, 3 functions, 0x100cdc9f-0x100ce277)
* **`walls_check_patrols` (0x100cdc9f):** checks every wall segment between two existing posts. The segments are 0x86-byte records at iso+0xe123.
* **`wall_cuts_patrols` (0x100cddcd):** asks whether the wall between two posts cuts across a patrol.
  * It looks at the units of allied players near either post.
  * For each, it checks the next 128 of the patrol (86 for soldiers) with the segment test 0x10090030.
  * Such a unit is marked (+0x1cd bit 11) and 1 is returned. The original also calls 0x1007abf1 and ignores what it returns.
  * Otherwise the two posts are connected (0x1007aa2b).
* **`sectors_near` (0x100ce197):** whether a unit's sector is another's or one of its 8 neighbours.
* **Arguments and floats:** the posts' +0x1cb are passed with the record pointer's upper half above them, as in the original. All floats go through memory as floats between the steps.
* Checked: the trace shows all three run. Unit state log with scripted moves (951, a known fork, and 980-990, the same as an all-original run at the same time), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 149: enemy wall posts on a patrol way (`patrolblock.c`, 3 functions, 0x100ce277-0x100ce796)
* **`patrol_blocked` (0x100ce277):** walks a unit's patrol as far as 3 times its range plus 80, cell by cell (0x100ce796, steps from table 0x101590c8). It looks for a cell marked bit 14 (not its own) that holds a wall post (0x100d26a4) of a player it is not allied with.
  * On finding one it calls 0x100ce61d (not reached in the tests, left original) and returns 1.
  * Units marked +0x1cd bit 5, units that ignore marks, and units without legs are skipped.
* **Packed points:** points are passed as two shorts packed in a dword, as in the original.
* **A harmless leftover:** the post's id goes on with two stack-leftover bytes above it. No callee reads them.
* **`unit_set_hold` (0x100ce71f):** sets or clears +0x1cd bit 12.
* **`cell_of` (0x100ce756):** the map cell at a point.
* Checked: the trace shows all three run (`unit_set_hold` through 0x100ce8a6). Unit state log with scripted moves (980-990, the same as an all-original run at the same time), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 150: how many cells a straight leg crosses (`patrolcells.c`, 0x100ce796-0x100ce8a6)
* **`cells_between`** counts the cells between two points (two shorts packed in a dword each) along a direction. It measures:
  * east and west, the x distance over 64;
  * on the diagonals, the x distance over 32;
  * north and south, the y distance over 32.
* **Rounding:** each count is rounded by 0x1008cc80 from the quotient rounded to a float, as in the original.
* Checked: the trace shows it run, tested together with part 151. Unit state log with scripted moves (both 980-990; an all-original run at the same time took the 970 fork), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 151: the patrolling vehicles' cells (`patrolveh.c`, 0x100ce8a6-0x100cef5d)
* **`vehicles_patrol_update`** runs every 3rd tick for each vehicle (not hovercraft, not inside anything). A vehicle on patrol holds a map cell (+0x368, marked with its id and bit 3), and remembers where it stood the time before (+0x36c/+0x36e).
* **While its patrol runs (0x100cef5d):** it plans its legs again and moves its held cell along (0x100cf5a1). Near its stop it also takes the cell it stands on.
* **Otherwise:**
  * it stops patrolling (0x100cf9df);
  * at its goal it either backs out of a cell another object holds (0x100caf40) or stops and gives up its legs;
  * it then gives up its held cell and holds its own.
* **Standing still:** +0x1cd bit 1 notes that the vehicle stood still at its stop.
* **An unused value:** the original also works out the cell it stands on and does not use it.
* Checked: the trace shows it run, tested together with part 150. Unit state log with scripted moves (both 980-990; an all-original run at the same time took the 970 fork), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 152: whether a vehicle's patrol is still under way (`patrolactive.c`, 0x100cef5d-0x100cf1d3)
* **`patrol_active`** tells whether a unit's patrol is under way. It is not when the unit:
  * stands at its stop (with +0x1cd bit 3), or else at its goal;
  * has stood still for more than 150 (counted at +0x364 in steps of 3; it then stops there and sets bit 3);
  * has +0x1cd bit 1 set;
  * stands on the patrol's only waypoint.
* **The stop:** whenever it stops, the unit notes where it stands (+0x360/+0x362).
* Checked: the trace shows it run. Unit state log with scripted moves (both 980-990, the same as an all-original run at the same time), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 153: a patrolling vehicle's held cell and stopping a patrol (`patrolcell.c`, 2 functions, 0x100cf5a1-0x100cfb0d)
* **`patrol_cell_taken` (0x100cf5a1):** checks the cell a vehicle holds. The vehicle is hit by the wall, once, when the cell:
  * is marked bit 14;
  * holds a wall post of a player it is not allied with;
  * has a live post next to it along the wall (0x10077b2b);
  * and the wall lies ahead (0x100cf7cf, not reached in the tests).
* **Being hit:** the hit is 0x1000d4c2 with 50, plus a sound, once (+0x1cd bit 10). Off such cells the marks (+0x1cd bits 9, 10) are cleared.
* **`patrol_stop` (0x100cf9df):** a vehicle stops patrolling. When it is boxed in on a blocked cell (its cell and all 8 neighbours blocked), it is marked (+0x3e bit 23) and gets action 0x14.
* **Harmless leftovers:** the post and direction words go on with stack-leftover bytes above them. Their callees read only the low word.
* Checked: the trace shows both run. Unit state log with scripted moves (both 980-990, the same as an all-original run at the same time), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 154: the patrolling soldiers (`patrolsoldier.c`, 0x100cfb0d-0x100cff4a)
* **`soldiers_patrol_update`** runs every 3rd tick for each soldier:
  * while its patrol runs it plans its legs again and moves its held cell along;
  * otherwise it stops at its goal, and +0x1cd bit 1 notes that it stood still at its stop.
* **Stuck soldiers:** up to 30 soldiers that stood still and are stuck (0x100ccb2b) are listed at 0x10180358. They are sorted with the original's `qsort` and comparator 0x100cb254. The first whose way is not clear (0x100d0270) is helped out (0x100cff4a).
* **The sort key:** it sits next to each entry in that list and is never written here. Earlier writes to the same global decide the order, the same in ours as in the original, since we use the same global.
* Checked: the trace shows it run. Unit state log with scripted moves (both 980-990, the same as an all-original run at the same time), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 155: a computer unit's patrol fields, and patrols that meet (`soldierway.c`, 2 functions, 0x100d01d7-0x100d0622)
* **`ai_patrol_clear` (0x100d01d7):** clears a unit's computer patrol (+0x1cd bit 5, +0x382-+0x388).
* **`patrols_collide` (0x100d0270):** whether a unit's patrol meets that of another patrolling unit near it (not a soldier) whose patrol box (+0x1d7-+0x1e3, grown by 8) overlaps its own.
  * It does at once when that unit stands at its stop (+0x1cd bit 3).
  * Otherwise each pair of their legs is tested (0x100d0622).
* **Floats:** the floats go through memory as floats between the steps, and the comparisons follow the x87 compares.
* Checked: the trace shows both run. Unit state log with scripted moves: both runs took the known 970 fork and match an all-original run on that fork up to ticks 1000-1002. Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 156: whether a soldier's patrol ends somewhere it can stand (`soldiercheck.c`, 3 functions, 0x100d0e81-0x100d1afa)
* **`soldier_patrol_check` (0x100d0e81):** whether a soldier has to plan its patrol again because its last leg ends on a cell it cannot stand on. Such a cell is:
  * one taken by an impassable unit that is not of kind 3 (a patrolling unit there also sets +0x1cd bit 8);
  * one with a passable object on a taken cell (bit 3);
  * or a blocked cell boxed in ahead and on both sides.
* **A second look:** such an end may still do when the cell next to it for the soldier's formation slot (+0x10e bits 10-11) is open. That cell comes from the tables at 0x1017c538-0x1017c570, or lies 32 / 16 further along.
* **Truncation:** floats become ints by truncation, as with the original's `_ftol`.
* **`unit_kind_of` (0x100d1aaa) and `unit_kind` (0x100d1adf):** a unit's kind (+0x28).
* Checked: the trace shows all three run. Unit state log with scripted moves (both 980-990, the same as an all-original run at the same time), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 157: the debug view of the patrol legs (`patroldraw.c`, 2 functions, 0x100d1afa-0x100d1dbc)
* **`patrols_draw` (0x100d1afa):** runs each tick and calls `patrol_draw` for every soldier and vehicle not inside anything.
* **`patrol_draw` (0x100d1bec):** with debug flag 0xeeb bit 2 set, draws a unit's legs as a GDI line on the debug DC (iso+0xd7b3): white while it patrols, black otherwise. The points go through the original's global 0x10180448.
* **Not tested:** the tests run both functions, but none sets the debug flag, so the drawing itself has not run.
* Checked: the trace shows both run. Unit state log with scripted moves (both 980-990, the same as an all-original run at the same time), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 158: the units on the line of a planned wall (`wallunits.c`, 2 functions, 0x100d1e0f-0x100d2319)
* **The test:** each unit counts as a short segment across the wall, half a step each way at right angles to it, and is tested against the line between the two posts (0x10090030).
* **`posts_too_close` (0x100d1e0f):** whether a unit allied with the first post's player, near either post, stands on that line.
* **`units_between` (0x100d2083):** lists (at most 20) the soldiers and vehicles not allied with that player, near either post, that stand on the line.
* **Floats:** the floats go through memory as floats, as in the original.
* Checked: the trace shows both run. Unit state log with scripted moves (both 980-990, the same as an all-original run at the same time), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 159: whether a unit with a new path has to wait (`unitblocked.c`, 1 function, 0x100d2319-0x100d2552)
* **`unit_blocked` (0x100d2319):** plans a unit's legs along its new path; returns 0 when the unit is already at the path's only point.
* **Held back:** a soldier when a wall post stands in its way (unless +0x1cd bit 9) or its way ends where it cannot stand; a vehicle when its patrol meets another's or a post stands in its way.
* **A held unit** stops at its place (+0x1cd bit 3, +0x360/+0x362) and waits (0x100e97ca); the function returns 1.
* Checked: the trace shows it run. Unit state log with scripted moves (one run 980-990, the same as an all-original run at the same time; the other took the 949 split, see Open items), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 160: two small unit tests (`unitrel.c`, 2 functions, 0x100d25a3 and 0x100d26fb)
* **`players_allied` (0x100d25a3):** whether two players are the same or, in a network game only, allied (the first player's alliance bits, +0x1ba0 of the player block, 0x1461 bytes each).
* **`ignores_marks` (0x100d26fb):** whether a unit is a hovercraft: its class (+0x10, the type number times 4) is 0x30-0x37, the Hover Bomber or Hover APC.
* **Left original, not reached:** 0x100d2624, 0x100d2664, 0x100d26a4, 0x100d273b, 0x100d2792 and 0x100d27d8.
* Checked: the trace shows both run. Unit state log with scripted moves (both 980-990, the same as an all-original run at the same time, and equal to it up to ticks 1052-1054), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 161: the map margin test and loading the popup menu's graphics (`popupinit.c`, 2 functions, 0x100d2a70 and 0x100d2b10)
* **`on_map_px` (0x100d2a70):** whether a pixel lies inside the map's half sizes (+0xd709 / +0xd71e), less a margin of 32 / 16.
* **`IsoInitPopupMenu` (0x100d2b10):** loads the menu's resource and takes the first word of seven more resources (+0xdad0-+0xdae8).
* **Left original, not reached:** `IsoDeInitPopupMenu` (0x100d2ca0).
* Checked: the trace shows both run. Unit state log with scripted moves (both 980-990; one run equal to an all-original run at the same time for all 1,748 ticks), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 162: the popup menu's settings (`popupconf.c`, 1 function, 0x100d2d70)
* **`popup_read_settings` (0x100d2d70):** reads three values from the `[Dominion]` section of `dominion.ini`: `PopupEnabled` (default 1), `PopupShowDisabling` (default 1) and `PopupClickLength` (default 400, kept within 200-800).
* Checked: the trace shows it run. Unit state log with scripted moves (runs at 980-1000, known forks, with an all-original run at 980-990 at the same time), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 163: building and showing the popup menu (`popupshow.c`, 1 function, 0x100d2e41-0x100d3c1e)
* **`IsoShowPopupMenu` (0x100d2e41):** builds the menu into its DIB from three 64-bit item masks (shown, usable, ticked):
  * the title (the unit's name, "Multiple Units", "SC Plant", "General Actions" or "Command Points");
  * items 0-0x1f, with the prices on items 0x10 ("Sell") and 0x12 ("Upgrade");
  * after a divider, items 0x20-0x2f ("%d to %s", one per player);
  * after a divider, the tick-box items 0x30-0x3f;
  * for a single unit, its health, coloured by how much is left.
* **Placement:** the menu moves left or up when it would leave the screen, and its item rectangles move with it.
* **Not tested:** the tests open the menu on the ground, a unit and a selected unit. The several-units title, the Sell and Upgrade prices and the per-player items have not run.
* Checked: the popup test (both menus drawn correctly, differences from the original runs no larger than between those runs). Unit state log with scripted moves (both 980-990, the same as an all-original run at the same time), Human mission 1, the orders, groups and hover tests, Alt+F4.

### Part 164: the pointer over the popup menu, hiding it, and its items (`popupitem.c`, 3 functions, 0x100d3c1e-0x100d4450)
* **`IsoBrowsePopupMenu` (0x100d3c1e):** moves the highlight to the item under the pointer. With a click it returns that item, or -2 when the item is not usable or there is none; without a click, -1. With +0xd79f set, a pointer over no item also clears the highlight.
* **`IsoHidePopupMenu` (0x100d4263):** hides the menu.
* **`popup_item_draw` (0x100d42ea):** draws one item's text, grey when not usable, highlighted when asked, indented by the local player's side.
* Checked: a one-off run that hovers over items of the ground and unit menus and clicks a tick-box item, on ours and twice on the original: all five new screenshots pixel-identical to one original run. Unit state log with scripted moves (both 980-990, the same as an all-original run at the same time), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 165: the linked lists of path nodes (`pathlink.c`, 3 functions, 0x100d4450, 0x100d4947, 0x100d4bfa)
* **`vector_link` (0x100d4450):** sets a node's next node (+0xe of the 18-byte nodes at iso+0x1984). Its third argument has no effect.
* **`path_stop` (0x100d4947):** frees a unit's path after a given node, or from its start (+0x113), up to a node with +8 <= 0. While +0xe113 is set, each node's cell gives up a mark, unless the node's +0xa bit 9 is set.
* **`unit_path_join` (0x100d4bfa):** joins a list of nodes into a unit's second path (+0x119), before or after a given node, or at the end.
* **Left original, not reached:** 0x100d44ba, 0x100d45f7, 0x100d46fd, 0x100d4af8 and 0x100d4b87.
* Checked: the trace shows all three run. Unit state log with scripted moves (runs at 980-1002, known forks, with an all-original run at 980-990 at the same time), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 166: adding a list of path nodes to a unit's path (`pathappend.c`, 1 function, 0x100d4e05)
* **`path_append` (0x100d4e05):** appends a node list to the end of a unit's second path (+0x119). The list's last node takes over what the path's last node led to.
* **Left original, not reached:** 0x100d4f51 and 0x100d51bb.
* Checked: the trace shows it run. Unit state log with scripted moves (both 980-990, the same as an all-original run at the same time), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 167: freeing the nodes of a unit's second path (`pathrelease.c`, 1 function, 0x100d559a-0x100d59e4)
* **`path_release` (0x100d559a):** frees a unit's second path (+0x119) after a given node, or all of it, up to a node with +8 <= 0.
  * Nodes with +0xa bit 1 stay and are relinked, unless all are to go.
  * A node that links to itself is cut off.
  * The path's cursor (+0x11b) moves on to the next kept node.
  * While +0xe113 is set, a freed node's cell gives up a mark, unless the node's +0xa bit 9 is set.
* **Left original, not reached:** 0x100d59e4-0x100d5c0c.
* Checked: the trace shows it run. Unit state log with scripted moves (both 980-990, the same as an all-original run at the same time), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 168: the debug view's crosses on the map (`debugmarks.c`, 3 functions, 0x100d5e5c, 0x100d6003, 0x100d601d)
* **`debug_mark` (0x100d5e5c):** adds a cross at a point unless one is already there (at most 128, 12 bytes each at 0x101806e8); returns their number.
* **`marks_clear` (0x100d6003):** removes all crosses.
* **`marks_draw` (0x100d601d):** with debug flag 0xeeb bit 2 set, draws the crosses on the debug DC, black or white.
* **Not tested:** the tests run all three, but none sets the debug flag, so the drawing itself has not run.
* **Left original, not reached:** 0x100d5c0c, 0x100d5f2e and 0x100d6250.
* Checked: the trace shows all three run. Unit state log with scripted moves (both 980-990, the same as an all-original run at the same time), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.

### Part 169: one step of a unit's action state machine (`unitstate.c`, 1 function, 0x100d62a0-0x100d7b3d)
* **`unit_ai_state` (0x100d62a0):** runs the state in +0x197 (copied to +0x196). A delay at +0x199 holds the unit until that tick. Leaving the idle state 0x1e calls the handler of the unit's type (table 0x10118d90).
* **The states** include moving (0xa, 0xf, 0x17), attacking (0xb, 0xc), aiming (0x14), firing (0x18), idle (0x1e), guarding and following (0x1f), boarding (0x32-0x34) and the repair-type orders (0x41-0x43). An unknown state logs "Undefined AI State" and goes idle.
* **Word arguments:** where the original pushes a 16-bit field with the high half of the unit's address and the callee passes it on, ours does the same.
* **Not tested:** the tests are single-player, so the network branches have not run, and not every state is reached.
* Checked: the trace shows it run. Unit state log with scripted moves (both 980-990, the same fork range as an all-original run at the same time), Human mission 1, the orders, popup, groups and hover tests, Alt+F4.
