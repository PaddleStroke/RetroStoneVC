/*
 * Puffin Plunge: shared declarations and the tuning (every gameplay number is here or in world.c's tables).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/puffinplunge/LICENSE.
 *
 * world.c is the rules: the endless well (made chunk by chunk from the hand-made chunks of levels/chunks.txt), the
 * puffin's movement, its bubble boots, the critters, the gems, the side rooms, the rest stops, the walrus and the bot.
 * It uses no SDK call (no drawing, no sound) and keeps its own RNG: deterministic on the frame clock, the same at every screen size (nothing
 * depends on what the screen shows), linked alone by tests/test_world.c. draw.c is the video, main.c the flow (the
 * title, the endless dive, the shops, the results, pause), input, sound, the music, save RAM, save states.
 */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>

/* ---- units ----------------------------------------------------------------------------------------------------------- */
#define SUB          256            /* positions: 1 px = 256 sub-pixels; speeds in sub-pixels per frame */
#define SEC          60
#define CELL         16             /* the shaft's cells, px */
#define COLS         12             /* the shaft: a wall cell on each side and 10 cells of room between them */
#define SHAFT_W      (COLS * CELL)  /* 192 px */
#define CHUNK_W      10             /* a chunk: the 10 inner columns ... */
#define CHUNK_H      12             /* ... and 12 rows (192 px) */
#define TOP_ROWS     7              /* the dive starts on a landing ledge under a ceiling, then a short drop */
#define RING         128            /* the well is endless: the rows kept, a ring (the puffin's ~45 above, ~64 below) */
#define AHEAD_ROWS   64             /* rows generated below the puffin (more than any screen shows, the critters wake) */
#define ZONES        4
#define ZONE_CHUNKS  25             /* a zone: 25 chunks (300 m); then the next, the four again, harder ("a cycle") */
#define REST_EVERY   15             /* a rest stop (a ledge with a shop) every 15 chunks (180 m) */
#define DIFF_M       120            /* the difficulty grows a step every 120 m (critters, speed, ledges, prices) */
#define GENTLE_M     180            /* the first metres: as gentle as the first level was */
#define M_PER_ROW    1              /* 1 m = one cell (16 px) */
/* depth in metres of absolute row r (the dive's rows count from the first ceiling) */
#define ROW_M(r)     ((r) - TOP_ROWS)

/* the cells (C_FLOOR: a rest stop's ledge; C_ARENA: the walrus's floor, gone once it is beaten) */
enum { C_EMPTY, C_ROCK, C_CRYSTAL, C_CRACKED, C_DOOR, C_SHUT, C_FLOOR, C_ARENA, C_COUNT };
#define SOLID(c)     ((c) == C_ROCK || (c) == C_CRYSTAL || (c) == C_CRACKED || (c) == C_SHUT || (c) == C_FLOOR || \
                      (c) == C_ARENA)

/* ---- the puffin ------------------------------------------------------------------------------------------------------- */
#define HB_W         10             /* its hitbox against the rock, px (x: the centre; y: the feet) */
#define HB_H         14
#define RUN_MAX      512            /* 2.0 px/f */
#define RUN_ACCEL    128            /* ground: full speed in 4 frames */
#define RUN_DECEL    160            /* ground, no input: a stop in 3-4 frames */
#define TURN_ACCEL   224            /* input against the motion: a snappy turn */
#define AIR_ACCEL    96
#define AIR_DECEL    48
#define GRAVITY      72             /* 0.28 px/f^2 */
#define FALL_MAX     1024           /* 4.0 px/f: terminal velocity */
#define JUMP_V       1180           /* 4.6 px/f: about 38 px high (2.4 cells) held */
#define JUMP_CUT     384            /* jump released while rising: the rise cut to 1.5 px/f */
#define COYOTE       5              /* frames after leaving a ledge when A still jumps (not fires) */
#define JUMP_BUFFER  6              /* A pressed this soon before landing, with no ammo: a jump on landing */
#define HURT_IFRAMES 90             /* safe after a hit (blinks) */
#define HURT_KICK_VY 600            /* a hit throws the puffin up a little */
#define HITSTOP_HURT 6
#define HITSTOP_KILL 2              /* the tiny freeze of a stomp */
#define STOMP_VY     1024           /* a stomp's bounce: 4.0 px/f (held A: STOMP_VY_HELD) */
#define STOMP_VY_HELD 1240
#define SPRING_BONUS 220            /* the bouncy boots: higher bounces */
#define START_HP     4
#define MAX_HP       8
#define START_AMMO   8
#define MAX_AMMO     16
#define MAGNET_R     22             /* px: gems fly to the puffin from this far (the magnet: MAGNET_R_BIG) */
#define MAGNET_R_BIG 64
#define PICK_R       10

