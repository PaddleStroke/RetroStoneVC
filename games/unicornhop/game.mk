# Unicorn Hop (games/unicornhop): its make targets, included by the root Makefile.
#
#   make unicornhop                host builds: unicornhop (SDL2), unicornhop_headless, unicornhop_libretro.so
#   make unicornhop-check          all its tests (the rules alone, libretro loader, smoke + bot + determinism, UI
#                                  screens, save states and their audit, the 22 languages), at 320 and at each
#                                  widescreen width of WIDTHS (427 520), and save states across widths
#   make unicornhop-dist           dist/windows/UnicornHop.exe, dist/libretro/unicornhop_libretro.so (+ .armhf.so)
#   make unicornhop-windows        only the Windows exe
#   make unicornhop-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make unicornhop-screenshots    games/unicornhop/docs/screenshots/
#   make unicornhop-art            redraw the code-drawn art (tools/make_art.py; commit art/)
#   make unicornhop-music-alt      the two alternative tunes (tools/make_music_alt.py -> music-alt/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/unicornhop/LICENSE; the rules below follow the root Makefile.

UNICORNHOP_MAKE = $(MAKE) --no-print-directory GAME=unicornhop GAME_NAME=UnicornHop
UNICORNHOP_DIR  = games/unicornhop
# the house style kit (games/common/src, docs/art-direction.md) is compiled in
HOUSE_UI_unicornhop = 1
# widescreen (docs/spec.md "Widescreen"; rs_game.max_width): the checks run at 320 and again at each width of WIDTHS
WIDESCREEN_unicornhop = 1

.PHONY: unicornhop unicornhop-check unicornhop-windows unicornhop-armhf unicornhop-dist unicornhop-screenshots \
        unicornhop-art unicornhop-music-alt

unicornhop:
	+$(UNICORNHOP_MAKE) host
unicornhop-windows:
	+$(UNICORNHOP_MAKE) windows
unicornhop-armhf:
	+$(UNICORNHOP_MAKE) armhf
unicornhop-dist:
	+$(UNICORNHOP_MAKE) dist

UNICORNHOP_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(UNICORNHOP_DIR)/src/*.c) $(HOUSE_SRC))

# the rules' test links play.c alone
build/host/unicornhop_rules: $(UNICORNHOP_DIR)/src/play.c $(UNICORNHOP_DIR)/src/game.h $(UNICORNHOP_DIR)/tests/test_rules.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -I$(UNICORNHOP_DIR)/src $(UNICORNHOP_DIR)/src/play.c \
	    $(UNICORNHOP_DIR)/tests/test_rules.c -o $@

unicornhop-check:
	+$(UNICORNHOP_MAKE) host build/host/test_libretro build/host/unicornhop_test_states build/host/unicornhop_rules
	./build/host/unicornhop_rules
	./build/host/test_libretro build/host/unicornhop_libretro.so 600
	sh $(UNICORNHOP_DIR)/tests/smoke_test.sh build/host/unicornhop_headless build
	sh $(UNICORNHOP_DIR)/tests/ui_test.sh build/host/unicornhop_headless build/unicornhop-ui
	sh $(UNICORNHOP_DIR)/tests/state_test.sh build/host/unicornhop_test_states build/states
	$(PYTHON) tools/state_audit.py --game unicornhop build/host/unicornhop_test_states $(UNICORNHOP_OBJ_HOST)
	set -e; for w in $(WIDTHS); do echo "== widescreen: $$w x 240"; \
	    a=$$(case $$w in 427) echo 16:9;; 520) echo Wide;; esac); \
	    [ -z "$$a" ] || ./build/host/test_libretro build/host/unicornhop_libretro.so 600 $$a $$w; \
	    RS_WIDTH=$$w sh $(UNICORNHOP_DIR)/tests/smoke_test.sh build/host/unicornhop_headless build/w$$w; \
	    RS_WIDTH=$$w sh $(UNICORNHOP_DIR)/tests/ui_test.sh build/host/unicornhop_headless build/unicornhop-ui-$$w; \
	    RS_WIDTH=$$w sh $(UNICORNHOP_DIR)/tests/state_test.sh build/host/unicornhop_test_states build/states-$$w; \
	    for o in 320 $(WIDTHS); do [ $$o = $$w ] || ./build/host/unicornhop_test_states --width $$o --opt bot=1 --opt ready=1 \
	        --input build/states-$$w/unicornhop-run.input --resume build/states-$$w/unicornhop-run --any-width | grep -q "all passed" \
	        || { echo "FAIL a state saved at $$w does not load at $$o"; exit 1; }; done; \
	    echo "  ok   a state saved at $$w loads at the other widths"; \
	done
	bash tools/i18n_test.sh build/host/unicornhop_headless unicornhop build/unicornhop-i18n 320 $(WIDTHS)

unicornhop-screenshots:
	+$(UNICORNHOP_MAKE) build/host/unicornhop_headless
	sh $(UNICORNHOP_DIR)/tests/ui_test.sh build/host/unicornhop_headless $(UNICORNHOP_DIR)/docs/screenshots 2 all

unicornhop-art:
	$(PYTHON) $(UNICORNHOP_DIR)/tools/make_art.py

unicornhop-music-alt:
	$(PYTHON) $(UNICORNHOP_DIR)/tools/make_music_alt.py
