# Cosmo Corgi (games/cosmocorgi): its make targets, included by the root Makefile.
#
#   make cosmocorgi                host builds: cosmocorgi (SDL2), cosmocorgi_headless, cosmocorgi_libretro.so
#   make cosmocorgi-check          all its tests (the rules alone, libretro loader, smoke + bot + determinism, touch,
#                                  UI screens, save states and their audit, the 22 languages), at 320 and at each
#                                  widescreen width of WIDTHS (427 520), and save states across widths
#   make cosmocorgi-dist           dist/windows/CosmoCorgi.exe, dist/libretro/cosmocorgi_libretro.so (+ .armhf.so)
#   make cosmocorgi-windows        only the Windows exe
#   make cosmocorgi-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make cosmocorgi-screenshots    games/cosmocorgi/docs/screenshots/
#   make cosmocorgi-art            redraw the code-drawn art (tools/make_art.py; commit art/)
#   make cosmocorgi-music-alt      the two alternative tunes (tools/make_music_alt.py -> music-alt/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/cosmocorgi/LICENSE; the rules below follow the root Makefile.

COSMOCORGI_MAKE = $(MAKE) --no-print-directory GAME=cosmocorgi GAME_NAME=CosmoCorgi
COSMOCORGI_DIR  = games/cosmocorgi
# the house style kit (games/common/src, docs/art-direction.md) is compiled in
HOUSE_UI_cosmocorgi = 1
# widescreen (docs/spec.md "Widescreen"; rs_game.max_width): the checks run at 320 and again at each width of WIDTHS
WIDESCREEN_cosmocorgi = 1

.PHONY: cosmocorgi cosmocorgi-check cosmocorgi-windows cosmocorgi-armhf cosmocorgi-dist cosmocorgi-screenshots \
        cosmocorgi-art cosmocorgi-music-alt

cosmocorgi:
	+$(COSMOCORGI_MAKE) host
cosmocorgi-windows:
	+$(COSMOCORGI_MAKE) windows
cosmocorgi-armhf:
	+$(COSMOCORGI_MAKE) armhf
cosmocorgi-dist:
	+$(COSMOCORGI_MAKE) dist

COSMOCORGI_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(COSMOCORGI_DIR)/src/*.c) $(HOUSE_SRC))

# the rules' test links play.c alone
build/host/cosmocorgi_rules: $(COSMOCORGI_DIR)/src/play.c $(COSMOCORGI_DIR)/src/game.h $(COSMOCORGI_DIR)/tests/test_rules.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -I$(COSMOCORGI_DIR)/src $(COSMOCORGI_DIR)/src/play.c \
	    $(COSMOCORGI_DIR)/tests/test_rules.c -o $@

cosmocorgi-check:
	+$(COSMOCORGI_MAKE) host build/host/test_libretro build/host/cosmocorgi_test_states build/host/cosmocorgi_rules
	./build/host/cosmocorgi_rules
	./build/host/test_libretro build/host/cosmocorgi_libretro.so 600
	sh $(COSMOCORGI_DIR)/tests/smoke_test.sh build/host/cosmocorgi_headless build
	sh $(COSMOCORGI_DIR)/tests/touch_test.sh build/host/cosmocorgi_headless build "$(WIDTHS)"
	sh $(COSMOCORGI_DIR)/tests/ui_test.sh build/host/cosmocorgi_headless build/cosmocorgi-ui
	sh $(COSMOCORGI_DIR)/tests/state_test.sh build/host/cosmocorgi_test_states build/states
	$(PYTHON) tools/state_audit.py --game cosmocorgi build/host/cosmocorgi_test_states $(COSMOCORGI_OBJ_HOST)
	set -e; for w in $(WIDTHS); do echo "== widescreen: $$w x 240"; \
	    a=$$(case $$w in 427) echo 16:9;; 520) echo Wide;; esac); \
	    [ -z "$$a" ] || ./build/host/test_libretro build/host/cosmocorgi_libretro.so 600 $$a $$w; \
	    RS_WIDTH=$$w sh $(COSMOCORGI_DIR)/tests/smoke_test.sh build/host/cosmocorgi_headless build/w$$w quick; \
	    RS_WIDTH=$$w sh $(COSMOCORGI_DIR)/tests/ui_test.sh build/host/cosmocorgi_headless build/cosmocorgi-ui-$$w; \
	    RS_WIDTH=$$w sh $(COSMOCORGI_DIR)/tests/state_test.sh build/host/cosmocorgi_test_states build/states-$$w quick; \
	    for o in 320 $(WIDTHS); do [ $$o = $$w ] || ./build/host/cosmocorgi_test_states --width $$o --opt bot=1 \
	        --opt ready=1 --input build/states-$$w/cosmocorgi-run.input --resume build/states-$$w/cosmocorgi-run \
	        --any-width | grep -q "all passed" || { echo "FAIL a state saved at $$w does not load at $$o"; exit 1; }; done; \
	    echo "  ok   a state saved at $$w loads at the other widths"; \
	done
	bash tools/i18n_test.sh build/host/cosmocorgi_headless cosmocorgi build/cosmocorgi-i18n 320 $(WIDTHS)

cosmocorgi-screenshots:
	+$(COSMOCORGI_MAKE) build/host/cosmocorgi_headless
	sh $(COSMOCORGI_DIR)/tests/ui_test.sh build/host/cosmocorgi_headless $(COSMOCORGI_DIR)/docs/screenshots 2 all

cosmocorgi-art:
	$(PYTHON) $(COSMOCORGI_DIR)/tools/make_art.py

cosmocorgi-music-alt:
	$(PYTHON) $(COSMOCORGI_DIR)/tools/make_music_alt.py
