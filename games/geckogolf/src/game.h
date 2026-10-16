/*
 * Gecko Golf: shared declarations.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/geckogolf/LICENSE.
 *
 * play.c is the game rules (no drawing, no sound: deterministic, testable alone): the endless course (generated
 * hole by hole from the seed), the ball's physics, the turns and the stroke budget, the shot search of the bot;
 * draw.c is the video (the course drawn into BG tiles as the camera moves); main.c the flow (the title with the
 * players joining, play, game over), input (pads and touch), sound, save RAM, options and save states.
 * Made from games/_template (tools/new_game.py): the house style kit is games/common (docs/art-direction.md).
 */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "rs.h"

#define MAX_PLAYERS 4                   /* the house kit's HU_MAX_PLAYERS: 4 pads */
#define FX 65536                        /* Q16 fixed point: positions and speeds in 1/65536 px (per frame) */

/* ---- tuning: every gameplay number is here ------------------------------------------------------------------- */
/* the stroke budget (DESIGN.md "Rules") */
#define START_STROKES   12      /* each player starts with this many strokes */
#define REFUND_ACE      3       /* a hole in one gives 3 strokes back (net +2) */
#define REFUND_TWO      2       /* a hole in two gives 2 back (it costs nothing); three or more: nothing back */
#define PENALTY         1       /* water or out of bounds: one penalty stroke, the ball back where it was hit */
#define STROKES_MAX     30      /* the budget never grows past this */
#define MEDAL_SCORES    {6, 12, 24, 40}          /* holes completed: bronze, silver, gold, pearl */

/* the shot */
#define GRAVITY         (FX * 12 / 100)          /* px per frame^2 */
#define V_MIN           (FX * 30 / 100)          /* the softest shot, px per frame */
#define V_MAX           (FX * 66 / 10)           /* the hardest: about 330 px on the flat at 45 degrees */
#define GAUGE_MAX       120     /* the power gauge's top */
#define GAUGE_RATE      2       /* gauge units per frame while A is held: full in 1 s, then back down (ping-pong) */
#define GAUGE_MIN_SHOT  6       /* a release below this is not a shot (a tap of A costs nothing) */
#define AIM_MIN         8       /* the aim, in half degrees from the right horizon (0) through up (180) to the */
#define AIM_MAX         352     /* left horizon (360): 4..176 degrees */
#define AIM_START       90      /* 45 degrees to the right */
#define ARC_FRAMES      26      /* the preview: the first frames of the flight (no full trajectory) */
#define ARC_STEP        3       /* a dot every 3 frames */

/* the ball on the surfaces: bounce (x/256 of the speed into the ground kept), slide along it on a bounce (x/256),
 * rolling friction (px per frame^2), the steepest pull the ball rests against (px per frame^2) */
#define GRASS_E         96
#define GRASS_KEEP      214
#define GRASS_MU        (FX * 22 / 1000)
#define GRASS_HOLD      (FX * 30 / 1000)
#define SAND_E          16      /* sand stops the ball where it lands */
#define SAND_KEEP       40
#define SAND_MU         (FX * 25 / 100)
#define SAND_HOLD       (FX * 12 / 100)
#define ICE_E           110     /* ice: it slides and slides */
#define ICE_KEEP        252
#define ICE_MU          (FX * 25 / 10000)
#define ICE_HOLD        (FX * 4 / 1000)
#define MUSH_E          232     /* the bouncy mushrooms: never rest on one */
#define MUSH_KEEP       240
#define MUSH_MIN        (FX * 19 / 10)           /* the least bounce off a mushroom */
#define MUSH_NUDGE      (FX * 6 / 10)            /* off the top of the cap */
#define ROLL_VN         (FX * 45 / 100)          /* a bounce slower than this (off the ground) turns into rolling */
#define STOP_U          (FX * 3 / 100)           /* rolling slower than this on a slope that holds it: at rest */
#define WALL_STEP       3       /* px: a height step bigger than this between columns is a wall, not a slope */
#define WALL_TOL        3       /* px: a ball more than this below a column's top hits its side */
#define WALL_E          128     /* a wall gives back half the speed */
#define LEAVE_TOL       (FX * 3 / 4)             /* the ground falls away more than this: the ball flies off */
#define BALL_MAX_T      1200    /* frames: a ball still moving after 20 s stops */
#define CUP_W           8       /* px: the cup (the ball is 4 px) */
#define CUP_DEPTH       9
#define CUP_IN          3       /* px below the lip: in */
#define CUP_CAPTURE     (FX * 17 / 10)           /* a ball rolling over the cup slower than this drops in */
#define OB_LEFT         2       /* stage x: past these the ball is out of bounds (penalty) */
#define OB_RIGHT        318

