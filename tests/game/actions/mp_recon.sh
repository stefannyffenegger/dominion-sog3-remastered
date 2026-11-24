# scouting (MP_MAP_ROW=1, DOMINION_ISO_STATELOG_ORDERS=6 and _UNITS=1): both players double-click
# their Recon unit (type 0x3c), which sets off on its own to sectors still in fog.
. tests/game/actions/mp_lib.sh
sleep 45
for d in :99 :98; do
    read -r x y < <(spot $d 60)
    DISPLAY=$d xdotool mousemove "$x" "$y" sleep 0.3 click --repeat 2 --delay 120 1; sleep 1; park $d
done
sleep 30; for d in :99 :98; do clean $d "$O/${C}_scout$d.png"; done
