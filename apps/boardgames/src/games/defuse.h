/*
 * Defuse the Bomb for one player: the rules. A bomb's circuit board: switches, push buttons and a battery at the
 * bottom, logic gates (Logic Gates' own: NOT, AND, OR, XOR, NAND, NOR) and memory chips above them, cuttable wires,
 * and at the top the disarm LEDs (A to D), the red DETONATE LED and, in some levels, the bomb's power socket. Disarm
 * it before its timer runs out: light every disarm LED at once, or cut the bomb's power. Light DETONATE on the way
 * and it blows. The order of the moves matters: every move's values must keep DETONATE dark.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The circuit and its drawing are Logic Gates' (logicgates.h: its level text, its parser, its router), so the two
 * games share their gates, their wires and their look; this game adds its own roles and rules on top:
 * - a switch: 0 or 1, flipped by a move;
 * - a push button: 1 while pressed only; a move presses it and lets it go (two evaluations, both must be safe);
 * - the battery: always 1 (a locked switch at 1 in Logic Gates' text);
 * - a memory chip (a latch): 1 from the first time its input is 1, for good (a NOT in Logic Gates' text);
 * - a cuttable wire (a jumper cable): cut by a move, for good (only undo or a restart mends it); 0 beyond the cut;
 * - a disarm LED (a lamp), DETONATE (the alarm lamp), the power socket (a lamp named by the roles).
 * The bomb is powered while its power socket carries 1 (always, without one). Unpowered, nothing lights. After every
 * evaluation: DETONATE lit (powered): lost; else every disarm LED lit, or the power gone: won.
 *
 * The level's text: the roles, " ~ ", then Logic Gates' text of the circuit (with its stored drawing):
 *   "T60 Pf4 Bb1 Mc2 ~ s@a1 S@b1 K@c1 AND@b2(a1,b1) NOT@c2(b1) L@a3(b2) A@b3(c2) L@f4(c1x) |..."
 * T: the timer in seconds; P: the power socket's cell; B: a push button's cell (a switch in the text, at 0); M: a
 * memory chip's cell (a NOT in the text). The battery: K. The disarm LEDs are lettered A.. from the left as drawn.
 *
 * The notation (script, the screenshots, the tests): "s2" flips switch 2, "b1" presses button 1, "c3" cuts cable 3
 * (numbered from 1: the switches and the buttons in Logic Gates' order of its switches, the cables in its order of its
 * cuttable wires); "u" undoes, "x" restarts, "h" asks a hint, "=" plays the level's best solution from the start.
 */
#ifndef DEFUSE_H
#define DEFUSE_H

#include <stdint.h>

#include "games/logicgates.h"

#define DF_CHAPTERS 6
#define DF_FIRST_CHAPTER 40                     /* chapter 1 (the free edition's 40 levels) */
#define DF_PER_CHAPTER 22                       /* each of the others */
#define DF_LEVELS (DF_FIRST_CHAPTER + (DF_CHAPTERS - 1) * DF_PER_CHAPTER)
#define DF_HIST 128
#define DF_BITS 18                              /* the most bits of a state: switches, cuts, latches */
#define DF_SECS 60
#define DF_TEXT 1024
#define DF_RANDOM 3                             /* the die tab's difficulties */

enum { DF_SW, DF_BUTTON, DF_BATTERY, DF_GATE, DF_LATCH, DF_LED, DF_DET, DF_PLUG };   /* a part's role */
enum { DF_MFLIP, DF_MPRESS, DF_MCUT };          /* a move's kind */
enum { DF_SAFE, DF_LOST, DF_WON };              /* a move's outcome */

static inline int df_chapter(int level)
{
    return level < DF_FIRST_CHAPTER ? 0 : 1 + (level - DF_FIRST_CHAPTER) / DF_PER_CHAPTER;
}
static inline int df_chapter_start(int c) { return c ? DF_FIRST_CHAPTER + (c - 1) * DF_PER_CHAPTER : 0; }

typedef struct {
    lg_level L;                                 /* the circuit and its drawing */
    uint8_t role[LG_NODES];
    uint8_t letter[LG_NODES];                   /* a disarm LED's letter (0: A) */
    int nled, det, plug, battery;               /* the disarm LEDs; DETONATE's part, the socket's, the battery's (-1) */
    int nsw, sw[LG_NODES];                      /* the switches and buttons (the notation's order: Logic Gates') */
    int ncut;                                   /* the cables: L.cut[] */
    int nlatch, latch[LG_NODES];
    int nbits;                                  /* a state: a bit for each switch or button (a button's stays 0),
                                                 * then each cut, then each latch */
    int secs;
} df_level;

