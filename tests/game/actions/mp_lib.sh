# helpers for two-player interface scripts, sourced by the MP_ACTIONS scenarios.
# The screen capture is often garbled, so clean retries until the bar's corner matches a
# reference crop. work/tmp/bar_ref.png is made locally from a game frame and never committed;
# without it the first capture is kept.
clean() { # display file
    for _ in 1 2 3 4 5 6 7 8 9 10 11 12; do
        DISPLAY=$1 magick import -window root "$2"
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
# spot DISPLAY TYPE: the screen position of the local player's first unit of TYPE (from the
# state log, DOMINION_ISO_STATELOG_UNITS=1), while the view follows the player's first unit of
# type below 16 (DOMINION_ISO_STATELOG_ORDERS=6). Host is p0, joiner p1.
spot() {
    local log=$O/${C}_h.log me=p0; [ "$1" = :98 ] && log=$O/${C}_j.log me=p1
    python3 - "$log" "$me" "$2" <<'P'
import sys
cur, last = [], []
for l in open(sys.argv[1], errors='replace'):
    if l.startswith('  '): cur.append(l.split())
    elif l[0].isdigit(): last, cur = cur, []
me = [u for u in last if u[2] == sys.argv[2]]
pos = lambda u: tuple(map(int, u[3].strip('()').split(',')))
(fx, fy), (tx, ty) = pos([u for u in me if int(u[1][1:]) < 16][0]), pos([u for u in me if u[1] == 't' + sys.argv[3]][0])
print(320 + tx - fx, 187 + ty - fy)
P
}
