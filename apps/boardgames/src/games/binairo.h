/*
 * Binairo for one player: the rules. Fill a square grid with ivory and ebony discs (0 and 1): never three of a colour
 * in a row or a column, as many of each colour in every row and every column, and no two rows alike, nor two columns.
 * Some discs are given. Every puzzle has one solution.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 6 x 6, 8 x 8, 10 x 10, 12 x 12, each easy, medium or hard (variant = size * 3 + level). A puzzle is
 * made from a seed: a full grid by chance (the solver's search, bench.h), then discs taken away as long as the
 * solver still solves it within its level. The ways of reasoning, by level:
 *   easy: two alike side by side, or two alike with a gap between them: the cells beside them (in the gap) the other
 *     colour; a line with all its discs of a colour: the rest the other colour;
 *   medium: a line seen whole: every way to fill it (its counts, no three alike, unlike every full line) agreeing on a
 *     cell;
 *   hard: chains (a disc tried, the easy ways followed to a contradiction).
 * The player's cells: empty, ivory or ebony (the given ones stay). Solved when the grid is full and keeps the rules.
 * Cells: x + y * n from the top left. The notation (script, the screenshots, the tests): a cell is its column letter
 * a.. and its row 1.. from the top; "c4" a tap (empty, ivory, ebony, empty...), ".c4" a press held (the other way
 * round), "c4-c9" a drag along a line (the cells as c4 was become as the tap made it), "h" a hint, "+n" the next n
 * cells (reading order) not yet as the solution has them put right.
 */
#ifndef BINAIRO_H
#define BINAIRO_H

#include <stdint.h>

#include "games/bench.h"

#define BN_SIZES 4
#define BN_VARIANTS (BN_SIZES * 3)
#define BN_MAX 12
#define BN_CELLS (BN_MAX * BN_MAX)
#define BN_LEVELS 150                           /* the designed levels (binairo_levels.h), in six chapters */
#define BN_CHAPTERS 6

enum { BN_EMPTY, BN_IVORY, BN_EBONY };         /* a cell (the player's, a given one) */

typedef struct {
    int n;
    uint8_t given[BN_CELLS];                    /* BN_EMPTY (to find), BN_IVORY, BN_EBONY */
} bn_puzzle;

typedef struct {
    bn_puzzle p;
    int level;
    uint32_t seed;
    uint8_t sol[BN_CELLS];                      /* BN_IVORY, BN_EBONY */
    uint16_t st[BN_CELLS];                      /* the grid now */
    bench_hist hist;
} bn_game;

int  bn_size(int variant);                      /* 6, 8, 10, 12 */
int  bn_level(int variant);

int  bn_grade(const bn_puzzle *p, uint8_t *sol); /* BENCH_* (the solution in sol when it solves it; may be NULL) */
int  bn_count(const bn_puzzle *p, int limit);
void bn_make(int n, int level, uint32_t seed, bn_puzzle *p, uint8_t *sol);
/* the levels (binairo_levels.h, made by tools/binairo_gen.c): a puzzle as text, "<n>:" and a character a cell, row
 * by row ('.' to find, '0' ivory, '1' ebony) */
int  bn_encode(const bn_puzzle *p, char *out, int size);   /* its length; 0: no room */
int  bn_decode(const char *s, bn_puzzle *p);    /* 1: a puzzle */
/* the work a puzzle asks of the solver: its grade (BENCH_*), the passes (each tier tried once round) and the
 * finds of the medium way */
int  bn_effort(const bn_puzzle *p, long *passes, long *mediums);

void bn_new(bn_game *g, int variant, uint32_t seed);
int  bn_load(bn_game *g, const bn_puzzle *p);   /* a puzzle given (a level), graded and solved; 1: it solves */
int  bn_free(const bn_game *g, int c);          /* 1: the player's to fill (not given) */
int  bn_tap_state(const bn_game *g, int c);
int  bn_hold_state(const bn_game *g, int c);
int  bn_paint(bn_game *g, const int *cells, int n, int state);
int  bn_hint(bn_game *g);
int  bn_undo(bn_game *g);
int  bn_triple(const bn_game *g, int c);        /* 1: in three alike in a row or a column */
int  bn_line_over(const bn_game *g, int way, int i);   /* a row (way 0) or column with too many of a colour */
int  bn_line_twin(const bn_game *g, int way, int i);   /* a full line alike another */
int  bn_count_line(const bn_game *g, int way, int i, int colour);
int  bn_solved(const bn_game *g);
int  bn_script(bn_game *g, const char *moves);

#endif
