# Kitty Kibble (games/kittykibble): its make targets, included by the root Makefile.
#
#   make kittykibble                host builds: kittykibble (SDL2), kittykibble_headless, kittykibble_libretro.so
#   make kittykibble-check          all its tests (the rules alone, libretro loader, smoke + bot + determinism, touch,
#                                   UI screens, save states and their audit, the 22 languages), at 320 and at each
#                                   widescreen width of WIDTHS (427 520), and save states across widths
#   make kittykibble-dist           dist/windows/KittyKibble.exe, dist/libretro/kittykibble_libretro.so (+ .armhf.so)
#   make kittykibble-windows        only the Windows exe
#   make kittykibble-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make kittykibble-screenshots    games/kittykibble/docs/screenshots/
#   make kittykibble-art            redraw the code-drawn art (tools/make_art.py; commit art/)
#   make kittykibble-music-alt      the two alternative tunes (tools/make_music_alt.py -> music-alt/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/kittykibble/LICENSE; the rules below follow the root Makefile.

KITTYKIBBLE_MAKE = $(MAKE) --no-print-directory GAME=kittykibble GAME_NAME=KittyKibble
KITTYKIBBLE_DIR  = games/kittykibble
# the house style kit (games/common/src, docs/art-direction.md) is compiled in
HOUSE_UI_kittykibble = 1
# widescreen (docs/spec.md "Widescreen"; rs_game.max_width): the checks run at 320 and again at each width of WIDTHS
WIDESCREEN_kittykibble = 1

.PHONY: kittykibble kittykibble-check kittykibble-windows kittykibble-armhf kittykibble-dist kittykibble-screenshots \
        kittykibble-art kittykibble-music-alt

kittykibble:
	+$(KITTYKIBBLE_MAKE) host
kittykibble-windows:
	+$(KITTYKIBBLE_MAKE) windows
kittykibble-armhf:
	+$(KITTYKIBBLE_MAKE) armhf
kittykibble-dist:
	+$(KITTYKIBBLE_MAKE) dist

KITTYKIBBLE_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(KITTYKIBBLE_DIR)/src/*.c) $(HOUSE_SRC))

# the rules' test links play.c and the mazes alone
build/host/kittykibble_rules: $(KITTYKIBBLE_DIR)/src/play.c $(KITTYKIBBLE_DIR)/src/mazes.c $(KITTYKIBBLE_DIR)/src/game.h \
                              $(KITTYKIBBLE_DIR)/tests/test_rules.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -I$(KITTYKIBBLE_DIR)/src $(KITTYKIBBLE_DIR)/src/play.c \
	    $(KITTYKIBBLE_DIR)/src/mazes.c $(KITTYKIBBLE_DIR)/tests/test_rules.c -o $@

kittykibble-check:
	+$(KITTYKIBBLE_MAKE) host build/host/test_libretro build/host/kittykibble_test_states build/host/kittykibble_rules
	./build/host/kittykibble_rules
	./build/host/test_libretro build/host/kittykibble_libretro.so 600
	sh $(KITTYKIBBLE_DIR)/tests/smoke_test.sh build/host/kittykibble_headless build
	sh $(KITTYKIBBLE_DIR)/tests/touch_test.sh build/host/kittykibble_headless build "$(WIDTHS)"
	sh $(KITTYKIBBLE_DIR)/tests/ui_test.sh build/host/kittykibble_headless build/kittykibble-ui
	sh $(KITTYKIBBLE_DIR)/tests/state_test.sh build/host/kittykibble_test_states build/states
	$(PYTHON) tools/state_audit.py --game kittykibble build/host/kittykibble_test_states $(KITTYKIBBLE_OBJ_HOST)
	set -e; for w in $(WIDTHS); do echo "== widescreen: $$w x 240"; \
	    a=$$(case $$w in 427) echo 16:9;; 520) echo Wide;; esac); \
	    [ -z "$$a" ] || ./build/host/test_libretro build/host/kittykibble_libretro.so 600 $$a $$w; \
	    RS_WIDTH=$$w sh $(KITTYKIBBLE_DIR)/tests/smoke_test.sh build/host/kittykibble_headless build/w$$w quick; \
	    RS_WIDTH=$$w sh $(KITTYKIBBLE_DIR)/tests/ui_test.sh build/host/kittykibble_headless build/kittykibble-ui-$$w; \
	    RS_WIDTH=$$w sh $(KITTYKIBBLE_DIR)/tests/state_test.sh build/host/kittykibble_test_states build/states-$$w quick; \
	    for o in 320 $(WIDTHS); do [ $$o = $$w ] || ./build/host/kittykibble_test_states --width $$o --opt bot=1 \
	        --opt ready=1 --input build/states-$$w/kittykibble-run.input --resume build/states-$$w/kittykibble-run \
	        --any-width | grep -q "all passed" || { echo "FAIL a state saved at $$w does not load at $$o"; exit 1; }; done; \
	    echo "  ok   a state saved at $$w loads at the other widths"; \
	done
	bash tools/i18n_test.sh build/host/kittykibble_headless kittykibble build/kittykibble-i18n 320 $(WIDTHS)

kittykibble-screenshots:
	+$(KITTYKIBBLE_MAKE) build/host/kittykibble_headless
	sh $(KITTYKIBBLE_DIR)/tests/ui_test.sh build/host/kittykibble_headless $(KITTYKIBBLE_DIR)/docs/screenshots 2 all

kittykibble-art:
	$(PYTHON) $(KITTYKIBBLE_DIR)/tools/make_art.py

kittykibble-music-alt:
	$(PYTHON) $(KITTYKIBBLE_DIR)/tools/make_music_alt.py
