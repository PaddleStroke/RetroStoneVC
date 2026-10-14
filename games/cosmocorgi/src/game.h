/*
 * Cosmo Corgi: shared declarations.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/cosmocorgi/LICENSE.
 *
 * play.c is the game rules (no drawing, no sound, no screen width: deterministic, testable alone; the bot too);
 * draw.c is the video, main.c the flow (the title with the players joining, play, game over), input (pads and touch),
 * sound, save RAM, options and states. Made from games/_template (tools/new_game.py): the house style kit is
 * games/common (docs/art-direction.md).
 *
 * Coordinates: stage pixels (the 320 x 240 play area, y down; widescreen shows more of it on both sides, the rules
 * never ask the width); things that move by fractions are Q16. Angles: 256 to the turn.
 */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "rs.h"

#define MAX_PLAYERS 4                   /* the house kit's HU_MAX_PLAYERS: 4 pads */
#define Q16_ONE 65536
#define STAGE_W 320
#define STAGE_H 240

/* ---- tuning: every gameplay number is here (DESIGN.md "Rules") ------------------------------------------------ */
/* widescreen (DESIGN.md "Widescreen"): a third of the extra width behind the corgis, two thirds ahead; the play is
 * the stage's 320 columns at every width: things come from beyond the widest screen and the shots' reach is fixed */
