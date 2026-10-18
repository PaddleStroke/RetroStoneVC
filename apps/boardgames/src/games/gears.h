/*
 * Gears for one player: the rules. A pegboard holds a motor's gear, always turning clockwise, target gears fixed on
 * their pegs (each must turn the way its arrow says), output shafts (later: an empty peg that must turn a given way
 * at a given speed, a ratio of the motor's) and empty pegs. Put gears from the tray (two or three sizes) on the empty
 * pegs so that every target turns as asked, with every gear put turning (driven by the motor) and nothing jammed.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board: a lattice w x h units, the pegs at whole points. A gear of size s (1 small, 2 medium, 3 large) has a
 * pitch circle of radius s units and 12 s teeth (stub teeth: their tips about 0.13 past it). Two gears
 * of sizes a and b on pegs d apart: meshed when d is exactly a + b, free when d >= a + b + 0.4 (clear by a margin),
 * else they collide: that gear cannot be put there. A gear must lie inside the board (its peg at least s from each
 * edge). Meshed gears turn opposite ways, at speeds inverse to their sizes (the motor's speed times its size over
 * theirs: a shaft's ratio is the motor's size over the gear's). A loop of an odd number of meshed gears jams the whole
 * train (none of it turns). A target is right when it turns in the motor's train its arrow's way (a shaft: with a gear
 * of its ratio's size on it); solved when every target is right, every gear put is in the motor's train and it is not
 * jammed.
 * The par: the fewest gears put, found by gea_solve: every train that can be built (a gear put only where it meshes
 * with the train already built, each set of placements met once), a jam pruned at once (more gears never free it).
 * The text of a board: the width and height as letters ('a' + n), the tray's sizes ('0' + its bits: bit s size s), then
 * four characters a peg: its kind ('M' the motor, 'T' / 't' a target turning clockwise / the other way, 'S' / 's' a
 * shaft, 'P' an empty peg), x and y as letters, the size (the motor's, the target's, the size a shaft's ratio asks
 * for; '0' an empty peg).
 * The notation: "c2" peg c (the pegs in their order in the text, 'a' first) gets a gear of size 2 ("c0" taken off),
 * "c>f" the gear on peg c moved to peg f, "h" a hint (the step played), "=" the rest of a shortest way played.
 */
#ifndef GEARS_H
#define GEARS_H

#include <stdint.h>

#define GEA_MAXW 24
#define GEA_MAXH 24
#define GEA_PEGS 24
#define GEA_SIZES 3
#define GEA_HIST 160
#define GEA_LEVELS 150
#define GEA_CHAPTERS 6
#define GEA_VARIANTS 3
#define GEA_TEXT (3 + 4 * GEA_PEGS + 1)

enum { GEA_PEG, GEA_MOTOR, GEA_TARGET, GEA_SHAFT };
enum { GEA_FREE, GEA_MESH, GEA_CLASH };         /* two gears on two pegs */
enum { GEA_OK, GEA_COLLIDES, GEA_OUTSIDE, GEA_TAKEN };   /* a gear put on a peg: allowed, or why not */

typedef struct {
    uint8_t x, y, kind, size;                   /* size: the motor's, a target's, the size a shaft asks for */
    int8_t dir;                                 /* a target's, a shaft's: +1 clockwise, -1 the other way */
} gea_peg;

typedef struct {
    int w, h, sizes;                            /* sizes: the tray's (bit s: size s) */
    int n;
    gea_peg p[GEA_PEGS];
} gea_map;

typedef struct { uint8_t g[GEA_PEGS]; } gea_state;   /* the gear on each peg: its size, 0 none */

typedef struct {
    int8_t dir[GEA_PEGS];                       /* +1 clockwise, -1 the other way, 0 still */
    int8_t from[GEA_PEGS];                      /* the gear that drives it (the train's tree; -1 the motor, none) */
    uint8_t train[GEA_PEGS];                    /* in the motor's train (jammed or not) */
    uint8_t ok[GEA_PEGS];                       /* a target or shaft turning as asked */
    int jam, solved, put, idle;                 /* put: gears put; idle: put but not driven */
} gea_eval_t;

typedef struct {
    gea_map m;
    gea_state s;
    gea_state hist[GEA_HIST];
    int nh, par;
} gea_game;

typedef struct {
    int w, h;                                   /* the board */
    int sizes;                                  /* the tray's (bits 1..3) */
    int motor, tsizes;                          /* the sizes the motor and the targets may have */
    int targets, shafts;                        /* how many */
    int chain;                                  /* the gears a target's chain takes at least (1..) */
    int decoys;                                 /* the empty pegs added beside the chains */
    int sym;                                    /* the pegs mirrored left to right */
    int lo, hi;                                 /* the par aimed at */
} gea_spec;

int  gea_parse(gea_map *m, const char *text);   /* 0: not a board */
void gea_text(const gea_map *m, char *out);     /* GEA_TEXT bytes */
int  gea_rel(const gea_map *m, int i, int a, int j, int b);   /* GEA_FREE / MESH / CLASH */
void gea_fresh(const gea_map *m, gea_state *s); /* the fixed gears only */
void gea_begin(gea_game *g);
int  gea_fits(const gea_map *m, const gea_state *s, int peg, int size);   /* GEA_OK, or why not */
int  gea_fixed(const gea_map *m, int peg);      /* 1: the motor or a target (a shaft is not) */
void gea_eval(const gea_map *m, const gea_state *s, gea_eval_t *e);
int  gea_solved(const gea_map *m, const gea_state *s);
int  gea_put(gea_game *g, int peg, int size);   /* size 0: taken off; 1 done (kept for undo) */
int  gea_shift(gea_game *g, int from, int to);  /* a gear moved to another peg in one step */
int  gea_undo(gea_game *g);
int  gea_count(const gea_map *m, const gea_state *s);   /* the gears put */
/* the fewest gears that solve it from the fixed gears alone (-1 never, or the search gave up after `limit` nodes; 0:
 * no limit); the placements (may be NULL); *nodes the search's nodes, *jams the trains it found jammed (may be NULL) */
int  gea_solve(const gea_map *m, gea_state *sol, long limit, long *nodes, long *jams);
/* how many solutions use the fewest gears (up to cap): few is harder */
int  gea_ways(const gea_map *m, int par, int cap);
/* the solution nearest to s (the fewest changes, then the fewest gears) and its first step: a gear to take off (size
 * 0) or one to put that meshes with the train; 0 none (solved, or none found) */
int  gea_hint(const gea_map *m, const gea_state *s, int par, int *peg, int *size);
void gea_make(gea_map *m, const gea_spec *spec, uint32_t seed, int *par);   /* par -1: none made */
int  gea_script(gea_game *g, const char *moves);
typedef struct { const char *text; uint8_t par; } gea_level_t;
extern const gea_level_t gea_levels[GEA_LEVELS];
int  gea_level(gea_game *g, int level);         /* 0: none */
void gea_variant(int variant, gea_spec *spec);
void gea_random(gea_game *g, int variant, uint32_t seed);

#endif
