/*
 * Heyawake (rooms) for one player: the rules. A square grid cut into rectangular rooms, a number in some of them;
 * shade cells so that a room with a number has exactly that many shaded cells. Shaded cells never touch side by
 * side, the cells left unshaded are all joined in one piece (side by side), and no straight line of unshaded cells
 * runs through more than two rooms (it never crosses two room walls).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 6 x 6, 8 x 8, 10 x 10, each easy, medium or hard (variant = size * 3 + level). A puzzle is made from a
 * seed: rooms cut by chance, cells shaded by chance where a line would run through three rooms (never touching, the
 * rest kept joined), every room's number counted; while the solver (bench.h) cannot finish it, a shaded cell moved
 * where the reasoning got stuck (a change kept when it gets as far or further); then numbers taken away one by one
 * while the solver still finishes it at its level. The ways of reasoning, by level:
 *   easy: round a shaded cell nothing is shaded; a room with all its shaded cells keeps the rest, one with just
 *     enough room shades it all; a line through three rooms with one cell left open shades it; a piece of unshaded
 *     cells with one way out takes it;
 *   medium: a cell the unshaded ones cannot do without (shaded, it would cut them apart) stays unshaded;
 *   hard: chains (shaded or not tried in a cell, the easy ways followed to a contradiction).
 * The player's cells: empty, shaded, or a dot (a note: unshaded). Solved when the shading keeps every rule (the one
 * way it can).
 * Cells: x + y * n from the top left. The notation (script, the screenshots, the tests): a cell is its column letter
 * a.. and its row 1.. from the top; "c4" a tap (shaded or not), ".c4" a press held (a dot in or out), "c4-c9" a drag
 * along a line (the cells as c4 was become as a tap there made it), ".c4-c9" the same held, "h" a hint, "+n" the next
 * n cells (reading order) not yet as the solution has them put right.
 */
#ifndef HEYAWAKE_H
#define HEYAWAKE_H

#include <stdint.h>

#include "games/bench.h"

#define HYK_SIZES 3
#define HYK_VARIANTS (HYK_SIZES * 3)
#define HYK_MAX 12
#define HYK_CELLS (HYK_MAX * HYK_MAX)
#define HYK_LEVELS 150                          /* the designed levels (heyawake_levels.h), in six chapters */
#define HYK_CHAPTERS 6

enum { HYK_EMPTY, HYK_SHADE, HYK_DOT };        /* a cell as the player left it */

typedef struct {
    int n, rooms;
    uint8_t room[HYK_CELLS];                    /* each cell's room, 0.. in the order of their top left cells */
    int8_t num[HYK_CELLS];                      /* each room's number; -1: none */
} hyk_puzzle;

typedef struct {
    hyk_puzzle p;
    int level;
    uint32_t seed;
    uint8_t sol[HYK_CELLS];                     /* 1: shaded in the solution */
    uint16_t st[HYK_CELLS];                     /* HYK_EMPTY, HYK_SHADE, HYK_DOT */
    bench_hist hist;
} hyk_game;

int  hyk_size(int variant);                     /* 6, 8, 10 */
int  hyk_level(int variant);

int  hyk_grade(const hyk_puzzle *p, uint8_t *sol);   /* BENCH_* (the solution in sol when it solves it; may be NULL) */
int  hyk_count(const hyk_puzzle *p, int limit);
void hyk_make(int n, int level, uint32_t seed, hyk_puzzle *p, uint8_t *sol);
/* the same with more time (budget: the solver's passes it may spend; the levels' tool) */
void hyk_make_ex(int n, int level, uint32_t seed, long budget, hyk_puzzle *p, uint8_t *sol);
/* the levels (heyawake_levels.h, made by tools/heyawake_gen.c): "<n>:" then each room in the order of their top left
 * cells, five characters: its left column, its top row, its width, its height (0-9 then a-z) and its number ('.'
 * none) */
int  hyk_encode(const hyk_puzzle *p, char *out, int size);   /* its length; 0: no room */
int  hyk_decode(const char *s, hyk_puzzle *p);  /* 1: a puzzle (every cell in one room) */
/* the work a puzzle asks of the solver: its grade (BENCH_*), the passes and the finds of the medium way */
int  hyk_effort(const hyk_puzzle *p, long *passes, long *mediums);

void hyk_new(hyk_game *g, int variant, uint32_t seed);
int  hyk_load(hyk_game *g, const hyk_puzzle *p);     /* a puzzle given (a level), graded and solved; 1: it solves */
int  hyk_free(const hyk_game *g, int c);        /* 1: a cell of the grid */
int  hyk_tap_state(const hyk_game *g, int c);
int  hyk_hold_state(const hyk_game *g, int c);
int  hyk_paint(hyk_game *g, const int *cells, int n, int state);
int  hyk_hint(hyk_game *g);
int  hyk_undo(hyk_game *g);
int  hyk_touching(const hyk_game *g, int c);    /* a shaded cell beside another */
int  hyk_apart(const hyk_game *g, int c);       /* an unshaded cell cut off from the others by the shading */
int  hyk_long(const hyk_game *g, int c);        /* an unshaded cell of a line of them through three rooms */
/* a room's shaded cells against its number: 1 too many, 2 just its number, 0 else (or no number) */
int  hyk_room(const hyk_game *g, int r);
int  hyk_solved(const hyk_game *g);
int  hyk_script(hyk_game *g, const char *moves);

#endif
