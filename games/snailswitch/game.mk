# Snail Switch (games/snailswitch): its make targets, included by the root Makefile.
#
#   make snailswitch                host builds: snailswitch (SDL2), snailswitch_headless, snailswitch_libretro.so
#   make snailswitch-check          all its tests (the rules, the course: solver + difficulty + variety, libretro loader,
#                                  smoke + bot + determinism, UI screens, save states, audit),
#                                  at 320 and then again at each widescreen width of WIDTHS (427 520), and the game
#                                  across widths (tests/wide_test.sh)
#   make snailswitch-dist           dist/windows/SnailSwitch.exe, dist/libretro/snailswitch_libretro.so (+ .armhf.so)
#   make snailswitch-windows        only the Windows exe
#   make snailswitch-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make snailswitch-screenshots    games/snailswitch/docs/screenshots/
#   make snailswitch-art            redraw the code-drawn art (tools/make_art.py; commit art/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/snailswitch/LICENSE; the rules below follow the root Makefile.

SNAILSWITCH_MAKE = $(MAKE) --no-print-directory GAME=snailswitch GAME_NAME=SnailSwitch
SNAILSWITCH_DIR  = games/snailswitch
# the house style kit (games/common/src, docs/art-direction.md) is compiled in
HOUSE_UI_snailswitch = 1
# widescreen (docs/spec.md "Widescreen"; rs_game.max_width): the checks run at 320 and at each width of WIDTHS
WIDESCREEN_snailswitch = 1

.PHONY: snailswitch snailswitch-check snailswitch-windows snailswitch-armhf snailswitch-dist snailswitch-screenshots snailswitch-art

snailswitch:
	+$(SNAILSWITCH_MAKE) host
snailswitch-windows:
	+$(SNAILSWITCH_MAKE) windows
snailswitch-armhf:
	+$(SNAILSWITCH_MAKE) armhf
snailswitch-dist:
	+$(SNAILSWITCH_MAKE) dist

SNAILSWITCH_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(SNAILSWITCH_DIR)/src/*.c) $(HOUSE_SRC))

snailswitch-check: build/host/snailswitch_test_physics
	./build/host/snailswitch_test_physics
	+$(SNAILSWITCH_MAKE) build/host/snailswitch_test_course
	./build/host/snailswitch_test_course
	+$(SNAILSWITCH_MAKE) build/host/snailswitch_test_scenery
	./build/host/snailswitch_test_scenery
	+$(SNAILSWITCH_MAKE) host build/host/test_libretro build/host/snailswitch_test_states
	./build/host/test_libretro build/host/snailswitch_libretro.so 600
	sh $(SNAILSWITCH_DIR)/tests/smoke_test.sh build/host/snailswitch_headless build
	sh $(SNAILSWITCH_DIR)/tests/ui_test.sh build/host/snailswitch_headless build/snailswitch-ui
	sh $(SNAILSWITCH_DIR)/tests/state_test.sh build/host/snailswitch_test_states build/states
	$(PYTHON) tools/state_audit.py --game snailswitch build/host/snailswitch_test_states $(SNAILSWITCH_OBJ_HOST)
	set -e; for w in $(WIDTHS); do echo "== widescreen: $$w x 240"; \
	    a=$$(case $$w in 427) echo 16:9;; 520) echo Wide;; esac); \
	    [ -z "$$a" ] || ./build/host/test_libretro build/host/snailswitch_libretro.so 600 $$a $$w; \
	    RS_WIDTH=$$w sh $(SNAILSWITCH_DIR)/tests/smoke_test.sh build/host/snailswitch_headless build/snailswitch-w$$w; \
	    RS_WIDTH=$$w sh $(SNAILSWITCH_DIR)/tests/ui_test.sh build/host/snailswitch_headless build/snailswitch-ui-$$w; \
	    RS_WIDTH=$$w sh $(SNAILSWITCH_DIR)/tests/state_test.sh build/host/snailswitch_test_states build/snailswitch-states-$$w; \
	done
	sh $(SNAILSWITCH_DIR)/tests/wide_test.sh build/host/snailswitch_headless build/host/snailswitch_test_states build "$(WIDTHS)"
	bash tools/i18n_test.sh build/host/snailswitch_headless snailswitch build/snailswitch-i18n $(if $(WIDESCREEN_snailswitch),320 $(WIDTHS),320)

snailswitch-screenshots:
	+$(SNAILSWITCH_MAKE) build/host/snailswitch_headless
	sh $(SNAILSWITCH_DIR)/tests/ui_test.sh build/host/snailswitch_headless $(SNAILSWITCH_DIR)/docs/screenshots 2

snailswitch-art:
	$(PYTHON) $(SNAILSWITCH_DIR)/tools/make_art.py
# the rules and the course (play.c, course.c, plan.c) and their headers: the tests below build them directly
SNAILSWITCH_RULES = $(SNAILSWITCH_DIR)/src/play.c $(SNAILSWITCH_DIR)/src/course.c $(SNAILSWITCH_DIR)/src/plan.c
SNAILSWITCH_HDRS  = $(wildcard $(SNAILSWITCH_DIR)/src/*.h)
build/host/snailswitch_test_physics: games/snailswitch/tests/test_physics.c $(SNAILSWITCH_RULES) $(SNAILSWITCH_HDRS) build/host/librs.a
	gcc -O2 -Igames/snailswitch/src -Isdk/include $(filter %.c %.a,$^) -lm -o $@

# the course's proofs and measures (tests/test_course.c: the solver, the measured difficulty, the variety)
build/host/snailswitch_test_course: games/snailswitch/tests/test_course.c $(SNAILSWITCH_RULES) $(SNAILSWITCH_HDRS)
	gcc -O2 $(WARN) -Igames/snailswitch/src -Isdk/include $(filter %.c,$^) -lm -o $@

build/host/snailswitch_test_scenery: games/snailswitch/tests/test_scenery.c games/snailswitch/src/draw.c $(SNAILSWITCH_RULES) $(SNAILSWITCH_HDRS) build/gen/snailswitch/assets.c build/gen/snailswitch/game_text.c build/host/librs.a
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) $(SDK_INC) -Igames/snailswitch/src -Ibuild/gen/snailswitch -Igames/common/src games/snailswitch/tests/test_scenery.c games/snailswitch/src/draw.c $(SNAILSWITCH_RULES) build/gen/snailswitch/assets.c build/gen/snailswitch/game_text.c games/common/src/house_ui.c build/host/librs.a -lm -o $@


