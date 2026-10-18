/*
 * Close the Loops for one player: the rules. Every square of the board holds a tile carrying a piece of line: an end,
 * a straight, a corner, a tee or a cross (or nothing). A tap turns a tile a quarter clockwise. The board is done when
 * every line end meets another one across the side of its square: no line left open, none running into the board's
 * edge, so the lines make closed shapes. Any such way of turning the tiles wins; there is no source and no sink.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A tile: its arms as 4 bits (1 up, 2 right, 4 down, 8 left); a quarter turn clockwise moves each arm to the next.
 * The par: the fewest taps that close the board from the start, over every closing way (lcl_solve: a search along
 * the rows keeping, for each column, whether the tile above reaches down and whether the tile on the left reaches
 * right: exact, and fast for boards up to LCL_MAXW wide). A tap that changes nothing (a cross, an empty square) is
 * refused. The boards: made by lcl_make (the levels' tool tools/loopclose_gen.c, offline, and at random alike) from
 * a symmetric design: a symmetric set of the squares between the tiles' middles chosen (their outlines become the
 * lines: closed shapes), then symmetric sets of inner walls (tees, crosses) and stubs (ends) maybe added, the tiles
 * then turned at random, the board never closed at the start.
 * A board as text: rows from the top split by '|', one hexadecimal digit a square: its arms at the start. The notation
 * (script, the screenshots, the tests): a square ("c4": column c from the left, row 4 from the top) taps it once; "h"
 * takes a hint (a tile not as in the nearest closing way, turned into it); "=" turns every tile into it.
 */
#ifndef LOOPCLOSE_H
#define LOOPCLOSE_H

#include <stdint.h>

#define LCL_MAXW 10
#define LCL_MAXH 13
#define LCL_CELLS (LCL_MAXW * LCL_MAXH)
#define LCL_HIST 2048
#define LCL_LEVELS 150
#define LCL_CHAPTERS 6
#define LCL_VARIANTS 3
#define LCL_TEXT (LCL_CELLS + LCL_MAXH + 2)

enum { LCL_UP = 1, LCL_RIGHT = 2, LCL_DOWN = 4, LCL_LEFT = 8 };

static inline int lcl_rot(int m) { return ((m << 1) | (m >> 3)) & 15; }

typedef struct {
    int w, h;
    uint8_t start[LCL_CELLS];                   /* x + y * w: the arms at the start */
} lcl_map;

typedef struct {
    lcl_map m;
    uint8_t cur[LCL_CELLS];
    uint8_t hist[LCL_HIST];                     /* the squares tapped, in turn */
    int nh, moves, par, hints;
} lcl_game;

typedef struct {
    int w, h;
    int sym;                                    /* 0 a mirror across, 1 both mirrors, 2 a quarter turn (square), 3 a
                                                 * half turn */
    int fill;                                   /* how likely a square between the tiles is inside, % */
    int walls;                                  /* how likely an inner wall is added, % */
    int stubs;                                  /* how likely a stub is added by an empty square, % */
    int mix;                                    /* how likely a tile is turned at the start, % (0: all; -1 none: the
                                                 * design itself, closed) */
} lcl_spec;

int  lcl_parse(lcl_map *m, const char *text);   /* 0: not a board */
void lcl_text(const lcl_map *m, char *out);     /* LCL_TEXT bytes */
void lcl_begin(lcl_game *g);
int  lcl_taps(int from, int to);                /* quarter turns clockwise from one tile to the other, -1 never */
int  lcl_tap(lcl_game *g, int c);               /* 1: turned (kept for undo), 0: refused (nothing would change) */
int  lcl_undo(lcl_game *g);
int  lcl_closed(int w, int h, const uint8_t *cur);
int  lcl_solved(const lcl_game *g);
/* the fewest taps closing the board from cur (-1: never), the tiles of such a way (may be NULL), how many closing
 * ways there are (capped at 1000000; may be NULL) */
int  lcl_solve(int w, int h, const uint8_t *cur, uint8_t *way, long *ways);
/* a hint: a tile not as in the nearest closing way, and the taps it needs; 0 none */
int  lcl_hint(const lcl_game *g, int *cell, int *taps);
int  lcl_make(lcl_map *m, const lcl_spec *spec, uint32_t seed);   /* 0: no design came out of the spec */
int  lcl_script(lcl_game *g, const char *moves);
typedef struct { const char *board; uint16_t par; } lcl_level_t;
extern const lcl_level_t lcl_levels[LCL_LEVELS];
int  lcl_level(lcl_game *g, int level);         /* 0: none */
void lcl_variant(int variant, lcl_spec *spec);
void lcl_random(lcl_game *g, int variant, uint32_t seed);

#endif
