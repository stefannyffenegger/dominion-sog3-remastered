# the joiner's own colours (MP_MAP_ROW=1, DOMINION_ISO_STATELOG_ORDERS=7 and _UNITS=1): a Main
# Plant, then the red pixels of its trim. Two players of the same race get different colours in
# a network game: on the original the joiner's plant is grey (about 0 red), in the race's red not.
. tests/game/actions/mp_lib.sh
sleep 45
build :98 128 590 320 290 25
sleep 5; park :98; clean :98 "$O/${C}_colour.png"
echo "red on the joiner's Main Plant: $(magick "$O/${C}_colour.png" -crop 160x60+240+240 +repage -fx 'r>0.6&&g<0.3&&b<0.3' -format '%[fx:int(mean*w*h)]' info:)"
