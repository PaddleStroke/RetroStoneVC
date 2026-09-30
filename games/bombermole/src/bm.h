/*
 * Bomber Mole: shared definitions.
 * All rights reserved, 8BCraft.
 */
#ifndef BM_H
#define BM_H

#include "rs.h"
#include "assets.h"

#define GW 20                   /* playfield cells */
#define GH 14
#define NDEPTH 3
#define CELL 16
#define HUD_H 16
#define SUB 256                 /* movement progress units per cell */
#define MAX_ACTORS 48
#define MAX_BOMBS 32
#define MAX_FX 64
#define MAX_LOGS 8
#define MAX_PLAYERS 4
#define MAX_SWARMS 4
#define GAS_TIME 180            /* a gas cloud stuns for 3 s */
#define MAX_HARV 4
#define MAX_PUMPKINS 12
#define MAX_CARTS 4
#define MAX_SNOWBALLS 12
#define NCHAN 8

enum { SEASON_SPRING, SEASON_SUMMER, SEASON_AUTUMN, SEASON_WINTER, SEASONS };
enum { DIR_UP, DIR_RIGHT, DIR_DOWN, DIR_LEFT, DIR_NONE };
extern const int DX[4], DY[4];

enum terrain {
    TR_FLOOR, TR_STONE, TR_DIRT, TR_ROCK, TR_ROOTS, TR_FROZEN, TR_LEAVES, TR_WATER, TR_PUDDLE,
    TR_THIN, TR_HOLE_DOWN, TR_HOLE_UP, TR_LADDER, TR_EXIT, TR_BRIDGE, TR_ICE, TR_THIN_ICE, TR_MUD,
    TR_COVER, TR_BURNT, TR_GATE, TR_PLATE, TR_LEVER, TR_VENT, TR_PIPE, TR_CRATE, TR_SPRINKLER,
    TR_WINDMILL, TR_HIVE, TR_GAS, TR_TREE, TR_SHROOM, TR_RAIL, TR_NEST, TR_PLUG,
    TR_SNOW, TR_WELL, TR_CRANK, TR_PERCH, TR_COUNT
};
enum item { IT_NONE, IT_GRUB, IT_BOMB, IT_FIRE, IT_SPEED, IT_REMOTE, IT_HEART, IT_APPLE, IT_COUNT };
enum actor_kind { AK_NONE, AK_MOLE, AK_FERRET, AK_CAT, AK_BOSS, AK_DOG, AK_CROC, AK_ANTS };
enum push_kind { PUSH_NONE = 0, PUSH_WIND = 1, PUSH_FLOW = 2 };
enum game_mode { MODE_SOLO, MODE_COOP, MODE_BATTLE };   /* multiplayer hooks */

/* ---- data-driven enemy types (DESIGN.md, "Enemy tiers") ---- */
enum { MOVE_WANDER, MOVE_PATROL, MOVE_CHASE };
enum { AWARE_NONE, AWARE_LATE, AWARE_ALWAYS };
typedef struct enemy_type {
    const char *name;           /* level-file word */
    uint8_t kind;               /* AK_FERRET or AK_CAT (the base sprite) */
    uint8_t tier;               /* 1..4 */
    uint8_t variant;            /* palette variant VAR_* */
    int16_t speed;              /* progress per frame (the mole walks at 20) */
    uint8_t move;               /* MOVE_* */
    uint8_t vision;             /* cells; 0 = never chases */
    uint8_t los;                /* 1: needs a clear straight line (cats); 0: path length (ferrets smell) */
    uint8_t aware;              /* AWARE_*: bombs */
    uint8_t aware_fuse;         /* AWARE_LATE: only bombs whose fuse is below this (frames) */
    uint8_t react;              /* reaction delay before fleeing (frames) */
    uint8_t pounce, cooldown;   /* cats: pounce length (cells), rest after a pounce (frames) */
    uint8_t hits;               /* blasts to defeat */
} enemy_type;
enum { ET_SLEEPY_FERRET, ET_FERRET, ET_POLECAT, ET_STOAT, ET_GINGER_CAT, ET_GREY_CAT, ET_BLACK_CAT,
       ET_SIAMESE_CAT, ET_COUNT };
