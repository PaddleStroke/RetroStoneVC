# Ladybug Laps (games/ladybuglaps): its make targets, included by the root Makefile.
#
#   make ladybuglaps                host builds: ladybuglaps (SDL2), ladybuglaps_headless, ladybuglaps_libretro.so
#   make ladybuglaps-check          all its tests (the rules alone, libretro loader, smoke + bot + determinism, touch,
#                                  UI screens, save states and their audit, the 22 languages), at 320 and at each
#                                  widescreen width of WIDTHS (427 520), and save states across widths
#   make ladybuglaps-dist           dist/windows/LadybugLaps.exe, dist/libretro/ladybuglaps_libretro.so (+ .armhf.so)
#   make ladybuglaps-windows        only the Windows exe
#   make ladybuglaps-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make ladybuglaps-screenshots    games/ladybuglaps/docs/screenshots/
#   make ladybuglaps-art            redraw the code-drawn art (tools/make_art.py; commit art/)
#   make ladybuglaps-music-alt      the two alternative tunes (tools/make_music_alt.py -> music-alt/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/ladybuglaps/LICENSE; the rules below follow the root Makefile.

LADYBUGLAPS_MAKE = $(MAKE) --no-print-directory GAME=ladybuglaps GAME_NAME=LadybugLaps
LADYBUGLAPS_DIR  = games/ladybuglaps
# the house style kit (games/common/src, docs/art-direction.md) is compiled in
HOUSE_UI_ladybuglaps = 1
# widescreen (docs/spec.md "Widescreen"; rs_game.max_width): the checks run at 320 and again at each width of WIDTHS
WIDESCREEN_ladybuglaps = 1

.PHONY: ladybuglaps ladybuglaps-check ladybuglaps-windows ladybuglaps-armhf ladybuglaps-dist ladybuglaps-screenshots \
        ladybuglaps-art ladybuglaps-music-alt

ladybuglaps:
	+$(LADYBUGLAPS_MAKE) host
ladybuglaps-windows:
	+$(LADYBUGLAPS_MAKE) windows
ladybuglaps-armhf:
	+$(LADYBUGLAPS_MAKE) armhf
ladybuglaps-dist:
	+$(LADYBUGLAPS_MAKE) dist

LADYBUGLAPS_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(LADYBUGLAPS_DIR)/src/*.c) $(HOUSE_SRC))

# the rules' test links play.c and tracks.c alone
build/host/ladybuglaps_rules: $(LADYBUGLAPS_DIR)/src/play.c $(LADYBUGLAPS_DIR)/src/tracks.c $(LADYBUGLAPS_DIR)/src/game.h \
                              $(LADYBUGLAPS_DIR)/tests/test_rules.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -I$(LADYBUGLAPS_DIR)/src $(LADYBUGLAPS_DIR)/src/play.c \
	    $(LADYBUGLAPS_DIR)/src/tracks.c $(LADYBUGLAPS_DIR)/tests/test_rules.c -o $@

# the tracks are drawn from src/tracks.c (tools/trackdata.py reads it): the assets follow it
build/gen/ladybuglaps/assets.c: $(LADYBUGLAPS_DIR)/src/tracks.c $(LADYBUGLAPS_DIR)/src/game.h

ladybuglaps-check:
	+$(LADYBUGLAPS_MAKE) host build/host/test_libretro build/host/ladybuglaps_test_states build/host/ladybuglaps_rules
	./build/host/ladybuglaps_rules
	./build/host/test_libretro build/host/ladybuglaps_libretro.so 600
	sh $(LADYBUGLAPS_DIR)/tests/smoke_test.sh build/host/ladybuglaps_headless build
	sh $(LADYBUGLAPS_DIR)/tests/touch_test.sh build/host/ladybuglaps_headless build "$(WIDTHS)"
	sh $(LADYBUGLAPS_DIR)/tests/ui_test.sh build/host/ladybuglaps_headless build/ladybuglaps-ui
	sh $(LADYBUGLAPS_DIR)/tests/state_test.sh build/host/ladybuglaps_test_states build/states
	$(PYTHON) tools/state_audit.py --game ladybuglaps build/host/ladybuglaps_test_states $(LADYBUGLAPS_OBJ_HOST)
	set -e; for w in $(WIDTHS); do echo "== widescreen: $$w x 240"; \
	    a=$$(case $$w in 427) echo 16:9;; 520) echo Wide;; esac); \
	    [ -z "$$a" ] || ./build/host/test_libretro build/host/ladybuglaps_libretro.so 600 $$a $$w; \
	    RS_WIDTH=$$w sh $(LADYBUGLAPS_DIR)/tests/smoke_test.sh build/host/ladybuglaps_headless build/w$$w quick; \
	    RS_WIDTH=$$w sh $(LADYBUGLAPS_DIR)/tests/ui_test.sh build/host/ladybuglaps_headless build/ladybuglaps-ui-$$w; \
	    RS_WIDTH=$$w sh $(LADYBUGLAPS_DIR)/tests/state_test.sh build/host/ladybuglaps_test_states build/states-$$w quick; \
	    for o in 320 $(WIDTHS); do [ $$o = $$w ] || ./build/host/ladybuglaps_test_states --width $$o --opt bot=1 \
	        --opt ready=1 --input build/states-$$w/ladybuglaps-run.input --resume build/states-$$w/ladybuglaps-run \
	        --any-width | grep -q "all passed" || { echo "FAIL a state saved at $$w does not load at $$o"; exit 1; }; done; \
	    echo "  ok   a state saved at $$w loads at the other widths"; \
	done
	bash tools/i18n_test.sh build/host/ladybuglaps_headless ladybuglaps build/ladybuglaps-i18n 320 $(WIDTHS)

ladybuglaps-screenshots:
	+$(LADYBUGLAPS_MAKE) build/host/ladybuglaps_headless
	sh $(LADYBUGLAPS_DIR)/tests/ui_test.sh build/host/ladybuglaps_headless $(LADYBUGLAPS_DIR)/docs/screenshots 2 all

ladybuglaps-art:
	$(PYTHON) $(LADYBUGLAPS_DIR)/tools/make_art.py

ladybuglaps-music-alt:
	$(PYTHON) $(LADYBUGLAPS_DIR)/tools/make_music_alt.py
