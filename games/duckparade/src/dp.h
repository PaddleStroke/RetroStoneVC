/*
 * Duck Parade: shared declarations.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/duckparade/LICENSE.
 *
 * lanes.c (the lanes, the generator, the fairness judge) and world.c (a run: the ducks, the parades, the
 * camera, the fox) are the game rules: no drawing, no sound (the unit tests link them alone). draw.c, sfx.c,
 * bot.c and main.c are the console side.
 */
#ifndef DP_H
#define DP_H

#include <stdint.h>
#include "rs.h"
#include "tuning.h"

#define MAX_PLAYERS 4           /* Mother, Father and two more parents (house: 4 pads, 4 players) */

static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline int absi(int v) { return v < 0 ? -v : v; }
static inline int modi(int a, int m) { int r = a % m; return r < 0 ? r + m : r; }

/* ================================ lanes.c ========================================================== */
enum lane_kind { LK_GRASS, LK_ROAD, LK_RIVER, LK_RAIL, LK_PARK, LK_POND, LK_NEST, LK_HEDGE, LK_KINDS };
/* what moves (the sprite drawn; the hit box is the mover's len) */
enum mover_kind {
    MV_CAR0, MV_CAR1, MV_CAR2, MV_CAR3, MV_CAR4, MV_CAR5,      /* cars: six colours */
    MV_BIKE, MV_BUS, MV_JOGGER, MV_MOWER,
    MV_LOG2, MV_LOG3, MV_LOG4, MV_PAD, MV_BOAT, MV_KINDS
};
#define MOVER_MAX 8

typedef struct mover {
    int16_t p0;                 /* loop px of its top edge at frame 0 */
    uint8_t len, kind;          /* px along the lane; enum mover_kind */
} mover;

typedef struct lane {
    int32_t col;                /* world column (the ring slot's owner; -1 = empty) */
    uint8_t kind, n, down;      /* enum lane_kind; movers; 1 = traffic flows down (+y) */
    uint8_t deco;               /* decoration variant (tiles) */
    uint16_t v;                 /* Q8 px per frame (all movers of a lane share it) */
    uint16_t block;             /* grass/hedge: rows with a tree; pond: rows with a pad */
    uint16_t period, phase;     /* railway: frames between trains, phase */
    uint8_t cars;               /* railway: carriages (the locomotive + cars - 1) */
    uint8_t unit_first;         /* the first lane of a hazard group */
    mover m[MOVER_MAX];
} lane;

int  lane_is_ground(const lane *l);              /* grass, road, rail, park, pond, nest, hedge: rows */
int  lane_is_safe_kind(const lane *l);           /* grass, nest: nothing moves (grass runs) */
int  lane_offset(const lane *l, uint32_t t);     /* px moved by the lane's movers since frame 0 */
int  mover_pos(const lane *l, int i, uint32_t t);/* loop px (0..LOOP_PX-1) of mover i's top edge */
/* the train: the field px range [*y0, *y1) it covers at frame t (empty if none); returns the phase:
 * 0 quiet, 1 warning (light and bell, not on screen yet), 2 passing (light on) */
int  train_at(const lane *l, uint32_t t, int *y0, int *y1);
int  train_len(const lane *l);
/* hits: does a box [y0, y1) (field px) overlap anything that moves in the lane at frame t? */
int  lane_hits(const lane *l, uint32_t t, int y0, int y1);
/* the platform (river) under a centre point (field px) at t: mover index or -1 */
int  lane_platform(const lane *l, uint32_t t, int centre);
/* can a duck stand at top y (field px) at frame t? (not a tree, on a pad or platform, nothing hitting) */
int  lane_can_stand(const lane *l, uint32_t t, int y, int hit_h);
int  field_y_of_loop(int loop_px);               /* loop px -> field px, in [-LOOP_TOP, LOOP_PX - LOOP_TOP) */

/* the generator: columns are made ahead of the camera in units (a hazard group + a grass run) */
#define RING 64
typedef struct spawn { int32_t col; int8_t row; } spawn;
#define SPAWN_MAX 48
typedef struct gen_state {
    rs_rng rng;
    int32_t next_col;           /* first column not generated yet */
    int32_t start_col;          /* the grass column the next group starts from (the judge's start) */
    int32_t last_nest;          /* column of the last nest pond */
    int32_t next_nest_at;
    int unit;                   /* units made */
    int river_down;             /* direction of the last river lane (alternation) */
    /* the unit being judged (staged over frames) */
    int pending, tries, start_k, ok_starts;
    lane pend[12];
    int npend, ngroup;
    /* statistics */
    int judged, rerolls, fallbacks;
} gen_state;

