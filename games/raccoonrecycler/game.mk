# Raccoon Recycler (games/raccoonrecycler): its make targets, included by the root Makefile.
#
#   make raccoonrecycler                host builds: raccoonrecycler (SDL2), raccoonrecycler_headless, raccoonrecycler_libretro.so
#   make raccoonrecycler-check          all its tests (libretro loader, smoke + bot + determinism, UI screens, save states, audit)
#   make raccoonrecycler-dist           dist/windows/RaccoonRecycler.exe, dist/libretro/raccoonrecycler_libretro.so (+ .armhf.so)
#   make raccoonrecycler-windows        only the Windows exe
#   make raccoonrecycler-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make raccoonrecycler-screenshots    games/raccoonrecycler/docs/screenshots/
#   make raccoonrecycler-art            redraw the code-drawn art (tools/make_art.py; commit art/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/raccoonrecycler/LICENSE; the rules below follow the root Makefile.

RACCOONRECYCLER_MAKE = $(MAKE) --no-print-directory GAME=raccoonrecycler GAME_NAME=RaccoonRecycler
RACCOONRECYCLER_DIR  = games/raccoonrecycler
# the house style kit (games/common/src, docs/art-direction.md) is compiled in
HOUSE_UI_raccoonrecycler = 1

.PHONY: raccoonrecycler raccoonrecycler-check raccoonrecycler-windows raccoonrecycler-armhf raccoonrecycler-dist raccoonrecycler-screenshots raccoonrecycler-art

raccoonrecycler:
	+$(RACCOONRECYCLER_MAKE) host
raccoonrecycler-windows:
	+$(RACCOONRECYCLER_MAKE) windows
raccoonrecycler-armhf:
	+$(RACCOONRECYCLER_MAKE) armhf
raccoonrecycler-dist:
	+$(RACCOONRECYCLER_MAKE) dist

RACCOONRECYCLER_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(RACCOONRECYCLER_DIR)/src/*.c) $(HOUSE_SRC))

build/host/raccoonrecycler_physics: games/raccoonrecycler/tests/test_physics.c games/raccoonrecycler/src/play.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) $(SDK_INC) -Igames/raccoonrecycler/src $^ -o $@

raccoonrecycler-check: build/host/raccoonrecycler_physics
	./build/host/raccoonrecycler_physics
	+$(RACCOONRECYCLER_MAKE) host build/host/test_libretro build/host/raccoonrecycler_test_states
	./build/host/test_libretro build/host/raccoonrecycler_libretro.so 600
	sh $(RACCOONRECYCLER_DIR)/tests/smoke_test.sh build/host/raccoonrecycler_headless build
	sh $(RACCOONRECYCLER_DIR)/tests/ui_test.sh build/host/raccoonrecycler_headless build/raccoonrecycler-ui
	sh $(RACCOONRECYCLER_DIR)/tests/state_test.sh build/host/raccoonrecycler_test_states build/states
	$(PYTHON) tools/state_audit.py --game raccoonrecycler build/host/raccoonrecycler_test_states $(RACCOONRECYCLER_OBJ_HOST)

raccoonrecycler-screenshots:
	+$(RACCOONRECYCLER_MAKE) build/host/raccoonrecycler_headless
	sh $(RACCOONRECYCLER_DIR)/tests/ui_test.sh build/host/raccoonrecycler_headless $(RACCOONRECYCLER_DIR)/docs/screenshots 2

raccoonrecycler-art:
	$(PYTHON) $(RACCOONRECYCLER_DIR)/tools/make_art.py
