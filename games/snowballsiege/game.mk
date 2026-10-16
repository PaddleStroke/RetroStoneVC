# Snowball Siege (games/snowballsiege): its make targets, included by the root Makefile.
#
#   make snowballsiege                host builds: snowballsiege (SDL2), snowballsiege_headless, snowballsiege_libretro.so
#   make snowballsiege-check          all its tests (the rules alone, libretro loader, smoke + bot + determinism, touch,
#                                     UI screens, save states and their audit, the 22 languages), at 320 and at each
#                                     widescreen width of WIDTHS (427 520), and save states across widths
#   make snowballsiege-dist           dist/windows/SnowballSiege.exe, dist/libretro/snowballsiege_libretro.so (+ .armhf.so)
#   make snowballsiege-windows        only the Windows exe
#   make snowballsiege-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make snowballsiege-screenshots    games/snowballsiege/docs/screenshots/
#   make snowballsiege-art            redraw the code-drawn art (tools/make_art.py; commit art/)
#   make snowballsiege-music-alt      the two alternative tunes (tools/make_music_alt.py -> music-alt/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/snowballsiege/LICENSE; the rules below follow the root
# Makefile.

SNOWBALLSIEGE_MAKE = $(MAKE) --no-print-directory GAME=snowballsiege GAME_NAME=SnowballSiege
SNOWBALLSIEGE_DIR  = games/snowballsiege
# the house style kit (games/common/src, docs/art-direction.md) is compiled in
HOUSE_UI_snowballsiege = 1
# widescreen (docs/spec.md "Widescreen"; rs_game.max_width): the checks run at 320 and again at each width of WIDTHS
WIDESCREEN_snowballsiege = 1

.PHONY: snowballsiege snowballsiege-check snowballsiege-windows snowballsiege-armhf snowballsiege-dist \
        snowballsiege-screenshots snowballsiege-art snowballsiege-music-alt

snowballsiege:
	+$(SNOWBALLSIEGE_MAKE) host
snowballsiege-windows:
	+$(SNOWBALLSIEGE_MAKE) windows
snowballsiege-armhf:
	+$(SNOWBALLSIEGE_MAKE) armhf
snowballsiege-dist:
	+$(SNOWBALLSIEGE_MAKE) dist

SNOWBALLSIEGE_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(SNOWBALLSIEGE_DIR)/src/*.c) $(HOUSE_SRC))

# the rules' test links play.c alone
build/host/snowballsiege_rules: $(SNOWBALLSIEGE_DIR)/src/play.c $(SNOWBALLSIEGE_DIR)/src/game.h \
                                $(SNOWBALLSIEGE_DIR)/tests/test_rules.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -I$(SNOWBALLSIEGE_DIR)/src $(SNOWBALLSIEGE_DIR)/src/play.c \
	    $(SNOWBALLSIEGE_DIR)/tests/test_rules.c -o $@

snowballsiege-check:
	+$(SNOWBALLSIEGE_MAKE) host build/host/test_libretro build/host/snowballsiege_test_states \
	    build/host/snowballsiege_rules
	./build/host/snowballsiege_rules
	./build/host/test_libretro build/host/snowballsiege_libretro.so 600
	sh $(SNOWBALLSIEGE_DIR)/tests/smoke_test.sh build/host/snowballsiege_headless build
	sh $(SNOWBALLSIEGE_DIR)/tests/touch_test.sh build/host/snowballsiege_headless build "$(WIDTHS)"
	sh $(SNOWBALLSIEGE_DIR)/tests/ui_test.sh build/host/snowballsiege_headless build/snowballsiege-ui
	sh $(SNOWBALLSIEGE_DIR)/tests/state_test.sh build/host/snowballsiege_test_states build/states
	$(PYTHON) tools/state_audit.py --game snowballsiege build/host/snowballsiege_test_states $(SNOWBALLSIEGE_OBJ_HOST)
	set -e; for w in $(WIDTHS); do echo "== widescreen: $$w x 240"; \
	    a=$$(case $$w in 427) echo 16:9;; 520) echo Wide;; esac); \
	    [ -z "$$a" ] || ./build/host/test_libretro build/host/snowballsiege_libretro.so 600 $$a $$w; \
	    RS_WIDTH=$$w sh $(SNOWBALLSIEGE_DIR)/tests/smoke_test.sh build/host/snowballsiege_headless build/w$$w quick; \
	    RS_WIDTH=$$w sh $(SNOWBALLSIEGE_DIR)/tests/ui_test.sh build/host/snowballsiege_headless build/snowballsiege-ui-$$w; \
	    RS_WIDTH=$$w sh $(SNOWBALLSIEGE_DIR)/tests/state_test.sh build/host/snowballsiege_test_states build/states-$$w quick; \
	    for o in 320 $(WIDTHS); do [ $$o = $$w ] || ./build/host/snowballsiege_test_states --width $$o --opt bot=1 \
	        --opt ready=1 --input build/states-$$w/snowballsiege-run.input --resume build/states-$$w/snowballsiege-run \
	        --any-width | grep -q "all passed" || { echo "FAIL a state saved at $$w does not load at $$o"; exit 1; }; done; \
	    echo "  ok   a state saved at $$w loads at the other widths"; \
	done
	bash tools/i18n_test.sh build/host/snowballsiege_headless snowballsiege build/snowballsiege-i18n 320 $(WIDTHS)

snowballsiege-screenshots:
	+$(SNOWBALLSIEGE_MAKE) build/host/snowballsiege_headless
	sh $(SNOWBALLSIEGE_DIR)/tests/ui_test.sh build/host/snowballsiege_headless $(SNOWBALLSIEGE_DIR)/docs/screenshots 2 all

snowballsiege-art:
	$(PYTHON) $(SNOWBALLSIEGE_DIR)/tools/make_art.py

snowballsiege-music-alt:
	$(PYTHON) $(SNOWBALLSIEGE_DIR)/tools/make_music_alt.py
