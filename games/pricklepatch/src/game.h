/*
 * Prickle Patch: shared declarations and the tuning (every gameplay number is here or in world.c's tables).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pricklepatch/LICENSE.
 *
 * world.c is the rules: the gardener, the pests and their spawner, the tools (weapons), the passives, the gems, the
 * pickups, the level-ups and the chests. It calls no drawing or sound and keeps its own RNG: deterministic on the frame
 * clock, the same at every screen size (the pests spawn on a ring of the world, not at the screen's edge). soft.c is
 * the soft sprite layer (the swarm drawn into BG tiles), draw.c the video, main.c the flow (the title, the shed, the
 * run, the level-ups, the results), input (the pad, the floating touch stick, the bot), sound, save RAM, save states.
 */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "rs.h"

/* ---- units ------------------------------------------------------------------------------------------------------- */
#define FX           16             /* world positions: 1 px = 16 sub-pixels (Q4); speeds in sub-pixels per frame */
#define SEC          60             /* frames per second */
#define RUN_FRAMES   (15 * 60 * SEC)    /* the final boss comes at 15:00 */

/* ---- the gardener ------------------------------------------------------------------------------------------------- */
#define HERO_R       7              /* px: the body's radius against the pests */
#define HURT_IFRAMES 30             /* frames of safety after a hit */
#define PICKUP_R     30             /* px: the gems fly to the gardener from this far (the magnet passive grows it) */
#define TOUCH_R      14             /* px: a chest, an apple, a magnet is picked up this close */
#define GEM_ACCEL    5              /* a gem flying to the gardener: its speed grows by this (Q4) each frame ... */
#define GEM_MAXV     112            /* ... up to 7 px/f */
#define REGEN_TICK   (SEC * 2)      /* the honey's regeneration heals every 2 s */
#define NCHAR        3              /* the hedgehog, the tortoise, the hen */
enum { CHR_HEDGEHOG, CHR_TORTOISE, CHR_HEN };

/* ---- the field ---------------------------------------------------------------------------------------------------- */
#define SPAWN_RX     300            /* px: the pests come from an ellipse around the gardener (outside every screen) */
#define SPAWN_RY     236
#define FAR_R        560            /* px: a pest further than this is brought back to the ring */
#define GRID_CELL    32             /* px: the collision grid's buckets ... */
#define GRID_N       40             /* ... GRID_N x GRID_N of them, centred on the gardener (1280 px) */
#define SPAWN_RATE(m) (2 + ((m) < 15 ? (m) : 15))  /* pests a second at most, minute m (the field fills up to its target) */
#define SEPARATE_R   11             /* px: two pests push apart closer than 3/4 of their radii, this at most */

/* ---- capacities ------------------------------------------------------------------------------------------------------ */
#define MAX_EN       360            /* pests alive */
#define MAX_SHOT     220            /* shots and zones */
#define MAX_GEM      420            /* XP gems on the ground or flying */
#define MAX_ITEM     24             /* apples, magnets, coins, chests */
#define MAX_WEAPONS  6
#define MAX_PASSIVES 6
#define MAX_LEVEL    5              /* a tool or a passive; a tool at 5 with its passive evolves */
#define MAX_POPS     16             /* hits reported to draw.c each step (the number pops) */
#define MAX_KILLFX   24             /* kills reported each step (the poofs) */

/* ---- pests -------------------------------------------------------------------------------------------------------------- */
enum { ET_SLUG, ET_APHID, ET_BEETLE, ET_ANT, ET_WASP, ET_THISTLE, ET_CROW, ET_LOCUST, ET_QUEEN, ET_STAG, ET_CROWKING,
       ET_COUNT };
#define ET_BOSS0     ET_QUEEN
typedef struct enemy_info {
    int32_t hp;                     /* at minute 0 (grows with the time: hp_scale) */
    int8_t speed;                   /* Q4 px per frame */
    int8_t dmg;                     /* a touch's damage */
    int8_t r;                       /* px: its body's radius */
    int8_t xp;                      /* the gem it drops */
    int8_t knock;                   /* 0..8: how far a hit pushes it (8: fully) */
    uint8_t frame;                  /* its soft-layer art (SF_*) */
} enemy_info;
extern const enemy_info EN_INFO[ET_COUNT];
typedef struct enemy {
    int32_t x, y;                   /* Q4 */
    int32_t hp, hpmax;
    int16_t kx, ky;                 /* knock-back, Q4 per frame (decays) */
    int16_t ax, ay;                 /* a set heading (ants marching, a crow's swoop, a boss's charge), Q4 per frame */
    uint16_t lh;                    /* the last shot that hit it (a shot hits a pest once) */
    uint8_t type, elite, flash, face;   /* face: 1 = looks left */
    uint8_t cd_orbit, cd_zone;      /* cooldowns of the bees and of the flames */
    uint8_t st;                     /* its AI state */
    uint8_t anim;
    uint16_t st_t;                  /* frames in that state */
} enemy;

