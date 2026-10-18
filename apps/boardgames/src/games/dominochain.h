/*
 * Domino Chain for one player: the rules. A board of wooden squares on the green table holds a chain of standing
 * dominoes with gaps in it, from the start (a red domino over an arrow) to a brass bell. Put pieces from the stock on
 * free squares, then push: the start falls, and each piece that falls knocks over the next one in its way. Make one
 * push topple every piece on the board and ring the bell.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The squares: felt (nothing can stand there), wood (a piece can be put there). The pieces (given ones and the
 * stock's), each on one square; a piece hit by a falling piece that travels its way falls on that way (ways as
 * swipepad.h's: 0 up, 1 right, 2 down, 3 left; a piece's axis: way & 1):
 * - a domino stands across its axis: hit along it, it falls on and hits the next square; hit from the side it stays;
 * - a turn (a curve of small dominoes joining two sides of its square, corner r: sides r and r + 1): hit coming in by
 *   one of its sides, it falls out by the other (a quarter turn); hit otherwise it stays;
 * - a long domino (two squares long) falls like a domino but reaches two squares on: it hits the next square's piece
 *   if one stands there, else it reaches over that square (felt, empty wood, a fallen piece) to the one after;
 * - a splitter, hit from any side, falls both ways across the push (two chains go on);
 * - the start falls on its way at the push; the bell rings when anything hits it.
 * Time goes in steps: the pieces hit in one step fall in the next, in a fixed order (a piece hit twice in one step
 * falls once, by the first hit). The push is a success when every piece on the board has fallen and the bell rang.
 * The par: the fewest pieces from the stock, by a search over the chain as it goes (each wooden square the chain
 * reaches is left empty or given a piece that falls the way it is hit), deepening the pieces allowed one by one.
 * The text of a board: rows from the top split by '|': '#' felt, '.' wood, 'h' 'v' a domino falling across (a
 * horizontal chain) or along the board (a vertical chain), 'H' 'V' long dominoes, '1'..'4' a turn (corner 0..3: top
 * and right, right and bottom, bottom and left, left and top), '+' a splitter, 'U' 'R' 'D' 'L' the start and its way,
 * 'B' the bell. A stock: four digits, the dominoes, turns, long dominoes and splitters. Cells x + y * w from the top
 * left. The notation: a piece put "<kind><x><y><r>" (kind d t b s; x, y as letters from a; r the axis or the corner),
 * "x<x><y>" a piece taken back, "p" the push, "h" a hint (a piece put or taken back), "=" the hints to the end and the
 * push.
 */
#ifndef DOMINOCHAIN_H
#define DOMINOCHAIN_H

#include <stdint.h>

#define DMC_MAXW 8
#define DMC_MAXH 10
#define DMC_CELLS (DMC_MAXW * DMC_MAXH)
#define DMC_HIST 256
#define DMC_LEVELS 150
#define DMC_CHAPTERS 6
#define DMC_VARIANTS 3
#define DMC_TEXT (DMC_CELLS + DMC_MAXH + 1)
#define DMC_KINDS 4                             /* the stock's kinds */
#define DMC_FALLS (2 * DMC_CELLS)

enum { DMC_FELT, DMC_WOOD, DMC_DOMINO, DMC_TURN, DMC_LONG, DMC_SPLIT, DMC_START, DMC_BELL };
enum { DMC_K_DOMINO, DMC_K_TURN, DMC_K_LONG, DMC_K_SPLIT };

typedef struct {
    int w, h;
    uint8_t t[DMC_CELLS];                       /* DMC_FELT, DMC_WOOD or a given piece */
    uint8_t o[DMC_CELLS];                       /* a domino's axis (the way & 1 it falls), a corner, the start's way */
    uint8_t stock[DMC_KINDS];
    int start, bell;
} dmc_map;

