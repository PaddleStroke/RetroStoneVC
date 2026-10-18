# Nutty Branch (games/nuttybranch): its make targets, included by the root Makefile.
#
#   make nuttybranch                host builds: nuttybranch (SDL2), nuttybranch_headless, nuttybranch_libretro.so
#   make nuttybranch-check          all its tests: libretro loader, physics + generator fairness + determinism
#                                   (tests/test_physics.c), smoke (flow, 1-4 players, determinism), the screen-only
#                                   bot, the UI screens (0-3 players joined), save states and their audit, the props'
#                                   sizes and anchors (lod_test.sh), the bark's furrows on screen (furrow_test.sh),
#                                   the touch controls (touch_test.sh), the frame cost; then again at each widescreen width of WIDTHS (427 520), and the
#                                   game across widths (tests/wide_test.sh) and on tall screens (tests/tall_test.sh)
#   make nuttybranch-dist           dist/windows/NuttyBranch.exe, dist/libretro/nuttybranch_libretro.so (+ .armhf.so)
#   make nuttybranch-windows        only the Windows exe
#   make nuttybranch-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make nuttybranch-screenshots    games/nuttybranch/docs/screenshots/
#   make nuttybranch-bench          the per-frame cost of busy scenes (1 and 4 players; at 320, then 520 columns)
#   make nuttybranch-bot            the bot's distance distribution over many seeds (BOT_RUNS=40)
#   make nuttybranch-art            redraw the code-drawn art (tools/make_art.py; commit art/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE; the rules below follow the root Makefile.

NUTTYBRANCH_MAKE = $(MAKE) --no-print-directory GAME=nuttybranch GAME_NAME=NuttyBranch
NUTTYBRANCH_DIR  = games/nuttybranch
# the house style kit (games/common/src, docs/art-direction.md) is compiled in
HOUSE_UI_nuttybranch = 1
# widescreen (docs/spec.md "Widescreen"; rs_game.max_width; DESIGN.md): the checks run at 320 and at each width of
# WIDTHS, then across widths (tests/wide_test.sh)
WIDESCREEN_nuttybranch = 1
BOT_RUNS ?= 40

.PHONY: nuttybranch nuttybranch-check nuttybranch-windows nuttybranch-armhf nuttybranch-dist nuttybranch-screenshots \
        nuttybranch-art nuttybranch-bench nuttybranch-bot

nuttybranch:
	+$(NUTTYBRANCH_MAKE) host
nuttybranch-windows:
	+$(NUTTYBRANCH_MAKE) windows
nuttybranch-armhf:
	+$(NUTTYBRANCH_MAKE) armhf
nuttybranch-dist:
	+$(NUTTYBRANCH_MAKE) dist

