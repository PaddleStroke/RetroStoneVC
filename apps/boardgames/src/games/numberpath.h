/*
 * Number Path for one player: the rules (no SDL). A grid of w x h cells; put the numbers 1 to w * h in it, one a cell,
 * so that each number touches the next: all around (side by side or corner to corner) in the first way, side by side
 * only in the second. Some numbers are given, 1 and the last always; each puzzle has one solution.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The maker: a path through every cell drawn by chance (a snake through the rows, then many "backbites": an end of
 * the path steps to a neighbour and the loop it closes is turned round), every number given, then the givens taken
 * away in a random order while the reasoning below still solves the puzzle (so it has one solution), and some put
 * back for the easier puzzles. The reasoning, as a person's:
 *   - each missing number lies between the nearest known numbers below and above it, a and b: in an empty cell no
 *     further (in steps through empty cells) than v - a from a's cell and b - v from b's; side by side only, a cell's
 *     colour (as a chessboard's) also follows the number's parity;
 *   - a number with one cell left goes there (easy); a cell with one number left takes it (medium);
 *   - every step, all the numbers and cells found at once.
 * The search counts the solutions: the reasoning, then the number with the fewest cells left tried in each of them
 * (the reasoning again each time); the tests also check the smaller levels with a plain search of every path.
 * The player's numbers: a run grows from a number (given or placed) to an empty cell touching it with the next
 * number (or the one before, going down from a number whose next is there); see nph_head. The stars: the hints and
 * the mistakes (bench.h): a wrong number taken back with undo.
 * Cells: x + y * w from the top left.
 */
#ifndef NUMBERPATH_H
#define NUMBERPATH_H

#include <stdint.h>

#include "games/bench.h"

#define NPH_MAXW 9
#define NPH_CELLS (NPH_MAXW * NPH_MAXW)
#define NPH_LEVELS 300                          /* 150 a way, in six chapters of 50: the two ways in turn */
#define NPH_CHAPTERS 6
#define NPH_SIZES 3                             /* the random puzzles: 6 x 6, 7 x 7, 9 x 9 */
#define NPH_VARIANTS (2 * NPH_SIZES)            /* variant = way * NPH_SIZES + size */

enum { NPH_ALL, NPH_SIDE };                     /* the two ways numbers touch */

typedef struct {
    int w, h, way;
    uint8_t given[NPH_CELLS];                   /* a given number, 0 none */
} nph_puzzle;

typedef struct {
    nph_puzzle p;
    uint8_t sol[NPH_CELLS];
    uint16_t val[NPH_CELLS];                    /* the numbers in the cells: the givens and the player's (0 empty) */
    bench_hist hist;
} nph_game;

extern const char *const nph_levels[NPH_LEVELS];

int  nph_cells(const nph_puzzle *p);
int  nph_touch(const nph_puzzle *p, int a, int b);   /* 1: cells a and b touch the way the puzzle says */
void nph_variant(int variant, int *w, int *h, int *way);

/* a path through every cell (sol: each cell's number 1..) drawn from the seed */
void nph_path(int w, int h, int way, uint32_t seed, uint8_t *sol);
/* the reasoning from the numbers in val (0 empty): 1 solved (val filled), 0 stuck, -1 a contradiction. *steps: its
 * rounds; *mediums: the rounds that needed a cell with one number left (both may be NULL) */
int  nph_reason(const nph_puzzle *p, uint8_t *val, int *steps, int *mediums);
/* one finding of the reasoning from val: the cell (its number in *v); -1 none */
int  nph_next(const nph_puzzle *p, const uint8_t *val, int *v);
/* the solutions counted up to limit (the first in sol, may be NULL); budget: the search's nodes (0: no end; past it
 * the count so far, *nodes how many it took, may be NULL) */
int  nph_count(const nph_puzzle *p, int limit, uint8_t *sol, long budget, long *nodes);
/* a puzzle from a path: the givens taken away while the reasoning solves it, then about `keep` of the cells given
 * (0..1: the part; 0: as few as the reasoning allows) */
void nph_make(int w, int h, int way, float keep, uint32_t seed, nph_puzzle *p, uint8_t *sol);

/* the levels: "<w><h><a|s>:<the first cell, two digits>:<a step a number, 0..7>:<the givens, hex: bit i number i+1>" */
int  nph_encode(const nph_puzzle *p, const uint8_t *sol, char *out, int size);   /* its length; 0: no room */
int  nph_decode(const char *s, nph_puzzle *p, uint8_t *sol);                    /* 1: a level */

void nph_new(nph_game *g, int variant, uint32_t seed);
int  nph_load(nph_game *g, const nph_puzzle *p, const uint8_t *sol);
int  nph_level(nph_game *g, int level);         /* 0..NPH_LEVELS-1 */
int  nph_given(const nph_game *g, int c);
int  nph_where(const nph_game *g, int v);       /* the cell holding number v; -1 none */
/* the end of the run through cell c (numbers in a row touching), where the next number goes: its cell, *dir +1 (the
 * next above) or -1 (the next below); -1: none to place from there */
int  nph_head(const nph_game *g, int c, int *dir);
int  nph_wrongs(const nph_game *g);             /* the player's numbers the solution does not have */
int  nph_clash(const nph_game *g, int c);       /* 1: its number's next or last is on the board, not touching it */
int  nph_solved(const nph_game *g);
/* an action: a run of cells from a number (from; the cells in to[], n of them) or erased cells; 1: changed */
int  nph_place(nph_game *g, int from, const int *to, int n);   /* the numbers after from's head, along the cells */
int  nph_erase(nph_game *g, const int *cells, int n);
int  nph_undo(nph_game *g);                     /* the cell changed last; -1 nothing to undo */
/* the hint: a wrong number taken out, else the next finding put in; the cell, *put 1 when it put a number; -1 none */
int  nph_hint(nph_game *g, int *put);
/* "c4" a run from a number: "c4-d5-e5" the cells after it; "xc4" erased; "h" a hint; "+n" the next n numbers of the
 * solution put in (no hint counted) */
int  nph_script(nph_game *g, const char *s);

#endif
