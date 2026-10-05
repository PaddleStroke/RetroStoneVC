# RetroStone Arcade: the launcher and the game bundle

Several RetroStone VC games and a menu in **one program**: the two Android apps (`apps/android/`: RetroStone Arcade
Free and the paid RetroStone Arcade) and the desktop `dist/windows/RetroStoneArcadeFree.exe` and
`RetroStoneArcade.exe`. The games' code is not changed: they are linked as they are, as **modules**, and an app layer
turns touches into the virtual SNES pad.

| | |
|---|---|
| ![320](docs/launcher-320.png) | ![panel](docs/panel.png) |
| ![520, a 19.5:9 phone](docs/launcher-520.png) | ![played](docs/played.png) |
| ![French, 16:9](docs/panel-fr-427.png) | ![Japanese](docs/panel-ja.png) |
| ![help](docs/help.png) | ![credits](docs/credits.png) |
| ![a licence](docs/licence.png) | ![with a gamepad](docs/launcher-pad.png) |

## Build and test

```
make -f apps/launcher/bundle.mk                 # build/bundle/host/retrostonearcade (SDL2) and ..._headless
make -f apps/launcher/bundle.mk check           # the tests below
make -f apps/launcher/bundle.mk PLATFORM=win64  # dist/windows/RetroStoneArcadeFree.exe
make -f apps/launcher/bundle.mk PLATFORM=win64 check   # the tests on the Windows build (WSL runs the .exe)
make -f apps/launcher/bundle.mk shots           # apps/launcher/docs/*.png
make -f apps/launcher/bundle.mk CONF=apps/android/app-full.conf [check|PLATFORM=win64]   # the same, the full app
apps/android/build.sh free|full                 # the Android APKs (apps/android/README.md)
```

An app's name, package id, games and (free app) the paid app's link are in **`apps/android/app.conf`** (the free
app, the default) or **`apps/android/app-full.conf`** (the full app: `FREE_GAMES=all`, no locked game, no promotion),
one file per app for Android and the desktop. Everything is built in `build/bundle/<platform>/` (the full app:
`build/bundle/<platform>-full/`); of the root Makefile only the games' generated assets are used
(`tools/game_assets.sh`: one wait for the lock `/tmp/rsvc-build.lock` for all the games).

`check` proves: (1) each playable game started in the bundle gives **exactly the pictures of its standalone build**
for the same input, fresh, at width 520 and after another game ran first in the same process (it also reports which
games are widescreen and which are pillarboxed); (2) the flow with touches: a tap on a card starts the game, taps are
its pad, Back saves its state and returns to the menu, the game resumes where it was (the same picture) and waits for
a tap; free app: a locked card opens "Get the full version", its button asks for the Play Store page, Back closes the
panel; full app: no store request; Back in the menu quits; (3) the credits: the "i" opens the list, a line its
licence's text, a drag scrolls it, Back goes back step by step; (4) every game launches in one process and writes and
resumes its own save state; (5) each game's touch scheme gives pad buttons; (6) the launcher in each of the 22
languages (the menu, the footer, the credits, the free app's panel): the house kit reports no layout error.

## The module system (several games in one binary)

Each game is a standalone program: its `rs_game_main()`, and globals that other games define too. `tools/modulize.py`
links a game into one relocatable object:

1. its objects' writable data (`.data`, `.data.rel`, `.data.rel.local`) are renamed into one section and its `.bss`
   into another (ELF `.data.rsgame` / `.bss.rsgame`; Windows COFF `.data$rsg_<id>_1` / `.bss$rsg_<id>_1`);
2. `ld -r` links `src/rsb_glue.c` (begin markers), the objects, and the glue again (end markers, and the
   descriptor `rsb_module_<id>`: its id, its `rs_game_main`, the bounds of its data and bss): `ld -r` keeps the
   input order inside a section, so the markers bound exactly the game's writable data;
3. `objcopy --keep-global-symbol=rsb_module_<id>` makes every other symbol it defines local; its references to the
   SDK, the house kit and libc stay undefined: **the SDK and the house kit are linked once**, shared by all;
4. checks: no writable data outside the two sections, one global symbol.

A game's translated text (`build/gen/<id>/game_text.c`, docs/i18n.md) registers its pack from a constructor: in one
program all of them would run at load and the last would win, so `modulize.py` renames the call
(`rs_i18n_register` -> `rsb_glue_i18n_register`): the runtime keeps each game's pack and installs it when the game
starts. The games speak the player's language (the phone's; `RS_LANG=fr` or `--lang fr` on the desktop), and so does
the launcher (`strings/`).
The house kit (`games/common`) is a module too (`--house`: its functions stay global). At start-up the runtime copies
every module's data; **each time a game starts, its data is restored and its bss cleared** (and the house kit's), so
a game always starts as in a fresh process, whatever ran before; the SDK resets itself in `rs_host_init()`.

