# Lemur Leap (games/lemurleap): its make targets, included by the root Makefile.
#
#   make lemurleap                host builds: lemurleap (SDL2), lemurleap_headless, lemurleap_libretro.so
#   make lemurleap-check          all its tests: the rules, the generator's path over 1000 seeds and the bot's climbs
#                                 (tests/test_world.c), the libretro loader, the smoke tests (the flow, the wardrobe,
#                                 pause, the climb kept and continued, the fall and the record in save RAM, the touches,
#                                 the bot climbing, determinism), the UI screens, the save states and their audit, at 320
#                                 and at each widescreen width of WIDTHS (427 520), states across widths, the tall screen
#                                 and the phone turning (tests/tall_test.sh), the frame cost (tests/bench_test.sh), the
#                                 22 languages
#   make lemurleap-dist           dist/windows/LemurLeap.exe, dist/libretro/lemurleap_libretro.so (+ .armhf.so)
#   make lemurleap-windows        only the Windows exe
#   make lemurleap-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make lemurleap-screenshots    games/lemurleap/docs/screenshots/
#   make lemurleap-art            redraw the code-drawn art (tools/make_art.py; commit art/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/lemurleap/LICENSE; the rules below follow the root Makefile.

LEMURLEAP_MAKE = $(MAKE) --no-print-directory GAME=lemurleap GAME_NAME=LemurLeap
LEMURLEAP_DIR  = games/lemurleap
# the house style kit (games/common/src, docs/art-direction.md) is compiled in
HOUSE_UI_lemurleap = 1
# widescreen (docs/spec.md "Widescreen"; rs_game.max_width): the checks run at 320 and again at each width of WIDTHS
WIDESCREEN_lemurleap = 1

.PHONY: lemurleap lemurleap-check lemurleap-windows lemurleap-armhf lemurleap-dist lemurleap-screenshots lemurleap-art

lemurleap:
	+$(LEMURLEAP_MAKE) host
lemurleap-windows:
	+$(LEMURLEAP_MAKE) windows
lemurleap-armhf:
	+$(LEMURLEAP_MAKE) armhf
lemurleap-dist:
	+$(LEMURLEAP_MAKE) dist

LEMURLEAP_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(LEMURLEAP_DIR)/src/*.c) $(HOUSE_SRC))

# the rules alone (world.c) with their tests: the generator's path, the bot's climbs, determinism
build/host/lemurleap_world: $(LEMURLEAP_DIR)/src/world.c $(LEMURLEAP_DIR)/src/game.h $(LEMURLEAP_DIR)/tests/test_world.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -I$(LEMURLEAP_DIR)/src $(LEMURLEAP_DIR)/src/world.c $(LEMURLEAP_DIR)/tests/test_world.c -o $@

lemurleap-check:
	+$(LEMURLEAP_MAKE) host build/host/test_libretro build/host/lemurleap_test_states build/host/lemurleap_world
	./build/host/lemurleap_world 1000
	./build/host/test_libretro build/host/lemurleap_libretro.so 600
	sh $(LEMURLEAP_DIR)/tests/smoke_test.sh build/host/lemurleap_headless build
	sh $(LEMURLEAP_DIR)/tests/ui_test.sh build/host/lemurleap_headless build/lemurleap-ui
	sh $(LEMURLEAP_DIR)/tests/state_test.sh build/host/lemurleap_test_states build/states
	$(PYTHON) tools/state_audit.py --game lemurleap build/host/lemurleap_test_states $(LEMURLEAP_OBJ_HOST)
	set -e; for w in $(WIDTHS); do echo "== widescreen: $$w x 240"; \
	    a=$$(case $$w in 427) echo 16:9;; 520) echo Wide;; esac); \
	    [ -z "$$a" ] || ./build/host/test_libretro build/host/lemurleap_libretro.so 600 $$a $$w; \
	    RS_WIDTH=$$w sh $(LEMURLEAP_DIR)/tests/smoke_test.sh build/host/lemurleap_headless build/w$$w quick; \
	    RS_WIDTH=$$w sh $(LEMURLEAP_DIR)/tests/ui_test.sh build/host/lemurleap_headless build/lemurleap-ui-$$w; \
	    RS_WIDTH=$$w sh $(LEMURLEAP_DIR)/tests/state_test.sh build/host/lemurleap_test_states build/states-$$w quick; \
	    for o in 320 $(WIDTHS); do [ $$o = $$w ] || ./build/host/lemurleap_test_states --width $$o --opt bot=1 \
	        --opt ready=1 --opt seed=7 --input build/states-$$w/lemurleap-run.input \
	        --resume build/states-$$w/lemurleap-run --any-width | grep -q "all passed" \
	        || { echo "FAIL a state saved at $$w does not load at $$o"; exit 1; }; done; \
	    echo "  ok   a state saved at $$w loads at the other widths"; \
	done
	sh $(LEMURLEAP_DIR)/tests/tall_test.sh build/host/lemurleap_headless build
	sh $(LEMURLEAP_DIR)/tests/bench_test.sh build/host/lemurleap_headless
	bash tools/i18n_test.sh build/host/lemurleap_headless lemurleap build/lemurleap-i18n 320 $(WIDTHS)

lemurleap-screenshots:
	+$(LEMURLEAP_MAKE) build/host/lemurleap_headless
	sh $(LEMURLEAP_DIR)/tests/ui_test.sh build/host/lemurleap_headless $(LEMURLEAP_DIR)/docs/screenshots 1 all

lemurleap-art:
	$(PYTHON) $(LEMURLEAP_DIR)/tools/make_art.py
