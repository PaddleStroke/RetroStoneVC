/*
 * Seal Shove: shared declarations.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/sealshove/LICENSE.
 *
 * play.c is the game rules and the bots' brains (no drawing, no sound, no screen width: deterministic, testable
 * alone); draw.c is the video (the floe painted pixel by pixel into its own tiles, the sea, the seals), main.c the flow
 * (the title with the players joining, the matches, the game over), input (pads and touch), save RAM, options and
 * save states. Made from games/_template (tools/new_game.py): the house style kit is games/common.
 */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "rs.h"

#define MAX_PLAYERS 4                   /* the house kit's HU_MAX_PLAYERS: 4 pads */
#define SEALS       4                   /* always 4 seals on the floe: the players first, bots fill the rest */

/* ---- tuning: every gameplay number is here ------------------------------------------------------------------- */
/* fixed point: positions and speeds in 1/4096 px (Q12); stage coordinates (the 320 x 240 stage, centred) */
#define FX            4096
/* the floe: a round ice floe of cells, broken into chunks (3 rings of sectors around a core) */
#define FLOE_CX       160       /* its centre, stage px */
#define FLOE_CY       136
#define CELL          4         /* px: the rules' grid (the edge is the grid's field, smoothed between cell centres) */
#define GRID          52        /* cells a side: 208 px */
#define GRID_X0       (FLOE_CX - GRID * CELL / 2)
#define GRID_Y0       (FLOE_CY - GRID * CELL / 2)
#define FLOE_R        94        /* px: the rim (+- FLOE_JITTER: every round's floe has its own wobbly outline) */
#define FLOE_JITTER   4
#define RING1_R       72        /* the rings' inner edges (+- RING_JITTER): ring 0 94..72, ring 1 72..50, ring 2 50..30 */
#define RING2_R       50
#define CORE_R        30        /* the core breaks last */
#define RING_JITTER   3
#define RING_SECTORS  {12, 9, 6}
#define RING_WEIGHTS  {6, 3, 1} /* the odds of the next crack (outer chunks first) */
#define MAX_CHUNKS    28        /* 12 + 9 + 6 + the core */
/* the cracks: a chunk on the edge cracks (the warning: a dark crack, it blinks), then breaks off and drifts away */
#define FIRST_CRACK   150       /* frames after GO! */
#define CRACK_GAP0    130       /* frames between two cracks at first ... */
#define CRACK_GAP_STEP 5        /* ... 5 fewer after each ... */
#define CRACK_GAP_MIN 60        /* ... down to 1 s */
#define CRACK_WARN    100       /* frames from the crack to the break */
#define CORE_DELAY    240       /* the core cracks 4 s after the last chunk around it went */
#define HURRY_GAP     30        /* every human out (bots fighting on): the floe crumbles faster */
#define HURRY_WARN    50
/* the seals */
#define SEAL_R        10        /* px: the body's radius (collisions) */
#define HEAVY_R       12        /* with the big belly */
#define SEAL_MASS     256
#define WADDLE_ACC    (FX * 60 / 1000)  /* px a frame a frame (0.06): top speed WADDLE_ACC / MOVE_FRICTION */
#define MOVE_FRICTION 11        /* /256 of the speed a frame with the d-pad held: 1.4 px a frame at most */
#define IDLE_FRICTION 16        /* let go (or knocked): it slides to a stop in about 22 px from top speed */
#define KNOCK_FRAMES  30        /* frames a bumped seal slides (KNOCK_FRICTION even with the d-pad held) */
#define KNOCK_FRICTION 18       /* /256: a shoved seal slides about 13 px per px a frame it was given */
#define DASH_V        (FX * 4)  /* the belly slide (A): 4 px a frame ... */
#define DASH_FRAMES   14        /* ... for 14 frames ... */
#define DASH_FRICTION 4         /* ... nearly frictionless (about 50 px) ... */
#define BRAKE_FRAMES  14        /* ... then the flippers brake (about 25 px more) */
#define BRAKE_FRICTION 26
#define DASH_COOLDOWN 45        /* frames after a slide before the next */
#define DASH_MASS_PCT 150       /* a sliding seal hits as 1.5 x its mass */
#define BUMP_E        110       /* % restitution of a bump (a little bouncier than elastic) */
#define BUMP_MIN      (FX * 8 / 10) /* px a frame: two seals that touch always part at least this fast */
#define CLASH_V       (FX * 26 / 10)/* two slides head-on: both bounce back at 2.6 px a frame ... */
#define STUN_FRAMES   24        /* ... and are dizzy for 0.4 s */
#define FALL_FRICTION 40        /* a falling seal's slide stops fast */
#define FALL_SPLASH   10        /* frames from the edge to the splash */
#define FALL_FRAMES   80        /* then its head pops up and it swims off: out of the round */
#define KO_CREDIT     150       /* frames: a seal that falls within 2.5 s of a push is the pusher's knock-out */
#define DOUBLE_WINDOW 90        /* two knock-outs this close: DOUBLE KNOCK-OUT! */
/* the fish: now and then one flops on the floe; the seal that grabs it gets a power */
#define FISH_FIRST    420       /* frames after GO! */
#define FISH_GAP      540       /* then every 9 s + up to 6 s */
#define FISH_GAP_RND  360
#define FISH_LIFE     480       /* it hops back into the sea after 8 s */
#define FISH_R        6         /* px: grabbed within SEAL_R + FISH_R */
#define FISH_SAFE     14        /* it lands with this much ice around it ... */
#define FISH_FAR      34        /* ... and this far from every seal */
#define HEAVY_MASS_PCT 250      /* the blue fish: BIG BELLY, 2.5 x the mass ... */
#define HEAVY_FRAMES  420       /* ... for 7 s */
#define MEGA_SPEED_PCT 130      /* the golden fish: the next slide is a MEGA SLIDE: 1.3 x as fast, */
#define MEGA_MASS_PCT 250       /* hits as 2.5 x its mass ... */
#define MEGA_FRAMES   600       /* ... within 10 s */
/* rounds and matches */
#define START_R       46        /* px: the seals start on a circle round the centre */
#define INTRO_FRAMES  90        /* ROUND n, then GO! */
#define GO_SHOW       40
#define END_FRAMES    130       /* the round's winner shown */
#define MATCH_FRAMES  170       /* the match's winner shown */
#define WIN_ROUNDS    3         /* a match: the first to 3 rounds */
/* score (1 player; the results of 2-4 players rank by rounds won) */
#define PTS_KO        10        /* a seal you pushed off */
#define PTS_ROUND     25
#define PTS_MATCH     100
#define PTS_DOUBLE    50        /* two knock-outs within 1.5 s */
#define PTS_SWEEP     50        /* you knocked all three off in one round */
#define MEDAL_SCORES  {100, 400, 750, 1200}     /* bronze, silver, gold, pearl */
#define MSG_FRAMES    100       /* DOUBLE KNOCK-OUT!, CLEAN SWEEP!, BIG BELLY!, MEGA SLIDE! */
/* the bots (play.c bot_think): smart but fair. Per level: react, react_rnd (frames between decisions), aim (1/256 of
 * a turn of wobble), dodge (% of the slides coming at it dodged), dodge_dash (% of those with a slide), margin (px of
 * ice it keeps between its slide's end and the water), dash (% chance to slide when lined up behind a target),
 * mistake (% of decisions that forget the edge), fish (% of decisions that go for a fish), warn (% that see the
 * cracks) */
