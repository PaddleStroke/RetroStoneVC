/*
 * Calcudoku for one player: the rules. A square of n x n cells (4, 5, 6, 7 or 9) to fill with the digits 1..n so that
 * every row and every column holds each digit once (a Latin square). The grid is cut into cages, each with a target
 * and an operation: its digits added (+), multiplied (x), or, in a cage of two, the larger less the smaller (-) or
 * the larger divided by the smaller (/), give the target. A cage of one cell holds its target. Every puzzle has one
 * solution.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: the five sizes, each at three levels (4 easy, 4 medium, 4 hard, 5 easy...). A puzzle is made from a
 * seed: a Latin square filled by chance, cut into cages (the harder, the larger), an operation chosen for each; then
 * graded, and made again until it is of its level. The level is the reasoning the solver needs (bench.h):
 *   0 easy: what each cage allows on its own (the digits of every way of making its target) and the singles (a cell
 *     with one digit left; a digit with one place left in a row or column);
 *   1 medium: also a cage's digits that must lie in one row or column (taken from the rest of that line), and naked
 *     and hidden pairs and triples in a line;
 *   2 hard: also trials (a digit that leads, by the easy ways, to a contradiction).
 * The player's digits may be wrong; the clashes (a digit twice in a line, a full cage missing its target) can be
 * shown. Cells: 0..n*n-1, row by row from the top left. The notation (script, the screenshots, the tests): a cell is
 * its column letter a.. and its row 1.. from the top; "c2=4" puts 4 in c2, "c2=0" empties it, "c2'4" turns the note 4
 * on or off, "h" a hint, "+n" fills the next n empty cells (reading order) with their digits.
 */
#ifndef CALCUDOKU_H
#define CALCUDOKU_H

#include <stdint.h>

#include "games/bench.h"

#define CDK_MAX 9
#define CDK_CELLS (CDK_MAX * CDK_MAX)
#define CDK_CAGE 4                              /* the most cells in a cage */
#define CDK_SIZES 5
#define CDK_LEVELS 3
#define CDK_VARIANTS (CDK_SIZES * CDK_LEVELS)

enum { CDK_EASY, CDK_MEDIUM, CDK_HARD };
enum { CDK_ONE, CDK_ADD, CDK_SUB, CDK_MUL, CDK_DIV };   /* a cage's operation (ONE: a single cell) */

static const int CDK_SIZE[CDK_SIZES] = {4, 5, 6, 7, 9};
static inline int cdk_size_of(int variant) { return CDK_SIZE[variant / CDK_LEVELS % CDK_SIZES]; }
static inline int cdk_level_of(int variant) { return variant % CDK_LEVELS; }

typedef struct {
    uint8_t op, n;
    uint8_t cell[CDK_CAGE];                     /* in reading order: cell[0] carries the label */
    int target;
} cdk_cage;

typedef struct {
    int n, ncages;
    uint8_t cage_of[CDK_CELLS];
    cdk_cage cage[CDK_CELLS];
    uint8_t sol[CDK_CELLS];
} cdk_puzzle;

typedef struct {
    int variant, level;
    uint32_t seed;
    cdk_puzzle p;
    uint8_t val[CDK_CELLS];                     /* the grid now (0: empty) */
    uint16_t notes[CDK_CELLS];                  /* bit d: the note d */
    bench_hist hist;
} cdk_game;

/* the solutions, counted up to limit (a plain search: the tests' check) */
int  cdk_count(const cdk_puzzle *p, int limit);
/* the level the solver needs (CDK_EASY..CDK_HARD), BENCH_GUESS when it is stuck, BENCH_NONE: no solution */
int  cdk_rate(const cdk_puzzle *p);
/* a puzzle n x n of the level from the seed */
void cdk_make(int n, int level, uint32_t seed, cdk_puzzle *p);
int  cdk_value(int op, const int *v, int k);    /* the result of the operation on k digits (-1: none) */

void cdk_new(cdk_game *g, int variant, uint32_t seed);
int  cdk_put(cdk_game *g, int cell, int digit); /* a digit (0: empty); its notes in the row and column go; 0: no */
int  cdk_note(cdk_game *g, int cell, int digit);
int  cdk_erase(cdk_game *g, int cell);          /* the digit, else the notes; 0: nothing */
int  cdk_hint(cdk_game *g);                     /* a wrong digit put right, else a cell found by reasoning: the cell */
int  cdk_undo(cdk_game *g);                     /* a cell it changed; -1: none */
int  cdk_clash(const cdk_game *g, int cell);    /* its digit twice in a line, or its full cage missing the target */
int  cdk_solved(const cdk_game *g);
int  cdk_count_digit(const cdk_game *g, int digit);
int  cdk_script(cdk_game *g, const char *moves);

#endif