/* ---- the bubble boots: the weapons (one at a time) --------------------------------------------------------------------- */
enum { W_BUBBLE, W_SPREAD, W_BEAM, W_BURST, W_COUNT };
typedef struct weapon_info {
    uint8_t period;                 /* frames between two shots while A is held */
    uint8_t ammo;                   /* per shot */
    uint8_t dmg;
    int16_t hover;                  /* each shot slows the fall to this (sub-px/f; negative: a little up) */
} weapon_info;
extern const weapon_info WEAPONS[W_COUNT];
#define SHOT_V       1792           /* 7 px/f straight down */
#define SHOT_SIDE_V  640            /* the spread's side shots */
#define BEAM_LEN     176            /* px: the beam's reach */
#define BEAM_FRAMES  7
#define MAX_SHOT     28

/* ---- the critters -------------------------------------------------------------------------------------------------------- */
enum {
    ET_SNAIL, ET_BAT, ET_URCHIN,                    /* zone 1: the crystal grotto */
    ET_CRAB, ET_FISH, ET_JELLY, ET_PUFFER,          /* zone 2: the coral trench */
    ET_BEETLE, ET_MOTH, ET_PUFFCAP, ET_SPORE,       /* zone 3: the mushroom cave (a spore: a puffcap's shot) */
    ET_GRUB, ET_ICEBAT, ET_ICICLE, ET_WISP,         /* zone 4: the frozen deep */
    ET_WALRUS, ET_SNOWBALL,                         /* the bottom of the well: the boss and its snowballs */
    ET_COUNT
};
enum { B_WALK, B_FLY, B_STATIC, B_FLOAT, B_PUFF, B_TURRET, B_BULLET, B_DROP, B_WISP, B_SWIM, B_BOSS };
typedef struct enemy_info {
    uint8_t beh;                    /* B_* */
    uint8_t hp;
    uint8_t soft;                   /* 1: a stomp hurts it (0: spiky, a stomp hurts the puffin) */
    uint8_t hw, hh;                 /* its hitbox's half width and height, px */
    uint8_t gems;                   /* the gems it drops */
    int16_t speed;                  /* sub-px/f */
} enemy_info;
extern const enemy_info EN_INFO[ET_COUNT];
typedef struct enemy {
    int32_t x, y;                   /* the centre, sub-px */
    int16_t vx, vy;
    uint8_t type, hp, alive, flash; /* flash: frames of the hit's white */
    uint8_t st, face, awake, zone;  /* face: 1 = looks left; zone: the zone it lives in (its palette: zone & 1) */
    uint16_t t;                     /* frames in its state */
    int32_t home_y;                 /* where it started (world px): floaters bob round it, the walrus's floor */
} enemy;
#define MAX_EN       120
#define WAKE_ABOVE   160            /* px: a critter moves while the puffin is less than this above it ... */
#define WAKE_BELOW   320            /* ... or this below it (the same at every screen size) */

/* ---- gems ------------------------------------------------------------------------------------------------------------------ */
typedef struct gem { int32_t x, y; int16_t vx, vy; uint8_t big, alive, rest; uint8_t t; } gem;
#define MAX_GEM      72

/* ---- upgrades: the shops sell them ------------------------------------------------------------------------------------------ */
enum {
    UP_HEART, UP_HEAL, UP_AMMO,                     /* +1 heart (and healed), 2 hearts back, +2 bubbles */
    UP_SPREAD, UP_BEAM, UP_BURST,                   /* the boot modules (a weapon replaces the one worn) */
    UP_MAGNET, UP_SPRINGS, UP_FEATHER, UP_SHELL, UP_LUCKY,     /* passives (each once) */
    UP_COUNT
};
#define P_MAGNET     1
#define P_SPRINGS    2
#define P_FEATHER    4
#define P_SHELL      8
#define P_LUCKY      16
#define MAX_OFFER    3
typedef struct offer { uint8_t up, sold; uint16_t price; } offer;