#define BOT_LEVEL_TABLE { \
    /* 0 */ {24, 14, 20,  8,  0, 14, 22, 16, 30, 30}, \
    /* 1 */ {19, 12, 15, 18, 10, 17, 32, 11, 45, 50}, \
    /* 2 */ {15, 10, 11, 28, 20, 19, 42,  8, 60, 65}, \
    /* 3 */ {12,  8,  8, 40, 30, 21, 52,  5, 70, 80}, \
    /* 4 */ { 9,  6,  6, 50, 40, 23, 62,  3, 80, 90}, \
    /* 5 */ { 7,  5,  4, 60, 50, 25, 72,  2, 90, 100}}
#define BOT_LEVELS    6         /* 1 player: match n's bots play at level n - 1 (then 5) */
#define BOT_LEVEL_MULTI 1       /* 2-4 players: the bots that fill the floe */
#define BOT_PLAYER    {6, 4, 3, 65, 50, 24, 80, 1, 90, 100}   /* the player bot (bot=1); botskill=E makes it human */
#define BOT_LOOK      14        /* frames of its own slide a bot looks ahead */
#define BOT_DASH_RANGE 70       /* px: it slides at a target this close ... */
#define BOT_ALIGN     205       /* ... lined up behind it (cos >= 0.8, /256) */
#define EDGE_SCAN     60        /* px: how far a bot looks for the water */
#define EXPOSE_W      2         /* a target near the water counts as this much nearer per px */
#define CHASERS_MAX   2         /* at most 2 seals go for the same one */
#define DODGE_T       24        /* frames: a slide arriving sooner is dodged (or not) */
#define FISH_SEEK     90        /* px: a bot goes for a fish this close */
/* the flow */
#define PANEL_DELAY   60        /* frames between the lost match and the game-over panel */
#define RETRY_LOCK    36        /* frames the panel ignores the button (no accidental retry) */
#define BOT_START     20        /* frames on the title before the bot starts the run */
#define HINT_FRAMES   360       /* A: BELLY SLIDE at the start of a run */
/* touch (player 1; play.c touch_*): a stick on the left part of the screen, the DASH button at the bottom right */
#define STICK_ZONE_PCT 55       /* the stick: a finger going down on the left 55% of the screen */
#define STICK_R       26        /* px of drag for full speed */
#define STICK_DEAD    3         /* px: no movement below this */
#define BTN_R         22        /* the DASH button: radius, centre from the screen's bottom-right corner */
#define BTN_DX        36
#define BTN_DY        38
#define BTN_SLOP      8         /* px around it that still press it */

