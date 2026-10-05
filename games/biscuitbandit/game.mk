# Biscuit Bandit (games/biscuitbandit): its make targets, included by the root Makefile.
#
#   make biscuitbandit                host builds: biscuitbandit (SDL2), biscuitbandit_headless, biscuitbandit_libretro.so
#   make biscuitbandit-check          all its tests (libretro loader, smoke + bot + determinism, UI screens, save states, audit)
#   make biscuitbandit-dist           dist/windows/BiscuitBandit.exe, dist/libretro/biscuitbandit_libretro.so (+ .armhf.so)
#   make biscuitbandit-windows        only the Windows exe
#   make biscuitbandit-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make biscuitbandit-screenshots    games/biscuitbandit/docs/screenshots/
#   make biscuitbandit-art            redraw the code-drawn art (tools/make_art.py; commit art/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/biscuitbandit/LICENSE; the rules below follow the root Makefile.

BISCUITBANDIT_MAKE = $(MAKE) --no-print-directory GAME=biscuitbandit GAME_NAME=BiscuitBandit
BISCUITBANDIT_DIR  = games/biscuitbandit
# the house style kit (games/common/src, docs/art-direction.md) is compiled in
HOUSE_UI_biscuitbandit = 1

.PHONY: biscuitbandit biscuitbandit-check biscuitbandit-windows biscuitbandit-armhf biscuitbandit-dist biscuitbandit-screenshots biscuitbandit-art

biscuitbandit:
	+$(BISCUITBANDIT_MAKE) host
biscuitbandit-windows:
	+$(BISCUITBANDIT_MAKE) windows
biscuitbandit-armhf:
	+$(BISCUITBANDIT_MAKE) armhf
biscuitbandit-dist:
	+$(BISCUITBANDIT_MAKE) dist

BISCUITBANDIT_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(BISCUITBANDIT_DIR)/src/*.c) $(HOUSE_SRC))

build/host/biscuitbandit_test_physics: games/biscuitbandit/src/play.c games/biscuitbandit/src/game.h games/biscuitbandit/tests/test_physics.c build/host/librs.a
	$(CC) -O2 -Wall -Wextra -Isdk/include -Igames/biscuitbandit/src games/biscuitbandit/src/play.c games/biscuitbandit/tests/test_physics.c build/host/librs.a -lm -o $@

biscuitbandit-check:
	+$(BISCUITBANDIT_MAKE) host build/host/test_libretro build/host/biscuitbandit_test_states build/host/biscuitbandit_test_physics
	./build/host/biscuitbandit_test_physics
	./build/host/test_libretro build/host/biscuitbandit_libretro.so 600
	sh $(BISCUITBANDIT_DIR)/tests/smoke_test.sh build/host/biscuitbandit_headless build
	sh $(BISCUITBANDIT_DIR)/tests/ui_test.sh build/host/biscuitbandit_headless build/biscuitbandit-ui
	sh $(BISCUITBANDIT_DIR)/tests/state_test.sh build/host/biscuitbandit_test_states build/states
	$(PYTHON) tools/state_audit.py --game biscuitbandit build/host/biscuitbandit_test_states $(BISCUITBANDIT_OBJ_HOST)

biscuitbandit-screenshots:
	+$(BISCUITBANDIT_MAKE) build/host/biscuitbandit_headless
	sh $(BISCUITBANDIT_DIR)/tests/ui_test.sh build/host/biscuitbandit_headless $(BISCUITBANDIT_DIR)/docs/screenshots 2

biscuitbandit-art:
	$(PYTHON) $(BISCUITBANDIT_DIR)/tools/make_art.py
