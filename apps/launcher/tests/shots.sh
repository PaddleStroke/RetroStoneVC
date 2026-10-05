#!/bin/sh
# RetroStone Arcade: the launcher's screenshots for the docs.
#   shots.sh <app>_headless <dir>
# launcher-320.png (4:3), launcher-427.png (16:9), launcher-520.png (a 19.5:9 phone), launcher-scrolled.png (the
# grid dragged up), panel.png ("Get the full version"; panel-fr-427.png, panel-ja.png: in French at 16:9, in Japanese), played.png (after a game of Leady Squid played by its bot:
# the best-score badge), help.png, credits.png, licence.png (the corner buttons' dialogs), launcher-pad.png (with a
# gamepad: the A glyph and the selected game in the footer). The saves go to a scratch folder.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
set -e
B=$1
D=$2
T=${TMPDIR:-/tmp}/rsb-shots.$$
mkdir -p "$D" "$T"
"$B" --saves "$T" --frames 40 --shot 39:"$D/launcher-320.png" > /dev/null
"$B" --saves "$T" --width 427 --frames 40 --shot 39:"$D/launcher-427.png" > /dev/null
"$B" --saves "$T" --width 520 --frames 40 --shot 39:"$D/launcher-520.png" > /dev/null
cat > "$T/scroll.txt" <<EOF
10 touch 160 200
14 move 160 150
18 move 160 100
22 move 160 60
24 up
EOF
"$B" --saves "$T" --frames 120 --input "$T/scroll.txt" --shot 119:"$D/launcher-scrolled.png" > /dev/null
cat > "$T/panel.txt" <<EOF
10 tap-at 56 156
EOF
"$B" --saves "$T" --frames 60 --input "$T/panel.txt" --shot 59:"$D/panel.png" > /dev/null
"$B" --saves "$T" --lang fr --width 427 --frames 60 --input "$T/panel.txt" --shot 59:"$D/panel-fr-427.png" > /dev/null
"$B" --saves "$T" --lang ja --frames 60 --input "$T/panel.txt" --shot 59:"$D/panel-ja.png" > /dev/null
cat > "$T/played.txt" <<EOF
10 tap-at 56 68
1500 back
EOF
"$B" --saves "$T" --opt bot=1 --opt botstop=7 --frames 1540 --input "$T/played.txt" --shot 1539:"$D/played.png" > /dev/null
# the corner buttons' dialogs: help ("?"), credits ("i") and a licence; the menu with a gamepad (its hints)
printf '10 tap-at 308 14\n' > "$T/help.txt"
"$B" --saves "$T" --frames 40 --input "$T/help.txt" --shot 39:"$D/help.png" > /dev/null
printf '10 tap-at 12 14\n' > "$T/credits.txt"
"$B" --saves "$T" --frames 40 --input "$T/credits.txt" --shot 39:"$D/credits.png" > /dev/null
printf '10 tap-at 12 14\n40 tap-at 160 108\n' > "$T/licence.txt"
"$B" --saves "$T" --frames 80 --input "$T/licence.txt" --shot 79:"$D/licence.png" > /dev/null
"$B" --saves "$T" --gamepad --frames 40 --shot 39:"$D/launcher-pad.png" > /dev/null
rm -rf "$T"
ls -l "$D"
