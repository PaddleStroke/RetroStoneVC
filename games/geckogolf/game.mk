# Gecko Golf (games/geckogolf): its make targets, included by the root Makefile.
#
#   make geckogolf                host builds: geckogolf (SDL2), geckogolf_headless, geckogolf_libretro.so
#   make geckogolf-check          all its tests (the rules alone, libretro loader, smoke + bot + determinism, touch,
#                                  UI screens, save states and their audit, the 22 languages), at 320 and at each
#                                  widescreen width of WIDTHS (427 520), and save states across widths
#   make geckogolf-dist           dist/windows/GeckoGolf.exe, dist/libretro/geckogolf_libretro.so (+ .armhf.so)
#   make geckogolf-windows        only the Windows exe
#   make geckogolf-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make geckogolf-screenshots    games/geckogolf/docs/screenshots/
#   make geckogolf-art            redraw the code-drawn art (tools/make_art.py; commit art/)
#   make geckogolf-music-alt      the two alternative tunes (tools/make_music_alt.py -> music-alt/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/geckogolf/LICENSE; the rules below follow the root Makefile.

GECKOGOLF_MAKE = $(MAKE) --no-print-directory GAME=geckogolf GAME_NAME=GeckoGolf
GECKOGOLF_DIR  = games/geckogolf
# the house style kit (games/common/src, docs/art-direction.md) is compiled in
HOUSE_UI_geckogolf = 1
# widescreen (docs/spec.md "Widescreen"; rs_game.max_width): the checks run at 320 and again at each width of WIDTHS
WIDESCREEN_geckogolf = 1

.PHONY: geckogolf geckogolf-check geckogolf-windows geckogolf-armhf geckogolf-dist geckogolf-screenshots \
        geckogolf-art geckogolf-music-alt

geckogolf:
	+$(GECKOGOLF_MAKE) host
geckogolf-windows:
	+$(GECKOGOLF_MAKE) windows
geckogolf-armhf:
	+$(GECKOGOLF_MAKE) armhf
geckogolf-dist:
	+$(GECKOGOLF_MAKE) dist

GECKOGOLF_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(GECKOGOLF_DIR)/src/*.c) $(HOUSE_SRC))

# the rules' test links play.c alone
build/host/geckogolf_rules: $(GECKOGOLF_DIR)/src/play.c $(GECKOGOLF_DIR)/src/game.h $(GECKOGOLF_DIR)/tests/test_rules.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -I$(GECKOGOLF_DIR)/src $(GECKOGOLF_DIR)/src/play.c \
	    $(GECKOGOLF_DIR)/tests/test_rules.c -o $@

geckogolf-check:
	+$(GECKOGOLF_MAKE) host build/host/test_libretro build/host/geckogolf_test_states build/host/geckogolf_rules
	./build/host/geckogolf_rules
	./build/host/test_libretro build/host/geckogolf_libretro.so 600
	sh $(GECKOGOLF_DIR)/tests/smoke_test.sh build/host/geckogolf_headless build
	sh $(GECKOGOLF_DIR)/tests/touch_test.sh build/host/geckogolf_headless build "$(WIDTHS)"
	sh $(GECKOGOLF_DIR)/tests/ui_test.sh build/host/geckogolf_headless build/geckogolf-ui
	sh $(GECKOGOLF_DIR)/tests/state_test.sh build/host/geckogolf_test_states build/states
	$(PYTHON) tools/state_audit.py --game geckogolf build/host/geckogolf_test_states $(GECKOGOLF_OBJ_HOST)
	set -e; for w in $(WIDTHS); do echo "== widescreen: $$w x 240"; \
	    a=$$(case $$w in 427) echo 16:9;; 520) echo Wide;; esac); \
	    [ -z "$$a" ] || ./build/host/test_libretro build/host/geckogolf_libretro.so 600 $$a $$w; \
	    RS_WIDTH=$$w sh $(GECKOGOLF_DIR)/tests/smoke_test.sh build/host/geckogolf_headless build/w$$w quick; \
	    RS_WIDTH=$$w sh $(GECKOGOLF_DIR)/tests/ui_test.sh build/host/geckogolf_headless build/geckogolf-ui-$$w; \
	    RS_WIDTH=$$w sh $(GECKOGOLF_DIR)/tests/state_test.sh build/host/geckogolf_test_states build/states-$$w quick; \
	    for o in 320 $(WIDTHS); do [ $$o = $$w ] || ./build/host/geckogolf_test_states --width $$o --opt bot=1 \
	        --opt ready=1 --input build/states-$$w/geckogolf-run.input --resume build/states-$$w/geckogolf-run \
	        --any-width | grep -q "all passed" || { echo "FAIL a state saved at $$w does not load at $$o"; exit 1; }; done; \
	    echo "  ok   a state saved at $$w loads at the other widths"; \
	done
	bash tools/i18n_test.sh build/host/geckogolf_headless geckogolf build/geckogolf-i18n 320 $(WIDTHS)

geckogolf-screenshots:
	+$(GECKOGOLF_MAKE) build/host/geckogolf_headless
	sh $(GECKOGOLF_DIR)/tests/ui_test.sh build/host/geckogolf_headless $(GECKOGOLF_DIR)/docs/screenshots 2 all

geckogolf-art:
	$(PYTHON) $(GECKOGOLF_DIR)/tools/make_art.py

geckogolf-music-alt:
	$(PYTHON) $(GECKOGOLF_DIR)/tools/make_music_alt.py
