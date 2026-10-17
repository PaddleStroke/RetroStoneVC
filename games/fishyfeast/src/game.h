/*
 * Fishy Feast: shared declarations.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/fishyfeast/LICENSE.
 *
 * play.c is the game rules and the bots' brains (no drawing, no sound, no screen width: deterministic, testable
 * alone); draw.c is the video (the cameras, the zoom, the split screen), main.c the flow (the title with the players
 * joining, play, game over), input (pads and touch), save RAM, options and save states.
 * Made from games/_template (tools/new_game.py): the house style kit is games/common (docs/art-direction.md).
 */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "rs.h"

#define MAX_PLAYERS 4                   /* the house kit's HU_MAX_PLAYERS: 4 pads */
#define BOTS        15                  /* the bot fish, always (with 1 to 4 players) */
#define OWNERS      (MAX_PLAYERS + BOTS)/* players 0..3, then the bots */

/* ---- tuning: every gameplay number is here ------------------------------------------------------------------- */
/* fixed point: positions and radii in 1/256 px (Q8), masses in 1/256 of a mass unit */
#define FX            256
#define MQ            256
/* the pond: a rounded rectangle, world px */
#define POND_W        1920
#define POND_H        1440
#define POND_CORNER   240       /* the corners' radius */
/* sizes and speeds */
#define START_MASS    10        /* a new fish (players, bots respawn at START_MASS .. +BOT_RESPAWN_RND) */
#define RADIUS_K      1239      /* radius = sqrt(mass * RADIUS_K) in Q8: 2.2 * sqrt(mass) px (10: 7 px, 100: 22) */
#define SPEED_NUM     64        /* full speed = SPEED_NUM / (radius + SPEED_OFS) px a frame: 1.73 at 10, 1.23 at */
#define SPEED_OFS     30        /* 100, 0.81 at 500 */
#define SPEED_MIN     128       /* Q8: 0.5 px a frame at least */
#define ACCEL_DIV     6         /* the velocity goes 1/6 of the way to the target each frame (a little glide) */
#define MASS_CAP      5000      /* nobody grows past this (a safety; the decay stops growth much earlier) */
/* eating */
#define EAT_NUM       5         /* a fish eats another one EAT_NUM / EAT_DEN as heavy or more: 25% bigger */
#define EAT_DEN       4
#define EAT_COVER     40        /* ... when it covers it: the distance <= its radius - 40% of the prey's */
#define PLANKTON_N    800       /* plankton specks in the pond (1 mass each) */
#define PLANKTON_REGROW 360     /* frames before an eaten speck grows again elsewhere, + PLANKTON_REGROW_RND */
#define PLANKTON_REGROW_RND 600
/* split (A): every piece of mass SPLIT_MIN or more splits in two, the front half shoots ahead */
#define SPLIT_MIN     36        /* each half >= 18 */
#define SPLIT_MAX     16        /* pieces per fish at most */
#define SPLIT_V0      6         /* the shot half's start speed: SPLIT_V0 + radius / SPLIT_V_DIV px a frame */
#define SPLIT_V_DIV   12
#define BOOST_KEEP    224       /* a boost keeps 224/256 a frame: it carries about 8x its start speed */
#define MERGE_BASE    900       /* frames before the pieces may merge again (15 s) ... */
#define MERGE_PER_MASS 2        /* ... + 2 frames per mass unit of the piece ... */
#define MERGE_EXTRA_MAX 600     /* ... + 10 s at most (25 s for a big piece) */
#define COHESION_DIV  40        /* a fish's pieces drift towards their centre: 1/40 of the gap a frame ... */
#define COHESION_MAX  96        /* ... at most 3/8 px a frame */
/* eject (B): every piece of mass EJECT_MIN or more spits a pellet forward */
#define EJECT_MIN     32
#define EJECT_COST    10        /* the piece loses 10 ... */
#define EJECT_MASS    8         /* ... the pellet holds 8 (anyone eats it) */
#define EJECT_V0      9         /* px a frame at the start (the boost: about 70 px) */
#define EJECT_OWN_DELAY 40      /* frames before the ejecting fish may eat its own pellet */
#define MAX_PELLETS   96
#define PELLET_LIFE   (60 * 60) /* a pellet left alone dissolves after a minute */
/* sea urchins: fixed; a fish heavier than URCHIN_MASS that touches one bursts */
#define URCHINS_N     14
#define URCHIN_MASS   120       /* the urchin's size (radius 24 px) */
#define URCHIN_TOUCH  50        /* touching: the distance < the fish's radius + 50% of the urchin's */
#define BURST_PIECES  8         /* a burst fish breaks into up to 8 pieces (16 in all at most) */
#define BURST_V0      5         /* px a frame, outwards */
/* the decay: big fish slowly lose mass */
#define DECAY_FROM    150       /* above this total mass ... */
#define DECAY_PPM     42        /* ... every piece loses 42 millionths of its mass a frame (0.25% a second) */
#define BOT_CAP       450       /* a bot heavier than this ... */
#define BOT_CAP_PPM   500       /* ... loses 500 millionths a frame more (3% a second): the bots stay beatable */
/* players and bots */
#define SPAWN_SAFE    260       /* px: a fish is born away from the bigger ones (best of SPAWN_TRIES spots) */
#define SPAWN_TRIES   16
#define BOT_RESPAWN   150       /* frames before an eaten bot comes back, + BOT_RESPAWN_RND */
#define BOT_RESPAWN_RND 150
#define BOT_START_MASSES {12, 14, 16, 18, 20, 24, 28, 34, 40, 48, 58, 70, 84, 100, 130}
#define BOT_RESPAWN_MASS 6      /* a bot comes back at START_MASS + 0..BOT_RESPAWN_MASS-1 */
/* the bots' brains (play.c bot_think): fair play */
#define PLAYER_GRACE  900       /* frames: no bot hunts a player in the first 15 s */
#define NEWBIE_MASS   40        /* a player lighter than this is only hunted when it swims right by (NEWBIE_NEAR) */
#define NEWBIE_NEAR   48
#define CHASERS_SMALL 1         /* bots hunting the same player at once: 1 while it is under CHASERS_MASS, */
#define CHASERS_BIG   2         /* then 2 */
#define CHASERS_MASS  150
#define BOT_VIEW_PX   18        /* a bot sees what a player of its size sees (draw.c's full view, ZOOM_TARGET_FULL) */
#define BOT_VIEW_HW   170       /* the view's half size at zoom 1: about a 320 x 240 screen */
#define BOT_VIEW_HH   130
#define FLEE_PAD      24        /* px added to a threat's flee range (edge to edge) */
#define FLEE_MIN      40        /* /256: the fear that makes a bot flee */
#define BOT_SPLIT_CD  360       /* frames between two split attacks of a bot */
#define BOT_SPLIT_PIECES 2      /* a bot splits to attack only while in 2 pieces or fewer (the player bot: 4) */
#define IGNORE_FRAMES 240       /* a prey given up is left alone this long */
#define AIM_NOISE     6         /* 1/256 turn: the bots' aim wobbles by up to this (+2 per skill step) */
/* the bot kinds: react_min, react_rnd (frames between decisions), flee (% of the threat's radius, edge to edge),
 * chase_max (frames on one prey), split (% chance at a prey in reach), forget (% of decisions that ignore the
 * urchins), misjudge (% error on sizes), prey, food (weights %), pieces (% more for split pieces), lead (frames
 * of aim ahead) */
