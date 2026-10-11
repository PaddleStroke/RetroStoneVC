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
| `src/art.c` `art.h` | the pictures, drawn by code at the screen's resolution: shapes (`art_fig`), the house pieces (`art_piece` for chess, `art_counter` for discs, go stones, draughts men and kings), the wooden boards (`art_board_n`), icons. |
| `src/cards.c` `cards.h` | the card and tile kit: five decks drawn by code, a texture cache, cards drawn and moved, hands, piles (see "Card and tile games"). |
| `src/sound.c` | the sounds, made by code: `ui_sound(SND_MOVE / CAPTURE / PLACE / DROP / CHECK / END / TAP / DENY)`. |
| `src/text.c` `text.h`, `help/<lang>/<id>.txt`, `tools/gen_text.py` | the texts: each game's rules (the ? button) and name (the menu card) in 22 languages, the device's language; Noto Sans CJK cut down at build time to the characters used, drawn with stb_truetype. `help/<lang>/_app.txt`: the buttons, after every game's rules. |
| `src/games/<id>.c` | a game: its board on the screen, its input; registers itself (`REGISTER_GAME`). |
| `src/games/<id>_rules.c` `<id>.h` | its rules, without SDL (tested by `tests/<id>_test.c`). Chess: `chess.c` / `chess_ui.c`, `tests/perft.c`. |

## A game (the house rules)

One look across the app, like the Arcade games' house kit:

- **Players** 0 and 1. Player 0 moves first and sits at the bottom (unless flipped). Their colours are the house's
  ivory and ebony (`art_player[]`), whichever plays first by the game's tradition (reversi: ebony first).
- **More players, or one**: `players` (bit n: n can play; ludo `1 << 2 | 1 << 3 | 1 << 4`) makes the picker ask how
  many; the game reads `ui.players` in `start()`. Players 2..5 are red lacquer, jade, amber, lapis (`art_player[]`,
  pieces, counters and dice all follow). `home(player)` says where each one sits (a point of the board, 0..1): their
  bar goes on that side, top or bottom; `gs_win(player)` / `gs_winner(status)` for the status. One player (a puzzle,
  a code to find): `GS_WIN0` solved, `GS_WIN1` failed; the shell hides flip and face to face. `players_usual`: the
  count the picker offers first (tarot 4; else two if it can, else the fewest).
- **Sharing the win**: `winners()` (optional) gives the players who share it once the game is over (bit n: player n):
  partners (pachisi, belote: both of the team), a tie for the best score (`GS_DRAW`: the tied players). The end card
  stands them all in gold light together (a draw: standing, no gold) and lays the others down, dimmed; their bars all
  glow. The status stays `gs_win(any winner)` or `GS_DRAW`. Without it: `gs_winner(status)` alone (a draw: everyone
  stands).
- **The menu's sections**: strategy, chess family, dice & race, cards & tiles, guessing games (headings in
  `help/<lang>/_menu.txt`, a line each). The shell's `SECTION_OF` table files a game by its id (unlisted: strategy);
  within a section the games go by `order`.
- **Wood**: every board is the house wood (`art_board_n`: checkered squares, or one plain plank with lines drawn on
  it), in its walnut frame, on the shell's green table.
- **Pieces** from `art.c` (`art_piece`, `art_counter`), with `ui_shadow()` under the standing ones and
  `ui_shadow_flat()` under the flat ones (discs, stones, men); `ui_flat_at()` draws a flat piece with its shadow,
  faded or turned round.
- **Marks** from the shell: `ui_mark_last` (the last move), `ui_mark_select` (and `_round` for round holes and
  points), `ui_mark_dot` (where a piece can go: a dark dot in a thin cream rim, readable on light wood, dark wood and
  a dark point), `ui_mark_ring` (a capture), `ui_mark_glow` (danger), `ui_mark_gold` (a success: a mill, a winning
  line).
- **The bars**: `ui_number()` for a count or a score, `ui_bar_count(box, piece, n)` for a small piece and how many
  (the pieces taken).
- **Dice** from the shell: `ui_dice_throw` / `ui_dice_show` (a player's dice; `_each`: a colour each, backgammon's
  opening), `ui_dice_draw`, `ui_die_at` (one die at rest: waiting to be thrown, a past throw), `ui_random`.
- **Nothing copied**: anything two games would draw the same way belongs in `art.c` or the shell, not in each game.
- **The end waits**: `busy()` says the last move still animates; the shell shows the end of the game after it.
- **No text on the board** (pictures, and `ui_number()` for scores and counts). The words are in the rules page only:
  `help/en/<id>.txt` (the first line the game's name, then the rules: `# ` headings, `- ` list items, paragraphs),
  translated in the 21 other languages (`help/<lang>/<id>.txt`; a missing one shows the English).
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

## Card and tile games

Cards with secrets are played by passing the device round. `view` (`VIEW_OPEN`, `VIEW_HIDDEN`) makes the screen one
player's view at a time, `ui.viewer`'s (`ui_bottom()` too): their hand at the bottom of the table, their bar under it,
the others' bars at the top in turn order (count, score: the game's `bar()`; the player to move in gold light), no flip
nor face to face (they mean nothing here). `layout(box)` gets the table: a rectangle, all the room between the bars.

