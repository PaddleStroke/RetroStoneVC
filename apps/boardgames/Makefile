# RetroStone Board: board games for two on one device, in one app. Plain C + SDL2 (not the RetroStone VC SDK).
# Run from the repository root:
#   make -f apps/boardgames/Makefile            $(B)/host/retrostoneboard (Linux: the tests, the screenshots)
#   make -f apps/boardgames/Makefile test       every game's rules test (tests/perft.c, tests/<game>_test.c)
#   make -f apps/boardgames/Makefile shots      $(B)/shots/*.png: the menu, chess (needs python3 + Pillow)
#   make -f apps/boardgames/Makefile win64      dist/windows/RetroStoneBoard.exe (mingw, SDL2 static: build/deps)
# GAMES="chess go": build only these games (src/games/<id>*.c; default: all). B=<dir>: another build directory
# (parallel work: each its own).
# EDITION=boardfree: RetroStone Board Free (apps/android/app-boardfree.conf: its FREE_GAMES and FREE_LEVELS, the other
# games locked; win64: dist/windows/RetroStoneBoardFree.exe); the default, board: the paid app (app-board.conf, every
# game). CONF=<file>: another .conf. Read at build time by tools/gen_edition.py ($(B)/gen/edition.c, src/edition.h).
# DEMO=1: with a little game of levels (tests/levels_demo.c, "levelsdemo": the level screen's test), never shipped.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).

