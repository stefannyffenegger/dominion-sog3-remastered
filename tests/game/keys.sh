#!/usr/bin/env bash
# usage: keys.sh OUTDIR - Human mission 1, scrolling with the keys
cd "$(dirname "$(readlink -f "$0")")/../.."
export DOMINION_HOME=${DOMINION_HOME:-$PWD/work/dh2}
X=tools/xvfb_session.sh; O=$1; mkdir -p $O; D="DISPLAY=${XVFB_DISPLAY:-:99}"
$X start >/dev/null; sleep 15
for i in 1 2 3; do $X key Escape; sleep 3; done; sleep 6
$X click 320 75; sleep 3; $X click 320 136; sleep 4
$X click 440 151; sleep 1; $X click 320 322; sleep 6
for i in 1 2 3 4; do $X key Escape; sleep 6; done
$X click 510 425; sleep 4
DISPLAY=${XVFB_DISPLAY:-:99} xdotool mousemove 320 200
DISPLAY=${XVFB_DISPLAY:-:99} xdotool keydown Right sleep 0.5 keyup Right; sleep 3; $X shot $O/k1_right.png
DISPLAY=${XVFB_DISPLAY:-:99} xdotool keydown Down sleep 0.5 keyup Down; sleep 3; $X shot $O/k2_down.png
DISPLAY=${XVFB_DISPLAY:-:99} xdotool keydown Home sleep 0.5 keyup Home; sleep 3; $X shot $O/k3_home.png
DISPLAY=${XVFB_DISPLAY:-:99} xdotool keydown Left sleep 0.3 keyup Left keydown Up sleep 0.3 keyup Up; sleep 3; $X shot $O/k4_left_up.png
$X stop; sleep 2
