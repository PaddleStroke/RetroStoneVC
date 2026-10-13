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

`GAMES="chess go"` builds only these games; `B=build/boardgames-go` another build directory (parallel work). A game
built on another one's rules names them in the Makefile (`NEEDS_chesspuzzles := .../chess.c`), so `GAMES="id"` alone
links. `DEMO=1` adds `tests/levels_demo.c` (five tiny games: the level screen, the keys, the score,
the solved puzzle's card, levels with random play; never shipped).

## The pieces

| | |
|---|---|
| `src/shell.c` `shell.h` | the app: the menu, the variant picker, the players' bars, the buttons (home, undo, flip, face to face, new game), the end-of-game card, the level screen, the picture mode. Everything the games share, written once. |
| `src/save.c` | what the app keeps between two runs (`ui_save_get` / `ui_save_set`): one small file on the device (see "What the app keeps"). |
| `src/art.c` `art.h` | the pictures, drawn by code at the screen's resolution: shapes (`art_fig`; strokes `art_band`, arrow heads `art_head`, one-colour styles `art_flat`, a figure into a picture `art_fig_blend` / `art_fig_paint`), the house pieces (`art_piece` for chess, `art_counter` for discs, go stones, draughts men and kings, `art_die`), the wooden boards (`art_board_n`), carved holes (`art_recess`), the dice games' felt (`art_felt`), a flat piece and its shadow in a thumbnail (`art_flat_put`), icons. |
| `src/cards.c` `cards.h` | the card and tile kit: the house's decks drawn by code (and a game's own, `cards_deck_add`), a texture cache, cards drawn and moved, hands, piles (see "Card and tile games"). |
| `src/cards_italian.c` | the Italian pack of 40 (`DECK_ITALIAN`), drawn by code. |
| `src/cards_spanish.c` | the Spanish pack of 40 or 48 (`DECK_SPANISH`), drawn by code. |
| `src/cards_families.c` | the family deck's animals (`games/families_deck.h`, `fam_deck()`: a deck of the kit's own), drawn by code. |
| `src/cards_table.c` `cards_table.h` | the shared card table (prefix `ct_`): what card games draw round their cards, written once (see "Card and tile games"). |
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
  solitaires, puzzles (their names in `help/<lang>/_menu.txt`, a line each, over the games). The shell's
  `SECTION_OF` table files a game by its id (unlisted: strategy); within a section the games go by `order`. `--tab n`
  in the picture mode.
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
  `ui_bar_box(player, &upside)`: where a player's bar is (something flying to it). `ui_bar_fit(box, counts, n,
  buttons)`: counts (`ui_count`: a small picture or NULL, a number) and room for round buttons at the right end, fitted
  to a narrow bar (the first picture goes, then all is set smaller); it returns the buttons' size, `ui_bar_spot` each
  one's centre. `ui_bar_countdown(box, left)`: the turns left before a draw (an hourglass, the number) at the bar's
  right end; it returns the room left of it. `ui_tokens_changed()`: `token()` now paints other pictures (banqi's
  colours, given at the first flip).
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
  Portrait: the top bar at the top of the screen, the bottom bar and the buttons at its bottom, the square in the room
  between. **The whole room** (`fill` 1): `layout(box)` gets all of it, a rectangle (portrait: the bars' width, all the
  height between them; landscape: all the width left of the side column), as the card games' tables;
  `ui_grid_fit(box, cols, rows, frame)` fits a grid of square cells in it, turned a quarter when the cells grow
  (`ui_grid_cell`, `ui_grid_hit`); `art_plank(w, h, fr)` a plain plank of any shape. **Tools** (`tools` n, up to
  `UI_MAX_TOOLS`): the game's own round buttons, laid out by the shell where they cost the board least (portrait: up
  to three at the right end of the bottom bar, more in a row above it; landscape: in the side column); the game draws
  them (`ui_tool(i, ICON_*, how)`, `ui_tool_paint`) and finds them in `press()` (`ui_tool_hit`); `ui_tools_shown(n)`
  the first n only. **Trays** (`ui_trays_place`, `ui_trays_slot`, `ui_trays_hit`): a two-player board placed in the
  room with a tray by each player's side (above and below it, or beside it) for the pieces they hold or took (dara,
  jungle, xiangqi). **The loupe** (`ui_loupe_small`, `ui_loupe_at`, `ui_loupe_begin` / `ui_loupe_end`): cells small for
  a finger shown magnified beside it while it is down (go 19 x 19, hex, word tiles).
- **The game's pictures for the shell**: `thumb(variant, size)` (the menu's card, -1, and the picker's cards),
  `token(player, size)` (the bars, the end-of-game card: a piece of that player).
- **Undo** goes back one move (all the way to the start), **status** says who won, **turn** who moves,
  **can_pass / pass** if the game has a pass (go), **bar** what a player's bar shows (pieces taken, a score),
  **script** plays moves in the game's own notation (the screenshots, the tests).
- **Keys** (optional `key(k)`): a key pressed in the game (once, not its repeats) that the shell does not use itself
  (Esc, Back, M, H, F1, U, Backspace, F, T, N are the shell's): the arrows for a push puzzle. Never while the rules
  page or the hand-off cover is up, nor on another screen. No game watches SDL's events itself.
- **A score** (optional `score()`, one player): the end card shows it in big digits beside the token (the final
  score, the moves a puzzle took).
- **A puzzle solved** (one player, `level_stars()`, with levels or without): the end card shows the stars popping in,
  the moves (`score()`) beside `par()` (optional: the fewest moves; 0 not known) after a flag, a replay button (the
  same puzzle: the shell gives `start()` the random state it had) and a next one (a new puzzle of the variant; the next
  level). `ui_par_stars(moves, par)`, `ui_logic_stars(hints, mistakes)`: the house's rules for stars.
- **Variants on two axes** (`axes` {sizes, levels}, variant = size * levels + level; `axis_pic[2]` optional): the
  picker shows the sizes above a row of the levels (by default as many ebony stones) instead of a card each.
- **Secret hands in some games only** (optional `hidden()`): asked at each hand-off instead of `VIEW_HIDDEN` (Durak
  with open hands).

## Levels

A game of designed levels (Logic Gates, Box Pusher) declares them; the shell then shows its **level screen** instead
of the picker, and keeps the progress.

- `levels` (how many), `chapters` (split evenly; or `chapter_of(level)`, never going down, for uneven ones), optional
  `chapter_icon(chapter, size, rgba)` (a tab's picture; else its number; a wide tab shows both), optional
  `level_thumb(level, size, rgba)` (a tile's picture; else its number), `level_stars()` (the level solved: 0..3; NULL:
  no stars), `levels_open` (the unsolved levels open at once; 0: 3).
- The screen: home at the top left, the stars won at the right, the chapters' tabs (the menu's), then the chapter's
  levels as tiles right under the tabs, numbered from the first level to the last (a second chapter of 25: 26..50):
  solved in gold with their stars, open in the house's wood, locked in dim walnut; the next level to play in gold
  light. A tap on an open tile, a swipe or the arrow keys for another chapter. It opens at the chapter of the level
  last played (else of the first one not solved).
- A tile calls `start(level)` (0..`levels`-1): a new game of that level. Solved is `status()` `GS_WIN0`: the shell asks
  `level_stars()` and keeps the best (`ui_level_best(level)` reads it: 0..3, -1 not solved). Its end card shows the
  stars, replay (the same level) and a next button (after the last level: back to the level screen). The new game
  button shows the level grid's icon and goes back to the level screen (a level in play: tapped twice, as usual).
- The game draws no level grid, keeps no progress, and has no next-level button of its own; it shows its level (a
  chapter's picture and number) and its moves as it likes.
- `ui_star(cx, cy, d, lit)`: the shell's star (gold, or a hollow), for a game's bar.
- **Levels and random play** (optional `start_random(variant)`, the game's `variants` kept): one more tab after the
  chapters, a die: the variant picker under the same tabs, and a card picked there starts `start_random(variant)`;
  `ui.level` tells the game which way it was started (the level, or -1 at random). The end card's next button: a new
  random puzzle of the variant; stars are kept per level only.

## What the app keeps

`ui_save_get(key, buf, size)` / `ui_save_set(key, data, size)` (`src/save.c`): small values under keys namespaced by
the game (`"logicgates/x"`; `"shell/..."` are the shell's), up to 4096 bytes each, written as bytes (numbers wider than
a byte little-endian). The shell keeps the sound (`shell/mute`), the menu's tab (`shell/tab`), each game's choice
(`shell/game/<id>`: its variant, the last level played, how many play; the picker then shows that count, and the
variant chosen in gold light; a game of levels with random play: also its random variant and which way it was last
played) and each game's levels (`shell/levels/<id>`: a byte a level, 0 not solved, else 1 + its
stars).

- **Where**: `board.sav` in `SDL_GetPrefPath("8BCraft", "RetroStoneBoard")`: Windows `%APPDATA%\8BCraft\RetroStoneBoard`,
  Linux `~/.local/share/8BCraft/RetroStoneBoard`, Android the app's own files. `--save DIR` another folder.
- **The file**: 20 bytes (`RSBS`, the version 1, three zeros, the records' count, their length, their CRC-32; little-
  endian), then each record: the key's length (1 byte), the key, the value's length (2 bytes), the value. Written whole
  to `board.sav.tmp`, flushed to the disk, then renamed over `board.sav` (a crash leaves the old file or the new one).
  A file that does not read back exactly (magic, version, length, checksum, records) is ignored, with a line on
  stderr: the app starts afresh, and the next change writes a good file over it. Written only when a value changed.
- **Privacy**: nothing leaves the device. The file holds settings and progress only, no names, nothing about the
  player; the app has no network code.

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
- **The kit** (`cards.h`). Cards are a byte: `CARD(suit, rank)` (`CARD_SUIT`, `CARD_RANK`). `DECK_FRENCH` (ace 1 .. king
  13; jokers `CARD(SUIT_JOKER, 0 red / 1 black)`), `DECK_TAROT` (14 a suit: jack 11, knight 12, queen 13, king 14;
  trumps `CARD(SUIT_TRUMP, 1..21)`, `TAROT_EXCUSE`), `DECK_NUMBER` (`NUMBER_CARD(-2..12)`), `DECK_TILES` (colours 0..3,
  1..13, two jokers), `DECK_ACTION` (colours 0..3: 0..9, `ACTION_SKIP`, `ACTION_REVERSE`, `ACTION_DRAW2`; `ACTION_WILD`,
  `ACTION_WILD4`), `DECK_ITALIAN` (`IT_COINS`, `IT_CUPS`, `IT_SWORDS`, `IT_CLUBS`; 1 .. 7, `IT_FANTE` 8, `IT_CAVALLO` 9,
  `IT_RE` 10; `cards_italian.c`; its notation: `games/italian.h`), `DECK_SPANISH` (`ES_OROS`, `ES_COPAS`, `ES_ESPADAS`,
  `ES_BASTOS`; 1 .. 9, `ES_SOTA` 10, `ES_CABALLO` 11, `ES_REY` 12; `cards_spanish.c`; its notation: `games/spanish.h`).
  `cards_pack(PACK_52 / 54 / 32 / TAROT / NUMBER / TILES / ACTION / 40 / ES40 / ES48, out)` and `cards_shuffle(cards, n,
  seed)` have no SDL: the rules deal from a seed (take it from `ui_random()`: tests and pictures repeat), undo replays.
  No letters on the faces: digits, pips, the courts as the house's chess figures in the suit's colour (jack a bishop,
  knight a knight, queen a queen, king a king), aces one big pip, trumps on a gold sun, the excuse a star, jokers a
  jester's cap; under about 52 px a plainer face (the index big). A 6 or 9 alone is underlined.
- **A game's own deck**: `cards_deck_add(&def)` (`{aspect, face(card, w, h, rgba), back(w, h, rgba) or NULL: the
  house's, symbol(suit, size, rgba) or NULL: its suits' signs}`, kept static) gives a deck number for every call below:
  the kit draws, flips, slides and piles its cards with its own shadow, gold light and lift (the families' animals:
  `games/families_deck.h` `fam_deck()`; the hanafuda: `hana_deck()`; Crapette's second pack with a red back; Pairs'
  pictures). Never lay a face over a kit card: give the game its deck, and the kit draws, flips and slides its faces.
- **Drawing**: `cards_draw(deck, card, cx, cy, w, deg, how)` (`CD_DOWN` its back, `CD_RAISED` chosen, `CD_GLOW` a card
  to tap, `CD_DIM` not playable, `CD_FLAT` no shadow), `cards_draw_alpha(..., alpha)` faded; `cards_draw_flip(..., t)`
  (0 back .. 1 face: scale-x); `cards_pile(deck, cx, cy, w, n, top, how)` (`top` `CARD_NONE`: face down; n 0: the empty
  place, in gold light with `CD_GLOW`); `cards_place` an empty place; `cards_aspect(deck)` the height / width. The
  textures are cached by deck, card and width. Pictures: `cards_paint_at` / `cards_paint_turned` (a card in a
  thumbnail), `cards_suit_icon(deck, suit, size)` (a French suit's pip on an ivory disc; an Italian or a Spanish suit's
  symbol; a game's own deck's `symbol`), `cards_token(player, size)` a chip in the player's colour.
- **Hands**: `cards_fan(cx, cy, width, w, aspect, n, arc, spots)` (overlapping when they must, fanned by `arc` degrees),
  `cards_hit(spots, n, w, aspect, x, y)` the top card under the finger; `cards_in` for a card lying straight.
- **Moving a card**: `cards_slide_start(&s, deck, card, A, up0, B, up1, ms, delay)` and `cards_slide_draw(&s)` each
  frame (1 while it moves: hide the card at its destination until then); a flip in place is a slide from A to A.
- **The card table** (`cards_table.h`, prefix `ct_`): what card games draw round their cards, written once.
  - Moving cards: one list, `ct_moves M = {.rest = 300}` (busy that long after each card lands), `ct_slide(&M, deck,
    card, A, up0, B, up1, ms, delay, tag, a, b, sound)` (its index), `ct_hides(&M, tag, a, b)` the cards still on their
    way there (-1: any; leave them out of their place until they land), `ct_show` (unseen at first: a card that waits),
    `ct_after` (drawn once another one has landed), `ct_rest`, `ct_hop`, `ct_until`, `ct_busy` (the game's `busy()`),
    `ct_moving`, `ct_quiet`, `ct_draw(&M)` each frame, `ct_calm` (they land at once).
  - The trick table (`ct_table`, `ct_layout(&T, box, players, viewer, max_hand)`; `T.deck` its cards): the viewer at the
    bottom, the others round the middle in turn order (`ct_draw_seats`, `ct_seat_spot` for what goes beside a chip), the
    trick (`ct_draw_trick`, `ct_gather`, `ct_draw_last`; `who`: each card's player for a lone hand, or NULL), the trick
    pile (`ct_draw_pile`, `ct_on_pile`), the viewer's hand (`ct_fan`, `ct_hand_draw`, `ct_hand_backs` under the cover,
    `ct_hand_hit` / `ct_hand_at`, `ct_deal`, `ct_play`) and the finger (`ct_touch`: a tap raises the card, a second tap,
    its place in the trick or a drag plays it: `ct_touch_hand`, `ct_touch_move / up`, `ct_tap_play`, `ct_lower`,
    `ct_dragged`, `ct_draw_target`). The hand's cards are told apart by their index in the hand as shown, never by their
    byte (a double pack holds two equal cards): `ct_is_raised(&F, i)`, the `CT_HAND_TAG` slides tagged by index. Several
    cards chosen at once (an exchange): `ct_chosen_hows`, `ct_toggle`, `ct_count`. The talon with its turned trump
    across under it: `ct_talon`. An open hand in suit columns at a seat (a dummy): `ct_columns_fit`, `ct_columns`,
    `ct_columns_hit`. The rules' side of trick games is `games/tricks.h` (no SDL).
  - Ready made (`ct_game`: the table, the moves, the finger, the points rising, undo's floor, the last trick shown):
    `ct_game_new / reset / look / draw_middle / draw_hand / draw_last / press_first / press_pile / press_card / release
    / played / float` (Doppelkopf, Schafkopf, Jass, Klaverjas).
  - The others along the top: `ct_others` (`ct_others_layout / x / end / draw / badge`: their fans, chips, counts and
    badges), or its parts `ct_seat`, `ct_seat_x`, `ct_backs` (a small fan of backs), `ct_backs_end`, `ct_label` (their
    chip and count); `ct_drag` (a finger dragging the chosen cards); `ct_bar_cards` (a back and the count in a bar). The
    viewer's hand fitted in a box (`ct_hand`) or along the table's bottom in one or two rows (`ct_rows_layout`,
    `ct_rows_hit`).
  - Pictures kept at the size they are drawn (take them each frame): `ct_token`, `ct_suit`, `ct_disc`, `ct_round`,
    `ct_icon` (tinted), `ct_picture` (a game's own painter: `ct_paint_fig / blend / card / piece / ivory` help paint
    it); `ct_pic(CT_ARROW / TICK / SLASH / MOON / DISC_* / CROSS / BOLT / CYCLE / HIGH / LOW / PAIR / FLOWER / CARD)`
    and the colours `CT_GOLD`, `CT_CREAM`, `CT_RED`; the calls' pictures `ct_call(CT_CALL_CARD / PAIR / FIG / UP / DOWN
    / ACES)`; `ct_button`, `ct_btn` rows (`ct_btn_row`, `ct_btn_hit`), `ct_step` (a stepper), `ct_amount` (a bet),
    `ct_on`, `ct_number` (nil: struck out), `ct_badge`, `ct_badge_plus / pic / icon / yes / no`, `ct_bolt`,
    `ct_red_disc`, `ct_strike`, `ct_pass`, `ct_pass_badge`, `ct_choice`, `ct_card_flat`.
  - The round's summary: `ct_summary(&T, rows, ...)` (`ct_summary_in` over a box) or `ct_panel(x, y, w, h, 255)`
    (opaque; less: a veil) over the dimmed table, rows split by `ct_rule`; `ct_chips`, `ct_total`, the deals played
    (`ct_deal_list`, `ct_dots`); `ct_number_w` a number's width; `ct_bar(b, deck, cards, score)`; points rising
    (`ct_floats`, `ct_float`, `ct_plus`).
  - Patience: `ct_column_steps` (a column fanned down, closer when long), `ct_run` (a run under the finger), `ct_leap`
    (the cards leaping off at a win); the games' rules share `games/solitaire.h`, their screen `games/solitaire_ui.h`.
- Scores below zero: `ui_number_signed`.
- **Golf** (`games/golf.c`) is the example: the view following the player to move, slides and flips (`ct_moves`), a
  round's summary, scores in the bars.

## Picture mode

```
retrostoneboard --shot out.bmp [--size 720x1280] [--game id] [--variant n] [--moves "..."] [--taps "fx,fy ..."]
                [--drag "fx0,fy0 fx1,fy1"] [--hold "fx,fy ms"] [--flip] [--f2f] [--keep] [--pick] [--players n]
                [--lang fr] [--help [--scroll px]] [--tab n] [--seed n] [--at ms] [--reveal] [--keys "Up Left"]
                [--level n] [--solved "1-12 13-14:2 15:0"] [--random] [--icons] [--save DIR]
