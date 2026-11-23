# Interface script for scenario.sh: a base session on a 640x480 screen. Uses $X, $O, $D.
shot() { $X shot "$O/$1.png"; }
sleep 5
# buildings tab, first item, wait for it, then place it on open ground
$X click 132 385; sleep 2; $X click 165 425; sleep 45; shot built
$X click 165 425; sleep 2; DISPLAY=$D xdotool mousemove 300 280; sleep 2; shot placing
$X click 300 280; sleep 25; shot placed
# personnel and vehicles: start one of each
$X click 188 385; sleep 2; $X click 165 425; sleep 3
$X click 160 385; sleep 2; $X click 165 425; sleep 3
$X click 216 385; sleep 2; $X click 165 425; sleep 20; shot trained
# select a group by dragging, send it across the map
DISPLAY=$D xdotool mousemove 60 40 mousedown 1; sleep 0.5
DISPLAY=$D xdotool mousemove 260 230; sleep 0.5; DISPLAY=$D xdotool mouseup 1; sleep 1
$X click 420 200; sleep 2; shot move1; sleep 3; shot move2; sleep 3; shot moved
# repair, upgrade, then sell the new structure (each button is a mode, a second click leaves it)
$X click 338 385; sleep 1; shot repair_mode; $X click 300 270; sleep 6; $X click 338 385; sleep 1; shot repair
$X click 366 385; sleep 1; shot upgrade_mode; $X click 300 270; sleep 6; $X click 366 385; sleep 1; shot upgrade
$X click 308 385; sleep 1; shot sell_mode; $X click 300 270; sleep 12; shot sold
