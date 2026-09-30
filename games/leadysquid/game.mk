# Leady Squid (games/leadysquid): its make targets, included by the root Makefile.
#
#   make leadysquid                host builds: leadysquid (SDL2), leadysquid_headless, leadysquid_libretro.so
#   make leadysquid-check          all its tests (SDK, libretro loader, physics, caps on columns, smoke, bot, save states,
#                                  determinism, art tools)
#   make leadysquid-dist           dist/windows/LeadySquid.exe, dist/libretro/leadysquid_libretro.so (+ .armhf.so)
#   make leadysquid-windows        only the Windows exe
#   make leadysquid-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make leadysquid-screenshots    games/leadysquid/docs/screenshots/
#   make leadysquid-bench          per-frame cost of a busy scene
#   make leadysquid-placeholders   redraw the code-drawn art (tools/make_placeholders.py --game leadysquid)
#   make leadysquid-todo           create / extend art/incoming/TODO.md for the image agent
#   make leadysquid-art            import the owner's VALIDATED art (art_sync.py --game leadysquid sync)
#   make leadysquid-art-review     the owner's review tool on http://localhost:8765
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/leadysquid/LICENSE; the rules below follow the root Makefile.

LS_MAKE = $(MAKE) --no-print-directory GAME=leadysquid GAME_NAME=LeadySquid
LS_DIR  = games/leadysquid
# the house style kit (games/common/src: the UI kit and the synthesiser) is compiled in
HOUSE_UI_leadysquid = 1

.PHONY: leadysquid leadysquid-check leadysquid-windows leadysquid-armhf leadysquid-dist leadysquid-screenshots \
        leadysquid-bench leadysquid-placeholders leadysquid-todo leadysquid-art leadysquid-art-review

leadysquid:
	+$(LS_MAKE) host
leadysquid-windows:
	+$(LS_MAKE) windows
leadysquid-armhf:
	+$(LS_MAKE) armhf
leadysquid-dist:
	+$(LS_MAKE) dist

# the physics unit tests link the game rules (physics.c, world.c) alone
build/host/leadysquid_test_physics: build/host/$(LS_DIR)/tests/test_physics.o build/host/$(LS_DIR)/src/physics.o \
                                    build/host/$(LS_DIR)/src/world.o build/host/librs.a
	$(HOST_CC) -o $@ $^ -lm

# the caps test runs the whole game (runtime, draw) with a scripted player and checks the picture
LS_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(LS_DIR)/src/*.c) $(HOUSE_SRC) build/gen/leadysquid/assets.c)
build/host/$(LS_DIR)/tests/test_caps.o: build/gen/leadysquid/assets.c
build/host/leadysquid_test_caps: build/host/$(LS_DIR)/tests/test_caps.o $(LS_OBJ_HOST) build/host/librs.a
	$(HOST_CC) -o $@ $^ -lm

leadysquid-check:
	+$(LS_MAKE) host build/host/test_sdk build/host/test_libretro build/host/leadysquid_test_physics \
	    build/host/leadysquid_test_caps build/host/leadysquid_test_states
	./build/host/test_sdk --golden sdk/tests/golden --out build
	./build/host/test_libretro build/host/leadysquid_libretro.so 600
	./build/host/leadysquid_test_physics
	sh $(LS_DIR)/tests/caps_test.sh build/host/leadysquid_test_caps build
	sh $(LS_DIR)/tests/smoke_test.sh build/host/leadysquid_headless build
	sh $(LS_DIR)/tests/state_test.sh build/host/leadysquid_test_states build/states
	$(PYTHON) tools/state_audit.py --game leadysquid build/host/leadysquid_test_states $(filter-out %/assets.o,$(LS_OBJ_HOST))
	$(PYTHON) $(LS_DIR)/tests/test_art_tools.py

leadysquid-screenshots:
	+$(LS_MAKE) build/host/leadysquid_headless
	sh $(LS_DIR)/tools/screenshots.sh build/host/leadysquid_headless $(LS_DIR)/docs/screenshots

leadysquid-bench:
	+$(LS_MAKE) build/host/leadysquid_headless
	./build/host/leadysquid_headless --frames 3000 --opt bot=1 --opt seed=7 --opt music=1 --bench 300 | tail -4

leadysquid-placeholders:
	$(PYTHON) tools/make_placeholders.py --game leadysquid

leadysquid-todo:
	$(PYTHON) tools/art_sync.py --game leadysquid todo

leadysquid-art:
	$(PYTHON) tools/art_sync.py --game leadysquid sync

leadysquid-art-review:
	$(PYTHON) tools/art_review.py --game leadysquid