#define WIDE_LEFT_NUM 1
#define WIDE_LEFT_DEN 3
#define SPAWN_R       496       /* enemies from the right appear here (560 px: the right edge is 480) ... */
#define SPAWN_L       (-96)     /* ... from the left here (560 px: the left edge is -80) */
#define ENTRY_R       336       /* a flight pattern starts (t = 0) just off the stage ... */
#define ENTRY_L       (-16)     /* ... on either side */
#define SHOT_RANGE    336       /* the corgis' shots go no further (the same reach at every width) */
#define FIELD_L       (-112)    /* things past these stage x leave the world */
#define FIELD_R       512
/* the rockets */
#define ROCKET_X0     64        /* the start: centre x, and y 120 (P1), 80, 160, 40 */
#define ROCKET_MIN_X  14        /* the centre stays in 14..300 x 14..226 (the stage) */
#define ROCKET_MAX_X  300
#define ROCKET_MIN_Y  14
#define ROCKET_MAX_Y  226
#define ROCKET_SPEED  98304     /* Q16 px/frame (1.5) on the d-pad ... */
#define SPEED_STEP    32768     /* ... +0.5 per speed boost ... */
#define SPEED_MAX     2         /* ... two of them (2.5) */
#define DIAG_K        181       /* /256: a diagonal goes 0.71 as fast on each axis */
#define HIT_HW        4         /* the hit box (bullets, robots): centre +-4 x +-3 (a small, forgiving core) */
#define HIT_HH        3
#define WALL_HW       9         /* against the walls: +-9 x +-5 (the rocket's body) */
#define WALL_HH       5
#define TOUCH_MAX     8         /* touch: the rocket follows the finger's movement 1:1, at most 8 px a frame */
/* the corgis' shots (auto-fire: always on) */
#define FIRE_GAP      9         /* frames between two volleys */
#define MAX_SHOTS     64
#define SHOTS_PER_PLAYER 16
#define BARK_SPEED    393216    /* Q16 px/frame (6): the bark, the basic shot */
#define SPREAD_VY     {0, 65536, 81920, 98304}  /* the spread shot's side shots, Q16 px/frame up and down, by level */
#define BEAM_SPEED    524288    /* the star beam: 8 px/frame, pierces */
#define BEAM_GAP      {0, 14, 11, 11}           /* frames between two beams, by level */
#define BEAM_DAMAGE   {0, 1, 2, 2}
#define BONE_SPEED    262144    /* homing bones: 4 px/frame ... */
#define BONE_TURN     7         /* ... turning at most 7/256 of a turn a frame toward the nearest robot ahead */
#define BONE_GAP      {0, 34, 34, 22}           /* frames between two bones, by level ... */
#define BONE_N        {0, 1, 2, 2}              /* ... bones each time */
#define BONE_LIFE     150
#define BONE_DAMAGE   2
#define WEAPON_MAX    3         /* weapon levels 1..3 */
#define PODS_MAX      2         /* puppy pods */
#define POD_DELAY     14        /* frames of the rocket's path between it and pod 1 (pod 2: twice) */
#define TRAIL_N       32
/* hits */
#define HIT_FREEZE    8         /* the hit-stop, frames */
#define HIT_FRAMES    60        /* the bonk: the rocket tumbles away, then the corgi is out */
#define SHIELD_INVUL  90        /* the shield popped: the rocket blinks, untouchable */
/* the scroll and the sections */
#define SCROLL_SPEED  65536     /* Q16 px/frame (1) in the first loop ... */
#define SCROLL_LOOP   16384     /* ... +0.25 each loop ... */
#define SCROLL_MAX    131072    /* ... at most 2 */
#define SECTIONS      4         /* the pastel nebula, the asteroid belt, the robot station, the icy comets */
#define SECTION_LEN   2560      /* px of scroll of waves in a section, then its boss */
#define SECTION_MAX_WAIT 480    /* px after SECTION_LEN at most for the last robots to go before the boss */
#define INTRO_FRAMES  150       /* the section's name */
#define WARN_FRAMES   120       /* BIG ROBOT! */
#define CLEAR_FRAMES  180       /* ROBOT POPPED!, then the next section */
/* the robots */
#define MAX_ENEMIES   32
#define KIND_HP       {1, 1, 2, 1, 10, 3, 2, 3, 3}             /* bug drone ufo dasher tank turret carrier rock comet */
#define KIND_POINTS   {10, 10, 30, 20, 100, 50, 50, 20, 20}
#define KIND_SPEED    {65536, 98304, 98304, 98304, 24576, 0, 49152, 81920, 98304}   /* Q16 px/frame */
#define LOOP_SPEED_PCT 15       /* robots 15% faster each loop (4 loops at most) */
#define SINE_W        2         /* sine waves: 2/256 of a turn a frame (a 128-frame wave) */
#define LOOP_R        32        /* the loop: radius, px ... */
#define LOOP_W        2         /* ... 2/256 of a turn a frame */
#define DASH_HOLD     50        /* the dasher: winds up 50 frames (sliding to a corgi's line, 1 px/frame) ... */
#define DASH_SPEED    262144    /* ... then dashes at 4 px/frame */
#define HOVER_FRAMES  110       /* the UFO hovers, fires at the middle, then leaves */
#define GROUP_BONUS   100       /* a gold group all popped: points and a capsule */
/* their shots: readable, never a bullet hell */
#define MAX_EBULLETS  48
#define EB_SPEED      81920     /* Q16 px/frame (1.25) in the first loop ... */
#define EB_SPEED_LOOP 16384     /* ... +0.25 each loop ... */
#define EB_SPEED_MAX  147456    /* ... at most 2.25 */
#define EB_FAIR_DIST  64        /* a robot never fires at a corgi closer than this (px) */
#define FIRE_LOOP_PCT 20        /* reloads 20% shorter each loop (down to half) */
#define TANK_FIRE     150       /* frames between the robo-turtle's 3-shot fans */
#define TURRET_FIRE   160       /* between a turret's aimed shots */
#define SMALL_FIRE_LOOP 2       /* from loop 2 the bugs and drones fire too, once each now and then */
/* capsules (power-ups) */
#define MAX_CAPS      6
#define CAPSULE_SPEED 32768     /* Q16 px/frame (0.5) drifting left, bobbing */
#define CAP_ORDER     {CAP_SPREAD, CAP_POD, CAP_SHIELD, CAP_SPEED, CAP_BEAM, CAP_POD, CAP_HOMING, CAP_SHIELD, \
                       CAP_SPREAD, CAP_SPEED, CAP_HOMING, CAP_BEAM}
#define MAXED_POINTS  500       /* a capsule of something already at its maximum */
/* the bosses */
#define BOSS_HP       {40, 56, 64, 72}          /* the ladybird, the crab, the owl, the walrus ... */
#define BOSS_HP_PLAYER 50       /* ... +50% per extra player ... */
#define BOSS_HP_LOOP  25        /* ... +25% each loop */
#define BOSS_X        252       /* its centre x when it fights */
#define BOSS_ENTER    120       /* frames to fly in (untouchable) */
#define BOSS_TIME     3600      /* after 60 s it flies away (no points) */
#define BOSS_POINTS   {1000, 1500, 2000, 2500}
#define BOSS_HIT_POINTS 10
#define BOSS_DOWN_FRAMES 90
/* the walls (the asteroid belt, the station, the comets): heights in 8-px tiles from the top and the bottom */
#define TER_RING      128       /* columns kept: 1024 px around the stage */
#define TER_MIN_GAP   13        /* tiles always open between them (104 px) */
#define FX_MAX        32
/* the flow */
#define PANEL_DELAY   40        /* frames between the last corgi's bonk and the game-over panel */
#define RETRY_LOCK    36        /* frames the panel ignores the button (no accidental retry) */
#define READY_BOB     4         /* the title rocket's bob amplitude, px */
#define BOT_START     20        /* frames on the title before the bot starts the run */
#define MSG_FRAMES    120       /* the power-ups' names */
#define MEDAL_SCORES  {2000, 6000, 13000, 25000} /* bronze, silver, gold, pearl (1 player; tuned with botskill) */

