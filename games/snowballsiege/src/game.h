/*
 * Snowball Siege: shared declarations.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/snowballsiege/LICENSE.
 *
 * play.c is the game rules (no drawing, no sound: deterministic, testable alone): the snowy battlefield (a bitmap,
 * carved by the splats), the critters, the throws, the turns, the computer side and the camera; draw.c is the video,
 * main.c the flow (the title with the players joining, play, game over), input (pads and touch), save RAM, options and
 * save states. Made from games/_template (tools/new_game.py): the house style kit is games/common.
 */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "rs.h"

#define MAX_PLAYERS 4                   /* the house kit's HU_MAX_PLAYERS: 4 pads */
#define Q16_ONE 65536

/* ---- tuning: every gameplay number is here ---------------------------------------------------------------------- */
/* the battlefield: wider than any screen (the camera pans); the same at every width */
#define WORLD_W       768       /* px (96 tiles) */
#define WORLD_H       240
#define BEDROCK_Y     212       /* frozen ground from here down: never carved */
#define SURF_MID      158       /* the hills' mean surface (y) */
#define SURF_MIN      108       /* the highest a hilltop may be */
#define SURF_MAX      190       /* the lowest ground outside the ponds */
#define HILL_AMP      {16, 8, 3}            /* the hills: three waves (amplitude px; + a random part as much) */
#define HILL_PERIOD   {384, 150, 52}        /* their periods, px */
#define SIDE_X        {{120, 648, 0, 0}, {104, 384, 664, 0}, {96, 288, 480, 672}}   /* the forts' centres, 2-4 sides */
#define PLAT_HALF     34        /* each side's flat ground: +-34 px */
#define PLAT_BLEND    20        /* blended into the hills over 20 px */
#define FORT_DIST     24        /* the fort's walls start 24 px from its centre */
#define FORT_THICK    8
#define FORT_H        18        /* the front wall (towards the middle of the field) */
#define FORT_BACK_H   9         /* the back wall */
#define CREN_H        4         /* the crenels on top: 3 px of snow, 3 px of gap */
#define POND_Y        196       /* the frozen pond's surface (the thin ice) */
#define POND_HALF     38        /* its half width */
#define POND_DEPTH    12
#define POND_BANK     28        /* the ground slopes down to it over 28 px */
#define TREES_MIN     4         /* pine trees (decoration, knocked down by a splat) */
#define TREES_RND     3
#define TREE_H        30

/* the critters: 2 per side (DESIGN.md "Teams") */
#define TEAM          2
#define MAX_SIDES     4
#define MAX_CRITTERS  (TEAM * MAX_SIDES)
#define HP_MAX        6         /* half-hearts: 3 hearts */
#define CR_H          12        /* body height, px (feet at y, the body above) */
#define CR_R          6         /* the body's hit radius around its centre (x, y - 6) */
#define CRITTER_GAP   {6, -10}  /* the two critters of a side, from its centre, towards the middle */
#define CLIMB         3         /* a step up or down of 3 px is walked over */
#define WALK_SPEED    (Q16_ONE / 2)         /* px a frame */
#define WALK_TURN     6         /* frames a direction is held before walking (a tap only turns round) */
#define MOVE_BUDGET   40        /* px a critter may walk in its turn */
#define SHUFFLE_PX    8         /* a touch tap beside the critter walks this far */
#define G_CRIT        (Q16_ONE * 15 / 100)  /* gravity on a knocked or falling critter, px/frame^2 */
#define FALL_VMAX     (Q16_ONE * 6)
#define FALL_SAFE     48        /* a fall up to 48 px does nothing ... */
#define FALL_PER      32        /* ... then 1 half-heart, and one more every 32 px */
#define KNOCK_BASE    (Q16_ONE * 6 / 10)    /* a splat pushes a critter away: 0.6 px/frame ... */
#define KNOCK_PER     (Q16_ONE * 45 / 100)  /* ... + 0.45 per half-heart of damage ... */
#define KNOCK_UP      (Q16_ONE * 12 / 10)   /* ... and up by 1.2 */

