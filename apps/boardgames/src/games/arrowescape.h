/*
 * Clear the Arrows for one player: the rules. A board is packed with arrow tiles: single squares, and bars two to four
 * squares long with the arrow at their head. A tapped tile slides off the board the way its arrow points when nothing
 * stands between its head and the edge; else it bumps the first tile in its way and stays: a mistake. Clear the board.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Ways (as swipepad.h's): 0 up (-y), 1 right, 2 down, 3 left; a quarter turn clockwise is way + 1. A bar lies along
 * its way: its head at (x, y), its other squares behind it. A turnstile (a single square on a brass pivot) that cannot
 * go turns a quarter clockwise instead (no mistake), so it leaves once any way out of it is clear.
 * Nothing ever comes back on the board: a tile free now stays free, so every board that can be cleared by taking the
 * free tiles in any order can always be cleared, and the only thing to lose is stars (mistakes, hints: the shell's
 * ui_logic_stars). The depth: the rounds of taking every free tile at once that clear the board (arx_rounds); the
 * levels' difficulty, with the tiles blocked at the start.
 * The boards: made by arx_make (the levels' tool tools/arrowescape_gen.c, offline, and at random alike) backwards:
 * tiles laid one by one (in symmetric pairs) onto the squares of a shape (a heart, a diamond...), each with its way
 * out clear of the tiles laid before it, which it may block: those leave after it. A square no tile could then take
 * is never left (the maker looks ahead).
 * A board as text: rows from the top split by '|', one character a square: '.' empty, 'U' 'R' 'D' 'L' a tile's head
 * pointing up, right, down, left, 'u' 'r' 'd' 'l' a bar's other squares (behind its head), '0'..'3' a turnstile
 * pointing up .. left. The notation (script, the screenshots, the tests): a square ("c4": column c from the left, row 4
 * from the top) taps the tile on it; "h" takes a hint (a tile free now, tapped); "=" taps every free tile in turn until
 * the board is clear.
 */
#ifndef ARROWESCAPE_H
#define ARROWESCAPE_H

#include <stdint.h>

#define ARX_MAXW 10
#define ARX_MAXH 12
#define ARX_CELLS (ARX_MAXW * ARX_MAXH)
#define ARX_MAXP ARX_CELLS
#define ARX_MAXLEN 4
#define ARX_HIST 512
#define ARX_LEVELS 150
#define ARX_CHAPTERS 6
#define ARX_VARIANTS 3
#define ARX_TEXT (ARX_CELLS + ARX_MAXH + 2)
#define ARX_NONE 0xFF

enum { ARX_UP, ARX_RIGHT, ARX_DOWN, ARX_LEFT };
enum { ARX_REFUSED, ARX_GONE, ARX_TURNED, ARX_BUMP };   /* what a tap did */
enum { ARX_RECT, ARX_DIAMOND, ARX_HEART, ARX_ROUND, ARX_PLUS, ARX_OCTAGON, ARX_BLOB, ARX_SHAPES };

static const int ARX_DX[4] = {0, 1, 0, -1}, ARX_DY[4] = {-1, 0, 1, 0};

typedef struct {
    int8_t x, y;                                /* its head */
    int8_t dir, len;                            /* its way at the start; its squares (1..ARX_MAXLEN) */
    int8_t turn;                                /* 1: a turnstile */
} arx_piece;

typedef struct {
    int w, h, n;
    arx_piece p[ARX_MAXP];
    uint8_t shape[ARX_CELLS];                   /* 1: a square of the board's shape (tiles at the start) */
} arx_map;

typedef struct { uint8_t piece, kind, dir; } arx_step;   /* kind ARX_GONE or ARX_TURNED; dir: its way before */

typedef struct {
    arx_map m;
    uint8_t dir[ARX_MAXP];                      /* each tile's way now (turnstiles turn) */
    uint8_t gone[ARX_MAXP];
    uint8_t at[ARX_CELLS];                      /* the tile on each square, or ARX_NONE */
    arx_step hist[ARX_HIST];
    int nh, left, moves, mistakes, hints;
} arx_game;

typedef struct {
    int w, h, shape;
    int sym;                                    /* 0 a mirror across, 1 a half turn, 2 none */
    int maxlen;                                 /* the longest bar */
    int bars;                                   /* how likely a tile is a bar, % */
    int turns;                                  /* how likely a single square is a turnstile, % */
    int deep;                                   /* how much the maker likes blocking the tiles laid before, 0..100 */
} arx_spec;

int  arx_parse(arx_map *m, const char *text);   /* 0: not a board */
void arx_text(const arx_map *m, char *out);     /* ARX_TEXT bytes */
void arx_begin(arx_game *g);
/* the squares of tile i lying its way dir (head first): how many */
int  arx_cells(const arx_map *m, int i, int dir, int *cells);
/* the first tile in tile i's way out (its way now), or -1: clear; *gap the empty squares before it (may be NULL) */
int  arx_blocker(const arx_game *g, int i, int *gap);
int  arx_free(const arx_game *g, int i);        /* 1: tile i can leave now (a turnstile: some way is clear) */
int  arx_tap(arx_game *g, int i, int *blocker); /* ARX_*: what it did (a bump: the mistake counted, the blocker) */
int  arx_undo(arx_game *g);                     /* a tile back (or a turnstile turned back); 0: none */
int  arx_solved(const arx_game *g);
/* a tile free now, the one that frees the most others (-1: none) */
int  arx_hint(const arx_game *g);
/* the rounds of taking every free tile at once that clear the board (-1: never cleared), the tiles free at first */
int  arx_rounds(const arx_map *m, int *free_at_start);
void arx_shape(int shape, int w, int h, uint8_t *mask);
int  arx_make(arx_map *m, const arx_spec *spec, uint32_t seed);   /* 0: the shape could not be filled */
int  arx_script(arx_game *g, const char *moves);
extern const char *const arx_levels[ARX_LEVELS];
int  arx_level(arx_game *g, int level);         /* 0: none */
void arx_variant(int variant, arx_spec *spec);
void arx_random(arx_game *g, int variant, uint32_t seed);

#endif