#define BOT_KIND_TABLE { \
    /* grazer      */ {16, 12, 200, 110,  0,  6, 15,  45, 140, 100,  4}, \
    /* timid       */ {12, 10, 380,  80,  0,  4, 12,  35, 120, 100,  4}, \
    /* hunter      */ {14, 10, 160, 330, 30, 22, 18, 150,  70, 120,  8}, \
    /* opportunist */ {12,  8, 210, 200, 25, 12, 15, 110,  90, 260,  6}, \
    /* player bot  */ { 6,  4, 260, 240, 45,  0,  0, 130, 100, 150, 10}}
enum { BK_GRAZER, BK_TIMID, BK_HUNTER, BK_OPPORTUNIST, BK_PLAYER, BK_COUNT };
#define BOT_KINDS_OF {BK_GRAZER, BK_TIMID, BK_HUNTER, BK_OPPORTUNIST, BK_GRAZER, BK_HUNTER, BK_TIMID, \
                      BK_OPPORTUNIST, BK_GRAZER, BK_TIMID, BK_HUNTER, BK_OPPORTUNIST, BK_GRAZER, BK_TIMID, BK_HUNTER}
/* the zoom (draw.c; play.c for the bots' view): steps of 1/256, the target radius of the player's fish on screen */
#define ZOOM_STEPS    10
#define ZOOM_STEP_TABLE {256, 216, 182, 154, 130, 110, 92, 78, 66, 56}
#define ZOOM_TARGET_FULL 18     /* px: 1 player (the full screen) */
#define ZOOM_TARGET_HALF 16     /* 2 players (halves) */
#define ZOOM_TARGET_QUAD 13     /* 3-4 players (quarters; they also start one step out) */
#define ZOOM_BIG_MAX  40        /* px: the biggest fish drawn (draw.c zooms out so that any fish in view fits) */
#define ZOOM_EASE     12        /* the camera's zoom goes 1/12 of the way a frame (a step: about a second) */
/* the flow */
#define PANEL_DELAY   90        /* frames between the last fish eaten and the game-over panel */
#define RETRY_LOCK    36        /* frames the panel ignores the button (no accidental retry) */
#define BOT_START     20        /* frames on the title before the bot starts the run */
#define HINT_FRAMES   300       /* the A: SPLIT - B: EJECT hint at the start */
#define TOP_MIN_MASS  60        /* BIGGEST FISH! needs at least this mass */
#define MEDAL_SCORES  {80, 250, 600, 1200}      /* bronze, silver, gold, pearl: the biggest mass reached */
/* touch (player 1; play.c touch_*): a stick anywhere on the left part, two buttons at the bottom right */
#define STICK_ZONE_PCT 60       /* the stick: a finger going down on the left 60% of player 1's view */
#define STICK_R       28        /* px of drag for full speed */
#define STICK_DEAD    3         /* px: no movement below this */
#define BTN_SPLIT_R   20        /* the SPLIT button: radius, centre from the view's bottom-right corner */
#define BTN_SPLIT_DX  30
#define BTN_SPLIT_DY  34
#define BTN_EJECT_R   16        /* the EJECT button, left of it and a little lower */
#define BTN_EJECT_DX  78
#define BTN_EJECT_DY  24
#define BTN_SLOP      6         /* px around a button that still press it */

