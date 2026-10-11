/*
 * Peg Solitaire for one player: the rules. Marbles fill the holes of a board but one; a marble jumps over a
 * neighbour into an empty hole just beyond it, in a straight line, and the marble jumped over is taken off. The aim:
 * one marble left, ideally in the board's own goal hole. No jump left with more than one marble: the game is over,
 * and the marbles left are its score (the fewer the better).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The boards, on a grid of 7 x 7 (column x from the left, row y from the top; a cell is y * 7 + x):
 *   0 English: the cross of 33 holes; the centre d4 empty at the start, and the goal.
 *   1 French (European): the cross and its four inner corners, 37 holes. No game from a single empty hole can end
 *     in the centre on this board (a parity rule), so the classic start is used: d6 (the middle column's second
 *     hole from the top) empty; the goal is its mirror, d2.
 *   2 Triangle: 15 holes in 5 rows (row y holds x = 0..y); the jumps go along its three lines (sideways, and along
 *     both slants: the steps (+-1, 0), (0, +-1), (+-1, +-1)). The top hole a5 empty at the start, and the goal.
 * The notation (script, the screenshots, the tests): a jump is its two holes, "d6d4"; a hole is its column letter
 * a.. and its row counted from the bottom (1..7; the triangle: 1..5, so its top is a5).
 */
#ifndef PEGSOLITAIRE_H
#define PEGSOLITAIRE_H

#include <stdint.h>

#define PS_VARIANTS 3
#define PS_SIDE 7
#define PS_CELLS (PS_SIDE * PS_SIDE)
#define PS_MAX_MOVES 128                        /* the jumps from a position (at most 37 marbles x 4 ways) */

enum { PS_ENGLISH, PS_FRENCH, PS_TRIANGLE };
enum { PS_PLAY, PS_ONE, PS_STUCK };             /* one marble left (solved); several and no jump (over) */

typedef struct { int8_t from, over, to; } psmove;

typedef struct {
    int variant;
    int rows;                                   /* 7, or 5 for the triangle */
    int goal;                                   /* the hole empty at the start: the goal for the last marble */
    uint8_t hole[PS_CELLS];                     /* 1: a hole of this board */
    uint8_t peg[PS_CELLS];                      /* 1: a marble in it */
    int holes, pegs;
    psmove hist[PS_CELLS];                      /* the jumps played (undo goes back to the start) */
    int n;
} psgame;

void ps_new(psgame *g, int variant);
int  ps_jump(const psgame *g, int from, int to, psmove *m);   /* 1: a legal jump from -> to (m: it, may be NULL) */
int  ps_moves(const psgame *g, psmove *out);    /* every legal jump; how many */
int  ps_moves_from(const psgame *g, int from, psmove *out);
int  ps_play(psgame *g, int from, int to);      /* 0: not a legal jump */
int  ps_undo(psgame *g);                        /* 0: nothing to undo */
int  ps_status(const psgame *g);                /* PS_* */
int  ps_gold(const psgame *g);                  /* 1: one marble left, in the goal hole */
int  ps_last(const psgame *g);                  /* the one marble left (any hole); -1: several */
int  ps_cell(const psgame *g, const char *s);   /* "d4": its cell; -1: not a hole of this board */
void ps_name(const psgame *g, int cell, char out[3]);
int  ps_script(psgame *g, const char *moves);   /* "d6d4 b5d5": 0 at the first bad one */

#endif
