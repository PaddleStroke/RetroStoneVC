/*
 * Magic Squares for one player: the rules. Numbers in a figure where every line adds up to the same target: a 3 x 3
 * or 4 x 4 square (its rows, its columns and its two diagonals), or a triangle (its three sides, corners shared).
 * Some numbers are given; the others wait in a tray, exactly the ones missing. Put each in its place so that every
 * line makes the target. Each puzzle has exactly one solution.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The figures: MSQ_TRI3 a triangle of 6 places (3 on a side), MSQ_TRI4 9 places (4 a side), MSQ_TRI5 12 (5 a side),
 * MSQ_SQ3 a 3 x 3 square, MSQ_SQ4 a 4 x 4 one. The places: the square's cells row by row; the triangle's corners
 * first (top, bottom left, bottom right), then each side's inner places from its first corner (side 0 top to bottom
 * left, side 1 bottom left to bottom right, side 2 bottom right to top). The maker: a magic filling by chance (the
 * triangles' and the 4 x 4's by a search over the numbers 1..N in a random order; the 3 x 3's by its formula round a
 * centre, any whole numbers; then sometimes every number times a factor plus a step, which keeps it magic), then
 * numbers taken away in a random order while the solution stays the only one (a full count). The variants (random
 * play): the small triangle, the 3 x 3, the big triangle, the 4 x 4. The levels: 150 of our own in six chapters
 * (tools/magicsquare_gen.c, magicsquare_levels.h).
 * The notation (script, the pictures, the tests): a place by its number a.. ("a" the first); "c=7" a tile of 7 from the
 * tray into place c (one there goes back), "c" its tile back to the tray, "c>e" moved (swapped), "!" a hint, "+n" the
 * next n places put right.
 */
#ifndef MAGICSQUARE_H
#define MAGICSQUARE_H

#include <stdint.h>

enum { MSQ_TRI3, MSQ_TRI4, MSQ_TRI5, MSQ_SQ3, MSQ_SQ4, MSQ_KINDS };
#define MSQ_PLACES 16
#define MSQ_LINES 10
#define MSQ_VARIANTS 4
#define MSQ_LEVELS 150
#define MSQ_CHAPTERS 6
#define MSQ_HIST 512

typedef struct {
    int kind, np, nl, target;
    int16_t val[MSQ_PLACES];                    /* the solution */
    uint8_t given[MSQ_PLACES];
    uint8_t len[MSQ_LINES], line[MSQ_LINES][5]; /* each line's places */
} msq_puzzle;

/* a make: the figure, how many numbers to take away at most (as many as keep one solution), and the numbers: 0 the
 * first ones (1..N), 1 a factor and a step at random (small), 2 larger (a 3 x 3 round any centre) */
typedef struct { int kind, blanks, spread; } msq_make;

typedef struct {
    msq_puzzle p;
    int ngap, gaps[MSQ_PLACES];
    int ntray;
    int16_t tray[MSQ_PLACES];                   /* sorted */
    int8_t at[MSQ_PLACES];                      /* gap i holds tray tile at[i], -1 none */
    int8_t hist[MSQ_HIST][MSQ_PLACES];
    int nh, moves, hints, mistakes, wrong_lines;
} msq_game;

void msq_shape(msq_puzzle *p, int kind);       /* the places and lines of a figure */
int  msq_magic(const msq_puzzle *p);            /* 1: every line of the solution makes the target, numbers distinct */
int  msq_count(const msq_puzzle *p, int limit, long *nodes);   /* the fillings of the gaps from the tray, to limit */
void msq_make_puzzle(const msq_make *m, uint32_t seed, msq_puzzle *p);
msq_make msq_variant_make(int variant);
/* the levels: "<kind>:<target>:" then the numbers place by place, comma separated, '?' before a missing one */
int  msq_encode(const msq_puzzle *p, char *out, int size);
int  msq_decode(const char *s, msq_puzzle *p);

void msq_load(msq_game *g, const msq_puzzle *p);
void msq_new(msq_game *g, int variant, uint32_t seed);
int  msq_gap_of(const msq_game *g, int place);
int  msq_holder(const msq_game *g, int t);
int  msq_tile_free(const msq_game *g, int t);
int  msq_value(const msq_game *g, int place);  /* as shown: given, a tile's, 0 empty */
int  msq_put(msq_game *g, int i, int t);        /* tile t into gap i (one there goes back) */
int  msq_take(msq_game *g, int i);
int  msq_move(msq_game *g, int i, int j);
int  msq_undo(msq_game *g);
int  msq_hint(msq_game *g);                     /* the gap put right, -1 none */
int  msq_line_state(const msq_game *g, int l);  /* 0 not full, 1 full and right, -1 full and wrong */
int  msq_solved(const msq_game *g);
int  msq_script(msq_game *g, const char *moves);

#endif
