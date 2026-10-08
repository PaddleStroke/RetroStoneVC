# RetroStone Arcade: the bundle build (the launcher and several games in one program). Run from the repository root:
#
#   make -f apps/launcher/bundle.mk                      build/bundle/host/: the SDL2 app and the headless runner
#   make -f apps/launcher/bundle.mk check                the bundle's tests (tests/review_test.c, tests/bundle_test.sh)
#   make -f apps/launcher/bundle.mk PLATFORM=win64 check  the same tests on the Windows build (WSL runs the .exe)
#   make -f apps/launcher/bundle.mk PLATFORM=win64       dist/windows/<EXE_NAME>.exe (SDL2 linked in)
#   make -f apps/launcher/bundle.mk PLATFORM=android-arm64-v8a    build/bundle/android-arm64-v8a/libmain.so
#        (also android-armeabi-v7a, android-x86_64; apps/android/build.sh does it all)
#   make -f apps/launcher/bundle.mk shots                docs: the launcher's screenshots (apps/launcher/docs/)
#   ... CONF=apps/android/app-full.conf                  the same for the full (paid) app: every game playable
#
# The app's identity and its games: apps/android/app.conf (the free app, the default) or app-full.conf (CONF=...).
# Everything is built in build/bundle/<platform>/ (the full app: build/bundle/<platform>-full/):
# the root Makefile's build/host etc. are not touched, only its generated game assets are used (through
# apps/launcher/tools/game_assets.sh, under the lock /tmp/rsvc-build.lock).
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).

CONF     ?= apps/android/app.conf
conf      = $(shell sed -n 's/^$(1)=//p' $(CONF))
PYTHON   ?= python3
PROFILE    := $(or $(call conf,PROFILE),free)
EXE_NAME   := $(call conf,EXE_NAME)
# the playable games (modules), in menu order: FREE_GAMES, or every game of games/ when FREE_GAMES=all
GAMES      := $(shell $(PYTHON) apps/launcher/tools/gen_catalog.py --conf $(CONF) --list)
TAG        := $(if $(filter free,$(PROFILE)),,-$(PROFILE))
PLATFORM ?= host
OPT      ?= -O2
BD       := build/bundle/$(PLATFORM)$(TAG)
GEN      := build/bundle/gen$(TAG)
APP      := retrostonearcade
EXE      := $(if $(filter win64,$(PLATFORM)),.exe)
HEADLESS  = $(BD)/$(APP)_headless$(EXE)

# ---- toolchains ------------------------------------------------------------------------------------------------------
ifeq ($(PLATFORM),host)
CC := gcc
LD := ld
OBJCOPY := objcopy
OBJDUMP := objdump
NM := nm
AR := ar
FLAVOR := elf
PCFLAGS := -fPIC
SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS := $(shell sdl2-config --libs)
else ifeq ($(PLATFORM),win64)
CC := x86_64-w64-mingw32-gcc
LD := x86_64-w64-mingw32-ld
OBJCOPY := x86_64-w64-mingw32-objcopy
OBJDUMP := x86_64-w64-mingw32-objdump
NM := x86_64-w64-mingw32-nm
AR := x86_64-w64-mingw32-ar
FLAVOR := coff
# the small code model: no .refptr indirections (they are shared between objects, so they would cross modules)
PCFLAGS := -mcmodel=small -D__USE_MINGW_ANSI_STDIO=1
SDL2_MINGW ?= build/deps/SDL2-2.32.10/x86_64-w64-mingw32
SDL_CFLAGS := -I$(SDL2_MINGW)/include/SDL2
SDL_LIBS := -L$(SDL2_MINGW)/lib -static -lmingw32 -lSDL2main -lSDL2 -mwindows -lm -ldinput8 -ldxguid -ldxerr8 \
            -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lversion -luuid -lsetupapi \
            -static-libgcc -s