/* the course (DESIGN.md "The course"): one hole per 320-px stage; the camera pans to the next */
#define TEE_X           40      /* stage x of the tee: the last hole's cup */
#define TEE_FLAT        12      /* px of flat ground after the tee */
#define GREEN_HALF      10      /* px of flat green before the cup (and TEE_FLAT after it) */
#define DIST_MIN        112     /* px from the tee to the cup */
#define DIST_RND0       60      /* + random(DIST_RND0 + DIST_RND_D * difficulty) */
#define DIST_RND_D      4
#define DIST_MAX        258
#define CUP_DY0         18      /* the cup's height differs from the tee's by up to CUP_DY0 + CUP_DY_D * diff */
#define CUP_DY_D        3
#define CUP_DY_MAX      90
#define Y_TOP           68      /* the highest ground (screen y) */
#define Y_BOT           212     /* the lowest */
#define CUP_Y_TOP       84
#define CUP_Y_BOT       204
#define DIFF_MAX        36      /* the difficulty grows with the hole number up to this */
#define HOLES_PER_BIOME 6       /* meadow, dunes, snowy hills, night, candy land, then again (harder) */
#define PRELUDE         240     /* px of course drawn before the first tee */
#define GEN_AHEAD       700     /* the course is generated this far past the camera (the same at every width) */
#define PROPS_PER_HOLE  4       /* the decor along a hole */

/* the flow (frames) */
#define SETTLE_FRAMES   24      /* the ball at rest, before the next turn */
#define HOLED_FRAMES    70      /* the ball in the cup: the cheer */
#define SPLASH_FRAMES   50      /* water or out of bounds, before the ball comes back */
#define PAN_FRAMES      80      /* the camera to the next hole */
#define MSG_FRAMES      100     /* a message on screen */
#define BIOME_FRAMES    150     /* a new course's name */
#define PANEL_DELAY     40      /* frames between the last player out and the game-over panel */
#define RETRY_LOCK      36      /* frames the panel ignores the button (no accidental retry) */
#define READY_BOB       4       /* the title hero's bob amplitude, px */
#define BOT_START       20      /* frames on the title before the bot starts the run */
#define BOT_THINK       24      /* frames the bot "looks" before it aims */

/* touch (player 1): the slingshot */
#define TOUCH_DEAD      10      /* px: a shorter pull is no shot (let go: nothing happens) */
#define TOUCH_FULL      110     /* px: this long a pull is full power */

/* ---- play.c: the rules ------------------------------------------------------------------------------------------ */
#define TERRAIN_N   2048        /* the course's ring buffer (px) */
#define TMASK       (TERRAIN_N - 1)
#define HOLE_RING   16

enum surface { SF_GRASS, SF_SAND, SF_ICE, SF_MUSH, SF_WATER, SF_CUP, SF_COUNT };
enum biome { BI_MEADOW, BI_DUNES, BI_SNOW, BI_NIGHT, BI_CANDY, BI_COUNT };
enum shape { SH_ROLL, SH_HILL, SH_VALLEY, SH_CLIFF_UP, SH_CLIFF_DOWN, SH_LEDGE, SH_COUNT };
enum hazard { HZ_POND = 1, HZ_BUNKER = 2, HZ_ICE = 4, HZ_MUSH = 8 };
enum prop_kind { PR_NONE, PR_FLOWER, PR_TUFT, PR_CACTUS, PR_ROCK, PR_PINE, PR_SNOWMAN, PR_GLOWSHROOM, PR_FIREFLY,
                 PR_LOLLIPOP, PR_CANDYCANE, PR_COUNT };

