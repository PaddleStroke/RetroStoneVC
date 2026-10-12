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
| `src/art.c` `art.h` | the pictures, drawn by code at the screen's resolution: shapes (`art_fig`; strokes `art_band`, arrow heads `art_head`, one-colour styles `art_flat`, a figure into a picture `art_fig_blend` / `art_fig_paint`), the house pieces (`art_piece` for chess, `art_counter` for discs, go stones, draughts men and kings, `art_die`), the wooden boards (`art_board_n`), carved holes (`art_recess`), the dice games' felt (`art_felt`), a flat piece and its shadow in a thumbnail (`art_flat_put`), icons. |
| `src/cards.c` `cards.h` | the card and tile kit: five decks drawn by code, a texture cache, cards drawn and moved, hands, piles (see "Card and tile games"). |
| `src/sound.c` | the sounds, made by code: `ui_sound(SND_MOVE / CAPTURE / PLACE / DROP / CHECK / END / TAP / DENY)`. |
| `src/text.c` `text.h`, `help/<lang>/<id>.txt`, `tools/gen_text.py` | the texts: each game's rules (the ? button) and name (the menu card) in 22 languages, the device's language; Noto Sans CJK (and DejaVu Sans for the letters it lacks) cut down at build time to the characters used, drawn with stb_truetype. `help/<lang>/_app.txt`: the buttons, after every game's rules. |
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
- **The menu's sections**, one tab each (a picture: a man, a knight, a die, a card, a question, a marble, a jigsaw
  piece; tap, swipe sideways or the arrow keys): strategy, chess family, dice & race, cards & tiles, guessing games,
  solitaires, puzzles (their names in `help/<lang>/_menu.txt`, a line each, over the games). The shell's `SECTION_OF` table files
  a game by its id (unlisted: strategy); within a section the games go by `order`. `--tab n` in the picture mode.
