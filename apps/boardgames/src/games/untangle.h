/*
 * Untangle for one player: the rules. Dots on a board, joined by cords; at the start the cords cross each other. Move
 * the dots (any dot, anywhere on the board) until no two cords cross.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The puzzles are made from a seed: dots scattered evenly by chance, joined shortest cords first wherever a cord
 * crosses none before it (a triangulation: no crossing at all), then cords taken away by chance (every dot keeps two at
 * least, all stay joined); that layout is kept (the hint's goal), and the dots are set round a circle in a shuffled
 * order, which crosses many cords. The variants: 10, 20, 30 and 45 dots.
 * Two cords cross when they meet anywhere but at a dot they share, touching included (a dot lying on a cord); two
 * cords from one dot cross when one runs along the other.
 * Places: x and y from 0 to 1 across the board, from its top left. The notation (script, the screenshots, the tests):
 * "3:0.25,0.6" moves dot 3 (from 0) there; "h" a hint; "=" hints to the end.
 */
#ifndef UNTANGLE_H
#define UNTANGLE_H

#include <stdint.h>

#define UT_VARIANTS 4
#define UT_MAXV 45
#define UT_MAXE 132                             /* a triangulation has at most 3 v - 6 */
#define UT_HIST 1024
#define UT_MIN 0.03f                            /* the board's margin a dot keeps */

typedef struct { uint8_t v; float x, y; } ut_change;

typedef struct {
    int nv, ne;
    uint8_t e[UT_MAXE][2];
    float x[UT_MAXV], y[UT_MAXV];               /* the dots' places */
    float hx[UT_MAXV], hy[UT_MAXV];             /* a layout without crossings: the maker's */
    uint8_t home[UT_MAXV];                      /* 1: put in its place by a hint, and not moved since */
    ut_change hist[UT_HIST];
    int nh, moves;
} ut_game;

/* 1: the segments a-b and c-d meet (touching included) */
int  ut_segments_cross(float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy);
int  ut_cords_cross(const ut_game *g, int i, int j);   /* cords i and j (as defined above) */
int  ut_crossings(const ut_game *g);            /* the pairs of cords that cross */
int  ut_crossed(const ut_game *g, int i);       /* 1: cord i crosses another */
int  ut_solved(const ut_game *g);
void ut_new(ut_game *g, int variant, uint32_t seed);

int  ut_move(ut_game *g, int v, float x, float y);   /* a dot moved (kept on the board); 0: it did not move */
/* one step: a dot moved where it lowers the crossings most, else one put where it is in the maker's layout: the dot;
 * -1: solved */
int  ut_hint(ut_game *g);
int  ut_undo(ut_game *g);                       /* the last move taken back: the dot; -1: none */
int  ut_script(ut_game *g, const char *moves);  /* 0 at the first bad one */

#endif