typedef struct prop { int32_t x; int16_t y; uint8_t kind, var; } prop;

typedef struct hole_rec {
    int32_t tee_x, cup_x;       /* world px */
    int16_t tee_y, cup_y;       /* the ground's y at the tee and at the cup's lip */
    uint8_t biome, seq, shape, hazards;  /* seq: the biome's number along the run (hole / HOLES_PER_BIOME) */
    prop props[PROPS_PER_HOLE];
} hole_rec;

typedef struct terrain {
    int16_t h[TERRAIN_N];       /* the ground's y at world x (& TMASK) */
    uint8_t s[TERRAIN_N];       /* its surface (SF_*) */
    uint8_t q[TERRAIN_N];       /* its biome's seq (low 8 bits): the palette to draw it with */
    hole_rec holes[HOLE_RING];  /* hole k at k % HOLE_RING */
    int32_t gen_x;              /* generated up to here (world px, exclusive) */
    int gen_hole, gen_y, prev_shape;
    uint32_t seed;
    uint32_t rev;               /* +1 at every change (the drawing rebuilds) */
} terrain;

enum ball_state { BS_REST, BS_FLY, BS_ROLL, BS_CUP, BS_SPLASH, BS_OB, BS_OFF };
typedef struct ball {
    int32_t x, y;               /* Q16, STAGE coordinates of the current hole (x relative to world.hole_cam); y the
                                   ball's bottom (the contact point) */
    int32_t vx, vy, u;          /* flying: the speed; rolling: u along the ground (+ = right) */
    int32_t lx, ly;             /* the last spot: where it was hit from */
    uint16_t t;                 /* frames moving */
    uint8_t state, surf;        /* BS_*, the surface last touched */
    uint8_t mush;               /* mushroom bounces in a row */
    uint8_t pad[3];
} ball;

typedef struct golfer {
    ball b;
    int strokes;                /* the budget left */
    int hole_strokes;           /* this hole's strokes (penalties included) */
    int last_hole;              /* the strokes of the last hole played (the honour at the next tee) */
    int holes, total, aces;     /* holes completed (the score), strokes used, holes in one */
    int aim;                    /* half degrees: 0 right .. 180 up .. 360 left */
    uint8_t out, done, pad[2];  /* out of strokes; holed this hole */
} golfer;

/* the player's input this frame (main.c makes it from the pad, the touches or the bot) */
typedef struct pin {
    int16_t turn;               /* aim change, half degrees (+ = counter-clockwise: towards the left) */
    int16_t set_aim;            /* >= 0: set the aim (touch, bot); -1: no */
    int16_t set_gauge;          /* >= 0: set the gauge (the touch slingshot's pull); -1: no */
    uint8_t hold;               /* A held: charge (released while charging: the shot) */
    uint8_t shoot;              /* the slingshot let go: shoot at the gauge */
    uint8_t cancel;             /* B: no shot */
    uint8_t any;
} pin;

enum phase { PH_AIM, PH_CHARGE, PH_FLIGHT, PH_SETTLE, PH_HOLED, PH_SPLASH, PH_PAN, PH_OVER };
enum msg { MSG_NONE, MSG_ACE, MSG_TWO, MSG_HOLED, MSG_WATER, MSG_OB, MSG_OUT, MSG_BIOME };

