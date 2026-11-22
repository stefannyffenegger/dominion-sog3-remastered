# Hidden, unused and unfinished things in Dominion: Storm Over Gift 3

Found while rewriting the game's modules (see `PHASE1B_MODULES.md`). Each entry says how sure we are:
* **code**: seen in the code, not tried in the game;
* **traced**: measured in the running game (`DOMINION_TRACE`, logging stand-ins);
* **tested**: tried in the game.

Addresses refer to the original files (`Rtlib32.dll`, `ISO.R32`, `NETPLAY.R32`, ...) of the English v1.1 release.

## How much of the engine the game uses
**traced.** The engine (`Rtlib32.dll`) has about 1,450 functions of its own. During the menus and Human mission 1, only about 640 of them ran: the 231 rewritten at the time, plus 411 of the remaining 1,286. Much of the rest is probably the script compiler, editor support and the unused sound system below. Screens the trace didn't visit (multiplayer, save/load, options, cinematics) will add some.

## Cheat codes
**code**, `ISO.R32` `IsoCheckCheatCodes` (0x10052df4). The game compares a text against a table of 30 codes (case does not matter). The text comes from a script variable; how a player enters it is not tested yet.

Only some codes are switched on in the shipped game:

| Allowed | Codes |
|---|---|
| Single player | `ZIPPER`, `INFRARED`, `COMBUSTION`, `SETI`, `CAMERON`, `LUSHEE` |
| Multiplayer | `GREEN`, `BLUE`, `RED`, `YELLOW`, `BLACK`, `TURQ`, `PURPLE`, `ORANGE` (the eight player colors) |
| **Disabled** | `OMNIPOTENT`, `OMNIPOTENT OFF`, `SOLAR`, `MAXTECH`, `TILT`, `I'LL BE BACK`, `HARIKARI`, `HIGH VOLTAGE`, `CRASH AND BURN`, `QUIT`, `GENERATE AI FOOTPRINTS`, `BANKRUPT`, `HARMLESS`, `WIN`, `LOSE`, `GHOST IN THE MACHINE` |

What a few of them do, from the code: `MAXTECH` sets every player's tech level to 13; `TILT` gives 9,999 men and 99,999 materials; `SOLAR` and `ZIPPER` toggle game-wide switches; `CAMERON` is logged as "GOD MODE"; code 0xe (probably `GENERATE AI FOOTPRINTS`, which is disabled) runs `ISO.R32` 0x1004fbc4, a 3 KB developer routine that writes the AI's structure-building data to a file `AISTRUCT.BLD` (0xa200 bytes) and reads it back. The others are not traced yet.

Multiplayer has an anti-cheat: a player caught using a code is removed from the game ("Booting player %s : CHEATING DETECTED!").

