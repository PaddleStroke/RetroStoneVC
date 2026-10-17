/*
 * Gopher Dig: shared declarations.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/gopherdig/LICENSE.
 *
 * play.c is the game rules (the well, the gopher, the falling blocks, the air, the strata, the bot; no drawing, no
 * sound: deterministic, testable alone: tests/test_rules.c); draw.c is the video, main.c the flow (the title with the
 * players joining, play, game over), input (pads and touch), sound, save RAM, options and save states.
 * Made from games/_template (tools/new_game.py): the house style kit is games/common (docs/art-direction.md).
 */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "rs.h"

#define MAX_PLAYERS 4                   /* the house kit's HU_MAX_PLAYERS: 4 pads */

/* ---- tuning: every gameplay number is here (DESIGN.md "Rules") ------------------------------------------------------ */
/* the well */
#define COLS          9         /* the well's width in blocks */
#define CELL          16        /* a block, px */
#define ROWS          40        /* the rows kept (a ring that follows the gopher down) */
#define ROWS_ABOVE    14        /* ... of which above the gopher's row (older rows are dropped) */
#define START_COL     4
#define START_ROW     (-1)      /* the gopher starts on the grass, above row 0 */
#define N_COLOURS     4         /* red, yellow, green, blue */
#define POP_MIN       4         /* a group of this many after a fall pops by itself */
/* the gopher */
#define WALK_SPEED    2         /* px a frame: 8 frames a block */
#define CLIMB_FRAMES  10        /* up one block step */
#define CLIMB_HOLD    8         /* frames the pad pushes against a block before the gopher climbs it */
#define GOPHER_FALL   4         /* px a frame */
#define DRILL_FRAMES  9         /* a drill's swing (the gopher waits) */
#define DRILL_REPEAT  12        /* A (or a finger) held: drill again every this many frames */
#define HARD_HITS     5         /* drills a hard rock takes */
/* the air (milli-percent: 100000 = a full tank) */
#define AIR_FULL      100000
#define AIR_ITEM      25000     /* an air bubble */
#define HARD_AIR      20000     /* breaking a hard rock costs this */
#define CHECK_AIR     30000     /* a checkpoint refills this much */
#define AIR_LOW       30000     /* below: the meter blinks, a beep */
/* the strata: every STRATUM_M metres a new theme, a little faster and harsher (the tables run out: the last row,
 * the drain still growing by DEEP_DRAIN a stratum) */
#define STRATUM_M     100
#define STRATA        6         /* garden soil, red clay, crystal cave, warm rocks, fossil beds, gem core (then again) */
#define CHECK_FREEZE  90        /* the checkpoint celebration: the well stands still, the gopher cheers */
#define S_SHAKE       {54, 50, 46, 42, 38, 34}     /* frames a loose group shakes before it falls */
#define S_FALL        {8, 8, 7, 7, 6, 6}           /* frames a falling block takes per row */
#define S_DRAIN       {28, 31, 34, 37, 40, 43}     /* air drain, milli-percent a frame (28: a tank lasts 60 s) */
#define S_HARD        {12, 35, 60, 80, 100, 120}   /* per mille of the blocks that are hard rocks */
#define S_AIRGAP      {10, 11, 12, 13, 14, 15}     /* rows between air bubbles (plus 0..3) */
#define S_SAME_H      {300, 280, 260, 240, 220, 200}   /* per mille: a block takes its left neighbour's colour */
#define S_SAME_V      {200, 190, 180, 160, 150, 140}   /* ... else the colour of the block above it */
#define S_HOLE        {0, 6, 10, 14, 18, 22}       /* per mille: an empty pocket */
#define DEEP_DRAIN    3         /* past the tables: the drain grows this much a stratum */
#define SAFE_ROWS     3         /* the first rows: no hard rock, no pocket */
/* flow */
#define PANEL_DELAY   60        /* frames between the last player out and the game-over panel */
#define RETRY_LOCK    36        /* frames the panel ignores the button (no accidental retry) */
#define OUT_FREEZE    8         /* hit-stop frames */
#define READY_BOB     4         /* the title gopher's bob, px */
#define BOT_START     20        /* frames on the title before the bot starts the run */
#define HINT_FRAMES   360       /* the controls hint, 1 player, at the start of a run */
#define MEDAL_DEPTHS  {60, 200, 400, 600}           /* bronze, silver, gold, pearl: metres (DESIGN.md "Medals") */

