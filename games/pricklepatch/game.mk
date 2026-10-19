# Prickle Patch (games/pricklepatch): its make targets, included by the root Makefile.
#
#   make pricklepatch                host builds: pricklepatch (SDL2), pricklepatch_headless, pricklepatch_libretro.so
#   make pricklepatch-check          all its tests: the libretro loader, the smoke tests (the flow, the shed, pause, the
#                                    run kept and continued, the bot surviving its first minutes with random choices,
#                                    determinism), the UI screens, the save states and their audit, at 320 and at each
#                                    widescreen width of WIDTHS (427 520), states across widths, the tall screen and the
#                                    phone turning (tests/tall_test.sh), the frame cost (tests/bench_test.sh), the 22
#                                    languages
#   make pricklepatch-dist           dist/windows/PricklePatch.exe, dist/libretro/pricklepatch_libretro.so (+ .armhf.so)
#   make pricklepatch-windows        only the Windows exe
#   make pricklepatch-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make pricklepatch-screenshots    games/pricklepatch/docs/screenshots/
#   make pricklepatch-art            redraw the code-drawn art (tools/make_art.py; commit art/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pricklepatch/LICENSE; the rules below follow the root Makefile.

PRICKLEPATCH_MAKE = $(MAKE) --no-print-directory GAME=pricklepatch GAME_NAME=PricklePatch
PRICKLEPATCH_DIR  = games/pricklepatch
# the house style kit (games/common/src, docs/art-direction.md) is compiled in
HOUSE_UI_pricklepatch = 1
# widescreen (docs/spec.md "Widescreen"; rs_game.max_width): the checks run at 320 and again at each width of WIDTHS
WIDESCREEN_pricklepatch = 1

.PHONY: pricklepatch pricklepatch-check pricklepatch-windows pricklepatch-armhf pricklepatch-dist pricklepatch-screenshots \
        pricklepatch-art

pricklepatch:
	+$(PRICKLEPATCH_MAKE) host
pricklepatch-windows:
	+$(PRICKLEPATCH_MAKE) windows
pricklepatch-armhf:
	+$(PRICKLEPATCH_MAKE) armhf
pricklepatch-dist:
	+$(PRICKLEPATCH_MAKE) dist

PRICKLEPATCH_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(PRICKLEPATCH_DIR)/src/*.c) $(HOUSE_SRC))

pricklepatch-check:
	+$(PRICKLEPATCH_MAKE) host build/host/test_libretro build/host/pricklepatch_test_states
	./build/host/test_libretro build/host/pricklepatch_libretro.so 600
	sh $(PRICKLEPATCH_DIR)/tests/smoke_test.sh build/host/pricklepatch_headless build
	sh $(PRICKLEPATCH_DIR)/tests/ui_test.sh build/host/pricklepatch_headless build/pricklepatch-ui
	sh $(PRICKLEPATCH_DIR)/tests/state_test.sh build/host/pricklepatch_test_states build/states
	$(PYTHON) tools/state_audit.py --game pricklepatch build/host/pricklepatch_test_states $(PRICKLEPATCH_OBJ_HOST)
	set -e; for w in $(WIDTHS); do echo "== widescreen: $$w x 240"; \
	    a=$$(case $$w in 427) echo 16:9;; 520) echo Wide;; esac); \
	    [ -z "$$a" ] || ./build/host/test_libretro build/host/pricklepatch_libretro.so 600 $$a $$w; \
	    RS_WIDTH=$$w sh $(PRICKLEPATCH_DIR)/tests/smoke_test.sh build/host/pricklepatch_headless build/w$$w quick; \
	    RS_WIDTH=$$w sh $(PRICKLEPATCH_DIR)/tests/ui_test.sh build/host/pricklepatch_headless build/pricklepatch-ui-$$w; \
	    RS_WIDTH=$$w sh $(PRICKLEPATCH_DIR)/tests/state_test.sh build/host/pricklepatch_test_states build/states-$$w quick; \
	    for o in 320 $(WIDTHS); do [ $$o = $$w ] || ./build/host/pricklepatch_test_states --width $$o --opt bot=1 \
	        --opt ready=1 --opt seed=7 --input build/states-$$w/pricklepatch-run.input \
	        --resume build/states-$$w/pricklepatch-run --any-width | grep -q "all passed" \
	        || { echo "FAIL a state saved at $$w does not load at $$o"; exit 1; }; done; \
	    echo "  ok   a state saved at $$w loads at the other widths"; \
	done
	sh $(PRICKLEPATCH_DIR)/tests/tall_test.sh build/host/pricklepatch_headless build
	sh $(PRICKLEPATCH_DIR)/tests/bench_test.sh build/host/pricklepatch_headless
	bash tools/i18n_test.sh build/host/pricklepatch_headless pricklepatch build/pricklepatch-i18n 320 $(WIDTHS)

pricklepatch-screenshots:
	+$(PRICKLEPATCH_MAKE) build/host/pricklepatch_headless
	sh $(PRICKLEPATCH_DIR)/tests/ui_test.sh build/host/pricklepatch_headless $(PRICKLEPATCH_DIR)/docs/screenshots 1 all

pricklepatch-art:
	$(PYTHON) $(PRICKLEPATCH_DIR)/tools/make_art.py