typedef struct lanes {
    lane ring[RING];
    gen_state g;
    spawn sp[SPAWN_MAX];        /* lost ducklings made by the generator, not yet taken by the world */
    int nsp;
} lanes;

void lanes_init(lanes *L, uint32_t seed);
/* makes columns until next_col > upto (budget: judge starts allowed this call; <0 = unlimited) */
void lanes_generate(lanes *L, int32_t upto, int budget);
const lane *lanes_get(const lanes *L, int32_t col);   /* NULL if not generated or recycled */
int  lanes_diff256(int32_t col);                      /* difficulty 0..256 */

/* the judge: can a duck standing anywhere in start (a mask of rows of column 0 of cols[]) at frame t0
 * reach column ncols-1 within window frames? cols[0] and cols[ncols-1] are grass runs' columns.
 * Returns the frames it took (>= 0) or -1. start_rows: bit r = row r (the duck at y = 16 r). */
int  judge_cross(const lane *cols, int ncols, uint16_t start_rows, uint32_t t0, int window);
/* the same, the duck starting in column start_j of cols[] (e.g. a whole grass run before the group) */
int  judge_cross_from(const lane *cols, int ncols, int start_j, uint16_t start_rows, uint32_t t0, int window);
/* the search itself: start_px / goal_px are 7 words of bits (bit y = a duck's top at field y, 0..208), goal_px NULL =
 * anywhere in column goal_j; arrival (n entries, or NULL) gets the first frame each column is reached (-1 never).
 * Returns the frames to the goal, or -1 within the window. */
#define JUDGE_WORDS 7
void judge_set_land_delay(int frames);            /* a duck must stand this long after landing before a hop (bot: 1) */
int  judge_run(const lane *cols, int ncols, int start_j, const uint32_t *start_px, uint32_t t0, int window,
               int goal_j, const uint32_t *goal_px, int16_t *arrival);
uint16_t lane_free_rows(const lane *l);               /* grass/nest: rows without a tree */

/* ================================ world.c ========================================================== */
enum { HOP_NONE, HOP_FWD, HOP_BACK, HOP_UP, HOP_DOWN };
enum duck_state { DK_READY, DK_ALIVE, DK_HIT, DK_SWEPT, DK_CAUGHT, DK_OUT };

/* a place on the path: a ground cell, or a spot on a platform (river) */
typedef struct place {
    int32_t col;
    int16_t y;                  /* ground: field px of the top (16 * row); platform: offset from its top */
    int8_t plat;                /* mover index, -1 = ground */
    int8_t pad_;
} place;

typedef struct hop {
    int8_t dir, t;              /* HOP_*, frame of the hop (0..dur-1); dir 0 = none */
    int8_t bump, bdir;          /* blocked hop: frames left of the bump, its direction */
    int8_t dur, pad_[3];        /* frames of the hop (HOP_FRAMES; a duckling's is shorter after its wait) */
    place from, to;
} hop;

#define HIST 64
typedef struct duckling {
    uint8_t lag;                /* it goes where its parent was `lag` hops ago */
    int8_t wait;                /* frames before its hop starts (the ripple down the line) */
    int8_t pad_[2];
    place at;                   /* where it stands (or leaves from) */
    hop h;                      /* its hop (h.dir != 0: moving to h.to) */
    int16_t id;                 /* for the drawing (a colour tuft), stable */
    int16_t age;
} duckling;

enum { LOOSE_FREE, LOOSE_WAIT, LOOSE_TUMBLE, LOOSE_FLY, LOOSE_HOME };
typedef struct loose {
    uint8_t state, lock, owner_was, pad_;
    int32_t col; int8_t row, pad2_[3];     /* where it waits (or flies to) */
    int32_t x, y;               /* Q8 world px of the top-left (while flying / tumbling) */
    int16_t t, id;
} loose;
#define LOOSE_MAX 40

typedef struct duck {
    int state, t;               /* enum duck_state, frames in it */
    place at;                   /* where it stands (its hop leaves from here) */
    hop h;
    int queued;                 /* a buffered press (HOP_*) */
    int32_t max_col;            /* the furthest column reached */
    int score, banked, bank_best, lanes;
    int hops, idle;             /* hops made; frames since the last hop */
    place hist[HIST];           /* hist[0] = where it stands or lands; hist[k] = k hops ago */
    int nhist;
    duckling line[LINE_MAX];
    int nline;
    int catchup_t;              /* frames since the line was last complete */
    int death_col, death_y;     /* where it died (field px) */
    int fox_warn;               /* 1 while the fox watches it */
    int land_t;                 /* frames since it landed */
} duck;