/* a phone held upright (DESIGN.md "Tall screen"): the zoomed screen, ZOOM_H lines (rs_screen_zoom; 222 columns shown
 * on a 1080x2520 phone, 240 on a 1080x2340 one: the floe and its rim's splashes), the stage TALL_UP lines from the
 * screen's top (the HUD above the floe, the DASH button below it); landscape and 4:3: 240 lines, unchanged */
#define ZOOM_H        520
#define TALL_UP       130
static inline int tall_screen(void) { return rs_screen_height() > 240; }
/* the stage x of the columns the display shows (every column without a zoom: rs_screen_left() .. right()) */
static inline int vis_left(void) { return rs_screen_visible_x0() - rs_screen_origin_x(); }
static inline int vis_right(void) { return rs_screen_visible_x1() - rs_screen_origin_x(); }

/* ---- play.c: the rules ------------------------------------------------------------------------------------------ */
enum { CH_ICE, CH_WARN, CH_GONE };
typedef struct chunk {
    uint8_t state, ring, edge, rev;     /* rev: +1 at each change (draw.c repaints) */
    int16_t t, warn;                    /* frames in its state; the warning's length */
    int16_t cx, cy;                     /* its centre, stage px */
    int16_t x0, y0, x1, y1;             /* its cells' box, stage px (x1, y1 past the end) */
    int16_t ncells, pad_;
    uint32_t adj;                       /* the chunks next to it (bits) */
} chunk;

enum { SS_PLAY, SS_DASH, SS_FALL, SS_OUT };
enum { PW_NONE, PW_HEAVY, PW_MEGA };
enum { BM_IDLE, BM_SAFE, BM_DODGE, BM_FISH, BM_DASH, BM_PUSH, BM_STALK };
typedef struct seal {
    int32_t x, y, vx, vy;               /* Q12 */
    uint8_t state, face, human, power;  /* face 0..7: east, south-east, south... (clockwise, y down) */
    int16_t t, dash_cd, stun, knock;    /* frames in its state; before the next slide; dizzy; sliding from a bump */
    int16_t brake, power_t, mega, walk; /* braking after a slide; the power's frames left; a mega slide; waddle */
    int16_t hit_by, hit_t;              /* the last seal that pushed it, frames since */
    int16_t kos, round_kos;             /* knock-outs (this match), this round */
    int16_t wins, matches;              /* rounds won in this match; matches won */
    int16_t bump_t, ready_t;            /* frames since a bump; since the slide came back */
    int32_t last_ko_t;                  /* when its last knock-out was (world t) */
    int32_t score;
    int16_t think_t, target, hx, hy;    /* the bot's brain: next decision, target seal, heading (1/256) */
    uint8_t want_dash, mode, pad_[2];
} seal;

typedef struct fishy { int16_t x, y, t, next; uint8_t on, kind, pad_[2]; } fishy;   /* kind: PW_HEAVY, PW_MEGA */

enum { PH_INTRO, PH_FIGHT, PH_END, PH_MATCH };
enum { MSG_NONE, MSG_DOUBLE, MSG_SWEEP, MSG_HEAVY, MSG_MEGA };
typedef struct world {
    seal s[SEALS];
    uint8_t cell[GRID * GRID];          /* the chunk of each cell + 1 (0: water), fixed for a round */
    chunk ch[MAX_CHUNKS];
    int nchunks, core;
    uint32_t gone;                      /* bits: the chunks gone */
    int16_t safe_x, safe_y;             /* the centre of the ice that is not cracking (px) */
    int floe_gen;                       /* +1 at each new floe (draw.c repaints it all) */
    int crack_t, crack_gap, last_break_t, hurry;
    fishy fish;
    rs_rng rng;
    uint32_t seed;
    int humans, level, match_no, round, phase, phase_t;
    int round_winner, match_winner, match_over;
    int t;
    int events[SEALS];                  /* EV_* of the last step, per seal */
    int gev;                            /* GEV_* of the last step */
    int msg, msg_seal, msg_t;           /* the latest message, whose, frames left */
    int bumps, dashes, clashes, breaks, falls, fishes;     /* counts (tests) */
} world;
enum { EV_DASH = 1, EV_BUMP = 2, EV_SMACK = 4, EV_CLASH = 8, EV_FALL = 16, EV_SPLASH = 32, EV_KO = 64,
       EV_DOUBLE = 128, EV_POWER = 256, EV_WIN_ROUND = 512, EV_WIN_MATCH = 1024, EV_READY = 2048 };
