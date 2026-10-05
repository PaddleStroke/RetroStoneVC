# Moth Around: house kit, portable host/libretro and static Windows builds.
HOUSE_UI_motharound = 1
# widescreen (docs/spec.md "Widescreen"; rs_game.max_width; DESIGN.md): the checks run at 320 and at each width of
# WIDTHS (427 520), then across widths (tests/wide_test.sh)
WIDESCREEN_motharound = 1
MOTHAROUND_MAKE = $(MAKE) --no-print-directory GAME=motharound GAME_NAME=MothAround
MOTHAROUND_OBJS = $(patsubst %.c,build/host/%.o,$(wildcard games/motharound/src/*.c) $(HOUSE_SRC))
.PHONY: motharound motharound-windows motharound-armhf motharound-dist motharound-art motharound-check motharound-screenshots
motharound:
	+$(MOTHAROUND_MAKE) host
motharound-windows:
	+$(MOTHAROUND_MAKE) windows
motharound-armhf:
	+$(MOTHAROUND_MAKE) armhf
motharound-dist:
	+$(MOTHAROUND_MAKE) dist
motharound-art:
	$(PYTHON) games/motharound/tools/make_art.py
build/host/motharound_physics: games/motharound/src/play.c games/motharound/src/game.h games/motharound/src/sine.h games/motharound/tests/test_physics.c
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -Igames/motharound/src games/motharound/src/play.c games/motharound/tests/test_physics.c -o $@
build/host/motharound_views: games/motharound/tests/test_views.c $(MOTHAROUND_OBJS) build/host/build/gen/motharound/assets.o build/host/build/gen/motharound/game_text.o build/host/librs.a
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -Igames/motharound/src -Igames/common/src $^ -lm -o $@
build/host/motharound_review: games/motharound/tests/review_scenes.c $(MOTHAROUND_OBJS) build/host/build/gen/motharound/assets.o build/host/build/gen/motharound/game_text.o build/host/sdk/frontends/common/rs_desktop.o build/host/librs.a
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -Isdk/frontends/common -Igames/motharound/src -Igames/common/src $^ -lm -o $@
motharound-check:
	+$(MOTHAROUND_MAKE) host build/host/test_libretro build/host/motharound_test_states build/host/motharound_physics build/host/motharound_views
	./build/host/motharound_physics
	./build/host/motharound_views
	./build/host/test_libretro build/host/motharound_libretro.so 600
	$(PYTHON) games/motharound/tests/check_game.py
	sh games/motharound/tests/state_test.sh build/host/motharound_test_states build/states
	$(PYTHON) tools/state_audit.py --game motharound build/host/motharound_test_states $(MOTHAROUND_OBJS)
	set -e; for w in $(WIDTHS); do echo "== widescreen: $$w x 240"; \
	    a=$$(case $$w in 427) echo 16:9;; 520) echo Wide;; esac); \
	    [ -z "$$a" ] || ./build/host/test_libretro build/host/motharound_libretro.so 600 $$a $$w; \
	    RS_WIDTH=$$w $(PYTHON) games/motharound/tests/check_game.py; \
	    RS_WIDTH=$$w sh games/motharound/tests/state_test.sh build/host/motharound_test_states build/states-$$w; \
	done
	sh games/motharound/tests/wide_test.sh build/host/motharound_headless build/host/motharound_test_states build "$(WIDTHS)"
	bash tools/i18n_test.sh build/host/motharound_headless motharound build/motharound-i18n $(if $(WIDESCREEN_motharound),320 $(WIDTHS),320)
motharound-screenshots:
	+$(MOTHAROUND_MAKE) build/host/motharound_headless
	$(PYTHON) games/motharound/tests/check_game.py --screenshots
	sh games/motharound/tools/wide_shots.sh build/host/motharound_headless games/motharound/docs/screenshots
