/*
 * Tents for one player: the rules. A square field with trees; put a tent next to each tree (above, below, left or
 * right of it), one tent a tree and one tree a tent. Tents never touch each other, not even at a corner. The number
 * beside a row or above a column gives its tents.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 6 x 6, 8 x 8, 10 x 10, 12 x 12, each easy, medium or hard (variant = size * 3 + level). A field is
 * made from a seed: tents put by chance where they touch none, each with its tree beside it, every row and column
 * counted; fields are drawn until the solver (bench.h) solves one at its level, and none past it. The ways of
 * reasoning, by level:
 *   easy: no tent away from the trees, nor round a tent; a line with all its tents, or with just room for them; a
 *     tree with one place left for its tent; a tree whose tent can only be one tent next to it (that tent is its own),
 *     so the cells round it that no other tree has get none;
 *   medium: a line whose tents just fit in the stretches of free cells left (every other cell of a stretch of three,
 *     one of a pair: none in the cells touching both of the pair); a tent tried in a cell, only the cells round it
 *     followed: a tree, a line or a tree's own tent left with no room rules that cell out;
 *   hard: chains (a tent or grass tried, the easy ways followed to a contradiction).
 * The player's cells: empty, a tent, or grass (a note: no tent here). Solved when the tents keep every rule (the one
 * way they can).
 * Cells: x + y * n from the top left. The notation (script, the screenshots, the tests): a cell is its column letter
 * a.. and its row 1.. from the top; "c4" a tap (a tent in or out), ".c4" a press held (grass in or out), "c4-c9" a
 * drag along a line (the cells as c4 was become as a held press made it: grass in or out), "h" a hint, "+n" the next n
 * cells (reading order) not yet as the solution has them put right.
 */
#ifndef TENTS_H
#define TENTS_H

#include <stdint.h>

#include "games/bench.h"

#define TN_SIZES 4
#define TN_VARIANTS (TN_SIZES * 3)
#define TN_MAX 12
#define TN_CELLS (TN_MAX * TN_MAX)
#define TN_LEVELS 150                           /* the designed levels (tents_levels.h), in six chapters */
#define TN_CHAPTERS 6

enum { TN_EMPTY, TN_TENT, TN_GRASS };          /* a cell as the player left it */

typedef struct {
    int n;
    uint8_t tree[TN_CELLS];                     /* 1: a tree */
    int8_t count[2][TN_MAX];                    /* the rows' [0] and the columns' [1] tents; -1: not told */
} tn_puzzle;

typedef struct {
    tn_puzzle p;
    int level;
    uint32_t seed;
    uint8_t sol[TN_CELLS];                      /* 1: a tent in the solution */
    uint16_t st[TN_CELLS];                      /* TN_EMPTY, TN_TENT, TN_GRASS */
    bench_hist hist;
} tn_game;

int  tn_size(int variant);                      /* 6, 8, 10, 12 */
int  tn_level(int variant);

int  tn_grade(const tn_puzzle *p, uint8_t *sol); /* BENCH_* (the solution in sol when it solves it; may be NULL) */
int  tn_count(const tn_puzzle *p, int limit);
void tn_make(int n, int level, uint32_t seed, tn_puzzle *p, uint8_t *sol);
/* the levels (tents_levels.h, made by tools/tents_gen.c): a field as text, "<n>:", the rows' counts, the columns'
 * (a digit each, '-' not told), then a character a cell, row by row ('T' a tree, '.' none) */
int  tn_encode(const tn_puzzle *p, char *out, int size);   /* its length; 0: no room */
int  tn_decode(const char *s, tn_puzzle *p);    /* 1: a puzzle */
/* the work a puzzle asks of the solver: its grade (BENCH_*), the passes (each tier tried once round) and the
 * finds of the medium way */
int  tn_effort(const tn_puzzle *p, long *passes, long *mediums);

void tn_new(tn_game *g, int variant, uint32_t seed);
int  tn_load(tn_game *g, const tn_puzzle *p);   /* a puzzle given (a level), graded and solved; 1: it solves */
int  tn_free(const tn_game *g, int c);          /* 1: not a tree */
int  tn_tap_state(const tn_game *g, int c);
int  tn_hold_state(const tn_game *g, int c);
int  tn_paint(tn_game *g, const int *cells, int n, int state);
int  tn_hint(tn_game *g);
int  tn_undo(tn_game *g);
int  tn_touching(const tn_game *g, int c);      /* a tent touching another */
int  tn_line_tents(const tn_game *g, int way, int i);   /* the tents in a row (way 0) or a column */
int  tn_tree_lost(const tn_game *g, int c);     /* a tree with no tent beside it and no room left for one */
int  tn_solved(const tn_game *g);
int  tn_script(tn_game *g, const char *moves);

#endif
