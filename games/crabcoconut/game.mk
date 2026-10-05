# Crab Coconut: house kit, portable host/libretro and static Windows builds.
HOUSE_UI_crabcoconut = 1
CRABCOCONUT_MAKE = $(MAKE) --no-print-directory GAME=crabcoconut GAME_NAME=CrabCoconut
CRABCOCONUT_OBJS = $(patsubst %.c,build/host/%.o,$(wildcard games/crabcoconut/src/*.c) $(HOUSE_SRC))
.PHONY: crabcoconut crabcoconut-windows crabcoconut-armhf crabcoconut-dist crabcoconut-art crabcoconut-check crabcoconut-screenshots
crabcoconut:
	+$(CRABCOCONUT_MAKE) host
crabcoconut-windows:
	+$(CRABCOCONUT_MAKE) windows
crabcoconut-armhf:
	+$(CRABCOCONUT_MAKE) armhf
crabcoconut-dist:
	+$(CRABCOCONUT_MAKE) dist
crabcoconut-art:
	$(PYTHON) games/crabcoconut/tools/make_art.py
build/host/crabcoconut_physics: games/crabcoconut/src/play.c games/crabcoconut/src/game.h games/crabcoconut/src/sine.h games/crabcoconut/tests/test_physics.c
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -Igames/crabcoconut/src games/crabcoconut/src/play.c games/crabcoconut/tests/test_physics.c -o $@
build/host/crabcoconut_views: games/crabcoconut/tests/test_views.c $(CRABCOCONUT_OBJS) build/host/build/gen/crabcoconut/assets.o build/host/build/gen/crabcoconut/game_text.o build/host/librs.a
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -Igames/crabcoconut/src -Igames/common/src $^ -lm -o $@
build/host/crabcoconut_review: games/crabcoconut/tests/review_scenes.c $(CRABCOCONUT_OBJS) build/host/build/gen/crabcoconut/assets.o build/host/build/gen/crabcoconut/game_text.o build/host/sdk/frontends/common/rs_desktop.o build/host/librs.a
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -Isdk/frontends/common -Igames/crabcoconut/src -Igames/common/src $^ -lm -o $@
crabcoconut-check:
	+$(CRABCOCONUT_MAKE) host build/host/test_libretro build/host/crabcoconut_test_states build/host/crabcoconut_physics build/host/crabcoconut_views
	./build/host/crabcoconut_physics
	./build/host/crabcoconut_views
	./build/host/test_libretro build/host/crabcoconut_libretro.so 600
	$(PYTHON) games/crabcoconut/tests/check_game.py
	sh games/crabcoconut/tests/state_test.sh build/host/crabcoconut_test_states build/states
	$(PYTHON) tools/state_audit.py --game crabcoconut build/host/crabcoconut_test_states $(CRABCOCONUT_OBJS)
	bash tools/i18n_test.sh build/host/crabcoconut_headless crabcoconut build/crabcoconut-i18n $(if $(WIDESCREEN_crabcoconut),320 $(WIDTHS),320)
crabcoconut-screenshots:
	+$(CRABCOCONUT_MAKE) build/host/crabcoconut_headless
	$(PYTHON) games/crabcoconut/tests/check_game.py --screenshots