typedef struct {
    uint8_t t[DMC_CELLS], o[DMC_CELLS];         /* the pieces put (t: DMC_WOOD none) */
    uint8_t left[DMC_KINDS];                    /* the stock left */
    int placed;
} dmc_state;

typedef struct {
    dmc_map m;
    dmc_state s;
    dmc_state hist[DMC_HIST];
    int nh, edits, par, hints, done;            /* done: the push toppled everything (solved) */
} dmc_game;

/* a push as it went: each fall (the piece's square, its step, the way it fell; a splitter falls both ways across:
 * its way is the push's), the hits that left a piece standing (the step, the square, the way), the bell's step */
typedef struct { uint8_t c, step, way; } dmc_event;
typedef struct {
    int n, nwob, steps, bell, ok;
    dmc_event fall[DMC_FALLS], wob[DMC_FALLS];
    uint8_t fallen[DMC_CELLS];
} dmc_trace;

typedef struct {
    int w, h;
    int runs;                                   /* straight runs of the chain */
    int gaps;                                   /* pieces taken out for the stock */
    int turns, longs, splits;                   /* how many of each the chain has (turns: at most) */
    int stock_turns, stock_longs, stock_splits; /* of these, taken out for the stock (at most) */
    int decoys;                                 /* wooden squares added round it (mirrored pairs) */
    int extra;                                  /* dominoes in the stock beyond the gaps */
} dmc_spec;

int  dmc_parse(dmc_map *m, const char *board, const char *stock);   /* 0: not a board */
void dmc_text(const dmc_map *m, const dmc_state *s, char *board);   /* s may be NULL: the given pieces alone */
void dmc_begin(dmc_game *g);
int  dmc_kind_of(int t);                        /* a piece's stock kind, -1 none */
/* the piece standing on square c (the given or the put one): its type, its axis or corner; DMC_WOOD / DMC_FELT none */
int  dmc_piece(const dmc_map *m, const dmc_state *s, int c, int *o);
int  dmc_free(const dmc_map *m, const dmc_state *s, int c);   /* 1: wood with nothing on it */
int  dmc_put(dmc_game *g, int c, int kind, int o);   /* 1: put (kept for undo), 0: refused */
int  dmc_take(dmc_game *g, int c);              /* a put piece taken back to the stock */
int  dmc_turn_piece(dmc_game *g, int c, int o); /* a put piece turned to o */
int  dmc_undo(dmc_game *g);
int  dmc_run(const dmc_map *m, const dmc_state *s, dmc_trace *tr);   /* 1: everything fell and the bell rang */
int  dmc_push(dmc_game *g, dmc_trace *tr);      /* the push: 1 solved (kept), 0 not */
/* the hits the push makes on empty wood (where a piece would be hit, and which way): bit c, and their ways */
int  dmc_open_hits(const dmc_map *m, const dmc_state *s, uint8_t *way);   /* how many; way[c] 0..3 or 255 */
/* the fewest stock pieces still to put with the pieces put kept (-1 none within `most` more, or the search gave up
 * past cap nodes); sol: the state then (its pieces in the chain's order: order[0..n-1] their squares) */
int  dmc_solve(const dmc_map *m, const dmc_state *s, int most, long cap, dmc_state *sol, int *order, long *nodes);
/* a hint: a piece to put (1, c, kind, o), or a put piece to take back (2, c); 0 none */
int  dmc_hint(const dmc_map *m, const dmc_state *s, int par, int *c, int *kind, int *o);
void dmc_make(dmc_map *m, dmc_state *sol, const dmc_spec *spec, uint32_t seed);   /* sol: the chain it was made from */
int  dmc_script(dmc_game *g, const char *moves);
typedef struct { const char *board, *stock; uint8_t par; } dmc_level_t;
extern const dmc_level_t dmc_levels[DMC_LEVELS];
int  dmc_level(dmc_game *g, int level);         /* 0: none */
void dmc_variant(int variant, dmc_spec *spec);
void dmc_random(dmc_game *g, int variant, uint32_t seed);

#endif
