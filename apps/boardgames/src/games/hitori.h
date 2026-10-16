/*
 * Hitori for one player: the rules. A square grid of numbers; shade some cells so that no number is left twice in a
 * row or a column. Shaded cells never touch side by side, and the cells left unshaded are all joined in one piece
 * (side by side). Only a number that repeats in its row or its column may be shaded.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 6 x 6, 8 x 8, 10 x 10, each easy, medium or hard (variant = size * 3 + level). A puzzle is made from a
 * seed: a Latin square by chance (each number once a row and a column), shaded cells put by chance (never touching,
 * the rest kept joined), each given the number of another cell of its row or column; then changed a little at a time
 * until the solver (bench.h) solves it at its level, and by no harder way. The ways of reasoning, by level:
 *   easy: a number alone in its row and its column stays; round a shaded cell nothing is shaded; a number kept rules
 *     out its twins in its row and column; between two twins (A B A) the middle stays; of two twins side by side one
 *     is shaded, so the line's other twins are; a kept cell with one way left to the others keeps it;
 *   medium: a cell the kept ones cannot do without (shaded, it would cut them apart) stays;
 *   hard: chains (shaded or kept tried in a cell, the easy ways followed to a contradiction).
 * The player's cells: empty, shaded, or circled (a note: kept). Solved when the shading keeps every rule (the one way
 * it can).
 * Cells: x + y * n from the top left. The notation (script, the screenshots, the tests): a cell is its column letter
 * a.. and its row 1.. from the top; "c4" a tap (shaded or not), ".c4" a press held (a circle in or out), "c4-c9" a
 * drag along a line (the cells as c4 was become as a tap there made it), ".c4-c9" the same held, "h" a hint, "+n" the
 * next n cells (reading order) not yet as the solution has them put right.
 */
#ifndef HITORI_H
#define HITORI_H

#include <stdint.h>

#include "games/bench.h"

#define HTO_SIZES 3
#define HTO_VARIANTS (HTO_SIZES * 3)
#define HTO_MAX 12
#define HTO_CELLS (HTO_MAX * HTO_MAX)
#define HTO_LEVELS 150                          /* the designed levels (hitori_levels.h), in six chapters */
#define HTO_CHAPTERS 6

enum { HTO_EMPTY, HTO_SHADE, HTO_RING };       /* a cell as the player left it */

typedef struct {
    int n;
    uint8_t num[HTO_CELLS];                     /* 1..n */
} hto_puzzle;

typedef struct {
    hto_puzzle p;
    int level;
    uint32_t seed;
    uint8_t sol[HTO_CELLS];                     /* 1: shaded in the solution */
    uint16_t st[HTO_CELLS];                     /* HTO_EMPTY, HTO_SHADE, HTO_RING */
    bench_hist hist;
} hto_game;

int  hto_size(int variant);                     /* 6, 8, 10 */
int  hto_level(int variant);

int  hto_grade(const hto_puzzle *p, uint8_t *sol);   /* BENCH_* (the solution in sol when it solves it; may be NULL) */
int  hto_count(const hto_puzzle *p, int limit);
void hto_make(int n, int level, uint32_t seed, hto_puzzle *p, uint8_t *sol);
/* the same with more time (budget: the solver's passes it may spend; the levels' tool) */
void hto_make_ex(int n, int level, uint32_t seed, long budget, hto_puzzle *p, uint8_t *sol);
/* the levels (hitori_levels.h, made by tools/hitori_gen.c): a puzzle as text, "<n>:" then a digit a cell, row by row
 * ('1'..'9', then 'a' for 10) */
int  hto_encode(const hto_puzzle *p, char *out, int size);   /* its length; 0: no room */
int  hto_decode(const char *s, hto_puzzle *p);  /* 1: a puzzle */
/* the work a puzzle asks of the solver: its grade (BENCH_*), the passes and the finds of the medium way */
int  hto_effort(const hto_puzzle *p, long *passes, long *mediums);

void hto_new(hto_game *g, int variant, uint32_t seed);
int  hto_load(hto_game *g, const hto_puzzle *p);     /* a puzzle given (a level), graded and solved; 1: it solves */
int  hto_free(const hto_game *g, int c);        /* 1: a cell of the grid */
int  hto_tap_state(const hto_game *g, int c);
int  hto_hold_state(const hto_game *g, int c);
int  hto_paint(hto_game *g, const int *cells, int n, int state);
int  hto_hint(hto_game *g);
int  hto_undo(hto_game *g);
int  hto_touching(const hto_game *g, int c);    /* a shaded cell beside another */
int  hto_twin(const hto_game *g, int c);        /* a circled cell whose number is circled again in its row or column */
int  hto_apart(const hto_game *g, int c);       /* an unshaded cell cut off from the others by the shading */
int  hto_solved(const hto_game *g);
int  hto_script(hto_game *g, const char *moves);

#endif