/* ---- play.c: the rules ------------------------------------------------------------------------------------------------ */
enum { K_EMPTY, K_RED, K_YELLOW, K_GREEN, K_BLUE, K_HARD, K_AIR, K_KINDS };
enum { C_STILL, C_SHAKE, C_FALL };                  /* a block's state */
#define K_IS_COLOUR(k) ((k) >= K_RED && (k) <= K_BLUE)

typedef struct gcell {
    uint8_t kind;               /* K_* */
    uint8_t st;                 /* C_* */
    uint8_t timer;              /* C_SHAKE: frames left */
    uint8_t hits;               /* K_HARD: drills so far; landed flag (bit 7) during a fall tick */
} gcell;

enum { ACT_IDLE, ACT_WALK, ACT_CLIMB, ACT_FALL, ACT_DRILL };
enum { AIM_SIDE, AIM_DOWN };
enum { OUT_NONE, OUT_CRUSH, OUT_AIR };
typedef struct gopher {
    int col, row;               /* its cell (walking, climbing, falling: the cell it goes to) */
    int x, y;                   /* well pixels of its cell's top-left, animated */
    int from_col, from_row;     /* where the action started */
    int face, aim;              /* -1 left / 1 right; AIM_* */
    int act, act_t, act_n;      /* ACT_*, frames in it, its length */
    int hold_t;                 /* frames pushing against a block (climb) */
    int drill_held;             /* frames A has been held */
    int buf_drill, buf_tap;     /* a drill asked for during another action: frames it is kept, the touch's target */
    int walk_to;                /* touch: the column it walks to (-1: none) */
    int drill_dir;              /* the last drill: 0 left, 1 right, 2 down (the sprite) */
    int32_t air;                /* milli-percent */
    int depth;                  /* the deepest, metres (the score) */
    int alive, out, out_t;      /* OUT_*, frames since */
    int stratum;                /* the deepest stratum reached */
    int cheer_t;                /* checkpoint: frames of the celebration left */
    int pops, drills, hards, airs, best_chain;      /* stats */
    int low_t;                  /* frames below AIR_LOW */
} gopher;

typedef struct bot_mem {
    int target_col, target_row; /* the column it digs towards, chosen on that row */
    int think_t;                /* human-like bot: frames until it thinks again */
    int plan;                   /* the planned input (held between thoughts) */
    int last_depth, stall;      /* frames without digging deeper */
    uint32_t rng;
} bot_mem;

typedef struct well {
    uint32_t seed;              /* the terrain (the same for every player) */
    gcell g[ROWS][COLS];        /* ring: row r is g[ring(r)] for top <= r < top + ROWS */
    int top;                    /* the first row kept; rows above are air, rows below are not dug yet (solid) */
    uint8_t gen_prev[COLS];     /* the last row generated, as generated (every player's well is the same) */
    int next_air;               /* the row of the next air bubble */
    int fall_t;                 /* frames since the last fall tick */
    int dirty;                  /* the blocks changed: their support is computed again */
    int chain;                  /* pops by themselves since the last drill */
    int moving;                 /* cells shaking or falling */
    int freeze;                 /* the checkpoint celebration: frames the well stands still */
    int ev;                     /* EV_* of this step */
    int gen_off;                /* the rules' tests: new rows are rock */
    gopher go;
    bot_mem bot;
} well;

typedef struct pin {            /* a player's input this frame */
    int8_t dir;                 /* -1, 0, 1: Left / Right held */
    uint8_t down;               /* Down held */
    uint8_t drill;              /* A pressed */
    uint8_t drill_hold;         /* A held */
    int8_t tap_drill;           /* touch: drill there now: 0 none, 1 left, 2 right, 3 down */
    int8_t walk_to;             /* touch: walk to this column (-1: no change) */
    uint8_t any;
} pin;

