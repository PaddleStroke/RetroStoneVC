# Puffin Plunge (games/puffinplunge): its make targets, included by the root Makefile.
#
#   make puffinplunge                host builds: puffinplunge (SDL2), puffinplunge_headless, puffinplunge_libretro.so
#   make puffinplunge-check          all its tests: the rules (tests/test_world.c: every chunk passable, the levels,
#                                    the bot diving through levels 1-2 on 40 seeds, determinism), the libretro loader,
#                                    the smoke tests (the flow, the shops, pause, the dive kept and continued, the bot,
#                                    determinism), the UI screens, the save states and their audit, at 320 and at each
#                                    widescreen width of WIDTHS (427 520), states across widths, the tall screen and the
#                                    phone turning (tests/tall_test.sh), the frame cost (tests/bench_test.sh), the 22
#                                    languages
#   make puffinplunge-dist           dist/windows/PuffinPlunge.exe, dist/libretro/puffinplunge_libretro.so (+ .armhf.so)
#   make puffinplunge-windows        only the Windows exe
#   make puffinplunge-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make puffinplunge-screenshots    games/puffinplunge/docs/screenshots/
#   make puffinplunge-art            redraw the code-drawn art (tools/make_art.py; commit art/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/puffinplunge/LICENSE; the rules below follow the root Makefile.

PUFFINPLUNGE_MAKE = $(MAKE) --no-print-directory GAME=puffinplunge GAME_NAME=PuffinPlunge
PUFFINPLUNGE_DIR  = games/puffinplunge
# the house style kit (games/common/src, docs/art-direction.md) is compiled in
HOUSE_UI_puffinplunge = 1
# widescreen (docs/spec.md "Widescreen"; rs_game.max_width): the checks run at 320 and again at each width of WIDTHS
WIDESCREEN_puffinplunge = 1

.PHONY: puffinplunge puffinplunge-check puffinplunge-windows puffinplunge-armhf puffinplunge-dist puffinplunge-screenshots \
        puffinplunge-art

puffinplunge:
	+$(PUFFINPLUNGE_MAKE) host
puffinplunge-windows:
	+$(PUFFINPLUNGE_MAKE) windows
puffinplunge-armhf:
	+$(PUFFINPLUNGE_MAKE) armhf
puffinplunge-dist:
	+$(PUFFINPLUNGE_MAKE) dist

PUFFINPLUNGE_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(PUFFINPLUNGE_DIR)/src/*.c) $(HOUSE_SRC))

# the rules alone (world.c) with their tests
build/host/puffinplunge_world: $(PUFFINPLUNGE_DIR)/src/world.c $(PUFFINPLUNGE_DIR)/src/game.h $(PUFFINPLUNGE_DIR)/tests/test_world.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -I$(PUFFINPLUNGE_DIR)/src $(PUFFINPLUNGE_DIR)/src/world.c $(PUFFINPLUNGE_DIR)/tests/test_world.c -o $@

puffinplunge-check:
	+$(PUFFINPLUNGE_MAKE) host build/host/test_libretro build/host/puffinplunge_test_states build/host/puffinplunge_world
	./build/host/puffinplunge_world $(PUFFINPLUNGE_DIR)/levels/chunks.txt 40
	./build/host/test_libretro build/host/puffinplunge_libretro.so 600
	sh $(PUFFINPLUNGE_DIR)/tests/smoke_test.sh build/host/puffinplunge_headless build
	sh $(PUFFINPLUNGE_DIR)/tests/ui_test.sh build/host/puffinplunge_headless build/puffinplunge-ui
	sh $(PUFFINPLUNGE_DIR)/tests/state_test.sh build/host/puffinplunge_test_states build/states
	$(PYTHON) tools/state_audit.py --game puffinplunge build/host/puffinplunge_test_states $(PUFFINPLUNGE_OBJ_HOST)
	set -e; for w in $(WIDTHS); do echo "== widescreen: $$w x 240"; \
	    a=$$(case $$w in 427) echo 16:9;; 520) echo Wide;; esac); \
	    [ -z "$$a" ] || ./build/host/test_libretro build/host/puffinplunge_libretro.so 600 $$a $$w; \
	    RS_WIDTH=$$w sh $(PUFFINPLUNGE_DIR)/tests/smoke_test.sh build/host/puffinplunge_headless build/w$$w quick; \
	    RS_WIDTH=$$w sh $(PUFFINPLUNGE_DIR)/tests/ui_test.sh build/host/puffinplunge_headless build/puffinplunge-ui-$$w; \
	    RS_WIDTH=$$w sh $(PUFFINPLUNGE_DIR)/tests/state_test.sh build/host/puffinplunge_test_states build/states-$$w quick; \
	    for o in 320 $(WIDTHS); do [ $$o = $$w ] || ./build/host/puffinplunge_test_states --width $$o --opt bot=1 \
	        --opt ready=1 --opt seed=7 --input build/states-$$w/puffinplunge-run.input \
	        --resume build/states-$$w/puffinplunge-run --any-width | grep -q "all passed" \
	        || { echo "FAIL a state saved at $$w does not load at $$o"; exit 1; }; done; \
	    echo "  ok   a state saved at $$w loads at the other widths"; \
	done
	sh $(PUFFINPLUNGE_DIR)/tests/tall_test.sh build/host/puffinplunge_headless build
	sh $(PUFFINPLUNGE_DIR)/tests/bench_test.sh build/host/puffinplunge_headless
	bash tools/i18n_test.sh build/host/puffinplunge_headless puffinplunge build/puffinplunge-i18n 320 $(WIDTHS)

puffinplunge-screenshots:
	+$(PUFFINPLUNGE_MAKE) build/host/puffinplunge_headless
	sh $(PUFFINPLUNGE_DIR)/tests/ui_test.sh build/host/puffinplunge_headless $(PUFFINPLUNGE_DIR)/docs/screenshots 1 all

puffinplunge-art:
	$(PYTHON) $(PUFFINPLUNGE_DIR)/tools/make_art.py
