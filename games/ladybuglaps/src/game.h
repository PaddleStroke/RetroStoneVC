/*
 * Ladybug Laps: shared declarations.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/ladybuglaps/LICENSE.
 *
 * play.c is the game rules and the bots' brains (no drawing, no sound, no screen width: deterministic, testable
 * alone); tracks.c the hand-made tracks (also read by tools/make_art.py, which draws them); draw.c is the video (the
 * world streamed into a BG map, the cars, the HUD), main.c the flow (the title with the players joining, the races,
 * the results between them, game over), input (pads and touch), save RAM, options and save states.
 * Made from games/_template (tools/new_game.py): the house style kit is games/common (docs/art-direction.md).
 */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "rs.h"

#define MAX_PLAYERS 4                   /* the house kit's HU_MAX_PLAYERS: 4 pads */
#define RACERS      4                   /* always 4 ladybugs: 1-4 players, bots for the others */

/* ---- tuning: every gameplay number is here ------------------------------------------------------------------- */
/* fixed point: positions and speeds in 1/256 px (Q8); angles in 1/1024 of a turn (0 east, 256 south: y down) */
#define FX            256
#define ANG_FULL      1024
/* the tracks: a grid of 64-px pieces (tracks.c), a band 48 px wide */
#define CELL          64
#define HALF_W        24        /* the band's half width (px) */
#define WP_STEP       16        /* px between two waypoints of the centre line */
#define MAX_WP        320
#define MAX_GW        24        /* the biggest grid, in cells */
#define MAX_GH        16
#define WALL_IN       12        /* walled tracks: the solid frame round the world (px) */
#define LAPS          3
/* the ladybug (a car): its handling, on the track; the surfaces scale it (SURF_TABLE) */
#define CAR_R         6         /* the collision radius (px) */
#define ACCEL         10        /* Q8 px a frame, each frame A is held (the surface's accel % applies) */
#define BRAKE         24        /* B while moving forward */
#define COAST         3         /* no pedal: rolling to a stop */
#define REV_ACCEL     6         /* B held at a standstill: reverse ... */
#define REV_MAX       320       /* ... up to 1.25 px a frame */
#define TURN_RATE     12        /* angle units a frame (a full turn in 85 frames) ... */
#define TURN_STILL    6         /* ... half that at a standstill */
#define TURN_FULL_AT  256       /* the full rate from 1 px a frame */
#define OVERSPEED_DIV 10        /* faster than the surface allows: lose 1/10 of the excess a frame */
/* the surfaces: max speed (Q8), accel %, grip (/256 of the sideways speed lost a frame: low = slippery), drag
 * (/256 of the speed lost a frame) */
enum { SF_TRACK, SF_ROUGH, SF_JAM, SF_WATER, SF_BUMP, SF_FALL, SF_WALL, SF_COUNT };
#define SURF_TABLE { \
    /* track  */ {704, 100,  60,  0}, \
    /* rough  */ {420,  70,  80,  5}, \
    /* jam    */ {230,  45, 160, 12}, \
    /* water  */ {704,  55,   5,  0}, \
    /* bump   */ {704, 100,  60,  0}, \
    /* fall   */ {704, 100,  60,  0}, \
    /* wall   */ {420,  70,  80,  5}}
/* hops: a bump taken at speed throws the ladybug up (it opens its wings); in the air: no grip, no steering */
#define HOP_MIN_SPEED 300       /* Q8: slower, it just rolls over */
#define HOP_VZ        180       /* Q8 px a frame up, + the speed / HOP_VZ_DIV */
#define HOP_VZ_DIV    3
#define HOP_GRAVITY   24        /* Q8 px a frame per frame */
/* bumps */
#define BUMP_E        150       /* /256: the bounce between two ladybugs (restitution) */
#define WALL_E        110       /* /256: the bounce off an object or a wall */
#define BUMP_SFX_MIN  96        /* Q8: a softer touch makes no sound */
/* falling off the table, being left behind */
#define FALL_FRAMES   42        /* the fall (it shrinks away), then it is put back on the track */
#define GHOST_FRAMES  60        /* after being put back: blinking, it goes through the others */
#define DROP_DELAY    30        /* left behind: gone this long, then put back behind the leader */
#define DROP_BACK_WP  3         /* ... this many waypoints behind it (and in its own lane) */
#define DROP_SPEED    85        /* % of the leader's speed it starts with */
#define DROP_GRACE    120       /* frames after the start before anyone can be left behind */
/* the camera follows the leader (the racer furthest along the track), looking ahead; the race zone is a FIXED
 * 320 x 240 box round it (the same at every screen width): a racer whose centre leaves it by more than ZONE_OUT px
 * is left behind */
