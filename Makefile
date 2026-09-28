# RetroStone VC: SDK, games and tools.
#
#   make                 host builds of the default game (libretro core, SDL2 runner, headless runner)
#   make check           unit tests (SDK renderer/input/save RAM, asset tools, cutter, levels)
#   make windows         dist/windows/BomberMole.exe (mingw-w64, SDL2 linked statically)
#   make armhf           armhf cross-build of the libretro core (Cortex-A7, RetroStone2)
#   make dist            all deliverables into dist/
#   make screenshots     headless screenshots into docs/screenshots/
#   make bench           per-frame cost of the heaviest scenes
#   make DEBUG=1 ...     -O0 -g and strict mode on by default
#
# MIT licence (build system and SDK), (c) 2026 Pierre-Louis Boyer (8BCraft).

GAME      ?= bombermole
GAME_NAME ?= BomberMole
PYTHON    ?= python3
JOBS      ?= $(shell nproc 2>/dev/null || echo 4)

HOST_CC   ?= gcc
WIN_CC    ?= x86_64-w64-mingw32-gcc
WIN_AR    ?= x86_64-w64-mingw32-ar
ARM_CC    ?= arm-linux-gnueabihf-gcc
ARM_AR    ?= arm-linux-gnueabihf-ar
ARM_FLAGS ?= -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard
# SDL2 development files for mingw (tools/fetch_sdl2_mingw.sh puts them here)
SDL2_MINGW ?= build/deps/SDL2-2.32.10/x86_64-w64-mingw32

ifeq ($(DEBUG),1)
OPT = -O0 -g -DRS_DEBUG
else
OPT = -O2
endif

WARN     = -Wall -Wextra -Wno-unused-parameter
CSTD     = -std=c11 -D_POSIX_C_SOURCE=200809L
SDK_INC  = -Isdk/include -Isdk/src -Isdk/third_party/libxmp-lite/include/libxmp-lite \
           -Isdk/third_party/stb -Isdk/third_party/libretro -Isdk/frontends/common
XMP_DEFS = -DLIBXMP_CORE_PLAYER -DLIBXMP_STATIC -DLIBXMP_NO_DEPACKERS

