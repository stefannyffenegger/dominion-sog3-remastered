#!/usr/bin/env bash
# usage: statelog.sh LOGFILE [SECONDS] - Human mission 1 left running without input (input
# lands on a tick that depends on timing), logging the unit state hash every tick
# (DOMINION_ISO_STATELOG) into LOGFILE
cd "$(dirname "$(readlink -f "$0")")/../.."
export DOMINION_HOME=${DOMINION_HOME:-$PWD/work/dh2}
L=$(realpath -m "$1"); export DOMINION_ISO_STATELOG="Z:${L//\//\\}"
X=tools/xvfb_session.sh; D=${XVFB_DISPLAY:-:99}
$X start >/dev/null; sleep 15
for i in 1 2 3; do $X key Escape; sleep 3; done; sleep 6
$X click 320 75; sleep 3; $X click 320 136; sleep 4
$X click 440 151; sleep 1; $X click 320 322; sleep 6
for i in 1 2 3 4; do $X key Escape; sleep 6; done
$X click 510 425; sleep 4
DISPLAY=$D xdotool mousemove 320 200; sleep ${2:-60}
$X stop; sleep 2
