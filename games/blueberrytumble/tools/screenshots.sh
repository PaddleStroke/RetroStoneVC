#!/bin/sh
# Blueberry Tumble: the screenshots of games/blueberrytumble/docs/screenshots (make blueberrytumble-screenshots): the
# title, each biome and the night, a mushroom pad, a dew drop, the leaf glider, the snowberry, a splat, a biome gate,
# game over with a medal, pause, get ready and a race with 2 players (tests/ui_test.sh makes and checks them).
#   screenshots.sh <headless binary> <out dir>
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/blueberrytumble/LICENSE.
exec sh "$(dirname "$0")/../tests/ui_test.sh" "$1" "${2:-games/blueberrytumble/docs/screenshots}" 2
