/*
 * Yin-Yang (ivory and ebony stones) for one player: the rules. Put a stone, ivory or ebony, in every cell of a square
 * grid: the ivory stones all joined in one group side by side, the ebony ones too, and no 2 x 2 square of one colour.
 * Some stones are given.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 6 x 6, 8 x 8, 10 x 10, each easy, medium or hard (variant = size * 3 + level). A puzzle is made from a
 * seed: an ebony group grown by chance (never a 2 x 2 of one colour, the ivory kept in one group), every stone given,
 * then stones taken away while the solver (bench.h) still finishes it at its level. The solver only proves, so a
 * puzzle it finishes has that one solution. The ways of reasoning, by level:
 *   easy: three stones of a colour in a 2 x 2 leave the fourth the other; no 2 x 2 crosswise (both colours could
 *     not then be joined); cells cut off from a colour's stones the other colour, a cell a colour cannot do without
 *     that colour; round the rim each colour lies in one stretch; and a cell tried each way, those ways followed
 *     once, to a contradiction;
 *   medium: chains of three steps; hard: chains of up to eight.
 * The player's cells: empty, ivory or ebony. Solved when every cell holds the solution's stone.
 * Cells: x + y * n from the top left. The notation (script, the screenshots, the tests): a cell is its column letter
 * a.. and its row 1.. from the top; "c4" a tap (empty, ebony, ivory, empty...), ".c4" a press held (ivory in or out),
 * "c4-c9" a drag along a line (the cells as c4 was become as a tap there made it), ".c4-c9" the same held, "h" a hint,
 * "+n" the next n cells (reading order) not yet as the solution has them put right.
 */
#ifndef YINYANG_H
#define YINYANG_H

#include <stdint.h>

#include "games/bench.h"

#define YYG_SIZES 3
#define YYG_VARIANTS (YYG_SIZES * 3)
#define YYG_MAX 10
#define YYG_CELLS (YYG_MAX * YYG_MAX)
#define YYG_LEVELS 150                          /* the designed levels (yinyang_levels.h), in six chapters */
#define YYG_CHAPTERS 6

enum { YYG_EMPTY, YYG_IVORY, YYG_EBONY };      /* a cell as the player left it (and a given stone) */

typedef struct {
    int n;
    uint8_t given[YYG_CELLS];                   /* YYG_EMPTY (none), YYG_IVORY, YYG_EBONY */
} yyg_puzzle;

typedef struct {
    yyg_puzzle p;
    int level;
    uint32_t seed;
    uint8_t sol[YYG_CELLS];                     /* YYG_IVORY, YYG_EBONY */
    uint16_t st[YYG_CELLS];                     /* YYG_EMPTY, YYG_IVORY, YYG_EBONY (a given: its stone) */
    bench_hist hist;
} yyg_game;

int  yyg_size(int variant);                     /* 6, 8, 10 */
int  yyg_level(int variant);

int  yyg_grade(const yyg_puzzle *p, uint8_t *sol);   /* BENCH_* (the solution in sol when it solves it; may be NULL) */
int  yyg_count(const yyg_puzzle *p, int limit);
/* the work a puzzle asks of the solver: its grade, the passes and the cells found by trials */
int  yyg_effort(const yyg_puzzle *p, long *passes, long *trials);
void yyg_make(int n, int level, uint32_t seed, yyg_puzzle *p, uint8_t *sol);
/* the same with more time (budget: the solver's work it may spend; the levels' tool) */
void yyg_make_ex(int n, int level, uint32_t seed, long budget, yyg_puzzle *p, uint8_t *sol);
/* the levels (yinyang_levels.h, made by tools/yinyang_gen.c): "<n>:" then a character a cell, row by row: "." none,
 * "o" ivory, "x" ebony */
int  yyg_encode(const yyg_puzzle *p, char *out, int size);   /* its length; 0: does not fit */
int  yyg_decode(const char *s, yyg_puzzle *p);  /* 1: a puzzle */

void yyg_new(yyg_game *g, int variant, uint32_t seed);
int  yyg_load(yyg_game *g, const yyg_puzzle *p);     /* a puzzle given (a level), graded and solved; 1: it solves */
int  yyg_free(const yyg_game *g, int c);        /* 1: a cell the player may change (no stone given) */
int  yyg_tap_state(const yyg_game *g, int c);
int  yyg_hold_state(const yyg_game *g, int c);
int  yyg_paint(yyg_game *g, const int *cells, int n, int state);
int  yyg_hint(yyg_game *g);
int  yyg_undo(yyg_game *g);
int  yyg_square(const yyg_game *g, int c);      /* a stone of a 2 x 2 of one colour */
int  yyg_apart(const yyg_game *g, int c);       /* a stone cut off from the rest of its colour (no way to join) */
int  yyg_solved(const yyg_game *g);
int  yyg_script(yyg_game *g, const char *moves);

#endif
