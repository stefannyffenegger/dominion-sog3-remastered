#!/usr/bin/env bash
# usage: popup.sh OUTDIR - Human mission 1: the right-click popup menu on the ground
# (nothing selected, a long click), on a unit, and on a selected unit (PopupEnabled=1, the default)
cd "$(dirname "$(readlink -f "$0")")/../.."
export DOMINION_HOME=${DOMINION_HOME:-$PWD/work/dh2}
X=tools/xvfb_session.sh; O=$1; mkdir -p $O; D=${XVFB_DISPLAY:-:99}
$X start >/dev/null; sleep 15
for i in 1 2 3; do $X key Escape; sleep 3; done; sleep 6
$X click 320 75; sleep 3; $X click 320 136; sleep 4
$X click 440 151; sleep 1; $X click 320 322; sleep 6
for i in 1 2 3 4; do $X key Escape; sleep 6; done
$X click 510 425; sleep 4
# on the ground the menu needs a long right click (PopupClickLength, 400 ms)
DISPLAY=$D xdotool mousemove 420 260 sleep 0.3 mousedown 3 sleep 0.8 mouseup 3; sleep 0.7; $X shot $O/p1_ground.png
DISPLAY=$D xdotool mousemove 600 300 sleep 0.3 click 1; sleep 1
DISPLAY=$D xdotool mousemove 285 98 sleep 0.5 mousedown 3 sleep 0.15 mouseup 3; sleep 0.7; $X shot $O/p2_unit.png
DISPLAY=$D xdotool mousemove 600 300 sleep 0.3 click 1; sleep 1
DISPLAY=$D xdotool mousemove 150 100 sleep 0.2 mousedown 1 sleep 0.2 mousemove 300 200 sleep 0.2 mousemove 380 250 sleep 0.2 mouseup 1; sleep 1
DISPLAY=$D xdotool mousemove 285 98 sleep 0.5 mousedown 3 sleep 0.15 mouseup 3; sleep 0.7; $X shot $O/p3_selected.png
$X stop; sleep 2
