#!/usr/bin/env bash
# usage: shots.sh OUTDIR  - capture a fixed tour of menu screens
set -e
cd "$(dirname "$(readlink -f "$0")")/../.."
export DOMINION_HOME=${DOMINION_HOME:-$PWD/work/dh2}
X=tools/xvfb_session.sh; O=$1; mkdir -p $O
$X start >/dev/null; sleep 20
for i in 1 2 3 4; do $X key Escape; sleep 3; done; sleep 6
$X shot $O/1_menu.png
$X click 320 75; sleep 4; $X shot $O/2_single.png
$X click 320 197; sleep 5; $X shot $O/3_mission.png
$X click 320 445; sleep 3; $X click 320 445; sleep 3
$X click 320 136; sleep 4; $X shot $O/4_mp.png
$X click 260 171; sleep 3; $X shot $O/5_callsign.png
$X click 320 445; sleep 3
$X click 320 322; sleep 5; $X shot $O/6_cine.png
$X stop; sleep 2