/* ---- play.c: the rules ------------------------------------------------------------------------------------------ */
enum hero_state { SH_READY, SH_PLAY, SH_HIT, SH_OUT, SH_OFF };
enum { W_BARK, W_SPREAD, W_BEAM, W_HOMING };
typedef struct hero {
    int state, t;               /* state, frames in it */
    int32_t x, y;               /* Q16 centre */
    int score;
    int weapon, wlevel;         /* W_*, level 0 (the bark) .. 3 */
    int speed, shield, pods;    /* speed boosts 0..2, the shield 0/1, puppy pods 0..2 */
    int invul;                  /* frames left blinking (untouchable) */
    int fire_t, beam_t, bone_t; /* frames to the next volley, beam, bones */
    int tilt;                   /* -1 up, 0, 1 down: the last vertical move (the sprite) */
    int moving;                 /* it moved this frame (the flame) */
    int16_t trail_x[TRAIL_N], trail_y[TRAIL_N];   /* its path (the pods follow it) */
    int trail_i;
    int shots, pops, caps;      /* volleys, robots popped, capsules caught */
    int bot_dx, bot_dy, bot_t;  /* the bot's last decision (a human-like bot reacts late) */
} hero;
enum { SK_BARK, SK_SPREAD, SK_BEAM, SK_BONE };
typedef struct shot { int32_t x, y, vx, vy; uint32_t hit; uint8_t on, kind, owner, damage; int16_t life, angle; } shot;
enum { EK_BUG, EK_DRONE, EK_UFO, EK_DASHER, EK_TANK, EK_TURRET, EK_CARRIER, EK_ROCK, EK_COMET, EK_KINDS };
enum { PAT_LINE, PAT_SINE, PAT_LOOP, PAT_SWOOP_TOP, PAT_SWOOP_BOT, PAT_BEHIND, PAT_DASH, PAT_HOVER, PAT_TANK,
       PAT_TURRET, PAT_DRIFT };
typedef struct enemy {
    int32_t x, y;               /* Q16 centre */
    int32_t vx, vy;             /* its last move (the pre-entry glide, the bot's forecast) */
    int32_t v;                  /* its speed */
    int32_t wx;                 /* turrets: world x */
    int16_t t;                  /* frames since its pattern started (< 0: still gliding in from beyond the screen) */
    int16_t y0, p, aux, aux_t;  /* the pattern's lane, parameter, phase and phase start */
    int16_t fire_t;             /* frames to its next shot */
    uint8_t on, kind, pat, hp, group, flash, side, fired;
} enemy;
enum { EB_PELLET, EB_BIG, EB_ICICLE };
typedef struct ebullet { int32_t x, y, vx, vy; uint8_t on, kind, pad[2]; } ebullet;
enum { CAP_SPREAD, CAP_BEAM, CAP_HOMING, CAP_SPEED, CAP_SHIELD, CAP_POD, CAP_KINDS };
typedef struct capsule { int32_t x, y; int16_t t; uint8_t on, kind; } capsule;
enum { FX_POP, FX_BIGPOP, FX_POINTS, FX_FIZZLE, FX_SHIELD, FX_SPARK };
typedef struct effect { int16_t x, y, value; uint8_t kind, t, on, pad; } effect;
enum { BK_LADYBIRD, BK_CRAB, BK_OWL, BK_WALRUS };
enum { BP_NONE, BP_ENTER, BP_FIGHT, BP_DOWN, BP_LEAVE };
typedef struct boss {
    int on, kind, phase, t, hp, hp0, flash, fire_t, volley, angle, dir;
    int32_t x, y;
} boss;
/* one player's input for a frame: the d-pad (-1, 0, 1 each axis); a touch drag (the finger's movement, px; touch 0:
 * no finger); any (a button: the first press of a ready=1 run) */
typedef struct pin { int dx, dy, touch, tdx, tdy, any; } pin;

