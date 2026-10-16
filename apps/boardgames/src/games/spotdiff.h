/*
 * Spot the Difference for one player: the rules. Two grids of coloured cells, the same but for a few cells; find
 * every cell that differs (a tap on it in either grid). No SDL: the tests and tools/spotdiff_gen.c use these.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Colours for every eye: the nine colours are the Okabe-Ito palette (made to stay apart for the common colour-vision
 * deficiencies), and each colour carries its own small symbol (black a dot, orange a triangle...). A cell is a colour
 * and a symbol (an empty cell: the bare wood, no symbol). A difference changes one cell on one side:
 *   SDF_BOTH    its colour and so its symbol (a cell painted, or emptied, or another colour with its own symbol);
 *   SDF_SYMBOL  its symbol only (the colour stays: anyone sees the shape change);
 *   SDF_COLOUR  its colour only (the symbol stays), and only between two colours that stay apart for protan, deutan
 *               and tritan eyes alike (sdf_pair_ok: never a change only red and green tell apart).
 * The grids: a pixel-art drawing (a house, a boat, a cat...) on the bare wood, a drawing on a scene (a sky and a
 * meadow), or a busy pattern (a kaleidoscope, a quilt, blobs of colour); made from a seed (puzrand.h). The levels
 * (spotdiff_levels.h) are stored whole, made by tools/spotdiff_gen.c.
 * The notation (script, the screenshots, the tests): a cell is its column letter a.. and its row 1.. from the top;
 * "c4" a tap there, "h" a hint, "+n" the next n differences found.
 */
#ifndef SPOTDIFF_H
#define SPOTDIFF_H

#include <stdint.h>

#define SDF_MAXW 14
#define SDF_MAXH 14
#define SDF_CELLS (SDF_MAXW * SDF_MAXH)
#define SDF_COLOURS 10                          /* 0 the bare wood, 1..9 the palette */
#define SDF_SYMBOLS 10                          /* 0 none, 1..9: colour c's own symbol is c */
#define SDF_MAXD 8                              /* differences at most */
#define SDF_VARIANTS 4                          /* the random puzzles: 8 x 8 drawings .. 14 x 14 patterns */
#define SDF_LEVELS 150
#define SDF_CHAPTERS 6

enum { SDF_BOTH, SDF_SYMBOL, SDF_COLOUR, SDF_KINDS };
enum { SDF_DRAWING, SDF_SCENE, SDF_PATTERN, SDF_BLOBS, SDF_NOISE };   /* the kinds of grid */

/* the palette: 0 the wood (not drawn), 1 black, 2 orange, 3 sky blue, 4 bluish green, 5 yellow, 6 blue, 7 vermilion,
 * 8 reddish purple, 9 white */
extern const uint8_t sdf_rgb[SDF_COLOURS][3];
/* the symbols: 1 dot, 2 triangle, 3 square, 4 plus, 5 star, 6 diamond, 7 cross, 8 ring, 9 bar */
int  sdf_light_ink(int colour);                 /* 1: the symbol in light ink on this colour, else dark */
/* the least CIELAB distance between two colours seen by normal, protan, deutan and tritan eyes (simulated) */
float sdf_cvd_dist(int a, int b);
int  sdf_pair_ok(int a, int b);                 /* 1: a colour-only change from a to b is fair for every eye */

typedef struct { int16_t at; uint8_t side, c, s; } sdf_diff;   /* cell, the grid changed (0 left/top), its colour,
                                                                * its symbol there */
typedef struct {
    int w, h, n;                                /* the grids' size, the differences */
    uint8_t base[SDF_CELLS];                    /* the colours (the symbol of each is its colour's) */
    sdf_diff d[SDF_MAXD];
} sdf_puzzle;

typedef struct {
    sdf_puzzle p;
    uint8_t found[SDF_MAXD];                    /* 0 not yet, 1 found, 2 shown by a hint */
    int8_t hist[SDF_MAXD];                      /* the differences found, in order (undo) */
    int nfound;
    int hints, mistakes, taps;                  /* taps: the cells tapped (a find or a mistake) */
} sdf_game;

/* a cell as one grid shows it (side 0 or 1): its colour and symbol */
void sdf_cell(const sdf_puzzle *p, int side, int i, int *c, int *s);
int  sdf_diff_at(const sdf_puzzle *p, int i);   /* the difference on cell i, else -1 */
int  sdf_kind(const sdf_puzzle *p, int k);      /* SDF_BOTH / SYMBOL / COLOUR of difference k */
int  sdf_check(const sdf_puzzle *p);            /* 1: n differences, on distinct cells, each a real and fair one */
int  sdf_busy(const sdf_puzzle *p, int i);      /* 0..8: the neighbours of a cell coloured otherwise */

/* the makers. A grid of a kind (SDF_DRAWING...) w x h from a seed (pic: the drawing for DRAWING and SCENE, -1 by
 * chance), then n differences put on it: subtle 0 plain and in calm places .. 1 symbols and colours in busy places */
#define SDF_PICS 26
int  sdf_pic_count(void);
void sdf_grid(sdf_puzzle *p, int kind, int w, int h, int pic, uint32_t seed);
int  sdf_differ(sdf_puzzle *p, int n, float subtle, uint32_t seed);   /* 1: n put */
void sdf_make_variant(sdf_puzzle *p, int variant, uint32_t seed);     /* a random puzzle of a variant */
void sdf_variant_size(int variant, int *w, int *h);
/* how hard it is to find them all (the tool's ranking): the kinds, the busy places, the size */
float sdf_effort(const sdf_puzzle *p);

/* the levels as text: "<w>x<h>:" a digit a cell (its colour) row by row, ":" then each difference as its cell (three
 * digits), its side, its colour and its symbol (a digit each) */
int  sdf_encode(const sdf_puzzle *p, char *out, int size);   /* its length; 0: no room */
int  sdf_decode(const char *s, sdf_puzzle *p);  /* 1: a puzzle */

/* the play */
void sdf_load(sdf_game *g, const sdf_puzzle *p);
int  sdf_tap(sdf_game *g, int i);               /* a tap on cell i: the difference found (0..), -1 a mistake, -2 one
                                                 * found before (nothing) */
int  sdf_hint(sdf_game *g);                     /* a difference not found, now found (counted); -1 none */
int  sdf_undo(sdf_game *g);                     /* the difference unfound; -1 nothing to undo */
int  sdf_solved(const sdf_game *g);
int  sdf_script(sdf_game *g, const char *moves);   /* 1: all played */

#endif
