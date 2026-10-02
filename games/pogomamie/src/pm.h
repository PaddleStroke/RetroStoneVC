/*
 * Pogo Mamie: shared declarations.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pogomamie/LICENSE.
 *
 * physics.c, world.c and gen.c are the game rules (no drawing, no sound: the unit tests link them alone);
 * draw.c, ui.c, sfx.c, bot.c and main.c are the console side.
 */
#ifndef PM_H
#define PM_H

#include <stdint.h>
#include "rs.h"
#include "tuning.h"

#define MAX_PLAYERS 4
#define BLD_MAX     48          /* buildings kept: from behind the camera to ~2 screens ahead */
#define OBJ_MAX     112
#define BUMP_MAX    2
#define HIST_MAX    32          /* the generator remembers the last roofs (a balloon's altitude) */

static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline int32_t min32(int32_t a, int32_t b) { return a < b ? a : b; }
static inline int32_t max32(int32_t a, int32_t b) { return a > b ? a : b; }
static inline int iabs(int v) { return v < 0 ? -v : v; }

/* ---- the buildings (and the Seine's quays, bridges and barges) ----------------------------------------------------- */
enum { BK_HOUSE, BK_HAUSS, BK_QUAY, BK_BRIDGE, BK_BARGE, BK_KINDS };
enum { RF_FLAT, RF_MANSARD, RF_PITCH };
#define MANSARD_W 16            /* 45-degree zinc slopes at both ends, 16 px wide and tall */

typedef struct bldg {
    int32_t x0, x1;             /* [x0, x1), multiples of 8 */
    int16_t top;                /* the highest roof line (flat part, ridge), a multiple of 8 */
    uint8_t kind, roof, style, district;
    int16_t sky0, sky1;         /* skylight [sky0, sky1) relative to x0 (sky1 = 0: none), multiples of 8 */
    uint8_t sky_broken, pad;
    int16_t bump_x[BUMP_MAX];   /* chimneys / bouquinistes' boxes / a barge's cabin: x relative to x0 (-1 = none) */
    uint8_t bump_w[BUMP_MAX], bump_h[BUMP_MAX];
    int32_t index;
} bldg;

/* ---- things on and between the roofs ------------------------------------------------------------------------------- */
enum {
    OB_NONE, OB_PIGEON, OB_ANTENNA, OB_AWNING, OB_POT, OB_CRADLE, OB_LINE, OB_LEDGE, OB_ITEM, OB_GUST, OB_BAGUETTE,
    OB_BALLOON, OB_KINDS
};
enum { IT_UMBRELLA, IT_BAGUETTE, IT_YARN, IT_CROISSANT, IT_COUNT };
/* a pigeon's way (obj.var): walking on a roof; flying in a gap: flapping in place, gliding, swooping */
enum { PG_WALK, PG_HOVER, PG_GLIDE, PG_SWOOP };

typedef struct obj {
    uint8_t kind, state, var, flags;    /* state: 0 = there; var: item type, awning side, pigeon's way... */
    int32_t x;                          /* world x of the left edge (a flying pigeon: of its path) */
    int16_t y;                          /* world y of the surface (landing props), of the feet (pigeons; a flying
                                         * one: at the ends of its path), of the top (a balloon), of the line's
                                         * left end */
    int16_t w;                          /* width (a flying pigeon: of its path, 0 when flapping in place) */
    int16_t a, b;                       /* pigeon: walk range [a, b] relative to x (flying: dip or bob, period);
                                         * cradle: y range; line: rest sag, the right end's y */
    int16_t t;                          /* animation / timer (a flying pigeon: its clock) */
    int32_t pos;                        /* pigeon: Q16 x offset; cradle: current y (Q16); balloon: x (Q16) */
    int16_t dir, c;                     /* pigeon, gust, balloon: direction; line: the load point (relative x);
                                         * flying pigeon: its clock's phase */
    int32_t sag, sagv;                  /* line: the dip at the load point and its speed (Q8 px, down > 0) */
    int16_t rider, pad;                 /* line: the player riding it + 1 (0: none) */
    int32_t bld;                        /* the building it belongs to (its index; tests) */
} obj;

