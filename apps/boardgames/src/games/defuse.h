/*
 * Defuse the Bomb for one player: the rules. One bomb, one circuit: the battery and the switches feed a single
 * network of logic gates, flip-flops, capacitors and a board timer, and three lamps on the bomb's module draw from
 * it: POWER (the bomb alive), DISARM and DETONATE. Disarm the bomb before its timer runs out: light DISARM, or put
 * POWER out. Light DETONATE (while powered) and it blows. The parts share their signals, so most moves touch every
 * lamp at once; and some parts live in time: the order of the moves, and when they are made, matter.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The circuit and its drawing are Logic Gates' (logicgates.h: its level text, its parser, its router); the roles and
 * the rules are this game's:
 * - a switch: 0 or 1, flipped by a move; the battery: always 1 (a locked switch at 1 in Logic Gates' text);
 * - a cuttable wire (a jumper cable): cut by a move, for good (undo or a restart mends it); 0 beyond the cut;
 * - the gates: Logic Gates' NOT, AND, OR, XOR, NAND, NOR;
 * - a flip-flop: its output turns over each time its input rises from 0 to 1 (a NOT in Logic Gates' text);
 * - a capacitor of n: while its input is 1 it charges one step a tick (a second); full (n steps), it fires: its
 *   output is 1 for an instant (a pulse: DETONATE, a flip-flop...), and it is empty again; its input at 0 empties it
 *   (a NOT in Logic Gates' text);
 * - the board timer of n: counts down a step a tick from the start; at 0 its output becomes 1, for good (a locked
 *   switch at 0 in Logic Gates' text);
 * - the lamps: POWER (the bomb alive while it carries 1), DISARM, DETONATE (Logic Gates' alarm lamp).
 * After every change (a move, a tick, a pulse and its end): POWER on and DETONATE on: blown; else POWER off, or
 * DISARM on: disarmed.
 *
 * The level's text: the roles, " ~ ", then Logic Gates' text of the circuit (with its stored drawing):
 *   "T60 Pc3 Zb1:9 Fb2:0 Cc2:4 ~ s@a1 k@b1 K@c1 ... |..."
 * T: the main timer in seconds; P: POWER's cell (the other lamp is DISARM); Z: the board timer's cell and its ticks;
 * F: a flip-flop's cell and its first state; C: a capacitor's cell and its size (2..9).
 *
 * The notation (script, the screenshots, the tests): "s2" flips switch 2, "c3" cuts cable 3 (numbered from 1 in Logic
 * Gates' orders), "w" lets a tick pass ("w5": five); "u" undoes, "x" restarts, "h" asks a hint, "=" plays the best
 * way from the start.
 * The solver: the states (switches, cuts, flip-flops, charges, the board timer) searched for the fewest moves, the
 * ticks passing between them; a person is taken to make at most DF_QUICK moves in a tick.
 */
#ifndef DEFUSE_H
#define DEFUSE_H

#include <stdint.h>

#include "games/logicgates.h"

#define DF_CHAPTERS 6
#define DF_PER_CHAPTER 20
#define DF_FIRST_CHAPTER DF_PER_CHAPTER        /* (the free edition plays two chapters) */
#define DF_LEVELS (DF_CHAPTERS * DF_PER_CHAPTER)
#define DF_HIST 128
#define DF_SECS 60
#define DF_TEXT 1024
#define DF_RANDOM 3                             /* the die tab's difficulties */
#define DF_TICK_MS 1000
#define DF_QUICK 2                              /* the moves a person makes in a tick, at most (the solver's) */
#define DF_MAXSW 8
#define DF_MAXCUT 6
#define DF_MAXFF 4
#define DF_MAXCAP 3

enum { DF_SW, DF_BATTERY, DF_GATE, DF_FLIP, DF_CAP, DF_TIMER, DF_DISARM, DF_DET, DF_POWER };   /* a part's role */
enum { DF_MFLIP, DF_MCUT, DF_MWAIT };          /* a move's kind (a wait: the hint's, the solver's) */
enum { DF_SAFE, DF_LOST, DF_WON };              /* an outcome */

static inline int df_chapter(int level) { return level / DF_PER_CHAPTER; }
static inline int df_chapter_start(int c) { return c * DF_PER_CHAPTER; }

typedef struct {
    lg_level L;                                 /* the circuit and its drawing */
    uint8_t role[LG_NODES];
    int8_t idx[LG_NODES];                       /* a switch's, flip-flop's or capacitor's number */
    uint8_t size[LG_NODES];                     /* a capacitor's size */
    int det, disarm, power, battery, timer;     /* the parts (-1: none) */
    int timer_ticks;
    int nsw, sw[DF_MAXSW];                      /* the switches (the notation's order: Logic Gates') */
    int ncut;                                   /* the cables: L.cut[] */
    int nff, ff[DF_MAXFF], ff0[DF_MAXFF];       /* the flip-flops, their first states */
    int ncap, cap[DF_MAXCAP];
    int secs;
} df_level;

typedef struct {
    uint8_t sw, cut, ff;                        /* bits */
    uint8_t ch[DF_MAXCAP];                      /* the capacitors' charges */
    uint8_t tmr;                                /* the board timer's ticks left */
} df_state;

