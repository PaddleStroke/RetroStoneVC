#!/bin/sh
# Headless screenshots for docs/screenshots (2x, like the 640x480 LCD).
#   screenshots.sh <headless binary> <out dir>
H=$1
O=${2:-docs/screenshots}
mkdir -p "$O"
run() { $H --scale 2 "$@" > /dev/null; }
run --frames 130 --shot 120:$O/title.png
run --frames 200 --opt level=spring-6 --opt nointro=1 --shot 150:$O/spring6-surface.png
run --frames 200 --opt level=spring-6 --opt nointro=1 --opt view=1 --shot 150:$O/spring6-underground1.png
run --frames 200 --opt level=spring-6 --opt nointro=1 --opt view=2 --shot 150:$O/spring6-underground2.png
run --frames 80 --opt level=spring-1 --opt nointro=1 --opt transition=1 --shot 46:$O/depth-transition.png
run --frames 200 --opt level=spring-5 --opt nointro=1 --opt scene=chain --shot 112:$O/explosion-chain.png
run --frames 200 --opt level=winter-1 --opt nointro=1 --shot 150:$O/winter1-frozen-river.png
run --frames 90 --opt level=spring-1 --shot 40:$O/level-intro-iris.png
run --frames 200 --opt level=spring-7 --opt nointro=1 --shot 160:$O/spring7-windmills.png
run --frames 200 --opt level=spring-8 --opt nointro=1 --shot 150:$O/spring8-boss.png
run --frames 260 --opt level=summer-8 --opt nointro=1 --shot 250:$O/summer-farmer-boss.png
run --frames 200 --opt level=autumn-1 --opt nointro=1 --shot 150:$O/autumn1-gale.png
run --frames 230 --opt level=summer-1 --opt nointro=1 --opt view=2 --shot 205:$O/summer1-steam-vents.png
# objectives: intro card, HUD, pause screen (map + HUD legend), "molehill open" banner and arrow
run --frames 70 --opt level=spring-2 --shot 60:$O/intro-card.png
run --frames 160 --opt level=spring-6 --opt nointro=1 --shot 150:$O/hud-in-play.png
printf "30 tap START
" > /tmp/bm_pause.input
run --frames 60 --opt level=spring-2 --opt nointro=1 --input /tmp/bm_pause.input --shot 50:$O/pause-map-and-legend.png
run --frames 60 --opt level=spring-1 --opt nointro=1 --opt opengrubs=1 --shot 50:$O/molehill-open-surface.png
run --frames 60 --opt level=spring-1 --opt nointro=1 --opt opengrubs=1 --opt spawn=1,9,3 --shot 50:$O/molehill-open-below.png
python3 -c "from PIL import Image; im = Image.open(\"$O/hud-in-play.png\"); im.crop((0, 0, 640, 32)).resize((1280, 64), Image.NEAREST).save(\"$O/hud-band-2x.png\")"
ls "$O"