extern const enemy_type ENEMY_TYPES[ET_COUNT];
int enemy_type_for(int kind, int tier);            /* the tier's type of a kind */
enum { DIFF_EASY, DIFF_NORMAL, DIFF_HARD };

/* One grid cell. */
typedef struct cell {
    uint8_t t;                  /* enum terrain */
    uint8_t item;               /* enum item (visible on floor, or hidden inside a block) */
    uint8_t pushdir;            /* DIR_* + 1 when this cell is a push field, else 0 */
    uint8_t pushkind;           /* PUSH_WIND or PUSH_FLOW */
    uint8_t chan;               /* switch channel (plates/levers/gates), pipe pair */
    uint8_t state;              /* gate open, lever on, thin-ice cracks, sprinkler dir, dig progress */
    uint16_t timer;             /* regrow / timed gate / splat */
    uint8_t regrow;             /* terrain to restore when timer runs out (0 = none) */
    uint8_t timed;              /* gate: open time in 1/10 s when triggered (0 = follows channel) */
    uint8_t blow;               /* windmill: DIR_* + 1 the wind blows (away from it; 0 = down) */
    uint8_t lane;               /* 1 when pushdir comes from a windmill's lane (recomputed) */
    uint8_t river;              /* winter: ice over a river (a blast opens it; the crocodile swims under it) */
    uint8_t icicle;             /* winter, tunnels: an icicle hangs above this cell */
} cell;

typedef struct spawn {
    uint8_t kind, depth, x, y, asleep, player;
    int8_t etype;               /* ET_* or -1: the level's default tier */
} spawn;

typedef struct level_def {
    char name[48], hint[100], music[16], file[32];
    int season, arc, num, bombs, range, speed, stub;
    int gust_period, gust_active, gust_step;
    int vent_period, vent_active;
    int dark;                   /* night / dark caves: lamp radius only */
    int croc_hp;                /* crocodiles: hits to defeat (0 = cannot be defeated, only stunned) */
    int fog;                    /* fog: the clear radius around the mole in pixels (0 = none) */
    struct { uint8_t depth, x, y; } pumpkins[MAX_PUMPKINS];
    int npumpkins;
    struct { uint8_t depth, x, y; } carts[MAX_CARTS];
    int ncarts;
    struct { uint8_t depth, x, y; } snowballs[MAX_SNOWBALLS];
    int nsnowballs;
    int night;                  /* winter night: the helmet lamp's radius in pixels (0 = day) */
    int boss;                   /* BOSS_* from assets.h */
    int tier;                   /* default enemy tier (1..4) for F and C */
    cell g[NDEPTH][GH][GW];
    spawn sp[64];
    int nsp;
    struct { uint8_t depth, x, y; } logs[MAX_LOGS];
    int nlogs;
    struct { uint8_t depth, x, y, dir; } harv[MAX_HARV];   /* harvesters: parking cell, first sweep */
    int nharv;
    int harvest_period, harvest_warn;                      /* frames between sweeps, warning time */
    char error[128];
} level_def;

typedef struct actor {
    uint8_t kind, depth, alive, dir, moving, forced, sliding, asleep, player, flip, hop;
    int8_t cx, cy, tx, ty;      /* current cell, target cell while moving */
    int16_t prog;               /* 0..SUB while moving */
    int16_t speed;              /* progress per frame */
    int16_t stun, invul, timer, anim, hp, aux, dig;
    uint8_t state;
    uint8_t etype, pal;         /* enemy type, sprite palette slot */
    int16_t react;              /* bomb reaction countdown */
    uint8_t chasing;            /* chase hysteresis */
    uint8_t since_reverse;      /* tiles since the last turn-around */
    uint8_t face, prev_face;    /* facing statistics (tests) */
    uint16_t face_changes, tiles_moved, jitter;
    uint32_t face_t;
} actor;