## Debug features left in
* **code.** `Rtlib32.dll` is a debug build: debug assertions of the C runtime are still in, and many functions write `DebugStr` messages ("setres: 640 x 480", "CREATE FLIP CHAIN", ...). Under Wine they appear with `WINEDEBUG=+debugstr`.
* **code.** A debug window: `UtilsInitDebug` takes a window that receives every `DebugStr` line (message 0x41a).
* **code.** The engine's error messages can add "Executing Script %s File %s" when script debug mode is on (0x100260f6). The engine reads a `Debug` setting from `DOMINION.INI` and then switches debug mode on regardless, so it is always on: script errors report with a message instead of ending the game with a numbered fatal error.
* **code.** The game logic has debug files and modes: `IsoOpenDebugFiles`, `IsoDebugFile`, `IsoSetDebugMode`, `IsoSetDebugSprite`, `IsoDebugScriptMessageProc`, a profiler (`IsoGetProfilerValue`), and AI debug logs.
* **code.** A map editor is built into the game logic: `IsoMapEditDisplayLoop`, the `EDITPATH_*` path editor, `IsoInitMapEditAITempFiles`, a tile library compiler, autofill, and saving maps with their scenario description ("Perform Terrain Analysis on %s?", "Save Map"). The game's script `DOMINION.bin` imports these functions, and the game logic has an editor mode: the script starts the engine without sprites (`IsoInitEngine`'s 10th argument), and then the tile commands work, the AI rule sets are written to `_AITemp<n>.txt` files, and map loading repairs bad tiles. No editor program shipped and no menu entry is known; whether some scene of the script still leads to it is not tested yet. Single Player → Mission lists custom missions (`.wxp`) from that editor; the shipped campaign maps don't appear there (**tested**).
* **code.** An unfinished **random map generator** (`IsoGenerateMap`, imported by the game's script `DOMINION.bin`; no test has reached it). It makes an empty map of the given size and terrain and seeds the random numbers from the size, terrain and a seed. It then divides the map into 9x9-cell blocks and puts up to N numbered areas on an ellipse around the centre, each two blocks wide and filled with one of four objects (0x78-0x7b; probably start positions or bases). Next it scatters clusters of trees and rocks from a per-terrain table over the free cells, in two kinds and in several densities. Steps for three other block kinds and two stages in between exist but are empty. It prints its block grid to the debugger, and `IsoAPIGetRandomMapInfo` does nothing but take the game's state pointer. Random maps were planned and dropped.
* **code.** The script compiler's name, "Dominion Compiler", is in the engine's error messages, and the resource library has everything needed to *write* resource files (`ResCreateFile`, `ResWriteScriptBuf`, ...). The engine contains the tools side of Ion Storm's script system.
* **code.** The engine is not Ion Storm's own. `Rtlib32.dll` identifies itself as "Runtime Player 1.2 (C) Copyright 1996 7th Level, Inc." and still reads settings from the registry key `Software\7th Level, Inc.`. 7th Level started Dominion before Ion Storm took the game over, and the game runs on 7th Level's multimedia engine: hence the `Rt` prefix. Source paths survive in its assertions and debug info: `D:\DEV\RUNTIME\SRC\load.cpp`, `rtlib.cpp`, and `D:\DEV\SPRITE\SRC\create.cpp`, `direct.cpp`, `palette.cpp`, `utility.cpp` for the sprite library (the `Sprite*` half of the DLL). It even contains the string "7th Level Living Wall Paper", from another 7th Level product.
* **code.** A two-player controller layer sits unused in the engine: two player slots, each bound to keyboard, joystick or mouse, with lists of button combinations that fire script handlers when entered in sequence within a time window. Each player gets eight keys (four directions, combined into eight, and four buttons) or a joystick with four buttons and four- or eight-way directions (dead zones of 8000 and 16000 around the center recorded at startup); the two joysticks can be swapped between players. Dominion does capture joysticks at startup (`MaxJoySticks`), but nothing uses them. Nothing in Dominion uses it. It is not a sign of a planned split-screen or co-op mode for Dominion: it is a general feature of the 7th Level engine, the kind of input a two-player arcade or fighting game needs, and most likely came along from the engine's earlier titles.
* **code.** The engine can store settings scrambled, though Dominion's own `Dominion.ini` has no scrambled values. A string is stored as two characters per byte (`'0'` plus the low and high nibble of the byte plus the first letter of the key's name); a number in the registry has its bits shuffled through a fixed table and 0xBEBC205 added. A script command can also back `DOMINION.INI` up as `7thlevel.001`, `.002`, ... in the Windows directory: one more trace of the engine's 7th Level origin.
* **code.** The engine can run as a **Windows screensaver**. Its command line takes `/s` (screensaver), `/w` (living wallpaper, below), `/k` (kiosk) and `/t "name"` (the disc name used in "Please insert ... into the CD-ROM drive"). In screensaver mode the window stays on top, hides the cursor, and closes on any key, click or mouse move (the first move is ignored) unless the scene is interactive; it also answers Windows' "start screensaver" request. In `/k` mode the engine subclasses its window so that closing it logs the user off Windows (`ExitWindowsEx`): a kiosk mode, where the engine runs as the machine's only program. Dominion's launcher passes none of these switches.
* **code.** The engine has a **"7th Level Living Wall Paper"** mode (`/w`): its window sits behind all other windows and covers the screen, an animated desktop. Double-clicking it opens the Windows task list, and starting a second one closes the first. You can drag a rectangle (or click for the whole screen) to save that picture as a `.bmp` in the Windows directory and make it the Windows wallpaper, tiled or not. This is where the "7th Level Living Wall Paper" string comes from: the engine also ran 7th Level's desktop wallpaper product.
* **code.** The engine can **launch another program** from a script: it starts it with `ShellExecute`, hides its own window, and checks every second which windows are open. Once the program's own windows are gone (or none has appeared after 60 seconds), the engine's window comes back and a script handler runs. It fits a CD-ROM front end that starts installers or other titles; Dominion doesn't use it.
* **code.** The engine has its own **text objects** (Windows fonts drawn into sprites, with a password-entry mode that shows `*`), unused by Dominion, which draws all its text with bitmap fonts through `BmpFont.r32`.
* **code.** Scripts can **print**: a built-in prints a bitmap resource on the default printer (from the `device` entry of `WIN.INI`), portrait or landscape, as large as fits on one page and centred, titled with the game's name. Another built-in draws a file's icon (a `.ico` file, or an icon of a program) into a sprite. Dominion uses neither. With no printer set up, the original crashes when asked to print.
* **code.** The script language has 204 built-in functions, and the menu tour and mission 1 call only 15 of them (copies, arrays, random numbers, strings, sprite messages, visibility; **traced**). The rest include a message box captioned **"Studio7"** (probably the name of 7th Level's authoring tool; a guess), an assertion that reports a script error, a debug print, the Windows user name (or "guest"), a check whether another copy of the engine is already running (it counts windows of class `VIEW_CLASS` with the game's title), registry settings under `Software\7th Level, Inc.`, starting other programs, CD and MIDI control, and a game clock in seconds.
* **code.** The engine's angle math uses π = 3.1415: radians become tenths of a degree through 1800 / 3.1415, so its angles are about 0.003% too large (a right angle comes out as 900.03 tenths of a degree, truncated to 900). The integer sine, cosine and tangent tables are unaffected.
* **code.** The game's `.r32` files are plugins of the engine: the resource file lists them by name, the engine loads `<name>.r32` and calls its `RtLibInit`; scripts call their functions by name. A new `.r32` could be added by listing it in the resource file. (Error texts: "plugin '%s' has no RtLibInit function", "plugin '%s' initialization failed", "extern function '%s' not found".)
* **code.** Background transitions in the menu script engine: blinds, open from the middle, close from the edges, roll down, slides. Whether Dominion's menus use any is not checked yet.
* **code.** Tile debugging leftovers: the tile engine can build a tile-shaped test pattern (edges in color 10, the upper half filled with 125, the lower with 115, the middle rows 225 and 230; 0x1005c010), draw any rectangle of a tile (0x1005c99a) and draw a frame in color 9 (0x1005df6e). Nothing calls them.

