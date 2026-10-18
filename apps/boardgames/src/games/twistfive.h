/*
 * Twist Five for two players on one device: the rules (a marble placed, a quarter of the board turned, five in a row).
 * No SDL, no engine.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board: 6 x 6 holes, a1 at the bottom left, a hole x + 6 y (x the file a..f, y the rank 1..6, from 0), in four
 * quarters of 3 x 3 that turn: 1 the top left (a4..c6), 2 the top right, 3 the bottom left, 4 the bottom right.
 * A turn has two steps: the player places a marble in an empty hole, then turns one quarter (any one, empty or not)
 * a quarter turn, clockwise or anticlockwise (as seen with a1 at the bottom left). Player 0 (ivory) begins.
 * Five marbles of one colour in a row (across, up and down, or diagonally) win: right after the marble is placed (the
 * turn then needs no twist), or after the twist. A twist that makes five for both players at once: a draw. The board
 * full after a twist and no five: a draw.
 * Here each step is a move of its own (undo takes back one step): `stage` says which comes next.
 * Notation (script, the screenshots, the tests, the bots): "c4/2+" a marble on c4, then quarter 2 turned clockwise
 * ('-' anticlockwise); a step alone, "c4" or "/2+"; a winning marble alone "c4"; separated by spaces.
 */
#ifndef TWISTFIVE_H
#define TWISTFIVE_H

#include <stdint.h>

#define TV_N 6
#define TV_CELLS 36
#define TV_LINES 32                             /* the rows of five on the board */
#define TV_DRAW 2                               /* tvpos.winner: nobody */
#define TV_MAX_PLY (2 * TV_CELLS)

enum { TV_PLACE, TV_TWIST };                    /* the steps of a turn */
enum { TV_CW, TV_CCW };

typedef struct {
    uint8_t cell[TV_CELLS];                     /* 0 empty, else player + 1 */
    uint8_t side;                               /* whose turn */
    uint8_t stage;                              /* TV_PLACE or TV_TWIST */
    int8_t last;                                /* the marble just placed (TV_TWIST), else -1 */
    int8_t winner;                              /* -1 playing, 0, 1, TV_DRAW */
    uint8_t count;                              /* marbles on the board */
} tvpos;

extern const uint8_t tv_line[TV_LINES][5];      /* the holes of each row of five */
int  tv_square(const char *s);                  /* "c4": its hole; -1 */
int  tv_quarter_of(int hole);                   /* 0..3 (quarters 1..4) */
/* the hole where the marble on `hole` goes when quarter q (0..3) turns (TV_CW, TV_CCW); a hole of another quarter
 * stays */
int  tv_turned(int hole, int q, int dir);
void tv_rotate(uint8_t *cell, int q, int dir);  /* turns a quarter of the board */
int  tv_five(const uint8_t *cell, int player);  /* 1: five of the player's in a row */
/* the holes in rows of five (mark[h] 1), any player's; their count */
int  tv_marks(const uint8_t *cell, uint8_t *mark);
void tv_start(tvpos *p);
void tv_place(tvpos *p, int hole);              /* a legal step (empty hole, TV_PLACE), the win checked */
void tv_twist(tvpos *p, int q, int dir);        /* a legal step (TV_TWIST), the end checked */

/* A game: every position since the start, so undo is free. */
typedef struct {
    tvpos pos[TV_MAX_PLY + 1];
    int8_t step[TV_MAX_PLY];                    /* a hole, or 64 + q * 2 + dir */
    int ply;
} tvgame;
void tv_new(tvgame *g);
int  tv_play_place(tvgame *g, int hole);        /* 0: not allowed */
int  tv_play_twist(tvgame *g, int q, int dir);  /* 0: not allowed */
int  tv_undo(tvgame *g);
int  tv_script(tvgame *g, const char *steps);   /* 0: one is bad (those before it played) */
#define tv_pos(g) (&(g)->pos[(g)->ply])

/* the bot (twistfive_bot.c, bots.h) */
const tvgame *twistfive_live(void);             /* the game on the screen, read-only: the bot copies it at entry */
int twistfive_show(const char *turn);           /* main thread: "c4/2+" as taps play it (animated); 0: not legal */

#endif
