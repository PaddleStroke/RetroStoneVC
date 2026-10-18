/*
 * Paper Fold for one player: the rules. A sheet of paper lies on a board of squares, its front and its back each
 * coloured square by square (a plain sheet, stripes, a frame...), some squares maybe cut out. A fold takes everything
 * on one side of a crease (a line between two rows or two columns of squares) and lays it over the other side, turned
 * over: its backs now show. Make the picture on the card: the same squares covered, each showing the same colour.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A fold must have paper on both sides of its crease, and what folds must land on the board. A pile of layers folded
 * shows, on top, the colour its bottom layer had underneath; what matters of a pile is only the colour it shows on top
 * and the colour it shows underneath (pfo_cell: up * 16 + down, PFO_NONE no paper), so a state is a cell a square.
 * The layers' count is kept for the picture only. A fold: PFO_FOLD(axis, k, side): axis 0 a crease between columns
 * k - 1 and k (side 0 the left part folds over to the right, 1 the right part over to the left), axis 1 between rows
 * (side 0 the top part folds down, 1 the bottom part up).
 * The par: the fewest folds, by a breadth-first search over the states (each fold makes the paper narrower or lower:
 * the search is small). The puzzles: made by pfo_make (the levels' tool tools/paperfold_gen.c, offline, and at random
 * alike): a sheet of the spec's size, its front and back from symmetric patterns, symmetric cut-outs, then random
 * folds; the picture they leave, its par found by the search.
 * The text of a sheet: rows from the top split by '|', two characters a square, its front and its back ('a'..'f': the
 * colours, PFO_COLOURS), ".." cut out. A picture: one character a square ('.' no paper). The notation: "L2" the left
 * part over crease 2 (between columns 1 and 2), "R2" the right part over it, "T1" the top part down over crease 1, "B1"
 * the bottom part up; "h" a hint (the first fold of a shortest way, played), "=" all of it.
 */
#ifndef PAPERFOLD_H
#define PAPERFOLD_H

#include <stdint.h>

#define PFO_MAXW 6
#define PFO_MAXH 6
#define PFO_CELLS (PFO_MAXW * PFO_MAXH)
#define PFO_COLOURS 6
#define PFO_NONE 0xFF
#define PFO_HIST 64
#define PFO_LEVELS 150
#define PFO_CHAPTERS 6
#define PFO_VARIANTS 3
#define PFO_TEXT (3 * PFO_CELLS + PFO_MAXH + 2)
#define PFO_FOLD(axis, k, side) (((axis) * 8 + (k)) * 2 + (side))
#define PFO_AXIS(f) ((f) >> 4)
#define PFO_LINE(f) ((f) >> 1 & 7)
#define PFO_SIDE(f) ((f) & 1)
#define PFO_MOVES 32                            /* fold numbers 0..31 */

static inline int pfo_up(uint8_t c) { return c >> 4; }
static inline int pfo_down(uint8_t c) { return c & 15; }

typedef struct {
    uint8_t cell[PFO_CELLS];                    /* x + y * w: up * 16 + down, or PFO_NONE */
    uint8_t layers[PFO_CELLS];                  /* the picture's: how many layers lie there */
} pfo_state;

typedef struct {
    int w, h;
    pfo_state start;
    uint8_t target[PFO_CELLS];                  /* the colour each square must show, or PFO_NONE */
} pfo_map;

typedef struct {
    pfo_map m;
    pfo_state s;
    pfo_state hist[PFO_HIST];
    int nh, moves, par;
} pfo_game;

typedef struct {
    int w, h;
    int front, back;                            /* the patterns (pfo_make's): 0 plain .. */
    int cuts;                                   /* symmetric sets of squares cut out */
    int folds;                                  /* the random folds made at first */
    int colours;                                /* how many colours the patterns may use */
    int least;                                  /* the fewest squares of paper the folds leave (0: a fifth, 2 at least) */
} pfo_spec;

int  pfo_parse(pfo_map *m, const char *sheet, const char *picture);   /* 0: not a map */
void pfo_text(const pfo_map *m, char *sheet, char *picture);         /* PFO_TEXT bytes each */
void pfo_begin(pfo_game *g);
/* a fold from s: 1 and the state after it (out may be NULL), 0 not possible */
int  pfo_try(const pfo_map *m, const pfo_state *s, int fold, pfo_state *out);
int  pfo_move(pfo_game *g, int fold);           /* 1: folded (kept for undo), 0: refused */
int  pfo_undo(pfo_game *g);
int  pfo_solved(const pfo_map *m, const pfo_state *s);
/* the cells a fold lifts (bit c of the mask: cell c is in the flap) */
void pfo_flap(const pfo_map *m, const pfo_state *s, int fold, uint64_t *mask);
/* where a flap's cell lands */
int  pfo_image(const pfo_map *m, int fold, int c);
/* the fewest folds from s (0 solved, -1 never), the first one (may be NULL); *states the states seen (may be NULL) */
int  pfo_solve(const pfo_map *m, const pfo_state *s, int *first, int *states);
/* how many different shortest ways there are (up to cap): few is harder */
int  pfo_ways(const pfo_map *m, int cap);
void pfo_make(pfo_map *m, const pfo_spec *spec, uint32_t seed);
int  pfo_script(pfo_game *g, const char *moves);
typedef struct { const char *sheet, *picture; uint8_t par; } pfo_level_t;
extern const pfo_level_t pfo_levels[PFO_LEVELS];
int  pfo_level(pfo_game *g, int level);         /* 0: none */
void pfo_variant(int variant, pfo_spec *spec);
void pfo_random(pfo_game *g, int variant, uint32_t seed);

#endif