/* events of the last step, per player (bits) */
enum {
    EV_HOP = 1, EV_BUMP = 2, EV_PICK = 4, EV_KNOCK = 8, EV_SPLASHLING = 16, EV_BANK = 32, EV_HIT = 64,
    EV_SWEPT = 128, EV_CAUGHT = 256, EV_FOXWARN = 512, EV_LAND = 1024, EV_START = 2048, EV_RIDE = 4096,
    EV_LOST = 8192
};

typedef struct world {
    lanes L;
    uint32_t t;                 /* frames since the run started (the lanes' clock) */
    int32_t cam;                /* Q8 world px of the screen's left edge */
    int players, started, over_t;
    duck d[MAX_PLAYERS];
    loose ls[LOOSE_MAX];
    int next_id;
    int events[MAX_PLAYERS];
    int ev_pick_n[MAX_PLAYERS], ev_bank_n[MAX_PLAYERS];   /* line length at the last pick / bank */
    int ev_knock_x[MAX_PLAYERS];                           /* screen x of the last knock (sound pan) */
    int bank_t[MAX_PLAYERS];    /* frames since the last banking (-1: none) */
    int bank_col[MAX_PLAYERS], bank_row[MAX_PLAYERS];
    uint32_t seed;
    int fixed_seed;
    int nest_ducklings;         /* banked ducklings swimming in the visible nest ponds (drawing) */
} world;

void world_init(world *w, int players, uint32_t seed);
/* one frame: press[p] = HOP_* pressed this frame (0 none) */
void world_step(world *w, const int press[MAX_PLAYERS]);
int  world_running(const world *w);          /* someone is alive and the run started */
int  world_all_out(const world *w);          /* everyone is out and their death animation is done */
int  world_cam_px(const world *w);           /* the camera's world px */
int  world_score(const world *w, int p);     /* parent p: its own lanes (furthest column) + its banked points */
/* the family score (shared): the furthest lane any parent reached + every parent's banked points (1 player: the
 * player's score) */
int  world_total(const world *w);
const lane *world_lane(const world *w, int32_t col);
/* the visual position of a duck (world px of the top-left, arc height) at this frame */
void duck_visual(const world *w, const duck *d, int *x, int *y, int *lift, int *squash16);
void duckling_visual(const world *w, const duck *parent, const duckling *k, int *x, int *y, int *lift);
int  place_field_y(const world *w, const place *p);   /* resolve a place to field px (top) now */
int  world_line_total(const world *w);       /* ducklings in all lines */
int  world_fox_state(const world *w, int *target);    /* 0 none, 1 warning, 2 pounce; target: the parent furthest behind */
int  medal_of(int score);                    /* 0 none, 1 bronze .. 4 pearl */

/* ================================ console side ======================================================= */
/* sfx.c */
enum { SFX_QUACK0, SFX_QUACK1, SFX_QUACK2, SFX_QUACK3, SFX_PEEP, SFX_HORN, SFX_SPLASH, SFX_BELL, SFX_FANFARE,
       SFX_CLICK, SFX_THUD, SFX_GROWL, SFX_BUMP, SFX_SWISH, SFX_SPARKLE, SFX_JOIN, SFX_PAUSE, SFX_PLOP,
       SFX_COUNT };
/* instrument samples of the music layers (after the sfx) */
enum { SMP_TRUMPET = SFX_COUNT, SMP_GLOCK, SMP_COUNT };
void sfx_init(void);
void sfx(int id);
void sfx_at(int id, int x, int pitch);       /* x = screen x (pan), pitch 0 = RS_PITCH_1 */
void music_start(void);
void music_stop(void);
void music_update(int layers);               /* per frame: the game's layers in step with the module */
void audio_set(int music, int sound);

/* draw.c */
enum { DS_TITLE, DS_READY, DS_PLAY, DS_DEAD, DS_OVER };
void draw_init(void);
void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused);
void draw_reset(const world *w);

/* bot.c: plays from the screen (OAM and the BG2 map) only */
int  bot_decide(int player);
void bot_reset(void);

/* save states */
void draw_state(void);
void sfx_state(void);
void bot_state(void);
void bot_state_loaded(void);

extern int opt_bot;
const world *dp_test_world(int *state);

#endif
