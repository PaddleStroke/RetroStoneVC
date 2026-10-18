/*
 * Balance Scales for one player: the rules. A few shapes, each of an unknown weight (a whole number from 1); scales,
 * each balanced, with shapes and numbered weights on their pans. Find every shape's weight. Each puzzle has exactly
 * one solution, found by reasoning: a scale holding one kind of shape still unknown gives its weight (the known ones
 * put in), and the harder ones need two scales compared (what one has more than the other weighs the difference).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A scale is an equation: the left pan's shapes and weight against the right pan's. The solver reasons as a person:
 * the "plain" step (one unknown shape left on a scale: its count on the heavier side against the rest), the "pair"
 * step (two scales added or taken one from the other leave one unknown shape). The maker deals the weights and the
 * scales by chance and keeps a puzzle when the scales pin every weight down (their rank is the shapes' count) and
 * the solver finds them all with the steps its make allows. The variants (random play): easy, medium, hard. The levels:
 * 150 of our own in six chapters (tools/balance_gen.c, balance_levels.h).
 * The player's guesses: a weight for each shape (0: none yet). A scale whose shapes all have a guess tilts to the
 * heavier side when the guesses do not balance it. Solved when every guess is right.
 * The notation (script, the pictures, the tests): "a=3" shape a weighs 3 (a.. the shapes in order), "a" its guess
 * taken off, "h" a hint (a weight the reasoning finds, put right), "+n" the next n shapes put right.
 */
#ifndef BALANCE_H
#define BALANCE_H

#include <stdint.h>

#define BAL_SHAPES 5                            /* kinds of shapes at most */
#define BAL_SCALES 5
#define BAL_PAN 6                               /* shapes on a pan at most */
#define BAL_VARIANTS 3
#define BAL_LEVELS 150
#define BAL_CHAPTERS 6
#define BAL_HIST 256

typedef struct {
    uint8_t n[2];                               /* the shapes on each pan (left, right) */
    uint8_t s[2][BAL_PAN];                      /* their kinds */
    int16_t w[2];                               /* the numbered weight on each pan (0: none) */
} bal_scale;

typedef struct {
    int k, m;                                   /* the shapes, the scales */
    int16_t wt[BAL_SHAPES];                     /* the solution */
    bal_scale sc[BAL_SCALES];
} bal_puzzle;

/* a make: the shapes, the scales, the heaviest weight, shapes on both pans allowed, a pair step wanted (0 none, 1 at
 * least one), the shapes on a pan at most */
typedef struct { int k, m, wmax, both, pair, pan; } bal_make;

typedef struct {
    bal_puzzle p;
    int16_t guess[BAL_SHAPES];
    int16_t hist[BAL_HIST][BAL_SHAPES];
    int nh, moves, hints, mistakes, full_wrong; /* full_wrong: every shape guessed and some wrong (once counted) */
} bal_game;

/* a scale's tilt with these weights (0 on any shape: unknown): -1 left down, 1 right down, 0 level, 2 not known */
int  bal_tilt(const bal_scale *s, const int16_t *w);
int  bal_check(const bal_puzzle *p);            /* 1: every scale balanced by the solution */
int  bal_rank(const bal_puzzle *p);             /* the scales' rank (the shapes' count: one solution) */
/* the reasoning: 1 when its steps find every weight (*plain, *pair: how many of each kind) */
int  bal_reason(const bal_puzzle *p, int *plain, int *pair);
void bal_make_puzzle(const bal_make *m, uint32_t seed, bal_puzzle *p);
bal_make bal_variant_make(int variant);
/* the levels: "<weights>;<scale>|<scale>..." the weights comma separated; a scale "left/right", each pan its items
 * comma separated: a shape a.., or a number */
int  bal_encode(const bal_puzzle *p, char *out, int size);
int  bal_decode(const char *s, bal_puzzle *p);

void bal_load(bal_game *g, const bal_puzzle *p);
void bal_new(bal_game *g, int variant, uint32_t seed);
int  bal_set(bal_game *g, int shape, int w);    /* a guess (0: taken off); a move; 0 nothing changed */
/* a guess finished (all its digits typed): every shape guessed and some wrong is a mistake, once until a guess
 * changes */
void bal_judge(bal_game *g);
int  bal_undo(bal_game *g);
int  bal_hint(bal_game *g);                     /* the shape put right, -1 none */
int  bal_solved(const bal_game *g);
int  bal_most(const bal_puzzle *p);             /* the heaviest weight (the guesses' digits) */
int  bal_script(bal_game *g, const char *moves);

#endif