/* ---- tools (weapons) and passives -------------------------------------------------------------------------------------- */
enum { W_SEED, W_CAN, W_BEES, W_MOWER, W_RAKE, W_PUMPKIN, W_THORNS, W_CHILI, W_PUFF, W_COUNT };
enum { P_COMPOST, P_HAT, P_BOOTS, P_HONEY, P_MAGNET, P_SUNDIAL, P_POUCH, P_CLOVER, P_COUNT };
extern const uint8_t EVOLVE_WITH[W_COUNT];      /* the passive a tool evolves with */
typedef struct weapon { uint8_t id, level, evo, pad; int16_t timer, aux; } weapon;

/* shots: projectiles and damage zones */
enum { SK_SEED, SK_SUNSEED, SK_SPLASH, SK_MOWER, SK_RAKE, SK_PUMPKIN, SK_BLAST, SK_FLAME, SK_PUFF, SK_THORNRING,
       SK_COUNT };
typedef struct shot {
    int32_t x, y;                   /* Q4 */
    int16_t vx, vy;                 /* Q4 per frame */
    int16_t dmg;
    uint16_t id;                    /* its serial (enemy.lh) */
    int16_t life;                   /* frames left */
    uint8_t kind, pierce;           /* pierce: hits left (255: any number) */
    uint8_t r, t;                   /* its radius px; its age */
    int16_t aux, aux2;              /* per kind (the pumpkin's target, the rake's turn, the splash's direction) */
} shot;

/* gems and pickups */
typedef struct gem { int32_t x, y; int16_t val, v; uint8_t fly, pad; } gem;   /* x, y: Q4 (on the ground: the 8-px cell's centre) */
enum { IT_APPLE, IT_MAGNET, IT_COIN, IT_CHEST };
typedef struct item { int32_t x, y; uint8_t kind, big; uint16_t t; } item;

/* a level-up's or a chest's choice */
enum { CH_NONE, CH_WNEW, CH_WUP, CH_EVOLVE, CH_PNEW, CH_PUP, CH_HEAL, CH_COINS };
typedef struct choice { uint8_t kind, id, level, pad; } choice;

/* ---- the gardener ------------------------------------------------------------------------------------------------------- */
typedef struct hero {
    int32_t x, y;                   /* Q4 */
    int16_t vx, vy;                 /* this frame's step */
    int16_t dx, dy;                 /* the last direction moved (64 = 1) */
    int32_t hp, hpmax;              /* Q4 */
    uint8_t hurt_t, face, moving, pad;
    uint16_t walk_t;
} hero;

/* what the meta-progression (the shed's boosts, save RAM) gives a run */
enum { B_MIGHT, B_VIGOR, B_SWIFT, B_REACH, B_GROWTH, B_HASTE, B_COUNT };
#define BOOST_MAX    5

/* ---- the world ------------------------------------------------------------------------------------------------------------ */
typedef struct pop { int16_t x, y, n; uint8_t big, pad; } pop;     /* x, y: world px */
typedef struct killfx { int16_t x, y; uint8_t type, elite; } killfx;
typedef struct world {
    hero h;
    uint8_t chr, map, boosts[B_COUNT];
    int32_t t;                      /* frames of the run */
    rs_rng rng;
    uint32_t seed;
    enemy en[MAX_EN];
    int n_en;
    shot sh[MAX_SHOT];
    int n_sh;
    uint16_t serial;
    gem gm[MAX_GEM];
    int n_gm;
    item it[MAX_ITEM];
    int n_it;
    weapon wp[MAX_WEAPONS];
    int n_wp;
    uint8_t pas[P_COUNT];           /* each passive's level (0: not owned) */
    uint8_t pas_order[MAX_PASSIVES];
    int n_pas;
    /* derived stats (stats_update) */
    int dmg_mul, area_mul, cd_mul, xp_mul, coin_mul;      /* Q8 (256 = 1) */
    int speed, pickup, amount, armor, regen;              /* speed Q4 px/f; pickup px; regen Q4 hp per tick */
    /* progress */
    int level, xp, xp_need, kills, coins, pending;        /* pending: level-ups waiting for a choice */
    int regen_t, magnet_t;          /* magnet_t: an active magnet pulls every gem */
    int boss, boss_type, boss_hpmax;    /* the boss alive (an index), -1 none */
    int bosses_done;                /* bits: the bosses beaten */
    uint32_t events_done;           /* bits: the spawner's events already played */
    int hitstop, won, dead, chest_open;   /* chest_open: a chest was just touched (main.c shows it) */
    choice offer[4];
    int n_offer;
    choice chest[3];
    int n_chest;
    int spawn_acc;                  /* the spawner's accumulator */
    /* reported each step to draw.c and main.c */
    uint32_t events;                /* EV_* */
    pop pops[MAX_POPS];
    int n_pops;
    killfx kfx[MAX_KILLFX];
    int n_kfx;
    int warn;                       /* a boss arrives (its type + 1) */
    int gems_got;                   /* gems collected this step (the chime's pitch) */
} world;
enum {
    EV_SHOT = 1, EV_HIT = 2, EV_KILL = 4, EV_GEM = 8, EV_LEVEL = 16, EV_HURT = 32, EV_SPLASH = 64, EV_BOOM = 128,
    EV_MOWER = 256, EV_CHEST = 512, EV_BOSS = 1024, EV_BOSSKILL = 2048, EV_COIN = 4096, EV_HEAL = 8192,
    EV_DEAD = 16384, EV_WIN = 32768, EV_MAGNET = 65536, EV_RAKE = 131072, EV_PUFF = 262144, EV_ELITE = 524288
};

