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
| `tests/game/chat.sh OUT` | two-player match, chat lines (start positions are random: compare the text area only; either player's screenshot can come out as garbage, in any run, also with the original: rerun) | the same |
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
| `DOMINION_WAVE_LOG=<Windows path>` | our `WAVEPLAY.R32` writes "name volume pan" for each wave played by name; compare a run of ours with a `DOMINION_ISO_ORIGINAL=all` run (the orders run plays 59 waves) |
| `DOMINION_ISO_TRACE=<Windows path>` | a list of original addresses; logs "first call" for each that runs (needs `DOMINION_ISO_ORIGINAL=all` and `WINEDEBUG=+debugstr`) |

`tests/game/statecheck.sh` runs ours with the scripted input and the virtual clock and compares it with four saved original runs, `work/cmp/state_o1.log` to `state_o4.log`. It prints up to which tick ours equals each of them and up to which tick they equal each other.

* **Reading the result:** original runs agree with each other up to about tick 950-1000, varying from run to run. Ours must agree at least that far. One low value is not an error (single runs have come out as low as 857 with no code change). **A busy machine pulls this down a lot**: with another heavy process running, even all-original runs (`DOMINION_ISO_ORIGINAL=all,-IsoDisplayLoop`) came out at 718, and runs of ours at 246 and 718. When values are low, run the all-original configuration at the same time to compare, or wait for an idle machine; three runs of ours all diverging earlier than any pair of originals is.
* **Fork points:** all runs, original or ours, take the same path up to tick 970, then one of a few: some fork there, others at 980 or 1002. A run reported as 970 against all four reference runs (which happen to have taken another branch) is one of these forks: compare it with an all-original run made at the same time (`tools/iso/statecmp.py A.log B.log`), which usually took the same branch.
* **One game at a time:** with a second game running on another display (its own `DOMINION_HOME` copy and `XVFB_DISPLAY`), every state check, the all-original one included, forked at tick 656, and the screenshots differed more (tested 2026-11-21 on 8 cores). The engine's timers run on real time, so the state checks need an otherwise idle machine. `exit.sh` counts only its own install's game processes, so it can run beside another install.
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
Ported but never run by a test (as of part 34):
* building and placing structures, power grids changing, relays and their ring;
* vehicles: boarding, loading, entering buildings;
* destroyed structures and bridges, units under a new building;
* effect (splash) damage and `unit_take_damage`;
* the radar (clicks, the view box moving), the countdown clock, `IsoDisplayPowerInfo`;
* `IsoFindNextUnit`, `IsoSetUnitItem` and the other script-value functions, `unit_line_clear`, scouting (`recon_next_sector`);
* the game logic in multiplayer (saved games: see `savecheck.sh` below);
* scout orders (`recon_scout_area`, `recon_rescout`);
* the action buttons other than the stances, and the toggle buttons (`iso_check_state`);
* clicks on structures and on other players' units, Shift+click, structure links (`structure_link`);
* the random map generator (`IsoGenerateMap`, `mapgen_*`);
* the sector path helpers (`paths.c`), the tile library and the map editor helpers (`mapedit.c`);
* most AI script commands (`aiglue.c`), the text scenario description parser (`sdl.c`; the game loads compiled `.sdc` files);
* boarding vehicles, entering buildings, engineers' orders (`order_board`, `order_enter`, `order_engineer`);
* the popup menu's items for structures, vehicles and other players' units (`menu_for_unit`).

Needed to close this: boarding on exact ticks and a two-player state log.

### Measuring what a scenario reaches
`tests/game/scenario.sh SCENARIO OUTDIR [SECONDS]` plays any mission through the normal menu, from a throwaway install of symlinks (`work/dhs`) in which `Human01.sdc` is the chosen scenario, so no game file changes. It writes screenshots and `OUTDIR/reached.txt`, the functions not yet rewritten that ran. `python3 tools/iso/coverage.py report DIR...` adds these up. `DOMINION_ISO_ORIGINAL=all` runs the same scenario on the original code.

`SCENARIO_ACTIONS=tests/game/actions/base.sh` also drives the interface on Darken 12: it builds and places a Power Plant, trains units, moves a group, and uses repair, upgrade and sell. These clicks land in real time, so such a run shows what is reached and whether it crashes, but it cannot be compared tick by tick: fights differ between two runs even on the original.

`tests/game/basecheck.sh OUTDIR` is the exact version for structures. On Darken 12, with the virtual clock (`DOMINION_ISO_STATELOG_ORDERS=2`), it starts a Power Plant at tick 100, places it at 300, damages it and another plant at 320, sends an engineer at 330, uses the repair button at 340 and the upgrade button at 400, and sells at 560, all through the interface's own calls. It runs the original three times and ours twice. At part 223 the original runs split from each other at ticks 843 to 844 and ours split from them at 843 to 850, so ours is within the original's own variation. Rare runs split earlier, on the original too (tick 736 in 12 original runs at part 224, tick 764 in 8 of ours). Finishing a build through the production bar is not in this test. Neither is boarding: Darken 12 starts without a carrier, and of the types with passenger room that were tried, 0x25 deploys in place and 0x35 flies off.

`tests/game/savecheck.sh OUTDIR` tests saved games on the same mission and orders. The game is saved at tick 600 by the original (twice) and by ours, then an original's save and our save are each loaded by both, through the menu, and the state is compared from tick 600 on. At part 223 our save differs from an original's by 2,654 of 2,048,045 bytes and two original saves differ from each other by 2,705. All four loads continue from tick 600. The loaded runs agree up to tick 2053 at least, and the run that splits there is one of the original's, so ours stays within the original's own variation. The virtual clock follows real time when no frame is shown for half a second, because loading waits on the clock with nothing displayed.

`tests/game/mpcoverage.sh OUTDIR [SECONDS]` measures a two-player match on one machine (2_2Prong, both players idle). It is for coverage only, with no state comparison yet. The multiplayer lobby and its options screen offer no computer players, so the AI is only exercised by the campaign missions.

`tests/game/mpstate.sh OUTDIR PAIRING [SECONDS]` is the strongest check. In a two-player match both games compute the same battle, so one side can run the original and the other ours (`op`, `po`; also `oo`, `pp`), and their unit state must agree on every tick, with no run-to-run noise. Selection is left out of the hash in a match, because it is each player's own. So far it covers idle matches and selecting units (about 985 ticks, no difference in any pairing). Building and fighting through the interface are the next step. `MP_ACTIONS=<script>` runs interface clicks during the match.

Multiplayer matches cannot be saved, so scenarios cannot start from a checkpoint. With `DOMINION_ISO_STATELOG_ORDERS=5` both games instead give every player full tech, plenty of men and materials, and every build prerequisite at tick 60, which fills all four build tabs without the long buildout. This is test code only. The real build order needs its own scenario. While a new structure goes up, the two sides can differ for about 9 ticks and then agree again; the all-original match does the same.

Reached without player input, 240 seconds each (Human 7 for 60), of 1,099 functions not yet rewritten at part 223: Human 7 84, Human 12 102, Scorpion 12 114, Mercenary 12 132, Darken 12 179, together 189 (19% of their bytes). With the scripted interface session on Darken 12: 210 (21%). With a two-player match as well: 262 (28%).

## 5. Working method for a part
1. `python3 tools/iso/extract.py LO HI work/tmp/iso/pN.c` and read the decompile.
2. `python3 tools/iso/retinfo.py work/dh/game/ISO.R32.orig ADDR...` for the part's functions **and every function they call**: many `Iso*` exports and some internals are stdcall. `tools/iso/proto.py ADDR...` prints the decompile's prototypes.
3. Read the assembly (`work/tmp/iso/iso.asm`) wherever the decompile shows `__ftol`, `extraout_`, `CONCAT`, `fastcall` or a 64-bit return: Ghidra hides x87 arithmetic, invents register arguments for plain cdecl functions, and has dropped whole computations (the attacker's health in the damage formula).
4. Write the C in `src/iso/`, add `PORTED(address, name)` lines to `ported.h`, test as in section 1, add the part to `PHASE1B_MODULES.md`, commit.
5. Before claiming a test reaches a function, check `work/tmp/iso/live.txt` (or trace it with `DOMINION_ISO_TRACE`).
6. A very large function can be ported a piece at a time: our function handles the cases rewritten so far and hands the rest to the original's body through a naked trampoline that replays the original's prologue (the bytes the hook overwrote) and jumps past it (see the history of `sdlexec.c`).

Traps met so far:
* a wrong calling convention unbalances the stack and crashes much later;
* `**(int **)(g + off)` in the decompile is `*I(int *, off)`, one dereference less than it looks;
* jump tables after an indirect `jmp` are disassembled as code and can look like a `ret N`;
* some calls pass a byte with whatever was above it in a register or on the stack; where that is the unit's own address our code can pass the same bits, otherwise it passes the clean value;
* a crash's location: `WINEDEBUG=+loaddll,+seh` gives the faulting address and the modules' load addresses.

The analysis inputs in `work/` come from the player's `ISO.R32`: `work/decomp/ISO.c` (Ghidra's decompile, one `// ==== name @ address` header per function, exported with the scripts in `tools/ghidra/`), `work/tmp/iso/iso.asm` (`objdump -d -M intel --no-show-raw-insn`), `work/tmp/iso/by_addr.txt` (address, name, size of every function) and `work/tmp/iso/live.txt` (the functions a trace saw running).
