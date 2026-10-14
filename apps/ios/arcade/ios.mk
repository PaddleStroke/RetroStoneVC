# RetroStone Arcade for iPhone and iPad: the part of apps/launcher/bundle.mk for PLATFORM=ios-<sdk>-<arch> (included
# at its end; macOS with Xcode). apps/ios/build.sh arcade runs it:
#   make -f apps/launcher/bundle.mk PLATFORM=ios-iphonesimulator-arm64 CONF=build/ios/arcade/app.conf GAMES="..." \
#        -o build/bundle/gen-ios/catalog.c ios-lib
# with the generated sources made on Linux beforehand (apps/ios/prepare.sh arcade: build/bundle/gen-ios).
#
# $(BD)/libarcade.a: the launcher, the runtime, the SDK, the house kit and every game as Mach-O modules
# (apps/launcher/tools/modulize.py --flavor macho), and the SDL frontend (rsb_sdl.c, its main() is SDL_main); the
# Xcode project (apps/ios/arcade/project.yml) links it with SDL2, the StoreKit backend and the entry point.
# Apple's tools rename no section, so each module's writable data goes into sections of its own AS IT IS COMPILED: a
# clang pragma (#pragma clang section, -include $(BD)/sect/<id>.h: __DATA,__rsgd<n> and __DATA,__rsgb<n>, n = 00
# for the house kit, then 01, 02... in GAMES order), and the game's translated text registers through the glue
# (-Drs_i18n_register=rsb_glue_i18n_register, what modulize.py's objcopy does elsewhere).
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).

SECT_DIR  := $(BD)/sect
MACHO_MODS := house $(GAMES)
# a module's section number, two digits
sect_of = $(shell n=0; for m in $(MACHO_MODS); do [ "$$m" = "$(1)" ] && break; n=$$((n + 1)); done; printf '%02d' $$n)
# the pragma headers, written now (only when they change: the objects then rebuild)
$(if $(GAMES),,$(error ios: GAMES is empty (apps/ios/build.sh passes the list of build/ios/arcade/games.txt)))
_ := $(shell sh apps/ios/arcade/sections.sh $(SECT_DIR) $(MACHO_MODS))

$(BD)/obj/games/common/src/%.o: CFLAGS_ALL += -include $(SECT_DIR)/house.h
$(foreach g,$(GAMES),$(eval $(BD)/games/$(g)/%.o: CFLAGS_ALL += -include $(SECT_DIR)/$(g).h \
                                                               -Drs_i18n_register=rsb_glue_i18n_register))
$(BD)/mod/house.o: MODULIZE += --sect 00 --ldflags="-arch $(IOS_ARCH)"
$(foreach g,$(GAMES),$(eval $(BD)/mod/$(g).o: MODULIZE += --sect $(call sect_of,$(g)) --ldflags="-arch $(IOS_ARCH)"))

.PHONY: ios-lib
ios-lib: $(BD)/libarcade.a

$(BD)/libarcade.a: $(call o,apps/launcher/src/rsb_sdl.c) $(CORE)
	@rm -f $@
	xcrun libtool -static -o $@ $^
