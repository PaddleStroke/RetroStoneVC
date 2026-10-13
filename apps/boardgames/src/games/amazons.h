/*
 * The Game of the Amazons for two players on one device: the rules (queen moves, arrows, the end). No engine.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board: 10 x 10 squares, a1 at the bottom left (player 0's side), a square x + y * 10 (x the file a..j, y the
 * rank 1..10, from 0). A square holds AM_EMPTY, an amazon (AM_AMAZON + player) or AM_ARROW (burnt for good).
 * The start: player 0 (ivory) on a4 d1 g1 j4, player 1 (ebony) on a7 d10 g10 j7; player 0 moves first.
 * A turn has two steps: an amazon of the player moves like a chess queen (any distance along a row, a column or a
 * diagonal, across empty squares only, no capturing), then from its new square it shoots an arrow the same way onto
 * an empty square, which is blocked for the rest of the game. The arrow may land on the square the amazon left.
 * A player who cannot make a whole turn loses: since the arrow can always fly back to the square just left, that is
 * a player whose amazons cannot move at all. No draws (every turn burns a square).
 * Here each step is a move of its own (undo takes back one step): `stage` says which comes next.
 * Notation (script, the screenshots and the tests): "d1-d7/g7" an amazon from d1 to d7, its arrow to g7; a step
 * alone, "d1-d7" (the arrow comes next) or "/g7" (the arrow of the amazon just moved); separated by spaces.
 */
#ifndef AMAZONS_H
#define AMAZONS_H

#include <stdint.h>

#define AM_N 10
#define AM_SQUARES (AM_N * AM_N)
#define AM_MAX_LINE 36                          /* the most squares a queen sees from one square (4 lines of 9) */
#define AM_MAX_PLY (2 * (AM_SQUARES - 8))       /* every turn burns a square: 92 turns of two steps at most */

enum { AM_EMPTY, AM_AMAZON, AM_ARROW = AM_AMAZON + 2 };
enum { AM_MOVE, AM_SHOOT };                     /* the steps of a turn */

typedef struct {
    uint8_t sq[AM_SQUARES];
    uint8_t side;                               /* whose turn */
    uint8_t stage;                              /* AM_MOVE or AM_SHOOT */
    int8_t at;                                  /* AM_SHOOT: the amazon that just moved, which shoots */
} ampos;

typedef struct { int8_t from, to; } amstep;     /* a move (from, to), or an arrow (from: the shooter) */

int  am_square(const char *s);                  /* "d10": its square; -1 */
void am_start(ampos *p);
int  am_line(const ampos *p, int from, uint8_t *out);   /* the empty squares a queen at `from` reaches; their count */
int  am_full_moves(const ampos *p);             /* the whole turns (move and arrow) the player to move has */
int  am_can_move(const ampos *p);               /* 1: the player to move has a whole turn */

/* A game: every position since the start, so undo is free. */
typedef struct {
    ampos pos[AM_MAX_PLY + 1];
    amstep step[AM_MAX_PLY];
    int ply;
} amgame;
void am_new(amgame *g);
int  am_move(amgame *g, int from, int to);      /* the turn's first step; 0: not allowed */
int  am_shoot(amgame *g, int to);               /* its second (from the amazon that moved); 0: not allowed */
int  am_undo(amgame *g);                        /* back one step; 0: nothing to undo */
int  am_winner(const amgame *g);                /* -1 the game goes on, else the winner (the other cannot move) */
int  am_script(amgame *g, const char *steps);   /* plays steps in the notation above; 0: one is bad */
#define am_pos(g) (&(g)->pos[(g)->ply])

/* the bot (amazons_bot.c, bots.h) */
const amgame *amazons_live(void);               /* the game on the screen, read-only: the bot copies it at entry */
int amazons_show(const char *turn);             /* main thread: "d1-d7/g7" as taps play it (animated); 0: not legal */

#endif