/* ---- the world ----------------------------------------------------------------------------------------------------------------- */
typedef struct hero {
    int32_t x, y;                   /* x: the centre, y: the feet, sub-px */
    int32_t vx, vy;
    uint8_t ground, face, coyote, buffer;   /* face 1: looks left */
    uint8_t fire_held, fire_t, hurt_t, stomp_t;
    uint8_t land_t, jump_t, shell, dead;     /* land_t / jump_t: squash and stretch; shell: the bubble shield is up */
    int16_t air_t;                  /* frames since the feet left the rock */
    uint16_t walk_t;
} hero;

typedef struct pop { int32_t x, y; int16_t n; uint8_t kind, pad; } pop;     /* world px; kind: POP_* */
enum { POP_COMBO, POP_BONUS, POP_HEART };
typedef struct fxev { int32_t x, y; uint8_t kind, a, pad[2]; } fxev;   /* world px; kind: FX_* (draw.c's effects) */
enum { FX_SHOT, FX_KILL, FX_HIT, FX_GEM, FX_BREAK, FX_LAND, FX_STOMP, FX_HURT, FX_JUMP, FX_BEAM, FX_SPORE_POP };
#define MAX_FXEV     24
#define MAX_POPS     4
#define MAX_DIRTY    16

#define MAX_DOORS    6
typedef struct world {
    /* the dive */
    uint32_t seed;
    int hp, hpmax, ammo, ammo_max, weapon, passives;
    int gems, score, kills, best_combo, depth_m;   /* gems: in the purse; score: the dive's points (a metre is one) */
    uint32_t rng;                   /* the play's RNG (each chunk has its own: the well is the same on a CONTINUE) */
    /* the well: an endless ring of rows (absolute row r at cell[r % RING]) */
    int gen_row, gen_chunk;         /* the next row and chunk to make */
    int top_row;                    /* the dive's first row (its ceiling; a CONTINUE starts lower) */
    int last_chunk;                 /* the last hand-made chunk picked (the logs) */
    uint8_t cell[RING][COLS];
    uint8_t cell_hp[RING][COLS];
    uint16_t row_zone[RING];        /* the zone each row belongs to (0, 1, 2 ...: zone % 4 its look, zone / 4 its cycle) */
    int door_row[MAX_DOORS], door_col[MAX_DOORS], door_kind[MAX_DOORS], ndoors;   /* side rooms: a shop (0), a gift (1) */
    int rest_used;                  /* the rest stop whose shop was opened (its row), -1 none */
    int boss, boss_hpmax, arena_row;    /* the walrus: an enemy index (-1 none), its health, its floor's first row */
    int walruses;                   /* the walruses beaten */
    int zone_now;                   /* the zone the puffin is in */
    hero h;
    int t;                          /* frames of the dive */
    int combo, combo_t;             /* kills since the feet left the rock; combo_t: the counter's pop */
    enemy en[MAX_EN];
    int n_en;
    struct shot { int32_t x, y; int16_t vx, vy; uint8_t alive, dmg, kind, t; } sh[MAX_SHOT];
    gem gm[MAX_GEM];
    int beam_t, beam_x, beam_y0, beam_y1;
    int hitstop;
    int32_t look;                   /* the camera's look-ahead (sub-px, smoothed from the fall: the same at every size) */
    int cleared;                    /* a rest stop reached: its shop */
    int room;                       /* a side room entered: its door's index + 1 */
    offer offers[MAX_OFFER];
    int n_offers;
    int prev_in;                    /* the last step's input (its edges) */
    int shop_mode;                  /* the offers: 0 a rest stop, 1 a side shop (cheaper), 2 a gift (one free) */
    /* reported each step to draw.c and main.c */
    uint32_t events;
    fxev fx[MAX_FXEV];
    int n_fx;
    pop pops[MAX_POPS];
    int n_pops;
    int dirty[MAX_DIRTY], n_dirty;  /* cell rows changed (a crystal broken, a door shut, a floor gone): redrawn */
    int gems_got;                   /* gems picked up this step */
} world;
enum {
    EV_JUMP = 1, EV_SHOT = 2, EV_KILL = 4, EV_HIT = 8, EV_GEM = 16, EV_LAND = 32, EV_STOMP = 64, EV_HURT = 128,
    EV_BREAK = 256, EV_COMBO = 512, EV_CLEAR = 1024, EV_ROOM = 2048, EV_DEAD = 4096, EV_BOSS = 8192, EV_WIN = 16384,
    EV_EMPTY = 32768, EV_BEAM = 65536, EV_SHIELD = 131072, EV_BOSSHIT = 262144, EV_REFILL = 524288, EV_ZONE = 1048576
};

