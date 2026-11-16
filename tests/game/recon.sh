#!/usr/bin/env bash
# usage: recon.sh OUTDIR - Human mission 1: double click on the Recon unit (it sets off to a
# sector still in fog; the original picks it with a wall-clock seed, so runs differ)
cd "$(dirname "$(readlink -f "$0")")/../.."
export DOMINION_HOME=${DOMINION_HOME:-$PWD/work/dh2}
X=tools/xvfb_session.sh; O=$1; mkdir -p $O; D=${XVFB_DISPLAY:-:99}
$X start >/dev/null; sleep 15
for i in 1 2 3; do $X key Escape; sleep 3; done; sleep 6
$X click 320 75; sleep 3; $X click 320 136; sleep 4
$X click 440 151; sleep 1; $X click 320 322; sleep 6
for i in 1 2 3 4; do $X key Escape; sleep 6; done
$X click 510 425; sleep 4
$X shot $O/r0_before.png
DISPLAY=$D xdotool mousemove 352 68 sleep 0.3 click --repeat 2 --delay 120 1; sleep 12; $X shot $O/r1_after.png
$X stop; sleep 2
