/*
 * Lemur Leap: shared declarations and the tuning (every gameplay number is here).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/lemurleap/LICENSE.
 *
 * world.c is the rules: the lemur's bounce, the leaves (the platforms) and their generator, the coins, the power-ups,
 * the pests, the seed shots, the camera, the bot. It calls no SDK function (its own RNG, no drawing, no sound):
 * deterministic on the frame clock, the same at every screen size, and linked alone by tests/test_world.c. draw.c is
 * the video, main.c the flow (the title and its wardrobe, the climb, the results, pause), input (the pad, the touches,
 * the bot), sound, save RAM, save states.
 */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>

/* ---- units --------------------------------------------------------------------------------------------------------- */
#define FX          256             /* world positions: 1 px = 256 sub-pixels (Q8); speeds in sub-pixels per frame */
#define SEC         60
#define COL_W       192             /* px: the column the lemur climbs in; it wraps (out on one side, in on the other) */
#define STAGE_H     240             /* px: the camera's window (the stage): the same at every screen size */
#define PX_PER_M    10              /* the height in metres: 10 px a metre */
#define CAM_LINE    104             /* px: the camera rises to keep the lemur's head at least this far down the stage */
#define FALL_LINE   (STAGE_H + 10)  /* px: feet below this stage line: the lemur has fallen out (the run is over) */
#define FALL_FRAMES 70              /* the fall the camera follows, then the results */
#define GEN_AHEAD   600             /* px: leaves are made this far above the stage's top (every screen: a tall one shows 240 more) */
#define KEEP_BELOW  300             /* px: ... and kept this far below its bottom (they fly past on a fall) */

/* ---- the lemur ----------------------------------------------------------------------------------------------------- */
#define HERO_HW     6               /* half width of its body (px) against pests and pickups */
#define HERO_H      18              /* its body's height (px), feet to the top of its head */
#define FEET_HW     6               /* half width of its feet against the leaves (generous) */
#define GRAVITY     52              /* 0.20 px/f^2 */
#define JUMP_V      1490            /* 5.8 px/f: a leaf's bounce, 80 px high (8 m), 28 frames up */
#define SHROOM_V    2010            /* 7.9 px/f: a mushroom, 152 px */
#define SPRING_V    2620            /* 10.2 px/f: a spring, 258 px */
#define CLOUD_V     1180            /* 4.6 px/f: a cloud's soft bounce, 52 px */
#define STOMP_V     1560            /* a pest stomped: 92 px */
#define MAX_FALL    1900            /* 7.4 px/f */
#define MAX_VX      704             /* 2.75 px/f */
#define ACCEL       84              /* 8-9 frames to full speed (snappy) ... */
#define DECEL       40              /* ... a little inertia when released (18 frames to a stop) */
#define TURN        (ACCEL + DECEL)
#define REACH       64              /* px: the generator's highest step between two leaves of the path (a bounce: 80) */
#define CLOUD_REACH 38              /* ... after a cloud (52) */
#define PATH_MARGIN 16              /* px: the path's sideways steps keep this much of the lemur's reach spare */
#define SHOT_V      2048            /* 8 px/f: a seed flies up */
#define SHOT_COOL   12              /* frames between two seeds (a press) ... */
#define SHOT_REPEAT 22              /* ... and while A is held */
#define SHOT_LIFE   44
#define MAX_SHOTS   4
#define PROP_V      1400            /* the propeller: 5.5 px/f up for PROP_FRAMES */
#define PROP_FRAMES 150
#define ROCKET_V    2240            /* the rocket: 8.75 px/f up for ROCKET_FRAMES */
#define ROCKET_FRAMES 170
#define FLY_ACCEL   120
#define SHIELD_FRAMES 720           /* the bubble: 12 s, or one touch */
#define MAGNET_FRAMES 600           /* the magnet: 10 s */
#define MAGNET_R    80              /* px: coins fly to the lemur from this far */
#define COIN_R      11              /* px: a coin is picked up this close (centre to the body's centre) */
#define PEST_COINS  3               /* a pest popped */

/* ---- the zones (metres) ------------------------------------------------------------------------------------------- */
enum { Z_GARDEN, Z_CLOUDS, Z_SKY, Z_SPACE, ZONES };
#define ZONE_CLOUDS 200
#define ZONE_SKY    500
#define ZONE_SPACE  900