- **The hand-off**: `ui_handoff(next)` when the device passes. The shell waits for `busy()` (the card played lands),
  then turns the view. `VIEW_HIDDEN` first covers the whole screen: the green table, the next player's token big in
  gold light, a "pass the device" picture, a round start button; home and the rules in the corners; nothing of the
  game (`draw()` is not called, `ui.viewer` is -1). Tapping start shows that player's view. A new game covers for
  `turn()`; a game opened again from the menu covers first. `ui_handing()`: a hand-off waits (the shell sends no
  input to the game then). `VIEW_OPEN` (golf): nothing is secret, the view just turns, no cover.
- **Undo never shows another hand**: a `VIEW_HIDDEN` game's `undo()` stops at its last `ui_handoff()` (the player's
  own turn only: keep the history's length then); the shell takes no undo while a hand-off waits. A `VIEW_OPEN` game
  undoes freely and calls `ui_handoff(turn)` after an undo.
- **The kit** (`cards.h`). Cards are a byte: `CARD(suit, rank)` (`CARD_SUIT`, `CARD_RANK`). `DECK_FRENCH` (ace 1 ..
  king 13; jokers `CARD(SUIT_JOKER, 0 red / 1 black)`), `DECK_TAROT` (14 a suit: jack 11, knight 12, queen 13, king
  14; trumps `CARD(SUIT_TRUMP, 1..21)`, `TAROT_EXCUSE`), `DECK_NUMBER` (`NUMBER_CARD(-2..12)`), `DECK_TILES` (colours
  0..3, 1..13, two jokers), `DECK_ACTION` (colours 0..3: 0..9, `ACTION_SKIP`, `ACTION_REVERSE`, `ACTION_DRAW2`;
  `ACTION_WILD`, `ACTION_WILD4`). `cards_pack(PACK_52 / 54 / 32 / TAROT / NUMBER / TILES / ACTION, out)` and
  `cards_shuffle(cards, n, seed)` have no SDL: the rules deal from a seed (take it from `ui_random()`: tests and
  pictures repeat), undo replays. No letters on the faces: digits, pips, the courts as the house's chess figures in the
  suit's colour (jack a bishop, knight a knight, queen a queen, king a king), aces one big pip, trumps on a gold sun,
  the excuse a star, jokers a jester's cap; under about 52 px a plainer face (the index big). A 6 or 9 alone is
  underlined.
- **Drawing**: `cards_draw(deck, card, cx, cy, w, deg, how)` (`CD_DOWN` its back, `CD_RAISED` chosen, `CD_GLOW` a
  card to tap, `CD_DIM` not playable, `CD_FLAT` no shadow); `cards_draw_flip(..., t)` (0 back .. 1 face: scale-x);
  `cards_pile(deck, cx, cy, w, n, top, how)` (`top` `CARD_NONE`: face down); `cards_place` an empty place;
  `cards_aspect(deck)` the height / width. The textures are cached by deck, card and width.
- **Hands**: `cards_fan(cx, cy, width, w, aspect, n, arc, spots)` (overlapping when they must, fanned by `arc`
  degrees), `cards_hit(spots, n, w, aspect, x, y)` the top card under the finger; `cards_in` for a card lying straight.
- **Moving cards**: `cards_slide_start(&s, deck, card, A, up0, B, up1, ms, delay)` and `cards_slide_draw(&s)` each
  frame (1 while it moves: hide the card at its destination until then); a flip in place is a slide from A to A.
- **Tokens**: `cards_token(player, size)` a chip in the player's colour. Scores below zero: `ui_number_signed`.
- **Golf** (`games/golf.c`) is the example: the view following the player to move, slides and flips, a round's
  summary, scores in the bars.

## Picture mode

```
retrostoneboard --shot out.bmp [--size 720x1280] [--game id] [--variant n] [--moves "..."] [--taps "fx,fy ..."]
                [--flip] [--f2f] [--keep] [--pick] [--players n] [--lang fr] [--help [--scroll px]] [--seed n] [--at ms]
                [--reveal]
```

No window: the menu (no `--game`), a game's picker (`--pick`), or a game after its moves (`--moves`, the game's
notation) and taps (fractions of the board's square: `0.5,0.5` its middle); `--help` its rules page (in `--lang`,
English by default). The app itself takes `--lang` too (else the device's language).

Card games: `--reveal` lifts the hand-off cover (as if start were tapped: before the taps, between them and at the
end), so a picture shows either the cover or the hand; between taps the clock moves on (each tap's cards have landed
and the device has passed); `--at ms` catches the last tap's slide or flip on its way.
