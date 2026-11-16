#!/usr/bin/env bash
# usage: chat.sh OUTDIR - two-player match, the host sends two chat lines
cd "$(dirname "$(readlink -f "$0")")/../.."
export DOMINION_HOME=$PWD/work/dh2 DOMINION_HOME2=$PWD/work/dh3; O=$1; mkdir -p $O
tools/mp_test.sh start >/dev/null 2>&1; sleep 25
DISPLAY=:99 xdotool key Return; sleep 1; DISPLAY=:99 xdotool type --delay 100 "hello there"; DISPLAY=:99 xdotool key Return; sleep 2
DISPLAY=:99 xdotool key Return; sleep 1; DISPLAY=:99 xdotool type --delay 100 "second line"; DISPLAY=:99 xdotool key Return; sleep 2
DISPLAY=:99 magick import -window root $O/c1_host.png; DISPLAY=:98 magick import -window root $O/c1_join.png
sleep 15
DISPLAY=:99 magick import -window root $O/c2_host.png; DISPLAY=:98 magick import -window root $O/c2_join.png
tools/mp_test.sh stop >/dev/null 2>&1; sleep 2
