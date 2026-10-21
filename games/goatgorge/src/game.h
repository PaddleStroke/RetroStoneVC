/*
 * Goat Gorge: shared declarations and the tuning table (every gameplay number is here).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/goatgorge/LICENSE.
 *
 * world.c is the rules: the rooms (parsed from the levels' text), the kid's movement and everything it touches. It uses
 * no SDK call (no drawing, no sound, no RNG): deterministic, linked alone by the tests and the room solver
 * (tests/test_rooms.c, tests/solver.c). draw.c is the video, main.c the flow (the title and its chapters, play, the
 * results, pause), input, sound, save RAM, options and save states.
 */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>

/* ---- the rooms: 20 x 15 cells of 16 px, one screen (320 x 240) ------------------------------------------------------ */
#define CELL        16
#define ROOM_W      20
#define ROOM_H      15
#define ROOM_PX_W   (ROOM_W * CELL)
#define ROOM_PX_H   (ROOM_H * CELL)
#define CHAPTERS    5
#define MAX_ROOMS   16              /* per chapter */
#define MAX_GEMS    8
#define MAX_SPRINGS 8
#define MAX_PLATS   6
#define MAX_CRUMBLE 48              /* crumbling cells per room */
#define MAX_DECO    10              /* decorative props per room */

/* the cells (the levels' characters: levels/README.md) */
enum {
    C_EMPTY, C_ROCK, C_LEDGE, C_CRUMBLE, C_SPIKE_U, C_SPIKE_D, C_SPIKE_L, C_SPIKE_R,
    C_COUNT
};

/* ---- the kid: hitbox and movement (fixed point: 1 px = 256 sub-pixels; speeds in sub-pixels per frame) ---------- */
#define SUB          256
#define HB_W         10             /* the solid hitbox (generous for ledges: narrower than a cell) */
#define HB_H         13
#define HURT_IN_X    2              /* the hurtbox against hazards: the hitbox inset (forgiving) */
#define HURT_IN_TOP  3
#define HURT_IN_BOT  1
#define MAX_RUN      512            /* 2.0 px/f */
#define RUN_ACCEL    80             /* ground: about 6 frames to full speed */
#define RUN_DECEL    112            /* ground, no input: 5 frames to a stop */
#define TURN_ACCEL   144            /* ground, input against the motion: a snappy turn */
#define AIR_ACCEL    56             /* air: 0.7 of the ground's */
#define AIR_DECEL    36
#define OVER_DECEL   24             /* faster than MAX_RUN (after a dash, a wall jump): eased back, ground */
#define OVER_DECEL_AIR 12           /* ... in the air */
#define GRAVITY      92             /* 0.36 px/f^2 */
#define APEX_GRAVITY 46             /* half gravity near the apex while jump is held */
#define APEX_V       176            /* |vy| under this: the apex */
#define MAX_FALL     1024           /* 4.0 px/f: terminal velocity */
#define FAST_FALL    1472           /* 5.75 px/f holding Down (fast-fall) */
#define FAST_ACCEL   40             /* how fast the fast-fall speed is reached */
#define JUMP_V       896            /* 3.5 px/f, held up to VAR_JUMP frames: full jump 3 cells, a tap ~1 */
#define VAR_JUMP     10
#define JUMP_CUT     384            /* jump released while rising: vy cut to -1.5 px/f at most */
#define JUMP_HBOOST  64             /* the jump adds this toward the input */
#define COYOTE       6              /* frames after leaving a ledge when a jump still works */
#define JUMP_BUFFER  6              /* frames a jump press waits for the ground / a wall */
#define WALL_CHECK   2              /* px: a wall this close can be jumped off */
#define WALL_JUMP_VX 640            /* 2.5 px/f away from the wall */
#define WALL_LOCK    9              /* frames the input is held away from the wall after a wall jump */
#define WALL_SLIDE   288            /* 1.125 px/f: the slide's top speed while pushing into a wall */
#define WALL_SLIDE_ACCEL 30
#define DASH_SPEED   2048           /* 8 px/f */
#define DASH_DIAG    1448           /* DASH_SPEED / sqrt 2 */
#define DASH_FRAMES  8              /* 64 px */
#define DASH_FREEZE  3              /* the freeze frame (the direction is read at its end: forgiving) */
#define DASH_BUFFER  4              /* frames a dash press waits for the dash to be back (a landing, a gem) */
#define DASH_COOLDOWN 12           /* frames before the next dash (a refill gem mid-dash) */
#define DASH_REFILL_COOL 6          /* frames after a dash starts before the ground refills it */
#define END_DASH     768            /* 3.0 px/f: the speed kept when a dash ends (up: 3/4 of it) */
#define SUPER_VX     832            /* a jump during a dash on the ground: 3.25 px/f along the dash */
#define CORNER_JUMP  4              /* px: a jump into a ceiling's corner slides around it */
#define CORNER_DASH  5              /* px: a dash into a ledge's corner pops over it */
#define LEDGE_POP    3              /* px: running / falling into a ledge's lip steps up onto it */
#define SPRING_V     1152           /* 4.5 px/f, kept SPRING_VAR frames: about 5 cells */
#define SPRING_VAR   13
#define GEM_RESPAWN  150
#define CRUMBLE_SHAKE 30
#define CRUMBLE_BACK 120
#define PLAT_PAUSE   30             /* frames a moving platform waits at each end */
#define DEATH_FRAMES 22             /* the burst, then the room again */
#define SPAWN_FLASH  14

