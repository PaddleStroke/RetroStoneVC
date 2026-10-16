/*
 * Number Puzzles for one player: the rules. A few numbers on tiles and a target. Take two tiles and an operation (+,
 * -, x, /): they merge into one tile holding the result. Three games:
 *   - Four numbers: four tiles, the target (24 at first): every tile used, one tile left showing the target;
 *   - Target: six tiles (small numbers and a few big ones), a target of three digits: reach it, or as near as you can;
 *     only whole numbers above zero (a division must come out exact); tiles may be left over;
 *   - All five: five tiles, a target: every tile used, as in Four numbers.
 * Four numbers and All five work in fractions: any result is kept (a fraction, below zero), only a division by zero
 * is refused. The early levels can all be done in whole numbers; later ones need a fraction on the way.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The solver: a full search over the pairs and the operations (Four numbers 4 tiles, All five 5: every way; Target
 * with whole numbers, results equal to an operand left out: they never help when tiles may be left over). "Nice"
 * ways keep every result a whole number not below zero; the hint takes one when there is one. Target's difficulty is
 * the fewest operations to reach it exactly; the others' the ways there are (nice ones first).
 * The variants (random play): the three games, each easy, medium or hard (variant = game * 3 + tier). The levels:
 * 450 of our own, 150 a game in three chapters each, made by tools/mathpz_gen.c (mathpz_levels.h: the numbers and
 * the target; the tests check every one can be done, and how).
 * Tiles are slots a.. that keep their place; a merge "a+b" takes tile a into tile b (the result in b's slot; a - b,
 * a / b in that order). The notation (script, the pictures, the tests): "a+b c*b" ("x" for times, ":" for divided
 * too), "h" a hint (one step of a way there, played; an undo when no way is left), "=" every step of a way (solved),
 * "!" Target's "done" (as near as it gets), "u" an undo.
 */
#ifndef MATHPZ_H
#define MATHPZ_H

#include <stdint.h>

enum { NPZ_FOUR, NPZ_TARGET, NPZ_FIVE, NPZ_GAMES };
enum { NPZ_ADD, NPZ_SUB, NPZ_MUL, NPZ_DIV, NPZ_OPS };
#define NPZ_TIERS 3
#define NPZ_VARIANTS (NPZ_GAMES * NPZ_TIERS)
#define NPZ_MAX 6                               /* tiles at most */
#define NPZ_PER_CHAPTER 50
#define NPZ_CHAPTERS 9                          /* three a game */
#define NPZ_LEVELS (NPZ_PER_CHAPTER * NPZ_CHAPTERS)
#define NPZ_NEAR 10                             /* Target: "done" within this of it */

typedef struct { int32_t n, d; } npz_q;         /* a fraction, d > 0, in its lowest terms */
typedef struct { int8_t a, op, b; } npz_step;   /* tile a op tile b, the result in b's slot */
typedef struct { npz_step s; npz_q va, vb; uint8_t mb; } npz_past;   /* a merge, for undo: the two tiles before */

typedef struct {
    int kind;                                   /* NPZ_FOUR, NPZ_TARGET, NPZ_FIVE */
    int n;                                      /* the slots */
    int target;
    npz_q v[NPZ_MAX];
    uint8_t live[NPZ_MAX];                      /* the slot holds a tile */
    uint8_t made[NPZ_MAX];                      /* the tile is a result (not one given) */
    uint8_t start[NPZ_MAX];                     /* the numbers given */
    npz_past hist[NPZ_MAX];
    int nh;
    int done;                                   /* Target: "done" said (solved, as near as it got) */
    int hints, mistakes;                        /* a mistake: every tile used and the last one wrong */
} npz_game;

/* the ways a puzzle can be done (the generator's and the tests' measure) */
typedef struct {
    long ways;                                  /* Four numbers, All five: every way (orders counted) */
    long nice;                                  /* those keeping every result a whole number >= 0 */
    long plain;                                 /* nice ones without a division */
    int fewest;                                 /* Target: the fewest operations reaching it (0: none within 5) */
    long ways_fewest;                           /* Target: the ways with that many */
} npz_rating;

int  npz_tiles(int kind);                       /* 4, 6, 5 */
/* a op b, as the game allows it (Target: whole numbers above zero, exact divisions); 0: refused */
int  npz_apply(int kind, npz_q a, int op, npz_q b, npz_q *r);
void npz_load(npz_game *g, int kind, const int *nums, int target);
int  npz_merge(npz_game *g, int a, int op, int b);   /* 0: refused (an empty slot, a division by zero...) */
int  npz_undo(npz_game *g);                     /* 0: nothing to undo; Target's "done" first */
int  npz_left(const npz_game *g);               /* the tiles left */
int  npz_nearest(const npz_game *g);            /* Target: the tile nearest the target (its slot), -1 none */
int  npz_off(const npz_game *g);                /* Target: how far the nearest tile is (0: there) */
int  npz_solved(const npz_game *g);
int  npz_stuck(const npz_game *g);              /* Four numbers, All five: one tile left, not the target */
int  npz_can_finish(const npz_game *g);         /* Target: "done" allowed now (not there, within NPZ_NEAR) */
int  npz_finish(npz_game *g);
int  npz_stars(const npz_game *g);              /* solved: 1..3 */
/* the next step of a way from here (nice ones first); 0: no way from here */
int  npz_next(const npz_game *g, npz_step *s);
/* a hint: the next step played (1), else the last merge undone (2); 0: nothing to do. Counted. */
int  npz_hint(npz_game *g, npz_step *s);
void npz_rate(int kind, const int *nums, int target, npz_rating *r);
/* a puzzle made at random for a variant (game * 3 + tier), from a seed; always possible */
void npz_make(int variant, uint32_t seed, int *nums, int *target);
void npz_new(npz_game *g, int variant, uint32_t seed);
int  npz_script(npz_game *g, const char *moves);
/* the designed levels (0..NPZ_LEVELS-1): game, numbers, target */
int  npz_level_kind(int level);
int  npz_level(npz_game *g, int level);
int  npz_level_nums(int level, int *nums, int *target);   /* the tiles' count; 0: none */

#endif
