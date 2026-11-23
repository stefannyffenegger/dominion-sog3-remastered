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
