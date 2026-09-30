# Blueberry Tumble (games/blueberrytumble): its make targets, included by the root Makefile.
#
#   make blueberrytumble                host builds: blueberrytumble (SDL2), blueberrytumble_headless, blueberrytumble_libretro.so
#   make blueberrytumble-check          all its tests (SDK, libretro loader, physics, the validator: patterns, links, streams,
#                                       beat sync, difficulty; smoke, bot, determinism, UI screens, save states, audit, music)
#   make blueberrytumble-dist           dist/windows/BlueberryTumble.exe, dist/libretro/blueberrytumble_libretro.so (+ .armhf.so)
#   make blueberrytumble-windows        only the Windows exe
#   make blueberrytumble-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make blueberrytumble-screenshots    games/blueberrytumble/docs/screenshots/
#   make blueberrytumble-tables         re-measure every pattern and rewrite src/tables.inc (after a pattern change)
#   make blueberrytumble-difficulty     the difficulty table, the bot's failure rates, docs/difficulty.png
#   make blueberrytumble-bench          per-frame cost of the heaviest scene
#   make blueberrytumble-art            redraw the code-drawn art (tools/make_art.py; commit art/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/blueberrytumble/LICENSE; the rules below follow the root Makefile.

BT_MAKE = $(MAKE) --no-print-directory GAME=blueberrytumble GAME_NAME=BlueberryTumble
BT_DIR  = games/blueberrytumble
# the house style kit (games/common/src, docs/art-direction.md) is compiled in
HOUSE_UI_blueberrytumble = 1

.PHONY: blueberrytumble blueberrytumble-check blueberrytumble-windows blueberrytumble-armhf blueberrytumble-dist \
        blueberrytumble-screenshots blueberrytumble-art blueberrytumble-tables blueberrytumble-difficulty blueberrytumble-bench

blueberrytumble:
	+$(BT_MAKE) host
blueberrytumble-windows:
	+$(BT_MAKE) windows
blueberrytumble-armhf:
	+$(BT_MAKE) armhf
blueberrytumble-dist:
	+$(BT_MAKE) dist

# the rules alone (no video, no sound): the physics tests and the validator link them
BT_RULES = $(patsubst %.c,build/host/%.o,$(BT_DIR)/src/course.c $(BT_DIR)/src/physics.c $(BT_DIR)/src/patterns.c \
           $(BT_DIR)/src/director.c)
build/host/blueberrytumble_test_physics: build/host/$(BT_DIR)/tests/test_physics.o $(BT_RULES) build/host/librs.a
	$(HOST_CC) -o $@ $^ -lm
build/host/blueberrytumble_validate: build/host/$(BT_DIR)/tests/validate.o build/host/$(BT_DIR)/tests/solver.o \
                                     $(BT_RULES) build/host/librs.a
	$(HOST_CC) -o $@ $^ -lm

BT_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(BT_DIR)/src/*.c) $(HOUSE_SRC))

blueberrytumble-check:
	+$(BT_MAKE) host build/host/test_sdk build/host/test_libretro build/host/blueberrytumble_test_states \
	    build/host/blueberrytumble_test_physics build/host/blueberrytumble_validate
	./build/host/test_sdk --golden sdk/tests/golden --out build
	./build/host/test_libretro build/host/blueberrytumble_libretro.so 600
	./build/host/blueberrytumble_test_physics
	./build/host/blueberrytumble_validate --check --report build/blueberrytumble-validate.txt
	$(PYTHON) $(BT_DIR)/tests/test_music.py build/gen/blueberrytumble/music
	sh $(BT_DIR)/tests/smoke_test.sh build/host/blueberrytumble_headless build
	sh $(BT_DIR)/tests/ui_test.sh build/host/blueberrytumble_headless build/blueberrytumble-ui
	sh $(BT_DIR)/tests/state_test.sh build/host/blueberrytumble_test_states build/states
	$(PYTHON) tools/state_audit.py --game blueberrytumble build/host/blueberrytumble_test_states $(BT_OBJ_HOST)

blueberrytumble-tables:
	+$(BT_MAKE) build/host/blueberrytumble_validate
	./build/host/blueberrytumble_validate --write-tables $(BT_DIR)/src/tables.inc
	+$(BT_MAKE) build/host/blueberrytumble_validate
	./build/host/blueberrytumble_validate --check-tables

blueberrytumble-difficulty:
	+$(BT_MAKE) build/host/blueberrytumble_validate build/host/blueberrytumble_headless
	./build/host/blueberrytumble_validate --difficulty --csv build/blueberrytumble-difficulty.csv
	$(PYTHON) $(BT_DIR)/tools/difficulty.py build/blueberrytumble-difficulty.csv build/host/blueberrytumble_headless \
	    $(BT_DIR)/docs/difficulty.png

blueberrytumble-screenshots:
	+$(BT_MAKE) build/host/blueberrytumble_headless
	sh $(BT_DIR)/tools/screenshots.sh build/host/blueberrytumble_headless $(BT_DIR)/docs/screenshots

blueberrytumble-bench:
	+$(BT_MAKE) build/host/blueberrytumble_headless
	sh $(BT_DIR)/tools/bench.sh build/host/blueberrytumble_headless

blueberrytumble-art:
	$(PYTHON) $(BT_DIR)/tools/make_art.py
