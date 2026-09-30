/*
 * Blueberry Tumble: shared declarations.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/blueberrytumble/LICENSE.
 *
 * The rules (no video, no sound: deterministic, testable alone, used by the validator's solver):
 *   course.c    the course: a ring of 16-px columns (ground + cells), cones, the frame clock, tempo tiers
 *   physics.c   the berry: one frame of motion and collisions on the course
 *   patterns.c  the ~20 parametrised patterns (one idea each) and their parameter spaces
 *   director.c  the endless stream: difficulty target, pattern choice, bridges, biomes, coins
 * The game: main.c (flow, input, save RAM, options, save states), draw.c (video), sfx.c (sounds), bot.c (the
 * screen-reading bot). The house kit is games/common (docs/art-direction.md).
 */
#ifndef BT_H
#define BT_H

#include <stdint.h>
#include "rs.h"
#include "tuning.h"

#define MAX_PLAYERS 2

/* ---- the course ------------------------------------------------------------------------------------------------- */
enum { K_EMPTY, K_BLOCK, K_LOG, K_THORN, K_HANG, K_PEBBLE, K_PAD, K_ORB, K_COIN, K_KINDS };
enum { G_GROUND, G_GAP, G_ICE, G_SNOW, G_WATER };
enum { F_LEAF = 1, F_LEAF_END = 2, F_GATE = 4, F_BREATH = 8 };   /* column flags (gates span the whole column) */
typedef struct bt_col {
    uint8_t ground, flags;
    uint8_t cell[ROWS];         /* K_*: row 0 stands on the ground */
    uint16_t gone;              /* bit r: cell r smashed or collected (drawing only: the physics never reads it) */
    uint16_t seg;               /* the segment (pattern instance) this column belongs to, low 16 bits */
} bt_col;

typedef struct bt_cone {
    int32_t meet;               /* px: where it meets player 1 (on a 16th note) */
    uint8_t k256, gone;         /* speed in 1/256 of the run speed; smashed */
} bt_cone;
#define CONE_RING 32

typedef struct bt_seg {
    int32_t col0;               /* first column */
    int16_t len;                /* columns */
    int8_t pat;                 /* pattern index, -1 = bridge/filler, -2 = gate, -3 = start */
    uint8_t tier, score;
    uint16_t combo;
    int16_t target;             /* the director's target when it was chosen */
} bt_seg;
#define SEG_RING 64

#define RING 256                /* columns kept (4096 px) */
#define START_COL (-16)         /* the course begins 16 columns behind the start (x = 0) */

typedef struct bt_director {
    rs_rng rng;
    uint32_t seed;
    int recent[DIRECTOR_MEMORY];
    int last;                   /* the last pattern placed (-1 none) */
    int32_t breath_wave;        /* the last wave that got its breather */
    int coins;                  /* coin spots placed */
    int fixed_tier;             /* -1: tiers follow the biomes */
} bt_director;

typedef struct bt_course {
    bt_col col[RING];
    int32_t next_col;           /* first column not generated yet */
    bt_cone cone[CONE_RING];
    int32_t ncone;              /* cones placed so far (the ring holds the last CONE_RING) */
    bt_seg seg[SEG_RING];
    int32_t nseg;
    int fixed_tier;             /* -1: tiers follow the biomes (the game); 0..4: one tier everywhere (the validator) */
    int32_t end_col;            /* the validator's finite courses: flat ground from here (INT32_MAX: none) */
    bt_director dir;
} bt_course;

void course_reset(bt_course *c, int fixed_tier);
const bt_col *course_col(const bt_course *c, int32_t col);        /* outside the ring: flat ground */
bt_col *course_col_w(bt_course *c, int32_t col);
int  course_tier_of_col(const bt_course *c, int32_t col);
int  course_biome_of_col(int32_t col);                              /* 0..3 (loops) */
int  course_loop_of_col(int32_t col);                               /* 0 = day, 1+ = night */
/* the frame clock: player 1's x (Q16) at frame f of the run, exact (no drift); the speed on that frame */
int64_t course_x(const bt_course *c, int32_t f);
int32_t course_speed(const bt_course *c, int32_t f);
int32_t tier_speed(int tier);                                       /* Q16 px per frame */
int  tier_fpb_num(int tier);
int  tier_fpb_den(int tier);
int  biome_frames(int tier);                                        /* frames per biome at that tier */
/* a cone's x (Q16) when player 1 is at x_ref (Q16) */
int64_t cone_x(const bt_cone *k, int64_t x_ref);
const bt_seg *course_seg_at(const bt_course *c, int32_t col);

