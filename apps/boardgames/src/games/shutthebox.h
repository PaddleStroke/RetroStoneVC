/*
 * Shut the Box (Ferme la boîte), the pub dice game, for one to four players on one device: the rules (the tiles, the
 * throws, the combinations, the one-die rule, the end of a turn, the scores). No SDL, no dice: the caller throws and
 * gives the rules the values.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Each player has a row of tiles numbered 1..9 (or 1..10, 1..12: the variants), all standing (open) at the start.
 * - A turn: throw two dice, then shut any open tiles whose numbers add up to the throw (one tile or several). Throw
 *   again, shut again... until a throw no open tiles can make: the turn ends, and its score is the sum of the tiles
 *   still open (lower is better).
 * - The one-die rule: once the player's tiles 7 and up (7, 8, 9; with 10 and 12 tiles all of 7..10, 7..12) are all
 *   shut, the player may throw one die instead of two, every throw (their choice).
 * - Shutting every tile shuts the box: that player wins at once, and the game ends there.
 * - One player: one turn against their own score; a shut box is solved, else the score stands.
 * - Two to four players: each has their own row; each plays one full turn, clockwise round the table (two: 0, 1;
 *   three: 0, 2, 1; four: 0, 2, 1, 3: the seats, clockwise from the bottom, are player 0's, 2's, 1's, 3's, player 1
 *   facing player 0 as with two). After the round the lowest score wins; a tie for the lowest is a draw.
 * Notation (script, the screenshots and the tests): "d" and the dice thrown ("d53" two dice, 5 and 3; "d4" one die),
 * then the tiles shut, joined by "+" ("8", "1+7", "2+10"); a throw that ends the turn needs nothing more:
 * "d53 8 d62 1+7 d44".
 */
#ifndef SHUTTHEBOX_H
#define SHUTTHEBOX_H

#include <stdint.h>

#define STB_PLAYERS 4
#define STB_MAX_TILES 12
#define STB_MAX_EVENTS 160                      /* throws and shuts: four players' turns at most (4 x (12 + 13)) */
#define STB_THROW_EV 0x8000u                    /* an event: a throw (| dice << 8 | first << 4 | second), else the
                                                 * tiles shut (bit t: tile t) */

enum { STB_NINE, STB_TEN, STB_TWELVE, STB_VARIANTS };    /* the tiles: 1..9, 1..10, 1..12 */
enum { STB_THROW, STB_SHUT, STB_OVER };         /* what comes next: a throw, tiles to shut, nothing */
enum { ST_SHUT = 1, ST_DEAD };                  /* a throw: tiles to shut; no combination (the turn ended) */

typedef struct {
    uint16_t open[STB_PLAYERS];                 /* each player's standing tiles: bit t, tile t (1..12) */
    uint8_t variant, tiles, players;            /* STB_NINE..; 9, 10 or 12; how many play (1..4) */
    uint8_t turn, phase;                        /* the player to throw or shut; STB_THROW / SHUT / OVER */
    uint8_t ndice, die[2];                      /* the throw to shut: one or two dice */
    uint8_t played;                             /* bit p: player p's turn is over */
    uint8_t boxed;                              /* 1: the box was shut (by `winner`) */
    int8_t winner;                              /* -1: none (playing, a draw, one player who did not shut it) */
    uint16_t throws;                            /* throws since the start */
} spos;

/* A game: the position before every event, so undo is free. */
typedef struct {
    spos pos, past[STB_MAX_EVENTS];
    uint16_t ev[STB_MAX_EVENTS];
    int n;
} sgame;

void stb_new(sgame *g, int variant, int players);
int  stb_tiles(int variant);                    /* 9, 10, 12 */
int  stb_seat(int player);                      /* the player's side, clockwise from the bottom: 0, 2, 1, 3 */
unsigned stb_full(int tiles);                   /* all the tiles: bits 1..tiles */
int  stb_sum(unsigned tiles);                   /* the tiles' numbers added up */
int  stb_combos(unsigned open, int sum);        /* how many sets of the open tiles make the sum */
int  stb_total(const spos *p);                  /* the throw to shut (0: none) */
int  stb_one_die(const spos *p);                /* 1: the player to throw may throw one die (tiles 7 and up shut) */
/* the open tiles that can join `sel` (tiles already chosen) in a set making the throw */
unsigned stb_usable(const spos *p, unsigned sel);
int  stb_throw(sgame *g, int ndice, int first, int second);   /* ST_*; 0: not now (second unused with one die) */
int  stb_shut(sgame *g, unsigned tiles);        /* 0: not allowed; 1: shut (throw again); 2: the box shut (a win) */
int  stb_score(const spos *p, int player);      /* the sum of the player's open tiles */
int  stb_undo(sgame *g);                        /* back to before the last tiles shut (the throws after dropped);
                                                 * 0: none */
int  stb_script(sgame *g, const char *moves);   /* plays throws and shuts in the notation above; 0: one is bad */

#endif
