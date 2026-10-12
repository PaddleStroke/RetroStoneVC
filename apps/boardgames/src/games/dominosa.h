/*
 * Dominosa for one player: the rules. A full set of dominoes, double-n (every pair of numbers 0..n once, doubles
 * too), was laid face up on a grid of n + 2 by n + 1 cells and then the dominoes' outlines were taken away: only the
 * numbers are left. Find where each domino lies, joining two neighbouring cells into one; every domino of the set
 * exactly once. Every puzzle has one solution.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: double-4, double-6 and double-8, each at three levels (4 easy, 4 medium, 4 hard, 6 easy...). A puzzle
 * is made from a seed: the grid tiled with dominoes by chance (the dominoes of a 2 x 2 square turned round, many
 * times), the set dealt onto them, each turned at random; then dominoes swapped or turned round, each change kept when
 * it brings the puzzle no further from its level (a climb, mostly round the cells the grader could not settle), until
 * the grader solves it at its level; the grader's deductions are sound, so a puzzle it solves has that one solution
 * (the tests count the solutions apart). The grader reasons as a person does (dsa_rate), each pair of neighbouring
 * cells a possible domino or not:
 *   0 easy: a cell with one neighbour left to pair with is that domino; a domino of the set with one place left is
 *     there (and then nothing else touches its cells, and it is nowhere else);
 *   1 medium: also a cell whose pairs left are all the same domino holds that domino, so it is nowhere else; a domino
 *     whose places left all share a cell is on that cell, so the cell pairs with nothing else; a pair that would
 *     leave a neighbouring cell with nothing to pair with is not a domino;
 *   2 hard: also a pair that, followed through with the easy ways, comes to a contradiction.
 * A puzzle needing more than these is not used; one is kept only when it needs its level's own ways.
 * The player's dominoes may be wrong; one that is laid twice can be shown. Solved when the dominoes cover the grid,
 * each of the set once.
 * Cells: x + y * w from the top left (w = n + 2 columns, n + 1 rows). The notation (script, the screenshots, the
 * tests): a cell is its column letter a.. and its row 1.. from the top; "c2-c3" lays a domino on c2 and c3 (the ones
 * on them go), "xc2" takes away the domino on c2, "h" a hint, "+n" lays the next n dominoes of the solution (reading
 * order).
 */
#ifndef DOMINOSA_H
#define DOMINOSA_H

#include <stdint.h>

#include "games/logicgrid2.h"

#define DSA_MAXN 8
#define DSA_MAXW (DSA_MAXN + 2)
#define DSA_MAXH (DSA_MAXN + 1)
#define DSA_CELLS (DSA_MAXW * DSA_MAXH)
#define DSA_TYPES ((DSA_MAXN + 1) * (DSA_MAXN + 2) / 2)
#define DSA_SIZES 3
#define DSA_LEVELS 3
#define DSA_VARIANTS (DSA_SIZES * DSA_LEVELS)

enum { DSA_EASY, DSA_MEDIUM, DSA_HARD, DSA_BEYOND };

static const int dsa_sets[DSA_SIZES] = {4, 6, 8};
static inline int dsa_set_of(int variant) { return dsa_sets[variant / DSA_LEVELS % DSA_SIZES]; }
static inline int dsa_level_of(int variant) { return variant % DSA_LEVELS; }
/* the domino a-b (either order) of a double-n set: 0..(n + 1)(n + 2) / 2 - 1 */
static inline int dsa_type(int n, int a, int b)
{
    if (a > b) { int t = a; a = b; b = t; }
    return a * (2 * n + 3 - a) / 2 + b - a;
}

typedef struct {
    int n, w, h;                                /* the set, double-n; the grid: w = n + 2 columns, h = n + 1 rows */
    uint8_t num[DSA_CELLS];                     /* each cell's number */
} dsa_puz;

typedef struct {
    int level;
    uint32_t seed;
    dsa_puz p;
    uint8_t sol[DSA_CELLS];                     /* the solution: each cell's other half */
    uint8_t link[DSA_CELLS];                    /* the player's: each cell's other half + 1 (0: none) */
    lg2_hist hist;
} dsa_game;

/* the solutions of a puzzle, counted up to limit; the first found in sol (each cell's other half; may be NULL) */
int  dsa_count(const dsa_puz *p, int limit, uint8_t *sol);
/* the grade of a puzzle (DSA_EASY..DSA_HARD; DSA_BEYOND: more than the grader's ways, or no single solution) */
int  dsa_rate(const dsa_puz *p);
/* a puzzle of the set (4, 6, 8) and level from the seed, and its solution */
void dsa_make(int n, int level, uint32_t seed, dsa_puz *p, uint8_t *sol);

void dsa_new(dsa_game *g, int variant, uint32_t seed);
void dsa_load(dsa_game *g, int level, const dsa_puz *p, const uint8_t *sol);
int  dsa_lay(dsa_game *g, int a, int b);        /* a domino on the neighbouring cells a and b (the ones on them go);
                                                 * 0: not neighbours, or nothing changed */
int  dsa_lift(dsa_game *g, int cell);           /* the domino on the cell taken away; 0: none */
int  dsa_hint(dsa_game *g);                     /* a wrong domino taken away, else the easiest one laid: a cell of it;
                                                 * -1: none */
int  dsa_undo(dsa_game *g);                     /* the last action taken back: a cell it changed; -1: none */
int  dsa_clash(const dsa_game *g, int cell);    /* 1: its domino is laid elsewhere too */
int  dsa_laid(const dsa_game *g, int type);     /* how many times the player laid that domino */
int  dsa_solved(const dsa_game *g);
int  dsa_script(dsa_game *g, const char *moves);   /* 0 at the first bad one */

#endif
