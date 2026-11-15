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
PORTED(0x10001f74, power_check)
PORTED(0x100023e3, power_icon_hide)
PORTED(0x100024cb, power_percent)
PORTED(0x1000258e, power_grid_update_x)
PORTED(0x100027ca, power_grid_fill)
PORTED(0x10002bc1, power_grid_sum)
PORTED(0x10002f2e, power_icon_alloc)
PORTED(0x10002fd2, power_icon_free_x)
PORTED(0x1000300f, power_unit_update)
PORTED(0x10003309, power_unit_tick)
PORTED(0x10003331, power_mark_all)
PORTED(0x100033fd, IsoDisplayPowerInfo)
PORTED(0x100036d4, power_reach_x)
PORTED(0x10003b00, power_unit_counts)
PORTED(0x10003bb2, power_reach_others)
PORTED(0x10003e95, power_player_reach)

/* view.c */
PORTED(0x10003fd4, IsoInitIntfPowerBar)
PORTED(0x10004067, power_bar_set)
PORTED(0x1000413d, IsoPowerBarBrowse)
PORTED(0x10004190, view_set_scroll_box)
PORTED(0x100043c3, IsoCheckRadarScreenAreaUpdate)
PORTED(0x10004573, view_shake)
PORTED(0x100045fe, view_scroll_keys)
PORTED(0x10004824, view_scroll)
PORTED(0x10005579, view_center_on)
PORTED(0x1000585b, selection_clear)
PORTED(0x10005968, selection_remove)

/* display.c */
PORTED(0x10005bb1, debug_sprite_show)
PORTED(0x10005bf1, debug_sprite_hide)
PORTED(0x10005c34, debug_sprite_number)
PORTED(0x10005d13, unit_line_clear)
PORTED(0x10005ef5, unit_direction)
PORTED(0x1000605e, messages_draw)
PORTED(0x10006459, table_get)
PORTED(0x100064c8, IsoSetDisplayDelay)
PORTED(0x1000652a, IsoInitDisplayProcess)
PORTED(0x1000659a, IsoStopDisplayProcess)

/* loop.c */
PORTED(0x100065c3, IsoDisplayLoop)
PORTED(0x10007668, IsoMapEditDisplayLoop)
PORTED(0x1000788d, IsoSetDebugSprite)
PORTED(0x100079bd, IsoFindNextUnit)
PORTED(0x10007e49, IsoSelectGroup)

/* screen.c */
PORTED(0x10008772, IsoGetProfilerValue)
PORTED(0x10008797, IsoRefreshScreen)
PORTED(0x100087fb, IsoRefreshRect)
PORTED(0x10008847, IsoRadarClickPos)
PORTED(0x10008c12, IsoSetDirectDrawMode)
PORTED(0x10008c7d, IsoHideUnitNamePopup)
PORTED(0x10008d30, radar_show)
PORTED(0x10008e49, unit_update_bounds)
PORTED(0x10008f27, countdown_draw)

/* unit.c */
PORTED(0x100090e0, unit_apply_upgrade)
PORTED(0x100092bb, unit_place)
PORTED(0x1000a474, unit_sector_update)

/* action.c */
PORTED(0x1000a86b, unit_action_tick)