/* world.c */
void world_init(world *w, int chr, int map, const uint8_t boosts[B_COUNT], uint32_t seed);
void world_step(world *w, int ix, int iy);     /* ix, iy: the move, -64..64 each (length <= 64) */
void world_offer(world *w);                   /* the level-up's choices (w->offer, n_offer) */
void world_choose(world *w, int k);           /* take offer k */
void world_apply(world *w, const choice *c);
void world_open_chest(world *w, int n);      /* a chest: n upgrades (w->chest, n_chest), applied */
void bees_pos(const world *w, const weapon *p, int k, int n, int *x, int *y);   /* bee k of n, world px */
int  choice_icon(const choice *c);            /* draw.c: its icon (SPR_*) */
uint32_t world_hash(const world *w);
int  hp_scale(int32_t t);                     /* the pests' health at frame t, Q8 */
int  weapon_level_of(const world *w, int id);    /* 0: not owned */
int  xp_for(int level);                       /* the gems from level to level + 1 */
/* the run's snapshot for CONTINUE (save RAM): what a run needs to go on, without its pests and gems */
typedef struct snapshot {
    int8_t chr, map;
    uint8_t level, n_wp, n_pas, pad;
    uint16_t kills, coins;
    int32_t t, xp, hp;
    uint32_t seed, events_done;
    uint8_t bosses_done, pad2[3];
    uint8_t wp[MAX_WEAPONS][3];     /* id, level, evo */
    uint8_t pas[MAX_PASSIVES][2];   /* id, level */
} snapshot;
void world_snapshot(const world *w, snapshot *s);
void world_resume(world *w, const snapshot *s, const uint8_t boosts[B_COUNT]);
void world_skip_to(world *w, int s);         /* (tests) the events before second s done */

/* ---- soft.c: the soft sprite layer (the swarm drawn into BG2's tiles) ------------------------------------------------- */
void soft_setup(int map_w, int map_h);        /* the layer's map size (64 or 128 x 32 or 64), its pool cleared */
void soft_begin(int cam_x, int cam_y, int x0, int y0, int x1, int y1);  /* world px: the camera; the window drawn */
void soft_draw(int frame, int x, int y, int flip, int remap);   /* world px of the frame's top-left; remap: REMAP_* */
void soft_end(void);
int  soft_overflow(void);                     /* the frames that dropped pixels (the pool full), so far */
int  soft_used(void);                         /* the most tiles a frame used, so far */
void soft_state(void);
enum { REMAP_NONE, REMAP_FLASH, REMAP_ELITE, REMAP_COUNT };

/* ---- draw.c ------------------------------------------------------------------------------------------------------------ */
enum { ST_TITLE, ST_PLAY, ST_LEVELUP, ST_CHEST, ST_SHED, ST_OVER };
#define RETRY_LOCK   36             /* frames the results ignore the buttons */
#define MENU_LOCK    18             /* frames a level-up's choices ignore the buttons (no accidental pick) */
typedef struct view {
    int state, st_t, paused, sel, chr, map, coins, best_secs, best_kills, run_secs, run_level, run_chr, can_continue;
    int unlocked;                   /* bits: the gardeners bought */
    int new_best, earned, shed_sel, maps_open;
    int boosts[B_COUNT];
    int stick, stick_x0, stick_y0, stick_x, stick_y;      /* the floating touch stick (screen px) */
    int first_run;                  /* the first run: the controls' hint */
    int bot;
} view;
void draw_init(void);
void draw_frame(const world *w, const view *v);
void draw_events(const world *w);   /* after each world step: the effects of its events */
void draw_state(void);
void draw_state_loaded(void);
void draw_reset_run(void);          /* a run starts: the effects cleared */
/* where the menus' lines are (for the touches), filled by draw_frame: screen rows of the UI layer */
typedef struct hot { int y0, y1, x0, x1; } hot;
#define HOT_MAX 8
extern hot draw_hot[HOT_MAX];
enum { HOT_PREV, HOT_NEXT, HOT_PROMPT, HOT_NEWRUN, HOT_SHED, HOT_MAP, HOT_QUIT };   /* the title's; HOT_QUIT: paused (the shed: its rows) */
const char *weapon_name(int id, int evo);
const char *passive_name(int id);
int  shed_cost(int boost, int level);
int  char_cost(int chr);

/* ---- sound (main.c) -------------------------------------------------------------------------------------------------- */
enum { SFX_SHOT, SFX_HIT, SFX_POP, SFX_GEM, SFX_LEVEL, SFX_HURT, SFX_SPLASH, SFX_BOOM, SFX_CHEST, SFX_BOSS, SFX_RAKE,
       SFX_COUNT };
#define SFX_HOUSE SFX_COUNT

#endif
