/*
 * Logic Gates for one player: the rules. A circuit on a board: switches, logic gates above them, lamps at the top;
 * wires carry the signal (0 or 1) upwards. Set the switches so that every lamp is lit, while the alarm lamps (when a
 * level has one) stay dark. 165 levels, every one of them our own: chapter 1 of 40 (AND and OR, then NOT; the free
 * edition's levels), then 5 chapters of 25, each bringing a gate or two; every chapter grows from a few switches to
 * 8. And circuits made at random (the die tab).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The parts:
 * - a switch: 0 or 1, flipped by a tap; a locked switch (a brass bar screwed over it) cannot be flipped;
 * - the gates: NOT (its input reversed), AND (1 when all its inputs are 1), OR (when any is), XOR (when an odd number
 *   of them are: two inputs, exactly one), NAND (not AND), NOR (not OR); NOT has one input, the others two or three;
 * - a route switch: one input, two outputs (left and right); its input goes on along the way it is set to, the other
 *   way carries 0; a tap sets it the other way;
 * - a cuttable wire (a fuse in the text; on the board a jumper cable between two terminals) can be cut by a tap: then
 *   it carries 0 beyond the cut (it is disconnected). A cut stays cut (only undo or a restart mends it); a level may
 *   allow fewer cuts than it has cuttable wires;
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
 * - " |...": the drawing, stored (lg_layout_text): read as it is, no search.
 * Every wire goes up (from a lower row); every output feeds at least one input.
 * The cells only name the parts: the drawing is laid out from the circuit alone (logicgates_rules.c): the lamps on
 * the top row, each other part just under the parts it feeds (a switch right under its gate, at any height); each row
 * ordered for the fewest crossings, each part centred over its inputs; a wire rises from its source, bends in the gap
 * under a row, and rises into its pin; a crossing left gets a hop, a branching a dot.
 *
 * The notation (script, the screenshots, the tests), in the level open (lg_open; the screenshots: --level n): "s3"
 * flips switch 3, "r2" route switch 2, "c5" cuts wire 5 (numbered from 1 in the level's text: the switches and the
 * route switches in reading order of their cells from the bottom left, row by row; the fuses by the part they lead
 * into, in the same order, then from the left); "u" undoes, "x" restarts the level, "h" asks a hint (the screen shows
 * it blinking; lg_script plays it); "=" plays the level's best solution.
 */
#ifndef LOGICGATES_H
#define LOGICGATES_H

#include <stdint.h>

#define LG_COLS 12                              /* the text's cells: columns a..l, rows 1..9 */
#define LG_ROWS 9
#define LG_NODES 40
#define LG_WIRES 64
#define LG_VARS 16                              /* the most parts a level lets the player change */
#define LG_PTS 20                               /* the most points on a wire's drawing */
#define LG_MARKS 48                             /* the most hops, and the most branchings */
#define LG_CHAPTERS 6
#define LG_FIRST_CHAPTER 40                     /* chapter 1: AND and OR, then NOT (the free edition's 40 levels) */
#define LG_PER_CHAPTER 25                       /* each of the others */
#define LG_LEVELS (LG_FIRST_CHAPTER + (LG_CHAPTERS - 1) * LG_PER_CHAPTER)
#define LG_HIST 256
#define LG_NH 0.5f                              /* a part's height, rows (the rest of a row: the gap with the tracks) */
#define LG_HALF 0.40f                           /* half a part's width, columns */
#define LG_SW_HALF 0.27f                        /* half a switch's width (a switch is narrower) */
#define LG_PAD_B 0.12f                         /* the room under the bottom row and over the top one (the lamps */
#define LG_PAD_T 0.46f                          /* stand taller than a row), rows */
#define LG_LAYOUT 400                           /* a stored layout's text, at most */

enum { LG_SWITCH, LG_NOT, LG_AND, LG_OR, LG_XOR, LG_NAND, LG_NOR, LG_ROUTE, LG_LAMP, LG_ALARM, LG_KINDS };
enum { LG_MSWITCH, LG_MROUTE, LG_MCUT };        /* a move's kind */

typedef struct {
    uint8_t kind, col, row, nin;                /* col, row: its cell in the text (its name, not its place) */
    uint8_t locked;                             /* a switch the player cannot flip */
    uint8_t init;                               /* a switch's value, a route switch's way (0 left, 1 right) */
    int8_t in[3];                               /* the wires into it, in the text's order */
    uint8_t layer;                              /* its row in the drawing (0: the switches) */
    float x, y;                                 /* its place: its middle (columns), its bottom (rows) */
} lg_node;