```

No window: the menu (no `--game`; `--tab n` a section), a game's picker (`--pick`), or a game after its moves
(`--moves`, the game's notation) and gestures; `--help` its rules page (in `--lang`, English by default). The app
itself takes `--lang` too (else the device's language).

- **Gestures**, in the order given, in fractions of the board's box (`0.5,0.5` its middle; on the menu, a picker or
  the level screen: of the screen): `--taps` (several), `--drag` (the finger down, slid to the second point, lifted),
  `--hold` (the finger down `ms` long; as the last gesture, the picture is taken with the finger still down), `--keys`
  (keys by SDL's names: `Up`, `Left`, `Return`, `1`...).
- **Levels**: `--pick` the level screen (`--tab n` its chapter, from 0) (`--tab` the chapters' count: the random tab),
  `--random` a random puzzle (`--variant n`), `--level n` level n (from 1) at once,
  `--solved` the progress first (ranges, `:k` the stars, 3 by default).
- `--icons`: every icon of the shell on its buttons (plain, in gold light, on the gold disc).
- **Nothing kept**: the picture mode reads and writes no file; its values live in memory, empty at first, so the
  same picture comes out every time. `--save DIR` uses that folder's `board.sav` (read first, written at the end): the
  tests of what the app keeps.
- **The clock** is the picture mode's own: it stands still, so the same picture comes out every time. Before each
  gesture it moves on until the game's animations have ended (`busy()`, the dice: a deal, a slide), so no tap is
  lost; at the end it moves on until everything is at rest, or to `--at ms` after the last gesture (a slide, a flip,
  a throw on its way).
- **Card games**: `--reveal` lifts the hand-off cover (as if start were tapped: before the taps, between them and at
  the end), so a picture shows either the cover or the hand; between taps the device has passed.
