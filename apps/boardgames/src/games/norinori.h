/*
 * Norinori (pairs in rooms) for one player: the rules. A square grid cut into outlined regions; shade cells so that
 * every region holds exactly two shaded cells, and every shaded cell touches exactly one other shaded cell side by
 * side: the shading is all dominoes, never touching one another side by side (a domino may lie across two regions).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 6 x 6, 8 x 8, 10 x 10, each easy, medium or hard (variant = size * 3 + level). A puzzle is made from a
 * seed: regions grown by chance, a shading found for them by a search with random choices, then cells passed from a
 * region to its neighbour (cells left unshaded only, so the shading keeps the rules) while the solver gets further,
 * until it finishes the puzzle at its level. The solver (bench.h) only proves, so a puzzle it finishes has that one
 * solution. The ways of reasoning, by level:
 *   easy: a shaded cell with its partner has nothing else shaded round it, one with a single way left takes it; a
 *     cell beside two shaded cells, or with no neighbour that could be shaded, stays unshaded; a region with its two
 *     keeps the rest unshaded, one with just two cells left shades them;
 *   medium: a short chain (a cell tried shaded or not, the easy ways followed twice, to a contradiction);
 *   hard: longer chains (the easy ways followed to the end).
 * The player's cells: empty, shaded, or a dot (a note: unshaded). Solved when the shading keeps every rule.
 * Cells: x + y * n from the top left. The notation (script, the screenshots, the tests): a cell is its column letter
 * a.. and its row 1.. from the top; "c4" a tap (shaded or not), ".c4" a press held (a dot in or out), "c4-c9" a drag
 * along a line (the cells as c4 was become as a tap there made it), ".c4-c9" the same held, "h" a hint, "+n" the next
 * n cells (reading order) not yet as the solution has them put right.
 */
#ifndef NORINORI_H
#define NORINORI_H

#include <stdint.h>

#include "games/bench.h"

#define NRN_SIZES 3
#define NRN_VARIANTS (NRN_SIZES * 3)
#define NRN_MAX 12
#define NRN_CELLS (NRN_MAX * NRN_MAX)
#define NRN_LEVELS 150                          /* the designed levels (norinori_levels.h), in six chapters */
#define NRN_CHAPTERS 6

enum { NRN_EMPTY, NRN_SHADE, NRN_DOT };        /* a cell as the player left it */

typedef struct {
    int n, regions;
    uint8_t region[NRN_CELLS];                  /* each cell's region, 0.. in the order of their first cells */
} nrn_puzzle;

typedef struct {
    nrn_puzzle p;
    int level;
    uint32_t seed;
    uint8_t sol[NRN_CELLS];                     /* 1: shaded in the solution */
    uint16_t st[NRN_CELLS];                     /* NRN_EMPTY, NRN_SHADE, NRN_DOT */
    bench_hist hist;
} nrn_game;

int  nrn_size(int variant);                     /* 6, 8, 10 */
int  nrn_level(int variant);

int  nrn_grade(const nrn_puzzle *p, uint8_t *sol);   /* BENCH_* (the solution in sol when it solves it; may be NULL) */
int  nrn_count(const nrn_puzzle *p, int limit);
/* the work a puzzle asks of the solver: its grade, the passes and the finds of the medium way */
int  nrn_effort(const nrn_puzzle *p, long *passes, long *mediums);
void nrn_make(int n, int level, uint32_t seed, nrn_puzzle *p, uint8_t *sol);
/* the same with more time (budget: the solver's passes it may spend; the levels' tool) */
void nrn_make_ex(int n, int level, uint32_t seed, long budget, nrn_puzzle *p, uint8_t *sol);
/* the levels (norinori_levels.h, made by tools/norinori_gen.c): "<n>:" then a character a cell, row by row, its
 * region (a-z, A-Z, 0-9) */
int  nrn_encode(const nrn_puzzle *p, char *out, int size);   /* its length; 0: does not fit */
int  nrn_decode(const char *s, nrn_puzzle *p);  /* 1: a puzzle (every region in one piece) */

void nrn_new(nrn_game *g, int variant, uint32_t seed);
int  nrn_load(nrn_game *g, const nrn_puzzle *p);     /* a puzzle given (a level), graded and solved; 1: it solves */
int  nrn_free(const nrn_game *g, int c);        /* 1: a cell of the grid */
int  nrn_tap_state(const nrn_game *g, int c);
int  nrn_hold_state(const nrn_game *g, int c);
int  nrn_paint(nrn_game *g, const int *cells, int n, int state);
int  nrn_hint(nrn_game *g);
int  nrn_undo(nrn_game *g);
int  nrn_partner(const nrn_game *g, int c);     /* a shaded cell's one shaded neighbour; -1: none or more than one */
int  nrn_crowded(const nrn_game *g, int c);     /* a shaded cell beside two or more, or one left with no way out */
int  nrn_region(const nrn_game *g, int r);      /* its shaded cells: 1 too many, 2 just two, 0 else */
int  nrn_solved(const nrn_game *g);
int  nrn_script(nrn_game *g, const char *moves);

#endif
