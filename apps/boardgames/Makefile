# RetroStone Board (prototype): chess for two on one device. Plain C + SDL2 (not the RetroStone VC SDK).
# Run from the repository root:
#   make -f apps/boardgames/Makefile            build/boardgames/host/chess (Linux: the tests, the screenshots)
#   make -f apps/boardgames/Makefile test       the rules: perft counts, mate and draw detection
#   make -f apps/boardgames/Makefile shots      build/boardgames/shots/*.png (needs python3 + Pillow)
#   make -f apps/boardgames/Makefile win64      dist/windows/RetroStoneChess.exe (mingw, SDL2 static: build/deps)
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).

D      := apps/boardgames
B      := build/boardgames
SRC    := $(D)/src/chess.c $(D)/src/art.c $(D)/src/main.c
HDR    := $(wildcard $(D)/src/*.h)
CFLAGS := -O2 -std=c11 -Wall -Wextra -I$(D)/src

SDL2_MINGW ?= build/deps/SDL2-2.32.10/x86_64-w64-mingw32
WIN_LIBS := -L$(SDL2_MINGW)/lib -static -lmingw32 -lSDL2main -lSDL2 -mwindows -lm -ldinput8 -ldxguid -ldxerr8 \
            -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lversion -luuid -lsetupapi \
            -static-libgcc -s

.PHONY: host test shots win64
host: $(B)/host/chess

$(B)/host/chess: $(SRC) $(HDR)
	@mkdir -p $(@D)
	gcc $(CFLAGS) $(shell sdl2-config --cflags) -o $@ $(SRC) $(shell sdl2-config --libs) -lm

$(B)/host/perft: $(D)/tests/perft.c $(D)/src/chess.c $(HDR)
	@mkdir -p $(@D)
	gcc $(CFLAGS) -o $@ $(D)/tests/perft.c $(D)/src/chess.c

test: $(B)/host/perft
	$(B)/host/perft

# a few positions: the start, a selection, a check, a promotion, mate, a draw, landscape
shots: $(B)/host/chess
	@mkdir -p $(B)/shots
	$< --shot $(B)/shots/start.bmp
	$< --shot $(B)/shots/select.bmp --moves "e2e4 e7e5 g1f3 b8c6 f1c4 g8f6" --select c4
	$< --shot $(B)/shots/check.bmp --moves "e2e4 e7e5 f1c4 b8c6 d1h5 g8f6" --select h5
	$< --shot $(B)/shots/mate.bmp --moves "e2e4 e7e5 f1c4 b8c6 d1h5 g8f6 h5f7"
	$< --shot $(B)/shots/promote.bmp --moves "a2a4 b7b5 a4b5 a7a6 b5a6 c8b7 a6a7 b8c6" --promote a7a8
	$< --shot $(B)/shots/flipped.bmp --moves "d2d4 d7d5 c2c4 d5c4 e2e4 b7b5" --flip
	$< --shot $(B)/shots/wide.bmp --size 1280x720 --moves "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 a7a6"
	python3 -c "import glob; from PIL import Image; [Image.open(f).save(f[:-3] + 'png') for f in glob.glob('$(B)/shots/*.bmp')]"

win64: dist/windows/RetroStoneChess.exe

dist/windows/RetroStoneChess.exe: $(SRC) $(HDR)
	@mkdir -p $(@D)
	x86_64-w64-mingw32-gcc $(CFLAGS) -I$(SDL2_MINGW)/include/SDL2 -Dmain=SDL_main -o $@ $(SRC) $(WIN_LIBS)
