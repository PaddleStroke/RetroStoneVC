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
run --frames 110 --opt level=spring-3 --shot 100:$O/level-start-box.png
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
# the reworked surfaces, corn over the characters, the farmer when he is vulnerable, the crocodile's tell
run --frames 20 --opt level=spring-3 --opt nointro=1 --shot 10:$O/spring3-surface.png
run --frames 20 --opt level=summer-1 --opt nointro=1 --shot 10:$O/summer1-surface.png
run --frames 20 --opt level=summer-3 --opt nointro=1 --shot 10:$O/summer3-surface.png
run --frames 20 --opt level=summer-6 --opt nointro=1 --shot 10:$O/summer6-surface.png
run --frames 12 --data $TD --opt level=summer-9 --opt nointro=1 --opt spawn=0,10,2 --opt god=1 --shot 8:$O/corn-overlay-mole-hidden.png
run --frames 70 --opt level=summer-8 --opt nointro=1 --opt nocrates=1 --opt god=1 --shot 63:$O/farmer-vulnerable-boss-bar.png
run --frames 30 --data $TD --opt level=summer-16 --opt nointro=1 --opt spawn=0,9,4 --opt god=1 --shot 20:$O/croc-telegraph.png
python3 "$(dirname "$0")/glow_shot.py" "$H" "$O/hidden-grub-glow.png" spring-3
# autumn: the levels 2-8 and the mechanics (test arenas in tests/data)
for n in 2 3 4 5 6 7 8; do run --frames 60 --opt level=autumn-$n --opt nointro=1 --shot 50:$O/autumn-$n.png; done
printf "5 RIGHT\n150 -\n" > /tmp/bm_a.input
run --frames 170 --data $TD --opt level=autumn-9 --opt nointro=1 --opt spawn=0,3,2 --input /tmp/bm_a.input --shot 165:$O/autumn-pumpkin-plug.png
printf "5 tap B\n8 LEFT\n40 -\n" > /tmp/bm_a.input
run --frames 200 --data $TD --opt level=autumn-9 --opt nointro=1 --opt spawn=0,2,6 --opt god=1 --input /tmp/bm_a.input --shot 190:$O/autumn-pumpkin-mush.png
run --frames 200 --data $TD --opt level=autumn-10 --opt nointro=1 --opt spawn=0,4,3 --opt god=1 --input /tmp/bm_a.input --shot 175:$O/autumn-apples.png
printf "5 tap RIGHT\n" > /tmp/bm_a.input
run --frames 30 --data $TD --opt level=autumn-11 --opt nointro=1 --opt spawn=0,4,3 --input /tmp/bm_a.input --shot 22:$O/autumn-mushroom-hop.png
run --frames 20 --data $TD --opt level=autumn-12 --opt nointro=1 --shot 10:$O/autumn-fog-eyes.png
printf "5 tap UP\n" > /tmp/bm_a.input
run --frames 60 --data $TD --opt level=autumn-13 --opt nointro=1 --opt spawn=0,1,3 --input /tmp/bm_a.input --shot 30:$O/autumn-mine-cart.png
run --frames 120 --data $TD --opt level=autumn-14 --opt nointro=1 --opt spawn=0,1,8 --shot 100:$O/autumn-ants.png
run --frames 60 --data $TD --opt level=autumn-16 --opt nointro=1 --shot 45:$O/autumn-leaves-gust.png
run --frames 80 --data $TD --opt level=autumn-15 --opt nointro=1 --opt god=1 --opt foxrest=1 --shot 60:$O/autumn-fox-resting.png
printf "5 tap RIGHT\n" > /tmp/bm_a.input
run --frames 200 --data $TD --opt level=autumn-17 --opt nointro=1 --opt spawn=0,1,9 --opt god=1 --input /tmp/bm_a.input --shot 150:$O/autumn-heavy-scene.png
ls "$O"
