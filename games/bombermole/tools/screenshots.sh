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
# objectives: level banner, HUD, pause screen (map + one-line legend), "molehill open" banner and arrow,
# the glow of blocks hiding a grub
run --frames 110 --opt level=spring-2 --shot 100:$O/level-banner.png
run --frames 160 --opt level=spring-6 --opt nointro=1 --shot 150:$O/hud-in-play.png
printf "30 tap START
" > /tmp/bm_pause.input
run --frames 60 --opt level=spring-2 --opt nointro=1 --input /tmp/bm_pause.input --shot 50:$O/pause-map-and-legend.png
run --frames 60 --opt level=spring-1 --opt nointro=1 --opt opengrubs=1 --shot 50:$O/molehill-open-surface.png
run --frames 60 --opt level=spring-1 --opt nointro=1 --opt opengrubs=1 --opt spawn=1,9,3 --shot 50:$O/molehill-open-below.png
python3 -c "from PIL import Image; im = Image.open(\"$O/hud-in-play.png\"); im.crop((0, 0, 640, 32)).resize((1280, 64), Image.NEAREST).save(\"$O/hud-band-2x.png\")"
# objects over the ground, bridges both ways, remote pickup, lever link, windmill lane (test arenas in tests/data)
TD="$(dirname "$0")/../tests/data"
run --frames 180 --data $TD --opt level=spring-11 --opt nointro=1 --shot 170:$O/bridges-and-objects-spring.png
run --frames 180 --data $TD --opt level=winter-9 --opt nointro=1 --shot 170:$O/objects-over-snow-winter.png
printf "160 RIGHT
190 -
" > /tmp/bm_remote.input
run --frames 210 --data $TD --opt level=spring-11 --opt nointro=1 --input /tmp/bm_remote.input --shot 200:$O/remote-pickup-banner.png
printf "160 tap RIGHT
" > /tmp/bm_lever.input
run --frames 180 --data $TD --opt level=spring-11 --opt nointro=1 --opt spawn=0,5,6 --opt god=1 --input /tmp/bm_lever.input --shot 177:$O/lever-linked-gate-flash.png
run --frames 290 --opt level=spring-7 --opt nointro=1 --shot 280:$O/windmill-lanes.png
# summer: the levels 2-7 and the mechanics (test arenas in tests/data)
for n in 2 3 4 5 6 7; do run --frames 180 --opt level=summer-$n --opt nointro=1 --shot 170:$O/summer-$n.png; done
printf "5 tap B\n8 LEFT\n40 -\n" > /tmp/bm_s.input
run --frames 200 --data $TD --opt level=summer-9 --opt nointro=1 --opt spawn=0,8,2 --opt god=1 --input /tmp/bm_s.input --shot 185:$O/summer-corn-fire.png
run --frames 200 --data $TD --opt level=summer-10 --opt nointro=1 --opt spawn=0,8,3 --opt god=1 --input /tmp/bm_s.input --shot 185:$O/summer-bees.png
run --frames 300 --data $TD --opt level=summer-11 --opt nointro=1 --shot 296:$O/summer-harvester-warning.png
run --frames 350 --data $TD --opt level=summer-11 --opt nointro=1 --shot 345:$O/summer-harvester-sweep.png
run --frames 290 --data $TD --opt level=summer-12 --opt nointro=1 --opt spawn=0,2,4 --opt god=1 --shot 262:$O/summer-badger-charge.png
run --frames 200 --data $TD --opt level=summer-13 --opt nointro=1 --opt spawn=0,6,6 --opt god=1 --input /tmp/bm_s.input --shot 170:$O/summer-gas.png
printf "180 tap B\n183 LEFT\n200 -\n" > /tmp/bm_b.input
run --frames 460 --data $TD --opt level=summer-15 --opt nointro=1 --opt spawn=0,10,6 --opt god=1 --input /tmp/bm_b.input --shot 450:$O/summer-heavy-scene.png
python3 "$(dirname "$0")/glow_shot.py" "$H" "$O/hidden-grub-glow.png" spring-3
ls "$O"