/* ---- the berry ------------------------------------------------------------------------------------------------------ */
enum { M_NORMAL, M_SNOW, M_GLIDE };
enum { D_NONE, D_HAZARD, D_WALL, D_FALL };
typedef struct berry {
    int64_t x;                  /* Q16: centre x (world px; 64-bit: long runs) */
    int32_t h, vy;              /* Q16: (world px), bottom above the ground, vertical speed (+ = up) */
    uint8_t mode, grounded, held, need_release, on_ice, dead, coin_got, pad;
    int32_t orb_used, pad_used, coin_last;   /* object ids (col * 16 + row), -1 none */
    /* cosmetic, not part of the solver's state */
    uint16_t angle;             /* one turn = 65536 */
    uint8_t land_t, air_t, grow_t, cause;
    int64_t dead_x;
    int32_t dead_h;
} berry;

enum {
    EV_JUMP = 1, EV_LAND = 2, EV_ORB = 4, EV_PAD = 8, EV_DIE = 16, EV_SMASH = 32, EV_GROW = 64, EV_SHRINK = 128,
    EV_GLIDE = 256, EV_GLIDE_END = 512, EV_COIN = 1024, EV_ICE = 2048
};
typedef struct step_info {
    int nsmash;
    int32_t smash_id[6];        /* col * 16 + row, or -1 - cone index */
    int32_t coin_id;
} step_info;

void berry_reset(berry *b, int64_t x);
/* one frame: move the berry to x_new (Q16), player 1 at x_ref (cones), A held or not. Returns EV_* bits. */
int  berry_step(berry *b, const bt_course *c, int64_t x_new, int64_t x_ref, int held, step_info *info);
int  berry_size(const berry *b);
int  berry_hash(const berry *b);   /* the solver's state, hashed */
/* the object id of a cell */
#define OBJ_ID(col, row) ((int32_t)(col) * 16 + (row))

/* ---- patterns -------------------------------------------------------------------------------------------------------- */
enum { SK_JUMP = 1, SK_ORB = 2, SK_PAD = 4, SK_GLIDE = 8, SK_SNOW = 16, SK_ICE = 32, SK_CONE = 64 };
#define NSKILLS 7
#define PAT_MAX_LEN 96
#define PAT_MAX_PARAMS 5
#define PAT_MAX_CONES 6
typedef struct pat_param {
    const char *name;
    int8_t lo, hi[NTIERS];      /* the range at each tier (hi < lo: the pattern is not made at that tier) */
} pat_param;
typedef struct pbuild {
    bt_col col[PAT_MAX_LEN];
    int len;                    /* set by the builder (cells used), rounded up to whole beats by pat_build */
    bt_cone cone[PAT_MAX_CONES];/* meet relative to the pattern's start (px) */
    int ncone;
    int coin;                   /* 1: place the golden blueberry at the pattern's coin spot */
    int tier;
} pbuild;
typedef struct pattern_def {
    const char *name, *idea;
    uint8_t biomes;             /* bit b: may appear in biome b (0 summit .. 3 village); the night loop takes any */
    uint8_t breather;           /* 1: a breather (easy rest after a wave) */
    uint8_t coin_spot;          /* 1: has a golden blueberry spot */
    uint8_t nparams;
    pat_param p[PAT_MAX_PARAMS];
    void (*build)(pbuild *b, const int *v);
} pattern_def;
extern const pattern_def bt_patterns[];
extern const int bt_npatterns;
enum { PAT_GATE = -2, PAT_START = -3, PAT_BRIDGE = -1 };

