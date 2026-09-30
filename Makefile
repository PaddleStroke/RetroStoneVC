# RetroStone VC: SDK, games and tools.
#
#   make                 host builds of the default game (libretro core, SDL2 runner, headless runner)
#   make check           unit tests (SDK renderer/input/save RAM, asset tools, cutter, levels)
#   make windows         dist/windows/BomberMole.exe (mingw-w64, SDL2 linked statically)
#   make armhf           armhf cross-build of the libretro core (Cortex-A7, RetroStone2)
#   make dist            all deliverables into dist/
#   make screenshots     headless screenshots into docs/screenshots/
#   make art-ai          regenerate the default art set (games/bombermole/art-ai/) from art/incoming
#   make ART=art ...     the validated-only art (VALIDATED strips + placeholders, 16-px characters)
#   make bench           per-frame cost of the heaviest scenes
#   make DEBUG=1 ...     -O0 -g and strict mode on by default
#
# MIT licence (build system and SDK), (c) 2026 Pierre-Louis Boyer (8BCraft).

GAME      ?= bombermole
GAME_NAME ?= BomberMole
# The art set: a folder of games/$(GAME)/ (or a path relative to it). The default is the game's committed
# AI set, art-ai/, when it has one (make art-ai: every GENERATED and VALIDATED strip of art/incoming, the
# placeholders for the TODO ones, never the REJECTED ones), else art/. ART=art is the validated-only look:
# the owner's VALIDATED strips (make art) and the placeholders, with 16-px characters.
ART       ?= $(if $(wildcard games/$(GAME)/art-ai),art-ai,art)
# Character size (Bomber Mole: 16, 24 or 32). Each set is made at one size: art-ai at ART_AI_CHAR, art/ at 16.
ART_AI_CHAR ?= 24
CHAR_SIZE ?= $(if $(filter art-ai,$(ART)),$(ART_AI_CHAR),16)
export BM_CHAR_SIZE := $(CHAR_SIZE)
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

