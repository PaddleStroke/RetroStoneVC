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
#define NCHAN 8

enum { SEASON_SPRING, SEASON_SUMMER, SEASON_AUTUMN, SEASON_WINTER, SEASONS };
enum { DIR_UP, DIR_RIGHT, DIR_DOWN, DIR_LEFT, DIR_NONE };
extern const int DX[4], DY[4];

enum terrain {
    TR_FLOOR, TR_STONE, TR_DIRT, TR_ROCK, TR_ROOTS, TR_FROZEN, TR_LEAVES, TR_WATER, TR_PUDDLE,
    TR_THIN, TR_HOLE_DOWN, TR_HOLE_UP, TR_LADDER, TR_EXIT, TR_BRIDGE, TR_ICE, TR_THIN_ICE, TR_MUD,
    TR_COVER, TR_BURNT, TR_GATE, TR_PLATE, TR_LEVER, TR_VENT, TR_PIPE, TR_CRATE, TR_SPRINKLER,
    TR_WINDMILL, TR_COUNT
};
enum item { IT_NONE, IT_GRUB, IT_BOMB, IT_FIRE, IT_SPEED, IT_REMOTE, IT_HEART, IT_COUNT };
enum actor_kind { AK_NONE, AK_MOLE, AK_FERRET, AK_CAT, AK_BOSS, AK_DOG };
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
    int boss;                   /* BOSS_* from assets.h */
    int tier;                   /* default enemy tier (1..4) for F and C */
    cell g[NDEPTH][GH][GW];
    spawn sp[64];
    int nsp;
    struct { uint8_t depth, x, y; } logs[MAX_LOGS];
    int nlogs;
    char error[128];
} level_def;

typedef struct actor {
    uint8_t kind, depth, alive, dir, moving, forced, sliding, asleep, player, flip;
    int8_t cx, cy, tx, ty;      /* current cell, target cell while moving */
    int16_t prog;               /* 0..SUB while moving */
    int16_t speed;              /* progress per frame */
    int16_t stun, invul, timer, anim, hp, aux, dig;
    uint8_t state;
    uint8_t etype, pal;         /* enemy type, sprite palette slot */
    int16_t react;              /* bomb reaction countdown */
} actor;

typedef struct bomb {
    uint8_t active, depth, owner, range, moving, dir, falling;
    int8_t cx, cy, tx, ty;
    int16_t prog, fuse;
    uint16_t order;
} bomb;

enum fx_kind { FXP_DUST, FXP_WIND, FXP_SPRAY, FXP_STEAM, FXP_ZZZ, FXP_SPLASH, FXP_STAR, FXP_TOMATO };
typedef struct fxp {
    uint8_t kind, depth, flip, frame;
    int16_t x, y, t, life, vx, vy;   /* pixels (x16 for x,y) */
} fxp;

typedef struct logobj {
    uint8_t alive, depth, moving, dir;
    int8_t cx, cy, tx, ty;
    int16_t prog;
} logobj;

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
    int grubs_left, grubs_total, exit_open, boss_alive;
    uint32_t t;                          /* frames since level start */
    uint8_t chan_on[NCHAN];              /* plates pressed / levers on */
    uint16_t chan_timer[NCHAN];          /* timed gates */
    uint8_t lever[NCHAN];
    int dirty[NDEPTH];                   /* map needs redraw */
    uint8_t cell_dirty[NDEPTH][GH][GW];
    int events;                          /* EV_* raised this frame (for the game flow) */
    int shake;
} world;

enum { EV_DEPTH = 1, EV_EXIT = 2, EV_DEAD = 4, EV_BOSS_DOWN = 8, EV_GRUB = 16, EV_EXIT_OPEN = 32 };

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
extern int pending_depth, pending_from;          /* depth change requested by the player */

/* ---- draw.c ---- */
void draw_init_vram(int season, int boss);
void draw_title_vram(void);
void draw_playfield_full(int depth, int slot);
void draw_cells_dirty(int depth, int slot);
void draw_world_sprites(int depth, int yoff, int first);
void draw_hud(void);
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

/* ---- sfx.c ---- */
enum sfx_id {
    SFX_BOMB_DROP, SFX_FUSE, SFX_BLAST, SFX_BREAK, SFX_DIG, SFX_GRUB, SFX_POWERUP, SFX_HURT,
    SFX_KO, SFX_EXIT_OPEN, SFX_DEPTH, SFX_ENEMY_DOWN, SFX_POUNCE, SFX_BOSS_HIT, SFX_MENU_MOVE,
    SFX_MENU_OK, SFX_SPLASH, SFX_FIZZLE, SFX_SWITCH, SFX_STEAM, SFX_WOOF, SFX_SPLAT, SFX_COUNT
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
