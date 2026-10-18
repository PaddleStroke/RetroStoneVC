/*
 * Falling Dominoes for one player (the id stays "dominochain"): the rules. Dominoes stand on the wooden squares of a
 * board with gaps in it. Move some of them (a move: one piece picked up and put down on a free wooden square, or
 * turned where it stands), then push one domino: everything must fall, and the red trigger domino must fall last.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Ways as swipepad.h's: 0 up, 1 right, 2 down, 3 left; a piece's axis: way & 1 (1: it falls left or right). Time goes
 * in steps; a piece hit at step s by a piece travelling way d:
 * - a domino, the trigger: along its axis it falls at s and hits the next square at s + 1; across, it stays;
 * - a delay domino (an hourglass on it): along its axis it falls at s + 2 (hit again meanwhile: nothing more);
 * - a long domino: along its axis it falls and reaches two squares on: it hits the next square's piece if one stands
 *   there, else it reaches over that square (a gap, empty wood, a fallen piece) to the one after;
 * - a turn (a curve of small dominoes joining two sides of its square, corner r: sides r and r + 1): coming in by
 *   one of its sides it falls out by the other; otherwise it stays;
 * - a splitter falls both ways across the push (two chains go on);
 * - a stopper never falls: it stops the chain.
 * The pieces hit in a step fall in a fixed order: the delayed ones due, then the hits in the order they were made (a
 * piece hit twice falls once, by the first hit). The push: a domino, a delay domino, a long one or the trigger, along
 * its axis, falls at step 0. Solved: every piece but the stoppers fell, the trigger at a later step than all others.
 * The trigger never moves; every other piece may be moved or turned (dominoes: their axis; turns: a quarter).
 * The par: the fewest moves, by a breadth-first search over the positions (pieces of a kind are alike) that tries
 * every push in each. The board's text: rows from the top split by '|': '#' a gap, '.' wood, 'h' 'v' a domino falling
 * across the board or along it, 'X' 'Y' the trigger, 'd' 'e' a delay domino, 'l' 'i' a long one, '1'..'4' a turn
 * (corner 0..3: top and right, right and bottom, bottom and left, left and top), '+' a splitter, 'S' a stopper.
 * Cells x + y * w from the top left. The notation: "m<x><y><x><y>" a move (squares as letters from a), "r<x><y>" a
 * turn in place, "p<x><y><u r d l>" the push, "h" a hint (the next move of a shortest way, or the push), "=" all.
 */
#ifndef DOMINOCHAIN_H
#define DOMINOCHAIN_H

#include <stdint.h>

#define DMC_MAXW 8
#define DMC_MAXH 10
#define DMC_CELLS (DMC_MAXW * DMC_MAXH)
#define DMC_PIECES 16
#define DMC_HIST 256
#define DMC_LEVELS 100
#define DMC_CHAPTERS 5
#define DMC_VARIANTS 3
#define DMC_TEXT (DMC_CELLS + DMC_MAXH + 1)
#define DMC_FALLS (2 * DMC_CELLS)
#define DMC_SOL 8                               /* moves in a stored way at most */

enum { DMC_GAP, DMC_WOOD, DMC_DOMINO, DMC_TRIGGER, DMC_DELAY, DMC_LONG, DMC_TURN, DMC_SPLIT, DMC_STOPPER };
#define DMC_MOVE(from, to) ((from) * 128 + (to))   /* to == from: turned in place */
#define DMC_FROM(m) ((m) / 128)
#define DMC_TO(m) ((m) % 128)
#define DMC_PUSH(c, d) ((c) * 4 + (d))

typedef struct {
    int w, h;
    uint8_t t[DMC_CELLS], o[DMC_CELLS];         /* DMC_GAP, DMC_WOOD or a piece; its axis, its corner */
} dmc_state;

typedef struct {
    int n, nwob, steps, trigger, ok;            /* the trigger's step */
    struct { uint8_t c, step, way; } fall[DMC_FALLS], wob[DMC_FALLS];
    uint8_t fallen[DMC_CELLS];
} dmc_trace;

typedef struct {                                /* what the search learned about a position */
    int par;                                    /* the fewest moves (-1: none within the depth searched) */
    int goals;                                  /* positions at that depth a push solves */
    int firsts, good;                           /* the first moves possible, those that begin a shortest way */
    long states;
    int sol[DMC_SOL], push;                     /* a shortest way and its push */
} dmc_info;

typedef struct {
    dmc_state start;
    int par, nsol, push;                        /* a shortest way (sol[0..nsol-1]) and its push */
    int sol[DMC_SOL];
} dmc_map;

typedef struct {
    dmc_map m;
    dmc_state s;
    dmc_state hist[DMC_HIST];
    int nh, moves, par, hints, done;
} dmc_game;

typedef struct {
    int w, h;
    int runs, turns, splits, delays, longs, stoppers;   /* the chain laid at first: its runs, its special pieces */
    int scramble;                               /* moves made at random from the solved position */
    int depth;                                  /* the search's depth */
} dmc_spec;

int  dmc_parse(dmc_state *s, const char *board);   /* 0: not a board */
void dmc_text(const dmc_state *s, char *board);
int  dmc_movable(const dmc_state *s, int c);    /* 1: a piece that may be moved or turned */
extern int dmc_frozen;                          /* kinds kept still (bit t; the levels' tool asks what a kind is for) */
int  dmc_can_move(const dmc_state *s, int m);   /* 1: a legal move */
void dmc_apply(dmc_state *s, int m);            /* (a legal move) */
int  dmc_can_push(const dmc_state *s, int p);
int  dmc_run(const dmc_state *s, int p, dmc_trace *tr);   /* 1: solved by that push (tr may be NULL) */
int  dmc_solving_push(const dmc_state *s);      /* a push that solves it, -1 none */
/* the search from s to `depth` moves (cap: positions at most); 1 found */
int  dmc_search(const dmc_state *s, int depth, long cap, dmc_info *info);
/* 1: the obvious play solves it within depth moves (after a push, a piece the chain rocked turned, or a piece left
 * standing carried to where the chain stopped): such a board needs no idea */
int  dmc_obvious(const dmc_state *s, int depth);
void dmc_begin(dmc_game *g);
int  dmc_move(dmc_game *g, int m);              /* 1: moved (kept for undo) */
int  dmc_undo(dmc_game *g);
int  dmc_push(dmc_game *g, int p, dmc_trace *tr);   /* 1: solved (kept) */
/* a hint: 1 a move (*m), 2 the push (*p), 3 the last move to take back, 0 none */
int  dmc_hint(dmc_game *g, int *m, int *p);
int  dmc_make(dmc_state *s, const dmc_spec *spec, uint32_t seed);   /* a solved chain scrambled; 0 failed */
int  dmc_script(dmc_game *g, const char *moves);
int  dmc_set(dmc_game *g, const char *board, int par, const char *sol);   /* a level from its text */
typedef struct { const char *board; uint8_t par; const char *sol; } dmc_level_t;
extern const dmc_level_t dmc_levels[DMC_LEVELS];
int  dmc_level(dmc_game *g, int level);         /* 0: none */
void dmc_variant(int variant, dmc_spec *spec);
void dmc_random(dmc_game *g, int variant, uint32_t seed);
void dmc_notation(const dmc_state *s, int m, char *out);   /* a move as "m...." or "r..", 6 bytes */

#endif