enum { PH_READY, PH_INTRO, PH_WAVES, PH_WARN, PH_BOSS, PH_CLEAR };
enum { MSG_NONE, MSG_SPREAD, MSG_BEAM, MSG_HOMING, MSG_SPEED, MSG_SHIELD, MSG_POD, MSG_MAX };
enum { THEME_NEBULA, THEME_ROCK, THEME_STATION, THEME_ICE };
typedef struct world {
    /* the scroll and the section */
    int32_t scroll, scroll_v;   /* Q16 px travelled; px per frame */
    int32_t sec_dist;           /* Q16 px since the section began */
    int sec, loop, phase, phase_t, waves_next, cap_n, boss_away;
    uint32_t waves_done;        /* the section's waves already sent (bits) */
    /* the walls: a ring of columns (world column c at c % TER_RING) */
    uint8_t ter_ceil[TER_RING], ter_floor[TER_RING], ter_theme[TER_RING];
    int ter_col;                /* the next column to make */
    int ter_sec, ter_col0;      /* the section the walls being made belong to, its first column */
    int ter_next_col0;          /* the next section's first column (-1: not yet known) */
    int ter_c, ter_f, ter_run, ter_gate;
    rs_rng ter_rng;
    /* the play */
    hero h[MAX_PLAYERS];
    shot shots[MAX_PLAYERS * SHOTS_PER_PLAYER];
    enemy en[MAX_ENEMIES];
    ebullet eb[MAX_EBULLETS];
    capsule caps[MAX_CAPS];
    effect fxs[FX_MAX];
    int fxs_next;
    int group_left[8], group_n;  /* the gold groups: robots still to pop (-1: one got away) */
    boss b;
    int freeze;
    rs_rng rng;
    uint32_t seed;
    int players, t, started;
    int msg, msg_t, msg_player;
    int events[MAX_PLAYERS];    /* EV_* of the last step */
    int wevents;                /* WEV_* of the last step */
} world;
enum { EV_FIRE = 1, EV_POP = 2, EV_HIT = 4, EV_OUT = 8, EV_CAPSULE = 16, EV_SHIELD = 32, EV_BOSS_HIT = 64,
       EV_BOSS_DOWN = 128, EV_START = 256, EV_GROUP = 512, EV_MAXED = 1024, EV_BONE = 2048 };
enum { WEV_ESHOT = 1, WEV_SECTION = 2, WEV_WARN = 4, WEV_BOSS_IN = 8, WEV_CLEAR = 16, WEV_DASH = 32, WEV_LOOP = 64,
       WEV_BOSS_FIRE = 128, WEV_BOSS_AWAY = 256 };

void world_init(world *w, int players, uint32_t seed);
void world_start_section(world *w, int sec, int loop);   /* tests, --opt section=N */
void world_to_boss(world *w);                            /* tests, --opt boss=1: the section's boss now */
void world_step(world *w, const pin in[MAX_PLAYERS]);
int  world_running(const world *w);             /* someone still plays */
int  world_all_out(const world *w);
int  world_theme(int sec);                      /* THEME_* of a section */
int  world_stage(const world *w);               /* 1, 2, ...: loop * 4 + section + 1 */
int  ter_ceil_at(const world *w, int wcol);     /* the walls at a world column (tiles; 0 none) */
int  ter_floor_at(const world *w, int wcol);
int  ter_theme_at(const world *w, int wcol);
int  wall_hit(const world *w, int x0, int y0, int x1, int y1);   /* a stage box touches a wall */
int  pod_pos(const world *w, int p, int k, int *x, int *y);      /* puppy pod k (0, 1) of player p */
int  enemy_half_w(int kind);
int  enemy_half_h(int kind);
int  boss_half_w(int kind);
int  boss_half_h(int kind);
int  isin256(int a);                            /* sine, Q14, a in 1/256 of a turn */
#define icos256(a) isin256((a) + 64)
/* the bot: the input it gives player p (skill 0 plays as well as it can; 1..10 a human-like player, worse) */
pin  bot_decide(world *w, int p, int skill);

/* ---- draw.c -------------------------------------------------------------------------------------------------------- */
enum { ST_TITLE, ST_PLAY, ST_DEAD, ST_OVER };      /* no "get ready": the title is the only menu */
void draw_init(void);
void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused);
void draw_state(void);
void draw_state_loaded(void);       /* widescreen: after a state saved at another width */

/* ---- sound (main.c) ------------------------------------------------------------------------------------------------ */
enum { SFX_SHOT, SFX_POP, SFX_HIT, SFX_ESHOT, SFX_CAPSULE, SFX_SHIELD, SFX_BOSS_HIT, SFX_BOSS_DOWN, SFX_WARN,
       SFX_DASH, SFX_BARK, SFX_BONE, SFX_COUNT };
#define SFX_HOUSE SFX_COUNT                                    /* first slot of the house sounds (ha_init) */

#endif
