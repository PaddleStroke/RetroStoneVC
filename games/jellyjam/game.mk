# Jelly Jam (games/jellyjam): its make targets, included by the root Makefile.
#
#   make jellyjam                host builds: jellyjam (SDL2), jellyjam_headless, jellyjam_libretro.so
#   make jellyjam-check          all its tests (the rules alone, libretro loader, smoke + bot + determinism, touch,
#                                UI screens, save states and their audit, the 22 languages), at 320 and at each
#                                widescreen width of WIDTHS (427 520), and save states across widths
#   make jellyjam-dist           dist/windows/JellyJam.exe, dist/libretro/jellyjam_libretro.so (+ .armhf.so)
#   make jellyjam-windows        only the Windows exe
#   make jellyjam-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make jellyjam-screenshots    games/jellyjam/docs/screenshots/
#   make jellyjam-art            redraw the code-drawn art (tools/make_art.py; commit art/)
#   make jellyjam-music-alt      the two alternative tunes (tools/make_music_alt.py -> music-alt/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/jellyjam/LICENSE; the rules below follow the root Makefile.

JELLYJAM_MAKE = $(MAKE) --no-print-directory GAME=jellyjam GAME_NAME=JellyJam
JELLYJAM_DIR  = games/jellyjam
# the house style kit (games/common/src, docs/art-direction.md) is compiled in
HOUSE_UI_jellyjam = 1
# widescreen (docs/spec.md "Widescreen"; rs_game.max_width): the checks run at 320 and again at each width of WIDTHS
WIDESCREEN_jellyjam = 1

.PHONY: jellyjam jellyjam-check jellyjam-windows jellyjam-armhf jellyjam-dist jellyjam-screenshots jellyjam-art \
        jellyjam-music-alt

jellyjam:
	+$(JELLYJAM_MAKE) host
jellyjam-windows:
	+$(JELLYJAM_MAKE) windows
jellyjam-armhf:
	+$(JELLYJAM_MAKE) armhf
jellyjam-dist:
	+$(JELLYJAM_MAKE) dist

JELLYJAM_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(JELLYJAM_DIR)/src/*.c) $(HOUSE_SRC))

# the rules' test links play.c alone
build/host/jellyjam_rules: $(JELLYJAM_DIR)/src/play.c $(JELLYJAM_DIR)/src/game.h $(JELLYJAM_DIR)/tests/test_rules.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -I$(JELLYJAM_DIR)/src $(JELLYJAM_DIR)/src/play.c \
	    $(JELLYJAM_DIR)/tests/test_rules.c -o $@

jellyjam-check:
	+$(JELLYJAM_MAKE) host build/host/test_libretro build/host/jellyjam_test_states build/host/jellyjam_rules
	./build/host/jellyjam_rules
	./build/host/test_libretro build/host/jellyjam_libretro.so 600
	sh $(JELLYJAM_DIR)/tests/smoke_test.sh build/host/jellyjam_headless build
	sh $(JELLYJAM_DIR)/tests/touch_test.sh build/host/jellyjam_headless build "$(WIDTHS)"
	sh $(JELLYJAM_DIR)/tests/ui_test.sh build/host/jellyjam_headless build/jellyjam-ui
	sh $(JELLYJAM_DIR)/tests/state_test.sh build/host/jellyjam_test_states build/states
	$(PYTHON) tools/state_audit.py --game jellyjam build/host/jellyjam_test_states $(JELLYJAM_OBJ_HOST)
	set -e; for w in $(WIDTHS); do echo "== widescreen: $$w x 240"; \
	    a=$$(case $$w in 427) echo 16:9;; 520) echo Wide;; esac); \
	    [ -z "$$a" ] || ./build/host/test_libretro build/host/jellyjam_libretro.so 600 $$a $$w; \
	    RS_WIDTH=$$w sh $(JELLYJAM_DIR)/tests/smoke_test.sh build/host/jellyjam_headless build/w$$w quick; \
	    RS_WIDTH=$$w sh $(JELLYJAM_DIR)/tests/ui_test.sh build/host/jellyjam_headless build/jellyjam-ui-$$w; \
	    RS_WIDTH=$$w sh $(JELLYJAM_DIR)/tests/state_test.sh build/host/jellyjam_test_states build/states-$$w quick; \
	    for o in 320 $(WIDTHS); do [ $$o = $$w ] || ./build/host/jellyjam_test_states --width $$o --opt bot=1 \
	        --opt ready=1 --input build/states-$$w/jellyjam-run.input --resume build/states-$$w/jellyjam-run \
	        --any-width | grep -q "all passed" || { echo "FAIL a state saved at $$w does not load at $$o"; exit 1; }; done; \
	    echo "  ok   a state saved at $$w loads at the other widths"; \
	done
	bash tools/i18n_test.sh build/host/jellyjam_headless jellyjam build/jellyjam-i18n 320 $(WIDTHS)

jellyjam-screenshots:
	+$(JELLYJAM_MAKE) build/host/jellyjam_headless
	sh $(JELLYJAM_DIR)/tests/ui_test.sh build/host/jellyjam_headless $(JELLYJAM_DIR)/docs/screenshots 2 all

jellyjam-art:
	$(PYTHON) $(JELLYJAM_DIR)/tools/make_art.py

jellyjam-music-alt:
	$(PYTHON) $(JELLYJAM_DIR)/tools/make_music_alt.py
