/*
 * Thermometers for one player: the rules. A square grid covered with thermometers, each a glass tube running from
 * its bulb through a line of cells side by side. Fill some of them: a thermometer fills from its bulb up, with no gap
 * (as far as any cell, or not at all). The number beside each row and above each column gives its filled cells.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 6 x 6, 8 x 8, 10 x 10, each easy, medium or hard (variant = size * 3 + level). A puzzle is made from
 * a seed: the grid cut into thermometers by chance (walks of two cells or more, from a random end), each filled to a
 * level by chance, every row and column counted; then the levels changed a little at a time until the solver
 * (bench.h) solves it at its level, and by no harder way. The ways of reasoning, by level:
 *   easy: a filled cell fills those below it, an empty one empties those above; a line with all its filled cells, or
 *     with just room for them;
 *   medium: each level a thermometer could reach tried against every line it crosses (the others' least and most
 *     there): the levels no line allows ruled out;
 *   hard: chains (filled or empty tried in a cell, the easy ways followed to a contradiction).
 * The player's cells: empty, filled, or crossed (a note: empty). Solved when the filling keeps every rule (the one way
 * it can).
 * Cells: x + y * n from the top left. The notation (script, the screenshots, the tests): a cell is its column letter
 * a.. and its row 1.. from the top; "c4" a tap (filled up to it, or emptied from it), ".c4" a press held (crossed from
 * it on, or the cross taken out), "c4-c9" a drag along a line (crosses on the cells as c4 was), "h" a hint, "+n" the
 * next n cells (reading order) not yet as the solution has them put right.
 */
#ifndef THERMOMETERS_H
#define THERMOMETERS_H

#include <stdint.h>

#include "games/bench.h"

#define THM_SIZES 3
#define THM_VARIANTS (THM_SIZES * 3)
#define THM_MAX 10
#define THM_CELLS (THM_MAX * THM_MAX)
#define THM_LEVELS 150                          /* the designed levels (thermometers_levels.h), in six chapters */
#define THM_CHAPTERS 6

enum { THM_EMPTY, THM_FULL, THM_CROSS };       /* a cell as the player left it */
enum { THM_BULB = 4 };                          /* thm_puzzle.from: the bulb; else 0 up, 1 down, 2 left, 3 right */

typedef struct {
    int n;
    uint8_t from[THM_CELLS];                    /* where a cell's tube comes from (the cell below it in the
                                                 * thermometer): 0 above, 1 below, 2 left, 3 right, THM_BULB */
    int8_t count[2][THM_MAX];                   /* the rows' [0] and the columns' [1] filled cells */
    /* worked out by thm_link: each cell's thermometer, its place in it (0: the bulb), each thermometer's cells */
    int thermos;
    uint8_t of[THM_CELLS], at[THM_CELLS], len[THM_CELLS], cell[THM_CELLS][THM_MAX * 2];
} thm_puzzle;

typedef struct {
    thm_puzzle p;
    int level;
    uint32_t seed;
    uint8_t sol[THM_CELLS];                     /* 1: filled in the solution */
    uint16_t st[THM_CELLS];                     /* THM_EMPTY, THM_FULL, THM_CROSS */
    bench_hist hist;
} thm_game;

int  thm_size(int variant);                     /* 6, 8, 10 */
int  thm_level(int variant);

int  thm_link(thm_puzzle *p);                   /* the thermometers from p->from; 1: each a line from a bulb */
int  thm_prev(const thm_puzzle *p, int c);      /* the cell below c in its thermometer (-1: c is the bulb) */
int  thm_grade(const thm_puzzle *p, uint8_t *sol);   /* BENCH_* (the solution in sol when it solves it; may be NULL) */
int  thm_count(const thm_puzzle *p, int limit);
void thm_make(int n, int level, uint32_t seed, thm_puzzle *p, uint8_t *sol);
/* the same with more time (budget: the solver's passes it may spend; the levels' tool) */
void thm_make_ex(int n, int level, uint32_t seed, long budget, thm_puzzle *p, uint8_t *sol);
/* the levels (thermometers_levels.h, made by tools/thermometers_gen.c): a puzzle as text, "<n>:", the rows' counts,
 * the columns' (a digit each, 'a' for 10), then a character a cell, row by row: 'o' a bulb, else where its tube comes
 * from ('u' above, 'd' below, 'l' left, 'r' right) */
int  thm_encode(const thm_puzzle *p, char *out, int size);   /* its length; 0: no room */
int  thm_decode(const char *s, thm_puzzle *p);  /* 1: a puzzle */
/* the work a puzzle asks of the solver: its grade (BENCH_*), the passes and the finds of the medium way */
int  thm_effort(const thm_puzzle *p, long *passes, long *mediums);

void thm_new(thm_game *g, int variant, uint32_t seed);
int  thm_load(thm_game *g, const thm_puzzle *p);     /* a puzzle given (a level), graded and solved; 1: it solves */
int  thm_tap_state(const thm_game *g, int c);
int  thm_hold_state(const thm_game *g, int c);
/* what an action makes of the cells st (n x n): the cell a set to state (filled: those below it too; emptied: those
 * above it; crossed: those above it crossed too), and the same for each cell of the run a..b that was as a was */
void thm_apply(const thm_puzzle *p, uint16_t *st, int a, int b, int state);
int  thm_act(thm_game *g, int a, int b, int state);  /* thm_apply as a move (undone together); 1: something changed */
int  thm_hint(thm_game *g);
int  thm_undo(thm_game *g);
int  thm_line_full(const thm_game *g, int way, int i);  /* the filled cells of a row (way 0) or a column */
int  thm_solved(const thm_game *g);
int  thm_script(thm_game *g, const char *moves);

#endif