#define ZONE_HW       160
#define ZONE_HH       120
#define ZONE_OUT      4
#define WARN_PX       24        /* the "!" over a racer this close to the zone's edge */
#define LOOK_FRAMES   22        /* the camera aims at the leader + its speed x 22 frames ... */
#define LOOK_MAX      56        /* ... at most 56 px ahead */
#define CAM_EASE      8         /* it goes 1/8 of the way a frame ... */
#define CAM_MAX_STEP  5         /* ... at most 5 px a frame (a new leader: a smooth pan) */
/* the race: points */
#define DROP_POINTS   1         /* a racer left behind: the leader takes a point ... */
#define DROP_LOSS     1         /* ... and it loses one (never under 0) */
#define FINISH_POINTS {3, 2, 1, 0}   /* and the places at the finish */
#define COUNTDOWN     120       /* frames of the start lights (3, 2, 1: 40 frames each), then GO */
#define FINISH_FRAMES 150       /* the leader crossed the line: the race goes on this long behind the flag */
/* the start grid: 2 x 2 behind the line */
#define GRID_BACK     20        /* px behind the line, the first row */
#define GRID_ROW      26        /* px between the rows */
#define GRID_LANE     11        /* px from the centre line */
/* the championship (1 player): the 5 tracks, then the 5 again driven the other way; top 3 goes on */
#define TRACKS        5
#define CHAMP_RACES   10
#define QUALIFY_PLACE 3
#define CUP_RACES     3         /* 2-4 players: a cup of 3 races */
#define MEDAL_SCORES  {20, 80, 160, 280}    /* bronze, silver, gold, pearl: the championship points */
/* the bots (play.c bot_think): they drive the racing line with some error, brake for the corners they see coming;
 * rubber band: mild; the difficulty rises along the championship */
#define BOT_LOOK_WP   3         /* aim this many waypoints ahead (+1 per BOT_LOOK_SPEED of speed) */
#define BOT_LOOK_SPEED 256
#define BOT_BRAKE_WP  7         /* the corner ahead: the sharpest heading change over 3 waypoints in the next 7 ... */
#define BOT_TURN_FREE 70        /* ... up to this (a sweeping corner) at full speed; sharper: Q8 BOT_TURN_K slower */
#define BOT_TURN_K    1         /* per angle unit (/ the bot's corner %) ... */
#define BOT_SAFE_MIN  400       /* ... never under 1 px a frame */
#define BOT_STUCK     45        /* frames slower than 0.2 px a frame: reverse out */
#define BOT_REVERSE   36
#define RL_K          3         /* the racing line: px toward the inside per 16 units of turn ahead ... */
#define RL_MAX        14        /* ... at most */
#define RUBBER_PX     260       /* 1 player: a bot this far ahead of the player eases off, this far behind tries harder */
#define RUBBER_PCT    3         /* ... by 3% of its top speed */
/* the bots' personalities by colour: top speed %, corner speed % (how hard it takes a turn), line error px, wide
 * (px outward in the turns), reaction frames, lane px */
#define BOT_TABLE { \
    /* red    */ {97, 100, 6, 2, 10, -4}, \
    /* yellow */ {95, 108, 8, 6, 14,  5}, \
    /* blue   */ {98,  94, 5, 4, 12, -6}, \
    /* green  */ {96, 104, 7, 3, 16,  3}}
/* the difficulty (0 .. CHAMP_RACES - 1 in 1 player, CUP_LEVEL for 2-4 players): the bots' top speed % = the table's -
 * DIFF_SPEED0 + DIFF_SPEED_STEP x level / 2 (capped at the table's value + DIFF_SPEED_MAX); their error x (DIFF_ERR0 -
 * level) / DIFF_ERR0 + 1/2 */
#define DIFF_SPEED0   14
#define DIFF_SPEED_STEP 3        /* half points of top speed per level */
#define DIFF_SPEED_MAX 2
#define DIFF_ERR0     12
#define CUP_LEVEL     4
/* the player bot (bot=1) and the human-like one (botskill=E, 1..10: slower reactions, more error, timid corners) */
#define PBOT_SPEED    100
#define PBOT_CORNER   102
/* the flow */
#define RESULTS_FRAMES 360      /* the race results between two races (A goes on after RESULTS_LOCK) */
#define RESULTS_LOCK  45
#define PANEL_DELAY   60        /* frames between the end and the game-over panel */
#define RETRY_LOCK    36        /* frames the panel ignores the button (no accidental retry) */
#define BOT_START     20        /* frames on the title before the bot starts the run */
#define HINT_FRAMES   240       /* the controls hint at the start of the first race */
/* touch (player 1; play.c touch_*): a stick anywhere on the left part, a BRAKE button at the bottom right */
#define STICK_ZONE_PCT 62       /* the stick: a finger going down on the left 62% of the screen */
#define STICK_R       24        /* px of drag: the stick's ring (the knob stops there) */
#define STICK_DEAD    6         /* px: less than this keeps the heading (the ladybug still accelerates) */
#define BTN_BRAKE_R   22        /* the BRAKE button: radius, centre from the screen's bottom-right corner */
#define BTN_BRAKE_DX  34
#define BTN_BRAKE_DY  34
#define BTN_SLOP      6         /* px around the button that still press it */

