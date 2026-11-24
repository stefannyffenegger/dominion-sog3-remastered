# boarding (MP_MAP_ROW=1, DOMINION_ISO_STATELOG_ORDERS=7 and _UNITS=1), on the joiner: Main
# Plant, Power Plant and Hover Pad (placed where the outline turns green), a Hover APC (built
# once the pad stands, picked as the newest unit), then the starting soldiers box-selected and sent into it.
. tests/game/actions/mp_lib.sh
sleep 45
read -r x y < <(spot :98 60)  # the Recon a little aside, so the box below holds soldiers only
ck :98 "$x" "$y"; sleep 1; ck :98 130 200; sleep 1; park :98
build :98 128 590 320 290 25
ck :98 128 384; sleep 1; ck :98 162 422; sleep 42; ck :98 162 422; sleep 1; place :98 power
ck :98 158 384; sleep 1; ck :98 542 422; sleep 75; ck :98 542 422; sleep 1; place :98 hoverpad
sleep 35; ck :98 214 384; sleep 1; ck :98 353 422; sleep 70; park :98  # Hover APC, once the pad stands
clean :98 "$O/${C}_apc.png"
read -r ix iy < <(spot :98 0)  # the starting soldiers, boxed and kept as group 1
DISPLAY=:98 xdotool mousemove $((ix - 110)) $((iy - 70)) sleep 0.3 mousedown 1 sleep 0.3 mousemove $ix $iy sleep 0.2 mousemove $((ix + 110)) $((iy + 70)) sleep 0.4 mouseup 1
sleep 1; DISPLAY=:98 xdotool key ctrl+1; sleep 1; park :98; clean :98 "$O/${C}_sel.png"
read -r x y < <(spot :98 new)  # a click on the docked APC picks the pad, a box picks the APC
DISPLAY=:98 xdotool mousemove $((x - 45)) $((y - 45)) sleep 0.3 mousedown 1 sleep 0.3 mousemove $x $y sleep 0.2 mousemove $((x + 45)) $((y + 25)) sleep 0.4 mouseup 1
sleep 1; park :98; clean :98 "$O/${C}_apcsel.png"; ck :98 "$ix" $((iy - 90)); sleep 8  # the APC next to them
park :98; clean :98 "$O/${C}_moved.png"
DISPLAY=:98 xdotool key 1; sleep 1
read -r x y < <(spot :98 new); y=$((y - 35)); echo "soldiers at $ix,$iy, APC at $x,$y"  # it hovers above its spot
DISPLAY=:98 xdotool mousemove "$x" "$y"; sleep 1.5; clean :98 "$O/${C}_over.png"
ck :98 "$x" "$y"; sleep 1; park :98; sleep 20; clean :98 "$O/${C}_board.png"
read -r x y < <(spot :98 new); y=$((y - 35)); ck :98 "$x" "$y"; sleep 1; ck :98 480 130; sleep 10  # loaded, flown off
park :98; clean :98 "$O/${C}_flown.png"
read -r x y < <(spot :98 new); y=$((y - 35)); ck :98 "$x" "$y"; sleep 20  # selected, a click on itself unloads
park :98; clean :98 "$O/${C}_unload.png"