typedef struct bomb {
    uint8_t active, depth, owner, range, moving, dir, falling, hop;
    int8_t cx, cy, tx, ty;
    int16_t prog, fuse;
    uint16_t order;
} bomb;

enum fx_kind { FXP_DUST, FXP_WIND, FXP_SPRAY, FXP_STEAM, FXP_ZZZ, FXP_SPLASH, FXP_STAR, FXP_TOMATO, FXP_APPLE, FXP_LEAF, FXP_ICICLE, FXP_CRACK };
typedef struct fxp {
    uint8_t kind, depth, flip, frame;
    int16_t x, y, t, life, vx, vy;   /* pixels (x16 for x,y) */
} fxp;

typedef struct logobj {
    uint8_t alive, depth, moving, dir;
    int8_t cx, cy, tx, ty;
    int16_t prog;
} logobj;

/* a bee swarm out of a bombed hive: it chases the nearest creature for a while */
typedef struct swarm {
    uint8_t alive, depth;
    int8_t cx, cy, tx, ty;
    int16_t prog, life, target, cool;
} swarm;

/* a harvester: parked at one end of its lane, it warns (rumble, flashing lane) then sweeps it */
enum { HV_IDLE, HV_WARN, HV_MOVE };
typedef struct harvester {
    uint8_t alive, depth, dir, state;
    int8_t cx, cy, ex, ey;          /* current cell, end of the lane */
    int16_t prog, timer;
} harvester;

/* a pumpkin: pushed Sokoban-style; on a hole or in water it plugs it (walkable); a blast smashes it */
typedef struct pumpkin {
    uint8_t alive, depth, moving, dir, plug, under;
    int8_t cx, cy, tx, ty;
    int16_t prog;
} pumpkin;

/* a mine cart on its rails: walk into it to ride it to the end of the line; levers switch junctions */
typedef struct cart {
    uint8_t alive, depth, moving, dir, rider;   /* rider: 1 = the mole rides it */
    int8_t cx, cy, tx, ty;
    int16_t prog;
} cart;

/* a snowball: a blast rolls it; it grows over snow (big after 3 cells), crushes what it meets, stops at obstacles */
typedef struct snowball {
    uint8_t alive, depth, moving, dir, big, rolled;
    int8_t cx, cy, tx, ty;
    int16_t prog;
} snowball;

/* a well bucket: an elevator between the surface and depth 2 (the wells of one channel); a crank calls it */
typedef struct bucket {
    uint8_t used, at, to, rider;    /* at: the depth it is at (0 or 2); to: where it goes while moving */
    int8_t x, y;                    /* the well's cell (the same on both depths) */
    int16_t moving;                 /* frames left of the trip */
} bucket;

typedef struct pstats { int bombs, range, speed, remote, hearts, lives, placed; } pstats;

