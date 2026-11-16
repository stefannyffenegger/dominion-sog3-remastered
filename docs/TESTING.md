# Testing the rewrite against the original

How each rewritten part is checked, what the numbers mean, and what the tests do not reach. The scripts are in `tests/game/` (whole-game runs under Wine on a hidden display) and `tests/` (byte-exact unit comparisons for the engine); analysis helpers are in `tools/iso/`.

Everything here needs the player's own installed game in `work/` (never committed): `work/dh` (the main test install), `work/dh2` and `work/dh3` (640x480 installs whose `game/` folder links to `work/dh/game`), and reference output in `work/cmp/`.

## 1. After every part of `ISO.R32`
```
cmake --build build
python3 tools/iso/retcheck.py N          # the last N entries of ported.h: same "ret" as the original?
export DOMINION_HOME=$PWD/work/dh2
tools/install_modules.sh $PWD/work/dh/game build
tests/game/statecheck.sh                 # game logic, tick by tick (section 3)
tests/game/shots.sh work/cmp/p_menu;  tools/xvfb_session.sh stop
tests/game/mission.sh work/cmp/p_mis; tools/xvfb_session.sh stop
```
then compare the screenshots with `compare -metric AE` against `work/cmp/o_menu` (menus) and `work/cmp/m_orig2`, `m_orig3` (mission), and run `tests/game/exit.sh` (to the main menu, then Alt+F4: it must print 2 processes running, then 0).

Normal differences in pixels (animation phase, not errors):

| Screenshot | Normal values |
|---|---|
| menus | 0-40 |
| `1_briefing` | 2763, 1614 or 3790 (a frame of the intro movie) |
| `2_paused` | about 2170, 2740 or 14 |
| `3_mission` | 55-140 |
| `4_moved` | 50-80, sometimes about 350-365 |
| `5_scrolled` | 0 or 1 against one reference and about 5527 against the other (the edge scroll stops at one of two positions, also in the original) |
| `6_scrolled_left` | 0-115 |

A value outside these, or a "page fault" line in `$DOMINION_HOME/xvfb_game.log`, means something is wrong. Rerun once before suspecting the code: a slow start shifts the animation phase.

The reference screenshots were recorded with the all-original game, before the engine was replaced. They can only be recorded again after `tools/install_modules.sh --restore`.

