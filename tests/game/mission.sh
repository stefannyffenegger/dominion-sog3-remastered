#!/usr/bin/env bash
# usage: mission.sh OUTDIR - Human campaign mission 1 on the 640x480 test install
cd "$(dirname "$(readlink -f "$0")")/../.."
export DOMINION_HOME=${DOMINION_HOME:-$PWD/work/dh2}
X=tools/xvfb_session.sh; O=$1; mkdir -p $O
$X start >/dev/null; sleep 15
for i in 1 2 3; do $X key Escape; sleep 3; done; sleep 6        # menu
$X click 320 75; sleep 3; $X click 320 136; sleep 4              # single player, campaign
$X click 440 151; sleep 1; $X click 320 322; sleep 6             # human, start
$X shot $O/1_briefing.png
for i in 1 2 3 4; do $X key Escape; sleep 6; done
$X shot $O/2_paused.png
$X click 510 425; sleep 4                                        # resume
$X shot $O/3_mission.png
DISPLAY=${XVFB_DISPLAY:-:99} xdotool mousemove 150 100 sleep 0.2 mousedown 1 sleep 0.2 mousemove 300 200 sleep 0.2 mousemove 380 250 sleep 0.2 mouseup 1; sleep 2
$X click 500 300; sleep 8
$X shot $O/4_moved.png
DISPLAY=${XVFB_DISPLAY:-:99} xdotool mousemove 639 240 sleep 0.25 mousemove 320 240; sleep 3  # scroll right a little
$X shot $O/5_scrolled.png
DISPLAY=${XVFB_DISPLAY:-:99} xdotool mousemove 0 240 sleep 0.4 mousemove 320 240; sleep 3     # scroll left
$X shot $O/6_scrolled_left.png
$X stop; sleep 2
