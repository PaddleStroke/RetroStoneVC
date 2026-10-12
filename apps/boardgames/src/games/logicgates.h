/*
 * Logic Gates for one player: the rules. A circuit on a board: switches along the bottom, logic gates above them,
 * lamps at the top; wires carry the signal (0 or 1) upwards. Set the switches so that every lamp is lit, while the
 * alarm lamps (when a level has one) stay dark. 150 levels in 6 chapters of 25, every one of them our own.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The parts:
 * - a switch: 0 or 1, flipped by a tap; a locked switch (a brass bar screwed over it) cannot be flipped;
 * - the gates: NOT (its input reversed), AND (1 when all its inputs are 1), OR (when any is), XOR (when an odd number
 *   of them are: two inputs, exactly one), NAND (not AND), NOR (not OR); NOT has one input, the others two or three;
 * - a route switch: one input, two outputs (left and right); its input goes on along the way it is set to, the other
 *   way carries 0; a tap sets it the other way;
 * - a wire with a fuse can be cut by a tap: then it carries 0 beyond the fuse (it is disconnected). A cut stays cut
 *   (only undo or a restart mends it); a level may allow fewer cuts than it has fuses;
 * - a lamp: lit when its wire carries 1. Solved when every lamp is lit and every alarm lamp is dark.
 * A move: one flip of a switch or a route switch, or one cut. The par: the fewest moves that solve the level (found by
 * trying every setting: the switches, the route switches, and the sets of cuts within the level's limit). Stars: 3 when
 * solved at par, 2 within two moves of it, else 1; a hint used in the attempt keeps it to 2. The move counter counts
 * the moves still made (undo takes one back; a restart clears them).
 *
 * The level format (the levels: logicgates_levels.h): tokens separated by spaces.
 * - a part: kind@cell, then its inputs in brackets: "AND@c3(b2,d2)". The cell: its column a.. from the left, its row
 *   1.. from the bottom (switches on row 1). The kinds: s (a switch at 0), S (at 1), k / K (locked at 0 / 1), NOT,
 *   AND, OR, XOR, NAND, NOR, y / Y (a route switch set to the left / to the right), L (a lamp), A (an alarm lamp).
 * - an input: the cell of the part it comes from; a route switch's outputs: "c2<" (its left) and "c2>" (its right);
 *   then an x for a wire with a fuse: "OR@d4(c3x,e2>x)".
 * - "%n": the cuts allowed (else as many as there are fuses).
 * Every wire goes up (from a lower row); every output feeds at least one input; the drawing's wires never overlap
 * (lg_route checks it: each wire rises from its source, runs along a track in the gap under its destination's row,
 * and rises into it).
 *
 * The notation (script, the screenshots, the tests): "L12" opens level 12 (1..150, chapter after chapter); "P3"
 * shows the level grid at chapter 3; "s3" flips switch 3, "r2" route switch 2, "c5" cuts fuse 5 (numbered from 1:
 * the switches and the route switches in reading order from the bottom left, row by row; the fuses by the part they
 * lead into, in the same order, then from the left); "u" undoes, "x" restarts the level, "h" asks a hint (the screen
 * shows it blinking; lg_script plays it); "=" plays the level's best solution; "+n" marks levels 1..n solved (3
 * stars), "+n:k" with k stars (the screen only).
 */
#ifndef LOGICGATES_H
#define LOGICGATES_H

#include <stdint.h>

#define LG_COLS 7
#define LG_ROWS 8
#define LG_NODES 40
#define LG_WIRES 64
#define LG_VARS 16                              /* the most parts a level lets the player change */
#define LG_TRACKS 4                             /* the most tracks in the gap between two rows */
#define LG_CHAPTERS 6
#define LG_PER_CHAPTER 25
#define LG_LEVELS (LG_CHAPTERS * LG_PER_CHAPTER)
#define LG_HIST 256
#define LG_NH 0.5f                              /* a part's height, rows (the rest of a row: the gap with the tracks) */
#define LG_HALF 0.34f                           /* half a part's width, columns */

enum { LG_SWITCH, LG_NOT, LG_AND, LG_OR, LG_XOR, LG_NAND, LG_NOR, LG_ROUTE, LG_LAMP, LG_ALARM, LG_KINDS };
enum { LG_MSWITCH, LG_MROUTE, LG_MCUT };        /* a move's kind */

typedef struct {
    uint8_t kind, col, row, nin;
    uint8_t locked;                             /* a switch the player cannot flip */
    uint8_t init;                               /* a switch's value, a route switch's way (0 left, 1 right) */
    int8_t in[3];                               /* the wires into it, from the left */
} lg_node;

typedef struct {
    uint8_t from, port, to, slot;               /* port: 0 the output, 1 a route switch's left, 2 its right */
    uint8_t cuttable;
    int net;                                    /* from * 3 + port: the wires carrying one signal */
    /* the drawing, in grid units: x columns from the left, y rows up from the bottom */
    int npt;
    float px[4], py[4];
    float len;                                  /* its length */
    float fs, fx, fy;                           /* the fuse: how far along, its centre */
    int fvert;                                  /* 1: the fuse lies on a vertical stretch */
} lg_wire;

