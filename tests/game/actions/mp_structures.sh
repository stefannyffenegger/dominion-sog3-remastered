# structures on the unlocked start (ORDERS=5), joiner: place one, repair mode, upgrade, sell.
. tests/game/actions/mp_lib.sh
sleep 32
clean :98 "$O/${C}_s0.png"
if [ "$(magick "$O/${C}_s0.png" -format '%[fx:p{630,5}.r+p{630,5}.g+p{630,5}.b<0.05?1:0]' info:)" = 1 ]; then
    MX=440 MY=200
else
    MX=220 MY=90
fi
echo "view: main $MX,$MY"
build :98 128 590 $MX $MY 25; clean :98 "$O/${C}_s1.png"
ck :98 379 386; sleep 1; ck :98 $MX $MY; sleep 1; park :98; sleep 20; clean :98 "$O/${C}_s2.png"
ck :98 351 386; sleep 1; ck :98 $MX $MY; sleep 1; park :98; sleep 8; clean :98 "$O/${C}_s3.png"
ck :98 323 386; sleep 1; ck :98 $MX $MY; sleep 1; park :98; sleep 8; clean :98 "$O/${C}_s4.png"
