#!/usr/bin/env bash
# usage: groups.sh OUTDIR - Human mission 1: make group 1, deselect, recall it
cd "$(dirname "$(readlink -f "$0")")/../.."
export DOMINION_HOME=${DOMINION_HOME:-$PWD/work/dh2}
X=tools/xvfb_session.sh; O=$1; mkdir -p $O; D=${XVFB_DISPLAY:-:99}
$X start >/dev/null; sleep 15
for i in 1 2 3; do $X key Escape; sleep 3; done; sleep 6
$X click 320 75; sleep 3; $X click 320 136; sleep 4
$X click 440 151; sleep 1; $X click 320 322; sleep 6
for i in 1 2 3 4; do $X key Escape; sleep 6; done
$X click 510 425; sleep 4
DISPLAY=$D xdotool mousemove 150 100 sleep 0.2 mousedown 1 sleep 0.2 mousemove 300 200 sleep 0.2 mousemove 380 250 sleep 0.2 mouseup 1; sleep 2
DISPLAY=$D xdotool mousemove 320 200; sleep 1; $X shot $O/g1_selected.png
DISPLAY=$D xdotool key ctrl+1; sleep 2; $X shot $O/g2_grouped.png
DISPLAY=$D xdotool mousemove 560 60 click 3 sleep 0.3 mousemove 320 200; sleep 2; $X shot $O/g3_deselected.png
DISPLAY=$D xdotool key 1; sleep 2; $X shot $O/g4_recalled.png
$X stop; sleep 2
