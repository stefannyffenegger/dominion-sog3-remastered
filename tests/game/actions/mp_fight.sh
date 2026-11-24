# infantry fight (MP_MAP_ROW=1 for Backwoods, DOMINION_ISO_STATELOG_ORDERS=6 and _UNITS=1): both
# players box-select their starting units and walk them toward each other through the interface.
# Moves onto unexplored ground are refused, so the test keeps the view on the units and each hop
# clicks seen ground a little ahead, in the direction of the other player's start.
. tests/game/actions/mp_lib.sh
start() { # log player: the player's first unit position "x y"
    grep -am1 " $2 (" "$1" | sed 's/.*(\(-\?[0-9]*\),\(-\?[0-9]*\)).*/\1 \2/'
}
go() {
    local log=$O/${C}_h.log me=p0 you=p1; [ "$1" = :98 ] && log=$O/${C}_j.log me=p1 you=p0
    read -r ax ay < <(start "$log" $me); read -r bx by < <(start "$log" $you)
    read -r x y hops < <(python3 -c "
import math; dx,dy=$bx-($ax),$by-($ay); d=math.hypot(dx,dy) or 1
print(int(320+200*dx/d), int(190+150*dy/d), int(d/2/140)+14)")
    echo "$me at $ax,$ay toward $bx,$by: clicks $x,$y, $hops hops"
    DISPLAY=$1 xdotool mousemove 4 4 sleep 0.3 mousedown 1 sleep 0.3 mousemove 320 180 sleep 0.2 mousemove 636 368 sleep 0.4 mouseup 1
    sleep 6
    local j=0 off
    for hop in $(seq 1 "$hops"); do  # aside every other hop, in case the target is a tree
        off=$(( (j++ % 3 - 1) * 70 ))
        ck "$1" "$x" $((y + off)); sleep 0.3; park "$1"; sleep 8
    done
    clean "$1" "$O/${C}_go$1.png"
}
sleep 32
go :99 & go :98 & wait
