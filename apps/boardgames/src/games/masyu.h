/*
 * Masyu (pearls) for one player: the rules. A square grid with pearls in some cells; draw one closed loop through the
 * cells' middles, from cell to side-by-side cell, never crossing or touching itself, through every pearl. On a white
 * pearl the loop goes straight, and it turns in at least one of the two cells before and after it. On a black pearl
 * it turns, and goes straight on through the next cell on both sides.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 6 x 6, 8 x 8, 10 x 10, each easy, medium or hard (variant = size * 3 + level). A puzzle is made from a
 * seed: a region of the squares between the cells' middles grown by chance (in one piece, no holes, never touching
 * itself at a corner), its border the loop; a pearl put on every cell where one could go; while the solver (bench.h)
 * cannot finish it, the region changed a square at a time where the reasoning got stuck (a change kept when it gets
 * as far or further); then pearls taken away one by one while the solver still finishes it at its level. The ways of
 * reasoning, by level:
 *   easy: a cell has two lines or none (a pearl two); a black pearl's lines turn and each runs on through the next
 *     cell; a white pearl's go straight, and do not run on straight on both sides; no small loop closed while lines
 *     or pearls lie elsewhere;
 *   medium: a black pearl never heads into a black pearl, nor into a cell that already turns; a white pearl does not
 *     lie along a line whose next cells on both sides must go straight (a white pearl, a cell that can only go on);
 *   hard: chains (a line or none tried between two cells, the easy ways followed to a contradiction).
 * The links between the cells: the across ones first, row by row (n - 1 a row), then the down ones (n a row, n - 1
 * rows). A link is empty, a line, or crossed (the player sure it has no line). The notation (script, the screenshots,
 * the tests): a cell is its column letter a.. and its row 1.. from the top; "c4-c7" a drag along a row or a column
 * (lines drawn on every link, or taken away when the first link was a line), "c4/c5" a tap on the link between two
 * cells (a line in or out), ".c4/c5" a press held there (a cross in or out), "h" a hint, "+n" the next n links put
 * right.
 */
#ifndef MASYU_H
#define MASYU_H

#include <stdint.h>

#include "games/bench.h"

#define MSY_SIZES 3
#define MSY_VARIANTS (MSY_SIZES * 3)
#define MSY_MAX 12
#define MSY_CELLS (MSY_MAX * MSY_MAX)
#define MSY_LINKS (2 * MSY_MAX * (MSY_MAX - 1))
#define MSY_LEVELS 150                          /* the designed levels (masyu_levels.h), in six chapters */
#define MSY_CHAPTERS 6

enum { MSY_NO_PEARL, MSY_WHITE, MSY_BLACK };    /* a cell's pearl */
enum { MSY_EMPTY, MSY_LINE, MSY_CROSS };        /* a link as the player marks it */

/* the links of a grid n wide: across (the cell x, y to x + 1, y), down (x, y to x, y + 1) */
static inline int msy_links(int n) { return 2 * n * (n - 1); }
static inline int msy_across(int n, int x, int y) { return y * (n - 1) + x; }
static inline int msy_down(int n, int x, int y) { return n * (n - 1) + y * n + x; }
/* a link's two cells */
static inline void msy_ends(int n, int e, int *a, int *b)
{
    if (e < n * (n - 1)) {
        *a = e / (n - 1) * n + e % (n - 1);
        *b = *a + 1;
    } else {
        *a = e - n * (n - 1);
        *b = *a + n;
    }
}
/* the link between two cells side by side; -1: none */
static inline int msy_link(int n, int a, int b)
{
    if (a > b) { int t = a; a = b; b = t; }
    if (a < 0 || b >= n * n) return -1;
    if (b == a + 1 && a % n < n - 1) return msy_across(n, a % n, a / n);
    if (b == a + n) return msy_down(n, a % n, a / n);
    return -1;
}

typedef struct {
    int n;
    uint8_t pearl[MSY_CELLS];                   /* MSY_NO_PEARL, MSY_WHITE, MSY_BLACK */
} msy_puzzle;

typedef struct {
    msy_puzzle p;
    int level;
    uint32_t seed;
    uint8_t sol[MSY_LINKS];                     /* 1: a line in the solution */
    uint16_t st[MSY_LINKS];                     /* MSY_EMPTY, MSY_LINE, MSY_CROSS */
    bench_hist hist;
} msy_game;

int  msy_size(int variant);                     /* 6, 8, 10 */
int  msy_level(int variant);

int  msy_grade(const msy_puzzle *p, uint8_t *sol);   /* BENCH_* (the solution in sol when it solves it; may be NULL) */
int  msy_count(const msy_puzzle *p, int limit);
void msy_make(int n, int level, uint32_t seed, msy_puzzle *p, uint8_t *sol);
/* the same with more time (budget: the solver's passes it may spend; the levels' tool) */
void msy_make_ex(int n, int level, uint32_t seed, long budget, msy_puzzle *p, uint8_t *sol);
/* the levels (masyu_levels.h, made by tools/masyu_gen.c): "<n>:" then a character a cell, row by row ('.' none, 'w'
 * a white pearl, 'b' a black one) */
int  msy_encode(const msy_puzzle *p, char *out, int size);   /* its length; 0: no room */
int  msy_decode(const char *s, msy_puzzle *p);  /* 1: a puzzle */
/* the work a puzzle asks of the solver: its grade (BENCH_*), the passes and the finds of the medium ways */
int  msy_effort(const msy_puzzle *p, long *passes, long *mediums);

void msy_new(msy_game *g, int variant, uint32_t seed);
int  msy_load(msy_game *g, const msy_puzzle *p);     /* a puzzle given (a level), graded and solved; 1: it solves */
int  msy_tap_state(const msy_game *g, int e);   /* a tap: a line, or none */
int  msy_hold_state(const msy_game *g, int e);  /* a press held: a cross, or none */
int  msy_set(msy_game *g, int e, int state);    /* one action; 1: something changed */
int  msy_stroke(msy_game *g, int e, int state, int first);   /* a drag's links: one action from the first */
int  msy_hint(msy_game *g);                     /* the link put right; -1: none */
int  msy_undo(msy_game *g);                     /* the last link put back; -1: nothing to undo */
int  msy_lines(const msy_game *g, int c);       /* the lines at a cell */
int  msy_broken(const msy_game *g, int c);      /* a pearl whose rule the lines drawn already break */
int  msy_solved(const msy_game *g);
int  msy_script(msy_game *g, const char *moves);

#endif