typedef struct {
    uint8_t from, port, to, slot;               /* port: 0 the output, 1 a route switch's left, 2 its right */
    uint8_t cuttable;
    int net;                                    /* from * 3 + port: the wires carrying one signal */
    /* the drawing, in grid units: x columns from the left, y rows up from the bottom */
    int npt;
    float px[LG_PTS], py[LG_PTS];
    float len;                                  /* its length */
    /* a cuttable wire: a jumper cable between two terminals on one straight stretch of it (the printed trace stops
     * at them); the cable bows sideways by `bow` (grid units: + right of a vertical stretch, above a level one) */
    float fs, fx, fy;                           /* the cable's middle: how far along, its place */
    int fvert;                                  /* 1: on a vertical stretch */
    float ta, tb;                               /* the terminals: how far along */
    float tax, tay, tbx, tby;                   /* their places */
    float bow;
} lg_wire;

/* a level's chapter (0..), a chapter's first level */
static inline int lg_chapter(int level)
{
    return level < LG_FIRST_CHAPTER ? 0 : 1 + (level - LG_FIRST_CHAPTER) / LG_PER_CHAPTER;
}
static inline int lg_chapter_start(int c) { return c ? LG_FIRST_CHAPTER + (c - 1) * LG_PER_CHAPTER : 0; }
static inline int lg_chapter_size(int c) { return c ? LG_PER_CHAPTER : LG_FIRST_CHAPTER; }

static inline float lg_half(int kind) { return kind == LG_SWITCH ? LG_SW_HALF : LG_HALF; }   /* half its width */

typedef struct { float x, y, s; int wire; } lg_mark;   /* a hop or a branching: where, and how far along a wire */