/* the chunks (levels/chunks.txt: levels/README.md) */
typedef struct chunk {
    char name[16];
    uint8_t zones;                  /* bit z: used in zone z */
    uint8_t weight;
    char c[CHUNK_H][CHUNK_W + 1];
} chunk;
#define MAX_CHUNK_DEFS 96
int  chunks_parse(const char *text, chunk *out, int max, char *err, int errn);
int  chunk_check(const chunk *c, char *err, int errn);      /* 1: every row passable from the top (tests) */

/* the input bits */
enum { IN_LEFT = 1, IN_RIGHT = 2, IN_JUMP = 4, IN_DOWN = 8 };

/* world.c */
void world_new_run(world *w, uint32_t seed);
void world_begin(world *w, int k, const chunk *defs, int ndefs);     /* the dive from chunk k (0: the top) */
void world_step(world *w, int in, const chunk *defs, int ndefs);
uint32_t world_hash(const world *w);
int  world_cell(const world *w, int c, int r);    /* cell (column c, absolute row r) */
int  world_row_zone(const world *w, int r);       /* the zone of absolute row r */
int  world_chunk_kind(int k);                     /* chunk k: CK_* */
enum { CK_PLAIN, CK_GATE, CK_REST, CK_ARENA1, CK_ARENA2 };
int  world_reach(const world *w, int row0, int rows, int cols_from, char *err, int errn);   /* (tests) */
void world_gen_to(world *w, int r, const chunk *defs, int ndefs);                        /* (tests) */
int  world_difficulty(int depth_m);               /* 0, 1, 2 ...: a step every DIFF_M */
int  zone_look(int zone);                         /* 0..3: grotto, coral, mushroom, frozen */
int  world_offer_price(const world *w, int up);
void world_make_offers(world *w, int n, int mode);   /* the offers: mode 0 a rest stop, 1 a side shop, 2 a gift */
int  world_buy(world *w, int k);              /* 1: bought */
void world_leave_room(world *w);              /* back in the shaft from a side room */
void world_leave_rest(world *w);              /* back in the shaft from a rest stop's shop */
int  world_can_buy(const world *w, int up);   /* (a full purse aside) */
int  bot_input(const world *w, uint32_t *bot_state);     /* the bot: the inputs of this frame */
int  bot_shop(const world *w);                /* the bot in a shop: the offer to buy, -1 leave */
/* the dive's snapshot for CONTINUE (save RAM): its chunk and everything the puffin carries */
typedef struct snapshot {
    int16_t chunk;
    int8_t hp, hpmax, ammo_max, weapon, passives, walruses;
    uint16_t gems, kills;
    uint32_t seed, score;
    uint16_t best_combo, depth_m;
} snapshot;
void world_snapshot(const world *w, snapshot *s);
void world_resume(world *w, const snapshot *s, const chunk *defs, int ndefs);

/* ---- draw.c ------------------------------------------------------------------------------------------------------------------------ */
enum { ST_TITLE, ST_PLAY, ST_CARD, ST_SHOP, ST_OVER };
#define RETRY_LOCK   36
#define CARD_FRAMES  130            /* a zone's card */
typedef struct view {
    int state, st_t, paused;
    int best, new_best, can_continue, run_depth, title_sel;   /* title_sel: 0 continue, 1 a new dive */
    int medal, shop_sel, shop_kind;   /* shop_kind: 0 a rest stop, 1 a side room's shop, 2 a gift */
    int card, first_run, bot, touch;   /* card: the zone's card, frames left */
    int best_depth, new_depth;      /* the deepest dive (metres, the marker in the well); new_depth: frames of NEW BEST! */
} view;
void draw_init(void);
void draw_frame(const world *w, const view *v);
void draw_events(const world *w);
void draw_level(const world *w);              /* a new dive: the maps, the zones' colours */
void draw_state(void);
void draw_state_loaded(const world *w);
const char *upgrade_name(int up);
const char *zone_name(int z);

/* ---- sound (main.c) ------------------------------------------------------------------------------------------------------------- */
enum { SFX_JUMP, SFX_SHOT, SFX_BEAM, SFX_HIT, SFX_POP, SFX_STOMP, SFX_GEM, SFX_LAND, SFX_HURT, SFX_BREAK, SFX_EMPTY,
       SFX_COMBO, SFX_BUY, SFX_DOOR, SFX_BOSS, SFX_COUNT };
#define SFX_HOUSE SFX_COUNT

#endif