int  pat_ncombo(int pat, int tier);                         /* 0: not made at that tier */
void pat_combo_values(int pat, int tier, int combo, int *v);
void pat_build(pbuild *b, int pat, int tier, int combo, int coin);  /* len is a multiple of BEAT_CELLS */
void pat_build_gate(pbuild *b, int biome);
const char *pat_name(int pat);

/* ---- the measured table (src/tables.inc, generated by tests/validate.c --write-tables) --------------------------- */
typedef struct pat_tier_info {
    uint16_t ncombo, first;     /* this pattern's combos at this tier: scores at bt_combo_score[first..] */
    int8_t tail, head;          /* frames: see DESIGN.md "Links" */
    uint8_t min_score, max_score, min_window;
} pat_tier_info;
const pat_tier_info *table_info(int pat, int tier);         /* NULL: the table has no entry (stale) */
int  table_score(int pat, int tier, int combo);             /* 0..100, or -1 */
int  table_window(int pat, int tier, int combo);
int  bridge_cells(int prev_pat, int next_pat, int tier);    /* flat cells between two patterns (whole beats) */
extern const int bt_table_npatterns;

/* ---- the director ------------------------------------------------------------------------------------------------------ */
void director_start(bt_course *c, uint32_t seed);
void director_fill(bt_course *c, int32_t upto_col);         /* generate columns up to upto_col (exclusive) */
int  difficulty_target(int32_t metres);                     /* the curve (without the band) */
int  difficulty_wave(int32_t metres);                       /* the wave index at that distance */
/* place one pattern instance (the validator uses it to build test courses): returns its first column */
int32_t course_place(bt_course *c, int pat, int tier, int combo, int coin, int target);
void course_place_flat(bt_course *c, int cells, int pat_kind);

/* ---- the game ------------------------------------------------------------------------------------------------------------ */
enum { ST_TITLE, ST_READY, ST_PLAY, ST_DEAD, ST_OVER };
typedef struct world {
    bt_course course;
    berry b[MAX_PLAYERS];
    int players;
    int32_t f;                  /* the course clock: frames rolled (it stops when every berry is down) */
    int32_t t;                  /* frames since the run started (it goes on after the last splat) */
    int started;
    int coins[MAX_PLAYERS];     /* golden blueberries this run */
    int32_t metres[MAX_PLAYERS];
    int events[MAX_PLAYERS];
    step_info info[MAX_PLAYERS];
    int32_t dead_f[MAX_PLAYERS];/* t at the splat */
    int god;                    /* a test aid (screenshots, --opt god=1): the berries bounce off everything */
    uint32_t seed;
} world;
void world_init(world *w, int players, uint32_t seed);
void world_step(world *w, const int held[MAX_PLAYERS]);
int  world_running(const world *w);
int  world_x(const world *w, int p);                        /* px */
int  world_score(const world *w, int p);

/* draw.c */
void draw_init(void);
void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused, int attempt, int medal);
void draw_state(void);
void draw_reset(void);                                      /* a new run */
void draw_events(const world *w);                           /* the effects of this frame's events */
void draw_view(int *hofs, int *vofs, int *c256);            /* the playfield registers (the bot reads them) */
/* the bot reads the screen: these say what the playfield tiles are (draw.c) */
int  draw_tile_kind(uint16_t map_entry);                    /* K_* for a solid/hazard tile, -1 = none; ground: 100 + G_* */

/* sfx.c */
enum { SFX_JUMP, SFX_LAND, SFX_SQUELCH, SFX_BOING, SFX_CHIME, SFX_WHOOSH, SFX_CRUNCH, SFX_GROW, SFX_SPLASH,
       SFX_COIN, SFX_SKID, SFX_GATE, SFX_COUNT };
#define SFX_HOUSE SFX_COUNT
void sfx_init(int sound_on);
void sfx_play(int id, int x);

/* bot.c: plays from the screen (the PPU's map and scroll, the OAM) */
void bot_reset(void);
int  bot_decide(int player);
void bot_state(void);

#endif
