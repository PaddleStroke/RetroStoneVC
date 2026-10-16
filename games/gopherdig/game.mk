# Gopher Dig (games/gopherdig): its make targets, included by the root Makefile.
#
#   make gopherdig                host builds: gopherdig (SDL2), gopherdig_headless, gopherdig_libretro.so
#   make gopherdig-check          all its tests (the rules alone, libretro loader, smoke + bot + determinism, touch,
#                                  UI screens, save states and their audit, the 22 languages), at 320 and at each
#                                  widescreen width of WIDTHS (427 520), and save states across widths
#   make gopherdig-dist           dist/windows/GopherDig.exe, dist/libretro/gopherdig_libretro.so (+ .armhf.so)
#   make gopherdig-windows        only the Windows exe
#   make gopherdig-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make gopherdig-screenshots    games/gopherdig/docs/screenshots/
#   make gopherdig-art            redraw the code-drawn art (tools/make_art.py; commit art/)
#   make gopherdig-music-alt      the two alternative tunes (tools/make_music_alt.py -> music-alt/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/gopherdig/LICENSE; the rules below follow the root Makefile.

GOPHERDIG_MAKE = $(MAKE) --no-print-directory GAME=gopherdig GAME_NAME=GopherDig
GOPHERDIG_DIR  = games/gopherdig
# the house style kit (games/common/src, docs/art-direction.md) is compiled in
HOUSE_UI_gopherdig = 1
# widescreen (docs/spec.md "Widescreen"; rs_game.max_width): the checks run at 320 and again at each width of WIDTHS
WIDESCREEN_gopherdig = 1

.PHONY: gopherdig gopherdig-check gopherdig-windows gopherdig-armhf gopherdig-dist gopherdig-screenshots \
        gopherdig-art gopherdig-music-alt

gopherdig:
	+$(GOPHERDIG_MAKE) host
gopherdig-windows:
	+$(GOPHERDIG_MAKE) windows
gopherdig-armhf:
	+$(GOPHERDIG_MAKE) armhf
gopherdig-dist:
	+$(GOPHERDIG_MAKE) dist

GOPHERDIG_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(GOPHERDIG_DIR)/src/*.c) $(HOUSE_SRC))

# the rules' test links play.c alone
build/host/gopherdig_rules: $(GOPHERDIG_DIR)/src/play.c $(GOPHERDIG_DIR)/src/game.h $(GOPHERDIG_DIR)/tests/test_rules.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -I$(GOPHERDIG_DIR)/src $(GOPHERDIG_DIR)/src/play.c \
	    $(GOPHERDIG_DIR)/tests/test_rules.c -o $@

gopherdig-check:
	+$(GOPHERDIG_MAKE) host build/host/test_libretro build/host/gopherdig_test_states build/host/gopherdig_rules
	./build/host/gopherdig_rules
	./build/host/test_libretro build/host/gopherdig_libretro.so 600
	sh $(GOPHERDIG_DIR)/tests/smoke_test.sh build/host/gopherdig_headless build
	sh $(GOPHERDIG_DIR)/tests/touch_test.sh build/host/gopherdig_headless build "$(WIDTHS)"
	sh $(GOPHERDIG_DIR)/tests/ui_test.sh build/host/gopherdig_headless build/gopherdig-ui
	sh $(GOPHERDIG_DIR)/tests/state_test.sh build/host/gopherdig_test_states build/states
	$(PYTHON) tools/state_audit.py --game gopherdig build/host/gopherdig_test_states $(GOPHERDIG_OBJ_HOST)
	set -e; for w in $(WIDTHS); do echo "== widescreen: $$w x 240"; \
	    a=$$(case $$w in 427) echo 16:9;; 520) echo Wide;; esac); \
	    [ -z "$$a" ] || ./build/host/test_libretro build/host/gopherdig_libretro.so 600 $$a $$w; \
	    RS_WIDTH=$$w sh $(GOPHERDIG_DIR)/tests/smoke_test.sh build/host/gopherdig_headless build/w$$w quick; \
	    RS_WIDTH=$$w sh $(GOPHERDIG_DIR)/tests/ui_test.sh build/host/gopherdig_headless build/gopherdig-ui-$$w; \
	    RS_WIDTH=$$w sh $(GOPHERDIG_DIR)/tests/state_test.sh build/host/gopherdig_test_states build/states-$$w quick; \
	    for o in 320 $(WIDTHS); do [ $$o = $$w ] || ./build/host/gopherdig_test_states --width $$o --opt bot=1 \
	        --opt ready=1 --input build/states-$$w/gopherdig-run.input --resume build/states-$$w/gopherdig-run \
	        --any-width | grep -q "all passed" || { echo "FAIL a state saved at $$w does not load at $$o"; exit 1; }; done; \
	    echo "  ok   a state saved at $$w loads at the other widths"; \
	done
	bash tools/i18n_test.sh build/host/gopherdig_headless gopherdig build/gopherdig-i18n 320 $(WIDTHS)

gopherdig-screenshots:
	+$(GOPHERDIG_MAKE) build/host/gopherdig_headless
	sh $(GOPHERDIG_DIR)/tests/ui_test.sh build/host/gopherdig_headless $(GOPHERDIG_DIR)/docs/screenshots 2 all

gopherdig-art:
	$(PYTHON) $(GOPHERDIG_DIR)/tools/make_art.py

gopherdig-music-alt:
	$(PYTHON) $(GOPHERDIG_DIR)/tools/make_music_alt.py
