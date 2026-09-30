/*
 * Leady Squid: shared declarations.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/leadysquid/LICENSE.
 *
 * physics.c + world.c are the game rules (no drawing, no sound: the unit tests
 * link them alone); draw.c, sfx.c, bot.c and main.c are the console side.
 */
#ifndef LS_H
#define LS_H

#include <stdint.h>
#include "rs.h"
#include "tuning.h"

#define MAX_PLAYERS 2
#define OB_MAX 16

static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline int32_t min32(int32_t a, int32_t b) { return a < b ? a : b; }
static inline int32_t max32(int32_t a, int32_t b) { return a > b ? a : b; }

/* ---- physics.c: one squid, one obstacle (pure functions) ------------------ */
enum squid_state { SQ_READY, SQ_SWIM, SQ_HIT, SQ_SINK, SQ_REST, SQ_OFF };

typedef struct squid {
    int state, t;               /* state and frames in it */
    int32_t y, vy;              /* Q16: centre of the hitbox (screen y), speed */
    int32_t tilt;               /* Q8 degrees, + = nose up (tuning.h) */
    int x;                      /* screen x of the hitbox centre */
    int score, next_ob;         /* points; index of the next obstacle to score */
    int flap_t;                 /* frames since the last flap (squeeze animation) */
    int flaps;
} squid;

typedef struct obstacle {
    int x;                      /* world x of the left edge (a multiple of 8) */
    int gap_top;                /* the gap is [gap_top, gap_top + GAP) */
    int index, theme;
} obstacle;

typedef struct box { int x0, y0, x1, y1; } box;   /* [x0, x1) x [y0, y1) */

void squid_reset(squid *s, int x);
void squid_flap(squid *s);
void squid_swim_step(squid *s, int flap);     /* one frame of swimming (flap = pressed this frame) */
void squid_sink_step(squid *s);               /* one frame of the death fall */
int  squid_tilt_frame(int32_t tilt);          /* 0..5: the pre-drawn angle shown */
int  squid_tilt_shown(int32_t tilt);          /* degrees shown (capped at +20) */
box  squid_box(const squid *s);               /* hitbox, screen coordinates */
int  squid_rest_y(const squid *s);            /* centre y when lying on the seabed */
int  bob_offset(int t);                       /* get-ready bob, pixels */

obstacle obstacle_make(rs_rng *rng, int index);
int  box_hits_obstacle(box b, const obstacle *o, int scroll_px);
int  box_hits_seabed(box b);

/* ---- world.c: a run -------------------------------------------------------- */
enum { EV_FLAP = 1, EV_SCORE = 2, EV_HIT = 4, EV_LAND = 8, EV_SINK = 16, EV_START = 32 };

typedef struct world {
    int32_t scroll;             /* Q16: world x of the screen's left edge */
    obstacle ob[OB_MAX];
    int nob, next_index;
    rs_rng rng;
    uint32_t seed;
    squid sq[MAX_PLAYERS];
    int players, t, started;
    int first_index;            /* debug: start further along the course (--opt skip=N) */
    int fixed_seed;             /* the course does not depend on the start frame (--opt seed=N) */
    int events[MAX_PLAYERS];    /* EV_* of the last step */
} world;

void world_init(world *w, int players, uint32_t seed, int first_index);
void world_step(world *w, const int flap[MAX_PLAYERS]);
int  world_scroll_px(const world *w);
int  world_running(const world *w);          /* the scroll moves (someone swims) */
int  world_all_resting(const world *w);      /* everyone is on the seabed */
int  world_theme_at(const world *w);         /* theme of the next obstacle ahead of player 1 */
const obstacle *world_next_obstacle(const world *w, int player);

int medal_of(int score);                      /* 0 none, 1 bronze .. 4 pearl */

/* ---- console side ------------------------------------------------------------ */
/* sfx.c */
enum { SFX_BLOOP, SFX_DING, SFX_THUD, SFX_CLANK, SFX_SWISH, SFX_SPARKLE, SFX_JOIN, SFX_PAUSE, SFX_COUNT };
void sfx_init(void);
void sfx(int id);
void sfx_pan(int id, int x);
void music_start(void);
void audio_set(int music, int sound);

/* draw.c; the screens (game states) */
enum { DS_TITLE, DS_READY, DS_PLAY, DS_DEAD, DS_OVER };
void draw_init(void);
void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused);
void draw_reset_course(const world *w);
void fx_flap(int x, int y);
void fx_land(int x, int y);
void fx_update(const world *w);
int  depth_level(void);

/* bot.c: plays from the screen (OAM) only */
int  bot_decide(int player);
void bot_notify_flap(int player);
void bot_reset(void);

/* save states: each console-side file registers its objects (rs_state_var...), called from main.c */
void draw_state(void);
void sfx_state(void);
void bot_state(void);
void bot_state_loaded(void);

extern int opt_bot, opt_botstop;

/* main.c: test hook (tests/test_caps.c) */
const world *ls_test_world(int *state);

#endif