/* throwing */
#define AIM_MIN       (-30)     /* degrees above the horizontal, towards where the critter faces */
#define AIM_MAX       88
#define AIM_START     45
#define AIM_RATE      2         /* frames per degree while up/down is held */
#define POWER_MAX     1000      /* power units */
#define POWER_START   150       /* where the gauge starts when A goes down */
#define CHARGE_STEP   12        /* per frame A is held: full after 71 frames (1.2 s), then it throws by itself */
#define V_MAX         (Q16_ONE * 82 / 10)   /* the throw's speed at full power: 8.2 px/frame */
#define G_PROJ        (Q16_ONE / 10)        /* gravity on a throw: 0.1 px/frame^2 (full power at 45 deg: 670 px) */
#define WIND_MAX      10        /* the wind, -10..10 (left .. right) */
#define WIND_STEP     4         /* it changes by up to 4 each turn */
#define WIND_ACC      98        /* Q16 px/frame^2 per wind level (wind 10 moves a long throw ~100 px) */
#define PROJ_SAFE     6         /* frames a throw cannot hit its thrower */
#define PROJ_MAX_T    900       /* a throw still flying after 15 s is lost */
#define BOUNCE_KEEP   150       /* /256 of the speed an ice cube keeps at a bounce */
#define BOUNCE_VMIN   (Q16_ONE * 6 / 10)
/* the throwables: hit radius, splat (carve) radius, damage at the centre (half-hearts), wind (%), bounces, stock
 * per side and match (-1: any number) */
enum { PK_SNOW, PK_BIG, PK_ICE, PK_COUNT };
#define PK_TABLE { {2, 11, 3, 100, 0, -1}, {3, 18, 5, 60, 0, 2}, {2, 8, 3, 80, 3, 2} }

/* turns */
#define TURN_FRAMES   (20 * 60) /* the turn timer: 20 s */
#define TICK_FROM     (5 * 60)  /* the last 5 s tick */
#define SETTLE_MIN    40        /* frames after a splat before the next turn (everything must have landed) */
#define SETTLE_MAX    360
#define TIMEUP_WAIT   50
#define END_WAIT      150       /* frames from the last out to the next match (1 player) or the game over */
#define FROST_TURN    30        /* from the 30th turn, the cold bites: every critter loses half a heart a turn ... */
#define MAX_TURNS     80        /* ... and the 80th ends the match (the most hearts left wins) */
#define BANNER_T      100       /* "YOUR TURN!", "MATCH 2" */

/* scoring (1 player; also the 2-4 player results) */
#define PTS_DAMAGE    10        /* per half-heart of damage to the other side */
#define PTS_SNOWED    50        /* a critter snowed under (0 hearts) */
#define PTS_SPLASH    100       /* a critter knocked into the pond */
#define PTS_DOUBLE    50        /* one throw hitting two critters of the other side */
#define PTS_LONG      30        /* a hit from 300 px away or more */
#define LONG_DIST     300
#define PTS_WIN       150       /* a match won: 150, + 50 per level above 1 ... */
#define PTS_WIN_LEVEL 50
#define PTS_HP_LEFT   20        /* ... + 20 per half-heart your side has left */
#define MEDAL_SCORES  {300, 900, 1800, 3000}    /* bronze, silver, gold, pearl (DESIGN.md "Medals") */

