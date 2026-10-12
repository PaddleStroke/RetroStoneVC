/*
 * Peg Solitaire for one player: the rules. Marbles stand in some holes of a board; a marble jumps over a neighbour
 * into an empty hole just beyond it, in a straight line, and the marble jumped over is taken off. The aim: one marble
 * left, in the goal hole. No jump left with more than one marble: stuck (undo).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The boards, on a grid of 9 x 9 (column x from the left, row y from the top; a cell is y * 9 + x), each in its top
 * left corner:
 *   English: the cross of 33 holes (7 x 7);
 *   French (European): the cross and its four inner corners, 37 holes (7 x 7);
 *   Diamond: 41 holes, every hole at most 4 steps across and down from the middle (9 x 9);
 *   Wiegleb's: the wide cross of 45 holes, arms 3 holes wide (9 x 9);
 *   Triangle: 15 holes in 5 rows (row y holds x = 0..y), and the big triangle, 21 holes in 6 rows. The jumps go along
 *   its three lines: the steps (+-1, 0), (0, +-1), (+-1, +-1). On the others: across and up and down only.
 * A game: a board, its marbles, its goal hole, and a known way to one marble in the goal (the solution, sol: the
 * hint follows it while the game is on it, else searches). Three ways to start one:
 *   ps_new: the classic start (every hole full but one; English: the middle, and it is the goal; French: d6 empty, d2
 *   the goal, since no game from one empty hole ends in its middle; the triangle: its top, the goal too), for the
 *   pictures and the tests;
 *   ps_level: the levels (pegsolitaire_levels.h, made by tools/pegsolitaire_gen.c: positions found by playing back
 *   from one marble in the goal, the full classic boards solved by the search, six chapters of 25);
 *   ps_random: a random position of a board (the variants), played back from one marble in a goal drawn at random.
 * The stars: one marble left in the goal with no hint: 3; a star less for missing the goal, a star less for any hint
 * (at least 1). Every jump of a game is needed (a game always takes the marbles but one), so a par of jumps would
 * mean nothing; the goal hole is the genre's own measure.
 * The notation (script, the screenshots, the tests): a jump is its two holes, "d6d4"; a hole is its column letter a..
 * and its row counted from the bottom of the board (the crosses: 1..7, so the middle is d4; the triangle: 1..5, its
 * top a5).
 */
#ifndef PEGSOLITAIRE_H
#define PEGSOLITAIRE_H

#include <stdint.h>

#define PS_SIDE 9
#define PS_CELLS (PS_SIDE * PS_SIDE)
#define PS_MAXH 64                              /* a board's holes, at most (a position: a 64-bit mask of them) */
#define PS_MAX_MOVES 192                        /* the jumps from a position */
#define PS_LEVELS 150
#define PS_CHAPTERS 6

enum { PS_ENGLISH, PS_FRENCH, PS_TRIANGLE, PS_DIAMOND, PS_WIEGLEB, PS_TRIANGLE6, PS_BOARDS };
#define PS_VARIANTS PS_BOARDS                   /* the random play's boards */
enum { PS_PLAY, PS_ONE, PS_STUCK };             /* one marble left (solved); several and no jump (stuck) */

typedef struct { int8_t from, over, to; } psmove;

typedef struct {
    int board, tri;                             /* PS_*; tri: the triangle's six ways */
    int cols, rows;                             /* the board's size on the grid */
    int goal;                                   /* the goal hole */
    uint8_t hole[PS_CELLS];                     /* 1: a hole of this board */
    uint8_t peg[PS_CELLS];                      /* 1: a marble in it */
    uint8_t start[PS_CELLS];                    /* the marbles at the start */
    int holes, pegs;
    psmove hist[PS_MAXH];                       /* the jumps played (undo goes back to the start) */
    int n;
    psmove sol[PS_MAXH];                        /* a way from the start to one marble in the goal */
    int nsol;
    int hints;
} psgame;

typedef struct { uint8_t board, goal; uint64_t pegs; const char *sol; } ps_level_t;
extern const ps_level_t ps_levels[PS_LEVELS];

void ps_board(psgame *g, int board);            /* the board's holes, no marble, no goal (the tools) */
void ps_new(psgame *g, int board);              /* the classic start */
/* a start: the board, its goal, the marbles (bit i: the board's hole i, in cell order), the solution's jumps
 * ("d6d4 b5d5", NULL: none known); 0 when it is not one (the solution must end in the goal) */
int  ps_set(psgame *g, int board, int goal, uint64_t pegs, const char *sol);
int  ps_level(psgame *g, int level);            /* level 0..PS_LEVELS-1 */
/* a random start on that board: one marble in a goal drawn from the seed, then `back` jumps played backwards
 * (0: as many as suits the board) */
void ps_random(psgame *g, int board, uint32_t seed, int back);
uint64_t ps_mask(const psgame *g);              /* the marbles as ps_set's bits */
int  ps_index(const psgame *g, int cell);       /* a hole's bit; -1: not a hole */
int  ps_jump(const psgame *g, int from, int to, psmove *m);   /* 1: a legal jump from -> to (m: it, may be NULL) */
int  ps_moves(const psgame *g, psmove *out);    /* every legal jump; how many */
int  ps_moves_from(const psgame *g, int from, psmove *out);
int  ps_play(psgame *g, int from, int to);      /* 0: not a legal jump */
int  ps_undo(psgame *g);                        /* 0: nothing to undo */
int  ps_status(const psgame *g);                /* PS_* */
int  ps_gold(const psgame *g);                  /* 1: one marble left, in the goal hole */
int  ps_last(const psgame *g);                  /* the one marble left (any hole); -1: several */
/* the search from here to one marble (in `target`, -1: anywhere), within `budget` positions: 1 found (its jumps
 * into path, may be NULL), 0 none, -1 the budget ran out */
int  ps_solve(const psgame *g, int target, psmove *path, long budget);
void ps_solver_size(int bits);                  /* the search's table of dead ends: 2^bits positions (18 at first) */
/* the hint: a jump into *m; 1 it leads to one marble in the goal, 2 to one marble elsewhere only; 0 no way left to
 * one marble (undo); -1 the search ran out of time */
int  ps_hint(psgame *g, psmove *m);
int  ps_stars(const psgame *g);                 /* one marble left: 1..3 */
int  ps_cell(const psgame *g, const char *s);   /* "d4": its cell; -1: not a hole of this board */
void ps_name(const psgame *g, int cell, char out[3]);
int  ps_script(psgame *g, const char *moves);   /* "d6d4 b5d5": 0 at the first bad one */

#endif
