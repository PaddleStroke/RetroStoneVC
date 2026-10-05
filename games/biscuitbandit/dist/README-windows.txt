BISCUIT BANDIT - a RetroStone VC game for Windows
(c) 2026 Pierre-Louis Boyer (8BCraft). Code MIT, art/music/sound CC BY-NC-SA 4.0.
See LICENSE-biscuitbandit.txt.

Catch every cookie with your tray and avoid the cups. Each catch earns 10 points
immediately. Cookies accumulate without a plate capacity limit; there are no
baskets or deliveries. One missed or spilled cookie ends your run, as does a cup.

Left/right accelerates smoothly. Reversing tilts the tray; cookies retain inertia.
The moving conveyor carries a continuous stream. New items start about 1.3 seconds
apart and get closer together, reaching about 0.7 seconds apart. Watch the items
themselves: there are no landing guides. The first cookie is already on the belt
in the frozen title, and starting retains that same scene.
Drop locations move across both sides of the cafe. Consecutive distances shrink
as the pace increases, leaving time to catch each cookie and dodge the cups.

Run BiscuitBandit.exe. No installation needed (SDL2 is built in).

Controls                    Gamepad                 Keyboard
  move                      Left / Right            Left / Right arrows
  start                     A or Start              X or Enter
  pause / resume            Select or Start         Backspace, Esc or Enter
  back to title on results  A / Start / Select       X / Enter / Esc
  players 2-4 join           A on their pad on title (B leaves)
  player 2 keyboard         A/D move, H join, G leave, T start

Up to four full-size raccoons share one cafe. Each independently catches the
same drop; several aligned trays all get the cookie, with no player priority.
Raccoons do not collide. Eliminated players wait while the others continue.
F11 or Alt+Enter: fullscreen. F12: screenshot. Close the window to quit.
Options: BiscuitBandit.exe --scale 4 --fullscreen
Widescreen: the picture follows the shape of the window (drag its edges or go
fullscreen: a wider window shows more of the cafe). To fix it: --width 427
(16:9), --width 520 (phones), or --aspect 4:3 for the classic 320x240.
The checked best score is saved in biscuitbandit.srm next to the exe.
