/*
 * Hidden Fleet (the solitaire puzzle of ships in a grid) for one player: the rules. A fleet of straight ships, one to
 * four cells long, lies hidden in a square sea. The number beside each row and above each column tells how many of
 * its cells are ship cells; ships never touch, not even at a corner; a few cells are shown at the start (water, or a
 * ship's part: a one-cell ship, an end pointing one way, a middle). Find every ship. One solution.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 6 x 6, 8 x 8, 10 x 10, each easy, medium or hard (variant = size * 3 + level); the fleet grows with
 * the sea (hfl_fleet). A puzzle is made from a seed: the fleet laid by chance, the rows and columns counted, every
 * cell shown, then the shown cells taken away (water first) while the solver (bench.h) still finishes it at its
 * level. The solver only proves, so a puzzle it finishes has that one solution. The ways of reasoning, by level:
 *   easy: the cells round a ship's corners are water; a line with all its ship cells has water in the rest, one with
 *     just room for them ships in all; the shown parts' sides; ships in a line keep their sides water; a ship that can
 *     grow no longer (no ship left that long) ends in water; a cell that would join a ship longer than any left is
 *     water; and a cell tried each way, those ways followed once, to a contradiction;
 *   medium: chains of three steps; hard: chains of up to eight.
 * The player's cells: empty, a ship cell, or water (a note). Solved when the ship cells are the fleet's.
 * Cells: x + y * n from the top left. The notation (script, the screenshots, the tests): a cell is its column letter
 * a.. and its row 1.. from the top; "c4" a tap (a ship cell in or out), ".c4" a press held (water in or out),
 * "c4-c9" a drag along a line (water in the cells as c4 was, or out), "h" a hint, "+n" the next n cells (reading order)
 * not yet as the solution has them put right.
 */
#ifndef FLEET_H
#define FLEET_H

#include <stdint.h>

#include "games/bench.h"

#define HFL_SIZES 3
#define HFL_VARIANTS (HFL_SIZES * 3)
#define HFL_MAX 10
#define HFL_CELLS (HFL_MAX * HFL_MAX)
#define HFL_SHIPS 10                            /* the most ships in a fleet */
#define HFL_LONG 4                              /* the longest ship */
#define HFL_LEVELS 150                          /* the designed levels (fleet_levels.h), in six chapters */
#define HFL_CHAPTERS 6

enum { HFL_EMPTY, HFL_SHIP, HFL_WATER };       /* a cell as the player left it */
/* what a cell shows (a clue): nothing, water, a one-cell ship, an end (the ship goes on down, up, right, left from
 * it: its top, bottom, left, right end), a middle */
enum { HFL_NONE, HFL_SEA, HFL_ONE, HFL_TOP, HFL_BOTTOM, HFL_LEFT, HFL_RIGHT, HFL_MIDDLE };

typedef struct {
    int n;
    int8_t row[HFL_MAX], col[HFL_MAX];          /* each row's and column's ship cells */
    uint8_t given[HFL_CELLS];                   /* HFL_NONE .. HFL_MIDDLE */
} hfl_puzzle;

typedef struct {
    hfl_puzzle p;
    int level;
    uint32_t seed;
    uint8_t sol[HFL_CELLS];                     /* 1: a ship cell in the solution */
    uint16_t st[HFL_CELLS];                     /* HFL_EMPTY, HFL_SHIP, HFL_WATER (a shown cell: as it shows) */
    bench_hist hist;
} hfl_game;

int  hfl_size(int variant);                     /* 6, 8, 10 */
int  hfl_level(int variant);
/* the fleet of a sea of side n: how many ships of each length 1..HFL_LONG (count[0] unused); the ships in all */
int  hfl_fleet(int n, int *count);
/* the part a ship cell is in a full shading sh (HFL_ONE .. HFL_MIDDLE), as its neighbours show */
int  hfl_part(const uint8_t *sh, int n, int c);

int  hfl_grade(const hfl_puzzle *p, uint8_t *sol);   /* BENCH_* (the solution in sol when it solves it; may be NULL) */
int  hfl_count(const hfl_puzzle *p, int limit);
/* the work a puzzle asks of the solver: its grade, the passes and the cells found by trials */
int  hfl_effort(const hfl_puzzle *p, long *passes, long *trials);
void hfl_make(int n, int level, uint32_t seed, hfl_puzzle *p, uint8_t *sol);
/* the same with more time (budget: the solver's passes it may spend; the levels' tool) */
void hfl_make_ex(int n, int level, uint32_t seed, long budget, hfl_puzzle *p, uint8_t *sol);
/* the levels (fleet_levels.h, made by tools/fleet_gen.c): "<n>:" then the rows' counts and the columns' (a digit
 * each, or a letter a.. past 9), ":" and the shown cells, each its column and row letters and what it shows
 * ("~" water, "o" one-cell ship, "^" top end, "v" bottom end, "<" left end, ">" right end, "=" middle) */
int  hfl_encode(const hfl_puzzle *p, char *out, int size);   /* its length; 0: does not fit */
int  hfl_decode(const char *s, hfl_puzzle *p);  /* 1: a puzzle */

void hfl_new(hfl_game *g, int variant, uint32_t seed);
int  hfl_load(hfl_game *g, const hfl_puzzle *p);     /* a puzzle given (a level), graded and solved; 1: it solves */
int  hfl_free(const hfl_game *g, int c);        /* 1: a cell the player may change (not shown) */
int  hfl_tap_state(const hfl_game *g, int c);
int  hfl_hold_state(const hfl_game *g, int c);
int  hfl_paint(hfl_game *g, const int *cells, int n, int state);
int  hfl_hint(hfl_game *g);
int  hfl_undo(hfl_game *g);
int  hfl_line(const hfl_game *g, int way, int i);   /* a row's (way 0) or column's (1) ship cells now */
int  hfl_clash(const hfl_game *g, int c);       /* a ship cell touching another at a corner, or in a ship too long */
/* the ships the player has closed (water or the rim all round; one of the longest by its length alone), by length
 * (count[0] unused) */
void hfl_found(const hfl_game *g, int *count);
int  hfl_solved(const hfl_game *g);
int  hfl_script(hfl_game *g, const char *moves);

#endif