NUTTYBRANCH_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(NUTTYBRANCH_DIR)/src/*.c) $(HOUSE_SRC))

# the rules' unit tests link the rules alone (play.c, gen.c, road.c)
build/host/nuttybranch_test_physics: build/host/$(NUTTYBRANCH_DIR)/tests/test_physics.o \
        build/host/$(NUTTYBRANCH_DIR)/src/play.o build/host/$(NUTTYBRANCH_DIR)/src/gen.o \
        build/host/$(NUTTYBRANCH_DIR)/src/road.o build/host/librs.a
	$(HOST_CC) -o $@ $^ -lm

nuttybranch-check:
	+$(NUTTYBRANCH_MAKE) host build/host/test_libretro build/host/nuttybranch_test_states \
	    build/host/nuttybranch_test_physics
	./build/host/test_libretro build/host/nuttybranch_libretro.so 600
	./build/host/nuttybranch_test_physics
	sh $(NUTTYBRANCH_DIR)/tests/smoke_test.sh build/host/nuttybranch_headless build
	sh $(NUTTYBRANCH_DIR)/tests/bot_test.sh build/host/nuttybranch_headless build 12
	sh $(NUTTYBRANCH_DIR)/tests/ui_test.sh build/host/nuttybranch_headless build/nuttybranch-ui
	sh $(NUTTYBRANCH_DIR)/tests/state_test.sh build/host/nuttybranch_test_states build/states
	sh $(NUTTYBRANCH_DIR)/tests/lod_test.sh build/host/nuttybranch_headless
	sh $(NUTTYBRANCH_DIR)/tests/furrow_test.sh build/host/nuttybranch_headless build
	sh $(NUTTYBRANCH_DIR)/tests/touch_test.sh build/host/nuttybranch_headless build
	sh $(NUTTYBRANCH_DIR)/tests/bench_test.sh build/host/nuttybranch_headless
	$(PYTHON) tools/state_audit.py --game nuttybranch build/host/nuttybranch_test_states $(NUTTYBRANCH_OBJ_HOST)
	set -e; for w in $(WIDTHS); do echo "== widescreen: $$w x 240"; \
	    a=$$(case $$w in 427) echo 16:9;; 520) echo Wide;; esac); \
	    [ -z "$$a" ] || ./build/host/test_libretro build/host/nuttybranch_libretro.so 600 $$a $$w; \
	    RS_WIDTH=$$w sh $(NUTTYBRANCH_DIR)/tests/smoke_test.sh build/host/nuttybranch_headless build/w$$w; \
	    RS_WIDTH=$$w sh $(NUTTYBRANCH_DIR)/tests/ui_test.sh build/host/nuttybranch_headless build/nuttybranch-ui-$$w; \
	    RS_WIDTH=$$w sh $(NUTTYBRANCH_DIR)/tests/state_test.sh build/host/nuttybranch_test_states build/states-$$w; \
	    RS_WIDTH=$$w sh $(NUTTYBRANCH_DIR)/tests/lod_test.sh build/host/nuttybranch_headless; \
	    RS_WIDTH=$$w sh $(NUTTYBRANCH_DIR)/tests/furrow_test.sh build/host/nuttybranch_headless build/w$$w; \
	    RS_WIDTH=$$w sh $(NUTTYBRANCH_DIR)/tests/touch_test.sh build/host/nuttybranch_headless build/w$$w; \
	done
	RS_WIDTH=520 sh $(NUTTYBRANCH_DIR)/tests/bench_test.sh build/host/nuttybranch_headless
	sh $(NUTTYBRANCH_DIR)/tests/wide_test.sh build/host/nuttybranch_headless build/host/nuttybranch_test_states build "$(WIDTHS)"
	sh $(NUTTYBRANCH_DIR)/tests/tall_test.sh build/host/nuttybranch_headless build
	bash tools/i18n_test.sh build/host/nuttybranch_headless nuttybranch build/nuttybranch-i18n $(if $(WIDESCREEN_nuttybranch),320 $(WIDTHS),320)

nuttybranch-screenshots:
	+$(NUTTYBRANCH_MAKE) build/host/nuttybranch_headless
	sh $(NUTTYBRANCH_DIR)/tests/ui_test.sh build/host/nuttybranch_headless $(NUTTYBRANCH_DIR)/docs/screenshots 2

nuttybranch-bench:
	+$(NUTTYBRANCH_MAKE) build/host/nuttybranch_headless
	sh $(NUTTYBRANCH_DIR)/tests/bench_test.sh build/host/nuttybranch_headless verbose
	RS_WIDTH=520 sh $(NUTTYBRANCH_DIR)/tests/bench_test.sh build/host/nuttybranch_headless verbose

nuttybranch-bot:
	+$(NUTTYBRANCH_MAKE) build/host/nuttybranch_headless
	sh $(NUTTYBRANCH_DIR)/tests/bot_test.sh build/host/nuttybranch_headless build $(BOT_RUNS) verbose

nuttybranch-art:
	$(PYTHON) $(NUTTYBRANCH_DIR)/tools/make_art.py