/* the computer side (1 player): a ballistic search, then errors that shrink match by match (level 1, 2, 3...) */
#define CPU_LEVELS    8         /* the tables' length: level 8 and above use the last entry */
#define CPU_ANG_ERR   {7, 6, 5, 4, 3, 2, 1, 1}               /* +- degrees */
#define CPU_POW_ERR   {90, 75, 60, 46, 34, 24, 14, 8}        /* +- power units */
#define CPU_WIND_READ {20, 40, 55, 70, 82, 92, 100, 100}     /* % of the wind it allows for */
#define CPU_WIND_NOISE {2, 2, 1, 1, 1, 0, 0, 0}              /* +- wind levels misread */
#define CPU_THINK     {80, 70, 60, 54, 48, 42, 36, 32}       /* frames it "thinks" before it aims */
#define CPU_BIG_PCT   60        /* it throws a big snowball at a strong critter or a huddle this often (%) */
#define CPU_ICE_PCT   12
#define CPU_SIMS      90        /* trajectories simulated per frame (spread over the thinking time) */
#define CPU_ANG_STEP  2         /* the search: every 2 degrees, every 2nd gauge step, then refined around the best */
#define CPU_POW_STEP  2

/* the camera (presentation: it never changes the rules) */
#define CAM_LERP      8         /* moves 1/8 of the way a frame ... */
#define CAM_VMAX      8         /* ... at most 8 px */
#define CAM_PAN       4         /* L/R (or a finger dragged on the field) pans 4 px a frame */
#define CAM_AHEAD     150       /* a throw: the camera frames the ball and where it will be 150 frames on (or lands) */
#define CAM_EDGE      44        /* ... the ball kept about 44 px inside the screen */

/* touch (player 1; DESIGN.md "Touch") */
#define SLING_GRAB    22        /* a finger down this near the active critter pulls the slingshot */
#define SLING_MIN     10        /* a pull shorter than this is cancelled on release */
#define SLING_MAX     72        /* px of pull for full power */
#define SHUFFLE_ZONE  56        /* a tap within 56 px left or right of the critter shuffles it */
#define BTN_R         15        /* the round switch button (bottom right) */
#define BTN_X         (-24)     /* its centre: from the screen's right edge */
#define BTN_Y         200

#define PANEL_DELAY   40        /* frames between the last out and the game-over panel */
#define RETRY_LOCK    36        /* frames the panel ignores the button (no accidental retry) */
#define BOT_START     20        /* frames on the title before the bot starts the run */

/* ---- play.c: the rules ------------------------------------------------------------------------------------------ */
enum { CS_STAND, CS_AIR, CS_SNOWED, CS_SPLASHED };
typedef struct critter {
    int32_t x, y, vx, vy;       /* Q16: x the centre, y the feet (the row of ground under them) */
    int8_t side, state, hp, facing;
    int16_t apex;               /* the highest y of the current fall (fall damage) */
    int16_t hurt_t, anim_t;     /* frames since hit (flash), since the last pose change */
    int16_t out_t;              /* frames since out */
    int8_t last_hit;            /* the side that hit it last (-1: none): who scores a fall or a splash */
    int8_t pad;
} critter;

typedef struct side {
    int8_t on, player, species, cpu;   /* player 0..3 (-1: the computer), species 0..3, cpu level (0: a human) */
    int8_t next;                /* the next of its critters to play */
    int8_t stock[PK_COUNT];     /* throwables left (-1: any number) */
    int8_t out_turn_set;
    int16_t out_turn;           /* the turn it lost its last critter (the 2-4 player ranking) */
    int16_t damage;             /* half-hearts dealt to the other sides */
    int32_t score;
} side;

typedef struct proj {
    int8_t on, kind, bounces, side, crit, pad;
    int16_t t;
    int32_t x, y, vx, vy;       /* Q16 */
    int32_t x0;                 /* where it was thrown from (the long shot) */
} proj;

typedef struct tree { int16_t x, y; int8_t on, kind; } tree;
typedef struct rect16 { int16_t x0, y0, x1, y1; } rect16;          /* inclusive */
enum { FX_CHUNK, FX_PUFF, FX_SPLASH, FX_TREE, FX_SPARK };
typedef struct fx { int32_t x, y, vx, vy; uint8_t kind, t, life, pad; } fx;
#define MAX_FX        40
#define DIRTY_N       16

