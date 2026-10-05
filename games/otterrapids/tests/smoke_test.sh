#!/bin/sh
set -eu
python3 games/otterrapids/tests/check_game.py "$1" "${2:-build}"