/* ---- tracks.c: the tracks ---------------------------------------------------------------------------------------- */
enum { SH_CIRCLE, SH_BOX };
enum { OE_SOLID, OE_BUMP, OE_DECO };            /* an object stops you, throws you up, or is only drawn */
typedef struct obj_kind { uint8_t shape, effect; int16_t w, h; } obj_kind;   /* circle: w = radius; box: w x h */
enum { OB_CUP, OB_SUGAR, OB_TEAPOT, OB_SPOON, OB_COOKIE, OB_CRAYON_H, OB_CRAYON_V, OB_ERASER, OB_PENCIL, OB_PAINT,
       OB_APPLE, OB_SANDWICH, OB_JUICE, OB_BASKET, OB_TWIG, OB_LETTUCE, OB_PUMPKIN, OB_STONE, OB_CARROT, OB_POT,
       OB_SNAIL, OB_CAN, OB_COUNT };
extern const obj_kind obj_kinds[OB_COUNT];
typedef struct tobj { uint8_t kind, var; int16_t x, y; } tobj;      /* centre, world px; var: a colour */
enum { ZS_ELLIPSE, ZS_RECT };
typedef struct tzone { uint8_t surf, shape; int16_t x, y, rx, ry; } tzone;   /* a jam spill, a puddle (rect: half sizes) */
enum { LF_LEAF, LF_LEAF2, LF_FROND, LF_COUNT };
typedef struct tleaf { uint8_t kind, flip; int16_t x, y; } tleaf;  /* the canopy over the track (sprites) */
enum { TH_TEA, TH_DESK, TH_PICNIC, TH_VEG, TH_PATIO };
typedef struct track_def {
    const char *const *grid;            /* gh strings of gw pieces (tracks.c explains them) */
    int gw, gh;
    int sx, sy, sdir;                   /* the start: a straight's cell, the direction (0 E, 1 S, 2 W, 3 N) */
    int table;                          /* 1: a table, off the rect below is a fall; 0: a walled garden */
    int16_t tx0, ty0, tx1, ty1;         /* the table (px) */
    const tobj *obj; int nobj;
    const tzone *zone; int nzone;
    const tleaf *leaf; int nleaf;
    int theme;
} track_def;
extern const track_def tracks[TRACKS];

/* ---- play.c: the rules ------------------------------------------------------------------------------------------ */
enum { CS_RACE, CS_FALL, CS_OUT };              /* racing; falling off the table; left behind (gone for a moment) */
typedef struct car {
    int32_t x, y, vx, vy;               /* Q8 */
    int32_t z, vz;                      /* Q8: the hop's height */
    int16_t ang, steer;                 /* the heading; the turn this frame (the drift) */
    uint8_t state, surf, colour, human; /* colour 0 red, 1 yellow, 2 blue, 3 green; human: player + 1 (0: a bot) */
    int16_t timer, ghost;               /* frames left in the state; of passing through the others */
    int16_t wp, lap;                    /* the nearest waypoint (the checkpoints: only a few ahead count), laps done */
    int32_t prog;                       /* progress along the race (px): lap x length + the distance along */
    int32_t gift, drop_prog;            /* the ground gained by being put back behind the leader (not counted for the
                                         * places); the progress when it was left behind */
    int16_t points, place;              /* this race's points; the place now (1..4) */
    int16_t drops, falls;               /* counts: left behind, fallen */
    /* the brain (bots and the player bot) */
    int16_t err, err_t, stuck_t, rev_t, react;
    int16_t lane, wide, line_err, speed_pct, corner_pct;
    uint8_t brake, gas;                 /* the pedals this frame (draw.c: brake lights, dust) */
    uint8_t warn, pad_;                 /* near the zone's edge */
} car;

typedef struct waypoints {
    int n;
    int32_t len;                        /* a lap, px */
    int16_t x[MAX_WP], y[MAX_WP];       /* the centre line, px */
    int16_t dir[MAX_WP];                /* the direction to the next one */
    int16_t rl[MAX_WP];                 /* the racing line: px to the right of the centre line (+: right) */
    int32_t cum[MAX_WP];                /* the distance from the start line */
} waypoints;

