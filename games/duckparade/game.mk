# Duck Parade (games/duckparade): its make targets, included by the root Makefile.
#
#   make duckparade                host builds: duckparade (SDL2), duckparade_headless, duckparade_libretro.so
#   make duckparade-check          all its tests: the rules and the generator's fairness, the libretro loader, smoke +
#                                  the bot (10 seeds) + determinism, the UI screens, save states and their audit
#   make duckparade-dist           dist/windows/DuckParade.exe + README-DuckParade.txt, dist/libretro/duckparade_libretro.so
#                                  (+ .armhf.so)
#   make duckparade-windows        only the Windows exe
#   make duckparade-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make duckparade-screenshots    games/duckparade/docs/screenshots/
#   make duckparade-bench          per-frame cost of the heaviest scene
#   make duckparade-art            redraw the code-drawn art (tools/make_art.py; commit art/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/duckparade/LICENSE; the rules below follow the root Makefile.

DP_MAKE = $(MAKE) --no-print-directory GAME=duckparade GAME_NAME=DuckParade
DP_DIR  = games/duckparade
# the house style kit (games/common/src, docs/art-direction.md) is compiled in
HOUSE_UI_duckparade = 1

.PHONY: duckparade duckparade-check duckparade-windows duckparade-armhf duckparade-dist duckparade-screenshots \
        duckparade-bench duckparade-art duckparade-rules

duckparade:
	+$(DP_MAKE) host
duckparade-windows:
	+$(DP_MAKE) windows
duckparade-armhf:
	+$(DP_MAKE) armhf
duckparade-dist:
	+$(DP_MAKE) dist

# the rules' tests link the game rules (lanes.c, world.c) alone
build/host/duckparade_test_rules: build/host/$(DP_DIR)/tests/test_rules.o build/host/$(DP_DIR)/src/lanes.o \
                                  build/host/$(DP_DIR)/src/world.o build/host/librs.a
	$(HOST_CC) -o $@ $^ -lm

duckparade-rules:
	+$(DP_MAKE) build/host/duckparade_test_rules
	./build/host/duckparade_test_rules

# the UI test runs the whole game (runtime, draw, the kit) with scripted input and checks the picture
DP_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(DP_DIR)/src/*.c) $(HOUSE_SRC) build/gen/duckparade/assets.c)
build/host/$(DP_DIR)/tests/test_ui.o: build/gen/duckparade/assets.c
build/host/duckparade_test_ui: build/host/$(DP_DIR)/tests/test_ui.o $(DP_OBJ_HOST) \
                               build/host/sdk/frontends/common/rs_desktop.o build/host/librs.a
	$(HOST_CC) -o $@ $^ -lm

duckparade-check:
	+$(DP_MAKE) host build/host/test_libretro build/host/duckparade_test_rules build/host/duckparade_test_ui \
	    build/host/duckparade_test_states
	./build/host/duckparade_test_rules
	./build/host/test_libretro build/host/duckparade_libretro.so 600
	sh $(DP_DIR)/tests/smoke_test.sh build/host/duckparade_headless build
	sh $(DP_DIR)/tests/ui_test.sh build/host/duckparade_test_ui build/duckparade-ui
	sh $(DP_DIR)/tests/state_test.sh build/host/duckparade_test_states build/states
	$(PYTHON) tools/state_audit.py --game duckparade build/host/duckparade_test_states $(filter-out %/assets.o,$(DP_OBJ_HOST))

duckparade-screenshots:
	+$(DP_MAKE) build/host/duckparade_headless
	sh $(DP_DIR)/tools/screenshots.sh build/host/duckparade_headless $(DP_DIR)/docs/screenshots

duckparade-bench:
	+$(DP_MAKE) build/host/duckparade_headless
	sh $(DP_DIR)/tools/bench.sh build/host/duckparade_headless

duckparade-art:
	$(PYTHON) $(DP_DIR)/tools/make_art.py