/* ---- play.c: the rules ------------------------------------------------------------------------------------------ */
typedef struct cell {                   /* one piece of a fish */
    int32_t x, y, vx, vy, bx, by;       /* Q8: position, velocity, boost (a split or a burst; decays) */
    int32_t mass, r;                    /* MQ, Q8 */
    int16_t owner, merge_t;             /* the fish it belongs to; frames before it may merge */
    uint8_t on, face;                   /* face 1: looks left */
    uint8_t immune, pad_;               /* frames the urchins leave it alone (the pieces of a burst) */
    uint16_t age;                       /* frames since it was made */
} cell;
#define MAX_CELLS 200

typedef struct pellet {                 /* ejected mass */
    int32_t x, y, bx, by;
    int16_t owner, t;                   /* who spat it, frames since */
    uint8_t on, colour;
    uint16_t life;
} pellet;

typedef struct speck { int16_t x, y; uint16_t regrow; uint8_t on, kind; } speck;
typedef struct urchin { int16_t x, y; uint16_t shake; } urchin;

enum { OW_PLAYER, OW_BOT };
enum { BM_GRAZE, BM_FLEE, BM_CHASE, BM_WANDER };
typedef struct owner {                  /* a fish: a player or a bot, in 1..SPLIT_MAX pieces */
    uint8_t kind, alive, out, pers;     /* out: a player eaten (one life) */
    uint8_t name, colour;               /* bots: the name and the colour (0..7); players: colour 8 + p */
    int16_t ncells, eaten_by;           /* pieces now; who ate its last piece (-1) */
    int32_t mass;                       /* MQ: the total of its pieces */
    int32_t best;                       /* the biggest total mass reached (mass units): the score */
    int32_t cx, cy;                     /* Q8: the centre of its pieces (mass-weighted) */
    int16_t dirx, diry;                 /* the steering, 1/256 (length <= 256) */
    int16_t respawn_t;                  /* bots: frames before coming back */
    int16_t think_t, mode, target, chase_t, split_cd, ignore, ignore_t;   /* the brain */
    int16_t eats, kills;                /* plankton eaten, fish eaten (whole fish) */
    uint8_t face, last_split, last_eject, pad_;
    int32_t alive_t;
} owner;

typedef struct world {
    owner own[OWNERS];
    cell c[MAX_CELLS];
    pellet pl[MAX_PELLETS];
    speck sp[PLANKTON_N];
    urchin u[URCHINS_N];
    rs_rng rng;
    uint32_t seed;
    int players, t, started;
    int leader;                         /* the heaviest fish now (-1) */
    int events[MAX_PLAYERS];            /* EV_* of the last step */
    int splits, bursts, eats, merges;   /* counts (tests) */
    int16_t msg_top[MAX_PLAYERS];       /* frames left of BIGGEST FISH! */
    int nfx;                            /* the effects of the last step (draw.c: bubbles) */
    struct { int16_t x, y, r; uint8_t kind, pad_; } fx[16];
} world;
enum { FX_EAT, FX_BURST, FX_MERGE };
#define TOP_MSG_FRAMES 150
enum { EV_START = 1, EV_PLANKTON = 2, EV_EAT = 4, EV_SPLIT = 8, EV_EJECT = 16, EV_BURST = 32, EV_LOST = 64,
       EV_OUT = 128, EV_MERGE = 256, EV_TOP = 512, EV_PELLET = 1024 };