/* ---- input: the bits world_step() reads (main.c maps the pad onto them; the solver and the replays feed them) ---- */
enum { IN_LEFT = 1, IN_RIGHT = 2, IN_UP = 4, IN_DOWN = 8, IN_JUMP = 16, IN_DASH = 32 };

/* ---- a room, as parsed (constant while it is played) ------------------------------------------------------------- */
typedef struct plat_def {
    int16_t x0, y0, x1, y1;         /* the left-top corner's two ends (px) */
    int16_t w, h;                   /* px */
    int16_t dist;                   /* px between the ends */
} plat_def;
typedef struct room {
    uint8_t cell[ROOM_H][ROOM_W];
    int16_t start_x, start_y;       /* the kid's hitbox top-left at the start */
    int16_t berry_x, berry_y;       /* the berry's centre (has_berry) */
    uint8_t has_berry;
    uint8_t ngems, nsprings, nplats, ncrumble;
    int16_t gem_x[MAX_GEMS], gem_y[MAX_GEMS];           /* centres */
    int16_t spring_x[MAX_SPRINGS], spring_y[MAX_SPRINGS]; /* the cell's top-left */
    plat_def plat[MAX_PLATS];
    uint8_t crumble_cx[MAX_CRUMBLE], crumble_cy[MAX_CRUMBLE], crumble_group[MAX_CRUMBLE];
    int8_t crumble_at[ROOM_H][ROOM_W];  /* index into the crumble list, -1 none */
    int16_t wind_x, wind_y;         /* sub-px per frame */
    int16_t wind_on, wind_off;      /* a gust cycle: on frames, then off frames (off 0: always) */
    uint8_t exit_side;              /* EXIT_* */
    uint8_t hint;                   /* a tip shown in this room (1 jump, 2 wall jump, 3 dash...; 0 none) */
    uint8_t open_top;               /* the top row opens on the sky (with a side's exit: not a way out) */
    uint8_t exit_set, wall_sides;   /* exit= on the room's line; then the sides not the way out are unseen walls */
    uint8_t ndeco;                  /* the chapter's props drawn in the room (d: a decoration, not solid) */
    uint8_t deco_cx[MAX_DECO], deco_cy[MAX_DECO];
    uint8_t grip_off;               /* GRIP_*: walls the kid cannot hold (tests/solver.c's skip check; the game: 0) */
} room;
enum { EXIT_NONE, EXIT_TOP, EXIT_LEFT, EXIT_RIGHT };
enum { GRIP_NO_BORDER = 1, GRIP_NO_INTERIOR = 2 };

