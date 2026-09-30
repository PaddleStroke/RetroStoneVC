# Pogo Mamie (games/pogomamie): its make targets, included by the root Makefile.
#
#   make pogomamie                host builds: pogomamie (SDL2), pogomamie_headless, pogomamie_libretro.so
#   make pogomamie-check          all its tests (SDK, libretro loader, physics + generator + determinism, the
#                                 tile/sprite alignment at every scroll phase, smoke + bot, save states and audit)
#   make pogomamie-dist           dist/windows/PogoMamie.exe, dist/libretro/pogomamie_libretro.so (+ .armhf.so)
#   make pogomamie-windows        only the Windows exe
#   make pogomamie-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make pogomamie-screenshots    games/pogomamie/docs/screenshots/
#   make pogomamie-bench          per-frame cost of the heaviest scene
#   make pogomamie-bot            the bot over 10 seeds (the distribution of its distances)
#   make pogomamie-art            redraw the code-drawn art (tools/make_art.py; commit art/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pogomamie/LICENSE; the rules below follow the root Makefile.

PM_MAKE = $(MAKE) --no-print-directory GAME=pogomamie GAME_NAME=PogoMamie
PM_DIR  = games/pogomamie
# the house style kit (games/common/src: the UI kit and the synthesiser) is compiled in
HOUSE_UI_pogomamie = 1

.PHONY: pogomamie pogomamie-check pogomamie-windows pogomamie-armhf pogomamie-dist pogomamie-screenshots \
        pogomamie-bench pogomamie-bot pogomamie-art pogomamie-placeholders

pogomamie:
	+$(PM_MAKE) host
pogomamie-windows:
	+$(PM_MAKE) windows
pogomamie-armhf:
	+$(PM_MAKE) armhf
pogomamie-dist:
	+$(PM_MAKE) dist

# the physics and generator tests link the game rules (physics.c, world.c, gen.c) alone
build/host/pogomamie_test_physics: build/host/$(PM_DIR)/tests/test_physics.o build/host/$(PM_DIR)/src/physics.o \
                                   build/host/$(PM_DIR)/src/world.o build/host/$(PM_DIR)/src/gen.o build/host/librs.a
	$(HOST_CC) -o $@ $^ -lm

# the alignment test runs the whole game (runtime, draw, the bot) and checks the picture against the world
PM_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(PM_DIR)/src/*.c) $(HOUSE_SRC) build/gen/pogomamie/assets.c)
build/host/$(PM_DIR)/tests/test_align.o: build/gen/pogomamie/assets.c
build/host/pogomamie_test_align: build/host/$(PM_DIR)/tests/test_align.o $(PM_OBJ_HOST) build/host/librs.a
	$(HOST_CC) -o $@ $^ -lm

pogomamie-check:
	+$(PM_MAKE) host build/host/test_sdk build/host/test_libretro build/host/pogomamie_test_physics \
	    build/host/pogomamie_test_align build/host/pogomamie_test_states
	./build/host/test_sdk --golden sdk/tests/golden --out build
	./build/host/test_libretro build/host/pogomamie_libretro.so 600
	./build/host/pogomamie_test_physics
	sh $(PM_DIR)/tests/align_test.sh build/host/pogomamie_test_align build
	sh $(PM_DIR)/tests/smoke_test.sh build/host/pogomamie_headless build
	sh $(PM_DIR)/tests/state_test.sh build/host/pogomamie_test_states build/states
	$(PYTHON) tools/state_audit.py --game pogomamie build/host/pogomamie_test_states $(filter-out %/assets.o,$(PM_OBJ_HOST))

pogomamie-bot:
	+$(PM_MAKE) build/host/pogomamie_headless
	sh $(PM_DIR)/tests/bot_test.sh build/host/pogomamie_headless build

pogomamie-screenshots:
	+$(PM_MAKE) build/host/pogomamie_headless
	sh $(PM_DIR)/tools/screenshots.sh build/host/pogomamie_headless $(PM_DIR)/docs/screenshots

pogomamie-bench:
	+$(PM_MAKE) build/host/pogomamie_headless
	sh $(PM_DIR)/tools/bench.sh build/host/pogomamie_headless

pogomamie-art pogomamie-placeholders:
	$(PYTHON) tools/make_placeholders.py --game pogomamie