typedef struct {
    int cols, rows, nn, nw, cut_limit;
    lg_node n[LG_NODES];                        /* in reading order from the bottom left (so inputs come first) */
    lg_wire w[LG_WIRES];
    int nsw, sw[LG_COLS * LG_ROWS];             /* the switches, routes and fuses in the notation's order */
    int nrt, rt[LG_NODES];
    int ncut, cut[LG_WIRES];
    int nvar;                                   /* the parts the player may change: unlocked switches, routes, fuses */
    uint8_t var_kind[LG_VARS], var_idx[LG_VARS];   /* LG_M*, and the node or the wire */
    int depth[LG_NODES];                        /* switches 0, a part one more than its deepest input */
    int maxdepth;
    int tracks[LG_ROWS];                        /* the tracks used in the gap above each row */
    int crossings;                              /* wires crossing in the drawing */
} lg_level;

typedef struct { uint8_t kind, idx; } lg_move;  /* LG_M*, and the switch's, route's or fuse's number (0..) */

typedef struct {
    lg_level L;
    int level;                                  /* 0..LG_LEVELS-1; -1 a level from text */
    int par;
    uint8_t val[LG_NODES];                      /* a switch's value, a route switch's way */
    uint8_t cut[LG_WIRES];
    uint8_t out[LG_NODES][2];                   /* each part's outputs (a route switch: left, right) */
    uint8_t wsrc[LG_WIRES], wend[LG_WIRES];     /* each wire's value at its start, and past its fuse */
    lg_move hist[LG_HIST];
    int nh;
    int hinted;                                 /* a hint was asked in this attempt */
} lg_game;

typedef struct {
    int par;                                    /* the fewest moves to a solution; -1: none */
    int nsol;                                   /* the settings that solve it */
    int nconf;                                  /* the settings tried */
    uint32_t best;                              /* the closest solution (bit i: variable i) */
} lg_answer;

/* the levels */
extern const char *const lg_levels[LG_LEVELS];
extern const uint8_t lg_pars[LG_LEVELS];

/* a gate's output from its inputs (nin 1..3) */
int  lg_gate(int kind, const uint8_t *in, int nin);
/* a level from its text: 0 and a reason in err (when not NULL) if it is wrong (its parts, its wires, its drawing) */
int  lg_parse(const char *src, lg_level *L, char *err, int errsize);
/* the drawing's tracks and fuses (lg_parse calls it): 0 when wires would overlap or a fuse has no room */
int  lg_route(lg_level *L);
/* the values everywhere for these settings; 1: solved */
int  lg_eval(const lg_level *L, const uint8_t *val, const uint8_t *cut, uint8_t out[][2], uint8_t *wsrc,
             uint8_t *wend);
/* every setting reachable from val/cut (cuts stay cut, within the limit): the closest solution */
void lg_solve(const lg_level *L, const uint8_t *val, const uint8_t *cut, lg_answer *a);
/* the variables' bits for a setting */
uint32_t lg_mask(const lg_level *L, const uint8_t *val, const uint8_t *cut);

void lg_load(lg_game *g, const lg_level *L, int level, int par);
int  lg_open(lg_game *g, int level);            /* one of the 150; 0: none such */
void lg_restart(lg_game *g);
int  lg_play(lg_game *g, lg_move m);            /* 0: not allowed (a locked switch, a fuse cut, no cut left) */
int  lg_undo(lg_game *g);                       /* 0: nothing to undo */
int  lg_solved(const lg_game *g);
int  lg_cuts(const lg_game *g);                 /* the cuts made */
int  lg_stars(const lg_game *g);                /* 1..3 for the moves made (once solved) */
int  lg_hint(const lg_game *g, lg_move *m);     /* a move towards the closest solution; 0: none (undo first) */
int  lg_script(lg_game *g, const char *moves);  /* the moves (s r c u x h =); 0 at the first bad one */

/* the generator: a level's text from a seed, for the parameters (the same seed, the same level) */
typedef struct {
    int cols, rows;                             /* the grid (rows: the switches' and the lamps' too) */
    int sw;                                     /* switches */
    unsigned kinds;                             /* the gates it may use: bit LG_NOT.. */
    int routes, fuses, locked;                  /* route switches, fuses, locked switches (at most) */
    int lamps, alarm;                           /* lamps (1..2), an alarm lamp (0/1) */
    int fan;                                    /* % of inputs taken from an output already used (fan-out) */
    int three;                                  /* % of three-input gates */
    int gates;                                  /* gates per row, at most */
} lg_params;
int lg_generate(uint32_t seed, const lg_params *p, char *out, int size);   /* 0: nothing came out of this seed */

/* the player's progress (logicgates_progress.c): the stars of each level, 0 unsolved */
void lg_progress_load(uint8_t stars[LG_LEVELS]);
void lg_progress_save(const uint8_t stars[LG_LEVELS]);

#endif
