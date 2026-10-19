/*
 * Fruit Drop for one player: the rules. A glass jar; a dropper moves along its top and lets a fruit fall. Two fruits
 * of the same kind that touch merge into the next kind (cherry, strawberry, grape, mandarin, lemon, apple, peach, pear,
 * coconut, melon, watermelon: eleven sizes); the merged fruit may touch its own kind and merge again (a chain). Two
 * watermelons merging are gone. A fruit that stays above the jar's rim line for 2 s ends the game. The next fruit is
 * shown; the dropper gives only the five smallest kinds.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The score: a merge of two fruits of size s (1..11) scores s (s + 1) / 2 (1, 3, 6 ... 66).
 * Two ways to play:
 * - Endless (the random tab): the fruits by chance (the game's own seeded numbers, puzrand.h), by the jar's width
 *   (wide, medium, narrow); the best score is kept by the UI. Overflowing ends it (never lost: the score counts).
 * - Levels (dropmerge_levels.h, made by tools/dropmerge_gen.c): a jar (its width, pegs fixed in it, fruits already in
 *   it), a fixed sequence of fruits (its length: the drops allowed) and a goal: make a fruit of a size, reach a score,
 *   make n fruits of a size, or a chain of n merges from one drop. The goal reached: solved. Overflowing, or the
 *   fruits used up with the jar at rest, fails it (restart).
 *
 * The physics: deterministic, in whole numbers (no floating point: the same on every device, so a game played again
 * from its record, the tests and the level maker all see the same thing). Units of DMJ_FP steps; the jar's inside is x
 * 0..width, the rim line y 0, the floor y DMJ_JH (y grows down). Time goes in ticks of 1/DMJ_HZ s. Each tick: gravity
 * and the speed move every fruit, DMJ_ITER passes push apart the fruits that overlap (by their masses, r squared) and
 * push them out of the walls, the floor and the pegs; the speed is then what each moved this tick, a small bounce
 * given back to the fruits that hit hard, the rubbing between them and on the glass taken off. A merged fruit grows to
 * its size over DMJ_GROW ticks (it pushes the others away gently) with a little hop. The world is at rest when nothing
 * has moved faster than a little for a while: its clock then only moves on.
 *
 * The notation (script, the pictures, the tests): "d<x>" a drop at x (units from the jar's left side, the fruit kept
 * inside), once the dropper is ready, then the world run until rest (or 1.5 s); "=n" n drops where the hint aims ("="
 * alone: to the end); "w<n>" n ticks; "r" a restart.
 */
#ifndef DROPMERGE_H
#define DROPMERGE_H

#include <stdint.h>

#define DMJ_FP 256                              /* steps in a unit */
#define DMJ_HZ 120                              /* ticks a second */
#define DMJ_SIZES 11
#define DMJ_DROPS 5                             /* the dropper gives sizes 1..5 */
#define DMJ_MAXB 96
#define DMJ_MAXPEG 12
#define DMJ_MAXPRE 24
#define DMJ_SEQ 120
#define DMJ_JH 470                              /* the jar's inside: the rim line (y 0) to the floor */
#define DMJ_TOP 70                              /* the glass rises this far above the rim line */
#define DMJ_SPAWN (-48)                         /* the dropper's fruit: its centre's height */
#define DMJ_WIDE 400                            /* the jars' widths (units) */
#define DMJ_ITER 8
#define DMJ_GROW 10                             /* ticks a merged fruit takes to grow */
#define DMJ_GAP 54                              /* ticks between two drops */
#define DMJ_OVER (2 * DMJ_HZ)                   /* above the rim this long: the end */
#define DMJ_SETTLE 180                          /* the notation's drop: the world run until rest, this long at most */
#define DMJ_EVENTS 64
#define DMJ_LEVELS 60
#define DMJ_CHAPTERS 4
#define DMJ_VARIANTS 3
#define DMJ_STAR2 900                           /* endless: 2 stars from this score, 3 from the next */
#define DMJ_STAR3 2200

extern const int dmj_radius[DMJ_SIZES + 1];     /* units, by size (1..11) */
extern const int dmj_widths[DMJ_VARIANTS];     /* the random tab's jars */

enum { DMJ_ENDLESS, DMJ_SIZE, DMJ_SCORE, DMJ_COUNT, DMJ_CHAIN, DMJ_GOALS };
enum { DMJ_PLAYING, DMJ_WON, DMJ_OVERFLOW, DMJ_OUT };   /* how a game stands */
enum { DMJ_E_MERGE, DMJ_E_DROP, DMJ_E_LAND, DMJ_E_GONE };

typedef struct {
    int32_t x, y, px, py, vx, vy, r, ang;       /* steps; px, py: a tick ago; ang: 1/65536 of a turn */
    int32_t born;                               /* the tick it was made (a merge: the squash; -1 there at the start) */
    uint16_t id;
    uint8_t size, fresh, hit;                   /* fresh: dropped, touching nothing yet */
    int16_t over;                               /* ticks above the rim line */
} dmj_ball;

