# Beaver Rush (games/beaverrush): its make targets, included by the root Makefile.
#
#   make beaverrush                host builds: beaverrush (SDL2), beaverrush_headless, beaverrush_libretro.so
#   make beaverrush-check          all its tests (rules and fairness, smoke and bot, UI pictures, save states)
#   make beaverrush-dist           dist/windows/BeaverRush.exe, dist/libretro/beaverrush_libretro.so (+ .armhf.so)
#   make beaverrush-windows        only the Windows exe
#   make beaverrush-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make beaverrush-screenshots    games/beaverrush/docs/screenshots/
#   make beaverrush-bench          per-frame cost of the heaviest scene
#   make beaverrush-art            the code-drawn sheets as PNG for review (build/beaverrush-art/)
#
# (c) 2026 Pierre-Louis Boyer (8BCraft). All rights reserved: games/beaverrush/LICENSE.

BR_MAKE = $(MAKE) --no-print-directory GAME=beaverrush GAME_NAME=BeaverRush
BR_DIR  = games/beaverrush

.PHONY: beaverrush beaverrush-check beaverrush-windows beaverrush-armhf beaverrush-dist beaverrush-screenshots \
        beaverrush-bench beaverrush-art

beaverrush:
	+$(BR_MAKE) host
beaverrush-windows:
	+$(BR_MAKE) windows
beaverrush-armhf:
	+$(BR_MAKE) armhf
beaverrush-dist:
	+$(BR_MAKE) dist

# the rules tests link the game rules (rules.c, world.c) alone
build/host/beaverrush_test_rules: build/host/$(BR_DIR)/tests/test_rules.o build/host/$(BR_DIR)/src/rules.o \
                                  build/host/$(BR_DIR)/src/world.o build/host/librs.a
	$(HOST_CC) -o $@ $^ -lm

beaverrush-check:
	+$(BR_MAKE) build/host/beaverrush_test_rules
	./build/host/beaverrush_test_rules
