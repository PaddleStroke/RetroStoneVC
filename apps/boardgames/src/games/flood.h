/*
 * Flood for one player: the rules (no SDL). A square board of coloured cells; the region grown from the top left
 * corner (the cells of its colour joined to it, side by side) takes a colour chosen, and with it every cell of that
 * colour touching it. Flood the whole board in one colour within the move limit.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants (the random puzzles): 6 x 6 with 4 colours, 10 x 10 with 5, 14 x 14 with 6. The levels (flood_levels.h,
 * made by tools/flood_gen.c): 5 x 5 with 3 colours up to 14 x 14 with 6, each with the best way the maker found.
 * The par: the fewest moves the solver finds. It works on the board's patches (the cells of one colour joined side
 * by side): a move floods the patches of its colour touching the region. A move that floods every patch left of its
 * colour is always as good as any (it is played at once); otherwise a beam search keeps the most promising regions,
 * ranked by a bound on the moves left (the colours left, and the farthest patch's distance in patches), and an exact
 * search (iterative deepening on the same bound) proves the par on the small boards. The move limit: the par and a
 * fifth more (at least two more). The stars: ui_par_stars(moves, par); undo takes a move back.
 * The hint: the next move of the best way the solver finds from the board as it is (played, counted).
 * Cells: x + y * n from the top left; colours 0..k-1.
 */
#ifndef FLOOD_H
#define FLOOD_H

#include <stdint.h>

#define FLD_MAX 14
#define FLD_CELLS (FLD_MAX * FLD_MAX)
#define FLD_COLOURS 6
#define FLD_MOVES 96                            /* the most moves kept (a limit is never near it) */
#define FLD_LEVELS 150
#define FLD_CHAPTERS 6

enum { FLD_SMALL, FLD_MEDIUM, FLD_LARGE, FLD_VARIANTS };

typedef struct {
    int n, k;                                   /* n x n cells, k colours */
    uint8_t c[FLD_CELLS];
} fld_board;

typedef struct {
    fld_board start, b;                         /* as dealt; as it is now */
    uint8_t in[FLD_CELLS];                      /* 1: in the flooded region */
    uint8_t moves[FLD_MOVES];                   /* the colours chosen */
    int nmoves;
    int par, limit;                             /* the fewest moves known; the most allowed */
    uint8_t way[FLD_MOVES];                     /* the best way known from the start (par moves) */
    int hints;
} fld_game;

extern const char *const fld_levels[FLD_LEVELS];

void fld_variant(int variant, int *n, int *k);  /* a random puzzle's size and colours */
int  fld_limit(int par);                        /* the move limit for a par */
void fld_deal(fld_board *b, int n, int k, uint32_t seed);   /* cells by chance (every colour there) */
/* the region from the corner (in: 1 inside); its cells' count */
int  fld_region(const fld_board *b, uint8_t *in);
/* the region recoloured c, absorbing what touches it: the cells flooded now; 0: nothing changed (c its colour) */
int  fld_fill(fld_board *b, uint8_t *in, int c);
int  fld_flooded(const fld_board *b);           /* 1: one colour everywhere */
/* the best way the solver finds from b (its colours into out, at most max); its length. width: the beam's (the
 * bigger, the better and slower); exact: also an exact search of that many nodes at most (0: none), *proved 1 when
 * it proved the length the fewest (may be NULL) */
int  fld_solve(const fld_board *b, uint8_t *out, int max, int width, long exact, int *proved);
int  fld_greedy(const fld_board *b);            /* the moves of the plain greedy way (the most cells each move) */

/* the levels: "<n>:<k>:" the cells (a digit 0..k-1 each, row by row) ":" the best way (a digit a move) */
int  fld_encode(const fld_board *b, const uint8_t *way, int len, char *out, int size);   /* its length; 0: no room */
int  fld_decode(const char *s, fld_board *b, uint8_t *way, int *len);                   /* 1: a level */

void fld_new(fld_game *g, int variant, uint32_t seed);
int  fld_load(fld_game *g, const fld_board *b, const uint8_t *way, int len);   /* 1: the way floods it */
int  fld_level(fld_game *g, int level);         /* 0..FLD_LEVELS-1 */
int  fld_can(const fld_game *g, int c);         /* 1: choosing c now changes the board (and the limit allows) */
int  fld_play(fld_game *g, int c);              /* the cells flooded; 0: not a move */
int  fld_undo(fld_game *g);                     /* 0: nothing to undo */
int  fld_done(const fld_game *g);
int  fld_out(const fld_game *g);                /* 1: the limit reached, not flooded (undo) */
int  fld_hint(fld_game *g);                     /* the colour to choose; -1: none */
/* "3 1 2" (or "312"): the colours chosen, 1..k; "h" the hint followed; "u" an undo */
int  fld_script(fld_game *g, const char *s);

#endif