## The engine's own sound system, unused
**code.** Its `.wav` reader and writer know a second format besides PCM: RIFF type "WAV7" instead of "WAVE", 7th Level's own compressed wave (a 10-byte header, then 4-bit IMA ADPCM). The engine can save 16-bit sounds in it.

**traced.** The engine has a complete sound system of about 200 functions: a software wave mixer on `waveOut` (with priorities, loops, fades and resampling), a MIDI sequencer (with a `RemapMidiDrums` option) and CD audio over MCI. Dominion never opens any of these devices. All its sound and music go through `WAVEPLAY.R32` (DirectSound and MCI CD audio), which only uses the engine's audio for a per-frame hook. Only 11 of the 200 functions ran.

## Multiplayer
* **tested.** The menu has its own **"Enter host address"** box, under SEARCH on the session screen, that players never saw. The original multiplayer module asked DirectPlay what kind of address its TCP/IP connection used. With the DirectPlay 6 files shipped on the disc the answer was "other", so the menu left addressing to DirectPlay's "Locate Session" dialog. With our module the box appears and works, and the screen also shows your own address.
* **code.** Connection types that no longer exist: modem and serial cable (`DPInitPhoneAddress`, `DPInitSerialAddress`), IPX, and MPlayer.com lobby support (`DPCheckForConnect`, `NetTallyMPlayerScores`, `MPLAYER.DLL`, `Mprdvous.dll`). DirectPlay lobby launching was supported too.
* **code.** Custom maps are sent to players who don't have them, in chunks (`DPSendFile`, message type 1). Not tested yet.
* **code.** Latency measurement (`DPSyncPlayer`): ping rounds to every player, used to time the lockstep.
* **code.** The version check between players (`ISO.R32` 0x100608c3, at startup): a CRC of the text "BUILD VERSION: Mon Jun  1 15:40:40 1998", the build's compile time. Only builds with the same time string can play together (unless `DisableISOCheck` is set).
* **code.** `DPEnumPlayersInSession`, listing the players of a session before joining, is never called.
* **tested.** When the host leaves a match, the others keep playing (the host is reported defeated): DirectPlay's peer-to-peer model. The module even picks a new "host slot", but nobody can join any more.
* **code.** `ISO.R32` `IsoDisplayPowerInfo` (0x100033fd): a developer box with the power figures of the selected structure (its grid, power given and used, its icon, damage), pausing the game while it is open. A flag at 0x10118030, toggled by a cheat code, gives every structure power.
* **code.** The Recon unit's double click (`ISO.R32` 0x10015137) sends it to a random unseen neighbouring sector, with the random numbers seeded from the clock (`time()`, whole seconds): the same click can send it elsewhere a second later. The function at 0x100147d0 meant to pick a scouting direction has a second mode that was never finished.
* **code.** Wounded units hit softer (`ISO.R32` 0x1000d96c): a weapon's damage is half again as much when its unit has more than half its health, and half when it has a quarter or less. A flag at 0x1013d6f4 (a cheat) makes the local player's units take no damage.
* **code.** `ISO.R32` 0x10005c34 would show a number (0-999, as three digits) on one of the debug sprites (`IsoSetDebugSprite`), but its first instruction jumps past the body: switched off before release.
* **code.** `ISO.R32` has `IsoInitReconnectAndPauseWindow` (a reconnect/pause window) and `IsoInitCRCCheck` (a checksum check for staying in sync). Not traced yet.