/* ---- surfaces and hits --------------------------------------------------------------------------------------------- */
enum { SF_NONE, SF_ROOF, SF_BUMP, SF_SKY, SF_PIT, SF_PIGEON, SF_AWNING, SF_POT, SF_CRADLE, SF_LINE, SF_LEDGE,
       SF_BAGUETTE, SF_BALLOON };
typedef struct hit {
    int kind, idx, sub;         /* kind SF_*, building or object index, bump number */
    int32_t y;                  /* the surface (px) */
    int slope;                  /* -2, -1, 0, 1, 2: downhill direction x steepness (2 = 45 degrees) */
} hit;

/* a hazard's box (world px, [x0, x1) x [y0, y1)) */
typedef struct hbox { int x0, y0, x1, y1; } hbox;

/* ---- one player ----------------------------------------------------------------------------------------------------- */
enum { MS_READY, MS_AIR, MS_SLING, MS_REEL, MS_FALL, MS_DOWN, MS_OFF, MS_TUMBLE };
enum { BN_NORMAL, BN_BIG, BN_SPRING, BN_SLING, BN_RECOVER };
#define MS_ALIVE(s) ((s) == MS_READY || (s) == MS_AIR || (s) == MS_SLING || (s) == MS_REEL || (s) == MS_TUMBLE)

typedef struct mamie {
    int state, t;
    int32_t x, y;               /* Q16 world: x of the centre, y of the feet (the pogo's tip) */
    int32_t vx, vy;             /* Q16 px/frame */
    int bounce, land_t;         /* the last bounce kind; frames since the last landing (squash and stretch) */
    int land_kind, land_y;      /* the last surface landed on (SF_*) and its y (px) */
    int face, stumble_t, knock_t;
    int umbrella_t, croissant_t, yarn, baguette;
    int reel_x, reel_y;         /* the yarn rescue: the ledge she is pulled to */
    int32_t start_x;
    int dist_m, score, chain, best_chain, stunts;
    int landings, big_bounces, pigeons, falls_saved;
    int down_kind;              /* 0 café awning, 1 the river */
    int fall_phase;             /* the comic fall: 0 falling, 1 bounced off the café awning */
    int item_got, stunt_pts;    /* the last power-up taken, the last stunt's points (effects) */
    int line, line_x;           /* riding a clothesline: its object, where she landed on it */
    int32_t line_sag0;          /* ... the line's dip when she landed (Q8) */
    int32_t last_bump;          /* the chimney of the last stunt (a stunt counts once per chimney) */
    int tumbles, deflect;       /* hazards hit; the last bounce's slope deflection (-1 back, 1 forward, 0 none) */
    int hit_kind;               /* what knocked her off (OB_*; effects) */
    int out_t;                  /* frames since she dropped out (2-4 players), the results' order */
} mamie;

/* ---- the terrain, as the physics sees it (the world, or the bot's view of the screen) -------------------------------- */
typedef struct terrain {
    /* the highest surface crossed by the foot going from (x0, y0) to (x1, y1) (Q16), or 0 */
    int (*land)(const void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, hit *h);
    /* a wall at world x for feet at y (px): 1 if solid (the body of a building rises above the step) */
    int (*solid)(const void *ctx, int x, int feet_y);
    /* wind drift at world x, Q16 px/frame */
    int32_t (*wind)(const void *ctx, int x, int y);
    const void *ctx;
} terrain;

/* ---- physics.c: pure functions ---------------------------------------------------------------------------------- */
enum { EV_LAND = 1, EV_BONK = 2, EV_BIG = 4, EV_START = 8, EV_STUNT = 16, EV_PIGEON = 32, EV_KNOCK = 64,
       EV_STUMBLE = 128, EV_ITEM = 256, EV_BREAK = 512, EV_GLASS = 1024, EV_SPRING = 2048, EV_SLING = 4096,
       EV_DOOMED = 8192, EV_DOWN = 16384, EV_SAVED = 32768, EV_OUT = 65536, EV_MEOW = 131072, EV_POT = 262144,
       EV_BASKET = 1 << 19, EV_BALLOON = 1 << 20, EV_DEFLECT = 1 << 21, EV_RECOVER = 1 << 22 };