typedef struct { int16_t x, y, r; } dmj_peg;    /* units */
typedef struct { uint8_t size; int16_t x, y; } dmj_pre;

typedef struct {
    uint8_t type, size, chain;
    int16_t x, y;                               /* sixteenths of a unit */
    int32_t tick, pts, force;
    uint16_t id;                                /* the fruit (a drop, a landing, a merge: the new one) */
} dmj_event;

/* a jar and its goal (a level, or endless) */
typedef struct {
    int goal, target, tsize;                    /* tsize: DMJ_COUNT's size */
    int width;                                  /* units */
    int npeg, npre, nseq;                       /* nseq 0: endless (fruits by chance) */
    dmj_peg peg[DMJ_MAXPEG];
    dmj_pre pre[DMJ_MAXPRE];
    uint8_t seq[DMJ_SEQ];
    uint32_t seed;
    int par;                                    /* a level: the fewest drops the maker's player needed */
} dmj_jar;

typedef struct {
    dmj_jar J;
    int nb;
    dmj_ball b[DMJ_MAXB];
    int32_t tick, calm, rest, end, end_tick, ready, last_drop;
    int drops, score, biggest, chain, best_chain, overmax;
    int still;                                  /* no merging (a test of the physics alone) */
    int made[DMJ_SIZES + 2];                    /* fruits made by merges, by size */
    int cur, next;                              /* the dropper's fruit and the next one (0: none left) */
    uint32_t rng;
    uint16_t serial;
    int nev;                                    /* events so far (the last DMJ_EVENTS kept, ev[i % DMJ_EVENTS]) */
    dmj_event ev[DMJ_EVENTS];
} dmj_world;

typedef struct {
    dmj_world w, w0;                            /* the game; the jar at the start (restart) */
    int hints, restarts, level;
} dmj_game;

/* ---- the world -------------------------------------------------------------------------------------------------- */
void dmj_build(dmj_world *w, const dmj_jar *j);   /* the jar at the start (its fruits settled) */
int  dmj_can_drop(const dmj_world *w);           /* the dropper is ready */
int  dmj_clamp(const dmj_world *w, int size, int x);   /* x (steps) kept so the fruit fits in the jar */
int  dmj_drop(dmj_world *w, int x);               /* x in steps; 1 dropped, 0 not now */
void dmj_step(dmj_world *w);                      /* one tick */
void dmj_run(dmj_world *w, int ticks);            /* ticks (at rest: the clock only) */
void dmj_settle(dmj_world *w, int most);          /* until rest, or the game over, or most ticks */
int  dmj_goal_left(const dmj_world *w);           /* how far from the goal (0 reached; endless: 0) */
int  dmj_danger(const dmj_world *w);              /* the longest a fruit has stayed above the rim (ticks) */
/* where a fruit dropped at x (steps) would first touch something, falling straight: its centre's y (steps) and what
 * it touches (a ball's index, -1 the floor, -2 a peg) */
int32_t dmj_land(const dmj_world *w, int size, int x, int *what);

/* ---- a player ----------------------------------------------------------------------------------------------------- */
/* where to drop the dropper's fruit (steps): each place across tried, rated (a merge where it lands, its kind close
 * by, low in the jar, not burying a smaller one); noise: a random part of each rating (from *rng; 0 none) */
int  dmj_aim(const dmj_world *w, int noise, uint32_t *rng);
/* the maker's way of playing: a drop when the dropper is ready and the jar at rest (or DMJ_SETTLE ticks after the
 * last drop), at xs[i] if given (units), else where dmj_aim says (noise); to the end of the game or `most` drops. The
 * places dropped are written to out (units) when not NULL. Returns how the game ended (DMJ_PLAYING: stopped). */
int  dmj_play(dmj_world *w, const int *xs, int nxs, int noise, uint32_t *rng, int most, int *out);

/* ---- a game ---------------------------------------------------------------------------------------------------- */
void dmj_endless(dmj_game *g, int variant, uint32_t seed);
int  dmj_level(dmj_game *g, int level);           /* 0: no such level */
int  dmj_parse(dmj_jar *j, int goal, int target, int tsize, int width, const char *seq, const char *pegs,
               const char *pre);                  /* 0: bad data */
void dmj_restart(dmj_game *g);
int  dmj_script(dmj_game *g, const char *moves);
int  dmj_stars(const dmj_game *g);                /* a level: drops against par; endless: by score */

typedef struct {
    uint8_t goal, tsize;
    uint16_t target, width, par;
    const char *seq, *pegs, *pre, *way;         /* the fruits ('1'..'5'); pegs "x y r ..."; fruits in "s x y ..."; the
                                                 * maker's way: its drops' places, "x x ..." (units) */
} dmj_level_def;
extern const dmj_level_def dmj_levels[DMJ_LEVELS];
int  dmj_load(dmj_jar *j, const dmj_level_def *d);
int  dmj_chapter_of(int level);

#endif
