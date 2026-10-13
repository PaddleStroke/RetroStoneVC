# Hamster Havoc (games/hamsterhavoc): its make targets, included by the root Makefile.
#
#   make hamsterhavoc                host builds: hamsterhavoc (SDL2), hamsterhavoc_headless, hamsterhavoc_libretro.so
#   make hamsterhavoc-check          all its tests (the rules alone, libretro loader, smoke + bot + determinism, touch,
#                                    UI screens, save states and their audit, the 22 languages), at 320 and at each
#                                    widescreen width of WIDTHS (427 520), and save states across widths
#   make hamsterhavoc-dist           dist/windows/HamsterHavoc.exe, dist/libretro/hamsterhavoc_libretro.so (+ .armhf.so)
#   make hamsterhavoc-windows        only the Windows exe
#   make hamsterhavoc-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make hamsterhavoc-screenshots    games/hamsterhavoc/docs/screenshots/
#   make hamsterhavoc-art            redraw the code-drawn art (tools/make_art.py; commit art/)
#   make hamsterhavoc-music-alt      the two alternative tunes (tools/make_music_alt.py -> music-alt/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/hamsterhavoc/LICENSE; the rules below follow the root Makefile.

HAMSTERHAVOC_MAKE = $(MAKE) --no-print-directory GAME=hamsterhavoc GAME_NAME=HamsterHavoc
HAMSTERHAVOC_DIR  = games/hamsterhavoc
# the house style kit (games/common/src, docs/art-direction.md) is compiled in
HOUSE_UI_hamsterhavoc = 1
# widescreen (docs/spec.md "Widescreen"; rs_game.max_width): the checks run at 320 and again at each width of WIDTHS
WIDESCREEN_hamsterhavoc = 1

.PHONY: hamsterhavoc hamsterhavoc-check hamsterhavoc-windows hamsterhavoc-armhf hamsterhavoc-dist \
        hamsterhavoc-screenshots hamsterhavoc-art hamsterhavoc-music-alt

hamsterhavoc:
	+$(HAMSTERHAVOC_MAKE) host
hamsterhavoc-windows:
	+$(HAMSTERHAVOC_MAKE) windows
hamsterhavoc-armhf:
	+$(HAMSTERHAVOC_MAKE) armhf
hamsterhavoc-dist:
	+$(HAMSTERHAVOC_MAKE) dist

HAMSTERHAVOC_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(HAMSTERHAVOC_DIR)/src/*.c) $(HOUSE_SRC))

# the rules' test links play.c alone
build/host/hamsterhavoc_rules: $(HAMSTERHAVOC_DIR)/src/play.c $(HAMSTERHAVOC_DIR)/src/game.h $(HAMSTERHAVOC_DIR)/tests/test_rules.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -I$(HAMSTERHAVOC_DIR)/src $(HAMSTERHAVOC_DIR)/src/play.c \
	    $(HAMSTERHAVOC_DIR)/tests/test_rules.c -o $@

hamsterhavoc-check:
	+$(HAMSTERHAVOC_MAKE) host build/host/test_libretro build/host/hamsterhavoc_test_states build/host/hamsterhavoc_rules
	./build/host/hamsterhavoc_rules
	./build/host/test_libretro build/host/hamsterhavoc_libretro.so 600
	sh $(HAMSTERHAVOC_DIR)/tests/smoke_test.sh build/host/hamsterhavoc_headless build
	sh $(HAMSTERHAVOC_DIR)/tests/touch_test.sh build/host/hamsterhavoc_headless build "$(WIDTHS)"
	sh $(HAMSTERHAVOC_DIR)/tests/ui_test.sh build/host/hamsterhavoc_headless build/hamsterhavoc-ui
	sh $(HAMSTERHAVOC_DIR)/tests/state_test.sh build/host/hamsterhavoc_test_states build/states
	$(PYTHON) tools/state_audit.py --game hamsterhavoc build/host/hamsterhavoc_test_states $(HAMSTERHAVOC_OBJ_HOST)
	set -e; for w in $(WIDTHS); do echo "== widescreen: $$w x 240"; \
	    a=$$(case $$w in 427) echo 16:9;; 520) echo Wide;; esac); \
	    [ -z "$$a" ] || ./build/host/test_libretro build/host/hamsterhavoc_libretro.so 600 $$a $$w; \
	    RS_WIDTH=$$w sh $(HAMSTERHAVOC_DIR)/tests/smoke_test.sh build/host/hamsterhavoc_headless build/w$$w; \
	    RS_WIDTH=$$w sh $(HAMSTERHAVOC_DIR)/tests/ui_test.sh build/host/hamsterhavoc_headless build/hamsterhavoc-ui-$$w; \
	    RS_WIDTH=$$w sh $(HAMSTERHAVOC_DIR)/tests/state_test.sh build/host/hamsterhavoc_test_states build/states-$$w; \
	    for o in 320 $(WIDTHS); do [ $$o = $$w ] || ./build/host/hamsterhavoc_test_states --width $$o --opt bot=1 \
	        --opt ready=1 --input build/states-$$w/hamsterhavoc-run.input --resume build/states-$$w/hamsterhavoc-run \
	        --any-width | grep -q "all passed" || { echo "FAIL a state saved at $$w does not load at $$o"; exit 1; }; done; \
	    echo "  ok   a state saved at $$w loads at the other widths"; \
	done
	bash tools/i18n_test.sh build/host/hamsterhavoc_headless hamsterhavoc build/hamsterhavoc-i18n 320 $(WIDTHS)

hamsterhavoc-screenshots:
	+$(HAMSTERHAVOC_MAKE) build/host/hamsterhavoc_headless
	sh $(HAMSTERHAVOC_DIR)/tests/ui_test.sh build/host/hamsterhavoc_headless $(HAMSTERHAVOC_DIR)/docs/screenshots 2 all

hamsterhavoc-art:
	$(PYTHON) $(HAMSTERHAVOC_DIR)/tools/make_art.py

hamsterhavoc-music-alt:
	$(PYTHON) $(HAMSTERHAVOC_DIR)/tools/make_music_alt.py