else ifneq ($(filter android-%,$(PLATFORM)),)
ABI := $(patsubst android-%,%,$(PLATFORM))
NDK ?= $(ANDROID_NDK_HOME)
# the apps' minSdk (apps/android/app/build.gradle)
API ?= 24
TC := $(NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin
TRIPLE_arm64-v8a := aarch64-linux-android
TRIPLE_armeabi-v7a := armv7a-linux-androideabi
TRIPLE_x86_64 := x86_64-linux-android
CC := $(TC)/$(TRIPLE_$(ABI))$(API)-clang
LD := $(TC)/ld.lld
OBJCOPY := $(TC)/llvm-objcopy
OBJDUMP := $(TC)/llvm-objdump
NM := $(TC)/llvm-nm
AR := $(TC)/llvm-ar
FLAVOR := elf
PCFLAGS := -fPIC
SDL_SRC ?= build/deps/src/SDL2-2.32.10
SDL_LIBDIR ?= build/android/sdl2/libs/$(ABI)
SDL_CFLAGS := -I$(SDL_SRC)/include
SDL_LIBS := -L$(SDL_LIBDIR) -lSDL2 -llog -landroid -lm
else
$(error PLATFORM: host, win64 or android-<abi>)
endif

WARN     = -Wall -Wextra -Wno-unused-parameter
CSTD     = -std=c11 -D_POSIX_C_SOURCE=200809L
SDK_INC  = -Isdk/include -Isdk/src -Isdk/third_party/libxmp-lite/include/libxmp-lite \
           -Isdk/third_party/stb -Isdk/third_party/libretro -Isdk/frontends/common
XMP_DEFS = -DLIBXMP_CORE_PLAYER -DLIBXMP_STATIC -DLIBXMP_NO_DEPACKERS
CFLAGS_ALL = $(CSTD) $(OPT) $(WARN) -fno-common $(PCFLAGS) $(SDK_INC)
APP_INC  = -Iapps/launcher/src -Igames/common/src -I$(GEN)/launcher

SDK_SRC  = $(wildcard sdk/src/*.c)
XMP_SRC  = $(wildcard sdk/third_party/libxmp-lite/src/*.c) $(wildcard sdk/third_party/libxmp-lite/src/loaders/*.c)
HOUSE_SRC = $(wildcard games/common/src/*.c)
APP_SRC  = apps/launcher/src/launcher.c apps/launcher/src/rsb_runtime.c apps/launcher/src/rsb_review.c \
           sdk/frontends/common/rs_desktop.c
o = $(patsubst %.c,$(BD)/obj/%.o,$(1))

# Two phases: the games' generated sources first (assets, translated text: whatever the root Makefile's "assets"
# target makes in build/gen/<id>/), then the build itself, in a sub-make that sees them. They are shared by the
# platforms: SKIP_ASSETS=1 uses the copies already in $(GEN) (apps/android/build.sh, after its first ABI).
.PHONY: all build gen check android-lib windows shots
PROGRAMS = $(if $(filter host,$(PLATFORM)),$(BD)/$(APP) $(BD)/$(APP)_headless,\
     $(if $(filter win64,$(PLATFORM)),dist/windows/$(EXE_NAME).exe $(HEADLESS),$(BD)/libmain.so))
SUBMAKE = $(MAKE) --no-print-directory -f apps/launcher/bundle.mk PLATFORM=$(PLATFORM)
all: gen
	+$(SUBMAKE) build
build: $(PROGRAMS)
gen:
	+$(if $(SKIP_ASSETS),true,sh apps/launcher/tools/game_assets.sh $(GEN) $(GAMES))
	sh tools/fetch_sdl2_src.sh build/deps
	$(PYTHON) apps/launcher/tools/launcher_text.py --out $(GEN)/launcher
	$(PYTHON) apps/launcher/tools/gen_credits.py --out $(GEN)
android-lib: all
windows:
	+$(MAKE) -f apps/launcher/bundle.mk PLATFORM=win64

# ---- compile ----------------------------------------------------------------------------------------------------------
$(BD)/obj/sdk/third_party/%.o: sdk/third_party/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) $(OPT) $(PCFLAGS) -w $(XMP_DEFS) -Isdk/third_party/libxmp-lite/include/libxmp-lite \
	    -Isdk/third_party/libxmp-lite/src -c $< -o $@
$(BD)/obj/apps/launcher/src/rsb_sdl.o: CFLAGS_ALL += $(SDL_CFLAGS)
$(BD)/obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_ALL) $(APP_INC) -MMD -c $< -o $@
$(BD)/obj/catalog.o: $(GEN)/catalog.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_ALL) $(APP_INC) -c $< -o $@
$(BD)/obj/credits.o: $(GEN)/credits.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_ALL) $(APP_INC) -c $< -o $@

# the launcher's strings (tools/launcher_text.py): its pack goes to the runtime (rsb_runtime.c), not to rs_i18n_register
$(BD)/obj/launcher_text.o: $(GEN)/launcher/game_text.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_ALL) $(APP_INC) -Drs_i18n_register=rsb_launcher_i18n_capture -c $< -o $@

$(BD)/librs.a: $(call o,$(SDK_SRC) $(XMP_SRC))
	@rm -f $@
	$(AR) rcs $@ $^

# ---- the catalogue (the cards, the touch schemes, the app's names) -------------------------------------------------------
$(GEN)/catalog.c: $(CONF) apps/launcher/games.cfg apps/launcher/tools/gen_catalog.py apps/launcher/tools/cards.py \
                  $(wildcard apps/launcher/cards/*) $(wildcard games/*/game.mk) $(wildcard games/*/docs/screenshots/*.png)
	$(PYTHON) apps/launcher/tools/gen_catalog.py --conf $(CONF) --out $(GEN)

# ---- modules: each free game, and the house kit -----------------------------------------------------------------------------
MODULIZE = $(PYTHON) apps/launcher/tools/modulize.py --flavor $(FLAVOR) --cc "$(CC)" --cflags "$(CFLAGS_ALL) $(APP_INC)" \
           --ld $(LD) --objcopy $(OBJCOPY) --objdump $(OBJDUMP) --nm $(NM)
MOD_DEPS = apps/launcher/tools/modulize.py apps/launcher/src/rsb_glue.c apps/launcher/src/rsb.h

$(BD)/mod/house.o: $(call o,$(HOUSE_SRC)) $(MOD_DEPS)
	@mkdir -p $(dir $@)
	$(MODULIZE) --house --id house --out $@ $(filter %.o,$^)

define GAME_RULES
$(BD)/games/$(1)/%.o: games/$(1)/src/%.c $(wildcard $(GEN)/$(1)/*.h)
	@mkdir -p $$(dir $$@)
	$(CC) $(CFLAGS_ALL) -Igames/$(1)/src -I$(GEN)/$(1) -Igames/common/src -MMD -c $$< -o $$@
$(BD)/games/$(1)/gen/%.o: $(GEN)/$(1)/%.c
	@mkdir -p $$(dir $$@)
	$(CC) $(CFLAGS_ALL) -Igames/$(1)/src -I$(GEN)/$(1) -Igames/common/src -c $$< -o $$@
$(BD)/mod/$(1).o: $(patsubst games/$(1)/src/%.c,$(BD)/games/$(1)/%.o,$(wildcard games/$(1)/src/*.c)) \
                  $(patsubst $(GEN)/$(1)/%.c,$(BD)/games/$(1)/gen/%.o,$(wildcard $(GEN)/$(1)/*.c)) $(MOD_DEPS)
	@mkdir -p $$(dir $$@)
	$(MODULIZE) --id $(1) --out $$@ $$(filter %.o,$$^)
endef
$(foreach g,$(GAMES),$(eval $(call GAME_RULES,$(g))))

MODS = $(BD)/mod/house.o $(foreach g,$(GAMES),$(BD)/mod/$(g).o)
CORE = $(call o,$(APP_SRC)) $(BD)/obj/catalog.o $(BD)/obj/credits.o $(BD)/obj/launcher_text.o $(MODS) $(BD)/librs.a

# ---- programs ---------------------------------------------------------------------------------------------------------------
$(BD)/$(APP): $(call o,apps/launcher/src/rsb_sdl.c) $(CORE)
	$(CC) -o $@ $^ $(SDL_LIBS) -lm
$(BD)/$(APP)_headless: $(call o,apps/launcher/src/rsb_headless.c) $(CORE)
	$(CC) -o $@ $^ -lm
$(BD)/$(APP)_headless.exe: $(call o,apps/launcher/src/rsb_headless.c) $(CORE)
	$(CC) -o $@ $^ -static -static-libgcc -lm
dist/windows/$(EXE_NAME).exe: $(call o,apps/launcher/src/rsb_sdl.c) $(CORE) $(SDL2_MINGW)/lib/libSDL2.a
	@mkdir -p $(dir $@)
	$(CC) -o $@ $(filter %.o %.a,$(filter-out %/libSDL2.a,$^)) $(SDL_LIBS)
	cp apps/launcher/dist/README-windows$(TAG).txt dist/windows/README-$(EXE_NAME).txt
	cp THIRD_PARTY.md dist/windows/THIRD_PARTY.md
	cp LICENSE-MIT dist/windows/LICENSE-MIT.txt
	cp LICENSE-CC-BY-NC-SA-4.0.txt dist/windows/LICENSE-CC-BY-NC-SA-4.0.txt
$(SDL2_MINGW)/lib/libSDL2.a:
	sh tools/fetch_sdl2_mingw.sh build/deps
$(BD)/libmain.so: $(call o,apps/launcher/src/rsb_sdl.c) $(CORE)
	$(CC) -shared -o $@ $^ -Wl,-soname,libmain.so -Wl,--no-undefined -Wl,-z,max-page-size=16384 \
	    -Wl,--build-id=sha1 $(SDL_LIBS)

# ---- tests and pictures -------------------------------------------------------------------------------------------------------
# the standalone games the tests compare with are built in the same wait for the lock as the assets, just before the
# bundle is compiled: both from the same moment of the games' sources
TESTS := build/bundle/tests-$(PLATFORM)$(TAG)
check:
	@mkdir -p $(TESTS)
	+STANDALONE=$(TESTS) $(MAKE) --no-print-directory -f apps/launcher/bundle.mk PLATFORM=$(PLATFORM) SKIP_ASSETS= all
	$(CC) $(CSTD) $(OPT) $(WARN) -Iapps/launcher/src -o $(TESTS)/review_test$(EXE) apps/launcher/tests/review_test.c \
	    apps/launcher/src/rsb_review.c
	$(TESTS)/review_test$(EXE) $(TESTS)
	PREBUILT=1 sh apps/launcher/tests/bundle_test.sh $(HEADLESS) $(TESTS) "$(GAMES)" $(PROFILE)
shots: all
	sh apps/launcher/tests/shots.sh $(HEADLESS) apps/launcher/docs

-include $(shell find $(BD) -type f -name '*.d' 2>/dev/null)
