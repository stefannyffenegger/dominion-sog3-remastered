/* Functions of the original game logic that are replaced by our rewrite.
 * PORTED(original address, our function); our function has the original's calling
 * convention and arguments. Grouped by part; see docs/PHASE1B_MODULES.md. */

/* power.c */
PORTED(0x10001000, power_mark)
PORTED(0x100012fa, power_reachable)
PORTED(0x1000158c, power_grid_add)
PORTED(0x1000172b, power_grid_remove)
PORTED(0x10001857, power_grids_clear)
PORTED(0x10001959, power_structure_placed)
PORTED(0x10001a6b, power_structure_removed)
PORTED(0x10001c1d, power_flag_near)
PORTED(0x10001dac, power_remark_flagged)
PORTED(0x10001eaa, IsoResetAllStructurePowerGrids)
