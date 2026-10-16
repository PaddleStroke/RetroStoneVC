# Llama Drama (games/llamadrama): its make targets, included by the root Makefile.
#
#   make llamadrama                host builds: llamadrama (SDL2), llamadrama_headless, llamadrama_libretro.so
#   make llamadrama-check          all its tests (the rules alone, libretro loader, smoke + bot + determinism, touch,
#                                  UI screens, save states and their audit, the 22 languages), at 320 and at each
#                                  widescreen width of WIDTHS (427 520), and save states across widths
#   make llamadrama-dist           dist/windows/LlamaDrama.exe, dist/libretro/llamadrama_libretro.so (+ .armhf.so)
#   make llamadrama-windows        only the Windows exe
#   make llamadrama-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make llamadrama-screenshots    games/llamadrama/docs/screenshots/
#   make llamadrama-art            redraw the code-drawn art (tools/make_art.py; commit art/)
#   make llamadrama-music-alt      the two alternative tunes (tools/make_music_alt.py -> music-alt/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/llamadrama/LICENSE; the rules below follow the root Makefile.

LLAMADRAMA_MAKE = $(MAKE) --no-print-directory GAME=llamadrama GAME_NAME=LlamaDrama
LLAMADRAMA_DIR  = games/llamadrama
# the house style kit (games/common/src, docs/art-direction.md) is compiled in
HOUSE_UI_llamadrama = 1
# widescreen (docs/spec.md "Widescreen"; rs_game.max_width): the checks run at 320 and again at each width of WIDTHS
WIDESCREEN_llamadrama = 1

.PHONY: llamadrama llamadrama-check llamadrama-windows llamadrama-armhf llamadrama-dist llamadrama-screenshots \
        llamadrama-art llamadrama-music-alt

llamadrama:
	+$(LLAMADRAMA_MAKE) host
llamadrama-windows:
	+$(LLAMADRAMA_MAKE) windows
llamadrama-armhf:
	+$(LLAMADRAMA_MAKE) armhf
llamadrama-dist:
	+$(LLAMADRAMA_MAKE) dist

LLAMADRAMA_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(LLAMADRAMA_DIR)/src/*.c) $(HOUSE_SRC))

# the rules' test links play.c alone
build/host/llamadrama_rules: $(LLAMADRAMA_DIR)/src/play.c $(LLAMADRAMA_DIR)/src/game.h $(LLAMADRAMA_DIR)/tests/test_rules.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(CSTD) $(OPT) $(WARN) -Isdk/include -I$(LLAMADRAMA_DIR)/src $(LLAMADRAMA_DIR)/src/play.c \
	    $(LLAMADRAMA_DIR)/tests/test_rules.c -o $@

llamadrama-check:
	+$(LLAMADRAMA_MAKE) host build/host/test_libretro build/host/llamadrama_test_states build/host/llamadrama_rules
	./build/host/llamadrama_rules
	./build/host/test_libretro build/host/llamadrama_libretro.so 600
	sh $(LLAMADRAMA_DIR)/tests/smoke_test.sh build/host/llamadrama_headless build
	sh $(LLAMADRAMA_DIR)/tests/touch_test.sh build/host/llamadrama_headless build "$(WIDTHS)"
	sh $(LLAMADRAMA_DIR)/tests/ui_test.sh build/host/llamadrama_headless build/llamadrama-ui
	sh $(LLAMADRAMA_DIR)/tests/state_test.sh build/host/llamadrama_test_states build/states
	$(PYTHON) tools/state_audit.py --game llamadrama build/host/llamadrama_test_states $(LLAMADRAMA_OBJ_HOST)
	set -e; for w in $(WIDTHS); do echo "== widescreen: $$w x 240"; \
	    a=$$(case $$w in 427) echo 16:9;; 520) echo Wide;; esac); \
	    [ -z "$$a" ] || ./build/host/test_libretro build/host/llamadrama_libretro.so 600 $$a $$w; \
	    RS_WIDTH=$$w sh $(LLAMADRAMA_DIR)/tests/smoke_test.sh build/host/llamadrama_headless build/w$$w quick; \
	    RS_WIDTH=$$w sh $(LLAMADRAMA_DIR)/tests/ui_test.sh build/host/llamadrama_headless build/llamadrama-ui-$$w; \
	    RS_WIDTH=$$w sh $(LLAMADRAMA_DIR)/tests/state_test.sh build/host/llamadrama_test_states build/states-$$w quick; \
	    for o in 320 $(WIDTHS); do [ $$o = $$w ] || ./build/host/llamadrama_test_states --width $$o --opt bot=1 \
	        --opt ready=1 --input build/states-$$w/llamadrama-run.input --resume build/states-$$w/llamadrama-run \
	        --any-width | grep -q "all passed" || { echo "FAIL a state saved at $$w does not load at $$o"; exit 1; }; done; \
	    echo "  ok   a state saved at $$w loads at the other widths"; \
	done
	bash tools/i18n_test.sh build/host/llamadrama_headless llamadrama build/llamadrama-i18n 320 $(WIDTHS)

llamadrama-screenshots:
	+$(LLAMADRAMA_MAKE) build/host/llamadrama_headless
	sh $(LLAMADRAMA_DIR)/tests/ui_test.sh build/host/llamadrama_headless $(LLAMADRAMA_DIR)/docs/screenshots 2 all

llamadrama-art:
	$(PYTHON) $(LLAMADRAMA_DIR)/tools/make_art.py

llamadrama-music-alt:
	$(PYTHON) $(LLAMADRAMA_DIR)/tools/make_music_alt.py