.PHONY: all host check test windows armhf dist screenshots bench clean assets placeholders golden preview art art-ai art-review tiles-code tiles-compare
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
# (ART and CHAR_SIZE: at the top)
# Terrain tileset: code = tools/make_tiles.py (drawn in code, colours from the AI tiles; default),
# ai = the art's own tiles.png (placeholders + imported AI tiles), ai_v2 = the low-detail AI set
# (tile_v2_* rows of the art TODO, assembled by art_sync into $(ART)/tilesets/ai_v2)
TILESET ?= code
TILESET_DIR_code  = games/$(GAME)/art/tilesets/code
TILESET_DIR_ai    =
TILESET_DIR_ai_v2 = games/$(GAME)/$(ART)/tilesets/ai_v2
TILESET_DIR = $(TILESET_DIR_$(TILESET))
# the asset build's settings: another art set, character size or tileset regenerates the assets
ASSET_STAMP = build/gen/$(GAME)/config-$(TILESET)-$(CHAR_SIZE)-$(subst /,_,$(subst .,,$(ART))).stamp
GAME_ART    = $(wildcard games/$(GAME)/$(ART)/*.png games/$(GAME)/$(ART)/overlays.txt) \
              $(if $(TILESET_DIR),$(wildcard $(TILESET_DIR)/*.png))
GAME_LEVELS = $(wildcard games/$(GAME)/levels/*.txt) $(wildcard games/$(GAME)/arenas/*.txt)
$(ASSET_STAMP):
	@mkdir -p $(dir $@); rm -f build/gen/$(GAME)/config-*.stamp build/gen/$(GAME)/tileset-*.stamp; touch $@
$(GAME_GEN): games/$(GAME)/tools/build_assets.py tools/rsasset.py tools/sheets.py $(GAME_ART) $(GAME_LEVELS) \
             $(wildcard games/$(GAME)/tools/*.py) $(ASSET_STAMP)
	@mkdir -p $(dir $@)
	$(PYTHON) games/$(GAME)/tools/build_assets.py --out $(dir $@) --art games/$(GAME)/$(ART) \
	    $(if $(TILESET_DIR),--tileset $(TILESET_DIR))
assets: $(GAME_GEN)
# the code-drawn tileset (committed; redrawn when its script changes)
tiles-code: games/$(GAME)/art/tilesets/code/tiles.png
games/$(GAME)/art/tilesets/code/tiles.png: tools/make_tiles.py tools/sheets.py
	$(PYTHON) tools/make_tiles.py --out $(dir $@)

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
	    -lshell32 -lversion -luuid -lsetupapi -static-libgcc -s
	cp games/$(GAME)/dist/README-windows.txt dist/windows/README-$(GAME_NAME).txt
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
build/host/test_libretro: build/host/sdk/tests/test_libretro.o
	$(HOST_CC) -o $@ $^ -ldl

test check: build/host/test_sdk build/host/test_libretro build/host/$(GAME)_headless build/host/$(GAME)_libretro.so
	./build/host/test_sdk --golden sdk/tests/golden --out build
	./build/host/test_libretro build/host/$(GAME)_libretro.so 600
	$(PYTHON) tools/tests/test_tools.py
	$(PYTHON) tools/tests/test_art_sync.py
	$(PYTHON) tools/tests/test_art_consistency.py
	$(PYTHON) tools/tests/test_art_review.py
	$(PYTHON) games/$(GAME)/tools/check_levels.py
	sh games/$(GAME)/tests/smoke_test.sh build/host/$(GAME)_headless build
	sh games/$(GAME)/tests/feature_test.sh build/host/$(GAME)_headless
	$(PYTHON) games/$(GAME)/tests/facing_capture.py build/host/$(GAME)_headless build/facing_capture.png
golden: build/host/test_sdk
	./build/host/test_sdk --update --golden sdk/tests/golden --out build

# the validated-only set, games/bombermole/art/ (16-px characters): the owner's VALIDATED strips
art:
	$(PYTHON) tools/art_sync.py sync --char-size 16

# ---- the default art set: games/bombermole/art-ai/ (committed) ------------------------------------------
# Every GENERATED and VALIDATED strip of art/incoming/TODO.md, the placeholders for the TODO rows, the
# REJECTED rows left out; ART_AI_CHAR-px characters. Run it when new strips arrive or statuses change, then
# commit art-ai/ (the sheets and REPORT.md; the strips stay in art/incoming, which the RetroStoneOS build
# and its CI do not copy, so they build from art-ai/ as it is committed).
ART_AI_DIR = games/bombermole/art-ai
art-ai:
	rm -rf $(ART_AI_DIR)
	$(PYTHON) tools/art_sync.py sync --include-generated --char-size $(ART_AI_CHAR) \
	    --out $(ART_AI_DIR) --report $(ART_AI_DIR)/REPORT.md

# ---- AI art preview: ALL generated art whatever its status, 24-px characters -------------------
# dist/windows/BomberMole-preview.exe + docs/art-preview/ingame-ai-*.png, straight from art/incoming
# (the default build uses art-ai/, the same art as of the last make art-ai), with the tileset
# comparisons; the normal build's generated assets are rebuilt afterwards.
PREVIEW_CHAR ?= 24
PREVIEW_TILESET ?= code
PREVIEW_ART  := build/art-preview-$(PREVIEW_CHAR)
preview:
	$(PYTHON) tools/art_sync.py sync --include-generated --char-size $(PREVIEW_CHAR) \
	    --out $(PREVIEW_ART) --report $(PREVIEW_ART)/REPORT.md
	rm -f $(GAME_GEN)
	$(MAKE) build/host/$(GAME)_headless ART=../../$(PREVIEW_ART) CHAR_SIZE=$(PREVIEW_CHAR) TILESET=ai
	cp build/host/$(GAME)_headless build/host/$(GAME)_headless_tiles_ai
	$(MAKE) build/host/$(GAME)_headless ART=../../$(PREVIEW_ART) CHAR_SIZE=$(PREVIEW_CHAR) TILESET=ai_v2
	cp build/host/$(GAME)_headless build/host/$(GAME)_headless_tiles_ai_v2
	$(MAKE) windows GAME_NAME=$(GAME_NAME)-preview ART=../../$(PREVIEW_ART) CHAR_SIZE=$(PREVIEW_CHAR) TILESET=$(PREVIEW_TILESET)
	$(MAKE) build/host/$(GAME)_headless ART=../../$(PREVIEW_ART) CHAR_SIZE=$(PREVIEW_CHAR) TILESET=$(PREVIEW_TILESET)
	sh tools/art_preview_shots.sh build/host/$(GAME)_headless docs/art-preview
	$(PYTHON) tools/tiles_compare.py build/host/$(GAME)_headless_tiles_ai "current AI tiles" \
	    build/host/$(GAME)_headless "tileset $(PREVIEW_TILESET)" docs/art-preview/tiles-compare.png
	$(PYTHON) tools/tiles_compare.py build/host/$(GAME)_headless "tileset $(PREVIEW_TILESET)" \
	    build/host/$(GAME)_headless_tiles_ai_v2 "AI tiles v2" docs/art-preview/tiles-compare-v2.png all
	$(PYTHON) games/$(GAME)/tests/facing_capture.py build/host/$(GAME)_headless docs/art-preview/facing-capture-ai.png
	$(PYTHON) tools/mole_helmets.py --out docs/art-preview/mole-helmets.png
	$(PYTHON) tools/art_variants.py --out docs/art-preview/enemy_variants.png
	rm -f $(GAME_GEN)

art-review:
	$(PYTHON) tools/art_review.py

placeholders:
	BM_CHAR_SIZE=16 $(PYTHON) tools/make_placeholders.py --out games/$(GAME)/art

screenshots: build/host/$(GAME)_headless
	sh games/$(GAME)/tools/screenshots.sh build/host/$(GAME)_headless docs/screenshots

bench: build/host/$(GAME)_headless build/host/test_sdk
	sh games/$(GAME)/tools/bench.sh build/host/$(GAME)_headless
	./build/host/test_sdk --bench

clean:
	rm -rf build/host build/win64 build/armhf build/gen

-include $(shell find build -name '*.d' 2>/dev/null)

# ---- other games: each brings its own targets (games/<game>/game.mk, e.g. make leadysquid-check) ----
include $(wildcard games/*/game.mk)
