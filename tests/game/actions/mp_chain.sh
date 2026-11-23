# the real build order on the joiner, from nothing: Main Plant, Power Plant, the next building,
# then an upgrade of the Main Plant. Run through mpstate.sh without the unlocked start.
# The start side changes between matches: the map's black corner tells which view we have.
. tests/game/actions/mp_lib.sh
sleep 32
clean :98 "$O/${C}_c0.png"
if [ "$(magick "$O/${C}_c0.png" -format '%[fx:p{630,5}.r+p{630,5}.g+p{630,5}.b<0.05?1:0]' info:)" = 1 ]; then
    MX=440 MY=200 PX=230 PY=70 CX=170 CY=320
else
    MX=220 MY=90 PX=470 PY=60 CX=300 CY=310
fi
echo "view: main $MX,$MY"
build :98 128 590 $MX $MY 25; clean :98 "$O/${C}_c1.png"
build :98 128 162 $PX $PY 40; clean :98 "$O/${C}_c2.png"
build :98 128 210 $CX $CY 40; clean :98 "$O/${C}_c3.png"
ck :98 379 386; sleep 1; ck :98 $MX $MY; sleep 1; park :98; clean :98 "$O/${C}_c4.png"
sleep 30; clean :98 "$O/${C}_c5.png"
for t in 128 158 185 214; do ck :98 $t 384; sleep 1.2; park :98; clean :98 "$O/${C}_t$t.png"; done
