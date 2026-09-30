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
# each season's surface and an underground, the bosses, readability (corn over the moles, fog, night), multiplayer
run --frames 200 --opt level=summer-2 --opt nointro=1 --shot 150:$O/ingame-ai-summer-surface.png
run --frames 200 --opt level=summer-6 --opt nointro=1 --opt view=1 --shot 150:$O/ingame-ai-summer-underground.png
run --frames 200 --opt level=autumn-2 --opt nointro=1 --shot 150:$O/ingame-ai-autumn-surface.png
run --frames 200 --opt level=autumn-3 --opt nointro=1 --opt view=1 --shot 150:$O/ingame-ai-autumn-underground.png
run --frames 200 --opt level=winter-4 --opt nointro=1 --opt view=1 --shot 150:$O/ingame-ai-winter-underground.png
run --frames 420 --opt level=autumn-8 --opt nointro=1 --opt god=1 --shot 400:$O/ingame-ai-fox.png
run --frames 220 --opt level=winter-8 --opt nointro=1 --opt god=1 --opt owlperch=1 --shot 200:$O/ingame-ai-owl.png
run --frames 200 --opt level=summer-5 --opt nointro=1 --opt god=1 --shot 180:$O/ingame-ai-badger.png
run --frames 12 --data games/bombermole/tests/data --opt level=summer-9 --opt nointro=1 --opt spawn=0,10,2 --opt god=1 --shot 8:$O/ingame-ai-corn-over-mole.png
run --frames 200 --opt level=autumn-5 --opt nointro=1 --shot 150:$O/ingame-ai-fog.png
run --frames 200 --opt level=winter-5 --opt nointro=1 --shot 150:$O/ingame-ai-night.png
run --frames 700 --opt mp=battle --opt players=4 --opt cpus=4 --opt arena=molehill-maze --shot 600:$O/ingame-ai-battle-4p.png
printf "5 P1 RIGHT\n40 P1 DOWN\n60 P1 -\n5 P2 DOWN\n30 P2 -\n" > /tmp/bm_ai_coop.input
run --frames 90 --opt mp=coop --opt players=2 --opt level=spring-3 --opt nointro=1 --input /tmp/bm_ai_coop.input --shot 80:$O/ingame-ai-coop-split.png
ls "$O"/ingame-ai-*.png