## Settings that the shipped Dominion.ini doesn't list
**code.** Read by the engine or the modules, with their defaults. None of these are in the `Dominion.ini` the installer writes.

| Section, key | Default | Meaning |
|---|---|---|
| `[Dominion] UseDDraw1` | 0 | use the DirectDraw 1 interface for the display mode |
| `[Dominion] DisableISOCheck` | 0 | multiplayer: allow joining a host with a different game version |
| `[Dominion] NumStartUnits` | 10 | multiplayer starting units (`MultiMen` and `MultiMat` next to it are listed) |
| `[Dominion] CinePlaybackMethod` | 2 | cutscene scaling (0 original, 1 doubled, 2 smoothed, 3 interlaced) |
| `[Dominion] SoundBuffers` | | number of sound channels (8-20) |
| `[Dominion] NetworkPing` | 3000 | code: multiplayer, milliseconds (the low 16 bits are kept); handed to the network module at startup and read back when a network game starts |
| `[Dominion] SyncThreshold` | 0 | code: multiplayer, handed to the network module with `NetworkPing` and read back into the game state (0xd8ec) |
| `[Dominion] EnableTerrainAnalysis` | 0 | code: read at startup and handed to the network module, but nothing reads it back: it switches nothing. The AI's terrain analysis (`ta_run`) does not depend on it |
| `[Dominion] NetColorMode` | 0 | code: network games only. With 0, players of the same race get different colours; with 1, units keep their race's colours as in single player. `IsoTogglePlayerColorMode` switches it during a match. The details of the colour choice are not checked yet |
| `[Dominion] KeyboardScrolling` | 0 | **tested**: 1 scrolls the map with the arrow keys and Home, End, Page Up, Page Down (diagonals); not while Alt or Ctrl is held. The game's own ini template (below) adds: "Shift-Arrow keys scroll fast, Ctrl-Arrow keys set facing directions on selected units" (the facing keys are not tested yet) |
| `[Dominion] DebuggingOn` | 0 | code: the debug switch of the game logic (state 0x10c); among other things it lets the scripts' tile commands (`IsoSetTile`, `IsoSetTileSet`) change the map during a game |
| `[Dominion] AutoSaveTime` | 0 | code: when set, a quick save is made every that many milliseconds in single player (0: never) |
| `[Dominion] RadarUpdateTime` | 1000 | code: milliseconds between radar redraws |
| `[audio] CdVolume`, `MidiVolume`, `WaveVolume` | 0x7fff | the engine's own volumes |
| `[audio] WaveFormatMask`, `MaxWaveBuffers`, `DisableWaveVolume`, `RemapMidiDrums`, `ProfileCD` | | the unused sound system |
| `[utils] Pentium` | 1 | treat a Pentium as a Pentium (processor detection) |
| `[DirectDraw] AllowTripleBuffer` | 1 | allow three screen buffers when video memory allows |
| `[DirectDraw] NoWaitForIFBlit` | 0 | don't wait for the interface panel copy |
| `[DirectDraw] WaitForFlip` | 0 | wait for the page flip later |
| `[DirectDraw] WaitIfStillDrawing` | 0 | read, but not used by the code rewritten so far |
| `[dominion] SafeModeSwitch` | 0 | read into the sprite engine; use not found yet |
| `[DirectDraw] AllowDirectDraw` | 1 | 0 forbids DirectDraw full screen |
| `[Dominion] AltTab` | 0 | 1 keeps the game running when you switch to another window; with 0, switching away (or Alt+Tab) pauses it, with the sound off and the arrow cursor |
| `[Dominion] Debug` | 0 | script debug mode; read, then always switched on regardless (see below) |
| `[Dominion] MaxJoySticks` | 2 | joysticks whose calibration (from `SYSTEM.INI`) is handed to the driver at startup |
| `[Dominion] MaxDropFrames` | 1 | read at startup; use not found yet |
| `[Input] Timeout` | 200 | milliseconds; used by the input code |
| `Mode640` (section: the game's title) | | read into the engine at every first scene; use not found yet |
| `LoMem` (same section) | | low-memory flag, read at every first scene and handed to plugins |
| `[resource]` | | data files copied to the hard disk, deleted at the next start (see below) |

**code.** `ISO.R32` (0x10091790) writes a default `Dominion.ini` into the Windows folder at startup when there is none. Its template (at 0x10150778) carries comments the shipped ini lacks, for example "GraphicsQuality: 1 = best performance, 2 = average, 3 = best quality", "Difficulty: 1 = Introductory, 2 = Experienced, 3 = Advanced", the popup settings (`PopupEnabled`, `PopupShowDisabling`, `PopupClickLength`), and network defaults that differ from the code's: `NetworkPing=5000` (the code falls back to 3000), `MultiMen=300`, `MultiMat=15000`.

## Leftovers from earlier versions
* **code.** The copy protection is switched off in the shipped game. `IsoCopyProtection` (`ISO.R32` 0x1005cfc7) passes check 100 through a function that just returns 1 (0x100c6d20), and checks 200 and 300 pass after a `rand()` call whose result is thrown away. With the flag at iso+0x170 set, every check passes at once. `IsoCheckForCD` (0x1005c252) is not reached in the tests.
* **code.** Resource files: the engine still reads an older, shorter header format (version 2), converting it on load.
* **code.** Playing from CD: the resource library can copy data files to the hard disk while playing, and list them in `[resource]` so leftovers are deleted at the next start. Dominion doesn't use it.
* **code.** `SpritePaintDirectDrawSurfacesBlack1`, an older version of the function that clears the screen, kept alongside the new one. `ClearVideoMem` (clear all video memory) is never called.
* **code.** The display can blit the frame instead of flipping pages; the game always flips.
* **code.** The game logic's debug output was switched off before release by making its functions return at once (`ISO.R32` 0x10095e98, 0x10095f64, `IsoDebugFile`); their bodies are still there. They printed messages by level (flags at iso+0xe09b) and wrote debug files. The AI also times each of its steps and keeps the slowest (0x10152b58); with timing output on (0x1017c9e0) it would print each step's time.
* **text.** The AI's debug names, used only for its (switched-off) debug output, list its task types: Scan Threats, Recon, Monitor Builds, Request to Build Infrastructure Unit, Request to Build Base Structure, Request to Build Mobile Unit, Request to Build Person, Base Defense, Attack, Defend, Build Base, Repair Base, Upgrade Base, Build Reserves, Reserves Rush, Sell Struct, Anti Virus, Implant Virus, Defense Supression, Spec Ops Mission and Write Log Data (`ISO.R32` 0x1009de5b). Its squads are Reserves, Defense, Attack, Recon, Engineer Corps, Special Forces, Virus Attack, Base Defense, Enemy Threat Group and Enemy Base (0x1009e31a), and its attitudes Passive, Defensive, Offensive and Aggressive (0x1009e26a). Task 10 has no name.
* **code.** `IsoDisplayRefineryInfo` (`ISO.R32` 0x100a655f), a debug export: with a refinery selected it pauses the game and shows a message box with the unit's id, type, player, position and its energy well's fields (position, state, amounts, frames, timers). Not seen called in the tests.
* **code.** `IsoSetDebugMode` (`ISO.R32` 0x100aa03b) treats the value 666 specially: instead of setting the debug mode it toggles a flag (iso+0x174 bit 5) that shows or hides the map-marker sprites of every building on the map (0x100f3b0d).
* **code.** A debug FPS display (`ISO.R32` main loop, 0x100af179): with iso+0xeeb bit 5 set, once a second it writes the frame rate and the average time taken by the movement, collision, display, AI and weapon loops into a text sprite (in a network game also the ping and network time).
* **code.** A function that walks a path's 20 sector slots without doing anything (`ISO.R32` 0x10098970), probably a check whose body was removed.

## Unfinished or broken in the original
These are kept in our rewrites for now (marked `original:` in the source) and are candidates for Phase 2.
* **code.** A unit's patrol order (`ISO.R32` 0x100b7a66) reads two of its local variables without setting them:
  * the direction of the next patrol step, which the helper it hands it to (0x100e663c) never fills in; units on patrol turn to whatever byte earlier calls left on the stack;
  * when the patrol was already set up, the step's time, which decides whether a newly started patrol sets off at once.
  Our rewrite takes both from the same stack slots, so the game behaves the same.
* **code.** The AI's script command 3, which ends the current base (`ISO.R32` 0x100e1073), means to clear the selection for the player that command 2 chose. The player is a local variable of each call, so command 3 always clears it for player 0. Our rewrite does the same.
* **code.** `SpriteGetSprite` is exported by the engine but was never written: it logs "**** SpriteGetSprite UNIMPLEMENTED ****" and returns nothing.
* **code.** Four more sprite functions were never written and only log "UNIMPLEMENTED": `SpriteSetTopMost` (keep a sprite above all others), `SpriteSwapOrder` (swap two sprites' drawing order), `SpriteCompToSprite` (draw one sprite into another; the code to do it exists, unused, at 0x100526a5), and `SpriteSetLevelRelative` (not exported, but script queue command 0x18, "level relative to sprite N", calls it, so the command exists but does nothing).
* **code.** `SpriteCompToBackground`, asked to draw one sprite into the background, goes on to draw every sprite, and (unless told to keep them) hides them all and empties their script queues.
* **code.** `SpritePaint`, when repainting everything by hand, reads the next sprite through a null pointer and would crash after the first sprite. That path doesn't seem to run (our rewrite walks the list correctly).
* **code.** The GDI DIB-section mode of the screen bitmap: its cleanup deletes the bitmap that was selected before instead of the DIB section, and deletes the DC with `DeleteObject`. Dominion uses DirectDraw, so this doesn't run.
* **code.** Note on the table above: the `[DirectDraw]` and `SafeModeSwitch` keys are read from `dominion.ini` in the Windows folder (a bare file name), not from the game's own `Dominion.ini`.
* **code.** `SpriteInitLibrary`, on a display without a palette, fills both halves of its copy of Windows' 20 static colors with the first 10.
* **code.** Switching the screen mode (0x10055c7e) frees the drawing bitmap but keeps the pointer to it.
* **code.** The bitmap copy routine (0x100629f4) can't copy more than one row of a run-length coded bitmap: it reads the next row's offset as 32 bits from a table of 16-bit offsets. With a horizontal flip on a plain bitmap it doesn't mirror; it reads from the wrong place instead. The game's backgrounds avoid both (run-length backgrounds go through the merge blitter).
* **code.** The sprite blitters, all hand-written assembly, have several latent bugs that the game never triggers:
  - with no color map, the plain-bitmap blitter jumps into a routine that removes one argument too few from the stack;
  - the fast path places the first row with the bitmap's width instead of its row pitch, so it writes outside the bitmap when the two differ (the 640-wide screen hides it);
  - the plain shadow aligns to 4 bytes using the wrong register;
  - effect type 2 in the sprite format is never drawn (an unfinished effect);
  - the plain-bitmap merge with a color map adds the color map's *address* to the source pointer after each row, so only the first row of such a bitmap can be right. The game never draws a plain bitmap with a color map.
* **code.** The tile map has small slips that rarely or never show:
  - a tile cut off at the bottom right of the view draws its lower rows to the view's edge even where the diamond is narrower, with pixels from the tile's next rows; the next row of tiles covers them;
  - a tile cut off on the right with an odd visible width loses its last column in the diamond's upper and lower parts;
  - the two masked overlays are crossed: the flag that enables the first mask array makes the code test the second array, and the other way round;
  - drawing the layers above the sprites puts the quality setting back only when something was drawn;
  - at the bottom of the view, tiles are drawn one pixel row further when the map has overlays than when it has none.
* **code.** The blit effects (`SpriteSetBlitFX`) have slips of their own:
  - the blend's code for a single leftover pixel (after the 4-at-a-time part) takes the sprite pixel two further on, in both blend routines;
  - the color-mapped blend, on an aligned destination, finishes in the plain blend's code: the last 1-3 pixels skip the color map, and it removes one argument too few from the stack (its caller hides it);
  - the screen door's short path has no check for zero pixels and would run through memory;
  - the shadow dither and the screen door of unclipped sprites get an uninitialized row number, so their pattern is stripes in a random phase rather than a checkerboard;
  - mirrored, effect 3 recolors the screen's pixel instead of the sprite's, and effect 1 never uses the screen door;
  - plain (not run-length) bitmaps are drawn without the effect and lose their color map.
* **code.** The DirectDraw tile blitter copies row 30 of a tile 2 bytes too long when the tile isn't 4-byte aligned on screen: two unused bytes of the tile file appear right of the diamond's lower tip, until the next row of tiles covers them.
* **code.** The pixel test for mirrored run-length sprites mirrors around the bitmap's row pitch instead of its width, so picking a mirrored sprite with the mouse is off by the difference (0-3 pixels).
* **code.** Sprite hit detection (`SpriteHitDetect`, `SpriteHitDetectRect`) is complete but unused, and looks like a leftover from an earlier engine:
  - it reads run-length bitmaps in the background format, not the sprite format, so it can't work on the game's sprites;
  - for flipped sprites it only moves the tested area to the other side of the bitmap and compares the pixels unmirrored;
  - with two run-length bitmaps, skipping a transparent run miscounts by one pixel: each skip shortens the tested row by a pixel, and in one direction the two bitmaps end up a pixel out of step;
  - two plain bitmaps are always read bottom-up, whatever their flags say.
* **code.** An unused function names drawing orders: "T", "T+O", "O+T", "T+S", "S+T", "O", "ARB" (tiles, overlays, sprites, arbitrary). "T+O" can never be returned: its test repeats the one for "T".
* **code.** The 16-bit color code is half-finished:
  - the engine calls the 16-bit fill with position and size swapped (x, y, w, h into a function that takes w, h, x, y);
  - that fill stores each pixel as 32 bits, so two zero bytes land after every row;
  - the two "only where both are not 0" copies test an uninitialized variable where they mean the height, and the 16-bit one tests 32 bits of the destination;
  - an unused 8-bit fill steps two bytes per pixel.
* **code.** The engine can read the desktop resolution from the Windows 95 registry (`Display\Settings`, "Resolution" and "BitsPerPixel"), but nothing calls that.
* **code.** Removing a hover region moves one region too many down, reading past the last one. The collision check, after removing a one-shot pair, skips the pair that moved into its place.
* **code.** `SpriteScreenShake` repaints the uncovered edges with bands one pixel wider on the left and bottom than on the right and top.
* **code.** `SpriteCopy` copies a sprite's whole block, including its list links and its info block pointer, so the copy shares the original's info and corrupts the sprite lists. The game doesn't call it.
* **code.** The bitmap packer can write a run-length method (block type 0x2000) that the unpacker refuses. The packer's setting is fixed to LZW, so no game file contains it.
* **code.** `ResExtractFile` reads a file's 6-byte head through the main resource file instead of the data file.
* **code.** `Tangent` reads one entry past its table for exactly 180 degrees.
* **code.** The sine table differs from a correctly rounded one in 93 of 901 entries (computed with low precision). Unit movement uses it as it is.
* **code.** Multiplayer: lowering the maximum number of players drops the wrong player (a 0-based index where a slot number is expected); the event that signals network messages has a fixed name, so two games on one Windows PC share it.
* **code.** The original IMA sound functions and the sprite run-length expander are hand-written assembly that doesn't keep a register (EBX) that compiled code relies on.
* More per module: `PHASE1B_MODULES.md` (fonts, cutscenes, sound, multiplayer, engine).
