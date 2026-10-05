#!/bin/sh
set -eu
python3 games/raccoonrecycler/tests/check_game.py "$1" "${2:-build}"
