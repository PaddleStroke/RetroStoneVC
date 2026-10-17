# Online multiplayer: what is ready

Nothing here plays online yet: no networking, no new library. This page says what a game must hold for turn-based
online play later (a server, each player on their own device), and where each game stands.

## The contract

1. A game in progress is: its id, variant, seats, seed, and the ordered moves in the game's notation (`script`).
   Replaying them from `start(variant)` with that seed rebuilds the exact state.
2. Every move goes one way: input, the UI proposes a move string, the rules check and apply it, the record grows.
3. Seats: person, bot, remote (later). A remote seat waits for a move as a bot seat does.
4. Hidden information: each seat sees only its own view. The server keeps the full state and the seed.
5. Chance comes from the rules state (a seed) or is written into the moves (dice faces). Never `rand()` or `time()`
   in the rules.

(The shell's part, the record and the seats, is described by its own section.)

## Chance (item 5)

The audit found no `rand()`, `srand()` or `time()` in any `*_rules.c`. Chance comes in three ways:

- **A seed in the rules state.** The cards, the tiles and the made puzzles: `start()` draws one number from
  `ui_random` and gives it to the rules (`cards_shuffle(seed)`, `puzrand.h`). Everything after that comes from the
  state. The record keeps that number as the game's seed.
- **Throws in the notation.** Dice games: the UI throws with `ui_random` and the faces are part of the move (`r4`,
  `d53`, `31:`). A replay needs no seed. Online, the server throws and sends the move. A client never sends its own
  faces.
- **A start draw that is not called a seed** (chess 960's position, the free cells deal number, the chess puzzle
  drawn): record that number as the seed.

To fix later (left as is: changing them would change pictures and tests, and all are one-player):

- `mahjong`: the deal and the `s` shuffle draw through an `rnd` callback (the shell's `ui_random`). The rules state
  has no seed of its own, so a replay must re-seed the shell. Fix: a `uint32_t seed` in `mjgame` and a rules-side
  generator (`cards_rand`).
- `secretcode`: the code is drawn through the same kind of callback at start.
- `logicgates_rules.c`: `search_orders` stops on `clock()` as a safety net (the drawing's layout only; in practice it
  always finishes first). It should be a count of tries.
- `mexico`: the script's `t??` throws with `ui_random`. A record must store the faces (`t53`).
- `wordtiles`: the rack rearrange button draws `ui_random` in the UI. It only changes the order on screen, not the
  rules.

Physics: `carrom` and `pool` replay their shots with floats. That is the same on one machine but may differ between
CPUs or compilers. Online, the server's result should travel with the move, or the physics should use fixed point.

## Moves through one path (item 2)

How it was checked: in each UI file, writes to the rules state variable (and its aliases) outside the rules
functions. The UI files were only read, not edited (the graphics agents own them).

- **Every two-player and multiplayer game passes.** Their UIs change the state only through the rules functions
  (`xx_play`, `xx_roll`...), the same ones `script` uses.
- **A few one-player puzzles write the state themselves** (see the notes column). None of them matters for online
  play:
  - hint counters: flood, tileconnect, twistycube, logicgates;
  - values computed at start from the seed: samegame, taquin;
  - the active cube on a hint: rollblock;
  - layout fix-ups: jigsaw;
  - a live drag that is put back before the real move: untangle;
  - undo history after a script: secretcode;
  - `G.last` before a hint's move: parking;
  - the thumbnail: wordtiles.
- **Left for the shell work.** The UI calls the rules with a move value, not a string. For the UI to *propose a
  string*, each game needs a move-to-notation formatter. Some rules headers already export one (draughts, halma,
  canasta, skat, yacht...). Most only parse notation. Adding `xx_format(move, buf)` per game is the main job left.

## Hidden information (item 4)

Proposed hook, to add to `game` as optional (NULL means everything is public): `visible_to(seat)`. It gives the
state with what the seat may not see replaced by "unknown". The rules-side form is
`void xx_view(const pos *p, int seat, pos *out)`: a copy with the hidden cards set to `CARD_NONE` and the seed zeroed
(the seed would give away the deal). **Model: `golf_view()` in `golf_rules.c`**, tested in `tests/golf_test.c`.

By family:

- **Open boards** (chess, go, draughts, hex...): nothing hidden, NULL.
- **Dice games** (backgammon, ludo, pig, yacht...): nothing hidden, NULL. The server throws. Liar's dice and Mexico
  hide each player's dice under the cup.
- **`VIEW_OPEN` card and tile games** (golf, war, crapette, pairs, banqi's face-down pieces): the face-down cards and
  the stock are hidden *from everyone*. The view is the same for every seat (golf_view).
- **`VIEW_HIDDEN` card and tile games** (hearts, belote, bridge, tarot, dominoes, tile rummy, word tiles...): the view
  is the seat's own hand, plus the public table (tricks, the discard pile's top, melds) and the other hands' *counts*;
  the stock order and the seed are hidden. The card kit already places each seat (`ct_` seats, `ui.viewer`), so a
  view per seat is the rules' hands with every other seat's cards set to `CARD_NONE`. `durak.hidden()` says when the
  hands are open.
- **Secret roles or setups**: seabattle (each fleet), spy and werewolves (roles), drawguess (the word),
  mysteryface (each chosen face). The view hides the other seats' secret.
- **One player** (patience, puzzles): no online play. Their seeds already rebuild them.

Bots: they read the whole live state (for example `golf_live()`). In hidden games, a bot that should not see more
than a person should start from `xx_view(seat)`.

## The shell's part: the record, the moves' notation, the seats (items 1-3)

### The record (item 1)

The save-game work keeps each game in progress (`shell.c`, "the game in progress kept"): the game id, the variant
(or level), random play or not, the players' count, each seat (person, or a bot and its level), the random state
`start()` was given, then every event in order. A person's moves reach the shell only as taps, so that record
replays the raw input. It cannot be sent to another device as it is: the other screen has another size and another
layout.

So a game may now give its moves in its own notation, whoever made them:

```c
/* game.history, optional (NULL: none) */
int (*history)(int i, char *move, int cap);
```

- `history(-1, NULL, 0)` returns how many moves have been played since `start()`. Undo takes them back. A game that
  forgets its oldest moves (a very long game) still counts them.
- `history(i, buf, cap)` writes move `i` in the notation `script()` reads ("" if it was forgotten).
- **The contract**: `start(variant)` with the same random state, then `script()` of all the moves joined by spaces,
  plays the same game again. A move is one action: a throw of the dice (`31:`), a card (`Th`), a pass of three cards
  (`pQsAh2c`), the tap to the next round (`n`), a checker (`13/10`), a disc (`d3`).

What the shell does with it:

- After every recorded event and on every frame of the game, it compares the game's moves with the list it keeps for
  that game (`NT[g]`). It takes off the moves that were undone and adds the new ones. Only the count is asked while
  nothing changes; the last move is compared too, which catches an undo and a new move together.
- It logs each change in the record as `EV_NOTE` (how many moves stay, then each new move's notation; no time of its
  own). When the record is played again (`--resume`), `EV_NOTE` plays nothing and only rebuilds the list. The record
  is now version 2; version 1 records still resume.
- The picture mode prints the list with `--notation`. `--moves` of that list plays the same game:

```
retrostoneboard --shot a.bmp --game chess --bots all:e --steps 12 --seed 3 --notation
notation: c2c4 g8f6 b1c3 e7e5 a2a4 f8e7 a4a5 b8a6 g1f3 e8h8 f3e5 e7d6
retrostoneboard --shot b.bmp --game chess --bots all:e --moves "c2c4 g8f6 ..." --seed 3 --notation
(the same list, the same picture)
```

Checked this way for chess, reversi, hearts (4 players) and backgammon: the same list, and the same picture (for
backgammon the dice's tilt differs: it is drawn at random when they are thrown, not part of the state), and the same
list after `--resume`.

**Games with `history()` so far** (the proof of the contract):

| game | notation writer (rules side, no SDL) | count |
|---|---|---|
| chess | `chess_move_text()` (chess.c), also used by its bot now | `G.ply` |
| reversi | in reversi.c (a square's name) | `G.ply` |
| hearts | `hearts_act_text()` (hearts_rules.c), also used by its bot now | `G.n` |
| backgammon | `bg_event_text()` (backgammon_rules.c): throws and single checker moves | `G.forgot + G.n` |
| hanoi | `hn_move_text()` (hanoi_rules.c) | `G.moves` |

Backgammon's events are now counted from the start (`bgame.forgot`: the events dropped when the 2048-event history
is full), so the count never goes back on a long game.

**Adding it to a game** (the main job left for each game, after the formatter part B lists above):

1. In `<id>_rules.c`, a formatter `xx_move_text(move, buf, cap)` that writes what `xx_script()` parses. If the bot
   writes its move by hand, make it call the formatter (no copied code).
2. In `<id>.c`, `history()` reads the rules state's move list. Return a count from the start, even if the list is a
   window (hanoi keeps the last 1024).
3. Check with `--notation`, then `--moves` of the result: the same picture.

Logic puzzles built on `bench.h` keep a list of *changes* (a cell and what it held), not of moves. They need the new
value stored too (`bench_change.now`) before they can say a move's notation.

### One path for every move (item 2)

Today a person's move goes: tap, the game's UI, the rules. A bot's move goes: its notation, `play()` (animated, as a
tap would) or `script()`. Both end in the same rules functions, and `history()` reports both the same way. Online, a
local person's move is what `history()` adds after their input: the shell sends that string. A remote move arrives as
a string and is played the way a bot's move is (`bot.play` or `script()`).

### Seats (item 3)

`shell.c` now has a seat type: `SEAT_PERSON`, `SEAT_BOT`, `SEAT_REMOTE`. Nothing makes a remote seat yet
(`remotemask` is always 0), so nothing has changed in play. The checks that meant "a person here" or "not a person
here" now go through helpers:

- `seat_is_local_person(p)`: face to face (`ui_upside`), the first person after a seat (`human_from`), the pass
  button.
- `seat_waits_for_move(p)`: the input is held while that seat must act (`bot_holds`), the hand-off to it keeps the
  last person's view (`ui_handoff`), undo goes back over its moves to the last person's (`undo_human`).
- `elsewhere()`: the seats no one here plays (bots and remotes). `humans()` counts the others, which also decides the
  cover between hands, the watch mode and the stats.
- `is_bot(p)` stays only for what belongs to a bot: starting its worker, its thinking badge, its level in the stats.

What is left for online play in the shell: a way to choose a remote seat (the picker), the network itself, and the
step that plays a received move (a `remote_step()` next to `bot_step()`, playing the move with `bot.play` or
`script()`). The record already has what a server would keep: the head (id, variant, seats, seed) and the moves'
notation.

## Per game

﻿| id | players | move path OK? | RNG OK? | hidden info | notes |
|---|---|---|---|---|---|
| chess | 2 | OK | OK: start draw | none | 960 start position drawn by ui_random(960) at start: record it as the seed |
| accordion | 1 | OK | OK: seed in state | stock order (one player) |  |
| alquerque | 2 | OK | OK: no chance | none |  |
| amazons | 2 | OK | OK: no chance | none |  |
| backgammon | 2 | OK | OK: throws in the notation | none | opening throw drawn in UI, recorded as "31:" |
| baghchal | 2 | OK | OK: no chance | none |  |
| bakersdozen | 1 | OK | OK: seed in state | stock order (one player) |  |
| ballsort | 1 | OK | OK: seed in state | none |  |
| banqi | 2 | OK | OK: seed in state | face-down pieces (from everyone) | "@seed" heads the notation (the deal) |
| bao | 2 | OK | OK: no chance | none |  |
| barbu | 2.. | OK | OK: seed in state | hands; the stock |  |
| bellelucie | 1 | OK | OK: seed in state | stock order (one player) |  |
| belote | 2.. | OK | OK: seed in state | hands; the stock |  |
| bigtwo | 2.. | OK | OK: seed in state | hands; the stock |  |
| binairo | 1 | OK | OK: seed in state | none |  |
| blocks | 1 | OK | OK: seed in state | none |  |
| boxpusher | 1 | OK | OK: no chance | none |  |
| breakthrough | 2 | OK | OK: no chance | none |  |
| bridge | 2.. | OK | OK: seed in state | hands; the stock |  |
| briscola | 2.. | OK | OK: seed in state | hands; the stock |  |
| calcudoku | 1 | OK | OK: seed in state | none |  |
| canasta | 2.. | OK | OK: seed in state | hands; the stock |  |
| canfield | 1 | OK | OK: seed in state | stock order (one player) |  |
| carrom | 2.. | OK | OK: no chance | none |  |
| chesspuzzles | 1 | OK | OK: start draw | none | the puzzle drawn by ui_random at start: record it as the seed |
| clock | 1 | OK | OK: seed in state | stock order (one player) |  |
| crapette | 2 | OK | OK: seed in state | stocks (from everyone) |  |
| crazyeights | 2.. | OK | OK: seed in state | hands; the stock |  |
| cribbage | 2.. | OK | OK: seed in state | hands; the crib; the deck |  |
| dara | 2 | OK | OK: no chance | none |  |
| diagonalsudoku | 1 | OK | OK: seed in state | none |  |
| dominoes | 2.. | OK | OK: seed in state | hands; the boneyard |  |
| dominosa | 1 | OK | OK: seed in state | none |  |
| doppelkopf | 2.. | OK | OK: seed in state | hands; the stock |  |
| dots | 2 | OK | OK: no chance | none |  |
| draughts | 2 | OK | OK: no chance | none |  |
| drawguess | 2 | OK | OK: seed in state | the word (all but the drawer) |  |
| durak | 2.. | OK | OK: seed in state | hands (open variant: none); the stock | .hidden(): hands secret only in some variants |
| ecarte | 2 | OK | OK: seed in state | hands; the stock |  |
| euchre | 2.. | OK | OK: seed in state | hands; the stock |  |
| families | 2.. | OK | OK: seed in state | hands; the stock |  |
| fanorona | 2 | OK | OK: no chance | none |  |
| farkle | 2.. | OK | OK: throws in the notation | none |  |
| fillomino | 1 | OK | OK: seed in state | none |  |
| flood | 1 | notes | OK: seed in state | none | UI counts hints (G.hints++) |
| fortythieves | 1 | OK | OK: seed in state | stock order (one player) |  |
| fourrow | 2 | OK | OK: no chance | none |  |
| fourtwoone | 2.. | OK | OK: throws in the notation | none |  |
| foxgeese | 2 | OK | OK: no chance | none |  |
| freecells | 1 | OK | OK: seed in state | stock order (one player) | the deal number drawn at start: record it as the seed |
| futoshiki | 1 | OK | OK: seed in state | none |  |
| ginrummy | 2 | OK | OK: seed in state | hands; the stock |  |
| go | 2 | OK | OK: no chance | none |  |
| golf | 2.. | OK | OK: seed in state | face-down cards, stock (from everyone) | model view: golf_view() in golf_rules.c (tested) |
| golfpatience | 1 | OK | OK: seed in state | stock order (one player) |  |
| gomoku | 2 | OK | OK: no chance | none |  |
| goose | 2.. | OK | OK: throws in the notation | none |  |
| goproblems | 1 | OK | OK: no chance | none |  |
| halma | 2.. | OK | OK: no chance | none |  |
| hanoi | 1 | OK | OK: seed in state | none |  |
| hashi | 1 | OK | OK: seed in state | none |  |
| hearts | 2.. | OK | OK: seed in state | hands; the stock |  |
| hex | 2 | OK | OK: no chance | none |  |
| heyawake | 1 | OK | OK: seed in state | none |  |
| hitori | 1 | OK | OK: seed in state | none |  |
| iceslide | 1 | OK | OK: seed in state | none |  |
| janggi | 2 | OK | OK: no chance | none |  |
| jass | 2.. | OK | OK: seed in state | hands; the stock |  |
| jigsaw | 1 | notes | OK: seed in state | none | UI clamps piece groups (G.s.p[].x/y) on layout: a per-screen fix-up, not a move |
| jigsawsudoku | 1 | OK | OK: seed in state | none |  |
| jungle | 2 | OK | OK: no chance | none |  |
| kakuro | 1 | OK | OK: seed in state | none |  |
| killersudoku | 1 | OK | OK: seed in state | none |  |
| klaverjas | 2.. | OK | OK: seed in state | hands; the stock |  |
| klondike | 1 | OK | OK: seed in state | stock order (one player) |  |
| klotski | 1 | OK | OK: seed in state | none |  |
| knightstour | 1 | OK | OK: seed in state | none |  |
| koikoi | 2 | OK | OK: seed in state | hands; the deck |  |
| konane | 2 | OK | OK: no chance | none |  |
| lasca | 2 | OK | OK: no chance | none |  |
| liarsdice | 2.. | OK | OK: throws in the notation | dice under each cup |  |
| lights | 1 | OK | OK: seed in state | none |  |
| lightup | 1 | OK | OK: seed in state | none |  |
| loa | 2 | OK | OK: no chance | none |  |
| logicgates | 1 | notes | OK: seed in state | stock order (one player) | rules layout search has a clock() safety net (lg_route_fast; drawing only, deterministic in practice); UI sets G.hinted |
| loop | 1 | OK | OK: seed in state | none |  |
| loopshift | 1 | OK | OK: seed in state | none |  |
| ludo | 2.. | OK | OK: throws in the notation | none |  |
| mahjong4 | 2.. | OK | OK: seed in state | hands; the wall |  |
| mahjong | 1 | OK | callback (shell stream) | none | "s" (shuffle) draws from the shell stream through an rnd callback, not a state seed: a replay must re-seed the shell; deal too |
| makruk | 2 | OK | OK: no chance | none |  |
| mancala | 2 | OK | OK: no chance | none |  |
| manille | 2.. | OK | OK: seed in state | hands; the stock |  |
| marblepush | 2 | OK | OK: no chance | none |  |
| masyu | 1 | OK | OK: seed in state | none |  |
| mathpz | 1 | OK | OK: seed in state | none |  |
| melody | 1 | OK | OK: seed in state | none |  |
| merge | 1 | OK | OK: seed in state | none |  |
| mexico | 2.. | OK | OK: throws in the notation | the throw under the cup | the script's "t??" throws via ui_random: a record must store the faces ("t53"), never "t??" |
| mines | 1 | OK | OK: seed in state | none |  |
| mirrors | 1 | OK | OK: seed in state | none |  |
| montana | 1 | OK | OK: seed in state | stock order (one player) |  |
| morris | 2 | OK | OK: no chance | none |  |
| mus | 2.. | OK | OK: seed in state | hands; the stock |  |
| mysteryface | 2 | OK | OK: no chance | each seat's chosen face |  |
| nainjaune | 2.. | OK | OK: seed in state | hands; the stock |  |
| nim | 2 | OK | OK: seed in state | none |  |
| nonogram | 1 | OK | OK: seed in state | none |  |
| numberpath | 1 | OK | OK: seed in state | none |  |
| nurikabe | 1 | OK | OK: seed in state | none |  |
| oldmaid | 2.. | OK | OK: seed in state | hands |  |
| pachisi | 2.. | OK | OK: throws in the notation | none |  |
| pairs | 2.. | OK | OK: seed in state | face-down tiles (from everyone) |  |
| parking | 1 | notes | OK: seed in state | none | UI hint sets G.last before its move |
| paths | 1 | OK | OK: seed in state | none |  |
| pegsolitaire | 1 | OK | OK: seed in state | none |  |
| pig | 2.. | OK | OK: throws in the notation | none |  |
| pinochle | 2.. | OK | OK: seed in state | hands; the stock |  |
| pipes | 1 | OK | OK: seed in state | none |  |
| piquet | 2 | OK | OK: seed in state | hands; the stock |  |
| pool | 2 | OK | OK: seed in state | none |  |
| president | 2.. | OK | OK: seed in state | hands; the stock |  |
| pyramid | 1 | OK | OK: seed in state | stock order (one player) |  |
| queens | 1 | OK | OK: seed in state | none |  |
| rectangles | 1 | OK | OK: seed in state | none |  |
| reversi | 2 | OK | OK: no chance | none |  |
| rikiki | 2.. | OK | OK: seed in state | hands; the stock |  |
| rithmomachia | 2 | OK | OK: no chance | none |  |
| rollblock | 1 | notes | OK: seed in state | none | UI hint writes G.s.act (the active cube; choosing a cube is not a move) |
| samegame | 1 | notes | OK: seed in state | none | start_random writes G.goal (computed from the seed) |
| schafkopf | 2.. | OK | OK: seed in state | hands; the stock |  |
| scopa | 2.. | OK | OK: seed in state | hands; the stock |  |
| scorpion | 1 | OK | OK: seed in state | stock order (one player) |  |
| seabattle | 2 | OK | OK: seed in state | fleets (each seat its own) |  |
| secretcode | 1 | notes | callback (shell stream) | none | the code drawn through an rnd callback at start (shell stream), not a state seed; UI clears G.nh after a script |
| senet | 2 | OK | OK: throws in the notation | none |  |
| shipcaptain | 2.. | OK | OK: throws in the notation | none |  |
| shogi | 2 | OK | OK: no chance | none |  |
| shogimate | 1 | OK | OK: no chance | none |  |
| shutthebox | 2.. | OK | OK: throws in the notation | none |  |
| skat | 2.. | OK | OK: seed in state | hands; the stock |  |
| skyscrapers | 1 | OK | OK: seed in state | none |  |
| slap | 2.. | OK | OK: seed in state | the piles (from everyone) |  |
| snakes | 2.. | OK | OK: throws in the notation | none |  |
| spades | 2.. | OK | OK: seed in state | hands; the stock |  |
| spider | 1 | OK | OK: seed in state | stock order (one player) |  |
| spotdiff | 1 | OK | OK: seed in state | none |  |
| spread | 2 | OK | OK: no chance | none |  |
| spy | 2 | OK | OK: seed in state | roles, the secret place |  |
| starbattle | 1 | OK | OK: seed in state | none |  |
| sudoku | 1 | OK | OK: seed in state | none |  |
| surakarta | 2 | OK | OK: no chance | none |  |
| tafl | 2 | OK | OK: no chance | none |  |
| tangram | 1 | OK | OK: seed in state | none |  |
| taquin | 1 | notes | OK: seed in state | none | start_random writes G.picture / G.par (computed from the seed) |
| tarot | 2.. | OK | OK: seed in state | hands; the stock |  |
| tents | 1 | OK | OK: seed in state | none |  |
| thermometers | 1 | OK | OK: seed in state | none |  |
| threepeaks | 1 | OK | OK: seed in state | stock order (one player) |  |
| tictactoe | 2 | OK | OK: no chance | none |  |
| tienlen | 2.. | OK | OK: seed in state | hands; the stock |  |
| tileconnect | 1 | notes | OK: seed in state | none | UI counts hints (G.hints++) |
| tilerummy | 2.. | OK | OK: seed in state | racks; the pool |  |
| tracks | 1 | OK | OK: seed in state | none |  |
| truco | 2.. | OK | OK: seed in state | hands; the stock |  |
| twistycube | 1 | notes | OK: seed in state | none | UI counts hints (G.hints++) |
| untangle | 1 | notes | OK: seed in state | none | live drag writes G.x/G.y, then restores and moves through ut_move (OK in the end) |
| ur | 2 | OK | OK: throws in the notation | none |  |
| war | 2.. | OK | OK: seed in state | the piles (from everyone) |  |
| watersort | 1 | OK | OK: seed in state | none |  |
| werewolves | 2 | OK | OK: seed in state | roles |  |
| whist | 2.. | OK | OK: seed in state | hands; the stock |  |
| wordtiles | 2.. | notes | OK: seed in state | racks; the bag | the rack rearrange button draws ui_random in the UI (cosmetic order, not a move); thumb() pokes G.pos.set and restores it |
| xiangqi | 2 | OK | OK: no chance | none |  |
| yacht | 2.. | OK | OK: throws in the notation | none |  |
| ygame | 2 | OK | OK: no chance | none |  |
| yukon | 1 | OK | OK: seed in state | stock order (one player) |  |
