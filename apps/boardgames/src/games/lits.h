/*
 * LITS (four-cell pieces in rooms) for one player: the rules. A square grid cut into outlined regions; shade four
 * cells joined side by side in every region (a piece of four: L, I, T or S, turned or mirrored). All the shaded cells
 * are joined in one piece, side by side; no 2 x 2 square is all shaded; and two pieces of the same shape never touch
 * side by side across a wall.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 6 x 6, 8 x 8, 10 x 10, each easy, medium or hard (variant = size * 3 + level). A puzzle is made from a
 * seed: pieces laid by chance, each touching those before (never two of a shape side by side, never a 2 x 2), the
 * regions grown round them over the cells left, then cells left unshaded passed from a region to its neighbour while
 * the solver gets further, until it finishes the puzzle at its level. The solver (bench.h) only proves, so a puzzle it
 * finishes has that one solution. The ways of reasoning, by level:
 *   easy: each region's pieces that still fit (the shaded cells in them, the unshaded ones out, no 2 x 2, no piece
 *     of the shape of a settled neighbour beside it): cells in all of them shaded, in none unshaded; three shaded
 *     cells of a 2 x 2 leave the fourth; cells cut off from the shading stay unshaded, a cell the shading cannot do
 *     without is shaded; and a cell tried each way, those ways followed once, to a contradiction;
 *   medium: chains of three steps; hard: chains of up to eight.
 * The player's cells: empty, shaded, or a dot (a note: unshaded). Solved when the shading keeps every rule.
 * Cells: x + y * n from the top left. The notation (script, the screenshots, the tests): a cell is its column letter
 * a.. and its row 1.. from the top; "c4" a tap (shaded or not), ".c4" a press held (a dot in or out), "c4-c9" a drag
 * along a line (the cells as c4 was become as a tap there made it), ".c4-c9" the same held, "h" a hint, "+n" the next
 * n cells (reading order) not yet as the solution has them put right.
 */
#ifndef LITS_H
#define LITS_H

#include <stdint.h>

#include "games/bench.h"

#define LTS_SIZES 3
#define LTS_VARIANTS (LTS_SIZES * 3)
#define LTS_MAX 12
#define LTS_CELLS (LTS_MAX * LTS_MAX)
#define LTS_LEVELS 150                          /* the designed levels (lits_levels.h), in six chapters */
#define LTS_CHAPTERS 6

enum { LTS_EMPTY, LTS_SHADE, LTS_DOT };        /* a cell as the player left it */
enum { LTS_L, LTS_I, LTS_T, LTS_S, LTS_SHAPES }; /* the pieces' shapes */

typedef struct {
    int n, regions;
    uint8_t region[LTS_CELLS];                  /* each cell's region, 0.. in the order of their first cells */
} lts_puzzle;

typedef struct {
    lts_puzzle p;
    int level;
    uint32_t seed;
    uint8_t sol[LTS_CELLS];                     /* 1: shaded in the solution */
    uint16_t st[LTS_CELLS];                     /* LTS_EMPTY, LTS_SHADE, LTS_DOT */
    bench_hist hist;
} lts_game;

int  lts_size(int variant);                     /* 6, 8, 10 */
int  lts_level(int variant);
/* the shape of four cells (LTS_L .. LTS_S); -1: not a piece of four joined side by side (or the square) */
int  lts_shape(const int *cells, int n);

int  lts_grade(const lts_puzzle *p, uint8_t *sol);   /* BENCH_* (the solution in sol when it solves it; may be NULL) */
int  lts_count(const lts_puzzle *p, int limit);
/* the work a puzzle asks of the solver: its grade, the passes and the finds of the trials */
int  lts_effort(const lts_puzzle *p, long *passes, long *trials);
void lts_make(int n, int level, uint32_t seed, lts_puzzle *p, uint8_t *sol);
/* the same with more time (budget: the solver's passes it may spend; the levels' tool) */
void lts_make_ex(int n, int level, uint32_t seed, long budget, lts_puzzle *p, uint8_t *sol);
/* the levels (lits_levels.h, made by tools/lits_gen.c): "<n>:" then a character a cell, row by row, its region (a-z,
 * A-Z, 0-9) */
int  lts_encode(const lts_puzzle *p, char *out, int size);   /* its length; 0: does not fit */
int  lts_decode(const char *s, lts_puzzle *p);  /* 1: a puzzle (every region in one piece, of four cells or more) */

void lts_new(lts_game *g, int variant, uint32_t seed);
int  lts_load(lts_game *g, const lts_puzzle *p);     /* a puzzle given (a level), graded and solved; 1: it solves */
int  lts_free(const lts_game *g, int c);        /* 1: a cell of the grid */
int  lts_tap_state(const lts_game *g, int c);
int  lts_hold_state(const lts_game *g, int c);
int  lts_paint(lts_game *g, const int *cells, int n, int state);
int  lts_hint(lts_game *g);
int  lts_undo(lts_game *g);
int  lts_piece(const lts_game *g, int r);       /* the shape of a region's four shaded cells; -1: not a piece */
int  lts_square(const lts_game *g, int c);      /* a shaded cell of an all shaded 2 x 2 */
int  lts_over(const lts_game *g, int r);        /* a region with more than four shaded cells */
int  lts_twins(const lts_game *g, int r);       /* a region's piece beside a piece of its shape across a wall */
int  lts_solved(const lts_game *g);
int  lts_script(lts_game *g, const char *moves);

#endif
