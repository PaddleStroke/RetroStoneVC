/*
 * Paths for one player: the rules (a classic pencil puzzle of joining pairs). Pairs of dots of one colour on a square
 * grid. Draw a path between the two dots of each pair, from square to square (up, down, left, right); two paths never
 * cross nor share a square. Solved when every pair is linked and the paths fill the whole grid.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 5 x 5, 7 x 7, 9 x 9, 12 x 12 (3 to 5, 5 to 8, 7 to 10, 10 to 13 pairs).
 * The maker: the grid filled by paths grown at random (from a square, a path grows at either end into free squares,
 * never touching itself: a square of a path is next to no other square of it than the ones before and after it, so no
 * path can take a short cut), the short ones joined to their neighbours (or to a piece of a path cut for them), more
 * paths than wanted. Kept when the solver's deductions alone solve it, without a guess (so the solution is the only
 * one). Then paths joined end to end (or one's end moved onto another's piece, when no join can be made), each change
 * kept only if the deductions still solve the puzzle alone, down to as many pairs as wanted.
 * The solver (nk_count): the links between neighbouring squares and the colours each square may take. The deductions:
 * a dot has one link, any other square two; linked squares share a colour, two squares that can share none are not
 * linked; no loop; each colour's dots must be joined through squares that may take it, and a square every such way
 * passes through takes it. Then guesses (a path's open end: which way it goes on), each followed by the deductions.
 * The player's paths: each colour's path starts at one of its dots. A stroke begins on a dot (the path starts again
 * from it) or on a square of a path (the path is cut back to it), then goes square by square: onto a free square, onto
 * another path's square (that path is cut there; it comes back if the stroke backs off), back along itself (cut back),
 * or onto its other dot (the pair is linked; it can go no further). Never onto another colour's dot. A stroke is one
 * action (undo takes it back).
 * Squares: x + y * n from the top left. The notation (script, the screenshots, the tests): a square is its column
 * letter a.. and its row 1.. from the top; "a1-a4-c4" is a stroke from a1, straight on to a4, then to c4; "?" a hint
 * (one pair's path laid as in the solution); "=n" lays the next n pairs that are not right, the same way.
 */
#ifndef PATHS_H
#define PATHS_H

#include <stdint.h>

#define NK_VARIANTS 4
#define NK_MAX 12
#define NK_CELLS (NK_MAX * NK_MAX)
#define NK_COLOURS 14
#define NK_HIST 600

typedef struct { uint8_t len[NK_COLOURS], cells[NK_CELLS]; } nk_snap;   /* every path, one after another */

typedef struct {
    int n, pairs;
    uint8_t end[NK_COLOURS][2];                 /* each pair's dots */
    uint8_t dot[NK_CELLS];                      /* the colour + 1 of the dot on a square (0: none) */
    uint8_t slen[NK_COLOURS], spath[NK_COLOURS][NK_CELLS];   /* the solution: each path from its first dot */
    uint8_t len[NK_COLOURS], path[NK_COLOURS][NK_CELLS];     /* the paths drawn, each from one of its dots */
    uint8_t own[NK_CELLS];                      /* the colour + 1 on each square now (a path, a dot; 0: free) */
    int stroke;                                 /* the colour being drawn, or -1 */
    nk_snap before;                             /* the paths when the stroke began */
    nk_snap hist[NK_HIST];
    int nh, moves;
} nk_game;

void nk_variant(int variant, int *n, int *lo, int *hi);   /* its size, and the pairs it may have */
/* the solutions that fill the grid (n x n, pairs dots: up to 32), counted up to limit; -1: more than budget steps
 * (a step: the deductions, then a guess; budget 1: the deductions alone) */
int  nk_count(int n, int pairs, const uint8_t (*end)[2], int limit, long budget);
/* a puzzle: the colour of each square in its one solution, each pair's dots, the pairs; returns the tries made */
int  nk_make(int variant, uint32_t seed, uint8_t *colour, uint8_t (*end)[2], int *pairs);

void nk_new(nk_game *g, int variant, uint32_t seed);
void nk_load(nk_game *g, int n, int pairs, const uint8_t *colour, const uint8_t (*end)[2]);
/* a stroke: begins on a square (its colour; -1: no stroke can start there), goes on towards a square (straight, one
 * square at a time, as far as it may: the squares gone), ends (1: the paths changed, one action) */
int  nk_begin(nk_game *g, int cell);
int  nk_toward(nk_game *g, int cell);
int  nk_end(nk_game *g);
int  nk_head(const nk_game *g, int colour);    /* the square a colour's path ends on (-1: none drawn) */
int  nk_linked(const nk_game *g, int colour);  /* 1: its path joins its two dots */
int  nk_right(const nk_game *g, int colour);   /* 1: its path is the solution's */
int  nk_free(const nk_game *g);                /* the squares no path covers */
int  nk_solved(const nk_game *g);
int  nk_hint(nk_game *g);                      /* a pair's path laid as in the solution: its colour; -1: none */
int  nk_undo(nk_game *g);                      /* the last action taken back; 0: none */
int  nk_cell(const nk_game *g, const char *s, int *used);   /* "c4": its square, and the characters read; -1 */
int  nk_script(nk_game *g, const char *moves); /* 0 at the first bad one */

#endif