enum { FX_POP, FX_CHAIN, FX_HARD, FX_CRACK, FX_AIR, FX_DUST, FX_CRUSH };
typedef struct fx {             /* an effect of the last step, for draw.c (well pixels of a cell's top-left) */
    int32_t x, y;               /* int32: a well deeper than 2047 m is past 32767 px */
    uint8_t kind, colour, player, n;
} fx;
#define FX_MAX 64

typedef struct world {
    well w[MAX_PLAYERS];
    int players, t, started;
    uint32_t seed;
    int events[MAX_PLAYERS];    /* EV_* of the last step */
    fx fx[FX_MAX];
    int nfx;
} world;
enum {
    EV_DRILL = 1, EV_POP = 2, EV_CHAIN = 4, EV_HARD_HIT = 8, EV_HARD_BREAK = 16, EV_AIR = 32, EV_SHAKE = 64,
    EV_LAND = 128, EV_STEP = 256, EV_CLIMB = 512, EV_CRUSH = 1024, EV_NOAIR = 2048, EV_CHECKPOINT = 4096,
    EV_LOW = 8192, EV_START = 16384, EV_WHIFF = 32768
};

void world_init(world *w, int players, uint32_t seed);
void world_step(world *w, const pin in[MAX_PLAYERS]);
int  world_running(const world *w);             /* someone still digs */
int  world_out_t(const world *w);               /* frames since the last player went out */
/* the well's cells (rows above the ring: empty; below: solid, not dug yet) */
const gcell *well_cell(const well *w, int col, int row);
int  well_kind(const well *w, int col, int row);     /* K_*; -1: outside the walls or not dug yet */
int  well_passable(const well *w, int col, int row); /* the gopher can go there (empty, or an air bubble) */
int  stratum_of_row(int row);                        /* 0.. (the theme: % STRATA) */
int  stratum_param(int s, int which);                /* the tables above by stratum */
enum { P_SHAKE, P_FALL, P_DRAIN, P_HARD, P_AIRGAP, P_SAME_H, P_SAME_V, P_HOLE };
int  well_fall_px(const well *w);               /* the falling blocks' progress into their row, 0..15 px */
int  well_danger(const well *w, int col, int row);  /* a shaking or falling block would come down on this cell */
/* test hooks (tests/test_rules.c): put a cell, settle the support */
void well_set(well *w, int col, int row, int kind);
void well_settle(well *w);
void well_clear(well *w);                       /* every row empty, the gopher on the floor of a 1-row box */

/* the bot (the test hook, the screenshots): skill 0 plays well, 1..10 more and more like a beginner */
void bot_think(world *W, int p, int skill, pin *out);

/* ---- draw.c ------------------------------------------------------------------------------------------------------------ */
enum { ST_TITLE, ST_PLAY, ST_DEAD, ST_OVER };      /* no "get ready": the title is the only menu */
typedef struct vrect { int x, y, w, h; } vrect;
int  view_layout(int players, vrect out[MAX_PLAYERS]);   /* the views (stage coordinates) */
int  view_well_x(const vrect *v);                 /* the well's left edge in a view (stage x) */
int  view_gopher_y(const vrect *v);               /* where the camera keeps the gopher (from the view's top) */
void draw_init(void);
void draw_new_run(const world *w);
void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused);
int  draw_cam_y(int p);                           /* player p's camera: the well y at its view's top */
void draw_state(void);
void draw_state_loaded(void);

/* ---- sound (main.c) -------------------------------------------------------------------------------------------------- */
enum { SFX_DRILL, SFX_POP, SFX_CHAIN, SFX_CLANK, SFX_CRUMBLE, SFX_AIR, SFX_RUMBLE, SFX_LAND, SFX_STEP, SFX_CRUSH,
       SFX_NOAIR, SFX_LOW, SFX_WHIFF, SFX_COUNT };
#define SFX_HOUSE SFX_COUNT                                    /* first slot of the house sounds (ha_init) */

#endif
