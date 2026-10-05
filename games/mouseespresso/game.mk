# Mouse Espresso: house kit, portable host/libretro and static Windows builds.
HOUSE_UI_mouseespresso = 1
MOUSEESPRESSO_MAKE = $(MAKE) --no-print-directory GAME=mouseespresso GAME_NAME=MouseEspresso
MOUSEESPRESSO_OBJS = $(patsubst %.c,build/host/%.o,$(wildcard games/mouseespresso/src/*.c) $(HOUSE_SRC))
.PHONY: mouseespresso mouseespresso-windows mouseespresso-armhf mouseespresso-dist mouseespresso-art mouseespresso-check mouseespresso-screenshots
mouseespresso:
	+$(MOUSEESPRESSO_MAKE) host
mouseespresso-windows:
	+$(MOUSEESPRESSO_MAKE) windows
mouseespresso-armhf:
	+$(MOUSEESPRESSO_MAKE) armhf
mouseespresso-dist:
	+$(MOUSEESPRESSO_MAKE) dist
mouseespresso-art:
	$(PYTHON) games/mouseespresso/tools/make_art.py
build/host/mouseespresso_physics: games/mouseespresso/src/play.c games/mouseespresso/src/game.h games/mouseespresso/src/sine.h games/mouseespresso/tests/test_physics.c
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -Igames/mouseespresso/src games/mouseespresso/src/play.c games/mouseespresso/tests/test_physics.c -o $@
build/host/mouseespresso_views: games/mouseespresso/tests/test_views.c $(MOUSEESPRESSO_OBJS) build/host/build/gen/mouseespresso/assets.o build/host/build/gen/mouseespresso/game_text.o build/host/librs.a
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -Igames/mouseespresso/src -Igames/common/src $^ -lm -o $@
build/host/mouseespresso_review: games/mouseespresso/tests/review_scenes.c $(MOUSEESPRESSO_OBJS) build/host/build/gen/mouseespresso/assets.o build/host/build/gen/mouseespresso/game_text.o build/host/sdk/frontends/common/rs_desktop.o build/host/librs.a
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -Isdk/frontends/common -Igames/mouseespresso/src -Igames/common/src $^ -lm -o $@
mouseespresso-check:
	+$(MOUSEESPRESSO_MAKE) host build/host/test_libretro build/host/mouseespresso_test_states build/host/mouseespresso_physics build/host/mouseespresso_views
	./build/host/mouseespresso_physics
	./build/host/mouseespresso_views
	./build/host/test_libretro build/host/mouseespresso_libretro.so 600
	$(PYTHON) games/mouseespresso/tests/check_game.py
	sh games/mouseespresso/tests/state_test.sh build/host/mouseespresso_test_states build/states
	$(PYTHON) tools/state_audit.py --game mouseespresso build/host/mouseespresso_test_states $(MOUSEESPRESSO_OBJS)
	bash tools/i18n_test.sh build/host/mouseespresso_headless mouseespresso build/mouseespresso-i18n $(if $(WIDESCREEN_mouseespresso),320 $(WIDTHS),320)
mouseespresso-screenshots:
	+$(MOUSEESPRESSO_MAKE) build/host/mouseespresso_headless
	$(PYTHON) games/mouseespresso/tests/check_game.py --screenshots
