# Pancake Tower (games/pancaketower): its make targets, included by the root Makefile.
#
#   make pancaketower                host builds: pancaketower (SDL2), pancaketower_headless, pancaketower_libretro.so
#   make pancaketower-check          all its tests (the rules, the breakthroughs frame by frame, libretro loader, smoke +
#                                    bot + determinism, UI screens, save states and their audit)
#   make pancaketower-dist           dist/windows/PancakeTower.exe, dist/libretro/pancaketower_libretro.so (+ .armhf.so)
#   make pancaketower-windows        only the Windows exe
#   make pancaketower-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make pancaketower-screenshots    games/pancaketower/docs/screenshots/
#   make pancaketower-bench          per-frame cost of the heaviest scenes
#   make pancaketower-art            redraw the review sheets of the code-drawn art (tools/make_art.py -> art/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pancaketower/LICENSE; the rules below follow the root Makefile.

PT_MAKE = $(MAKE) --no-print-directory GAME=pancaketower GAME_NAME=PancakeTower
PT_DIR  = games/pancaketower
# the house style kit (games/common/src, docs/art-direction.md) is compiled in
HOUSE_UI_pancaketower = 1

.PHONY: pancaketower pancaketower-check pancaketower-windows pancaketower-armhf pancaketower-dist \
        pancaketower-screenshots pancaketower-bench pancaketower-art

pancaketower:
	+$(PT_MAKE) host
pancaketower-windows:
	+$(PT_MAKE) windows
pancaketower-armhf:
	+$(PT_MAKE) armhf
pancaketower-dist:
	+$(PT_MAKE) dist

# the rules' unit tests link tower.c alone
build/host/pancaketower_test_tower: build/host/$(PT_DIR)/tests/test_tower.o build/host/$(PT_DIR)/src/tower.o
	$(HOST_CC) -o $@ $^ -lm

PT_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(PT_DIR)/src/*.c) $(HOUSE_SRC) build/gen/pancaketower/assets.c)
# the breakthrough test runs the whole game (runtime, draw, the bot) and checks the picture against the world
build/host/$(PT_DIR)/tests/test_break.o: build/gen/pancaketower/assets.c
build/host/pancaketower_test_break: build/host/$(PT_DIR)/tests/test_break.o $(PT_OBJ_HOST) build/host/librs.a
	$(HOST_CC) -o $@ $^ -lm

pancaketower-check:
	+$(PT_MAKE) host build/host/test_libretro build/host/pancaketower_test_states build/host/pancaketower_test_tower 	    build/host/pancaketower_test_break
	./build/host/pancaketower_test_tower
	sh $(PT_DIR)/tests/break_test.sh build/host/pancaketower_test_break build
	./build/host/test_libretro build/host/pancaketower_libretro.so 600
	sh $(PT_DIR)/tests/smoke_test.sh build/host/pancaketower_headless build
	sh $(PT_DIR)/tests/bot_test.sh build/host/pancaketower_headless build
	sh $(PT_DIR)/tests/ui_test.sh build/host/pancaketower_headless build/pancaketower-ui
	sh $(PT_DIR)/tests/state_test.sh build/host/pancaketower_test_states build/states
	$(PYTHON) tools/state_audit.py --game pancaketower build/host/pancaketower_test_states $(filter-out %/assets.o,$(PT_OBJ_HOST))

pancaketower-screenshots:
	+$(PT_MAKE) build/host/pancaketower_headless
	sh $(PT_DIR)/tools/screenshots.sh build/host/pancaketower_headless $(PT_DIR)/docs/screenshots

pancaketower-bench:
	+$(PT_MAKE) build/host/pancaketower_headless
	sh $(PT_DIR)/tools/bench.sh build/host/pancaketower_headless

pancaketower-art:
	$(PYTHON) $(PT_DIR)/tools/make_art.py