typedef struct world {
    terrain tr;
    golfer g[MAX_PLAYERS];
    int players, turn;          /* the player whose shot it is */
    int phase, phase_t;
    int gauge, gauge_dir, charge_touch;
    int hole;                   /* the current hole (0-based) */
    int32_t hole_cam;           /* world x of stage x 0 for the current hole */
    int32_t view_cam;           /* the camera drawn (pans from one hole_cam to the next) */
    int32_t pan_from;
    int started, t;
    int msg, msg_p, msg_n, msg_t;   /* the last message: MSG_*, its player, a number (strokes back), frames */
    int shots;                  /* shots played (all players) */
    int events[MAX_PLAYERS];    /* EV_* of the last step, by player */
    int wevents;                /* EV_* of the world (pan, biome) */
    int bounce_surf;            /* the surface of the last bounce (the sound) */
} world;
enum { EV_SHOT = 1, EV_BOUNCE = 2, EV_CUP = 4, EV_SPLASH = 8, EV_OB = 16, EV_OUT = 32, EV_ACE = 64,
       EV_REFUND = 128, EV_TURN = 256, EV_PAN = 512, EV_BIOME = 1024, EV_MUSH = 2048, EV_CHARGE = 4096,
       EV_REST = 8192, EV_OVER = 16384, EV_AIM = 32768 };

void world_init(world *w, int players, uint32_t seed, int first_hole);
void world_step(world *w, const pin in[MAX_PLAYERS]);
int  world_running(const world *w);            /* someone still plays */
int  world_score(const world *w, int p);       /* holes completed */
void world_gen_ahead(world *w);                /* the course generated GEN_AHEAD past the hole's camera */
const hole_rec *world_hole(const world *w, int k);
int  world_cup_sx(const world *w);             /* the current cup's stage x (centre) */
int  terrain_y(const terrain *t, int32_t wx);  /* the ground's y at world x */
int  terrain_surf(const terrain *t, int32_t wx);
int  terrain_seq(const terrain *t, int32_t wx);
int  biome_of_seq(int seq);
int  isin_hd(int a);                           /* sin of a half-degree angle 0..360, Q16 */
int  icos_hd(int a);
void shot_velocity(int aim, int gauge, int32_t *vx, int32_t *vy);
int  touch_shot(int dx, int dy, int *aim, int *gauge);   /* the slingshot pull (screen px, y down) -> 1: a shot */
int  pin_none(pin *in);

/* the ball alone (tests, the bot): one frame of physics on the current hole; returns EV_* */
int  ball_step(const world *w, ball *b);
void ball_shoot(ball *b, int aim, int gauge);
/* simulate a shot from player p's ball to its end: returns BS_REST, BS_CUP, BS_SPLASH or BS_OB; *end the ball */
int  sim_shot(const world *w, int p, int aim, int gauge, ball *end);

/* the bot: the shot it plays (botskill 0 the best, 1..10 human-like: aim and power off by up to that much) */
typedef struct bot_plan { int16_t aim, gauge; int32_t hole, stroke, valid; } bot_plan;
void bot_plan_shot(const world *w, int p, int skill, bot_plan *out);
void bot_give_up(const world *w, int p, bot_plan *out);   /* a shot out of bounds (the tests' game over) */

/* ---- draw.c -------------------------------------------------------------------------------------------------------- */
enum { ST_TITLE, ST_PLAY, ST_DEAD, ST_OVER };      /* no "get ready": the title is the only menu */
typedef struct touch_view { int on, x0, y0, x, y, seen; } touch_view;   /* the slingshot's finger (screen px) */
void draw_init(void);
void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused, const touch_view *tv);
void draw_state(void);
void draw_state_loaded(void);       /* widescreen: after a state saved at another width */

/* ---- sound (main.c) ------------------------------------------------------------------------------------------------ */
enum { SFX_SWING, SFX_TOK, SFX_SAND, SFX_ICE, SFX_BOING, SFX_CUP, SFX_SPLASH, SFX_OB, SFX_ACE, SFX_TICK,
       SFX_OUT, SFX_COUNT };     /* the game's own sounds; the house set follows */
#define SFX_HOUSE SFX_COUNT      /* first slot of the house sounds (ha_init) */

#endif
