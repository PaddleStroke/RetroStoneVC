#!/bin/sh
# In-game screenshots of the AI art preview build (make preview): headless, 2x
# like docs/screenshots.
#   art_preview_shots.sh <headless binary built with the preview art> <out dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
set -e
H=$1
O=${2:-docs/art-preview}
mkdir -p "$O"
run() { $H --scale 2 "$@" > /dev/null; }
run --frames 130 --shot 120:$O/ingame-ai-title.png
run --frames 200 --opt level=spring-3 --opt nointro=1 --shot 150:$O/ingame-ai-spring-surface.png
run --frames 200 --opt level=spring-6 --opt nointro=1 --opt view=1 --shot 150:$O/ingame-ai-underground.png
run --frames 260 --opt level=summer-8 --opt nointro=1 --shot 250:$O/ingame-ai-farmer.png
run --frames 200 --opt level=winter-1 --opt nointro=1 --shot 150:$O/ingame-ai-winter.png
ls "$O"/ingame-ai-*.png