.DEFAULT_GOAL := host
D      := apps/boardgames
B      ?= build/boardgames
GAMES  ?= all
# a game built on another one's rules: NEEDS_<id> := those files, built with it when it is built alone (GAMES="id")
NEEDS_chesspuzzles := $(D)/src/games/chess.c
BOT_NEEDS_chess := $(D)/src/games/chess.c
GAME_SRC := $(if $(filter all,$(GAMES)),$(wildcard $(D)/src/games/*.c),\
              $(sort $(foreach g,$(GAMES),$(wildcard $(D)/src/games/$(g).c $(D)/src/games/$(g)_*.c) $(NEEDS_$(g)))))
GEN    := $(B)/gen
# the texts (the rules in 22 languages) and their fonts, cut down to them: tools/gen_text.py (python3, fontTools, the
# system's fonts-noto-cjk)
TEXT   := $(GEN)/text_data.c
# the pictures (art/: the tarot's 1898 faces, JPEG) as bytes: tools/gen_art.py (python3)
ART    := $(GEN)/art_data.c
# the edition: the app's name, the games and levels playable, the paid app (tools/gen_edition.py, from its .conf;
# written only when it changes)
EDITION ?= board
CONF   ?= apps/android/app-$(EDITION).conf
$(if $(wildcard $(CONF)),,$(error no edition $(EDITION): $(CONF) is missing))
EXE_NAME := $(shell sed -n 's/^EXE_NAME=//p' $(CONF))
ED     := $(GEN)/edition.c
# the in-app purchase's backend (apps/common/iap.h): none here (iap_none.c: the edition decides, the panel links to
# the paid app); IAP=1 in the .conf: Google Play Billing (apps/common/iap_play.c, a slot not written yet:
# apps/common/README.md). The iOS app (apps/ios) builds apps/ios/common/iap_storekit.swift instead.
IAP_SRC := apps/common/$(if $(filter 1,$(shell sed -n 's/^IAP=//p' $(CONF))),iap_play.c,iap_none.c)
$(if $(wildcard $(IAP_SRC)),,$(error $(CONF) says IAP=1: $(IAP_SRC) (Google Play Billing) is not written yet))
# the AI kit and the bots' registry (no SDL: the tests link them); ai_worker.c runs a bot on a thread (SDL)
AI_SRC := $(D)/src/ai.c $(D)/src/bots.c
SRC    := $(D)/src/shell.c $(D)/src/save.c $(D)/src/sound.c $(D)/src/art.c $(wildcard $(D)/src/card*.c) \
          $(AI_SRC) $(D)/src/ai_worker.c $(D)/src/text.c $(TEXT) $(ART) $(ED) $(GAME_SRC) $(IAP_SRC) \
          $(if $(DEMO),$(D)/tests/levels_demo.c)
HDR    := $(wildcard $(D)/src/*.h $(D)/src/games/*.h) apps/common/iap.h
CFLAGS := -O2 -std=c11 -Wall -Wextra -I$(D)/src -Iapps/common -Isdk/third_party/stb

$(TEXT): $(wildcard $(D)/help/*/*.txt $(D)/help/*.txt) $(D)/tools/gen_text.py
	python3 $(D)/tools/gen_text.py --out $(GEN)
$(ART): $(wildcard $(D)/art/*/*.jpg) $(D)/tools/gen_art.py
	python3 $(D)/tools/gen_art.py --out $(GEN)
# looked at on every build (the edition asked for may differ from the last one); its date moves only when it changes
$(ED): FORCE
	@python3 $(D)/tools/gen_edition.py --conf $(CONF) --out $(GEN)
.PHONY: FORCE
FORCE:
# a game's tests: tests/<id>_test.c (its rules) and tests/ai_<id>_test.c (its bot); the AI kit's always
TESTS  := $(if $(filter all,$(GAMES)),$(wildcard $(D)/tests/*_test.c),\
              $(D)/tests/ai_kit_test.c \
              $(foreach g,$(GAMES),$(wildcard $(D)/tests/$(g)_test.c $(D)/tests/ai_$(g)_test.c)))

SDL2_MINGW ?= build/deps/SDL2-2.32.10/x86_64-w64-mingw32
WIN_LIBS := -L$(SDL2_MINGW)/lib -static -lmingw32 -lSDL2main -lSDL2 -mwindows -lm -ldinput8 -ldxguid -ldxerr8 \
            -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lversion -luuid -lsetupapi \
            -static-libgcc -s

.PHONY: host test shots win64
host: $(B)/host/retrostoneboard

$(B)/host/retrostoneboard: $(SRC) $(HDR)
	@mkdir -p $(@D)
	gcc $(CFLAGS) $(shell sdl2-config --cflags) -o $@ $(SRC) $(shell sdl2-config --libs) -lm

# a game's test: tests/<id>_test.c with its rules (src/games/<id>.c), no SDL; chess: tests/perft.c
$(B)/host/perft: $(D)/tests/perft.c $(D)/src/games/chess.c $(HDR)
	@mkdir -p $(@D)
	gcc $(CFLAGS) -o $@ $(D)/tests/perft.c $(D)/src/games/chess.c

$(B)/host/%_test: $(D)/tests/%_test.c $(HDR) $(wildcard $(D)/src/games/*_rules.c)
	@mkdir -p $(@D)
	gcc $(CFLAGS) -o $@ $< $(wildcard $(D)/src/games/$*_rules.c) -lm

# the AI kit's test (toy games: no game's rules)
$(B)/host/ai_kit_test: $(D)/tests/ai_kit_test.c $(AI_SRC) $(HDR)
	@mkdir -p $(@D)
	gcc $(CFLAGS) -o $@ $< $(AI_SRC) -lm

# a game's bot test: tests/ai_<id>_test.c (self-play, headless) with the kit, the rules and the bot
# (src/games/<id>_rules.c, <id>_bot.c; a bot built on other files: BOT_NEEDS_<id> := those files). Its stem is
# shorter: it wins over %_test.
$(B)/host/ai_%_test: $(D)/tests/ai_%_test.c $(D)/tests/ai_selfplay.h $(AI_SRC) $(HDR) \
                     $(wildcard $(D)/src/games/*_rules.c $(D)/src/games/*_bot.c)
	@mkdir -p $(@D)
	gcc $(CFLAGS) -o $@ $< $(AI_SRC) $(wildcard $(D)/src/games/$*_rules.c $(D)/src/games/$*_bot.c) $(BOT_NEEDS_$*) -lm

test: $(if $(filter all chess,$(GAMES)),$(B)/host/perft) $(patsubst $(D)/tests/%.c,$(B)/host/%,$(TESTS))
	@for t in $^; do echo "== $$t"; ./$$t || exit 1; done

shots: $(B)/host/retrostoneboard
	@mkdir -p $(B)/shots
	$< --shot $(B)/shots/menu.bmp
	$< --shot $(B)/shots/chess.bmp --game chess --moves "e2e4 e7e5 g1f3 b8c6 f1c4 g8f6" --taps "0.31,0.59"
	$< --shot $(B)/shots/chess-mate.bmp --game chess --moves "e2e4 e7e5 f1c4 b8c6 d1h5 g8f6 h5f7"
	$< --shot $(B)/shots/chess-f2f.bmp --game chess --moves "e2e4 d7d5 e4d5 d8d5" --f2f
	$< --shot $(B)/shots/chess-wide.bmp --size 1280x720 --game chess --moves "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4"
	python3 -c "import glob; from PIL import Image; [Image.open(f).save(f[:-3] + 'png') for f in glob.glob('$(B)/shots/*.bmp')]"

win64: dist/windows/$(EXE_NAME).exe

dist/windows/$(EXE_NAME).exe: $(SRC) $(HDR)
	@mkdir -p $(@D)
	x86_64-w64-mingw32-gcc $(CFLAGS) -I$(SDL2_MINGW)/include/SDL2 -Dmain=SDL_main -o $@ $(SRC) $(WIN_LIBS)

# ---- Android (apps/android/build.sh board: the app RetroStone Board, apps/android/app-board.conf) --------------------
#   make -f apps/boardgames/Makefile android ABI=arm64-v8a NDK=<ndk>   $(B)/android-<abi>/libmain.so (SDL2 apart:
#        build/android/sdl2/libs/<abi>/libSDL2.so, made by build.sh); also armeabi-v7a, x86_64
#   make -f apps/boardgames/Makefile icon-art ICON_DIR=<dir>           the launcher icon's pictures (make_icon.py)
ABI    ?= arm64-v8a
NDK    ?= $(ANDROID_NDK_HOME)
NDK_TRIPLE = $(if $(filter arm64-v8a,$(ABI)),aarch64-linux-android,$(if $(filter armeabi-v7a,$(ABI)),armv7a-linux-androideabi,x86_64-linux-android))
NDK_CC  = $(NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin/$(NDK_TRIPLE)24-clang
SDL_SRC ?= build/deps/src/SDL2-2.32.10
AB      = $(B)/android-$(ABI)
.PHONY: android icon-art
android: $(AB)/libmain.so

$(AB)/obj/%.o: %.c $(HDR)
	@mkdir -p $(@D)
	$(NDK_CC) $(CFLAGS) -fPIC -I$(SDL_SRC)/include -c -o $@ $<

$(AB)/libmain.so: $(patsubst %.c,$(AB)/obj/%.o,$(SRC))
	$(NDK_CC) -shared -o $@ $^ -Wl,-soname,libmain.so -Wl,--no-undefined -Wl,-z,max-page-size=16384 \
	    -Wl,--build-id=sha1 -Lbuild/android/sdl2/libs/$(ABI) -lSDL2 -llog -landroid -lm

icon-art: $(B)/host/icon_art
	@mkdir -p $(ICON_DIR)
	$< $(ICON_DIR)

$(B)/host/icon_art: $(D)/android/icon_art.c $(SRC) $(HDR)
	@mkdir -p $(@D)
	gcc $(CFLAGS) $(shell sdl2-config --cflags) -Dmain=board_main -c -o $(B)/host/icon_shell.o $(D)/src/shell.c
	gcc $(CFLAGS) $(shell sdl2-config --cflags) -o $@ $< $(B)/host/icon_shell.o $(filter-out %/shell.c,$(SRC)) \
	    $(shell sdl2-config --libs) -lm