typedef struct { uint8_t kind, idx; } df_move;

/* the values a state shows; fire: the capacitors firing now (bit k) */
typedef struct {
    uint8_t out[LG_NODES];                      /* each part's output (a lamp: its input) */
    uint8_t wsrc[LG_WIRES], wend[LG_WIRES];     /* each wire at its start, and past its cut */
    uint8_t ffin[DF_MAXFF], capin[DF_MAXCAP];   /* the flip-flops' and capacitors' inputs */
    int powered, det, disarm, win;
} df_values;

/* a change's steps as they happened (a move, or a tick: the pulse, its end): the values after each */
#define DF_STEPS 3
typedef struct { int n; df_values v[DF_STEPS]; int origin[DF_STEPS]; } df_steps;

typedef struct {
    df_level D;
    int level;                                  /* 0..DF_LEVELS-1; -1 random */
    int par;
    df_state start, s;
    df_state hist[DF_HIST];                     /* the state before each move */
    int nh;                                     /* the moves made */
    int ticks;                                  /* the ticks gone since the start */
    int hinted;
    int over;                                   /* DF_LOST, DF_WON, else 0 */
    df_values v;                                /* the values now */
} df_game;

/* the levels (defuse_levels.h) */
extern const char *const df_levels[DF_LEVELS];
extern const uint8_t df_pars[DF_LEVELS];
/* the die tab's puzzles (defuse_levels.h too): more of the tool's, kept apart, by difficulty (a search for one on a
 * phone takes seconds: they are made beforehand) */
#define DF_POOL 40
extern const char *const df_pool[DF_RANDOM][DF_POOL];
extern const uint8_t df_pool_pars[DF_RANDOM][DF_POOL];

int  df_parse(const char *src, df_level *D, char *err, int errsize);
df_state df_start(const df_level *D);
void df_eval(const df_level *D, df_state *s, const uint8_t *ffin_before, unsigned fire, df_values *v);
/* a move or a tick from a state: the outcome (-1: not allowed: a cable cut already); *s changes; the steps shown */
int  df_move_do(const df_level *D, df_state *s, df_move m, df_steps *st);
int  df_tick(const df_level *D, df_state *s, df_steps *st);
int  df_moves(const df_level *D, const df_state *s, df_move *m);
/* the fewest moves to a win from a state (-1: none), the ticks they take in *ticks; the first action in *first (a
 * wait when time must pass first) */
int  df_solve(const df_level *D, const df_state *s, df_move *first, int *ticks);

void df_load(df_game *g, const df_level *D, int level, int par);
int  df_open(df_game *g, int level);            /* 0: none such */
void df_restart(df_game *g);
int  df_play(df_game *g, df_move m);            /* the outcome; -1 not allowed */
int  df_pass(df_game *g);                       /* a tick passes: the outcome */
int  df_undo(df_game *g);                       /* 0: nothing (or blown) */
void df_blow(df_game *g);                       /* the main timer ran out */
int  df_stars(const df_game *g);
int  df_hint(const df_game *g, df_move *m);     /* 0: none from here (undo or restart) */
int  df_script(df_game *g, const char *moves);  /* 0 at the first bad move */

/* ---- the generator (the levels' tool and the die tab) -------------------------------------------------------------- */
typedef struct {
    int sw_lo, sw_hi;
    int gates_lo, gates_hi;
    unsigned kinds;                             /* bit LG_NOT..LG_NOR */
    unsigned need;                              /* kinds it must use */
    int conds;                                  /* DISARM's conditions at most (1: one signal; 2, 3: an AND) */
    int fuses_lo, fuses_hi;
    int ff_lo, ff_hi, cap_lo, cap_hi, timer;    /* flip-flops, capacitors, a board timer */
    int fan;                                    /* % of the inputs taken from a signal already used */
    int par_lo, par_hi;
    int easy;                                   /* a teaching level: no trap needed */
    int timing;                                 /* time must matter (a wait, or a deadline) */
    int cross;
    int secs;
} df_spec;

typedef struct {
    int par, ticks, vars, gates, depth, crossings;
    int moves_first, lose_first, dead_first;    /* the first moves: blowing, leaving no way */
    int ways;                                   /* the winning orders of the best way's moves, of `orders` */
    int orders;
    int nsol;                                   /* the first actions that start a best way */
    int greedy_ok;                              /* the obvious move (lighting most of DISARM's conditions) works */
    int timing;                                 /* waiting is needed, or a slow player loses */
    int deadline;                               /* the ticks before the bomb blows by itself (0: never) */
    int shared;                                 /* the controls that reach two lamps or more, % */
    float score;
    uint64_t key;
} df_rating;

/* 1: rated; 0: no puzzle from this start; -1: no puzzle at all (a part that never matters...) */
int  df_rate(const df_level *D, const df_state *start, df_rating *r);
int  df_make(uint32_t seed, const df_spec *sp, int tries, char *out, int size, df_rating *r);
void df_random_spec(int size, df_spec *sp);
int  df_random(df_game *g, int size, uint32_t seed);
int  df_text_with_layout(const char *src, const df_level *D, char *out, int size);
int  df_spec_ok(const df_spec *sp, const df_rating *r);

#endif
