/*
 * Nurikabe (islands in a sea) for one player: the rules. A square grid with numbers in some cells;
 * shade the sea so that each number lies on an island of exactly that many cells, with no other number on it. The
 * islands never touch each other but at a corner, the whole sea is joined in one piece (side to side), and the sea
 * never fills a 2 x 2 square anywhere. Every unshaded cell belongs to an island with its number.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 6 x 6, 7 x 7, 8 x 8, each easy, medium or hard (variant = size * 3 + level). A puzzle is
 * made from a seed: islands grown by chance in the sea (never touching, the sea kept in one piece) until no 2 x 2
 * pool is left, each island's size written in one of its cells; grids are drawn until the solver (bench.h) solves one
 * at its level, and none past it. The ways of reasoning, by level:
 *   easy: an island with all its cells is closed by the sea; a cell next to two islands is sea; an island (or a piece
 *     of land with no number yet) with one way out takes it; three sea cells of a 2 x 2 square make the fourth land;
 *     a piece of sea with one way out takes it; a cell no island can reach is sea; the land's cells all found (or the
 *     sea's), the rest are the other;
 *   medium: a cell the sea cannot do without (land there would cut it in two) is sea; a cell an island cannot do
 *     without (sea there would leave it too little room) is land; a cell joining an island to land that would make it
 *     too big is sea;
 *   hard: chains (land or sea tried in a cell, the easy ways followed to a contradiction).
 * The player's cells: empty, sea, or a dot (a note: land). Solved when the sea keeps every rule (the one way it can).
 * Cells: x + y * n from the top left. The notation (script, the screenshots, the tests): a cell is its column letter
 * a.. and its row 1.. from the top; "c4" a tap (sea in or out), ".c4" a press held (a dot in or out), "c4-c9" a drag
 * along a line (the cells as c4 was become as a tap there made it), ".c4-c9" the same held (as a held press made it),
 * "h" a hint, "+n" the next n cells (reading order) not yet as the solution has them put right.
 */
#ifndef NURIKABE_H
#define NURIKABE_H

#include <stdint.h>

#include "games/bench.h"

#define NRK_SIZES 3
#define NRK_VARIANTS (NRK_SIZES * 3)
#define NRK_MAX 10
#define NRK_CELLS (NRK_MAX * NRK_MAX)
#define NRK_LEVELS 150                          /* the designed levels (nurikabe_levels.h), in six chapters */
#define NRK_CHAPTERS 6

enum { NRK_EMPTY, NRK_SEA, NRK_DOT };          /* a cell as the player left it */

typedef struct {
    int n;
    uint8_t clue[NRK_CELLS];                    /* an island's size written in the cell; 0: none */
} nrk_puzzle;

typedef struct {
    nrk_puzzle p;
    int level;
    uint32_t seed;
    uint8_t sol[NRK_CELLS];                     /* 1: sea in the solution */
    uint16_t st[NRK_CELLS];                     /* NRK_EMPTY, NRK_SEA, NRK_DOT */
    bench_hist hist;
} nrk_game;

int  nrk_size(int variant);                     /* 6, 7, 8 */
int  nrk_level(int variant);

int  nrk_grade(const nrk_puzzle *p, uint8_t *sol);   /* BENCH_* (the solution in sol when it solves it; may be NULL) */
int  nrk_count(const nrk_puzzle *p, int limit);
void nrk_make(int n, int level, uint32_t seed, nrk_puzzle *p, uint8_t *sol);
/* the same with more time (budget: the solver's passes it may spend; the levels' tool) */
void nrk_make_ex(int n, int level, uint32_t seed, long budget, nrk_puzzle *p, uint8_t *sol);
/* the levels (nurikabe_levels.h, made by tools/nurikabe_gen.c): a puzzle as text, "<n>:" then a character a cell,
 * row by row ('.' none, '1'..'9' then 'a'.. for 10..: a number) */
int  nrk_encode(const nrk_puzzle *p, char *out, int size);   /* its length; 0: no room */
int  nrk_decode(const char *s, nrk_puzzle *p);  /* 1: a puzzle */
/* the work a puzzle asks of the solver: its grade (BENCH_*), the passes and the finds of the medium ways */
int  nrk_effort(const nrk_puzzle *p, long *passes, long *mediums);

void nrk_new(nrk_game *g, int variant, uint32_t seed);
int  nrk_load(nrk_game *g, const nrk_puzzle *p);     /* a puzzle given (a level), graded and solved; 1: it solves */
int  nrk_free(const nrk_game *g, int c);        /* 1: not a number */
int  nrk_tap_state(const nrk_game *g, int c);
int  nrk_hold_state(const nrk_game *g, int c);
int  nrk_paint(nrk_game *g, const int *cells, int n, int state);
int  nrk_hint(nrk_game *g);
int  nrk_undo(nrk_game *g);
int  nrk_pool(const nrk_game *g, int c);        /* a sea cell in a 2 x 2 square of sea */
/* a number's island as the player has it: 1 too big (its dots and numbers joined, more than it says, or another
 * number), 2 closed by the sea at just its size, 0 else */
int  nrk_island(const nrk_game *g, int c);
int  nrk_solved(const nrk_game *g);
int  nrk_script(nrk_game *g, const char *moves);

#endif
