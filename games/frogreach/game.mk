# Frog Reach: house kit, portable host/libretro and static Windows builds.
HOUSE_UI_frogreach = 1
FROGREACH_MAKE = $(MAKE) --no-print-directory GAME=frogreach GAME_NAME=FrogReach
FROGREACH_OBJS = $(patsubst %.c,build/host/%.o,$(wildcard games/frogreach/src/*.c) $(HOUSE_SRC))
.PHONY: frogreach frogreach-windows frogreach-armhf frogreach-dist frogreach-art frogreach-check frogreach-screenshots
frogreach:
	+$(FROGREACH_MAKE) host
frogreach-windows:
	+$(FROGREACH_MAKE) windows
frogreach-armhf:
	+$(FROGREACH_MAKE) armhf
frogreach-dist:
	+$(FROGREACH_MAKE) dist
frogreach-art:
	$(PYTHON) games/frogreach/tools/make_art.py
build/host/frogreach_physics: games/frogreach/src/play.c games/frogreach/src/game.h games/frogreach/src/sine.h games/frogreach/tests/test_physics.c
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -Igames/frogreach/src games/frogreach/src/play.c games/frogreach/tests/test_physics.c -o $@
build/host/frogreach_views: games/frogreach/tests/test_views.c $(FROGREACH_OBJS) build/host/build/gen/frogreach/assets.o build/host/librs.a
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -DGAME_FROG=1 -Isdk/include -Igames/frogreach/src -Igames/common/src $^ -lm -o $@
frogreach-check:
	+$(FROGREACH_MAKE) host build/host/test_libretro build/host/frogreach_test_states build/host/frogreach_physics build/host/frogreach_views
	./build/host/frogreach_physics
	./build/host/frogreach_views
	./build/host/test_libretro build/host/frogreach_libretro.so 600
	$(PYTHON) games/frogreach/tests/check_game.py
	sh games/frogreach/tests/state_test.sh build/host/frogreach_test_states build/states
	$(PYTHON) tools/state_audit.py --game frogreach build/host/frogreach_test_states $(FROGREACH_OBJS)
frogreach-screenshots:
	+$(FROGREACH_MAKE) build/host/frogreach_headless
	$(PYTHON) games/frogreach/tests/check_game.py --screenshots
