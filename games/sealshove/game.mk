# Seal Shove (games/sealshove): its make targets, included by the root Makefile.
#
#   make sealshove                host builds: sealshove (SDL2), sealshove_headless, sealshove_libretro.so
#   make sealshove-check          all its tests (the rules alone, libretro loader, smoke + bot + determinism, touch,
#                                 UI screens, save states and their audit, the 22 languages), at 320 and at each
#                                 widescreen width of WIDTHS (427 520), and save states across widths
#   make sealshove-dist           dist/windows/SealShove.exe, dist/libretro/sealshove_libretro.so (+ .armhf.so)
#   make sealshove-windows        only the Windows exe
#   make sealshove-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make sealshove-screenshots    games/sealshove/docs/screenshots/
#   make sealshove-art            redraw the code-drawn art (tools/make_art.py; commit art/)
#   make sealshove-music-alt      the two alternative tunes (tools/make_music_alt.py -> music-alt/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/sealshove/LICENSE; the rules below follow the root Makefile.

SEALSHOVE_MAKE = $(MAKE) --no-print-directory GAME=sealshove GAME_NAME=SealShove
SEALSHOVE_DIR  = games/sealshove
# the house style kit (games/common/src, docs/art-direction.md) is compiled in
HOUSE_UI_sealshove = 1
# widescreen (docs/spec.md "Widescreen"; rs_game.max_width): the checks run at 320 and again at each width of WIDTHS
WIDESCREEN_sealshove = 1

.PHONY: sealshove sealshove-check sealshove-windows sealshove-armhf sealshove-dist sealshove-screenshots sealshove-art \
        sealshove-music-alt

sealshove:
	+$(SEALSHOVE_MAKE) host
sealshove-windows:
	+$(SEALSHOVE_MAKE) windows
sealshove-armhf:
	+$(SEALSHOVE_MAKE) armhf
sealshove-dist:
	+$(SEALSHOVE_MAKE) dist

SEALSHOVE_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(SEALSHOVE_DIR)/src/*.c) $(HOUSE_SRC))

# the rules' test links play.c alone
build/host/sealshove_rules: $(SEALSHOVE_DIR)/src/play.c $(SEALSHOVE_DIR)/src/game.h $(SEALSHOVE_DIR)/tests/test_rules.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -I$(SEALSHOVE_DIR)/src $(SEALSHOVE_DIR)/src/play.c \
	    $(SEALSHOVE_DIR)/tests/test_rules.c -o $@

sealshove-check:
	+$(SEALSHOVE_MAKE) host build/host/test_libretro build/host/sealshove_test_states build/host/sealshove_rules
	./build/host/sealshove_rules
	./build/host/test_libretro build/host/sealshove_libretro.so 600
	sh $(SEALSHOVE_DIR)/tests/smoke_test.sh build/host/sealshove_headless build
	sh $(SEALSHOVE_DIR)/tests/touch_test.sh build/host/sealshove_headless build "$(WIDTHS)"
	sh $(SEALSHOVE_DIR)/tests/ui_test.sh build/host/sealshove_headless build/sealshove-ui
	sh $(SEALSHOVE_DIR)/tests/state_test.sh build/host/sealshove_test_states build/states
	$(PYTHON) tools/state_audit.py --game sealshove build/host/sealshove_test_states $(SEALSHOVE_OBJ_HOST)
	set -e; for w in $(WIDTHS); do echo "== widescreen: $$w x 240"; \
	    a=$$(case $$w in 427) echo 16:9;; 520) echo Wide;; esac); \
	    [ -z "$$a" ] || ./build/host/test_libretro build/host/sealshove_libretro.so 600 $$a $$w; \
	    RS_WIDTH=$$w sh $(SEALSHOVE_DIR)/tests/smoke_test.sh build/host/sealshove_headless build/w$$w quick; \
	    RS_WIDTH=$$w sh $(SEALSHOVE_DIR)/tests/ui_test.sh build/host/sealshove_headless build/sealshove-ui-$$w; \
	    RS_WIDTH=$$w sh $(SEALSHOVE_DIR)/tests/state_test.sh build/host/sealshove_test_states build/states-$$w quick; \
	    for o in 320 $(WIDTHS); do [ $$o = $$w ] || ./build/host/sealshove_test_states --width $$o --opt bot=1 \
	        --opt ready=1 --opt seed=3 --input build/states-$$w/sealshove-run.input --resume build/states-$$w/sealshove-run \
	        --any-width | grep -q "all passed" || { echo "FAIL a state saved at $$w does not load at $$o"; exit 1; }; done; \
	    echo "  ok   a state saved at $$w loads at the other widths"; \
	done
	bash tools/i18n_test.sh build/host/sealshove_headless sealshove build/sealshove-i18n 320 $(WIDTHS)

sealshove-screenshots:
	+$(SEALSHOVE_MAKE) build/host/sealshove_headless
	sh $(SEALSHOVE_DIR)/tests/ui_test.sh build/host/sealshove_headless $(SEALSHOVE_DIR)/docs/screenshots 2 all

sealshove-art:
	$(PYTHON) $(SEALSHOVE_DIR)/tools/make_art.py

sealshove-music-alt:
	$(PYTHON) $(SEALSHOVE_DIR)/tools/make_music_alt.py
