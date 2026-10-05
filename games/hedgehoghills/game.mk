# Hedgehog Hills: built with the 8BCraft house kit and the standard SDK.
HOUSE_UI_hedgehoghills = 1
HEDGEHOGHILLS_MAKE = $(MAKE) --no-print-directory GAME=hedgehoghills GAME_NAME=HedgehogHills
HEDGEHOGHILLS_DIR = games/hedgehoghills
HEDGEHOGHILLS_OBJS = $(patsubst %.c,build/host/%.o,$(wildcard $(HEDGEHOGHILLS_DIR)/src/*.c) $(HOUSE_SRC))
.PHONY: hedgehoghills hedgehoghills-windows hedgehoghills-armhf hedgehoghills-dist hedgehoghills-art hedgehoghills-check hedgehoghills-screenshots
hedgehoghills:
	+$(HEDGEHOGHILLS_MAKE) host
hedgehoghills-windows:
	+$(HEDGEHOGHILLS_MAKE) windows
hedgehoghills-armhf:
	+$(HEDGEHOGHILLS_MAKE) armhf
hedgehoghills-dist:
	+$(HEDGEHOGHILLS_MAKE) dist
hedgehoghills-art:
	$(PYTHON) $(HEDGEHOGHILLS_DIR)/tools/make_art.py
build/host/hedgehoghills_physics: $(HEDGEHOGHILLS_DIR)/src/play.c $(HEDGEHOGHILLS_DIR)/src/game.h $(HEDGEHOGHILLS_DIR)/src/curve.h $(HEDGEHOGHILLS_DIR)/tests/test_physics.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -I$(HEDGEHOGHILLS_DIR)/src $(HEDGEHOGHILLS_DIR)/src/play.c $(HEDGEHOGHILLS_DIR)/tests/test_physics.c -o $@
build/host/hedgehoghills_test_sky: $(HEDGEHOGHILLS_DIR)/tests/test_sky.c $(HEDGEHOGHILLS_OBJS) build/host/build/gen/hedgehoghills/assets.o build/host/build/gen/hedgehoghills/game_text.o build/host/librs.a
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -I$(HEDGEHOGHILLS_DIR)/src $^ -lm -o $@
build/host/hedgehoghills_test_split: $(HEDGEHOGHILLS_DIR)/tests/test_split.c $(HEDGEHOGHILLS_OBJS) build/host/build/gen/hedgehoghills/assets.o build/host/build/gen/hedgehoghills/game_text.o build/host/sdk/frontends/common/rs_desktop.o build/host/librs.a
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) $(GAME_INC) -Isdk/include -Isdk/frontends/common $^ -lm -o $@
hedgehoghills-check:
	+$(HEDGEHOGHILLS_MAKE) host build/host/test_libretro build/host/hedgehoghills_test_states build/host/hedgehoghills_physics build/host/hedgehoghills_test_sky build/host/hedgehoghills_test_split
	./build/host/hedgehoghills_physics
	./build/host/hedgehoghills_test_sky
	./build/host/hedgehoghills_test_split
	./build/host/test_libretro build/host/hedgehoghills_libretro.so 600
	$(PYTHON) $(HEDGEHOGHILLS_DIR)/tests/check_game.py
	sh $(HEDGEHOGHILLS_DIR)/tests/state_test.sh build/host/hedgehoghills_test_states build/states
	$(PYTHON) tools/state_audit.py --game hedgehoghills build/host/hedgehoghills_test_states $(HEDGEHOGHILLS_OBJS)
	bash tools/i18n_test.sh build/host/hedgehoghills_headless hedgehoghills build/hedgehoghills-i18n $(if $(WIDESCREEN_hedgehoghills),320 $(WIDTHS),320)
hedgehoghills-screenshots:
	+$(HEDGEHOGHILLS_MAKE) build/host/hedgehoghills_headless
	$(PYTHON) $(HEDGEHOGHILLS_DIR)/tests/check_game.py --screenshots
