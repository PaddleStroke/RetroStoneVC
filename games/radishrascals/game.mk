# Radish Rascals: house kit, portable host/libretro and static Windows builds.
HOUSE_UI_radishrascals = 1
# widescreen (docs/spec.md "Widescreen"; rs_game.max_width; DESIGN.md): the checks run at 320 and at each width of
# WIDTHS (427 520), then across widths (tests/wide_test.sh); test_views checks the views at 320, 427, 520 and 560
WIDESCREEN_radishrascals = 1
RADISHRASCALS_MAKE = $(MAKE) --no-print-directory GAME=radishrascals GAME_NAME=RadishRascals
RADISHRASCALS_OBJS = $(patsubst %.c,build/host/%.o,$(wildcard games/radishrascals/src/*.c) $(HOUSE_SRC))
.PHONY: radishrascals radishrascals-windows radishrascals-armhf radishrascals-dist radishrascals-art radishrascals-check radishrascals-screenshots
radishrascals:
	+$(RADISHRASCALS_MAKE) host
radishrascals-windows:
	+$(RADISHRASCALS_MAKE) windows
radishrascals-armhf:
	+$(RADISHRASCALS_MAKE) armhf
radishrascals-dist:
	+$(RADISHRASCALS_MAKE) dist
radishrascals-art:
	$(PYTHON) games/radishrascals/tools/make_art.py
build/host/radishrascals_physics: games/radishrascals/src/play.c games/radishrascals/src/game.h games/radishrascals/src/sine.h games/radishrascals/tests/test_physics.c
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -Igames/radishrascals/src games/radishrascals/src/play.c games/radishrascals/tests/test_physics.c -o $@
build/host/radishrascals_views: games/radishrascals/tests/test_views.c $(RADISHRASCALS_OBJS) build/host/build/gen/radishrascals/assets.o build/host/build/gen/radishrascals/game_text.o build/host/librs.a
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -Igames/radishrascals/src -Igames/common/src -Ibuild/gen/radishrascals $^ -lm -o $@
radishrascals-check:
	+$(RADISHRASCALS_MAKE) host build/host/test_libretro build/host/radishrascals_test_states build/host/radishrascals_physics build/host/radishrascals_views
	./build/host/radishrascals_physics
	./build/host/radishrascals_views
	./build/host/test_libretro build/host/radishrascals_libretro.so 600
	$(PYTHON) games/radishrascals/tests/check_game.py
	sh games/radishrascals/tests/state_test.sh build/host/radishrascals_test_states build/states
	$(PYTHON) tools/state_audit.py --game radishrascals build/host/radishrascals_test_states $(RADISHRASCALS_OBJS)
	bash tools/i18n_test.sh build/host/radishrascals_headless radishrascals build/radishrascals-i18n $(if $(WIDESCREEN_radishrascals),320 $(WIDTHS),320)
	set -e; for w in $(WIDTHS); do echo "== Radish Rascals at $$w x 240"; \
	    a=$$(case $$w in 427) echo 16:9;; 520) echo Wide;; esac); \
	    [ -z "$$a" ] || ./build/host/test_libretro build/host/radishrascals_libretro.so 600 $$a $$w; \
	    RS_WIDTH=$$w sh games/radishrascals/tests/smoke_test.sh; \
	    RS_WIDTH=$$w sh games/radishrascals/tests/state_test.sh build/host/radishrascals_test_states build/radishrascals-states-$$w; \
	done
	sh games/radishrascals/tests/wide_test.sh build/host/radishrascals_headless build/host/radishrascals_test_states build "$(WIDTHS)"
radishrascals-screenshots:
	+$(RADISHRASCALS_MAKE) build/host/radishrascals_headless
	$(PYTHON) games/radishrascals/tests/check_game.py --screenshots
	set -e; for w in $(WIDTHS); do RS_WIDTH=$$w $(PYTHON) games/radishrascals/tests/check_game.py --screenshots; done