void    mamie_reset(mamie *m, int32_t x, int y);
int32_t vx_after_input(const mamie *m, int dir);            /* one frame of air control */
int     mamie_air_step(mamie *m, const terrain *T, int dir, hit *h);   /* EV_LAND (h filled), EV_BONK */
void    mamie_tumble_step(mamie *m);                                    /* knocked off: no control, no landing */
int     mamie_move(mamie *m, const terrain *T, hit *h);                 /* gravity and the move (no control) */
int32_t bounce_speed(int kind);
void    mamie_bounce(mamie *m, const hit *h, int big);       /* sets the speed of the bounce off surface h */
/* the bounce off a slope: speed v leaves along the half angle between up and the roof's normal */
void    slope_deflect(int slope, int32_t v, int32_t *vx, int32_t *vy);
int     bldg_surface(const bldg *b, int x);                  /* roof surface y at world x (in the building) */
int     bldg_drawn_top(const bldg *b, int x);                /* the first drawn pixel of column x (bumps included) */
int     bldg_slope(const bldg *b, int x);                    /* the downhill direction x steepness at x */
int     bump_top(const bldg *b, int k);
int     medal_of(int dist_m);                                /* 0 none, 1 bronze .. 4 the cat caught */
int     difficulty_at(int32_t x);                            /* 0..1024 */
int     district_at(int32_t x);                              /* 0..3 */
int     night_at(int32_t x);                                 /* the loop count: 0 = the first (day) */
static inline int box_hit(int ax0, int ay0, int ax1, int ay1, const hbox *b)
{
    return ax0 < b->x1 && b->x0 < ax1 && ay0 < b->y1 && b->y0 < ay1;
}
/* her body's box for the feet at (x, y) px */
static inline hbox body_box(int x, int y) { hbox b = {x - HALF_W, y - BODY_H, x + HALF_W + 1, y}; return b; }

/* ---- world.c: a run ------------------------------------------------------------------------------------------------ */
typedef struct cat {
    int32_t x, y;               /* px: the feet */
    int32_t fx, fy, tx, ty;     /* jump from / to */
    int state, t, face;
} cat;

typedef struct world {
    bldg b[BLD_MAX];
    int nb;
    int32_t next_index;
    obj o[OBJ_MAX];
    int no;
    rs_rng rng;                 /* the generator */
    uint32_t seed;
    int32_t items_due;          /* world x of the next power-up */
    int32_t gust_end;           /* the current gust zone ends here (generator) */
    int32_t balloon_due;        /* world x of the next balloon (generator) */
    int32_t hist_x0[HIST_MAX];  /* the last roofs (generator): their ends and their highest drawn pixel */
    int32_t hist_x1[HIST_MAX];
    int16_t hist_top[HIST_MAX];
    int hist_n;
    mamie m[MAX_PLAYERS];
    int players, t, started, fixed_seed;
    int32_t camx, camy;         /* Q8 camera (the top-left corner of the screen, world px) */
    int32_t cam_focus;          /* Q8: the feet y the camera centres on */
    cat c;
    int events[MAX_PLAYERS];
    int leader;
    int32_t skip_x;             /* debug: the first roof starts this far along (--opt skip=M metres) */
    int outs;                   /* players out so far (2-4 players: the order they dropped out) */
} world;

void world_init(world *w, int players, uint32_t seed, int skip_m);
void world_step(world *w, const int dir[MAX_PLAYERS], const int a[MAX_PLAYERS], const int a_pressed[MAX_PLAYERS]);
int  world_camx(const world *w);                              /* integer camera, px */
int  world_camy(const world *w);
int  world_running(const world *w);                           /* someone is still bouncing */
int  world_all_down(const world *w);                          /* everyone is down (landed in the street) or out */
int  world_dist(const world *w);                              /* the leader's distance, m */
const bldg *world_bldg_at(const world *w, int x);
int  world_bldg_index_at(const world *w, int x);
terrain world_terrain(const world *w, int buildings_only);
int  world_doomed(const world *w, const mamie *m);
int  obj_line_y(const obj *o, int x);                         /* a clothesline's y at world x */
int32_t line_dip_target(const obj *o, int k);                 /* its full dip (Q8) under a load at k (relative x) */
void pigeon_at(const obj *o, int *cx, int *feet);             /* a pigeon's centre x and feet y now */
void pigeon_at_t(const obj *o, int t, int *cx, int *feet);    /* ... at its clock t (a flying one) */
int  pigeon_period(const obj *o);                             /* frames of a flying pigeon's loop */
int  pigeon_swept(const obj *o, hbox *out, int max);          /* the boxes it can be in, at any time */
int  balloon_x(const obj *o);                                 /* a balloon's left edge now */
void world_camera(world *w, int snap);

