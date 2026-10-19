# Goat Gorge (games/goatgorge): its make targets, included by the root Makefile.
#
#   make goatgorge                host builds: goatgorge (SDL2), goatgorge_headless, goatgorge_libretro.so
#   make goatgorge-check          all its tests: the rules and every room's recorded solution (tests/test_rooms.c),
#                                 the libretro loader, the smoke tests (flow, chapters, pause, save RAM, the bot climbing
#                                 every chapter, determinism), the UI screens, the save states and their audit, at 320
#                                 and at each widescreen width of WIDTHS (427 520), states across widths, the tall
#                                 screen and the phone turning (tests/tall_test.sh), the frame cost, the 22 languages
#   make goatgorge-solve          re-record the solutions (tests/solver.c -> levels/solutions.txt) after a room changed
#   make goatgorge-dist           dist/windows/GoatGorge.exe, dist/libretro/goatgorge_libretro.so (+ .armhf.so)
#   make goatgorge-windows        only the Windows exe
#   make goatgorge-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make goatgorge-screenshots    games/goatgorge/docs/screenshots/
#   make goatgorge-art            redraw the code-drawn art (tools/make_art.py; commit art/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/goatgorge/LICENSE; the rules below follow the root Makefile.

GOATGORGE_MAKE = $(MAKE) --no-print-directory GAME=goatgorge GAME_NAME=GoatGorge
GOATGORGE_DIR  = games/goatgorge
# the house style kit (games/common/src, docs/art-direction.md) is compiled in
HOUSE_UI_goatgorge = 1
# widescreen (docs/spec.md "Widescreen"; rs_game.max_width): the checks run at 320 and again at each width of WIDTHS
WIDESCREEN_goatgorge = 1

.PHONY: goatgorge goatgorge-check goatgorge-windows goatgorge-armhf goatgorge-dist goatgorge-screenshots goatgorge-art \
        goatgorge-solve

goatgorge:
	+$(GOATGORGE_MAKE) host
goatgorge-windows:
	+$(GOATGORGE_MAKE) windows
goatgorge-armhf:
	+$(GOATGORGE_MAKE) armhf
goatgorge-dist:
	+$(GOATGORGE_MAKE) dist

GOATGORGE_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(GOATGORGE_DIR)/src/*.c) $(HOUSE_SRC))

# the rules alone (world.c) with their tests, and the solver
build/host/goatgorge_rooms: $(GOATGORGE_DIR)/src/world.c $(GOATGORGE_DIR)/src/game.h $(GOATGORGE_DIR)/tests/test_rooms.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -I$(GOATGORGE_DIR)/src $(GOATGORGE_DIR)/src/world.c $(GOATGORGE_DIR)/tests/test_rooms.c -o $@
build/host/goatgorge_solver: $(GOATGORGE_DIR)/src/world.c $(GOATGORGE_DIR)/src/game.h $(GOATGORGE_DIR)/tests/solver.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -I$(GOATGORGE_DIR)/src $(GOATGORGE_DIR)/src/world.c $(GOATGORGE_DIR)/tests/solver.c -o $@

goatgorge-solve: build/host/goatgorge_solver
	sh $(GOATGORGE_DIR)/tools/solve_all.sh build/host/goatgorge_solver $(GOATGORGE_DIR)/levels

goatgorge-check:
	+$(GOATGORGE_MAKE) host build/host/test_libretro build/host/goatgorge_test_states build/host/goatgorge_rooms
	./build/host/goatgorge_rooms $(GOATGORGE_DIR)/levels
	./build/host/test_libretro build/host/goatgorge_libretro.so 600
	sh $(GOATGORGE_DIR)/tests/smoke_test.sh build/host/goatgorge_headless build
	sh $(GOATGORGE_DIR)/tests/ui_test.sh build/host/goatgorge_headless build/goatgorge-ui
	sh $(GOATGORGE_DIR)/tests/state_test.sh build/host/goatgorge_test_states build/states
	$(PYTHON) tools/state_audit.py --game goatgorge build/host/goatgorge_test_states $(GOATGORGE_OBJ_HOST)
	set -e; for w in $(WIDTHS); do echo "== widescreen: $$w x 240"; \
	    a=$$(case $$w in 427) echo 16:9;; 520) echo Wide;; esac); \
	    [ -z "$$a" ] || ./build/host/test_libretro build/host/goatgorge_libretro.so 600 $$a $$w; \
	    RS_WIDTH=$$w sh $(GOATGORGE_DIR)/tests/smoke_test.sh build/host/goatgorge_headless build/w$$w quick; \
	    RS_WIDTH=$$w sh $(GOATGORGE_DIR)/tests/ui_test.sh build/host/goatgorge_headless build/goatgorge-ui-$$w; \
	    RS_WIDTH=$$w sh $(GOATGORGE_DIR)/tests/state_test.sh build/host/goatgorge_test_states build/states-$$w quick; \
	    for o in 320 $(WIDTHS); do [ $$o = $$w ] || ./build/host/goatgorge_test_states --width $$o --opt bot=1 \
	        --opt ready=1 --input build/states-$$w/goatgorge-run.input --resume build/states-$$w/goatgorge-run \
	        --any-width | grep -q "all passed" || { echo "FAIL a state saved at $$w does not load at $$o"; exit 1; }; done; \
	    echo "  ok   a state saved at $$w loads at the other widths"; \
	done
	sh $(GOATGORGE_DIR)/tests/tall_test.sh build/host/goatgorge_headless build
	sh $(GOATGORGE_DIR)/tests/bench_test.sh build/host/goatgorge_headless
	bash tools/i18n_test.sh build/host/goatgorge_headless goatgorge build/goatgorge-i18n 320 $(WIDTHS)

goatgorge-screenshots:
	+$(GOATGORGE_MAKE) build/host/goatgorge_headless
	sh $(GOATGORGE_DIR)/tests/ui_test.sh build/host/goatgorge_headless $(GOATGORGE_DIR)/docs/screenshots 1 all

goatgorge-art:
	$(PYTHON) $(GOATGORGE_DIR)/tools/make_art.py