typedef struct { uint8_t kind, idx; } df_move;

/* the values a state shows (press: the button held, -1 none) */
typedef struct {
    uint8_t out[LG_NODES];                      /* each part's output (a lamp: its input) */
    uint8_t wsrc[LG_WIRES], wend[LG_WIRES];     /* each wire at its start, and past its cut */
    int powered, det, win, lit;                 /* lit: the disarm LEDs lit (dark when not powered) */
    uint32_t next;                              /* the state after it (the latches set) */
} df_values;

typedef struct {
    df_level D;
    int level;                                  /* 0..DF_LEVELS-1; -1 random */
    int par;
    uint32_t start, state;
    df_move hist[DF_HIST];
    int nh;
    int hinted;
    int over;                                   /* DF_LOST (blown: the move's or the timer's), DF_WON, else 0 */
    df_values v;                                /* the values now */
} df_game;

/* the levels (defuse_levels.h) */
extern const char *const df_levels[DF_LEVELS];
extern const uint8_t df_pars[DF_LEVELS];

int  df_parse(const char *src, df_level *D, char *err, int errsize);
uint32_t df_bit_switch(const df_level *D, int i);   /* the state's bits */
uint32_t df_bit_cut(const df_level *D, int i);
void df_eval(const df_level *D, uint32_t state, int press, df_values *v);
/* a move from a state: DF_SAFE, DF_LOST, DF_WON, or -1 not allowed (a cable cut already); *next the state after;
 * a press's held values in *held when not NULL */
int  df_step(const df_level *D, uint32_t state, df_move m, uint32_t *next, df_values *held);
/* the shortest safe way to a win from a state: its length (-1: none); its first move in *first (when not NULL) */
int  df_solve(const df_level *D, uint32_t state, df_move *first);
/* the moves possible from a state (cuts not made yet), into m; how many */
int  df_moves(const df_level *D, uint32_t state, df_move *m);

void df_load(df_game *g, const df_level *D, uint32_t start, int level, int par);
int  df_open(df_game *g, int level);            /* 0: none such */
void df_restart(df_game *g);
int  df_play(df_game *g, df_move m);            /* the outcome; -1 not allowed (over, a cable cut) */
int  df_undo(df_game *g);                       /* 0: nothing (or blown) */
void df_blow(df_game *g);                       /* the timer ran out */
int  df_stars(const df_game *g);
int  df_hint(const df_game *g, df_move *m);     /* 0: none from here (undo or restart) */
int  df_script(df_game *g, const char *moves);  /* 0 at the first bad move */

/* ---- the generator (the levels' tool and the die tab) -------------------------------------------------------------- */
typedef struct {
    int sw_lo, sw_hi, btn, latches;             /* switches; push buttons and latches (each button feeds a latch) */
    int gates_lo, gates_hi;
    unsigned kinds;                             /* bit LG_NOT..LG_NOR */
    unsigned need;                              /* kinds it must use */
    int leds_lo, leds_hi;
    int fuses_lo, fuses_hi;
    int battery, plug;                          /* a battery feeding gates; a power socket */
    int par_lo, par_hi;
    int trap;                                   /* a move of the best way, made first, blows */
    int dead;                                   /* a first cut that leaves no way to win */
    int depth_hi, cross;
    int secs;
} df_spec;

typedef struct {
    int par, vars, gates, depth, lose_first, dead_first, moves_first, order_trap, greedy_trap, nsol, crossings;
    float score;
    uint64_t key;                               /* what it does, to tell repeats */
} df_rating;

/* 1: rated; 0: not from this start (not safe, no way to win); -1: the circuit is no puzzle (a switch, cable or chip that
 * never matters, a gate stuck, a power socket that never goes dark) */
int  df_rate(const df_level *D, uint32_t start, df_rating *r);
/* a level for the spec from the seed: its text (no stored drawing: the tool adds it); 0 none in `tries` */
int  df_make(uint32_t seed, const df_spec *sp, int tries, char *out, int size, df_rating *r);
void df_random_spec(int size, df_spec *sp);
int  df_random(df_game *g, int size, uint32_t seed);
/* the level's text with the drawing stored (Logic Gates' lg_layout_text); 0: too long */
int  df_text_with_layout(const char *src, const df_level *D, char *out, int size);

#endif
