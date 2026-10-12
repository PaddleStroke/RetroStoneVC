/*
 * Skyscrapers for one player: the rules. A square of n x n cells is a town seen from above: put a building in every
 * cell, its height 1 to n, each height once in every row and every column. A number on the edge tells how many
 * buildings someone standing there sees along that row or column: a taller building hides every lower one behind it.
 * Some heights may be given. Every puzzle has one solution.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 4 x 4, 5 x 5, 6 x 6, 7 x 7, each easy, medium or hard (variant = size * 3 + level). A puzzle is made
 * from a seed: a town by chance (the solver's search, logicgrid.h), every edge number told (and heights given if the
 * numbers are not enough), then numbers and heights taken away as long as the solver still solves it within its
 * level. The ways of reasoning, by level:
 *   easy: a height used in a row or a column is not elsewhere in it; a height with one place left in a line; a cell
 *     with one height left; an edge number: 1 sees the tallest first, n sees them all in order, and a number k leaves
 *     no room for heights above n - k + 1 at the first cell, one more at the next, and so on;
 *   medium: a line seen whole: every order of its heights that its cells and its numbers allow agreeing on a cell;
 *   hard: chains (a height tried, the easy and medium ways followed to a contradiction).
 * The player's cells: a height (0: none) and notes (the heights the player keeps in mind). Solved when every cell has
 * its height and the town keeps the rules (the one solution).
 * Cells: x + y * n from the top left. The notation (script, the screenshots, the tests): a cell is its column letter
 * a.. and its row 1.. from the top; "c4=3" a height (0: none), "c4'3" a note in or out, "h" a hint, "+n" the next n
 * cells (reading order) without their height given theirs.
 */
#ifndef SKYSCRAPERS_H
#define SKYSCRAPERS_H

#include <stdint.h>

#include "games/logicgrid.h"

#define SKY_SIZES 4
#define SKY_VARIANTS (SKY_SIZES * 3)
#define SKY_MAX 7
#define SKY_CELLS (SKY_MAX * SKY_MAX)

enum { SKY_TOP, SKY_RIGHT, SKY_BOTTOM, SKY_LEFT };   /* the edges, clockwise */

typedef struct {
    int n;
    uint8_t clue[4][SKY_MAX];                   /* [edge][column or row]: buildings seen; 0: not told */
    uint8_t given[SKY_CELLS];                   /* a height given; 0: none */
} sky_puzzle;

typedef struct {
    sky_puzzle p;
    int level;
    uint32_t seed;
    uint8_t sol[SKY_CELLS];                     /* the heights */
    uint16_t st[SKY_CELLS];                     /* the grid now: the height (low 4 bits), the notes (bit 3 + h) */
    gl_hist hist;
} sky_game;

#define SKY_HEIGHT(st) ((st) & 15)
#define SKY_NOTE(st, h) ((st) >> (3 + (h)) & 1)

int  sky_size(int variant);                     /* 4, 5, 6, 7 */
int  sky_level(int variant);

int  sky_grade(const sky_puzzle *p, uint8_t *sol);   /* GL_* (the solution in sol when it solves it; may be NULL) */
int  sky_count(const sky_puzzle *p, int limit);
void sky_make(int n, int level, uint32_t seed, sky_puzzle *p, uint8_t *sol);
/* the buildings seen along a line from an edge (heights in the order seen; 0: an empty cell stops the count: -1) */
int  sky_seen(const uint8_t *heights, int n);
int  sky_line(int n, int edge, int i, int j);   /* the j-th cell seen from an edge along its line i */

void sky_new(sky_game *g, int variant, uint32_t seed);
void sky_load(sky_game *g, const sky_puzzle *p, int level);
int  sky_free(const sky_game *g, int c);        /* 1: the player's to fill (not given) */
int  sky_put(sky_game *g, int c, int h);        /* a height (0: none; the notes go, and its notes in the cell's lines);
                                                 * 0: nothing changed */
int  sky_note(sky_game *g, int c, int h);       /* a note in or out (a cell with no height) */
int  sky_clear(sky_game *g, int c);             /* the height, else the notes, taken out */
int  sky_hint(sky_game *g);
int  sky_undo(sky_game *g);
int  sky_clash(const sky_game *g, int c);       /* its height again in its row or column */
int  sky_edge_state(const sky_game *g, int edge, int i);   /* 0 open, 1 the line is full and seen right, -1 wrong */
int  sky_count_height(const sky_game *g, int h);
int  sky_solved(const sky_game *g);
int  sky_script(sky_game *g, const char *moves);

#endif
