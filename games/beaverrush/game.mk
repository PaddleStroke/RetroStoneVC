# Beaver Rush (games/beaverrush): its make targets, included by the root Makefile.
#
#   make beaverrush                host builds: beaverrush (SDL2), beaverrush_headless, beaverrush_libretro.so
#   make beaverrush-check          all its tests (libretro loader, rules and fairness, smoke + bot + determinism,
#                                  the UI screens, save states and their audit)
#   make beaverrush-dist           dist/windows/BeaverRush.exe (+ README-BeaverRush.txt),
#                                  dist/libretro/beaverrush_libretro.so (+ .armhf.so)
#   make beaverrush-windows        only the Windows exe
#   make beaverrush-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make beaverrush-screenshots    games/beaverrush/docs/screenshots/
#   make beaverrush-bench          per-frame cost of the heaviest scene
#   make beaverrush-art            the code-drawn sheets as PNG for review (build/beaverrush-art/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/beaverrush/LICENSE; the rules below follow the root Makefile.

BR_MAKE = $(MAKE) --no-print-directory GAME=beaverrush GAME_NAME=BeaverRush
BR_DIR  = games/beaverrush
# the house style kit (games/common/src, docs/art-direction.md) is compiled in
HOUSE_UI_beaverrush = 1

.PHONY: beaverrush beaverrush-check beaverrush-windows beaverrush-armhf beaverrush-dist beaverrush-screenshots \
        beaverrush-bench beaverrush-art

beaverrush:
	+$(BR_MAKE) host
beaverrush-windows:
	+$(BR_MAKE) windows
beaverrush-armhf:
	+$(BR_MAKE) armhf
beaverrush-dist:
	+$(BR_MAKE) dist

# the rules tests link the game rules (rules.c, world.c) alone
build/host/beaverrush_test_rules: build/host/$(BR_DIR)/tests/test_rules.o build/host/$(BR_DIR)/src/rules.o \
                                  build/host/$(BR_DIR)/src/world.o build/host/librs.a
	$(HOST_CC) -o $@ $^ -lm

BR_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(BR_DIR)/src/*.c) $(HOUSE_SRC))

beaverrush-check:
	+$(BR_MAKE) host build/host/test_libretro build/host/beaverrush_test_rules build/host/beaverrush_test_states
	./build/host/test_libretro build/host/beaverrush_libretro.so 600
	./build/host/beaverrush_test_rules
	sh $(BR_DIR)/tests/smoke_test.sh build/host/beaverrush_headless build
	sh $(BR_DIR)/tests/ui_test.sh build/host/beaverrush_headless build/beaverrush-ui
	sh $(BR_DIR)/tests/state_test.sh build/host/beaverrush_test_states build/states
	$(PYTHON) tools/state_audit.py --game beaverrush build/host/beaverrush_test_states $(BR_OBJ_HOST)

beaverrush-screenshots:
	+$(BR_MAKE) build/host/beaverrush_headless
	sh $(BR_DIR)/tools/screenshots.sh build/host/beaverrush_headless $(BR_DIR)/docs/screenshots

beaverrush-bench:
	+$(BR_MAKE) build/host/beaverrush_headless
	sh $(BR_DIR)/tools/bench.sh build/host/beaverrush_headless

beaverrush-art:
	$(PYTHON) $(BR_DIR)/tools/make_art.py --out build/beaverrush-art
