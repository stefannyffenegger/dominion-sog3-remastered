# Interface script for scenario.sh: loads the saved game $SCENARIO_SAVE through the menu.
cp "$SCENARIO_SAVE" work/dhs/game/test1.WSP
sleep 6
$X key Escape; sleep 3
$X click 510 270; sleep 3
$X click 100 132; sleep 2; $X shot "$O/l1_chosen.png"
$X click 510 270; sleep 8; $X shot "$O/l2_loaded.png"
