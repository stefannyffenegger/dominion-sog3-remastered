# helpers for two-player interface scripts, sourced by the MP_ACTIONS scenarios.
# The screen capture is often garbled, so clean retries until the bar's corner matches a
# reference crop. work/tmp/bar_ref.png is made locally from a game frame and never committed;
# without it the first capture is kept.
clean() { # display file
    for _ in 1 2 3 4 5 6 7 8 9 10 11 12; do
        DISPLAY=$1 timeout 10 magick import -window root "$2"  # a hung capture stalls the game
        [ -f work/tmp/bar_ref.png ] || return 0
        v=$(magick "$2" -crop 40x40+600+440 +repage miff:- | compare -metric AE - work/tmp/bar_ref.png null: 2>&1 | head -1 | awk '{print int($1)}')
        [ "${v:-9999}" -lt 200 ] && return 0
        sleep 0.4
    done; return 1
}
ck() { DISPLAY=$1 xdotool mousemove "$2" "$3" sleep 0.3 click 1; }
park() { DISPLAY=$1 xdotool mousemove 320 300; sleep 0.5; }
# build DISPLAY TABX SLOTX GROUNDX GROUNDY SECONDS: start an item, wait for it, place it
build() {
    ck "$1" "$2" 384; sleep 1; ck "$1" "$3" 422; sleep "$6"
    ck "$1" "$3" 422; sleep 1; ck "$1" "$4" "$5"; sleep 3; park "$1"
}
# spot DISPLAY TYPE: the screen position of the local player's first unit of TYPE (or "new":
# its newest unit; the type goes to stderr), from the
# state log (DOMINION_ISO_STATELOG_UNITS=1) and the view's corners that mode 6 or 7 logs (the
# screen starts 128 pixels inside them). Host is p0, joiner p1.
spot() {
    local log=$O/${C}_h.log me=p0; [ "$1" = :98 ] && log=$O/${C}_j.log me=p1
    python3 - "$log" "$me" "$2" <<'P'
import sys
cur, last, view = [], [], None
for l in open(sys.argv[1], errors='replace'):
    if l.startswith('# view'): view = list(map(int, l.split()[2:4]))
    elif l.startswith('  '): cur.append(l.split())
    elif l[0].isdigit(): last, cur = cur, []
mine = [u for u in last if u[2] == sys.argv[2]]
u = max(mine, key=lambda u: int(u[0])) if sys.argv[3] == 'new' else [u for u in mine if u[1] == 't' + sys.argv[3]][0]
print(u[1], file=sys.stderr)
x, y = map(int, u[3].strip('()').split(','))
print(x - view[0] - 128, y - view[1] - 128)
P
}
# place DISPLAY TAG: with a structure ready to place, hover a grid of spots and click the first
# whose placement outline is green (bright green pixels in the playfield, beyond those already
# there, such as selection rings; red means refused)
greens() { magick "$1" -crop 640x370+0+0 +repage -fx 'g>0.6&&r<0.3&&b<0.3' -format '%[fx:int(mean*w*h)]' info:; }
place() {
    local x y g g0
    park "$1"; clean "$1" "$O/${C}_$2_try.png"; g0=$(greens "$O/${C}_$2_try.png")
    for y in 300 240 180 120 60; do
        for x in 160 260 380 480 560 100; do
            DISPLAY=$1 xdotool mousemove $x $y; sleep 0.8; clean "$1" "$O/${C}_$2_try.png"
            g=$(greens "$O/${C}_$2_try.png")
            if [ $((g - g0)) -gt 150 ]; then ck "$1" $x $y; sleep 2; park "$1"; echo "$2 placed at $x,$y"; return 0; fi
        done
    done
    echo "$2: no free spot"; return 1
}
