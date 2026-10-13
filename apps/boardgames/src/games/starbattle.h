/*
 * Star Battle for one player: the rules. A square grid cut into outlined regions, as many regions as rows. Place
 * stars so that every row, every column and every region holds exactly k of them (one star on a grid of 6 or 8, two
 * on a grid of 10), and no two stars touch, not even at a corner. Every puzzle has one solution.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 6 x 6 with one star, 8 x 8 with one, 10 x 10 with two, each at three levels (6 easy, 6 medium, 6 hard,
 * 8 easy...). A puzzle is made from a seed: the stars placed by chance (k a row and a column, none touching), the
 * regions grown round them (k stars each; two stars joined by a path first), then cells passed from a region to its
 * neighbour, each change kept when it brings the puzzle no further from its level (a climb, mostly round the cells the
 * grader could not settle), until the grader solves it at its level; the grader's deductions are sound, so a puzzle
 * it solves has that one solution (the tests count the solutions apart). The grader reasons as a person does, each
 * cell a star, empty, or not known yet, its ways in four tiers:
 *   tier 0: the cells round a star are empty; a line or region with its k stars has its other cells empty; one with
 *     just as many cells left as stars missing has stars in all of them;
 *   tier 1: a cell whose star would leave some line or region too few cells is empty; a line or region whose cells
 *     left all lie in another one, both missing as many stars, empties the other one's remaining cells;
 *   tier 2: the same with two lines or regions packed in two others;
 *   tier 3: the same with three in three; a star that, followed through with the tier 0 ways, comes to a
 *     contradiction (tried in a line or region with two or three cells left).
 * One star: easy needs tier 0 alone, medium tier 1, hard tier 2 or 3. Two stars (no puzzle of two stars a line is
 * solved by tier 0 alone): easy needs tier 1, medium tier 2, hard tier 3 (sbx_rate gives the level). A puzzle
 * needing more than these is not used; one is kept only when it needs its level's own ways.
 * The player marks each cell: a star, a dot (sure to be empty: a note), or nothing. Stars touching, or too many in a
 * line or region, can be shown. Solved when the stars are the solution's (the dots do not matter).
 * Cells: x + y * n from the top left. The notation (script, the screenshots, the tests): a cell is its column letter
 * a.. and its row 1.. from the top; "*c4" a star there, ".c4" a dot, "-c4" nothing, ".c4-f4" dots along a row or a
 * column (one action: the drag), "h" a hint, "+n" the next n stars of the solution (reading order).
 */
#ifndef STARBATTLE_H
#define STARBATTLE_H

#include <stdint.h>

#include "games/bench.h"

#define SBX_MAX 10
#define SBX_CELLS (SBX_MAX * SBX_MAX)
#define SBX_SIZES 3
#define SBX_LEVELS 3
#define SBX_VARIANTS (SBX_SIZES * SBX_LEVELS)
#define SBX_STAGES 150                          /* the designed levels (starbattle_levels.h), in six chapters */
#define SBX_CHAPTERS 6

enum { SBX_EASY, SBX_MEDIUM, SBX_HARD, SBX_BEYOND };
enum { SBX_NONE, SBX_STAR, SBX_DOT };           /* a cell as the player marked it */

static const int sbx_sizes[SBX_SIZES] = {6, 8, 10}, sbx_stars[SBX_SIZES] = {1, 1, 2};
static inline int sbx_size_of(int variant) { return sbx_sizes[variant / SBX_LEVELS % SBX_SIZES]; }
static inline int sbx_stars_of(int variant) { return sbx_stars[variant / SBX_LEVELS % SBX_SIZES]; }
static inline int sbx_level_of(int variant) { return variant % SBX_LEVELS; }

typedef struct {
    int n, k;                                   /* the side, the stars a row (column, region) */
    uint8_t region[SBX_CELLS];                  /* each cell's region, 0..n-1 */
} sbx_puz;

typedef struct {
    int level;
    uint32_t seed;
    sbx_puz p;
    uint8_t sol[SBX_CELLS];                     /* 1: a star in the solution */
    uint8_t mark[SBX_CELLS];                    /* the player's: SBX_NONE, SBX_STAR, SBX_DOT */
    bench_hist hist;
} sbx_game;

/* the solutions of a puzzle, counted up to limit; the first found in sol (1: a star; may be NULL) */
int  sbx_count(const sbx_puz *p, int limit, uint8_t *sol);
/* the grade of a puzzle (SBX_EASY..SBX_HARD; SBX_BEYOND: more than the grader's ways, or no single solution) */
int  sbx_rate(const sbx_puz *p);
/* the same, and the grader's steps weighed by their tier (1, 3, 10, 30): its effort */
int  sbx_effort(const sbx_puz *p, int *steps);
/* the levels' code (starbattle_levels.h): "<n>/<k>:" and a letter a cell, row by row, its region ('a' the first);
 * 0: does not fit, or not a puzzle */
int  sbx_encode(const sbx_puz *p, char *out, int size);
int  sbx_decode(const char *code, sbx_puz *p);
/* a puzzle of the size (4..10), stars (1, 2) and level from the seed, and its solution */
void sbx_make(int n, int k, int level, uint32_t seed, sbx_puz *p, uint8_t *sol);

void sbx_new(sbx_game *g, int variant, uint32_t seed);
void sbx_load(sbx_game *g, int level, const sbx_puz *p, const uint8_t *sol);
int  sbx_open(sbx_game *g, const char *code); /* a level from its code (its solution found): 0 not one */
int  sbx_set(sbx_game *g, int cell, int mark);  /* one cell marked; 0: nothing changed */
int  sbx_tap(sbx_game *g, int cell);            /* nothing -> star -> dot -> nothing; 0: not a cell */
/* the cells from a to b (one row or column) that are as a is, set to `to` (dots along a run, or taken away): one
 * action; 0: nothing changed */
int  sbx_run(sbx_game *g, int a, int b, int to);
int  sbx_hint(sbx_game *g);                     /* a wrong star or dot taken away, else the easiest star placed: the
                                                 * cell; -1: none */
int  sbx_undo(sbx_game *g);                     /* the last action taken back: a cell it changed; -1: none */
int  sbx_clash(const sbx_game *g, int cell);    /* 1: its star touches another, or its row, column or region holds
                                                 * too many */
int  sbx_unit_full(const sbx_game *g, int way, int i);   /* way 0 row, 1 column, 2 region: 1 when it holds its k */
int  sbx_solved(const sbx_game *g);
int  sbx_script(sbx_game *g, const char *moves);   /* 0 at the first bad one */

#endif
