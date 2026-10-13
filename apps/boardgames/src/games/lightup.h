/*
 * Light Up for one player: the rules. A grid of white cells and black walls; put lamps in white cells so that every
 * white cell is lit. A lamp lights its own cell and its row and its column both ways, up to a wall or the edge; no
 * lamp may light another. A wall with a number has that many lamps next to it (above, below, left, right).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 7 x 7, 10 x 10, 14 x 14, each easy, medium or hard (variant = size * 3 + level). A puzzle is made
 * from a seed: walls by chance (the same turned half round), lamps put where it is still dark until all is lit, every
 * wall numbered, then numbers taken away as long as the solver (bench.h) still solves it within its level. The
 * ways of reasoning (lu_rules), by level:
 *   easy: a lamp's lines hold no other lamp; a number with as many free cells round it as it lacks lamps, or with its
 *     lamps all there; a dark cell that only one free cell can still light;
 *   medium: a lamp tried in a cell, and only its own light followed: a number short of room, or a dark cell no lamp
 *     can reach any more, rules that cell out;
 *   hard: chains (a lamp or a dot tried, the easy ways followed to a contradiction).
 * The player's cells: empty, a lamp, or a dot (a note: no lamp here). Solved when every white cell is lit, no lamp
 * lights another and every number has its lamps (the one solution).
 * Cells: x + y * n from the top left. The notation (script, the screenshots, the tests): a cell is its column letter
 * a.. and its row 1.. from the top; "c4" a tap (a lamp in or out), ".c4" a press held (a dot in or out), "c4-c9" a
 * drag along a line (the cells as c4 was become as a held press made it: dots in or out), "h" a hint, "+n" the next n
 * cells (reading order) not yet as the solution has them put right.
 */
#ifndef LIGHTUP_H
#define LIGHTUP_H

#include <stdint.h>

#include "games/bench.h"

#define LU_SIZES 3
#define LU_VARIANTS (LU_SIZES * 3)
#define LU_MAX 14
#define LU_CELLS (LU_MAX * LU_MAX)
#define LU_WHITE (-2)                           /* lu_puzzle.wall: a white cell */
#define LU_WALL (-1)                            /* a wall without a number (else 0..4) */
#define LU_LEVELS 150                           /* the designed levels (lightup_levels.h), in six chapters */
#define LU_CHAPTERS 6

enum { LU_EMPTY, LU_LAMP, LU_DOT };            /* a white cell as the player left it */

typedef struct {
    int n;
    int8_t wall[LU_CELLS];                      /* LU_WHITE, LU_WALL, or the number 0..4 */
} lu_puzzle;

typedef struct {
    lu_puzzle p;
    int level;
    uint32_t seed;
    uint8_t sol[LU_CELLS];                      /* 1: a lamp in the solution */
    uint16_t st[LU_CELLS];                      /* LU_EMPTY, LU_LAMP, LU_DOT */
    bench_hist hist;
} lu_game;

int  lu_size(int variant);                      /* 7, 10, 14 */
int  lu_level(int variant);                     /* BENCH_EASY .. BENCH_HARD */

int  lu_grade(const lu_puzzle *p, uint8_t *sol); /* BENCH_* (the solution in sol when it solves it; may be NULL) */
int  lu_count(const lu_puzzle *p, int limit);   /* the solutions, up to limit */
void lu_make(int n, int level, uint32_t seed, lu_puzzle *p, uint8_t *sol);
/* the levels (lightup_levels.h, made by tools/lightup_gen.c): a puzzle as text, "<n>:" and a character a cell, row
 * by row ('.' white, '#' a wall, '0'..'4' a numbered wall) */
int  lu_encode(const lu_puzzle *p, char *out, int size);   /* its length; 0: no room */
int  lu_decode(const char *s, lu_puzzle *p);    /* 1: a puzzle */
/* the work a puzzle asks of the solver: its grade (BENCH_*), the passes (each tier tried once round) and the
 * finds of the medium way */
int  lu_effort(const lu_puzzle *p, long *passes, long *mediums);

void lu_new(lu_game *g, int variant, uint32_t seed);
int  lu_load(lu_game *g, const lu_puzzle *p);    /* a puzzle given (a level), graded and solved; 1: it solves */
int  lu_white(const lu_game *g, int c);
/* the player's actions: a cell's state changed (cells: the drag's cells; the state each takes); 0: nothing changed */
int  lu_tap_state(const lu_game *g, int c);     /* what a tap makes of the cell */
int  lu_hold_state(const lu_game *g, int c);    /* what a held press makes of it */
int  lu_paint(lu_game *g, const int *cells, int n, int state);
int  lu_hint(lu_game *g);                       /* a wrong cell put right, else the easiest cell found: the cell; -1 */
int  lu_undo(lu_game *g);                       /* a cell the last action changed; -1: none */
int  lu_lit(const lu_game *g, int c);           /* 1: a lamp lights it */
int  lu_clash(const lu_game *g, int c);         /* a lamp lit by another lamp */
int  lu_lamps_round(const lu_game *g, int c);   /* the lamps next to a wall */
int  lu_solved(const lu_game *g);
int  lu_script(lu_game *g, const char *moves);  /* 0 at the first bad one */

#endif
