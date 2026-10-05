# Penguin Slalom: house kit, portable host/libretro and static Windows builds.
HOUSE_UI_penguinslalom = 1
# widescreen (docs/spec.md "Widescreen"; rs_game.max_width; DESIGN.md): the checks run at 320, then at each width of
# WIDTHS (427 520), then across widths (tests/wide_test.sh)
WIDESCREEN_penguinslalom = 1
PENGUINSLALOM_MAKE = $(MAKE) --no-print-directory GAME=penguinslalom GAME_NAME=PenguinSlalom
PENGUINSLALOM_OBJS = $(patsubst %.c,build/host/%.o,$(wildcard games/penguinslalom/src/*.c) $(HOUSE_SRC))
.PHONY: penguinslalom penguinslalom-windows penguinslalom-armhf penguinslalom-dist penguinslalom-art penguinslalom-check penguinslalom-screenshots
penguinslalom:
	+$(PENGUINSLALOM_MAKE) host
penguinslalom-windows:
	+$(PENGUINSLALOM_MAKE) windows
penguinslalom-armhf:
	+$(PENGUINSLALOM_MAKE) armhf
penguinslalom-dist:
	+$(PENGUINSLALOM_MAKE) dist
penguinslalom-art:
	$(PYTHON) games/penguinslalom/tools/make_art.py
build/host/penguinslalom_physics: games/penguinslalom/src/play.c games/penguinslalom/src/game.h games/penguinslalom/src/sine.h games/penguinslalom/tests/test_physics.c
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -Igames/penguinslalom/src games/penguinslalom/src/play.c games/penguinslalom/tests/test_physics.c -o $@
build/host/penguinslalom_views: games/penguinslalom/tests/test_views.c $(PENGUINSLALOM_OBJS) build/host/build/gen/penguinslalom/assets.o build/host/build/gen/penguinslalom/game_text.o build/host/librs.a
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -Igames/penguinslalom/src -Igames/common/src $^ -lm -o $@
build/host/penguinslalom_review: games/penguinslalom/tests/review_scenes.c $(PENGUINSLALOM_OBJS) build/host/build/gen/penguinslalom/assets.o build/host/build/gen/penguinslalom/game_text.o build/host/sdk/frontends/common/rs_desktop.o build/host/librs.a
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -Isdk/frontends/common -Igames/penguinslalom/src -Igames/common/src $^ -lm -o $@
penguinslalom-check:
	+$(PENGUINSLALOM_MAKE) host build/host/test_libretro build/host/penguinslalom_test_states build/host/penguinslalom_physics build/host/penguinslalom_views
	./build/host/penguinslalom_physics
	./build/host/penguinslalom_views
	./build/host/test_libretro build/host/penguinslalom_libretro.so 600
	$(PYTHON) games/penguinslalom/tests/check_game.py
	sh games/penguinslalom/tests/state_test.sh build/host/penguinslalom_test_states build/states
	$(PYTHON) tools/state_audit.py --game penguinslalom build/host/penguinslalom_test_states $(PENGUINSLALOM_OBJS)
	set -e; for w in $(WIDTHS); do echo "== widescreen: $$w x 240"; \
	    a=$$(case $$w in 427) echo 16:9;; 520) echo Wide;; esac); \
	    [ -z "$$a" ] || ./build/host/test_libretro build/host/penguinslalom_libretro.so 600 $$a $$w; \
	    RS_WIDTH=$$w ./build/host/penguinslalom_views; \
	    RS_WIDTH=$$w $(PYTHON) games/penguinslalom/tests/check_game.py; \
	    RS_WIDTH=$$w sh games/penguinslalom/tests/state_test.sh build/host/penguinslalom_test_states build/states-penguinslalom-$$w; \
	done
	sh games/penguinslalom/tests/wide_test.sh build/host/penguinslalom_headless build/host/penguinslalom_test_states build "$(WIDTHS)"
	bash tools/i18n_test.sh build/host/penguinslalom_headless penguinslalom build/penguinslalom-i18n $(if $(WIDESCREEN_penguinslalom),320 $(WIDTHS),320)
penguinslalom-screenshots:
	+$(PENGUINSLALOM_MAKE) build/host/penguinslalom_headless
	$(PYTHON) games/penguinslalom/tests/check_game.py --screenshots
	set -e; for w in $(WIDTHS); do RS_WIDTH=$$w $(PYTHON) games/penguinslalom/tests/check_game.py --screenshots; done
