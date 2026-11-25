# a destroyed structure (MP_MAP_ROW=1, DOMINION_ISO_STATELOG_ORDERS=7 and _UNITS=1), on the
# joiner: Main Plant, Power Plant and an Energy Beacon, then the starting units box-selected and
# sent against the beacon with Ctrl+click (a forced attack on an own unit) until it is gone. A
# structure repairs itself by default, faster than infantry can damage it, so the Auto Repair
# item of its right-click menu switches that off first.
. tests/game/actions/mp_lib.sh
sleep 45
build :98 128 590 320 290 25
ck :98 128 384; sleep 1; ck :98 162 422; sleep 42; ck :98 162 422; sleep 1; place :98 power
ck :98 158 384; sleep 1; ck :98 210 422; sleep 40; ck :98 210 422; sleep 1
read -r px py < <(spot :98 128)  # its outline is too small to see, so: beside the Power Plant, until the log has it
s=1; [ "$px" -gt 320 ] && s=-1  # toward the middle of the screen, so its menu fits
for d in "100 10" "70 -70" "0 80" "150 60" "0 -90"; do
    set -- $d; set -- $(($1 * s)) $2; ck :98 $((px + $1)) $((py + $2)); sleep 2
    spot :98 64 >/dev/null 2>&1 && { echo "beacon placed at $((px + $1)),$((py + $2))"; break; }
done
# its automatic repair off through its right-click menu (flag bit 1 of +0x3e, checked in the log)
repair_on() { python3 -c 'import sys
cur, last = [], []
for l in open(sys.argv[1], errors="replace"):
    if l.startswith("  "): cur.append(l.split())
    elif l[0].isdigit(): last, cur = cur, []
print(*[int(u[7][1:9], 16) >> 1 & 1 for u in last if u[1] == "t64" and u[2] == "p1"])' "$O/${C}_j.log"; }
read -r x y < <(spot :98 64)
DISPLAY=:98 xdotool mousemove "$x" $((y - 15)) sleep 0.5 mousedown 3 sleep 0.15 mouseup 3; sleep 0.7; clean :98 "$O/${C}_menu.png"
read -r cx cy < <(checkbox "$O/${C}_menu.png"); ck :98 $((cx + 40)) "$cy"; sleep 2  # its Auto Repair item, ticked
[ "$(repair_on)" = 0 ] && echo "beacon repair off" || echo "beacon repair still on"
read -r ix iy < <(spot :98 0)
DISPLAY=:98 xdotool mousemove $((ix - 130)) $((iy - 80)) sleep 0.3 mousedown 1 sleep 0.3 mousemove $ix $iy sleep 0.2 mousemove $((ix + 130)) $((iy + 80)) sleep 0.4 mouseup 1
sleep 1; park :98
read -r x y < <(spot :98 new); y=$((y - 20)); echo "units at $ix,$iy, beacon at $x,$y"
DISPLAY=:98 xdotool mousemove "$x" "$y" sleep 1 keydown ctrl sleep 0.5; clean :98 "$O/${C}_aim.png"
DISPLAY=:98 xdotool click 1 sleep 0.3 keyup ctrl; sleep 1; park :98
sleep 30; clean :98 "$O/${C}_attack.png"
for _ in $(seq 60); do sleep 10; spot :98 64 >/dev/null 2>&1 || { echo "beacon destroyed"; break; }; done
sleep 20; clean :98 "$O/${C}_gone.png"
