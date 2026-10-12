/*
 * Loop for one player: the rules. A grid of n x n cells (5, 7, 10 or 15) between (n + 1) x (n + 1)
 * dots. Draw one closed loop along the cells' sides, from dot to dot, never crossing or touching itself. A number in
 * a cell says how many of its four sides the loop uses. Every puzzle has one solution.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: the four sizes, each at three levels (5 easy, 5 medium, 5 hard, 7 easy...). A puzzle is made from a
 * seed: a region of cells grown by chance (all in one piece, no holes, never touching itself at a corner), its
 * border the loop; every cell's number counted, then numbers taken away one by one (in a random order) as long as the
 * solver still finishes it at its level. The level is the reasoning the solver needs (pzbench.h):
 *   0 easy: a number's sides (all its lines drawn: the others crossed; as many sides left as lines missing: all
 *     drawn), a dot's two lines (a dot has two lines or none), and no small loop closed while lines lie elsewhere;
 *   1 medium: also a number seen with the dots at its corners (its four sides and the sides leaving its corners
 *     worked out together: a 3 in a corner, a 1 against the edge...);
 *   2 hard: also trials (a side that leads, by the easy ways, to a contradiction).
 * The sides: the across ones first, row by row from the top ((n + 1) rows of n), then the down ones, row by row (n
 * rows of n + 1). A side is empty, a line, or crossed (the player sure it has no line). The notation (script, the
 * screenshots, the tests): a dot is its column letter a.. and its row 1.. from the top; "a1-b1" turns the side between
 * the dots a1 and b1 to its next state (empty, line, crossed), "a1-b1=1" draws it, "=2" crosses it, "=0" empties it;
 * "h" a hint, "+n" puts the next n sides right.
 */
#ifndef LOOP_H
#define LOOP_H

#include <stdint.h>

#include "games/pzbench.h"

#define SLK_MAX 15
#define SLK_CELLS (SLK_MAX * SLK_MAX)
#define SLK_SIDES (2 * SLK_MAX * (SLK_MAX + 1))
#define SLK_SIZES 4
#define SLK_LEVELS 3
#define SLK_VARIANTS (SLK_SIZES * SLK_LEVELS)

enum { SLK_EASY, SLK_MEDIUM, SLK_HARD };
enum { SLK_EMPTY, SLK_LINE, SLK_CROSS };        /* a side as the player marks it */

static const int SLK_SIZE[SLK_SIZES] = {5, 7, 10, 15};
static inline int slk_size_of(int variant) { return SLK_SIZE[variant / SLK_LEVELS % SLK_SIZES]; }
static inline int slk_level_of(int variant) { return variant % SLK_LEVELS; }

/* the sides of a grid n wide: across ones (from the dot x, y to x + 1, y), down ones (x, y to x, y + 1) */
static inline int slk_across(int n, int x, int y) { return y * n + x; }
static inline int slk_down(int n, int x, int y) { return n * (n + 1) + y * (n + 1) + x; }
static inline int slk_sides(int n) { return 2 * n * (n + 1); }
/* a side's two dots (x0, y0) and (x1, y1) */
static inline void slk_ends(int n, int s, int *x0, int *y0, int *x1, int *y1)
{
    if (s < n * (n + 1)) {
        *x0 = s % n, *y0 = s / n, *x1 = *x0 + 1, *y1 = *y0;
    } else {
        s -= n * (n + 1);
        *x0 = s % (n + 1), *y0 = s / (n + 1), *x1 = *x0, *y1 = *y0 + 1;
    }
}

typedef struct {
    int n;
    int8_t clue[SLK_CELLS];                     /* 0..3, or -1: none */
    uint8_t sol[SLK_SIDES];                     /* 1: the loop runs along it */
} slk_puzzle;

typedef struct {
    int variant, level;
    uint32_t seed;
    slk_puzzle p;
    uint8_t side[SLK_SIDES];                    /* SLK_EMPTY, SLK_LINE, SLK_CROSS */
    pzb_hist hist;
} slk_game;

int  slk_count(const slk_puzzle *p, int limit);
int  slk_rate(const slk_puzzle *p);
void slk_make(int n, int level, uint32_t seed, slk_puzzle *p);
int  slk_loop_ok(int n, const uint8_t *on);     /* 1: these sides make one closed loop, touching itself nowhere */

void slk_new(slk_game *g, int variant, uint32_t seed);
int  slk_set(slk_game *g, int side, int state); /* 0: the same */
int  slk_cycle(slk_game *g, int side);          /* empty -> line -> crossed -> empty */
/* a stroke of the finger: its first side a new action, the next ones joined to it (undone together); 0: the same */
int  slk_stroke(slk_game *g, int side, int state, int first);
int  slk_hint(slk_game *g);                     /* a wrong mark put right, else a side found by reasoning: the side */
int  slk_undo(slk_game *g);
int  slk_lines(const slk_game *g, int cell);    /* the lines round a cell now */
int  slk_dot_lines(const slk_game *g, int x, int y);   /* the lines at a dot */
int  slk_solved(const slk_game *g);             /* the lines are the loop */
int  slk_script(slk_game *g, const char *moves);

#endif