/* ---- the world: the kid and the room's moving parts (saved in states) ---------------------------------------------- */
typedef struct hero {
    int16_t x, y;                   /* the hitbox's top-left, px */
    int16_t rx, ry;                 /* the sub-pixel remainders */
    int16_t vx, vy;
    int8_t facing, wall_dir;        /* -1 / 1; the wall touched (0 none) */
    int8_t dash_dx, dash_dy;        /* the dash's direction */
    uint8_t grounded, dash_avail, dash_t, freeze_t, dash_cool, refill_cool;
    uint8_t coyote_t, buffer_t, dash_buf, var_t, auto_var, can_cut, wall_lock, wall_lock_dir;
    uint8_t held;                   /* the input bits of the last frame (edges) */
    uint8_t dead_t, spawn_t, land_t, jump_t, sliding, riding;   /* riding: platform index + 1 */
    int16_t var_v;                  /* the speed a variable jump holds (a jump, a spring) */
    int16_t land_v;                 /* the landing's speed (squash) */
    uint16_t run_t;                 /* frames running (the run cycle, dust) */
} hero;
typedef struct world {
    hero h;
    int32_t t;                      /* frames since the room (re)started (the platforms' clock) */
    int16_t crumble[MAX_CRUMBLE];   /* 0 there; > 0 shaking (frames left); < 0 gone (frames to come back) */
    int16_t gem_t[MAX_GEMS];        /* 0 there; > 0 frames until it is back */
    uint8_t spring_t[MAX_SPRINGS];  /* the bounce's animation */
    uint8_t berry_got, exited, wind_now;
    int8_t ignore_plat;             /* a platform moving its rider (not solid for it meanwhile), -1 none */
    int16_t plat_x[MAX_PLATS], plat_y[MAX_PLATS];   /* this frame's platform positions */
    uint32_t events;                /* EV_* of the last step */
} world;
enum {
    EV_JUMP = 1, EV_WALLJUMP = 2, EV_DASH = 4, EV_LAND = 8, EV_DEATH = 16, EV_BERRY = 32, EV_GEM = 64,
    EV_SPRING = 128, EV_CRUMBLE = 256, EV_EXIT = 512, EV_RESPAWN = 1024, EV_DASH_GO = 2048, EV_SUPER = 4096,
    EV_BONK = 8192
};

/* world.c */
int  room_parse(const char *text, room *r, char *err, int errlen);   /* one room's 15 lines; 0 ok */
/* a chapter's text (levels/chN.txt): its rooms in order; returns the count (-1: an error, in err) */
int  chapter_parse(const char *text, room *rooms, int max, char *err, int errlen);
void world_spawn(world *w, const room *r, int held);   /* the room from its start (held: the input already down) */
void world_step(world *w, const room *r, int input);
int  world_solid(const world *w, const room *r, int x, int y, int wd, int ht);
int  world_cell(const room *r, int cx, int cy);     /* outside the room: its border carried on, the bottom open */
int  border_rock(const room *r, int cx, int cy);    /* rock of a side wall: joined to the room's side along its row */
int  world_grip(const world *w, const room *r, int x, int y, int wd, int ht);  /* a wall to slide on, jump off */
void plat_pos(const plat_def *p, int32_t t, int *x, int *y);
int  crumble_solid(const world *w, int i);
int  wind_active(const room *r, int32_t t);
uint32_t world_hash(const world *w);

/* the solutions (tests/solver.c writes levels/solutions.txt): one input byte per frame, run-length coded */
int  sol_decode(const char *rle, uint8_t *out, int max);    /* the frames, -1 on a bad string */
extern const char sol_abc[65];                              /* the letters of the input values 0..63 */

/* ---- draw.c ---------------------------------------------------------------------------------------------------------- */
enum { ST_TITLE, ST_PLAY, ST_RESULTS };
#define RETRY_LOCK 36               /* frames the results ignore the button (no accidental skip) */
typedef struct view {               /* what draw_frame shows (main.c fills it) */
    int state, st_t, paused, chapter, room, rooms;
    int fade;                       /* 0 none .. 16 black (a room's change) */
    int time, deaths, berries, berries_max;                  /* the climb so far */
    int sel, unlocked;                                       /* the title: the chapter shown, the chapters open */
    int best_time[CHAPTERS], best_deaths[CHAPTERS], best_berries[CHAPTERS], cleared[CHAPTERS], room_count[CHAPTERS];
    int berry_count[CHAPTERS];                               /* the berries a chapter has */
    int new_time, new_deaths, new_berries;                   /* the results */
    int hud_t;                                               /* frames since the room began (the room's name) */
    int run_ch, run_room;                                    /* the climb under way (the title: CONTINUE), -1 none */
    int can_skip, skipped;                                   /* paused after many deaths: skip; the rooms skipped */
    int card;                                                /* the chapter's title card: frames left (0 none) */
} view;
void draw_init(void);
void draw_room(const room *r, int chapter, int room_no);    /* a new room: its map */
void draw_frame(const world *w, const room *r, const view *v);
void draw_state(void);
void draw_relayout(void);           /* after a state of another width or height */
void draw_events(const world *w, const room *r);           /* the effects of the step's events (dust, trail...) */

/* ---- sound (main.c) ------------------------------------------------------------------------------------------------ */
enum { SFX_JUMP, SFX_WALLJUMP, SFX_DASH, SFX_LAND, SFX_DEATH, SFX_BERRY, SFX_GEM, SFX_SPRING, SFX_CRUMBLE, SFX_CLEAR,
       SFX_RESPAWN, SFX_COUNT };
#define SFX_HOUSE SFX_COUNT

#endif
