/*
 * Cross Math for one player: the rules. A crossword of sums: equations across and down ("4 + 3 = 7", "2 x 6 - 5 = 7")
 * crossing on their numbers. Some numbers are missing; the tray holds exactly the missing ones. Put every tile in its
 * place so that every equation is true. Each puzzle has exactly one solution, found by reasoning alone.
 * Equations of three numbers (a op b op c = d) are worked out in the usual order: times and divided first, then plus
 * and minus, from the left. Every number is a whole number from 1 up; a division comes out exact; nothing on the way
 * goes below zero.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The maker: equations laid one by one across a free board, each new one crossing a number of the ones laid (no two
 * side by side, as in a crossword), its numbers drawn at random within the make's ranges; then numbers taken away
 * (into the tray) while reasoning alone still finds every one: an equation with one number missing (the one tile of
 * the tray that makes it true); a gap only one tile of the tray can fill (every equation through it still possible);
 * a tile that only fits one gap. The grade: the steps of the second and third kind (the "medium" ones).
 * The variants (random play): easy (plus and minus), medium (the four operations), hard (bigger, equations of three
 * numbers too). The levels: 150 of our own in six chapters (tools/mathcross_gen.c, mathcross_levels.h).
 * Cells: x + y * w from the top left. The notation (script, the pictures, the tests): a cell is its column letter a..
 * and its row 1.. from the top; "c4=12" a tile of 12 from the tray into c4 (one there goes back to the tray), "c4" the
 * tile in c4 back to the tray, "c4>e2" the tile in c4 moved to e2 (swapped with one there), "h" a hint, "+n" the next
 * n gaps (reading order) put right.
 */
#ifndef MATHCROSS_H
#define MATHCROSS_H

#include <stdint.h>

#define MCR_MAXW 13
#define MCR_MAXH 13
#define MCR_CELLS (MCR_MAXW * MCR_MAXH)
#define MCR_EQS 24                              /* equations at most */
#define MCR_GAPS 48                             /* missing numbers at most */
#define MCR_VARIANTS 3
#define MCR_LEVELS 150
#define MCR_CHAPTERS 6
#define MCR_HIST 512

enum { MCR_EMPTY, MCR_NUM, MCR_ADD, MCR_SUB, MCR_MUL, MCR_DIV, MCR_EQ };   /* a cell */

typedef struct {
    int len;                                    /* its cells: 5 (a op b = c) or 7 (a op b op c = d) */
    int cell[7];
} mcr_eq;

typedef struct {
    int w, h;
    uint8_t kind[MCR_CELLS];                    /* MCR_EMPTY .. MCR_EQ */
    int16_t val[MCR_CELLS];                     /* a number cell's number (the solution's) */
    uint8_t gap[MCR_CELLS];                     /* 1: missing (a tile of the tray goes there) */
} mcr_puzzle;

/* a make: the board's size at most, the equations wanted, the operations (bit MCR_ADD..MCR_DIV), the largest number
 * in a sum or a difference, the largest factor, the share of three-number equations (in hundredths), the share of
 * numbers to take away (in hundredths, as many as reasoning allows up to it) */
typedef struct {
    int w, h, eqs;
    unsigned ops;
    int most, factor, three, gaps;
} mcr_make;

typedef struct {
    mcr_puzzle p;
    int neq;
    mcr_eq eq[MCR_EQS];
    int ngap, gaps[MCR_GAPS];                   /* the gaps' cells, in reading order */
    int ntray;
    int16_t tray[MCR_GAPS];                     /* the tiles, sorted */
    int8_t at[MCR_GAPS];                        /* gap i holds tray tile at[i] (-1 none) */
    int8_t hist[MCR_HIST][MCR_GAPS];            /* undo: at[] before each action */
    int nh, moves, hints, mistakes, wrong_eqs;  /* wrong_eqs: the full equations wrong now */
} mcr_game;

int  mcr_parse(mcr_puzzle *p, mcr_eq *eq);     /* the equations of a board; their count (0: none, or one broken) */
/* an equation with these numbers in its number cells, left to right (or top down): 1 true, 0 false */
int  mcr_true(const mcr_puzzle *p, const mcr_eq *e, const int *nums);
/* k numbers (2 or 3), k-1 operations (MCR_ADD..MCR_DIV), the usual order: 1 and the result in *res when every step
 * gives a whole number not below zero (a division exact) */
int  mcr_eval(const int *nums, const int *ops, int k, int *res);

/* the reasoning: the steps it took (*medium: those not of the first kind); 1 when it fills every gap */
int  mcr_reason(const mcr_puzzle *p, int *medium);
int  mcr_count(const mcr_puzzle *p, int limit); /* the ways to fill the gaps with the tray's tiles, up to limit */
void mcr_make_puzzle(const mcr_make *m, uint32_t seed, mcr_puzzle *p);
mcr_make mcr_variant_make(int variant);

/* the levels (mathcross_levels.h): "<w>x<h>:" a character a cell ('.' empty, '#' a number, + - * / =), then ':' and
 * the numbers in reading order, comma separated, a missing one after a '?' */
int  mcr_encode(const mcr_puzzle *p, char *out, int size);
int  mcr_decode(const char *s, mcr_puzzle *p);

int  mcr_load(mcr_game *g, const mcr_puzzle *p);   /* 1: a puzzle with gaps */
void mcr_new(mcr_game *g, int variant, uint32_t seed);
int  mcr_gap_of(const mcr_game *g, int cell);  /* the gap at a cell, -1 none */
int  mcr_tile_free(const mcr_game *g, int t);  /* 1: tray tile t is in the tray */
int  mcr_holder(const mcr_game *g, int t);     /* the gap holding tile t, -1 none */
int  mcr_value(const mcr_game *g, int cell);   /* a number cell's number as shown: given, a tile's, 0 a gap left */
/* actions (each one undoable, a move): tile t into gap i (one there goes back; t taken from where it was), the tile
 * of gap i back to the tray, the tile of gap i moved to gap j (swapped); 0: nothing done */
int  mcr_put(mcr_game *g, int i, int t);
int  mcr_take(mcr_game *g, int i);
int  mcr_move(mcr_game *g, int i, int j);
int  mcr_undo(mcr_game *g);
int  mcr_hint(mcr_game *g);                     /* the gap put right, -1 none */
int  mcr_eq_state(const mcr_game *g, int e);    /* 0 not full, 1 full and true, -1 full and false */
int  mcr_solved(const mcr_game *g);
int  mcr_script(mcr_game *g, const char *moves);

#endif
