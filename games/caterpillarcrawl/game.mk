# Caterpillar Crawl (games/caterpillarcrawl): its make targets, included by the root Makefile.
#
#   make caterpillarcrawl                host builds: caterpillarcrawl (SDL2), caterpillarcrawl_headless, _libretro.so
#   make caterpillarcrawl-check          all its tests (the rules alone, libretro loader, smoke + bot + determinism,
#                                        touch, UI screens, save states and their audit, the 22 languages), at 320 and
#                                        at each widescreen width of WIDTHS (427 520), and save states across widths
#   make caterpillarcrawl-dist           dist/windows/CaterpillarCrawl.exe, dist/libretro/caterpillarcrawl_libretro.so
#   make caterpillarcrawl-windows        only the Windows exe
#   make caterpillarcrawl-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make caterpillarcrawl-screenshots    games/caterpillarcrawl/docs/screenshots/
#   make caterpillarcrawl-art            redraw the code-drawn art (tools/make_art.py; commit art/)
#   make caterpillarcrawl-music-alt      the two alternative tunes (tools/make_music_alt.py -> music-alt/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/caterpillarcrawl/LICENSE; the rules below follow the root
# Makefile.

CATERPILLARCRAWL_MAKE = $(MAKE) --no-print-directory GAME=caterpillarcrawl GAME_NAME=CaterpillarCrawl
CATERPILLARCRAWL_DIR  = games/caterpillarcrawl
# the house style kit (games/common/src, docs/art-direction.md) is compiled in
HOUSE_UI_caterpillarcrawl = 1
# widescreen (docs/spec.md "Widescreen"; rs_game.max_width): the checks run at 320 and again at each width of WIDTHS
WIDESCREEN_caterpillarcrawl = 1

.PHONY: caterpillarcrawl caterpillarcrawl-check caterpillarcrawl-windows caterpillarcrawl-armhf caterpillarcrawl-dist \
        caterpillarcrawl-screenshots caterpillarcrawl-art caterpillarcrawl-music-alt

caterpillarcrawl:
	+$(CATERPILLARCRAWL_MAKE) host
caterpillarcrawl-windows:
	+$(CATERPILLARCRAWL_MAKE) windows
caterpillarcrawl-armhf:
	+$(CATERPILLARCRAWL_MAKE) armhf
caterpillarcrawl-dist:
	+$(CATERPILLARCRAWL_MAKE) dist

CATERPILLARCRAWL_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(CATERPILLARCRAWL_DIR)/src/*.c) $(HOUSE_SRC))

# the rules' test links play.c alone
build/host/caterpillarcrawl_rules: $(CATERPILLARCRAWL_DIR)/src/play.c $(CATERPILLARCRAWL_DIR)/src/game.h \
                                   $(CATERPILLARCRAWL_DIR)/tests/test_rules.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -I$(CATERPILLARCRAWL_DIR)/src $(CATERPILLARCRAWL_DIR)/src/play.c \
	    $(CATERPILLARCRAWL_DIR)/tests/test_rules.c -o $@

caterpillarcrawl-check:
	+$(CATERPILLARCRAWL_MAKE) host build/host/test_libretro build/host/caterpillarcrawl_test_states \
	    build/host/caterpillarcrawl_rules
	./build/host/caterpillarcrawl_rules
	./build/host/test_libretro build/host/caterpillarcrawl_libretro.so 600
	sh $(CATERPILLARCRAWL_DIR)/tests/smoke_test.sh build/host/caterpillarcrawl_headless build
	sh $(CATERPILLARCRAWL_DIR)/tests/touch_test.sh build/host/caterpillarcrawl_headless build "$(WIDTHS)"
	sh $(CATERPILLARCRAWL_DIR)/tests/ui_test.sh build/host/caterpillarcrawl_headless build/caterpillarcrawl-ui
	sh $(CATERPILLARCRAWL_DIR)/tests/state_test.sh build/host/caterpillarcrawl_test_states build/states
	$(PYTHON) tools/state_audit.py --game caterpillarcrawl build/host/caterpillarcrawl_test_states \
	    $(CATERPILLARCRAWL_OBJ_HOST)
	set -e; for w in $(WIDTHS); do echo "== widescreen: $$w x 240"; \
	    a=$$(case $$w in 427) echo 16:9;; 520) echo Wide;; esac); \
	    [ -z "$$a" ] || ./build/host/test_libretro build/host/caterpillarcrawl_libretro.so 600 $$a $$w; \
	    RS_WIDTH=$$w sh $(CATERPILLARCRAWL_DIR)/tests/smoke_test.sh build/host/caterpillarcrawl_headless build/w$$w quick; \
	    RS_WIDTH=$$w sh $(CATERPILLARCRAWL_DIR)/tests/ui_test.sh build/host/caterpillarcrawl_headless \
	        build/caterpillarcrawl-ui-$$w; \
	    RS_WIDTH=$$w sh $(CATERPILLARCRAWL_DIR)/tests/state_test.sh build/host/caterpillarcrawl_test_states \
	        build/states-$$w quick; \
	    for o in 320 $(WIDTHS); do [ $$o = $$w ] || ./build/host/caterpillarcrawl_test_states --width $$o --opt bot=1 \
	        --opt ready=1 --input build/states-$$w/caterpillarcrawl-run.input \
	        --resume build/states-$$w/caterpillarcrawl-run --any-width | grep -q "all passed" \
	        || { echo "FAIL a state saved at $$w does not load at $$o"; exit 1; }; done; \
	    echo "  ok   a state saved at $$w loads at the other widths"; \
	done
	bash tools/i18n_test.sh build/host/caterpillarcrawl_headless caterpillarcrawl build/caterpillarcrawl-i18n 320 $(WIDTHS)

caterpillarcrawl-screenshots:
	+$(CATERPILLARCRAWL_MAKE) build/host/caterpillarcrawl_headless
	sh $(CATERPILLARCRAWL_DIR)/tests/ui_test.sh build/host/caterpillarcrawl_headless \
	    $(CATERPILLARCRAWL_DIR)/docs/screenshots 2 all

caterpillarcrawl-art:
	$(PYTHON) $(CATERPILLARCRAWL_DIR)/tools/make_art.py

caterpillarcrawl-music-alt:
	$(PYTHON) $(CATERPILLARCRAWL_DIR)/tools/make_music_alt.py