/* ---- capacities ---------------------------------------------------------------------------------------------------- */
#define MAX_PLAT    128
#define MAX_COIN    64
#define MAX_PEST    10
#define MAX_FX      24

/* ---- the leaves (platforms) ---------------------------------------------------------------------------------------- */
enum { P_LEAF, P_MOVE, P_DRY, P_PUFF, P_SHROOM, P_CLOUD, P_TYPES };
enum { PS_OK, PS_BROKEN, PS_GONE };
enum { IT_NONE, IT_PROP, IT_ROCKET, IT_SHIELD, IT_MAGNET, IT_TYPES };
typedef struct plat {
    int32_t x, y;                   /* Q8: the left end, the top (where the feet land) */
    int16_t vx;                     /* Q8 per frame (a moving leaf, a drifting cloud) */
    int16_t x0, x1;                 /* px: the bounds of its left end (moving) */
    int16_t fall_v;                 /* a broken dry leaf falling */
    uint8_t type, w;                /* P_*, its width (px) */
    uint8_t state, t;               /* PS_*, an animation timer (bounced, cracked) */
    uint8_t spring;                 /* 0: none, else the spring's left (px from the leaf's left) + 1 */
    uint8_t item;                   /* IT_* sitting on it */
    uint8_t path;                   /* on the generator's guaranteed path (the tests) */
    uint8_t used;
} plat;

typedef struct coin { int32_t x, y; uint8_t used, fly; uint8_t pad[2]; } coin;   /* Q8: the centre */

enum { E_BEE, E_STORM, E_BAT, E_UFO, E_TYPES };
typedef struct pest {
    int32_t x, y;                   /* Q8: the centre (x moves, y bobs) */
    int16_t x0, y0, amp;            /* px: the hover's centre and its swing */
    uint16_t phase, period;
    uint8_t type, used, dead_t, pad;
} pest;

typedef struct shot { int32_t x, y; uint8_t used, t; uint8_t pad[2]; } shot;

/* what the step did, for the effects (draw.c) */
enum { FX_DUST, FX_SPRING, FX_BREAK, FX_PUFF, FX_COIN, FX_POP, FX_STOMP, FX_ITEM, FX_SHIELD, FX_FLY, FX_CLOUD };
typedef struct fxev { int16_t x, y; uint8_t kind, arg; } fxev;   /* world px */

typedef struct hero {
    int32_t x, y;                   /* Q8: the centre of the feet; x in 0 .. COL_W * FX (wraps) */
    int32_t vx, vy;
    int8_t face;                    /* -1 left, 1 right */
    uint8_t land_t;                 /* the landing's squash (frames left) */
    uint8_t shoot_t, cool;          /* the shooting pose; the seed's cooldown */
    uint8_t fly;                    /* 0, IT_PROP, IT_ROCKET */
    uint8_t spin_t;                 /* a spring's somersault */
    uint8_t knocked;                /* hit by a pest: tumbling down, nothing holds it */
    uint8_t held;                   /* last frame's input (edges) */
    uint16_t fly_t, shield_t, magnet_t;
    uint8_t land_i, pad;             /* the leaf last landed on + 1 (0: the ground) */
    int32_t land_y;                 /* px: where the feet last landed (the bot) */
} hero;