SDK_SRC  = $(wildcard sdk/src/*.c)
XMP_SRC  = $(wildcard sdk/third_party/libxmp-lite/src/*.c) \
           $(wildcard sdk/third_party/libxmp-lite/src/loaders/*.c)
GAME_SRC = $(wildcard games/$(GAME)/src/*.c)
GAME_GEN = build/gen/$(GAME)/assets.c
GAME_INC = -Igames/$(GAME)/src -Ibuild/gen/$(GAME)

# ---- per-platform object lists ------------------------------------------------
define objs
$(patsubst %.c,build/$(1)/%.o,$(2))
endef

HOST_CFLAGS = $(CSTD) $(OPT) $(WARN) -fPIC $(SDK_INC)
WIN_CFLAGS  = $(CSTD) $(OPT) $(WARN) $(SDK_INC) -D__USE_MINGW_ANSI_STDIO=1
ARM_CFLAGS  = $(CSTD) $(OPT) $(WARN) -fPIC $(ARM_FLAGS) $(SDK_INC)

.PHONY: all host check test windows armhf dist screenshots bench clean assets placeholders golden
all: host

# ---- SDK static library --------------------------------------------------------
build/host/librs.a: $(call objs,host,$(SDK_SRC) $(XMP_SRC))
	@mkdir -p $(dir $@); rm -f $@; ar rcs $@ $^
build/win64/librs.a: $(call objs,win64,$(SDK_SRC) $(XMP_SRC))
	@mkdir -p $(dir $@); rm -f $@; $(WIN_AR) rcs $@ $^
build/armhf/librs.a: $(call objs,armhf,$(SDK_SRC) $(XMP_SRC))
	@mkdir -p $(dir $@); rm -f $@; $(ARM_AR) rcs $@ $^

# libxmp-lite: third-party code, built with its own defines and fewer warnings
build/host/sdk/third_party/%.o: sdk/third_party/%.c
	@mkdir -p $(dir $@); $(HOST_CC) $(CSTD) $(OPT) -fPIC -w $(XMP_DEFS) -Isdk/third_party/libxmp-lite/include/libxmp-lite -Isdk/third_party/libxmp-lite/src -c $< -o $@
build/win64/sdk/third_party/%.o: sdk/third_party/%.c
	@mkdir -p $(dir $@); $(WIN_CC) $(CSTD) $(OPT) -w $(XMP_DEFS) -Isdk/third_party/libxmp-lite/include/libxmp-lite -Isdk/third_party/libxmp-lite/src -c $< -o $@
build/armhf/sdk/third_party/%.o: sdk/third_party/%.c
	@mkdir -p $(dir $@); $(ARM_CC) $(CSTD) $(OPT) -fPIC $(ARM_FLAGS) -w $(XMP_DEFS) -Isdk/third_party/libxmp-lite/include/libxmp-lite -Isdk/third_party/libxmp-lite/src -c $< -o $@

build/host/%.o: %.c
	@mkdir -p $(dir $@); $(HOST_CC) $(HOST_CFLAGS) $(GAME_INC) -MMD -c $< -o $@
build/win64/%.o: %.c
	@mkdir -p $(dir $@); $(WIN_CC) $(WIN_CFLAGS) $(GAME_INC) -I$(SDL2_MINGW)/include/SDL2 -MMD -c $< -o $@
build/armhf/%.o: %.c
	@mkdir -p $(dir $@); $(ARM_CC) $(ARM_CFLAGS) $(GAME_INC) -MMD -c $< -o $@

sdk/src/rs_font.c: sdk/tools/gen_font.py
	$(PYTHON) $< > $@

# ---- game assets (generated C) ----------------------------------------------------
GAME_ART    = $(wildcard games/$(GAME)/art/*.png)
GAME_LEVELS = $(wildcard games/$(GAME)/levels/*.txt)
$(GAME_GEN): games/$(GAME)/tools/build_assets.py tools/rsasset.py tools/sheets.py $(GAME_ART) $(GAME_LEVELS) \
             $(wildcard games/$(GAME)/tools/*.py)
	@mkdir -p $(dir $@)
	$(PYTHON) games/$(GAME)/tools/build_assets.py --out $(dir $@)
assets: $(GAME_GEN)

GAME_OBJ_HOST  = $(call objs,host,$(GAME_SRC) $(GAME_GEN))
GAME_OBJ_WIN   = $(call objs,win64,$(GAME_SRC) $(GAME_GEN))
GAME_OBJ_ARM   = $(call objs,armhf,$(GAME_SRC) $(GAME_GEN))
$(GAME_OBJ_HOST) $(GAME_OBJ_WIN) $(GAME_OBJ_ARM): $(GAME_GEN)

# ---- host targets ---------------------------------------------------------------
HOST_BIN = build/host/$(GAME)_libretro.so build/host/$(GAME)_headless build/host/$(GAME)
host: $(HOST_BIN)

build/host/$(GAME)_libretro.so: $(GAME_OBJ_HOST) build/host/sdk/frontends/libretro/rs_libretro.o build/host/librs.a
	$(HOST_CC) -shared -o $@ $^ -lm -Wl,--no-undefined -Wl,--version-script=sdk/frontends/libretro/link.T
build/host/$(GAME)_headless: $(GAME_OBJ_HOST) build/host/sdk/frontends/headless/rs_headless.o \
                             build/host/sdk/frontends/common/rs_desktop.o build/host/librs.a
	$(HOST_CC) -o $@ $^ -lm
build/host/sdk/frontends/sdl2/rs_sdl.o: HOST_CFLAGS += $(shell sdl2-config --cflags)
build/host/$(GAME): $(GAME_OBJ_HOST) build/host/sdk/frontends/sdl2/rs_sdl.o \
                    build/host/sdk/frontends/common/rs_desktop.o build/host/librs.a
	$(HOST_CC) -o $@ $^ $(shell sdl2-config --libs) -lm

# ---- Windows (mingw-w64) ----------------------------------------------------------
windows: dist/windows/$(GAME_NAME).exe
$(SDL2_MINGW)/lib/libSDL2.a:
	sh tools/fetch_sdl2_mingw.sh build/deps
build/win64/sdk/frontends/sdl2/rs_sdl.o: $(SDL2_MINGW)/lib/libSDL2.a
dist/windows/$(GAME_NAME).exe: $(GAME_OBJ_WIN) build/win64/sdk/frontends/sdl2/rs_sdl.o \
                               build/win64/sdk/frontends/common/rs_desktop.o build/win64/librs.a \
                               $(SDL2_MINGW)/lib/libSDL2.a
	@mkdir -p $(dir $@)
	$(WIN_CC) -o $@ $(filter %.o %.a,$(filter-out $(SDL2_MINGW)/lib/libSDL2.a,$^)) \
	    -L$(SDL2_MINGW)/lib -static -lmingw32 -lSDL2main -lSDL2 -mwindows \
	    -lm -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 \
	    -lshell32 -lversion -luuid -lsetupapi -static-libgcc
	cp games/$(GAME)/dist/README-windows.txt dist/windows/README.txt
	cp THIRD_PARTY.md dist/windows/THIRD_PARTY.md

# ---- armhf (RetroStone2, Cortex-A7) --------------------------------------------------
armhf: build/armhf/$(GAME)_libretro.so
build/armhf/$(GAME)_libretro.so: $(GAME_OBJ_ARM) build/armhf/sdk/frontends/libretro/rs_libretro.o build/armhf/librs.a
	$(ARM_CC) $(ARM_FLAGS) -shared -o $@ $^ -lm -Wl,--no-undefined -Wl,--version-script=sdk/frontends/libretro/link.T

# ---- deliverables ---------------------------------------------------------------------
dist: windows build/host/$(GAME)_libretro.so armhf
	@mkdir -p dist/libretro
	cp build/host/$(GAME)_libretro.so dist/libretro/$(GAME)_libretro.so
	cp build/armhf/$(GAME)_libretro.so dist/libretro/$(GAME)_libretro.armhf.so
	@ls -l dist/windows dist/libretro

# ---- tests -------------------------------------------------------------------------
build/host/test_sdk: build/host/sdk/tests/test_sdk.o build/host/sdk/frontends/common/rs_desktop.o build/host/librs.a
	$(HOST_CC) -o $@ $^ -lm
test check: build/host/test_sdk build/host/$(GAME)_headless
	./build/host/test_sdk --golden sdk/tests/golden --out build
	$(PYTHON) tools/tests/test_tools.py
	$(PYTHON) games/$(GAME)/tools/check_levels.py
	sh games/$(GAME)/tools/smoke_test.sh build/host/$(GAME)_headless build
golden: build/host/test_sdk
	./build/host/test_sdk --update --golden sdk/tests/golden --out build

placeholders:
	$(PYTHON) tools/make_placeholders.py --out games/$(GAME)/art

screenshots: build/host/$(GAME)_headless
	sh games/$(GAME)/tools/screenshots.sh build/host/$(GAME)_headless docs/screenshots

bench: build/host/$(GAME)_headless
	sh games/$(GAME)/tools/bench.sh build/host/$(GAME)_headless

clean:
	rm -rf build/host build/win64 build/armhf build/gen

-include $(shell find build -name '*.d' 2>/dev/null)
