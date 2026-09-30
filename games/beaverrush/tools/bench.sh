#!/bin/sh
# Beaver Rush: the per-frame cost of the heaviest scenes (host clock), with the music on.
#   bench.sh <headless binary>
# 1 player: a winter night (snow, the woodpecker, the dam half built, logs in flight and floating), the bot at
# full speed; versus: the same scene in two viewports (the PPU draws every layer twice, per half).
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/beaverrush/LICENSE.
H=$1
echo "1 player, winter night (skip=560, the bot):"
$H --frames 3000 --opt bot=1 --opt botruns=50 --opt ready=1 --opt seed=5 --opt skip=560 --opt music=1 --bench 300 2>/dev/null | tail -6
echo "versus, autumn night (skip=400, two bots):"
$H --frames 3000 --opt bot=2 --opt botruns=50 --opt players=2 --opt seed=5 --opt skip=400 --opt music=1 --bench 300 2>/dev/null | tail -6