- **Wood**: every board is the house wood (`art_board_n`: checkered squares, or one plain plank with lines drawn on
  it), in its walnut frame, on the shell's green table. The shared headers draw the boards of a kind: `grid.h` (go's
  points; `grid_lines` and `grid_ink` for any plank ruled in squares: makruk, shogi), `lines.h` (lines and points,
  square or wider than tall, `lines_arc` for round lines: morris, alquerque, fanorona, surakarta), `palace.h`
  (xiangqi, janggi), `race.h` (inlays, hops), `dicecup.h` (the felt's dice games).
- **Pieces** from `art.c` (`art_piece`, `art_counter`), with `ui_shadow()` under the standing ones and
  `ui_shadow_flat()` under the flat ones (discs, stones, men); `ui_flat_at()` draws a flat piece with its shadow,
  faded or turned round.
- **Marks** from the shell: `ui_mark_last` (the last move), `ui_mark_select` (and `_round` for round holes and
  points), `ui_mark_dot` (where a piece can go: a dark dot in a thin cream rim, readable on light wood, dark wood and
  a dark point), `ui_mark_ring` (a capture), `ui_mark_glow` (danger), `ui_mark_gold` (a success: a mill, a winning
  line; `ui_mark_gold_a` fainter, a light coming or pulsing).
- **Numbers**: `ui_number()` (cream or dark), `ui_number_rot`, `ui_number_signed`; `ui_number_ex` in any colour and
  alpha, regular or bold, set tight (a number in a cell); `ui_number_w` its width.
- **The bars**: `ui_bar_count(box, piece, n)` for a small piece and how many (the pieces taken; 0 shows nothing),
  `ui_bar_number` the same showing 0 too, the piece optional (a move counter; it returns the x after the number).
  `ui_bar_box(player, &upside)`: where a player's bar is (something flying to it).
- **Buttons** of a game's own (on its table, in its bar): `ui_button(cx, cy, d, ICON_*, how)` the shell's disc with
  one of its icons (`ICON_TICK` done, `ICON_ARROW` next, `ICON_HINT` a lamp, `ICON_DEVICE`...), or
  `ui_button_paint(cx, cy, d, paint, kind, k, how)` with the game's picture (a painter: `paint(kind, size, rgba)`, k
  of the disc across; the shell makes it at the size shown and keeps it). `how`: `UI_LIT` (a gold light: press me),
  `UI_DIM` (not now), `UI_ON` (the gold disc: a mode on, the action ready), `UI_IVORY` (an ivory medallion, a picture
  in its own colours), `UI_TINT` (a white picture tinted cream), `UI_UPSIDE` (the top player's). `ui_button_hit` for
  the finger; `ui_button_pic` paints one into a thumbnail. No walnut knobs, no copies of the disc.
- **Dice** from the shell: `ui_dice_throw` / `ui_dice_show` (a player's dice, up to `UI_MAX_DICE` in a row; `_each`:
  a colour each, backgammon's opening), `ui_dice_draw`, `ui_die_at` (one die at rest: waiting to be thrown, a past
  throw), `ui_random`. Dice laid out by the game itself (kept, set aside, under a cup), one at a time: `ui_die` (turned,
  lifted, faded, darkened) and `ui_die_thrown` (tumbling `UI_DICE_MS`, then at rest); `ui_die_tumble` the same motion
  for a lot of the game's own (Ur's pyramids, pachisi's cowries); `art_die_blend` a die in a picture (a thumbnail).
- **Nothing copied**: anything two games would draw the same way belongs in `art.c` or the shell, not in each game.
- **The end waits**: `busy()` says the last move still animates; the shell shows the end of the game after it.
- **No text on the board** (pictures, and `ui_number()` for scores and counts). The words are in the rules page only:
  `help/en/<id>.txt` (the first line the game's name, then the rules: `# ` headings, `- ` list items, paragraphs),
  translated in the 21 other languages (`help/<lang>/<id>.txt`; a missing one shows the English). A game that truly
  needs a few words in play (tarot's bids) keeps them in `help/<lang>/<id>_words.txt` (a title line for the
  translators, then a word a line, in all 22 languages): `text_word(key, i, ...)` reads line i, `ui_word(key, i, px,
  maxw, rgb, &w, &h)` gives it as a texture fitted to a width (kept by the shell). Tarot's `tarot_bids.txt` is the
  first such file.
- **Characters on pieces and tiles** (xiangqi, shogi, mahjong, word tiles): add them to `help/glyphs.txt`; draw them
  with `text_glyph`. Noto Sans CJK draws them, DejaVu Sans the ones it lacks (Ą Ł Ş Є...), in every language.
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
  then turns the view. `VIEW_HIDDEN` first covers the screen: the table greyed under it, the next player's token big
  in gold light, a "pass the device" picture, a round start button; home and the rules in the corners. Under the
  cover `draw()` runs with `ui.viewer` -1: it must show every hand hidden (backs, or nothing), only what all players
  see (the table, the piles, the scores). Tapping start shows that player's view. A new game covers for
  `turn()`; a game opened again from the menu covers first. `ui_handing()`: a hand-off waits (the shell sends no
  input to the game then). `VIEW_OPEN` (golf): nothing is secret, the view just turns, no cover.
- **Undo never shows another hand**: a `VIEW_HIDDEN` game's `undo()` stops at its last `ui_handoff()` (the player's
  own turn only: keep the history's length then); the shell takes no undo while a hand-off waits. A `VIEW_OPEN` game
  undoes freely and calls `ui_handoff(turn)` after an undo.
- **The kit** (`cards.h`). Cards are a byte: `CARD(suit, rank)` (`CARD_SUIT`, `CARD_RANK`). `DECK_FRENCH` (ace 1 ..
  king 13; jokers `CARD(SUIT_JOKER, 0 red / 1 black)`), `DECK_TAROT` (14 a suit: jack 11, knight 12, queen 13, king
  14; trumps `CARD(SUIT_TRUMP, 1..21)`, `TAROT_EXCUSE`), `DECK_NUMBER` (`NUMBER_CARD(-2..12)`), `DECK_TILES` (colours
  0..3, 1..13, two jokers), `DECK_ACTION` (colours 0..3: 0..9, `ACTION_SKIP`, `ACTION_REVERSE`, `ACTION_DRAW2`;
  `ACTION_WILD`, `ACTION_WILD4`), `DECK_ITALIAN` (`IT_COINS`, `IT_CUPS`, `IT_SWORDS`, `IT_CLUBS`; 1 .. 7, `IT_FANTE`
  8, `IT_CAVALLO` 9, `IT_RE` 10; `cards_italian.c`; its notation: `games/italian.h`).
  `cards_pack(PACK_52 / 54 / 32 / TAROT / NUMBER / TILES / ACTION / 40, out)` and `cards_shuffle(cards, n, seed)` have
  no SDL: the rules deal from a seed (take it from `ui_random()`: tests and pictures repeat), undo replays. No letters
  on the faces: digits, pips, the courts as the house's chess figures in the suit's colour (jack a bishop, knight a
  knight, queen a queen, king a king), aces one big pip, trumps on a gold sun, the excuse a star, jokers a jester's
  cap; under about 52 px a plainer face (the index big). A 6 or 9 alone is underlined.
- **A game's own deck**: `cards_deck_add(&def)` (`{aspect, face(card, w, h, rgba), back(w, h, rgba) or NULL: the
  house's}`, kept static) gives a deck number for every call below: the kit draws, flips, slides and piles its cards
  with its own shadow, gold light and lift (the families' animals: `games/families_deck.h` `fam_deck()`; the hanafuda:
  `hana_deck()`; Crapette's second pack with a red back).
- **Drawing**: `cards_draw(deck, card, cx, cy, w, deg, how)` (`CD_DOWN` its back, `CD_RAISED` chosen, `CD_GLOW` a
  card to tap, `CD_DIM` not playable, `CD_FLAT` no shadow), `cards_draw_alpha(..., alpha)` faded;
  `cards_draw_flip(..., t)` (0 back .. 1 face: scale-x); `cards_pile(deck, cx, cy, w, n, top, how)` (`top`
  `CARD_NONE`: face down; n 0: the empty place, in gold light with `CD_GLOW`); `cards_place` an empty place;
  `cards_aspect(deck)` the height / width. The textures are cached by deck, card and width. Pictures:
  `cards_paint_at` / `cards_paint_turned` (a card in a thumbnail), `cards_suit_icon(deck, suit, size)` (a French
  suit's pip on an ivory disc; an Italian suit's symbol), `cards_token(player, size)` a chip in the player's colour.
- **Hands**: `cards_fan(cx, cy, width, w, aspect, n, arc, spots)` (overlapping when they must, fanned by `arc`
  degrees), `cards_hit(spots, n, w, aspect, x, y)` the top card under the finger; `cards_in` for a card lying straight.
- **Moving a card**: `cards_slide_start(&s, deck, card, A, up0, B, up1, ms, delay)` and `cards_slide_draw(&s)` each
  frame (1 while it moves: hide the card at its destination until then); a flip in place is a slide from A to A.
- **The card table** (`cards_table.h`, prefix `ct_`): what card games draw round their cards, written once.
  - Moving cards: one list, `ct_moves M = {.rest = 300}` (busy that long after each card lands), `ct_slide(&M, deck,
    card, A, up0, B, up1, ms, delay, tag, a, b, sound)` (its index), `ct_hides(&M, tag, a, b)` the cards still on
    their way there (-1: any; leave them out of their place until they land), `ct_show` (unseen at first: a card that
    waits), `ct_after` (drawn once another one has landed), `ct_rest`, `ct_hop`, `ct_until`, `ct_busy` (the game's
    `busy()`), `ct_moving`, `ct_quiet`, `ct_draw(&M)` each frame, `ct_calm` (they land at once).
  - The trick table (`ct_table`, `ct_layout(&T, box, players, viewer, max_hand)`; `T.deck` its cards): the viewer at
    the bottom, the others round the middle in turn order (`ct_draw_seats`, `ct_seat_spot` for what goes beside a
    chip), the trick (`ct_draw_trick`, `ct_gather`, `ct_draw_last`; `who`: each card's player for a lone hand, or
    NULL), the trick pile (`ct_draw_pile`, `ct_on_pile`), the viewer's hand (`ct_fan`, `ct_hand_draw`,
    `ct_hand_backs` under the cover, `ct_hand_at`, `ct_deal`, `ct_play`) and the finger (`ct_touch`: a tap raises
    the card, a second tap, its place in the trick or a drag plays it: `ct_touch_down / move / up`, `ct_tap_play`,
    `ct_draw_target`). The rules' side of trick games is `games/tricks.h` (no SDL).
  - The others along the top: `ct_seat`, `ct_seat_x`, `ct_backs` (a small fan of backs), `ct_backs_end`, `ct_label`
    (their chip and count). The viewer's hand fitted in a box (`ct_hand`) or along the table's bottom in one or two
    rows (`ct_rows_layout`, `ct_rows_hit`).
  - Pictures kept at the size they are drawn (take them each frame): `ct_token`, `ct_suit`, `ct_disc`, `ct_round`,
    `ct_icon` (tinted), `ct_picture` (a game's own painter); `ct_pic(CT_ARROW / TICK / SLASH / MOON / DISC_*)`,
    `ct_button`, `ct_on`, `ct_number` (nil: struck out), `ct_badge`, `ct_pass`, `ct_pass_badge`, `ct_choice`.
  - The round's summary: `ct_summary(&T, rows, ...)` or `ct_panel(x, y, w, h, alpha)` over the dimmed table, rows
    split by `ct_rule`; `ct_number_w` a number's width; `ct_bar` (cards in hand and a score); points rising
    (`ct_floats`, `ct_float`, `ct_plus`).
  - Patience: `ct_column_steps` (a column fanned down, closer when long), `ct_run` (a run under the finger),
    `ct_leap` (the cards leaping off at a win); the games' rules share `games/solitaire.h`, their screen
    `games/solitaire_ui.h`.
- Scores below zero: `ui_number_signed`.
- **Golf** (`games/golf.c`) is the example: the view following the player to move, slides and flips (`ct_moves`), a
  round's summary, scores in the bars.

## Picture mode

```
retrostoneboard --shot out.bmp [--size 720x1280] [--game id] [--variant n] [--moves "..."] [--taps "fx,fy ..."]
                [--drag "fx0,fy0 fx1,fy1"] [--hold "fx,fy ms"] [--flip] [--f2f] [--keep] [--pick] [--players n]
                [--lang fr] [--help [--scroll px]] [--tab n] [--seed n] [--at ms] [--reveal]
```

No window: the menu (no `--game`; `--tab n` a section), a game's picker (`--pick`), or a game after its moves
(`--moves`, the game's notation) and gestures; `--help` its rules page (in `--lang`, English by default). The app
itself takes `--lang` too (else the device's language).

- **Gestures**, in the order given, in fractions of the board's box (`0.5,0.5` its middle): `--taps` (several),
  `--drag` (the finger down, slid to the second point, lifted), `--hold` (the finger down `ms` long; as the last
  gesture, the picture is taken with the finger still down).
- **The clock** is the picture mode's own: it stands still, so the same picture comes out every time. Before each
  gesture it moves on until the game's animations have ended (`busy()`, the dice: a deal, a slide), so no tap is
  lost; at the end it moves on until everything is at rest, or to `--at ms` after the last gesture (a slide, a flip,
  a throw on its way).
- **Card games**: `--reveal` lifts the hand-off cover (as if start were tapped: before the taps, between them and at
  the end), so a picture shows either the cover or the hand; between taps the device has passed.