/* ---- gen.c: the endless generator ----------------------------------------------------------------------------------- */
void gen_reset(world *w);
void gen_ahead(world *w, int32_t until_x);                    /* generate buildings up to until_x */
/* the worst-case check: from a standstill `takeoff` px before the end of building `from`, a normal (big = 0) or big
 * bounce, holding Right until past `aim` (relative to the next building's x0), then braking, lands on building `to`
 * without touching a hazard (an antenna, a pigeon anywhere along its path); a landing on a slope that throws her
 * back must be followed by a recovery bounce that lands on `to` again */
int  gen_reachable(const world *w, int from, int to, int takeoff, int big, int aim);
int  gen_gap_ok(const world *w, int from, int to);             /* any of the take-off points, bounces and aims works */
int  gen_hazards(const world *w, int from, int to, hbox *out, int max);  /* the hazards the check avoids */
extern const int gen_takeoffs[3];
void gen_add_baguette(world *w, int32_t from_x);
int  obj_new(world *w);

/* ---- console side ---------------------------------------------------------------------------------------------------- */
/* sfx.c */
enum { SFX_BOING, SFX_BOING_BIG, SFX_SPRING, SFX_COO, SFX_CRACK, SFX_GLASS, SFX_POP, SFX_MEOW, SFX_WHISTLE,
       SFX_JINGLE, SFX_CLANG, SFX_PICKUP, SFX_THUMP, SFX_SPLASH, SFX_GRUMBLE, SFX_STUNT, SFX_JOIN, SFX_PAUSE,
       SFX_SWISH, SFX_MEDAL, SFX_COUNT };
void sfx_init(void);
void sfx(int id);
void sfx_at(int id, int screen_x, int pitch);
void music_district(int district, int night);
void music_stop(void);
void audio_set(int music, int sound);

/* draw.c; the screens */
enum { DS_TITLE, DS_READY, DS_PLAY, DS_FALL, DS_OVER };
void draw_init(void);
void draw_frame(const world *w, int state, int st_t, int paused);
void draw_reset(const world *w);
void draw_invalidate(void);
void fx_event(const world *w, int p, int ev, const hit *h);
void fx_update(const world *w);
int  draw_scroll_x(void);            /* the BG2 scroll registers as last set (the bot reads the screen through them) */
int  draw_scroll_y(void);
void draw_camera(int *x, int *y);   /* tests: the camera of the last frame drawn (world px, the shake included) */
void draw_test_backdrop(int on);    /* tests: a flat magenta backdrop */
int  draw_player_pal(int p);        /* the OBJ palette of player p's sprite */
/* the rope of a clothesline as drawn: the segments' end points (world px); returns the count */
int  draw_line_points(const obj *o, int *xs, int *ys, int max);

/* ui.c: text, panels, the title, the HUD (to move to games/common/house_ui) */
void ui_init(void);
void ui_frame(const world *w, int state, int st_t, int best_m, int best_score, int new_best, int paused);
void ui_sprites(const world *w, int state, int st_t, int best_m);

/* bot.c: plays from the screen (OAM, the BG2 map and its scroll) */
void bot_decide(int player, int *dir, int *a);
void bot_reset(void);

/* save states: each console-side file registers its objects */
void draw_state(void);
void ui_state(void);
void sfx_state(void);
void bot_state(void);
void bot_state_loaded(void);

extern int opt_bot;

/* main.c: test hooks */
const world *pm_test_world(int *state);

#endif
