/*
 * Fillomino for one player: the rules. A square grid with numbers in some cells; fill every cell with a number so
 * that each region of equal numbers joined side by side has exactly that many cells. So two regions of the same size
 * never touch side by side (they would be one region). The numbers go from 1 to 9.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 6 x 6, 8 x 8, 10 x 10, each easy, medium or hard (variant = size * 3 + level). A puzzle is made from a
 * seed: the grid cut by chance into regions of 1 to 9 cells (two of a size never side by side), every cell's number
 * given; then numbers taken away one by one (in a random order) while the solver (bench.h) still finishes it at its
 * level. The ways of reasoning, by level:
 *   easy: a region with all its cells is closed (no cell beside it takes its number); a region still growing that
 *     has one way out takes it, and can only reach so far; a number does not go where it would join regions too big
 *     for it, nor where there is not room enough for a region of its size;
 *   medium: a cell a growing region cannot do without (without it, too little room is left) takes its number;
 *   hard: chains (a number tried in a cell, the easy ways followed to a contradiction).
 * The player's cells: a number (1 to 9), or notes (small numbers). Cells: x + y * n from the top left. The notation
 * (script, the screenshots, the tests): a cell is its column letter a.. and its row 1.. from the top; "c4=5" puts 5
 * there ("=0" takes it out), "c4'5" a note in or out, "h" a hint, "+n" the next n empty cells filled right.
 */
#ifndef FILLOMINO_H
#define FILLOMINO_H

#include <stdint.h>

#include "games/bench.h"

#define FLM_SIZES 3
#define FLM_VARIANTS (FLM_SIZES * 3)
#define FLM_MAX 12
#define FLM_CELLS (FLM_MAX * FLM_MAX)
#define FLM_TOP 9                               /* the biggest region */
#define FLM_LEVELS 150                          /* the designed levels (fillomino_levels.h), in six chapters */
#define FLM_CHAPTERS 6

typedef struct {
    int n;
    uint8_t given[FLM_CELLS];                   /* 1..9; 0: none */
} flm_puzzle;

typedef struct {
    flm_puzzle p;
    int level;
    uint32_t seed;
    uint8_t sol[FLM_CELLS];
    uint8_t val[FLM_CELLS];                     /* the player's numbers (the given ones too); 0: none */
    uint16_t notes[FLM_CELLS];                  /* bit d: the note d */
    bench_hist hist;
} flm_game;

int  flm_size(int variant);                     /* 6, 8, 10 */
int  flm_level(int variant);

int  flm_grade(const flm_puzzle *p, uint8_t *sol);   /* BENCH_* (the solution in sol when it solves it; may be NULL) */
int  flm_count(const flm_puzzle *p, int limit);
void flm_make(int n, int level, uint32_t seed, flm_puzzle *p, uint8_t *sol);
/* the same with more time (budget: the solver's passes it may spend; the levels' tool) */
void flm_make_ex(int n, int level, uint32_t seed, long budget, flm_puzzle *p, uint8_t *sol);
/* the levels (fillomino_levels.h, made by tools/fillomino_gen.c): "<n>:" then a character a cell, row by row ('.'
 * none, '1'..'9') */
int  flm_encode(const flm_puzzle *p, char *out, int size);   /* its length; 0: no room */
int  flm_decode(const char *s, flm_puzzle *p);  /* 1: a puzzle */
/* the work a puzzle asks of the solver: its grade (BENCH_*), the passes and the finds of the medium way */
int  flm_effort(const flm_puzzle *p, long *passes, long *mediums);

void flm_new(flm_game *g, int variant, uint32_t seed);
int  flm_load(flm_game *g, const flm_puzzle *p);     /* a puzzle given (a level), graded and solved; 1: it solves */
int  flm_put(flm_game *g, int c, int d);        /* d in the cell (0: taken out); 1: changed */
int  flm_note(flm_game *g, int c, int d);       /* the note d in or out */
int  flm_erase(flm_game *g, int c);             /* the number, else the notes */
int  flm_hint(flm_game *g);                     /* the cell put right; -1: none */
int  flm_undo(flm_game *g);                     /* the cell put back; -1: nothing to undo */
int  flm_region(const flm_game *g, int c);      /* the size of c's region of equal numbers (0: empty) */
int  flm_clash(const flm_game *g, int c);       /* a number whose region is bigger than it */
int  flm_solved(const flm_game *g);
int  flm_script(flm_game *g, const char *moves);

#endif