## 2. Special runs
| Script | What it exercises | Compare with |
|---|---|---|
| `tests/game/groups.sh OUT` | drag-select, Ctrl+1, a right click on a unit (it opens the selection's popup menu, also in the original), 1; idle poses vary by up to about 300 pixels | two runs with `DOMINION_ISO_ORIGINAL=all` |
| `tests/game/hover.sh OUT` | cursors and the unit name popup | the same |
| `tests/game/keys.sh OUT` | arrow-key scrolling; needs `KeyboardScrolling=1` under `[Dominion]` in the test install's `Dominion.ini` (restore it afterwards) | the same |
| `tests/game/chat.sh OUT` | two-player match, chat lines (start positions are random: compare the text area only) | the same |
| `tests/game/popup.sh OUT` | the right-click popup menu: on the ground (a long click, the command points), on a unit and on the selection (the animated pool at the bottom left differs by about 50-90 pixels) | two runs with `DOMINION_ISO_ORIGINAL=all` |
| `tests/game/orders.sh OUT` | Ctrl+click (force attack: the infantry destroy the Recon) and Alt+click (guard) with the infantry selected; the keys: Ctrl force, Shift add to the selection, Alt order | two runs with `DOMINION_ISO_ORIGINAL=all` |
| `tests/game/recon.sh OUT` | double click on the Recon unit (its goal is random by wall clock: check that it runs, not pixels) | - |
| `tests/game/radar.sh OUT` | radar clicks; does nothing in mission 1, which has no radar | - |
| `tools/mp_test.sh start|shot|stop` | two players on one machine | by eye |

Idle animations differ by a frame from run to run, also between two runs of the same build. When a screenshot differs, look at what differs (positions and selections, or only poses) and run again.

## 3. The unit state log: checking game logic exactly
Screenshots cannot tell a logic change from timing. `statelog.c` in our `ISO.R32` writes one line per game tick with a hash of every unit's state, taken just before the tick's logic runs.

| Variable | Effect |
|---|---|
| `DOMINION_ISO_STATELOG=<Windows path>` | writes "tick units hash" lines |
| `DOMINION_ISO_STATELOG_UNITS=1` or `=from-to` | also each unit's fields (a range disturbs timing less) |
| `DOMINION_ISO_STATELOG_ORDERS=1` | scripted input for Human mission 1: all own units move into the enemy at tick 40 and on at tick 500; kneel at 60, crawl at 120, stand at 200 (with the interface's buttons, `IsoProcessAction`) |
| `DOMINION_ISO_STATELOG_CLOCK=15` | `timeGetTime` for the game logic advances 15 ms per displayed frame instead of real time |
| `DOMINION_ISO_ORIGINAL=name,name` / `all` / `all,-name` | keeps those functions original |
| `DOMINION_ISO_TRACE=<Windows path>` | a list of original addresses; logs "first call" for each that runs (needs `DOMINION_ISO_ORIGINAL=all` and `WINEDEBUG=+debugstr`) |

`tests/game/statecheck.sh` runs ours with the scripted input and the virtual clock and compares it with four saved original runs, `work/cmp/state_o1.log` to `state_o4.log`. It prints up to which tick ours equals each of them and up to which tick they equal each other.

* **Reading the result:** original runs agree with each other up to about tick 950-1000, varying from run to run. Ours must agree at least that far. One low value is not an error (single runs have come out as low as 857 with no code change); three runs of ours all diverging earlier than any pair of originals is.
* **When it diverges early:** `DOMINION_ISO_STATELOG_UNITS=from-to` around the tick on both builds, then `python3 tools/iso/statediff.py A.log B.log` shows the unit fields that differ; then bisect with `DOMINION_ISO_ORIGINAL`.
* **Recording the original runs again** (after changing the scripted input; about 12 minutes, run it in the background):
  ```
  export DOMINION_HOME=$PWD/work/dh2 DOMINION_ISO_STATELOG_ORDERS=1 DOMINION_ISO_STATELOG_CLOCK=15
  for r in 1 2 3 4; do
    DOMINION_ISO_ORIGINAL="all,-IsoDisplayLoop" tests/game/statelog.sh work/cmp/state_o$r.log 90
  done
  ```
  Our display loop stays in because it drives the virtual clock.
* **Why only 950 ticks:** some game logic runs per displayed frame rather than per tick. What differs first is a power-state flag on all of one player's structures flipping a tick earlier or later, after which the enemy AI reacts differently. Giving the engine the virtual clock too did not change it. Open.
* Fields left out of the hash because the display writes them every frame: 0x2d, bit 0x40 of 0x3e, bit 0x100 of 0x10e.

## 4. What the tests do not reach
Ported but never run by a test (as of part 22):
* building and placing structures, power grids changing, relays and their ring;
* vehicles: boarding, loading, entering buildings;
* destroyed structures and bridges, units under a new building;
* effect (splash) damage and `unit_take_damage`;
* the radar (clicks, the view box moving), the countdown clock, `IsoDisplayPowerInfo`;
* `IsoFindNextUnit`, `IsoSetUnitItem` and the other script-value functions, `unit_line_clear`, scouting (`recon_next_sector`);
* saved games (among them the record table of part 17), and the game logic in multiplayer;
* scout orders (`recon_scout_area`, `recon_rescout`);
* the action buttons other than the stances, and the toggle buttons (`iso_check_state`);
* clicks on structures and on other players' units, Shift+click, structure links (`structure_link`);
* the random map generator (`IsoGenerateMap`, `mapgen_*`);
* boarding vehicles, entering buildings, engineers' orders (`order_board`, `order_enter`, `order_engineer`);
* the popup menu's items for structures, vehicles and other players' units (`menu_for_unit`).

Needed to close this: a scenario with a base (a later mission or a saved game), building orders in the scripted input, and a two-player state log.

## 5. Working method for a part
1. `python3 tools/iso/extract.py LO HI work/tmp/iso/pN.c` and read the decompile.
2. `python3 tools/iso/retinfo.py work/dh/game/ISO.R32.orig ADDR...` for the part's functions **and every function they call**: many `Iso*` exports and some internals are stdcall. `tools/iso/proto.py ADDR...` prints the decompile's prototypes.
3. Read the assembly (`work/tmp/iso/iso.asm`) wherever the decompile shows `__ftol`, `extraout_`, `CONCAT`, `fastcall` or a 64-bit return: Ghidra hides x87 arithmetic, invents register arguments for plain cdecl functions, and has dropped whole computations (the attacker's health in the damage formula).
4. Write the C in `src/iso/`, add `PORTED(address, name)` lines to `ported.h`, test as in section 1, add the part to `PHASE1B_MODULES.md`, commit.

Traps met so far:
* a wrong calling convention unbalances the stack and crashes much later;
* `**(int **)(g + off)` in the decompile is `*I(int *, off)`, one dereference less than it looks;
* jump tables after an indirect `jmp` are disassembled as code and can look like a `ret N`;
* some calls pass a byte with whatever was above it in a register or on the stack; where that is the unit's own address our code can pass the same bits, otherwise it passes the clean value;
* a crash's location: `WINEDEBUG=+loaddll,+seh` gives the faulting address and the modules' load addresses.

The analysis inputs in `work/` come from the player's `ISO.R32`: `work/decomp/ISO.c` (Ghidra's decompile, one `// ==== name @ address` header per function, exported with the scripts in `tools/ghidra/`), `work/tmp/iso/iso.asm` (`objdump -d -M intel --no-show-raw-insn`), `work/tmp/iso/by_addr.txt` (address, name, size of every function) and `work/tmp/iso/live.txt` (the functions a trace saw running).
