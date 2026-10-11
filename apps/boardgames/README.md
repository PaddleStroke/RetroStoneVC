# RetroStone Board

Board games for two players on one device, in **one app**: chess, draughts, reversi, four in a row, nine men's
morris, go. Plain C + SDL2 (not the RetroStone VC SDK: board games want the screen's full resolution). Windows
`.exe` now, Android later (the same SDL2 code, the Arcade app's Android build).

```
make -f apps/boardgames/Makefile                 # build/boardgames/host/retrostoneboard (Linux)
make -f apps/boardgames/Makefile test            # every game's rules test
make -f apps/boardgames/Makefile shots           # build/boardgames/shots/*.png
make -f apps/boardgames/Makefile win64           # dist/windows/RetroStoneBoard.exe
```

`GAMES="chess go"` builds only these games; `B=build/boardgames-go` another build directory (parallel work).

## The pieces

| | |
|---|---|
| `src/shell.c` `shell.h` | the app: the menu, the variant picker, the players' bars, the buttons (home, undo, flip, face to face, new game), the end-of-game card, the picture mode. Everything the games share, written once. |
| `src/art.c` `art.h` | the pictures, drawn by code at the screen's resolution: shapes (`art_fig`), the house pieces (`art_piece` for chess, `art_counter` for discs, go stones, draughts men and kings), the wooden boards (`art_board_n`), icons, digits. |
| `src/sound.c` | the sounds, made by code: `ui_sound(SND_MOVE / CAPTURE / PLACE / DROP / CHECK / END / TAP / DENY)`. |
| `src/games/<id>.c` | a game: its board on the screen, its input; registers itself (`REGISTER_GAME`). |
| `src/games/<id>_rules.c` `<id>.h` | its rules, without SDL (tested by `tests/<id>_test.c`). Chess: `chess.c` / `chess_ui.c`, `tests/perft.c`. |

## A game (the house rules)

One look across the app, like the Arcade games' house kit:

- **Players** 0 and 1. Player 0 moves first and sits at the bottom (unless flipped). Their colours are the house's
  ivory and ebony (`art_player[]`), whichever plays first by the game's tradition (reversi: ebony first).
- **Wood**: every board is the house wood (`art_board_n`: checkered squares, or one plain plank with lines drawn on
  it), in its walnut frame, on the shell's green table.
- **Pieces** from `art.c` (`art_piece`, `art_counter`), with `ui_shadow()` under the standing ones.
- **Marks** from the shell: `ui_mark_last` (the last move), `ui_mark_select`, `ui_mark_dot` (where a piece can go),
  `ui_mark_ring` (a capture), `ui_mark_glow` (danger).
- **No text** (nothing to translate): pictures, and `ui_number()` for scores and counts.
- **Tap, or drag**: every move can be made with taps alone; a tap on something not allowed: `SND_DENY`.
- **Face to face** (`ui.f2f`): pieces that have a top and a bottom are drawn turned round for the top player
  (`ui_upside(player)`); the shell turns that player's bar itself.
- **Sounds**: a move `SND_MOVE`, a capture `SND_CAPTURE`, a stone or disc set down `SND_PLACE`, a disc dropped in a
  slot `SND_DROP`. The shell plays the end of the game and the buttons.
- **The board's square**: `layout(box)` gives it (side about 9 * `ui.unit`); the game fits its frame and grid in it.
- **The game's pictures for the shell**: `thumb(variant, size)` (the menu's card, -1, and the picker's cards),
  `token(player, size)` (the bars, the end-of-game card: a piece of that player).
- **Undo** goes back one move (all the way to the start), **status** says who won, **turn** who moves,
  **can_pass / pass** if the game has a pass (go), **bar** what a player's bar shows (pieces taken, a score),
  **script** plays moves in the game's own notation (the screenshots, the tests).

## Picture mode

```
retrostoneboard --shot out.bmp [--size 720x1280] [--game id] [--variant n] [--moves "..."] [--taps "fx,fy ..."]
                [--flip] [--f2f] [--keep] [--pick]
```

No window: the menu (no `--game`), a game's picker (`--pick`), or a game after its moves (`--moves`, the game's
notation) and taps (fractions of the board's square: `0.5,0.5` its middle).
