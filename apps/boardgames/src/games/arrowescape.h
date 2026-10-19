/*
 * Clear the Arrows for one player: the rules. A board is packed with arrows: winding snakes of 1 to 12 squares, an
 * arrowhead at one end. A tapped arrow slithers off the board when every square from its head straight to the edge is
 * free: its head goes straight out the way it points and its body follows the head's path square by square, so the
 * body never needs room of its own. Else its head bumps the first arrow in its way and it stays: a mistake. Clear the
 * board.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Ways (as swipepad.h's): 0 up (-y), 1 right, 2 down, 3 left. An arrow: its head at (x, y) pointing dir, then its body
 * square by square (step[k]: the way from its square k to square k + 1, towards its tail). Its neck is right behind
 * its head (a head points the way its body travels); its body never stands on its own way out (the squares ahead of
 * its head).
 * Nothing ever comes back on the board: an arrow free now stays free, so every board that can be cleared by taking the
 * free arrows in any order can always be cleared, and the only thing to lose is stars (mistakes, hints: the shell's
 * ui_logic_stars). The depth: the rounds of taking every free arrow at once that clear the board (arx_rounds).
 * The boards: made by arx_make (the levels' tool tools/arrowescape_gen.c, offline, and at random alike) backwards
 * from an empty board: arrows laid one by one (in symmetric pairs) onto the squares of a shape (a heart, a
 * diamond...), each with its way out clear of the arrows laid before it (it leaves before them), its body winding
 * through the free squares, over their ways out (they must wait for it). The squares left at the end are taken by
 * the tails of arrows nearby when that keeps every way out clear.
 * A board as text: rows from the top split by '|', one character a square: '.' empty, 'U' 'R' 'D' 'L' an arrow's head
 * pointing up, right, down, left, 'u' 'r' 'd' 'l' a body square, pointing to the next square towards its head. The
 * notation (script, the screenshots, the tests): a square ("c4": column c from the left, row 4 from the top) taps the
 * arrow on it; "h" takes a hint (an arrow free now, tapped); "=" taps every free arrow in turn until the board is
 * clear.
 */
#ifndef ARROWESCAPE_H
#define ARROWESCAPE_H

#include <stdint.h>

#define ARX_MAXW 10
#define ARX_MAXH 12
#define ARX_CELLS (ARX_MAXW * ARX_MAXH)
#define ARX_MAXP ARX_CELLS
#define ARX_MAXLEN 12
#define ARX_HIST 512
#define ARX_LEVELS 150
#define ARX_CHAPTERS 6
#define ARX_VARIANTS 3
#define ARX_TEXT (ARX_CELLS + ARX_MAXH + 2)
#define ARX_NONE 0xFF

enum { ARX_UP, ARX_RIGHT, ARX_DOWN, ARX_LEFT };
enum { ARX_REFUSED, ARX_GONE, ARX_BUMP };       /* what a tap did */
enum { ARX_RECT, ARX_DIAMOND, ARX_HEART, ARX_ROUND, ARX_PLUS, ARX_OCTAGON, ARX_BLOB, ARX_SHAPES };

static const int ARX_DX[4] = {0, 1, 0, -1}, ARX_DY[4] = {-1, 0, 1, 0};

typedef struct {
    int8_t x, y;                                /* its head */
    int8_t dir, len;                            /* its way out; its squares (1..ARX_MAXLEN) */
    int8_t step[ARX_MAXLEN];                    /* the way from square k to square k + 1 (towards its tail) */
} arx_piece;

typedef struct {
    int w, h, n;
    arx_piece p[ARX_MAXP];
    uint8_t shape[ARX_CELLS];                   /* 1: a square of the board's shape */
} arx_map;

typedef struct {
    arx_map m;
    uint8_t gone[ARX_MAXP];
    uint8_t at[ARX_CELLS];                      /* the arrow on each square, or ARX_NONE */
    uint8_t hist[ARX_HIST];                     /* the arrows gone, in turn */
    int nh, left, moves, mistakes, hints;
} arx_game;

typedef struct {
    int w, h, shape;
    int sym;                                    /* 0 a mirror across, 1 a half turn, 2 none */
    int lo, hi;                                 /* the arrows' lengths wanted */
    int turn;                                   /* how much the bodies like to bend, 0..100 */
    int hug;                                    /* how much the bodies like to run along others (interlocks), 0..100 */
    int deep;                                   /* how much the maker likes blocking the arrows laid before, 0..100 */
} arx_spec;

int  arx_parse(arx_map *m, const char *text);   /* 0: not a board */
void arx_text(const arx_map *m, char *out);     /* ARX_TEXT bytes */
void arx_begin(arx_game *g);
/* the squares of arrow i, head first (x + y * w): how many */
int  arx_cells(const arx_map *m, int i, int *cells);
int  arx_bends(const arx_map *m, int i);        /* how many times its body turns */
/* the first arrow in arrow i's way out, or -1: clear; *gap the empty squares before it (may be NULL) */
int  arx_blocker(const arx_game *g, int i, int *gap);
int  arx_free(const arx_game *g, int i);        /* 1: arrow i can leave now */
int  arx_tap(arx_game *g, int i, int *blocker); /* ARX_*: what it did (a bump: the mistake counted, the blocker) */
int  arx_undo(arx_game *g);                     /* the last arrow gone back; 0: none */
int  arx_solved(const arx_game *g);
/* an arrow free now, the one that frees the most others (-1: none) */
int  arx_hint(const arx_game *g);
/* the rounds of taking every free arrow at once that clear the board (-1: never cleared), the arrows free at first */
int  arx_rounds(const arx_map *m, int *free_at_start);
void arx_shape(int shape, int w, int h, uint8_t *mask);
int  arx_make(arx_map *m, const arx_spec *spec, uint32_t seed);   /* 0: the shape could not be filled */
int  arx_script(arx_game *g, const char *moves);
extern const char *const arx_levels[ARX_LEVELS];
int  arx_level(arx_game *g, int level);         /* 0: none */
void arx_variant(int variant, arx_spec *spec);
void arx_random(arx_game *g, int variant, uint32_t seed);

#endif