enum { GEV_GO = 1, GEV_CRACK = 2, GEV_BREAK = 4, GEV_FISH = 8, GEV_ROUND_END = 16, GEV_MATCH_END = 32,
       GEV_INTRO = 64, GEV_SWEEP = 128, GEV_FISH_GONE = 256 };

typedef struct pin { int16_t dx, dy; uint8_t dash, any, pad_[2]; } pin;   /* a seal's input this frame (1/256) */
typedef struct bot_par { int react, react_rnd, aim, dodge, dodge_dash, margin, dash, mistake, fish, warn; } bot_par;

void world_init(world *w, int humans, uint32_t seed, int level);     /* a match: round 1, its intro */
void world_next_match(world *w);                                    /* 1 player: the next match, the bots smarter */
void world_step(world *w, const pin in[SEALS]);                     /* the humans' inputs; the bots think themselves */
void world_new_round(world *w);                                     /* (tests) a new floe, the seals in place */
int  floe_field(const world *w, int xq, int yq, int warn_water);   /* the ice at (xq, yq) in 1/16 px: 0..256 */
int  on_ice(const world *w, int32_t x, int32_t y);                 /* Q12: the field >= 128 */
int  chunk_at(const world *w, int px, int py);                      /* the chunk under a stage pixel (-1: water) */
int  seal_standing(const seal *s);
int  seal_mass(const seal *s);
int  seal_radius(const seal *s);
int  seals_standing(const world *w);
int  edge_scan(const world *w, int px, int py, int warn_water, int *angle);   /* px to the nearest water (<= EDGE_SCAN) */
void bot_think(world *w, int s, const bot_par *bp, pin *out);       /* the brain (bots, and player 1 with bot=1) */
void bot_give_up(world *w, int s, pin *out);                        /* the tests' bot that walks off the ice */
void bot_par_level(int level, bot_par *out);
void bot_par_player(int skill, bot_par *out);                       /* skill 0: the player bot; 1..10 more human */
/* integer maths: angles in 1/256 of a turn (0 = east, 64 = south: y down), sines in Q12 */
int  isqrt64(int64_t v);
int  iatan2(int dy, int dx);
int  isin(int a);
int  icos(int a);
int  dir8(int dx, int dy);
/* touch, player 1 (main.c reads the fingers; the geometry is here, testable): stage coordinates */
typedef struct tbutton { int x, y, r; } tbutton;
void touch_button(int right, tbutton *b);                           /* the DASH button: right = the screen's right edge */
int  touch_on_button(const tbutton *b, int x, int y);
int  touch_stick_zone(int left, int width, int x, int y);
void touch_stick(int x0, int y0, int x, int y, int16_t *dx, int16_t *dy);
extern const char *const bot_names[SEALS];

/* ---- draw.c -------------------------------------------------------------------------------------------------------- */
enum { ST_TITLE, ST_PLAY, ST_DEAD, ST_OVER };      /* no "get ready": the title is the only menu */
typedef struct touch_view {                         /* what main.c's touch reading shows (the stick, the button) */
    uint8_t seen, stick_on, dash_on, pad_;
    int16_t sx0, sy0, sx, sy;                       /* the stick: where it went down, the finger (screen px) */
} touch_view;
void draw_init(void);
void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused, const touch_view *tv);
void draw_state(void);
void draw_state_loaded(void);       /* widescreen: after a state saved at another width */
void draw_new_run(void);

/* ---- sound (main.c) ------------------------------------------------------------------------------------------------ */
enum { SFX_DASH, SFX_BUMP, SFX_SMACK, SFX_CLASH, SFX_CRACK, SFX_BREAK, SFX_SPLASH, SFX_FISH, SFX_POWER, SFX_GO,
       SFX_COUNT };
#define SFX_HOUSE SFX_COUNT                                    /* first slot of the house sounds (ha_init) */

#endif