enum { PH_COUNT, PH_RACE, PH_FINISH, PH_DONE };
typedef struct world {
    car c[RACERS];
    waypoints wp;
    uint8_t grid[MAX_GH][MAX_GW];       /* the pieces (tracks.c's characters) */
    int track, reverse, level, humans;  /* the track, driven backwards?, the bots' difficulty, players racing */
    int phase, phase_t, t;
    int leader;
    int32_t camx, camy;                 /* Q8: the camera (the centre of the race zone) */
    int wpx, wpy;                       /* the world's size, px */
    int order[RACERS];                  /* the cars by place */
    int final_place[RACERS];            /* by car, 1..4, when the race is done (points, then the finish order) */
    int events[RACERS];                 /* EV_* of the last step */
    int skill;                          /* the human-like player bot (botskill) */
    rs_rng rng;
    uint32_t seed;
} world;
enum { EV_BUMP = 1, EV_WALL = 2, EV_HOP = 4, EV_LAND = 8, EV_FALL = 16, EV_DROP = 32, EV_POINT = 64, EV_LAP = 128,
       EV_FINISH = 256, EV_SPLASH = 512, EV_JAM = 1024, EV_GO = 2048, EV_BEEP = 4096, EV_BACK = 8192 };

typedef struct pin { int16_t dx, dy; uint8_t gas, brake, any, pad_; } pin;   /* a racer's input this frame */

extern const char *const bot_names[RACERS];
extern const int16_t sin_q14[ANG_FULL / 4 + 1];
int     isin(int a);                    /* Q14 */
int     icos(int a);
int     iatan2(int32_t dy, int32_t dx); /* 0..1023 */
int     isqrt64(int64_t v);
int     ang_diff(int a, int b);         /* b - a, -512..511 */
void    world_init(world *w, int track, int reverse, int level, int humans, uint32_t seed);
void    world_step(world *w, const pin in[RACERS]);   /* humans' inputs (the bots think for themselves) */
int     track_build(world *w, int track, int reverse); /* the grid and the waypoints; 0: a bad track */
int     surface_at(const world *w, int x, int y);     /* SF_* at a world pixel */
int     car_speed(const car *c);                      /* Q8, forward (negative: reversing) */
void    bot_think(world *w, int i, pin *out);         /* the brain (bots, and player 1 with bot=1) */
int     racer_out_of_zone(const world *w, int32_t x, int32_t y);  /* the left-behind test (Q8 position) */
int     race_points_place(const world *w, int i);    /* the place of car i by points then progress (1..4) */
/* the championship and the cup */
int     champ_track(int race);                        /* which track race n is on (0..TRACKS-1) */
int     champ_reverse(int race);
int     champ_qualifies(int place);                   /* 1: goes on to the next race */
/* touch, player 1 (main.c reads the fingers; the geometry is here, testable) */
typedef struct tbutton { int x, y, r; } tbutton;
void    touch_brake_button(int left, int width, tbutton *b);   /* stage coordinates of the screen */
int     touch_on_button(const tbutton *b, int x, int y);
int     touch_stick_zone(int left, int width, int x, int y);
void    touch_stick(int x0, int y0, int x, int y, int16_t *dx, int16_t *dy);   /* a direction (length 256) or 0 */

/* ---- draw.c -------------------------------------------------------------------------------------------------------- */
enum { ST_TITLE, ST_PLAY, ST_RESULTS, ST_DEAD, ST_OVER };   /* no "get ready": the title is the only menu */
typedef struct touch_view {                         /* what main.c's touch reading shows (the stick, the button) */
    uint8_t seen, stick_on, brake_on, pad_;
    int16_t sx0, sy0, sx, sy;                       /* the stick: where it went down, the finger (screen px) */
} touch_view;
typedef struct run_info {                           /* the run, for the screens */
    int race, races;                                /* this race (0..), how many (the cup) */
    int score[MAX_PLAYERS];                         /* the players' points so far */
    int total[RACERS];                              /* every racer's points so far (by car) */
    int champion, out;                              /* 1 player: won all the races; failed (a 4th place) */
} run_info;
void draw_init(void);
void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused, const touch_view *tv,
                const run_info *ri);
void draw_state(void);
void draw_state_loaded(void);       /* widescreen: after a state saved at another width */
void draw_new_race(const world *w); /* a new track: its tiles, palettes, map */

/* ---- sound (main.c) ------------------------------------------------------------------------------------------------ */
enum { SFX_BUMP, SFX_WALL, SFX_HOP, SFX_LAND, SFX_FALL, SFX_DROP, SFX_POINT, SFX_BEEP, SFX_GO, SFX_SPLASH, SFX_JAM,
       SFX_LAP, SFX_FINISH, SFX_COUNT };
#define SFX_HOUSE SFX_COUNT                                    /* first slot of the house sounds (ha_init) */

#endif
