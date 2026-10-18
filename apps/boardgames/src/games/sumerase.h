/*
 * Sum Erase for one player: the rules. A square grid of digits, a target at the end of each row and under each column.
 * Erase digits so that the digits left in every row and every column add up to its target. Each puzzle has exactly
 * one solution, found by reasoning alone.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants (random play): 6 x 6, 8 x 8, 9 x 9, each easy, medium or hard (variant = size * 3 + level). A puzzle is
 * made from a seed: digits and the cells kept dealt by chance (every line keeps some and loses some), the targets
 * added up; then cells changed a little at a time (a digit, kept or erased) until the solver (bench.h) solves it at
 * its level, and by no harder way. The ways of reasoning, by level:
 *   easy: a line's target reached: the rest goes; a digit bigger than what is left to find goes; a digit the line
 *     cannot do without (the others together fall short) stays;
 *   medium: the sums the line's open digits can make: a digit stays when no way without it reaches the target, goes
 *     when no way with it does;
 *   hard: chains (a cell tried erased or kept, the easy ways followed to a contradiction).
 * The player's cells: as dealt, erased, or ringed (a note: kept). Solved when every line's digits not erased add up to
 * its target (the one way they can).
 * Cells: x + y * n from the top left. The notation (script, the screenshots, the tests): a cell is its column letter
 * a.. and its row 1.. from the top; "c4" a tap (erased or not), ".c4" a press held (a ring in or out), "c4-c9" a drag
 * along a line (the cells as c4 was become as a tap there made it), ".c4-c9" the same held, "h" a hint, "+n" the next
 * n cells (reading order) not yet as the solution has them put right.
 */
#ifndef SUMERASE_H
#define SUMERASE_H

#include <stdint.h>

#include "games/bench.h"

#define SME_SIZES 3
#define SME_VARIANTS (SME_SIZES * 3)
#define SME_MAX 9
#define SME_CELLS (SME_MAX * SME_MAX)
#define SME_LEVELS 150                          /* the designed levels (sumerase_levels.h), in six chapters */
#define SME_CHAPTERS 6

enum { SME_EMPTY, SME_ERASE, SME_KEEP };        /* a cell as the player left it */

typedef struct {
    int n;
    uint8_t num[SME_CELLS];                     /* 1..9 */
    uint8_t row[SME_MAX], col[SME_MAX];         /* the targets: row y's, column x's */
} sme_puzzle;

typedef struct {
    sme_puzzle p;
    int level;
    uint32_t seed;
    uint8_t sol[SME_CELLS];                     /* 1: erased in the solution */
    uint16_t st[SME_CELLS];                     /* SME_EMPTY, SME_ERASE, SME_KEEP */
    bench_hist hist;
} sme_game;

int  sme_size(int variant);                     /* 6, 8, 9 */
int  sme_level(int variant);

void sme_sums(sme_puzzle *p, const uint8_t *erased);   /* the targets from the cells kept */
/* graded up to cap (BENCH_*: BENCH_HARD for the whole solver); the solution (1: erased) in sol when it solves it */
int  sme_grade(const sme_puzzle *p, int cap, uint8_t *sol);
int  sme_count(const sme_puzzle *p, int limit); /* the solutions, up to limit (a plain search) */
/* the work a puzzle asks of the solver: its grade (BENCH_*), the passes and the finds of the medium way */
int  sme_effort(const sme_puzzle *p, long *passes, long *mediums);
/* a puzzle of n x n at a level, digits 1..maxd, from a seed (budget: the solver's passes it may spend, 0 the usual) */
void sme_make(int n, int maxd, int level, uint32_t seed, long budget, sme_puzzle *p, uint8_t *sol);
/* the levels (sumerase_levels.h, made by tools/sumerase_gen.c): "<n>:" then a character a cell, row by row: '1'..'9'
 * a digit kept, 'a'..'i' a digit 1..9 erased (the targets are the sums) */
int  sme_encode(const sme_puzzle *p, const uint8_t *sol, char *out, int size);   /* its length; 0: no room */
int  sme_decode(const char *s, sme_puzzle *p, uint8_t *sol);                     /* 1: a puzzle */

void sme_new(sme_game *g, int variant, uint32_t seed);
int  sme_load(sme_game *g, const sme_puzzle *p);     /* a puzzle given (a level), graded and solved; 1: it solves */
int  sme_free(const sme_game *g, int c);        /* 1: a cell of the grid */
int  sme_tap_state(const sme_game *g, int c);
int  sme_hold_state(const sme_game *g, int c);
int  sme_paint(sme_game *g, const int *cells, int n, int state);
int  sme_hint(sme_game *g);                     /* the cell put right; -1 none */
int  sme_undo(sme_game *g);                     /* the cell put back; -1 nothing to undo */
/* a line (way 0 a row, 1 a column; i its index): 1 its digits not erased make the target, -1 it cannot any more (too
 * much erased, or the ringed digits alone past it), 0 otherwise */
int  sme_line(const sme_game *g, int way, int i);
int  sme_solved(const sme_game *g);
int  sme_script(sme_game *g, const char *moves);

#endif
