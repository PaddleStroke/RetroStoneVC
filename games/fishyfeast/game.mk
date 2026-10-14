# Fishy Feast (games/fishyfeast): its make targets, included by the root Makefile.
#
#   make fishyfeast                host builds: fishyfeast (SDL2), fishyfeast_headless, fishyfeast_libretro.so
#   make fishyfeast-check          all its tests (the rules alone, libretro loader, smoke + bot + determinism, touch,
#                                  UI screens, save states and their audit, the 22 languages), at 320 and at each
#                                  widescreen width of WIDTHS (427 520), and save states across widths
#   make fishyfeast-dist           dist/windows/FishyFeast.exe, dist/libretro/fishyfeast_libretro.so (+ .armhf.so)
#   make fishyfeast-windows        only the Windows exe
#   make fishyfeast-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make fishyfeast-screenshots    games/fishyfeast/docs/screenshots/
#   make fishyfeast-art            redraw the code-drawn art (tools/make_art.py; commit art/)
#   make fishyfeast-music-alt      the two alternative tunes (tools/make_music_alt.py -> music-alt/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/fishyfeast/LICENSE; the rules below follow the root Makefile.

FISHYFEAST_MAKE = $(MAKE) --no-print-directory GAME=fishyfeast GAME_NAME=FishyFeast
FISHYFEAST_DIR  = games/fishyfeast
# the house style kit (games/common/src, docs/art-direction.md) is compiled in
HOUSE_UI_fishyfeast = 1
# widescreen (docs/spec.md "Widescreen"; rs_game.max_width): the checks run at 320 and again at each width of WIDTHS
WIDESCREEN_fishyfeast = 1

.PHONY: fishyfeast fishyfeast-check fishyfeast-windows fishyfeast-armhf fishyfeast-dist fishyfeast-screenshots \
        fishyfeast-art fishyfeast-music-alt

fishyfeast:
	+$(FISHYFEAST_MAKE) host
fishyfeast-windows:
	+$(FISHYFEAST_MAKE) windows
fishyfeast-armhf:
	+$(FISHYFEAST_MAKE) armhf
fishyfeast-dist:
	+$(FISHYFEAST_MAKE) dist

FISHYFEAST_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(FISHYFEAST_DIR)/src/*.c) $(HOUSE_SRC))

# the rules' test links play.c alone
build/host/fishyfeast_rules: $(FISHYFEAST_DIR)/src/play.c $(FISHYFEAST_DIR)/src/game.h $(FISHYFEAST_DIR)/tests/test_rules.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -I$(FISHYFEAST_DIR)/src $(FISHYFEAST_DIR)/src/play.c \
	    $(FISHYFEAST_DIR)/tests/test_rules.c -o $@

fishyfeast-check:
	+$(FISHYFEAST_MAKE) host build/host/test_libretro build/host/fishyfeast_test_states build/host/fishyfeast_rules
	./build/host/fishyfeast_rules
	./build/host/test_libretro build/host/fishyfeast_libretro.so 600
	sh $(FISHYFEAST_DIR)/tests/smoke_test.sh build/host/fishyfeast_headless build
	sh $(FISHYFEAST_DIR)/tests/touch_test.sh build/host/fishyfeast_headless build "$(WIDTHS)"
	sh $(FISHYFEAST_DIR)/tests/ui_test.sh build/host/fishyfeast_headless build/fishyfeast-ui
	sh $(FISHYFEAST_DIR)/tests/state_test.sh build/host/fishyfeast_test_states build/states
	$(PYTHON) tools/state_audit.py --game fishyfeast build/host/fishyfeast_test_states $(FISHYFEAST_OBJ_HOST)
	set -e; for w in $(WIDTHS); do echo "== widescreen: $$w x 240"; \
	    a=$$(case $$w in 427) echo 16:9;; 520) echo Wide;; esac); \
	    [ -z "$$a" ] || ./build/host/test_libretro build/host/fishyfeast_libretro.so 600 $$a $$w; \
	    RS_WIDTH=$$w sh $(FISHYFEAST_DIR)/tests/smoke_test.sh build/host/fishyfeast_headless build/w$$w quick; \
	    RS_WIDTH=$$w sh $(FISHYFEAST_DIR)/tests/ui_test.sh build/host/fishyfeast_headless build/fishyfeast-ui-$$w; \
	    RS_WIDTH=$$w sh $(FISHYFEAST_DIR)/tests/state_test.sh build/host/fishyfeast_test_states build/states-$$w quick; \
	    for o in 320 $(WIDTHS); do [ $$o = $$w ] || ./build/host/fishyfeast_test_states --width $$o --opt bot=1 \
	        --opt ready=1 --input build/states-$$w/fishyfeast-run.input --resume build/states-$$w/fishyfeast-run \
	        --any-width | grep -q "all passed" || { echo "FAIL a state saved at $$w does not load at $$o"; exit 1; }; done; \
	    echo "  ok   a state saved at $$w loads at the other widths"; \
	done
	bash tools/i18n_test.sh build/host/fishyfeast_headless fishyfeast build/fishyfeast-i18n 320 $(WIDTHS)

fishyfeast-screenshots:
	+$(FISHYFEAST_MAKE) build/host/fishyfeast_headless
	sh $(FISHYFEAST_DIR)/tests/ui_test.sh build/host/fishyfeast_headless $(FISHYFEAST_DIR)/docs/screenshots 2 all

fishyfeast-art:
	$(PYTHON) $(FISHYFEAST_DIR)/tools/make_art.py

fishyfeast-music-alt:
	$(PYTHON) $(FISHYFEAST_DIR)/tools/make_music_alt.py