typedef struct {
    int cols, rows, nn, nw, cut_limit;          /* cols, rows: the text's cells */
    lg_node n[LG_NODES];                        /* in reading order from the bottom left (so inputs come first) */
    lg_wire w[LG_WIRES];
    int nsw, sw[LG_COLS * LG_ROWS];             /* the switches, routes and fuses in the notation's order */
    int nrt, rt[LG_NODES];
    int ncut, cut[LG_WIRES];
    int nvar;                                   /* the parts the player may change: unlocked switches, routes, fuses */
    uint8_t var_kind[LG_VARS], var_idx[LG_VARS];   /* LG_M*, and the node or the wire */
    int depth[LG_NODES];                        /* switches 0, a part one more than its deepest input */
    int maxdepth;
    /* the drawing (lg_route) */
    int layers;                                 /* its rows, the switches' to the lamps' */
    float ly[LG_ROWS];                          /* each row's bottom (the gaps between rows grow with their tracks) */
    float width, height;                        /* its size: the parts' middles at least half a column in */
    int stored;                                 /* laid out from the level's stored layout (no search) */
    char layout[LG_LAYOUT];                     /* the layout as text (lg_layout_text) */
    int crossings;                              /* wires crossing (each one drawn with a hop) */
    int nhop, njoint;
    lg_mark hop[LG_MARKS];                      /* the crossings: on the wire running sideways */
    lg_mark joint[LG_MARKS];                    /* the branchings (a dot) */
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
/* the drawing: the parts' places, the wires' paths, the cables, hops and dots (lg_parse calls it: from the layout
 * stored in the text when it has one, else lg_route_fast); 0 when lg_check finds it unclear.
 * - lg_route_fast: a bounded search (a few starts, small exact searches, a few milliseconds at most), for the die
 *   tab's circuits;
 * - lg_route_best: the levels' tool's search (each part's row tried too, many starts: seconds); its result goes into
 *   the level's text (lg_layout_text), so a level opens without a search. */
int  lg_route(lg_level *L);
int  lg_route_fast(lg_level *L);
int  lg_route_best(lg_level *L, int effort);
/* the layout as text: " |" then each part's row (a character each, '0'..), "/", then each row's items from the left
 * ('.' between rows; a part '0' + its index, a wire rising past the row 'a' + its trunk's index); appended to the
 * circuit's text, lg_parse lays the level out from it */
int  lg_layout_text(const lg_level *L, char *out, int size);
/* how big a gate comes out in a room rw x rh (pixels) laid out upright: its width (pixels); a column's and a row's
 * sizes (pixels; NULL: not wanted). Rows at most 1.7 columns tall; a column at most a 3.2th of the room's short
 * side. */
float lg_part_size(const lg_level *L, float rw, float rh, float *cw, float *ch);
/* the drawing is clear: no two parts overlap, no wire runs through a part, no two signals run along one line or
 * touch but by crossing squarely, every cable has room; 0 and why in err (when not NULL) */
int  lg_check(const lg_level *L, char *err, int errsize);
/* the values everywhere for these settings; 1: solved */
int  lg_eval(const lg_level *L, const uint8_t *val, const uint8_t *cut, uint8_t out[][2], uint8_t *wsrc,
             uint8_t *wend);
/* every setting reachable from val/cut (cuts stay cut, within the limit): the closest solution */
void lg_solve(const lg_level *L, const uint8_t *val, const uint8_t *cut, lg_answer *a);
/* the variables' bits for a setting */
uint32_t lg_mask(const lg_level *L, const uint8_t *val, const uint8_t *cut);

void lg_load(lg_game *g, const lg_level *L, int level, int par);
int  lg_open(lg_game *g, int level);            /* one of the levels; 0: none such */
void lg_restart(lg_game *g);
int  lg_play(lg_game *g, lg_move m);            /* 0: not allowed (a locked switch, a fuse cut, no cut left) */
int  lg_undo(lg_game *g);                       /* 0: nothing to undo */
int  lg_solved(const lg_game *g);
int  lg_cuts(const lg_game *g);                 /* the cuts made */
int  lg_stars(const lg_game *g);                /* 1..3 for the moves made (once solved) */
int  lg_hint(const lg_game *g, lg_move *m);     /* a move towards the closest solution; 0: none (undo first) */
int  lg_script(lg_game *g, const char *moves);  /* the moves (s r c u x h =); 0 at the first bad one */

/* a level written back as text (the format above; its cells as read) */
int  lg_text(const lg_level *L, char *out, int size);

/* ---- the generator (the levels' tool, tools/logicgates_gen.c, and the random puzzles of the die tab) ----------------
 * A circuit grows from its switches, laid in a row: a gate joins two (or three) neighbouring signals, or takes one and
 * one already used beside it (a branching), a NOT turns one, a route switch splits one in two; until the signals left
 * are the lamps'. It is kept when it is a puzzle (lg_make): few settings light it (sols at most, counting the routes
 * and the cuts), no gate's output is the same in every setting, every gate matters (turning its output changes what
 * lights), at most `herrings` switches and route switches never matter; a start with the par asked for, not every lamp
 * lit; a clear drawing with few crossings. */
typedef struct {
    int sw_lo, sw_hi;                           /* switches */
    int gates_lo, gates_hi;                     /* gates (NOTs counted; route switches not) */
    unsigned kinds;                             /* the gates it may use: bit LG_NOT.. */
    unsigned need;                              /* the kinds it must use */
    int routes_lo, routes_hi, fuses_lo, fuses_hi;
    int locked;                                 /* locked switches, at most */
    int lamps, alarms;                          /* lamps (1..2) and alarm lamps (0..1), at most */
    int alarm_need;                             /* an alarm lamp always (when alarms allows one) */
    int three, fan, nots;                       /* % three-input gates, % branchings, % NOTs (of the gates added) */
    int depth_lo, depth_hi;                     /* the deepest gate's depth */
    int par_lo, par_hi;
    int sols;                                   /* the settings that solve it, at most */
    int herrings;                               /* switches or route switches that never matter, at most */
    int need_route, need_cut;                   /* every solution turns a route switch / cuts a wire */
    int cross;                                  /* crossings in the drawing, at most */
    float wide;                                 /* the drawing's width, at most (columns; 0: any) */
} lg_spec;

/* what a level is like (lg_rate): its size, its par, how hard it is, and its keys (64-bit hashes) to tell repeats:
 * - exact: the circuit and its start, whatever the switches' order, a mirror image, the inputs' order of a gate (every
 *   gate is symmetric), a route switch's sides (its way set at the start is what counts);
 * - structure: the same without the start (the switches' values, the route switches' ways);
 * - shape: the structure with every gate the same (NOT, two inputs, three inputs);
 * - func: what lights it, whatever the variables' order (the solutions, each variable's sway);
 * - start: the start against the solutions (the par, the distance to each solution).
 * Two levels are the same when their exact keys are, or their shape, func and start keys are; near the same (one
 * puzzle with another start) when their structure keys are. */
typedef struct {
    int sw, vars, gates, depth, routes, fuses, lamps, alarms, xors, fans, three, nots;
    int par, nsol, herrings, decoys, crossings;
    int consts, idle;                           /* gates whose output never changes; gates that never matter */
    float score;                                /* the difficulty, as a number to sort by */
    uint64_t exact, structure, shape, func, start;
} lg_rating;
/* 0: not rated (more than 14 variables) */
int  lg_rate(const lg_level *L, lg_rating *r);
/* a level for the spec from the seed (the same seed, the same level): its text; 0: none found in `tries` circuits */
int  lg_make(uint32_t seed, const lg_spec *sp, int tries, char *out, int size, lg_rating *r);
/* the die tab's puzzles: 0 small (3 to 4 switches; AND, OR, NOT), 1 medium (5 to 6; XOR and NAND too), 2 big (7
 * to 8; everything) */
#define LG_RANDOM 3
void lg_random_spec(int variant, lg_spec *sp);
int  lg_random(lg_game *g, int variant, uint32_t seed);   /* a new random puzzle (g->level -1); 0: none (never) */

#endif
