# Nutty Branch (games/nuttybranch): its make targets, included by the root Makefile.
#
#   make nuttybranch                host builds: nuttybranch (SDL2), nuttybranch_headless, nuttybranch_libretro.so
#   make nuttybranch-check          all its tests (libretro loader, smoke + bot + determinism, UI screens, save states, audit)
#   make nuttybranch-dist           dist/windows/NuttyBranch.exe, dist/libretro/nuttybranch_libretro.so (+ .armhf.so)
#   make nuttybranch-windows        only the Windows exe
#   make nuttybranch-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make nuttybranch-screenshots    games/nuttybranch/docs/screenshots/
#   make nuttybranch-art            redraw the code-drawn art (tools/make_art.py; commit art/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE; the rules below follow the root Makefile.

NUTTYBRANCH_MAKE = $(MAKE) --no-print-directory GAME=nuttybranch GAME_NAME=NuttyBranch
NUTTYBRANCH_DIR  = games/nuttybranch
# the house style kit (games/common/src, docs/art-direction.md) is compiled in
HOUSE_UI_nuttybranch = 1

.PHONY: nuttybranch nuttybranch-check nuttybranch-windows nuttybranch-armhf nuttybranch-dist nuttybranch-screenshots nuttybranch-art

nuttybranch:
	+$(NUTTYBRANCH_MAKE) host
nuttybranch-windows:
	+$(NUTTYBRANCH_MAKE) windows
nuttybranch-armhf:
	+$(NUTTYBRANCH_MAKE) armhf
nuttybranch-dist:
	+$(NUTTYBRANCH_MAKE) dist

NUTTYBRANCH_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(NUTTYBRANCH_DIR)/src/*.c) $(HOUSE_SRC))

nuttybranch-check:
	+$(NUTTYBRANCH_MAKE) host build/host/test_libretro build/host/nuttybranch_test_states
	./build/host/test_libretro build/host/nuttybranch_libretro.so 600
	sh $(NUTTYBRANCH_DIR)/tests/smoke_test.sh build/host/nuttybranch_headless build
	sh $(NUTTYBRANCH_DIR)/tests/ui_test.sh build/host/nuttybranch_headless build/nuttybranch-ui
	sh $(NUTTYBRANCH_DIR)/tests/state_test.sh build/host/nuttybranch_test_states build/states
	$(PYTHON) tools/state_audit.py --game nuttybranch build/host/nuttybranch_test_states $(NUTTYBRANCH_OBJ_HOST)

nuttybranch-screenshots:
	+$(NUTTYBRANCH_MAKE) build/host/nuttybranch_headless
	sh $(NUTTYBRANCH_DIR)/tests/ui_test.sh build/host/nuttybranch_headless $(NUTTYBRANCH_DIR)/docs/screenshots 2

nuttybranch-art:
	$(PYTHON) $(NUTTYBRANCH_DIR)/tools/make_art.py
