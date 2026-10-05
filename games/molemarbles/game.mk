# Mole Marbles (games/molemarbles): its make targets, included by the root Makefile.
#
#   make molemarbles                host builds: molemarbles (SDL2), molemarbles_headless, molemarbles_libretro.so
#   make molemarbles-check          all its tests (libretro loader, smoke + bot + determinism, UI screens, save states, audit)
#   make molemarbles-dist           dist/windows/MoleMarbles.exe, dist/libretro/molemarbles_libretro.so (+ .armhf.so)
#   make molemarbles-windows        only the Windows exe
#   make molemarbles-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make molemarbles-screenshots    games/molemarbles/docs/screenshots/
#   make molemarbles-art            redraw the code-drawn art (tools/make_art.py; commit art/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/molemarbles/LICENSE; the rules below follow the root Makefile.

MOLEMARBLES_MAKE = $(MAKE) --no-print-directory GAME=molemarbles GAME_NAME=MoleMarbles
MOLEMARBLES_DIR  = games/molemarbles
# the house style kit (games/common/src, docs/art-direction.md) is compiled in
HOUSE_UI_molemarbles = 1

.PHONY: molemarbles molemarbles-check molemarbles-windows molemarbles-armhf molemarbles-dist molemarbles-screenshots molemarbles-art

molemarbles:
	+$(MOLEMARBLES_MAKE) host
molemarbles-windows:
	+$(MOLEMARBLES_MAKE) windows
molemarbles-armhf:
	+$(MOLEMARBLES_MAKE) armhf
molemarbles-dist:
	+$(MOLEMARBLES_MAKE) dist

MOLEMARBLES_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(MOLEMARBLES_DIR)/src/*.c) $(HOUSE_SRC))

molemarbles-check:
	+$(MOLEMARBLES_MAKE) host build/host/test_libretro build/host/molemarbles_test_states
	./build/host/test_libretro build/host/molemarbles_libretro.so 600
	sh $(MOLEMARBLES_DIR)/tests/smoke_test.sh build/host/molemarbles_headless build
	sh $(MOLEMARBLES_DIR)/tests/ui_test.sh build/host/molemarbles_headless build/molemarbles-ui
	sh $(MOLEMARBLES_DIR)/tests/state_test.sh build/host/molemarbles_test_states build/states
	$(PYTHON) tools/state_audit.py --game molemarbles build/host/molemarbles_test_states $(MOLEMARBLES_OBJ_HOST)

molemarbles-screenshots:
	+$(MOLEMARBLES_MAKE) build/host/molemarbles_headless
	sh $(MOLEMARBLES_DIR)/tests/ui_test.sh build/host/molemarbles_headless $(MOLEMARBLES_DIR)/docs/screenshots 2

molemarbles-art:
	$(PYTHON) $(MOLEMARBLES_DIR)/tools/make_art.py

build/host/molemarbles_physics: games/molemarbles/tests/test_physics.c games/molemarbles/src/play.c games/molemarbles/src/game.h
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -Igames/molemarbles/src $< games/molemarbles/src/play.c -o $@
molemarbles-physics: build/host/molemarbles_physics
	./build/host/molemarbles_physics
