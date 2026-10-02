/*
 * Beaver Rush: shared declarations.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/beaverrush/LICENSE.
 *
 * rules.c + world.c are the game rules (no drawing, no sound: the unit tests link them alone);
 * draw.c, scene.c, sfx.c, bot.c and main.c are the console side.
 */
#ifndef BR_H
#define BR_H

#include <stdint.h>
#include "rs.h"
#include "tuning.h"

#define MAX_PLAYERS 4

static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/* ---- rules.c: the trunk, its generator, the timer (pure functions) ----------------------------- */
enum { SIDE_NONE = 0, SIDE_L = 1, SIDE_R = 2 };
static inline int other_side(int s) { return s == SIDE_L ? SIDE_R : SIDE_L; }

typedef struct seg {
    uint8_t branch;             /* SIDE_* */
    uint8_t gold;               /* a golden log */
    uint8_t stolen;             /* a branch sent by the rival (versus) */
    uint8_t look;               /* bark variant 0..3 (cosmetic, from the generator) */
} seg;

#define GEN_QUEUE 12
typedef struct gen {
    rs_rng rng;
    int made;                   /* segments made */
    int last;                   /* branch side of the last segment made */
    int empty_run, same_run, since_gold;
    uint8_t queue[GEN_QUEUE];   /* the chunk being laid out: wanted branch sides (0 = empty) */
    int qn, qi;
} gen;

typedef struct tree {
    seg s[TRUNK_N];             /* s[0] = the bottom segment, level with the beaver's head */
    gen g;
    gen g_at8;                  /* the generator after segment 7 (a re-roll keeps the shown trunk) */
} tree;

void gen_init(gen *g, uint32_t seed);
seg  gen_next(gen *g);
int  gen_stage(const gen *g);   /* the stage of the next segment */
void tree_init(tree *t, uint32_t seed);
void tree_reroll(tree *t, uint32_t seed);   /* new segments from 8 up (off screen), same 0..7 */
void tree_pop(tree *t);                      /* the bottom segment is gnawed away */
/* a stolen branch: the first empty segment from `from` up where a branch keeps the trunk fair, on side
 * `pref` if allowed, else the other; returns the index or -1 */
int  tree_insert_branch(tree *t, int from, int pref);
int  trunk_fair(const seg *s, int n);        /* no opposite branches in adjacent segments */

int32_t drain_per_frame(int level);          /* BAR_FULL units */
int     level_of(int logs);
int     rate_q16(int level);                 /* gnaws per second that hold the bar, Q16 */

/* ---- world.c: a run (one to four beavers) ------------------------------------------------------- */
enum beaver_state { BV_READY, BV_PLAY, BV_BONK, BV_SLEEP, BV_WIN, BV_OFF };
enum { EV_GNAW = 1, EV_GOLD = 2, EV_BONK = 4, EV_SLEEP = 8, EV_MILESTONE = 16, EV_LEVEL = 32, EV_START = 64,
       EV_SENT = 128, EV_STOLEN = 256, EV_MOVE = 512 };

typedef struct beaver {
    int state, t;               /* state, frames in it */
    int side;                   /* SIDE_L / SIDE_R */
    int score, logs, golds;     /* score = logs + GOLD_BONUS x golds */
    int level;
    int32_t bar;
    int gnaw_t;                 /* frames since the last gnaw (animation) */
    int last_gnawed;            /* the segment gnawed last: its branch side | gold << 2 (cosmetic) */
    int bonk_hit;               /* 1 = hit A (the drop), 2 = hit B (walked in) */
    int stolen_sent;
    int sent_to;                /* versus: the player its last stolen branch went to (-1 none) */
    int out_t;                  /* versus: the frame it went out (bonk or out of breath), 0 = still standing */
    tree tr;
} beaver;

typedef struct world {
    beaver bv[MAX_PLAYERS];
    int players, t, started;
    uint32_t seed;
    int events[MAX_PLAYERS];
    int over;                   /* the run is over (1 player: out; versus: one beaver left standing, or none) */
    int winner;                 /* versus: the winning player, -1 a draw */
} world;

void world_init(world *w, int players, uint32_t seed);
/* one frame; press[p] = SIDE_L / SIDE_R when player p pressed a gnaw button this frame, else 0.
 * reroll: the seed that re-rolls the trees off screen at the first gnaw (0 = keep) */
void world_step(world *w, const int press[MAX_PLAYERS], uint32_t reroll);
int  world_dam_logs(const world *w);         /* logs of every beaver: the dam */
void world_skip(world *w, int logs);         /* tests: start with logs gnawed safely (no events, the bar full) */
int  world_stage(const world *w);            /* the scene: milestones reached by the best beaver */
int  medal_of(int score);                    /* 0 none, 1..4 */
/* versus: who gets the stolen branch of sender p's milestone: the leader (the highest score) among the OTHER
 * beavers still gnawing, ties to the first after p in turn order (p+1, p+2, ...); so when p leads it is the
 * runner-up. -1: nobody left. */
int  world_steal_target(const world *w, int p);
/* versus: the ranking key of each player (higher = better): the last standing first, then the later out (the
 * frame), equal frames by score; the results rank by it (hu_rank) */
void world_rank_keys(const world *w, int key[MAX_PLAYERS]);
int  world_standing(const world *w);         /* beavers still gnawing (BV_READY / BV_PLAY) */

/* ---- console side ------------------------------------------------------------------------------- */
/* sfx.c */
enum { SFX_CHOMP, SFX_SPLASH, SFX_GOLD, SFX_BONK, SFX_CHEER, SFX_SLEEP, SFX_TOK, SFX_WHOOSH, SFX_PANEL,
       SFX_PAUSE, SFX_JOIN, SFX_SPARKLE, SFX_COUNT };
void sfx_init(void);
void sfx(int id);
void sfx_pan(int id, int x, int pitch);
void music_update(int level, int playing);
void audio_set(int music, int sound);
int  music_tempo(void);

/* draw.c; the screens (game states: the title is the only menu, no "get ready"; 1 is free, the old one) */
enum { DS_TITLE = 0, DS_PLAY = 2, DS_END = 3, DS_OVER = 4 };
void draw_init(void);
void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused);
void draw_new_run(const world *w);
void draw_events(const world *w);            /* effects of the last step's events */
void draw_update(const world *w, int state); /* cosmetic motion, once per update */
int  draw_view_x(const world *w, int p);     /* the screen x of the centre of player p's tree (sound pan) */
/* the OAM range that holds the sprites of player p's view in the last frame (the whole OAM with one player) */
void draw_view_oam(int p, int *first, int *count);

/* bot.c: plays from the screen (BG maps, OAM) only */
int  bot_decide(int player);
void bot_reset(uint32_t seed);

/* save states: each console-side file registers its objects */
void draw_state(void);
void sfx_state(void);
void bot_state(void);

extern int opt_bot;

/* main.c: test hooks (tests/test_ui.c) */
const world *br_test_world(int *state);

#endif
