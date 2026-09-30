/*
 * Pancake Tower: shared declarations.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pancaketower/LICENSE.
 *
 * tower.c is the game rules (no drawing, no sound, no RNG: the unit tests link it alone); draw.c, scene.c,
 * sfx.c, bot.c and main.c are the console side.
 */
#ifndef PT_H
#define PT_H

#include <stdint.h>
#include "rs.h"
#include "tuning.h"

#define MAX_PLAYERS 2
#define RING 64                     /* the last layers kept (the screen shows at most 31) */

static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline int absi(int v) { return v < 0 ? -v : v; }

/* ---- tower.c: one tower (pure rules) -------------------------------------------------------------------- */
typedef struct geom {
    int w0, spawn;                  /* first width; slide half-range (px) */
    int32_t speed0;                 /* Q16 px/frame */
    int tol_min, tol_max;           /* the perfect window (px) */
    int regrow, slip;               /* px */
} geom;
extern const geom GEOM_1P, GEOM_2P;

enum layer_kind { LK_PANCAKE, LK_STRAWBERRY, LK_BLUEBERRY, LK_BANANA, LK_CHOCOLATE, LK_CREAM };
enum layer_flag { LF_SYRUP = 1, LF_BUTTER = 2, LF_PERFECT = 4, LF_BIG_BUTTER = 8 };

typedef struct layer {
    int16_t x, w;                   /* left edge relative to the plate's centre; width (px) */
    uint8_t kind, flags;
    uint16_t n;                     /* pancake number (1..), the art's variation */
} layer;

enum tower_state {
    TS_SLIDE,                       /* the slider ping-pongs */
    TS_DROP,                        /* it falls onto the tower (DROP_FRAMES) */
    TS_SLIP,                        /* it slides on syrup (SLIP_FRAMES) */
    TS_TOPPING,                     /* a topping falls (TOPPING_FRAMES) */
    TS_POUR,                        /* syrup is poured (POUR_FRAMES) */
    TS_MISSED                       /* game over for this tower */
};

/* events of the last step (bits) */
enum {
    EV_LAND = 1, EV_PERFECT = 2, EV_CUT = 4, EV_MISS = 8, EV_REGROW = 16, EV_TOPPING = 32, EV_TOPPING_LAND = 64,
    EV_POUR = 128, EV_SLIP = 256, EV_SPLASH = 512, EV_SPLASHED = 1024, EV_CEILING = 2048, EV_ROOF = 4096,
    EV_SPAWN = 8192, EV_DROP = 16384, EV_SPEEDUP = 32768
};

typedef struct cut_piece { int x, w, side, kind, whole; uint16_t n; } cut_piece;   /* side -1 left, +1 right; whole: a miss */

typedef struct tower {
    geom g;
    layer ring[RING];               /* layer i (0 = the lowest) is ring[i % RING] */
    int nlayers;                    /* pancakes + toppings */
    int pancakes, score, chain, best_chain, perfects;
    int state, t;                   /* state and frames in it */
    int32_t sx, vx;                 /* the slider: left edge (Q16, relative to the plate's centre), speed */
    int sw;                         /* its width */
    int drop_x;                     /* px: frozen at the press (then moved by the slip) */
    int slip_dir, slip_x0;
    int syrup;                      /* the top has syrup: the next pancake slips */
    int splash_in;                  /* syrup splashed by the rival while a pancake was already landing */
    int events;
    cut_piece cut;                  /* the last piece cut off (EV_CUT) or the missed pancake (EV_MISS) */
    int bonus;                      /* the bonus of the last landing */
} tower;

void  tower_init(tower *tw, const geom *g);
void  tower_step(tower *tw, int press);        /* one frame; press = the drop button went down */
const layer *tower_layer(const tower *tw, int i);
const layer *tower_top(const tower *tw);
int   tower_top_y(const tower *tw);            /* world y of the top surface */
int   tower_speed(const geom *g, int pancakes);  /* Q16 px/frame for the next pancake */
int   tower_tolerance(const geom *g, int w);   /* the perfect window for a width */
int   tower_slider_x(const tower *tw);         /* the slider's left edge in px (what is drawn) */
int   tower_slider_y(const tower *tw);         /* world y of the slider's bottom */
int   tower_side(int pancakes);                /* -1: the next pancake comes from the left, +1 from the right */
int   tower_topping_kind(int pancakes);        /* the topping after that many pancakes (0 = none) */
int   tower_syrup_due(int pancakes);           /* syrup is poured after that many pancakes */
void  tower_splash(tower *tw);                 /* the rival's syrup splash */
int   tower_alive(const tower *tw);
int   slip_offset(int slip, int k);            /* px slid after k frames of SLIP_FRAMES */

/* the cut: the slider at x (width w) onto the layer below (bx, bw). Returns 0 (miss), 1 (cut) or 2 (perfect);
 * fills the kept layer (kx, kw) and the piece cut off */
int   cut_resolve(const geom *g, int x, int w, int bx, int bw, int *kx, int *kw, cut_piece *piece);
int   segment_of(int world_y);                 /* SEG_* */
int   medal_of(int score);                     /* 0 none, 1 bronze .. 4 pearl */

/* ---- tower.c: a match (1 or 2 towers) ------------------------------------------------------------------ */
typedef struct match {
    tower tw[MAX_PLAYERS];
    int players, t;
} match;
void match_init(match *m, int players);
void match_step(match *m, const int press[MAX_PLAYERS]);
int  match_over(const match *m);               /* every tower has missed */
int  match_winner(const match *m);             /* 0 or 1; -1 = a draw */

/* ---- console side --------------------------------------------------------------------------------------- */
/* sfx.c */
enum { SFX_FLOP, SFX_DING, SFX_SQUISH, SFX_SPLAT, SFX_CRASH, SFX_ROOF, SFX_PLOP, SFX_POUR, SFX_SPLASH,
       SFX_WHOOSH, SFX_FANFARE, SFX_COUNT };        /* then the house set (house_audio.h HA_*) */
void sfx_init(void);
void sfx_play(int id, int x, int pitch_steps);   /* pitch in semitones (0 = as made) */
void sfx(int id);
void music_update(int segment, int playing);
void audio_set(int music, int sound);

/* draw.c; the screens (game states) */
enum { DS_TITLE, DS_READY, DS_PLAY, DS_OVER };
void draw_init(void);
void draw_reset(const match *m);
void draw_events(const match *m);               /* after each step: effects from the events */
void draw_update(const match *m);               /* after each step: cameras, effects, the chef */
void draw_frame(const match *m, int state, int st_t, int best, int new_best, int paused);
int  draw_camera(int player);                    /* world y at the screen's bottom (tests) */
void draw_oam_log(void);                         /* development: --opt oamlog=1 */
extern int opt_nodraw_bg;                        /* tests */

/* bot.c: plays from the screen (OAM) only */
int  bot_decide(int player);                     /* call it every frame (it watches the screen) */
void bot_reset(uint32_t seed);
void bot_jitter_counts(int out[5]);
void draw_view_oam(int player, int *first, int *count);   /* the player's part of the screen (OAM range) */
int  pt_players(void);
int  bot_stop_height(void);

/* save states: each console-side file registers its objects, called from main.c */
void draw_state(void);
void draw_state_loaded(void);
void sfx_state(void);
void bot_state(void);

extern int opt_bot;

/* main.c: test hooks */
const match *pt_test_match(int *state);

#endif