typedef struct world {
    const level_def *def;
    int season, mode, nplayers, diff;
    int8_t var_slot[VAR_COUNT];          /* OBJ palette slot of each variant in this level (-1 = unused) */
    cell g[NDEPTH][GH][GW];
    uint8_t blast[NDEPTH][GH][GW];       /* frames left */
    uint8_t shape[NDEPTH][GH][GW];       /* FX_* base */
    actor a[MAX_ACTORS];
    int na;
    bomb b[MAX_BOMBS];
    uint16_t bomb_order;
    logobj logs[MAX_LOGS];
    fxp fx[MAX_FX];
    pstats ps[MAX_PLAYERS];
    int grubs_left, grubs_total, exit_open, boss_alive, crates_total;
    uint32_t t;                          /* frames since level start */
    uint8_t chan_on[NCHAN];              /* plates pressed / levers on */
    uint16_t chan_timer[NCHAN];          /* timed gates */
    uint8_t lever[NCHAN];
    uint8_t chan_flash[NCHAN];           /* frames left of the linked-gate flash (lever toggled) */
    int pickup;                          /* IT_* picked up this frame (EV_PICKUP) */
    uint16_t wind_fx[4];                 /* wind streaks spawned per direction (tests) */
    uint8_t fire[NDEPTH][GH][GW];        /* burning corn: frames left */
    uint16_t apple_t[NDEPTH][GH][GW];    /* a fallen apple lies here for a while */
    pumpkin pumps[MAX_PUMPKINS];
    cart carts[MAX_CARTS];
    int riding;                          /* the cart the mole rides, or -1 */
    int apple_heart;                     /* the apple's heart is given once per level */
    snowball balls[MAX_SNOWBALLS];
    bucket buckets[NCHAN];
    int in_bucket;                       /* the channel of the bucket the mole rides, or 0 */
    int8_t safe_d, safe_x, safe_y;       /* the mole's last safe cell (drowning puts it back there) */
    int drifts;                          /* snowdrifts the blizzard has made */
    uint8_t gas[NDEPTH][GH][GW];         /* stun gas: frames left (> GAS_TIME: not reached yet) */
    swarm bees[MAX_SWARMS];
    harvester harv[MAX_HARV];
    struct { int burnt, stings, bee_kills, shaken, warns, crushed, gas_stuns, badger_holes, badger_stuns, croc_bites, croc_stuns,
             pushes, smashes, plugs, apple_stuns, hops, bomb_hops, rides, crushed_by_cart, ants_home, ants_dropped,
             fox_dashes, fox_rests, fox_hits, leaves_blown,
             drowned, enemies_drowned, thin_breaks, ice_breaks, croc_cracks, drift_slows, drifts_made, rolls, ball_grows,
             ball_crushes, ball_shatters, icicles_fallen, icicle_hits, bucket_rides, cranks, owl_swoops, owl_swoop_hits,
             owl_perches, owl_hits, owl_drops, owl_phase2; } stat;
    int dirty[NDEPTH];                   /* map needs redraw */
    uint8_t cell_dirty[NDEPTH][GH][GW];
    int events;                          /* EV_* raised this frame (for the game flow) */
    int shake;
} world;

enum { EV_DEPTH = 1, EV_EXIT = 2, EV_DEAD = 4, EV_BOSS_DOWN = 8, EV_GRUB = 16, EV_EXIT_OPEN = 32, EV_PICKUP = 64 };

/* ---- level.c ---- */
int  level_parse(level_def *L, const char *text, size_t len, const char *fname);
int  level_load(level_def *L, int arc, int num);   /* arc 0..3, num 1..8 */
const char *season_name(int s);

/* ---- world.c ---- */
extern world W;
void world_start(const level_def *L, const pstats *carry);
void world_update(void);
int  world_player_depth(int p);
actor *world_player(int p);
int  terrain_walkable(int t, int for_enemy);
int  world_danger(int depth, int from_depth);
int  world_enemies(int depth);
int  world_gust_on(void);
int  world_vent_on(void);
int  world_wind_lanes(int counts[4]);            /* recompute the windmill lanes; cells per direction */
int  world_harvester_at(int d, int x, int y);    /* index of a harvester on that cell, or -1 */
int  world_cats_seeing(void);                    /* cats that see the mole now (tests) */
int  world_pumpkin_at(int d, int x, int y);      /* index of a pumpkin standing on that cell, or -1 */
int  world_cart_at(int d, int x, int y);         /* index of a cart on that cell, or -1 */
int  world_snowball_at(int d, int x, int y);     /* index of a snowball on that cell, or -1 */
int  world_bucket_at(int d, int x, int y);       /* channel of a bucket standing at that well, or 0 */
int  world_owl(int *tx, int *ty);                /* the owl's state (OWL_*), its swoop target; -1: no owl */
enum { OWL_FLY, OWL_AIM, OWL_SWOOP, OWL_TO_PERCH, OWL_PERCHED };
extern int g_night;                              /* the night's lamp is on (draw.c) */
int  world_harvest_warning(int d, int x, int y); /* the cell lies in a lane about to be swept */
extern int pending_depth, pending_from;          /* depth change requested by the player */