/* the computer's turn, step by step (search, then aim like a player) */
typedef struct cpu_t {
    int8_t stage, weapon, facing, nfacing;
    int8_t fdir[2];
    int16_t ai, pj;             /* the search's position */
    int16_t best_ang, best_j, best_face;
    int32_t best_val;
    int16_t tgt_ang, tgt_power;
    int16_t wait, act_t;
    int32_t wind_seen;          /* Q16 px/frame^2 */
} cpu_t;

/* one frame of input for the side whose turn it is */
typedef struct tin {
    int8_t walk;                /* -1, 0, 1: held */
    int8_t aim;                 /* +1 raises, -1 lowers: held */
    uint8_t a_down;             /* A held (charge; released: throw) */
    uint8_t b_press;            /* switch the throwable */
    /* touch: the slingshot (sling: being pulled, the aim follows; fire: released) and a shuffle tap */
    uint8_t sling, fire;
    int8_t sl_facing, shuffle;
    int16_t sl_angle, sl_power;
} tin;

enum { PH_DEMO, PH_AIM, PH_FLIGHT, PH_SETTLE, PH_TIMEUP, PH_END };
enum { MSG_NONE, MSG_SPLASH, MSG_SNOWED, MSG_DOUBLE, MSG_LONG, MSG_TIMEUP, MSG_FROST, MSG_WON, MSG_DRAW };
typedef struct world {
    uint8_t ter[WORLD_H][WORLD_W / 8];  /* the terrain: bit (x & 7) of byte x / 8 = solid snow (or rock) */
    uint32_t gen;               /* bumped when the battlefield is made anew (draw.c redraws it all) */
    uint32_t dirty_seq;         /* carved rectangles (draw.c redraws those cells) */
    rect16 dirty[DIRTY_N];
    rect16 fort[2 * MAX_SIDES];
    int nfort;
    int16_t pond_x[2];          /* the ponds' centres (0: none) */
    tree trees[TREES_MIN + TREES_RND];
    int ntrees;
    critter c[MAX_CRITTERS];
    int ncrit;
    side s[MAX_SIDES];
    int nsides, players;        /* sides on the field; players (1: the 1-player matches against the computer) */
    int level, wins;            /* 1 player: the match number (the computer's level), matches won */
    /* the turn */
    int phase, t, turns;
    int cur_side, cur;          /* whose turn: the side, its critter */
    int timer, wind;
    int angle, power, charging, weapon, thrown;
    int a_prev, aim_t, walk_t, walk_dir;
    int32_t walk_acc;
    int moved, shuffle;         /* px walked this turn; px a touch shuffle still walks (signed) */
    int sling_on;               /* the touch slingshot is pulled (draw: the band) */
    proj p;
    cpu_t cpu;
    int last_x;                 /* the last splat's x (the camera) */
    int winner, over;           /* PH_END: the winning side (-1 a draw); over: the run (1 player) or match is over */
    int frost;                  /* the cold bites (FROST_TURN) */
    int hits, enemy_hits;       /* this throw */
    /* presentation, deterministic */
    int msg, msg_t, banner_t;
    fx fx[MAX_FX];
    rs_rng rng, fxrng;
    uint32_t seed;
    int t_all, started;
    int events;                 /* EV_* of the last step */
    int ev_x;                   /* where the last event happened (the sounds' pan) */
} world;
enum { EV_THROW = 1, EV_SPLAT = 2, EV_HIT = 4, EV_SPLASH = 8, EV_SNOWED = 16, EV_BOUNCE = 32, EV_TURN = 64,
       EV_TIMEUP = 128, EV_WON = 256, EV_LOST = 512, EV_SWITCH = 1024, EV_TICK = 2048, EV_LAND = 4096,
       EV_STEP = 8192, EV_MATCH = 16384, EV_PLOP = 32768, EV_AIM = 65536, EV_TREE = 131072 };