typedef struct world {
    uint32_t rng;                   /* xorshift32 */
    uint32_t seed;
    int32_t t;                      /* frames since the run began */
    hero h;
    int32_t cam_y;                  /* px: the world y of the stage's top line */
    int32_t top_y;                  /* px: the highest the feet have been (the height) */
    int32_t gen_y;                  /* px: the last row made */
    int32_t last_path_y;            /* px: the last leaf of the path */
    int32_t last_pest_y, last_item_y;
    int16_t last_path_l, last_path_r;   /* px: the last leaf of the path's run (its sweep, moving) */
    uint8_t last_path_type, zone;
    int32_t best_m;                 /* the record to pass (its marker), metres; 0 none */
    uint8_t best_passed, over, god, bot_aim;    /* bot_aim: the bot's leaf + 1 (it keeps to it) */
    int32_t fall_t;                 /* frames since the fall out began */
    int32_t coins, kills, bounces;
    plat p[MAX_PLAT];
    coin c[MAX_COIN];
    pest e[MAX_PEST];
    shot s[MAX_SHOTS];
    uint32_t events;                /* EV_* of the last step */
    fxev fx[MAX_FX];
    int n_fx;
} world;
enum {
    EV_BOUNCE = 1, EV_SPRING = 2, EV_SHROOM = 4, EV_CLOUD = 8, EV_BREAK = 16, EV_PUFF = 32, EV_COIN = 64,
    EV_SHOT = 128, EV_POP = 256, EV_STOMP = 512, EV_HURT = 1024, EV_ITEM = 2048, EV_SHIELD_POP = 4096,
    EV_FALL = 8192, EV_ZONE = 16384, EV_BEST = 32768, EV_FLY_END = 65536, EV_MAGNET = 131072
};

/* the input bits world_step reads */
enum { IN_LEFT = 1, IN_RIGHT = 2, IN_SHOOT = 4 };

/* world.c */
void world_init(world *w, uint32_t seed, int best_m);
void world_step(world *w, int input);
int  world_height_m(const world *w);            /* metres climbed */
int  zone_of(int metres);
uint32_t world_hash(const world *w);
int  bot_input(world *w);                       /* the bot's input for this frame (reads the world only) */
int  plat_reach(int type);                      /* px: the highest step the path may take after a leaf of this type */
int  wrap_dx(int dx);                           /* a horizontal distance across the wrap: -COL_W/2 .. COL_W/2 */
int  bounce_v(int type);                        /* a leaf's bounce speed (Q8) */
int  frames_down(int v, int gap);               /* frames from a bounce at v until the feet come down gap px higher */
int  side_reach(int t);                         /* px sideways in t frames from a standstill */
int  span_gap(int a, int b, int c, int d);      /* px between two column spans across the wrap (0: they overlap) */
void world_warp(world *w, int metres, int fall);  /* (tests, screenshots) the climb from that height */
void world_generate(world *w);                  /* the rows up to GEN_AHEAD above the camera (world_step does it; tests) */

/* ---- the wardrobe (cosmetic: coins buy them) -------------------------------------------------------------------- */
#define HATS        8               /* 0 none */
#define FURS        4               /* 0 the ring-tailed grey */
#define LOOKS       (HATS + FURS)   /* the title's list: the hats, then the furs */
extern const int16_t LOOK_COST[LOOKS];

/* ---- draw.c ----------------------------------------------------------------------------------------------------- */
enum { ST_TITLE, ST_PLAY, ST_OVER };
#define RETRY_LOCK  36
#define HOT_MAX     8
enum { HOT_PROMPT, HOT_PREV, HOT_NEXT, HOT_BUY, HOT_NEWRUN, HOT_QUIT };
typedef struct hot { int16_t x0, y0, x1, y1; } hot;   /* a tap zone, screen pixels */
extern hot draw_hot[HOT_MAX];
typedef struct view {
    int state, st_t, paused;
    int look, hat, fur;             /* the title: the look shown; the hat and fur worn */
    int owned;                      /* bits: the looks bought */
    int coins, best_m, best_coins, runs;
    int can_continue, run_m;        /* a run kept (the title: CONTINUE) */
    int new_best, earned;           /* the results */
    int touch;                      /* the hint names the touches */
    int first_run;
} view;
void draw_init(void);
void draw_frame(const world *w, const view *v);
void draw_events(const world *w);
void draw_reset_run(void);
void draw_state(void);
void draw_state_loaded(void);
const char *look_name(int k);

/* ---- sound (main.c) ---------------------------------------------------------------------------------------------- */
enum { SFX_BOUNCE, SFX_SPRING, SFX_SHROOM, SFX_CLOUD, SFX_BREAK, SFX_PUFF, SFX_COIN, SFX_SHOT, SFX_POP, SFX_STOMP,
       SFX_HURT, SFX_ITEM, SFX_SHIELD, SFX_FALL, SFX_PROP, SFX_ZONE, SFX_COUNT };
#define SFX_HOUSE SFX_COUNT

#endif
