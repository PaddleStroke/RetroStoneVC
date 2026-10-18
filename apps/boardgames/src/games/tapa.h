/*
 * Tapa (a wall round the clues) for one player: the rules. Shade cells of a square grid into one wall: all the shaded
 * cells joined side by side, no 2 x 2 square all shaded. A clue cell is never shaded; its numbers give the lengths of
 * the runs of shaded cells among its eight neighbours (in any order; runs apart from each other round the clue).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 6 x 6, 8 x 8, 10 x 10, each easy, medium or hard (variant = size * 3 + level). A puzzle is made from a
 * seed: a wall grown by chance from one cell (never a 2 x 2), every other cell a clue, then clues taken away while the
 * solver (bench.h) still finishes it at its level. The solver only proves, so a puzzle it finishes has that one
 * solution. The ways of reasoning, by level:
 *   easy: each clue's rings of neighbours that fit it and what is known (cells shaded in all of them shaded, in none
 *     unshaded); three shaded cells of a 2 x 2 leave the fourth; cells cut off from the wall unshaded, a cell the wall
 *     cannot do without shaded; and a cell tried each way, those ways followed once, to a contradiction;
 *   medium: chains of three steps; hard: chains of up to eight.
 * The player's cells: empty, shaded, or a dot (a note: unshaded). Solved when the shading keeps every rule.
 * Cells: x + y * n from the top left. The notation (script, the screenshots, the tests): a cell is its column letter
 * a.. and its row 1.. from the top; "c4" a tap (shaded or not), ".c4" a press held (a dot in or out), "c4-c9" a drag
 * along a line (the cells as c4 was become as a tap there made it), ".c4-c9" the same held, "h" a hint, "+n" the next
 * n cells (reading order) not yet as the solution has them put right.
 */
#ifndef TAPA_H
#define TAPA_H

#include <stdint.h>

#include "games/bench.h"

#define TPA_SIZES 3
#define TPA_VARIANTS (TPA_SIZES * 3)
#define TPA_MAX 10
#define TPA_CELLS (TPA_MAX * TPA_MAX)
#define TPA_LEVELS 150                          /* the designed levels (tapa_levels.h), in six chapters */
#define TPA_CHAPTERS 6

enum { TPA_EMPTY, TPA_SHADE, TPA_DOT };        /* a cell as the player left it */

typedef struct {
    int n;
    int8_t count[TPA_CELLS];                    /* how many numbers a clue has (a clue 0: one); -1 not a clue */
    uint8_t num[TPA_CELLS][4];                  /* its numbers, from the biggest */
} tpa_puzzle;

typedef struct {
    tpa_puzzle p;
    int level;
    uint32_t seed;
    uint8_t sol[TPA_CELLS];                     /* 1: shaded in the solution */
    uint16_t st[TPA_CELLS];                     /* TPA_EMPTY, TPA_SHADE, TPA_DOT (a clue: TPA_EMPTY) */
    bench_hist hist;
} tpa_game;

int  tpa_size(int variant);                     /* 6, 8, 10 */
int  tpa_level(int variant);
/* the runs of shaded cells round the clue cell c of a shading sh (1: shaded): how many, their lengths in len (from
 * the biggest) */
int  tpa_runs(const uint8_t *sh, int n, int c, uint8_t *len);

int  tpa_grade(const tpa_puzzle *p, uint8_t *sol);   /* BENCH_* (the solution in sol when it solves it; may be NULL) */
int  tpa_count(const tpa_puzzle *p, int limit);
/* the work a puzzle asks of the solver: its grade, the passes and the cells found by trials */
int  tpa_effort(const tpa_puzzle *p, long *passes, long *trials);
void tpa_make(int n, int level, uint32_t seed, tpa_puzzle *p, uint8_t *sol);
/* the same with more time (budget: the solver's work it may spend; the levels' tool) */
void tpa_make_ex(int n, int level, uint32_t seed, long budget, tpa_puzzle *p, uint8_t *sol);
/* the levels (tapa_levels.h, made by tools/tapa_gen.c): "<n>:" then the cells row by row, "." a cell, a clue its
 * numbers (from the biggest) and "," */
int  tpa_encode(const tpa_puzzle *p, char *out, int size);   /* its length; 0: does not fit */
int  tpa_decode(const char *s, tpa_puzzle *p);  /* 1: a puzzle */

void tpa_new(tpa_game *g, int variant, uint32_t seed);
int  tpa_load(tpa_game *g, const tpa_puzzle *p);     /* a puzzle given (a level), graded and solved; 1: it solves */
int  tpa_free(const tpa_game *g, int c);        /* 1: a cell the player may change (not a clue) */
int  tpa_tap_state(const tpa_game *g, int c);
int  tpa_hold_state(const tpa_game *g, int c);
int  tpa_paint(tpa_game *g, const int *cells, int n, int state);
int  tpa_hint(tpa_game *g);
int  tpa_undo(tpa_game *g);
/* a clue against the player's shading: 1 broken (its runs cannot be its numbers any more), 2 kept (its runs there
 * and none of its neighbours left empty: each shaded or a dot), 0 else */
int  tpa_clue(const tpa_game *g, int c);
int  tpa_square(const tpa_game *g, int c);      /* a shaded cell of an all shaded 2 x 2 */
int  tpa_solved(const tpa_game *g);
int  tpa_script(tpa_game *g, const char *moves);

#endif
