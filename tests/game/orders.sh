#!/usr/bin/env bash
# usage: orders.sh OUTDIR - Human mission 1: the selected infantry ordered at a unit:
# Ctrl+click on the Recon (force attack: order_attack), then Alt+click on a soldier (guard)
cd "$(dirname "$(readlink -f "$0")")/../.."
export DOMINION_HOME=${DOMINION_HOME:-$PWD/work/dh2}
X=tools/xvfb_session.sh; O=$1; mkdir -p $O; D=${XVFB_DISPLAY:-:99}
$X start >/dev/null; sleep 15
for i in 1 2 3; do $X key Escape; sleep 3; done; sleep 6
$X click 320 75; sleep 3; $X click 320 136; sleep 4
$X click 440 151; sleep 1; $X click 320 322; sleep 6
for i in 1 2 3 4; do $X key Escape; sleep 6; done
$X click 510 425; sleep 4
DISPLAY=$D xdotool mousemove 150 100 sleep 0.2 mousedown 1 sleep 0.2 mousemove 300 200 sleep 0.2 mousemove 380 250 sleep 0.2 mouseup 1; sleep 1
DISPLAY=$D xdotool mousemove 350 70 sleep 0.5 keydown ctrl sleep 0.3 click 1 sleep 0.3 keyup ctrl; sleep 3; $X shot $O/o1_attack.png
DISPLAY=$D xdotool mousemove 600 300; sleep 6; $X shot $O/o2_attack_later.png
DISPLAY=$D xdotool mousemove 150 100 sleep 0.2 mousedown 1 sleep 0.2 mousemove 300 200 sleep 0.2 mousemove 380 250 sleep 0.2 mouseup 1; sleep 1
DISPLAY=$D xdotool mousemove 285 98 sleep 0.5 keydown alt sleep 0.3 click 1 sleep 0.3 keyup alt; sleep 1; $X shot $O/o3_guard.png
DISPLAY=$D xdotool mousemove 600 300; sleep 6; $X shot $O/o4_guard_later.png
$X stop; sleep 2
