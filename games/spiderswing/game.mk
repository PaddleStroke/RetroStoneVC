# Spider Swing: house kit, portable host/libretro and static Windows builds.
HOUSE_UI_spiderswing = 1
SPIDERSWING_MAKE = $(MAKE) --no-print-directory GAME=spiderswing GAME_NAME=SpiderSwing
SPIDERSWING_OBJS = $(patsubst %.c,build/host/%.o,$(wildcard games/spiderswing/src/*.c) $(HOUSE_SRC))
.PHONY: spiderswing spiderswing-windows spiderswing-armhf spiderswing-dist spiderswing-art spiderswing-check spiderswing-screenshots
spiderswing:
	+$(SPIDERSWING_MAKE) host
spiderswing-windows:
	+$(SPIDERSWING_MAKE) windows
spiderswing-armhf:
	+$(SPIDERSWING_MAKE) armhf
spiderswing-dist:
	+$(SPIDERSWING_MAKE) dist
spiderswing-art:
	$(PYTHON) games/spiderswing/tools/make_art.py
build/host/spiderswing_physics: games/spiderswing/src/play.c games/spiderswing/src/game.h games/spiderswing/src/sine.h games/spiderswing/tests/test_physics.c
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -Igames/spiderswing/src games/spiderswing/src/play.c games/spiderswing/tests/test_physics.c -o $@
build/host/spiderswing_views: games/spiderswing/tests/test_views.c $(SPIDERSWING_OBJS) build/host/build/gen/spiderswing/assets.o build/host/build/gen/spiderswing/game_text.o build/host/librs.a
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -DGAME_FROG=0 -Isdk/include -Igames/spiderswing/src -Igames/common/src $^ -lm -o $@
build/host/spiderswing_review: games/spiderswing/tests/review_scenes.c $(SPIDERSWING_OBJS) build/host/build/gen/spiderswing/assets.o build/host/build/gen/spiderswing/game_text.o build/host/sdk/frontends/common/rs_desktop.o build/host/librs.a
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -Isdk/frontends/common -Igames/spiderswing/src -Igames/common/src -Ibuild/gen/spiderswing $^ -lm -o $@
spiderswing-check:
	+$(SPIDERSWING_MAKE) host build/host/test_libretro build/host/spiderswing_test_states build/host/spiderswing_physics build/host/spiderswing_views
	./build/host/spiderswing_physics
	./build/host/spiderswing_views
	./build/host/test_libretro build/host/spiderswing_libretro.so 600
	$(PYTHON) games/spiderswing/tests/check_game.py
	sh games/spiderswing/tests/state_test.sh build/host/spiderswing_test_states build/states
	$(PYTHON) tools/state_audit.py --game spiderswing build/host/spiderswing_test_states $(SPIDERSWING_OBJS)
	bash tools/i18n_test.sh build/host/spiderswing_headless spiderswing build/spiderswing-i18n $(if $(WIDESCREEN_spiderswing),320 $(WIDTHS),320)
spiderswing-screenshots:
	+$(SPIDERSWING_MAKE) build/host/spiderswing_headless
	$(PYTHON) games/spiderswing/tests/check_game.py --screenshots