The same object files work on Linux (GNU ld), Android (lld, the NDK's llvm-objcopy) and Windows (mingw ld: the
modules are compiled with `-mcmodel=small`, so no `.refptr` stub is shared between modules).

## The runtime (`src/rsb_runtime.c`)

- One program runs at a time: the launcher, or a game. A game gets `<saves>/<id>.srm` (its battery save) and
  `<saves>/<id>.state` (a save state, written when it is left: Back, the app going to the background, quitting).
  Starting a game loads its state when there is one: it resumes where it was, **held** (the frontend shows a play
  sign) until a touch or a button, and that touch is not passed to the game.
- Touches: `games.cfg` gives each game its scheme (zones of the screen -> pad buttons, some drawn by the frontend
  as translucent buttons, and the button of the small pause button in the corner); they are player 1. The launcher
  gets a pointer (`rsb_pointer_get()`) and Back.
- The menu: the "i" (top left) opens Credits & licences, the "?" (top right) a short help (touch is player 1,
  gamepads players 2 to 4, the in-game pause and back buttons); the dialogs close with their back arrow (top left), a
  tap outside them, Back or B. The pad's glyph hints (A, B, the selected game's name in the footer) are shown only
  while a gamepad is connected (`rsb_input.gamepad`; on the desktop, once the keyboard was used); the footer itself
  only when it has something (the free app's "full version" banner, or those hints).
- In a game, the frontend draws a pause button (top right) and a back button (top left, = Back: the state is saved,
  the menu comes back); the runtime keeps their touches from the game.
- Credits & licences: the list of the components and their licences (translated), and each licence's full text
  (English, scrolled). `tools/gen_credits.py` takes the texts
  from the files that come with each component (`LICENSE-*`, SDL2's `LICENSE.txt`, libxmp-lite's README, Unifont's
  OFL, stb_image_write's header), following THIRD_PARTY.md.
- Widescreen: the launcher is laid out on the width the display's aspect asks for (3 columns at 320, 4 at 16:9, 5
  from 520); a game gets that width when it opts in (`rs_game.max_width`, docs/spec.md "Widescreen"), else 320 and
  the frontend draws side borders.
- Buttons held when a program starts are ignored until released (a tap that launched a game does not also start it).

## Files

| | |
|---|---|
| `src/rsb.h` | the runtime's API (launcher, frontends), the module and catalogue types |
| `src/rsb_runtime.c` | switching, saves, touch mapping |
| `src/launcher.c` | the menu, a RetroStone VC program in the house style |
| `src/rsb_sdl.c` | the SDL2 frontend: Android and desktop (scaling, borders, the pause button and the play sign, lifecycle, the store link) |
| `src/rsb_headless.c` | the test frontend (scripted pad, touches, Back, switches) |
| `src/rsb_glue.c` | the module markers and descriptor |
| `games.cfg` | the touch schemes and best-score locations, per game |
| `strings/` | the launcher's words in the 22 languages (docs/i18n.md format; `tools/launcher_text.py` compiles them with `tools/i18n_build.py`) |
| `tools/gen_catalog.py` | `build/bundle/gen/catalog.c`: the cards (title screenshot + name), the locked games' pictures, the touch schemes, the app's names. **Every game of `games/` with a `docs/screenshots/title.png` appears, locked unless it is in `FREE_GAMES`** (`all`: none locked); `--list`: the playable games |
| `tools/gen_credits.py` | `build/bundle/gen/credits.c`: the credits screen's components and their licence texts |
| `tools/game_assets.sh` | the games' generated assets from the root Makefile, under one wait for its lock |
| `tools/modulize.py` | the module linker |
| `bundle.mk` | the build |
| `tests/` | `bundle_test.sh`, `shots.sh` |

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
