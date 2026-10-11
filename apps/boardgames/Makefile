# RetroStone Board: board games for two on one device, in one app. Plain C + SDL2 (not the RetroStone VC SDK).
# Run from the repository root:
#   make -f apps/boardgames/Makefile            $(B)/host/retrostoneboard (Linux: the tests, the screenshots)
#   make -f apps/boardgames/Makefile test       every game's rules test (tests/perft.c, tests/<game>_test.c)
#   make -f apps/boardgames/Makefile shots      $(B)/shots/*.png: the menu, chess (needs python3 + Pillow)
#   make -f apps/boardgames/Makefile win64      dist/windows/RetroStoneBoard.exe (mingw, SDL2 static: build/deps)
# GAMES="chess go": build only these games (src/games/<id>*.c; default: all). B=<dir>: another build directory
# (parallel work: each its own).
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).

.DEFAULT_GOAL := host
D      := apps/boardgames
B      ?= build/boardgames
GAMES  ?= all
GAME_SRC := $(if $(filter all,$(GAMES)),$(wildcard $(D)/src/games/*.c),\
              $(foreach g,$(GAMES),$(wildcard $(D)/src/games/$(g).c $(D)/src/games/$(g)_*.c)))
GEN    := $(B)/gen
# the texts (the rules in 22 languages) and their fonts, cut down to them: tools/gen_text.py (python3, fontTools, the
# system's fonts-noto-cjk)
TEXT   := $(GEN)/text_data.c
SRC    := $(D)/src/shell.c $(D)/src/sound.c $(D)/src/art.c $(D)/src/cards.c $(D)/src/text.c $(TEXT) $(GAME_SRC)
HDR    := $(wildcard $(D)/src/*.h $(D)/src/games/*.h)
CFLAGS := -O2 -std=c11 -Wall -Wextra -I$(D)/src -Isdk/third_party/stb

$(TEXT): $(wildcard $(D)/help/*/*.txt $(D)/help/*.txt) $(D)/tools/gen_text.py
	python3 $(D)/tools/gen_text.py --out $(GEN)
TESTS  := $(if $(filter all,$(GAMES)),$(wildcard $(D)/tests/*_test.c),\
              $(foreach g,$(GAMES),$(wildcard $(D)/tests/$(g)_test.c)))

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

win64: dist/windows/RetroStoneBoard.exe

dist/windows/RetroStoneBoard.exe: $(SRC) $(HDR)
	@mkdir -p $(@D)
	x86_64-w64-mingw32-gcc $(CFLAGS) -I$(SDL2_MINGW)/include/SDL2 -Dmain=SDL_main -o $@ $(SRC) $(WIN_LIBS)
