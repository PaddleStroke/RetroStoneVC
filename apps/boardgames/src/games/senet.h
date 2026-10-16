/*
 * Senet for two players on one device: the rules (Timothy Kendall's reconstruction, the one most played today). No
 * SDL, no sticks: the caller throws and gives the rules the value.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board: 30 squares in 3 rows of 10, the path a snake (boustrophedon): 1..10 along the top row from the left,
 * 11..20 back along the middle row, 21..30 along the bottom row to the right. Both players share the path.
 * The rules:
 * - Four throwing sticks, each with a light flat side and a dark round one: the light sides up count 1..4; none
 *   counts 5. A 1, a 4 or a 5 gives another throw; a 2 or a 3 ends the turn.
 * - At the start the pieces alternate on the first squares (5 each: 1..10, ivory on the odd ones; 7 each: 1..14).
 *   Ivory throws first.
 * - Move one piece forwards by the throw. A piece may not land on its own player's piece. Landing on a piece of the
 *   other player swaps the two (it goes to the square the mover came from), unless that piece is protected: a piece
 *   with one of its own on a square next to it (two or more in a row) cannot be attacked. Three or more in a row
 *   also block: no piece of the other player may pass them.
 * - Square 26 (the House of Beauty): every piece must stop on it exactly; no throw may jump past it.
 * - Square 27 (the House of Water): a piece landing on it goes back to square 15 (the House of Rebirth), or to the
 *   first free square before 15.
 * - Squares 26..30 lead off the board with the exact throw only: from 26 a 5, from 28 a 3, from 29 a 2, from 30 a 1.
 * - No piece can move forwards: one must move backwards by the throw (the same rules: no landing on its own, swaps,
 *   blocks). No move at all: the throw is lost and the turn passes.
 * - The first to bear off all their pieces wins.
 * Notation (script, the screenshots and the tests): a digit 1..5 is a throw; "s<n>" moves the piece of the player to
 * move on square n: "3 s9 1 s10 4".
 */
#ifndef SENET_H
#define SENET_H

#include <stdint.h>

#define SENET_SQ 30
#define SENET_OFF 31                            /* borne off: a "square" past the last */
#define SENET_REBIRTH 15
#define SENET_BEAUTY 26
#define SENET_WATER 27
#define SENET_MAX_PIECES 7
#define SENET_MAX_EVENTS 4096                   /* events kept for undo (a power of 2): longer, the first go */
#define SENET_MOVE_EV 32                        /* an event: 1..5 a throw, SENET_MOVE_EV + square a move */

enum { SENET_FIVE, SENET_SEVEN, SENET_VARIANTS };
enum { SENET_THROW, SENET_MOVE, SENET_OVER };   /* what comes next: a throw, a piece to move, nothing */
enum { SENET_T_MOVE = 1, SENET_T_PASS };        /* a throw: choose a piece; no move (the throw is lost) */

typedef struct {
    uint8_t sq[SENET_SQ + 1];                   /* squares 1..30: 0 empty, else 1 + the player */
    uint8_t off[2];                             /* each player's pieces borne off */
    uint8_t pieces;                             /* each player's pieces: 5 or 7 */
    uint8_t turn, phase, roll;                  /* the player to move, SENET_THROW / MOVE / OVER, the last throw */
    uint8_t back;                               /* 1: no piece can go forwards with this throw, the moves go back */
    int8_t winner;                              /* -1 */
    uint16_t throws;                            /* throws since the start */
} senet_pos;

/* A game: the position before every event, so undo is free. */
typedef struct {
    senet_pos pos, past[SENET_MAX_EVENTS];
    uint8_t ev[SENET_MAX_EVENTS];
    int n, variant;
} senet_game;

void senet_new(senet_game *g, int variant);
int  senet_again(int value);                    /* 1: a 1, a 4 or a 5: another throw */
/* the square's place on the board: col 0..9 from the left, row 0..2 from the top; 0: not a square */
int  senet_cell(int square, int *col, int *row);
int  senet_protected(const senet_pos *p, int square);   /* 1: a piece there with one of its own next to it */
int  senet_dest(const senet_pos *p, int from);  /* where the player to move's piece on `from` goes (31: off, 27 the
                                                 * water's square itself); -1: nowhere */
int  senet_takes(const senet_pos *p, int from); /* 1: that move swaps with a piece of the other player */
int  senet_landing(const senet_pos *p, int to); /* where a piece reaching `to` stays (27: 15 or before; else to) */
uint32_t senet_movable(const senet_pos *p);     /* the squares whose piece can move (bit i: square i) */
/* the position alone, no history (the bot's search): a throw made, a piece moved (legal only) */
void senet_roll(senet_pos *p, int value);
void senet_step(senet_pos *p, int from);
int  senet_throw(senet_game *g, int value);     /* SENET_T_*; 0: not now */
int  senet_move(senet_game *g, int from);       /* 0: not allowed; else 1 (+1: it swapped) */
int  senet_undo(senet_game *g);                 /* back to the last move's choice (with the throws after it) */
int  senet_script(senet_game *g, const char *moves);   /* plays throws and moves in the notation; 0: one is bad */

/* the bot (senet_bot.c, bots.h) */
const senet_game *senet_live(void);             /* the game on the screen, read-only: the bot copies it at entry */
/* main thread: plays a move as a tap does (animated): "roll" throws the sticks (the game's own), "s<n>" moves that
 * piece; 0: not now, or not legal */
int  senet_show(const char *move);

#endif
