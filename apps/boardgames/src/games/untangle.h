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
 * The levels (untangle_levels.h, made by tools/untangle_gen.c): 150 puzzles in six chapters, 6 to 36 dots: a layout
 * without crossings out of which a few dots were moved, each kept as text (ut_encode). Their par: the dots moved
 * (put back, they untangle it), at most one more than the fewest that might do (ut_cover: no smaller set of dots has
 * one of every crossing's four ends). A random puzzle's
 * par: the fewest dots found to move (some dots of the circle can stay where they are while the others go to the
 * maker's layout: ut_new works it out), the best known. Stars: the moves against par; a hint caps them (pzkit.h).
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
#define UT_LEVELS 150
#define UT_CHAPTERS 6
#define UT_CODE "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_"
static inline float ut_grid(int q) { return 0.06f + 0.88f * q / 63; }   /* a level's places: 64 steps across */

typedef struct { uint8_t v; float x, y; } ut_change;

typedef struct {
    int nv, ne;
    uint8_t e[UT_MAXE][2];
    float x[UT_MAXV], y[UT_MAXV];               /* the dots' places */
    float hx[UT_MAXV], hy[UT_MAXV];             /* a layout without crossings: the maker's */
    uint8_t home[UT_MAXV];                      /* 1: put in its place by a hint, and not moved since */
    ut_change hist[UT_HIST];
    int nh, moves;
    int par;                                    /* the fewest moves known to untangle it */
    int hints;                                  /* the hints given (undo does not take them back) */
} ut_game;

/* 1: the segments a-b and c-d meet (touching included) */
int  ut_segments_cross(float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy);
int  ut_cords_cross(const ut_game *g, int i, int j);   /* cords i and j (as defined above) */
int  ut_crossings(const ut_game *g);            /* the pairs of cords that cross */
int  ut_crossed(const ut_game *g, int i);       /* 1: cord i crosses another */
int  ut_solved(const ut_game *g);
void ut_new(ut_game *g, int variant, uint32_t seed);
/* nv dots, ne cords: the maker's layout (hx, hy) and the dots round the circle (x, y); 0: none found */
int  ut_make(ut_game *g, int nv, int ne, uint32_t seed);
/* the fewest dots that must move to untangle it: a set of dots that has one of every crossing's dots (the ends of its
 * two cords), the smallest found up to limit (limit + 1: more) */
int  ut_cover(const ut_game *g, int limit);
/* a few dots can stay where they are while the others go to the maker's layout: how many must move (a par) */
int  ut_upper(const ut_game *g);
/* a level kept as text: "<nv>:" then each dot's place in the layout without crossings (two characters of UT_CODE, x
 * then y, on ut_grid's steps), then its place at the start (the same), then each cord's two dots (two characters); its
 * par the dots not starting in their place. ut_decode: 0 when it is not one */
int  ut_encode(const ut_game *g, char *out, int size);
int  ut_decode(ut_game *g, const char *code);

int  ut_move(ut_game *g, int v, float x, float y);   /* a dot moved (kept on the board); 0: it did not move */
/* one step: a dot moved where it lowers the crossings most, else one put where it is in the maker's layout: the dot;
 * -1: solved */
int  ut_hint(ut_game *g);                       /* counted in hints */
int  ut_undo(ut_game *g);                       /* the last move taken back: the dot; -1: none */
int  ut_script(ut_game *g, const char *moves);  /* 0 at the first bad one */

#endif