/* ---- draw.c ---- */
void draw_init_vram(int season, int boss);
void draw_title_vram(void);
void draw_playfield_full(int depth, int slot);
void draw_cells_dirty(int depth, int slot);
void draw_world_sprites(int depth, int yoff, int first);
void draw_hud(void);
void draw_canopy(int depth);                   /* corn and tall grass over the sprites (BG1, high priority) */
void fog_set(int on, int cx, int cy, int r);   /* fog outside a circle around the mole */
void night_set(int on, int cx, int cy, int r); /* winter night: dark blue outside the helmet lamp */
int  fog_hides(int d, int x, int y);           /* that cell is in the fog (sprites become eyes) */
void fx_add_leaf(int d, int cx, int cy);       /* leaves rustle (the fox hiding) */
void draw_weather(int on);
void draw_frame_setup(void);
void text_box(int x, int y, int w, int h);
void text_clear_all(void);
void text_at(int x, int y, const char *s);
void textf_at(int x, int y, const char *fmt, ...);
void text_big(int x, int y, const char *s);
void spr_draw(int spr, int x, int y, int flags, int prio);
void spr_draw_pal(int spr, int x, int y, int flags, int prio, int pal);
void iris_set(int on, int cx, int cy, int r);
void lamp_set(int on, int cx, int cy, int r);
void set_scroll_slot(int y);
int  slot_y(int slot);
void hud_hide_lines(int on);
extern int view_slot;

/* ---- ui.c ---- */
void ui_init_level(void);
int  ui_grubs(int depth);
#define UI_DEV_ITEMS 6
void ui_pause_screen(int cursor, int dev, int quit_ask);   /* quit_ask: 0, 1 = YES, 2 = NO highlighted */
void ui_perf_overlay(void);
int ui_glow_count(int d);                /* blocks glowing on depth d (a grub inside) */
extern int dev_god, dev_reveal, dev_perf;
void ui_screen_done(void);
void ui_banner_exit_open(void);
void ui_level_banner(const level_def *L);
void ui_glow_pulse(uint32_t t);
void ui_play_overlays(int view_depth);
void ui_pickup_banner(int item);
void ui_start_box(const level_def *L);         /* the paused start box (name, objective, boss hint) */
void ui_boss_bar(void);                        /* boss health (and the farmer's crates) */
int  world_crates(void);                       /* crates left on the boss's depth */
void ui_hud_extras(void);

/* ---- sfx.c ---- */
enum sfx_id {
    SFX_BOMB_DROP, SFX_FUSE, SFX_BLAST, SFX_BREAK, SFX_DIG, SFX_GRUB, SFX_POWERUP, SFX_HURT,
    SFX_KO, SFX_EXIT_OPEN, SFX_DEPTH, SFX_ENEMY_DOWN, SFX_POUNCE, SFX_BOSS_HIT, SFX_MENU_MOVE,
    SFX_MENU_OK, SFX_SPLASH, SFX_FIZZLE, SFX_SWITCH, SFX_STEAM, SFX_WOOF, SFX_SPLAT, SFX_LEVER, SFX_COUNT
};
void sfx_init(void);
void sfx(int id);
void sfx_at(int id, int x);             /* panned by playfield x (pixels) */
void music_play(const char *name);
void audio_options(int music_on, int sfx_on);

/* ---- main.c ---- */
extern int opt_music, opt_sfx, opt_diff;
void draw_variant_pals(void);

static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline int in_grid(int x, int y) { return x >= 0 && y >= 0 && x < GW && y < GH; }

#endif