typedef struct pin { int16_t dx, dy; uint8_t split, eject, any, pad_; } pin;   /* a player's input this frame */

extern const char *const fish_names[BOTS];
void    world_init(world *w, int players, uint32_t seed);   /* the pond, plankton, urchins, bots; players wait */
void    world_start(world *w);                              /* the players' fish come in */
void    world_step(world *w, const pin in[MAX_PLAYERS]);    /* players' inputs; the bots think for themselves */
int     world_running(const world *w);                     /* a player still swims */
int32_t fish_radius(int32_t mass);                          /* MQ -> Q8 */
int32_t fish_speed(int32_t r);                              /* Q8 radius -> Q8 px a frame */
int     can_eat(int32_t eater, int32_t prey);               /* masses: 25% heavier */
int     owner_mass(const world *w, int o);                  /* in mass units */
int     owner_rank(const world *w, int o);                  /* 1 = the heaviest of the fish alive (0: not alive) */
int     leaderboard(const world *w, int out[], int n);      /* the n heaviest, heaviest first; returns how many */
int     owner_split(world *w, int o, int dx, int dy);       /* every piece big enough splits; returns how many */
int     owner_eject(world *w, int o, int dx, int dy);       /* returns pellets spat */
void    bot_think(world *w, int o, int skill, pin *out);    /* the brain (bots, and player 1 with bot=1) */
int     zoom_step_for(int32_t r, int target_px, int max_zoom);   /* the zoom (1/256) that shows radius r at target */
int     isqrt64(int64_t v);
extern const int16_t zoom_steps[ZOOM_STEPS];
int     pond_span(int y, int *x0, int *x1);                  /* the pond's columns on world row y (0: none) */

/* the views (split screen: draw.c builds its viewports from them; the touch buttons sit in player 1's view) */
typedef struct vrect { int x, y, w, h; } vrect;              /* stage coordinates */
enum { VK_FULL, VK_HALF, VK_QUAD };
int     view_layout(int players, int left, int width, vrect out[4]);   /* returns the rects (3 players: 4, the 4th a map) */
void    views_tall(vrect *v, int n, int top, int bottom);      /* a tall screen: the edge views reach top / bottom */
int     view_kind(int players);
int     view_target_px(int kind);
/* touch, player 1 (main.c reads the fingers; the geometry is here, testable) */
typedef struct tbutton { int x, y, r; } tbutton;
void    touch_buttons(const vrect *v, tbutton *split, tbutton *eject);
int     touch_on_button(const tbutton *b, int x, int y);
int     touch_stick_zone(const vrect *v, int x, int y);
void    touch_stick(int x0, int y0, int x, int y, int16_t *dx, int16_t *dy);

/* ---- draw.c -------------------------------------------------------------------------------------------------------- */
enum { ST_TITLE, ST_PLAY, ST_DEAD, ST_OVER };      /* no "get ready": the title is the only menu */
typedef struct touch_view {                         /* what main.c's touch reading shows (the stick, the buttons) */
    uint8_t seen, stick_on, split_on, eject_on;
    int16_t sx0, sy0, sx, sy;                       /* the stick: where it went down, the finger (screen px) */
} touch_view;
void draw_init(void);
void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused, const touch_view *tv);
void draw_state(void);
void draw_state_loaded(void);       /* widescreen: after a state saved at another width */
/* a phone held upright (DESIGN.md "Tall screen"): a zoomed screen of ZOOM_H lines (rs_screen_zoom), the sides cropped
 * while one fish plays; view_shown cuts a view to the columns the display shows (the camera's fit, the touch buttons) */
#define ZOOM_H 420
void view_shown(vrect *v);
void draw_new_run(const world *w);  /* the cameras jump to the fish */

/* ---- sound (main.c) ------------------------------------------------------------------------------------------------ */
enum { SFX_NOM, SFX_GULP, SFX_SPLIT, SFX_EJECT, SFX_BURST, SFX_EATEN, SFX_MERGE, SFX_COUNT };
#define SFX_HOUSE SFX_COUNT                                    /* first slot of the house sounds (ha_init) */

#endif
