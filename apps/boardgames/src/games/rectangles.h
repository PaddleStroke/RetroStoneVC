/*
 * Rectangles (the Shikaku genre) for one player: the rules. A square grid of 7, 10, 13 or 16 cells a side, numbers in
 * some cells. Divide the grid into rectangles so that each holds exactly one number, and that number is its area (how
 * many cells it covers). Every puzzle has one solution.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: the four sizes, each at three levels (7 easy, 7 medium, 7 hard, 10 easy...). A puzzle is made from a
 * seed: the grid cut into rectangles by chance (2 cells or more, squarish ones more often), a number put in each at a
 * random cell, and the puzzle kept only when the grader solves it at its level; the grader's deductions are sound, so
 * a puzzle it solves has that one solution (the tests count the solutions apart). The grader reasons as a person does
 * (rct_rate), each number with the rectangles it could still be:
 *   0 easy: the cells every rectangle of a number covers are its own, so no other number's rectangle covers them (a
 *     number with one rectangle left is sure of all of it);
 *   1 medium: also a cell only one number can reach is covered by that number's rectangle, and a rectangle that would
 *     leave another number no room is not the one;
 *   2 hard: also a rectangle that would leave any cell with no way to be covered is not the one, nor one that,
 *     followed through with the easier ways, comes to a contradiction (a number with four rectangles left at most).
 * A puzzle needing more than these is not used; one is kept only when it needs its level's own ways (else the
 * nearest found below it, rarely).
 * The player's rectangles may be wrong; a rectangle holding no number, two numbers, or a number not its area, is shown
 * pale. Solved when the rectangles cover the grid and each holds one number, its area.
 * Cells: x + y * n from the top left. The notation (script, the screenshots, the tests): a cell is its column letter
 * a.. and its row 1.. from the top; "b2-d3" draws the rectangle with these corners (the rectangles under it go),
 * "xc3" takes away the rectangle on c3, "h" a hint, "+n" draws the next n rectangles of the solution not yet drawn.
 */
#ifndef RECTANGLES_H
#define RECTANGLES_H

#include <stdint.h>

#include "games/bench.h"

#define RCT_MAX 16
#define RCT_CELLS (RCT_MAX * RCT_MAX)
#define RCT_CLUES 160                           /* the numbers a puzzle can have */
#define RCT_SIZES 4
#define RCT_LEVELS 3
#define RCT_VARIANTS (RCT_SIZES * RCT_LEVELS)

enum { RCT_EASY, RCT_MEDIUM, RCT_HARD, RCT_BEYOND };

static const int rct_sizes[RCT_SIZES] = {7, 10, 13, 16};
static inline int rct_size_of(int variant) { return rct_sizes[variant / RCT_LEVELS % RCT_SIZES]; }
static inline int rct_level_of(int variant) { return variant % RCT_LEVELS; }

/* a rectangle packed in a number (0: none): its corner x, y and its size w, h */
static inline uint32_t rct_pack(int x, int y, int w, int h)
{
    return 1u + (uint32_t)x + ((uint32_t)y << 4) + ((uint32_t)(w - 1) << 8) + ((uint32_t)(h - 1) << 12);
}
static inline void rct_unpack(uint32_t r, int *x, int *y, int *w, int *h)
{
    r--;
    *x = (int)(r & 15), *y = (int)(r >> 4 & 15), *w = (int)(r >> 8 & 15) + 1, *h = (int)(r >> 12 & 15) + 1;
}

typedef struct {
    int n;
    uint8_t clue[RCT_CELLS];                    /* the numbers (0: none) */
} rct_puz;

typedef struct {
    int n, level;
    uint32_t seed;
    rct_puz p;
    uint32_t sol[RCT_CELLS];                    /* the solution: each cell's rectangle */
    uint32_t own[RCT_CELLS];                    /* the player's: each cell's rectangle (0: none) */
    bench_hist hist;
} rct_game;

/* the solutions of a puzzle, counted up to limit; the first found in sol (each cell's rectangle; may be NULL) */
int  rct_count(const rct_puz *p, int limit, uint32_t *sol);
/* the grade of a puzzle (RCT_EASY..RCT_HARD; RCT_BEYOND: more than the grader's ways, or no single solution) */
int  rct_rate(const rct_puz *p);
/* a puzzle of the size and level from the seed, and its solution */
void rct_make(int n, int level, uint32_t seed, rct_puz *p, uint32_t *sol);

void rct_new(rct_game *g, int variant, uint32_t seed);
void rct_load(rct_game *g, int level, const rct_puz *p, const uint32_t *sol);
int  rct_draw(rct_game *g, int a, int b);       /* the rectangle with corners a and b (cells): the ones under it go;
                                                 * 0: nothing changed */
int  rct_erase(rct_game *g, int cell);          /* the rectangle on the cell taken away; 0: none */
int  rct_good(const rct_game *g, uint32_t r);   /* 1: the rectangle holds one number, its area */
int  rct_hint(rct_game *g);                     /* a wrong rectangle taken away, else the easiest one drawn: a cell of
                                                 * it; -1: none */
int  rct_undo(rct_game *g);                     /* the last action taken back: a cell it changed; -1: none */
int  rct_solved(const rct_game *g);
int  rct_script(rct_game *g, const char *moves);   /* 0 at the first bad one */

#endif