/* the battlefield and the match */
void world_init(world *w, int players, uint32_t seed);   /* the title's field (PH_DEMO) */
void world_start(world *w);                              /* the run starts: match 1 (1 player) or the match */
void world_step(world *w, const tin *in);                /* in: the input of the side whose turn it is */
int  world_over(const world *w);
int  world_human_turn(const world *w);                   /* the player whose turn it is (-1: the computer, none) */
int  side_alive(const world *w, int s);                  /* its critters still in */
int  side_hearts(const world *w, int s);
int  side_rank_key(const world *w, int s);               /* 2-4 players: survival, then the score */
critter *world_active(world *w);
const critter *world_active_c(const world *w);
int  species_of_player(int p);
/* terrain */
int  terrain_solid(const world *w, int x, int y);
void terrain_set(world *w, int x, int y, int on);
void terrain_carve(world *w, int cx, int cy, int r);
int  in_pond(const world *w, int x);
int  surface_y(const world *w, int x);                    /* the first solid row from the top (WORLD_H: none) */
int  fort_at(const world *w, int x, int y);
/* throws */
typedef struct impact { int end, x, y, crit, t; } impact;
enum { PE_NONE, PE_TERRAIN, PE_CRITTER, PE_WATER, PE_GONE };
void proj_launch(const world *w, const critter *c, int kind, int facing, int angle, int power, proj *p);
int  proj_frame(const world *w, proj *p, int32_t wind_acc, int *crit, int *bounced);
void proj_sim(const world *w, const critter *c, int kind, int facing, int angle, int power, int32_t wind_acc,
              impact *out);
int32_t wind_accel(int wind, int kind);
void splat(world *w, int kind, int x, int y);
int  splat_damage(int kind, int dist);                   /* half-hearts at dist px from the splat's centre */
const int8_t *pk_info(int kind);                          /* PK_TABLE row */
/* the computer: one frame of its input (the 1-player computer side, and the bots of the tests: level 0 = no error) */
void cpu_think(world *w, int level, tin *out);
int  cpu_shot_value(const world *w, int side, int kind, const impact *im);
/* touch: the slingshot's aim from a pull (dx, dy: from the finger to the critter, screen px; y down) */
void sling_aim(int dx, int dy, int *facing, int *angle, int *power);
/* trigonometry (degrees, Q16) */
int32_t isin(int deg);
int32_t icos(int deg);
int32_t isqrt64(int64_t v);

/* ---- the camera (play.c): presentation only; the screen's width never changes a shot ------------------------------- */
typedef struct camera {
    int32_t x;                  /* the world x at the screen's centre */
    int32_t pan;                /* a manual offset (L/R, a finger dragged on the field) */
    int8_t manual, pad[3];
} camera;
void camera_init(camera *c, const world *w, int screen_w);
void camera_step(camera *c, const world *w, int screen_w, int pan_dx, int t);
int  camera_left(const camera *c, int screen_w);         /* the world x at the screen's left edge */

/* ---- draw.c -------------------------------------------------------------------------------------------------------- */
enum { ST_TITLE, ST_PLAY, ST_DEAD, ST_OVER };      /* no "get ready": the title is the only menu */
typedef struct touch_view { int8_t seen, btn_on, sling, pad; int16_t fx, fy; } touch_view;
void draw_init(void);
void draw_frame(const world *w, const camera *cam, int state, int st_t, int best, int new_best, int paused,
                const touch_view *tv);
void draw_state(void);
void draw_state_loaded(void);       /* widescreen: after a state saved at another width */
void draw_new_world(void);          /* main.c made a new battlefield: draw it all again */
void touch_button_pos(int *x, int *y);   /* the switch button's centre, screen px */

/* ---- sound (main.c) ------------------------------------------------------------------------------------------------ */
enum { SFX_THROW, SFX_SPLAT, SFX_BIG, SFX_HIT, SFX_SPLASH, SFX_SNOWED, SFX_BOUNCE, SFX_STEP, SFX_TICK, SFX_SWITCH,
       SFX_WON, SFX_TREE, SFX_COUNT };
#define SFX_HOUSE SFX_COUNT                                    /* first slot of the house sounds (ha_init) */

#endif
