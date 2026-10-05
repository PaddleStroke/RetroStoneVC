# Otter Rapids (games/otterrapids): its make targets, included by the root Makefile.
#
#   make otterrapids                host builds: otterrapids (SDL2), otterrapids_headless, otterrapids_libretro.so
#   make otterrapids-check          all its tests (libretro loader, smoke + bot + determinism, UI screens, save states, audit),
#                                   at 320 and then again at each widescreen width of WIDTHS (427 520), and across widths
#   make otterrapids-dist           dist/windows/OtterRapids.exe, dist/libretro/otterrapids_libretro.so (+ .armhf.so)
#   make otterrapids-windows        only the Windows exe
#   make otterrapids-armhf          the libretro core for the RetroStone2 (Cortex-A7)
#   make otterrapids-screenshots    games/otterrapids/docs/screenshots/
#   make otterrapids-art            redraw the code-drawn art (tools/make_art.py; commit art/)
#
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/otterrapids/LICENSE; the rules below follow the root Makefile.

OTTERRAPIDS_MAKE = $(MAKE) --no-print-directory GAME=otterrapids GAME_NAME=OtterRapids
OTTERRAPIDS_DIR  = games/otterrapids
# the house style kit (games/common/src, docs/art-direction.md) is compiled in
HOUSE_UI_otterrapids = 1
# widescreen (docs/spec.md "Widescreen"; rs_game.max_width; DESIGN.md): the checks run at 320 and at each width of
# WIDTHS, then across widths (tests/wide_test.sh)
WIDESCREEN_otterrapids = 1

.PHONY: otterrapids otterrapids-check otterrapids-windows otterrapids-armhf otterrapids-dist otterrapids-screenshots otterrapids-art

otterrapids:
	+$(OTTERRAPIDS_MAKE) host
otterrapids-windows:
	+$(OTTERRAPIDS_MAKE) windows
otterrapids-armhf:
	+$(OTTERRAPIDS_MAKE) armhf
otterrapids-dist:
	+$(OTTERRAPIDS_MAKE) dist

OTTERRAPIDS_OBJ_HOST = $(patsubst %.c,build/host/%.o,$(wildcard $(OTTERRAPIDS_DIR)/src/*.c) $(HOUSE_SRC))

otterrapids-check: build/host/otterrapids_physics
	./build/host/otterrapids_physics
	+$(OTTERRAPIDS_MAKE) host build/host/test_libretro build/host/otterrapids_test_states
	./build/host/test_libretro build/host/otterrapids_libretro.so 600
	sh $(OTTERRAPIDS_DIR)/tests/smoke_test.sh build/host/otterrapids_headless build
	sh $(OTTERRAPIDS_DIR)/tests/ui_test.sh build/host/otterrapids_headless build/otterrapids-ui
	sh $(OTTERRAPIDS_DIR)/tests/state_test.sh build/host/otterrapids_test_states build/states
	$(PYTHON) tools/state_audit.py --game otterrapids build/host/otterrapids_test_states $(OTTERRAPIDS_OBJ_HOST)
	set -e; for w in $(WIDTHS); do echo "== widescreen: $$w x 240"; \
	    a=$$(case $$w in 427) echo 16:9;; 520) echo Wide;; esac); \
	    [ -z "$$a" ] || ./build/host/test_libretro build/host/otterrapids_libretro.so 600 $$a $$w; \
	    RS_WIDTH=$$w sh $(OTTERRAPIDS_DIR)/tests/smoke_test.sh build/host/otterrapids_headless build/otterrapids-w$$w; \
	    RS_WIDTH=$$w sh $(OTTERRAPIDS_DIR)/tests/ui_test.sh build/host/otterrapids_headless build/otterrapids-ui-$$w; \
	    RS_WIDTH=$$w sh $(OTTERRAPIDS_DIR)/tests/state_test.sh build/host/otterrapids_test_states build/otterrapids-states-$$w; \
	done
	sh $(OTTERRAPIDS_DIR)/tests/wide_test.sh build/host/otterrapids_headless build/host/otterrapids_test_states build "$(WIDTHS)"
	bash tools/i18n_test.sh build/host/otterrapids_headless otterrapids build/otterrapids-i18n $(if $(WIDESCREEN_otterrapids),320 $(WIDTHS),320)

otterrapids-screenshots:
	+$(OTTERRAPIDS_MAKE) build/host/otterrapids_headless
	sh $(OTTERRAPIDS_DIR)/tests/ui_test.sh build/host/otterrapids_headless $(OTTERRAPIDS_DIR)/docs/screenshots 2
	set -e; for w in 427 520; do \
	    RS_WIDTH=$$w sh $(OTTERRAPIDS_DIR)/tests/ui_test.sh build/host/otterrapids_headless build/otterrapids-shots-$$w 2; \
	    for s in title play; do cp build/otterrapids-shots-$$w/$$s.png $(OTTERRAPIDS_DIR)/docs/screenshots/$$s-$$w.png; done; \
	    cp build/otterrapids-shots-$$w/gameover-medal.png $(OTTERRAPIDS_DIR)/docs/screenshots/gameover-$$w.png; \
	done

otterrapids-art:
	$(PYTHON) $(OTTERRAPIDS_DIR)/tools/make_art.py

build/host/otterrapids_physics: games/otterrapids/tests/test_physics.c games/otterrapids/src/play.c games/otterrapids/src/game.h
	@mkdir -p build/host
	$(HOST_CC) $(HOST_CFLAGS) -Igames/otterrapids/src -o $@ $(filter %.c,$^) -lm
