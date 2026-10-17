/*
 * Llama Drama: shared declarations.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/llamadrama/LICENSE.
 *
 * play.c is the game rules (no drawing, no sound, no screen width: deterministic, testable alone; the bot too);
 * draw.c is the video, main.c the flow (the title with the players joining, play, game over), input (pads and touch),
 * sound, save RAM, options and states. Made from games/_template (tools/new_game.py): the house style kit is
 * games/common (docs/art-direction.md).
 *
 * Coordinates: stage pixels (the 320 x 240 play area, y down; widescreen shows more of it on both sides, the rules
 * never ask the width); things that move by fractions are Q16. Angles: 1024 to the turn, counter-clockwise on the
 * screen (an aim > 0 points the snout up).
 */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "rs.h"

#define MAX_PLAYERS 4                   /* the house kit's HU_MAX_PLAYERS: 4 pads */
#define Q16_ONE 65536
#define STAGE_W 320
#define STAGE_H 240
#define ANG_TURN 1024

/* ---- tuning: every gameplay number is here (DESIGN.md "Rules") ------------------------------------------------ */
/* widescreen (DESIGN.md "Widescreen"): a third of the extra width behind the llamas, two thirds ahead; the play is the
 * stage's 320 columns at every width: hornets come from beyond the widest screen, the spit's reach is fixed */
#define WIDE_LEFT_NUM 1
#define WIDE_LEFT_DEN 3
#define SPAWN_R       496       /* hornets appear here (560 px: the right edge is 480) ... */
#define ENTRY_R       336       /* ... and glide in along their lane to here, where their pattern starts */
#define GOB_RANGE     336       /* the spit flies no further (the same reach at every width) */
#define FIELD_L       (-48)     /* a hornet past this stage x has left */
/* the herd: where each player's llama stands (P1 the middle one; P2 behind, taller; P3 in front, shorter; P4 at the
 * back, tallest): its shoulder (where the neck starts) and its neck's length */
#define SLOT_X        {44, 24, 68, 4}
#define SLOT_Y        {182, 170, 196, 160}
#define SLOT_NECK     {64, 92, 30, 118}
#define DRAW_ORDER    {2, 0, 1, 3}      /* front to back: P3, P1, P2, P4 */
/* a phone held upright (DESIGN.md "Tall screen"; layout only, the rules unchanged): a screen of ZOOM_H lines
 * (rs_screen_zoom) filling the display's height, the sides cropped: the left columns shown (the herd and the sky ahead
 * of it, 205..240 columns on today's phones), the stage low, TALL_BELOW lines of meadow under it */
#define ZOOM_H        480
#define TALL_BELOW    32
/* aiming: UP/DOWN turn the head (and lean the neck) between the grass and the sky */
#define AIM_MIN       (-100)    /* -35 degrees: low, at the grass ... */
#define AIM_MAX       156       /* ... 55 degrees: high in the sky ... */
#define AIM_FRAMES    7         /* ... the head's frames, 15 degrees apart (make_art.py HEAD_ANGLES) */
#define AIM_START     14        /* 5 degrees */
#define AIM_SLOW      2         /* units a frame for the first AIM_ACCEL frames held (fine aim) ... */
#define AIM_FAST      5         /* ... then this: the whole range in about 1 s */
#define AIM_ACCEL     8
#define NECK_ANG0     205       /* the neck's angle at aim 0 (72 degrees) ... */
#define NECK_K        80        /* ... + aim * 80/256 (61..89 degrees: aiming low leans it toward the hornets) */
#define NECK_GAP      11        /* px between the neck's pieces */
/* the head (make_art.py HEAD_*): head units, u toward the snout, v up, from the sprite's centre */
#define HEAD_JOINT_U  (-5)      /* where the neck joins the head */
#define HEAD_JOINT_V  (-5)
#define HEAD_MOUTH_U  13        /* the mouth: the spit starts there */
#define HEAD_MOUTH_V  (-3)
#define HEAD_R        10        /* a hornet this close to the head's centre (+ its own radius) stings */
#define NECK_R        6         /* ... or this close to the neck */
#define SCARED_DIST   60        /* a hornet this close: the scared face */
/* the spit */
#define GOB_SPEED     327680    /* Q16 px/frame (5) along the aim ... */
#define GOB_GRAV      1311      /* ... and a slight arc: 0.02 px/frame/frame down */
#define GOB_R         3
#define GOBS_MAX      3         /* gobs in the air per llama at most ... */
#define RELOAD        12        /* ... and frames between two spits (no wall of spit) */
#define SPIT_BUFFER   12        /* a press during the reload (or with 3 gobs up) spits as soon as it can, 12 frames on */
#define PUFF_FRAMES   10        /* the puffed cheeks after a spit */
#define GROUND_HIT    200       /* the spit splats on the meadow below this y */
#define TOUCH_MIN_DX  12        /* touch: a tap at least this far right of the mouth aims there */
/* the hornets */
#define MAX_HORNETS   24
#define KIND_HP       {1, 1, 3}             /* small, dart, big */
#define KIND_POINTS   {10, 25, 50}
#define KIND_R        {6, 5, 11}            /* hit radius, px */
#define KIND_SPEED    {100, 100, 70}        /* % of the hornet speed */
#define HORNET_V0     49152     /* Q16 px/frame (0.75) at the start ... */
#define HORNET_V_K    2         /* ... +2% per heat point ... */
#define HORNET_V_MAX  160       /* ... up to +160% (1.95 px/frame) */
#define HOME_DX       112       /* nearer than this (x) to its llama's head, a hornet steers to it ... */
#define HOME_VY       45875     /* ... at up to 0.7 px/frame up or down (+1% per heat point, 2x at most) */
#define LANE_MIN      36        /* the lanes they come along */
#define LANE_MAX      172
#define LANE_EASY     64        /* the first waves (heat < 6): lanes in 64..150, the middle of the sky */
#define LANE_EASY_MAX 150
#define SINE_K        5         /* sine waves and zig-zags follow the x: 5/1024 of a turn a px (a 205-px wave) */
#define SINE_AMP      {14, 22, 30}          /* amplitude by heat: < 12, < 30, more */
#define ZIG_AMP       24
#define DIVE_PCT      170       /* a diving hornet: 1.7x its speed, at its llama's head */
#define LOOP_R        28        /* the loop's radius, px */
#define DART_PERCH    288       /* the dart stops here ... */
#define DART_WIND     50        /* ... buzzes (a warning) for this many frames ... */
#define DART_SPEED    176128    /* ... then dashes at its llama's head: 2.7 px/frame (+1% per heat point) */
#define BIG_PUSH      6         /* a big hornet hit is knocked back 6 px */
#define FLASH_FRAMES  8
/* the ramp (DESIGN.md "The ramp"): heat = the herd's score / 10 (with more llamas it counts less: HEAT_PF / 4) */
#define HEAT_PTS      10
#define HEAT_PF       {4, 6, 7, 8}
#define WAVE_FIRST    20        /* frames to the first wave */
#define GAP0          124       /* frames between waves per 2 units of cost at heat 0 ... */
#define GAP_K         2         /* ... 2 fewer per heat point ... */
#define GAP_MIN       38        /* ... this at least */
#define UNLOCK_ZIGZAG 5         /* the heat from which each wave comes */
#define UNLOCK_PAIR   8
#define UNLOCK_DIVE   12
#define UNLOCK_LOOP   16
#define UNLOCK_DART   22
#define UNLOCK_SWARM  28
#define UNLOCK_BIG    36
#define DOUBLE_FROM   50        /* from here, now and then two waves at once (more and more often) */
#define SWARM_N       {4, 5, 6} /* a swarm: 4 hornets, 5 from heat 50, 6 from 70 */
#define ESCORT_FROM   60        /* a big hornet comes with two small ones */
/* the sky: the day turns to a sunset, the dusk and the night as the heat rises */
#define SKY_SUNSET    30
#define SKY_DUSK      55
#define SKY_NIGHT     80
/* effects */
#define FX_MAX        40
/* hits */
#define HIT_FREEZE    8         /* the hit-stop, frames */
#define HIT_FRAMES    60        /* the bonk: the llama droops, then it is out */
/* the flow */
#define PANEL_DELAY   40        /* frames between the last llama's bonk and the game-over panel */
#define RETRY_LOCK    36        /* frames the panel ignores the button (no accidental retry) */
#define BOT_START     20        /* frames on the title before the bot starts the run */
/* the title's layout (draw.c title_place; looks only): 2-4 llamas wait 44 px further left, each head whole on the
 * screen and left of the logo (UI x 37), of P1's slot (30) or of the prompt's glyph; one llama stands 12 px left and
 * 12 px up the meadow (its hooves above the join line, its ears under the slots, its chest left of the glyph) */
#define TITLE_SHIFT   44
#define TITLE_SOLO_DX 12
#define TITLE_SOLO_LIFT 12
#define TITLE_HEAD_L  11        /* the head's extent from its centre (the ears ... the snout), its chest's from the */
#define TITLE_HEAD_R  15        /* shoulder */
#define TITLE_CHEST_R 10
#define TITLE_LOGO_X  37
#define TITLE_SLOT_X  30
#define TITLE_SLOT_ROW 9        /* the player slots' row on the title (under the logo, over the herd's heads) */
#define MSG_FRAMES    150       /* the new hornets' names */
#define HINT_FRAMES   300       /* the controls' hint at the start of a run */
#define MEDAL_SCORES  {150, 450, 1000, 3000} /* bronze, silver, gold, pearl (1 player; tuned with botskill) */

/* ---- play.c: the rules ------------------------------------------------------------------------------------------ */
enum llama_state { LS_READY, LS_PLAY, LS_HIT, LS_OUT, LS_OFF };
typedef struct llama {
    int state, t;               /* state, frames in it */
    int aim;                    /* 1/1024 turn, AIM_MIN..AIM_MAX */
    int hold, dir;              /* frames UP or DOWN held (the aim speeds up), which one (-1 up, 1 down) */
    int reload, buffer, puff;   /* frames to the next spit; a press waiting; the puffed cheeks */
    int scared;                 /* a hornet is close (the face) */
    int score, pops, spits;
    int bot_t, bot_dy, bot_spit, bot_err, bot_target;   /* the bot's last decision (a human-like bot is slower) */
} llama;
typedef struct gob { int32_t x, y, vx, vy; int16_t t; uint8_t on, owner; } gob;
enum { HK_SMALL, HK_DART, HK_BIG, HK_KINDS };
enum { PAT_STRAIGHT, PAT_SINE, PAT_ZIGZAG, PAT_DIVE, PAT_LOOP, PAT_DART };
typedef struct hornet {
    int32_t x, y;               /* Q16 centre */
    int32_t v;                  /* its speed (Q16 px/frame) */
    int32_t vx, vy;             /* its last move (the bot's forecast) */
    int32_t y0;                 /* its lane (Q16) */
    int32_t dx, dy;             /* a dive's or a dash's velocity; a loop's centre */
    int16_t t;                  /* frames since its pattern started (< 0: still gliding in) */
    int16_t amp, ph, aux, aux_t;/* the pattern's amplitude, phase, phase (0, 1, 2...) and when it began */
    uint8_t on, kind, pat, hp, target, flash, homing, pad;
} hornet;
enum { FX_FALL, FX_POINTS, FX_SPLAT, FX_FIZZLE, FX_PUFF };
typedef struct effect { int32_t x, y, vx, vy; int16_t value, t; uint8_t kind, on, sub, pad; } effect;
/* one player's input for a frame: dy -1 aims up, 1 down; spit (A pressed); touch 1 a finger went down at (tx, ty)
 * (stage px): aim there and spit; touch 2 a finger held there: the head follows it; any: a button (the first press
 * of a ready=1 run) */
typedef struct pin { int dy, spit, touch, tx, ty, any; } pin;

enum { MSG_NONE, MSG_DART, MSG_BIG, MSG_SWARM };
typedef struct world {
    llama l[MAX_PLAYERS];
    gob gobs[MAX_PLAYERS * GOBS_MAX];
    hornet hn[MAX_HORNETS];
    effect fx[FX_MAX];
    int fx_next;
    int wave_t, waves;          /* frames to the next wave; waves sent */
    int heat;                   /* the ramp (DESIGN.md) */
    int sky;                    /* 0..768: day, sunset, dusk, night (eases toward the heat's) */
    int seen;                   /* the hornets met already (1 << MSG_*) */
    int msg, msg_t;
    int freeze;
    int sting_kind, sting_pat;  /* the last hornet that stung (the log) */
    rs_rng rng;
    uint32_t seed;
    int players, t, started;
    int events[MAX_PLAYERS];    /* EV_* of the last step */
    int wevents;                /* WEV_* of the last step */
} world;
enum { EV_SPIT = 1, EV_POP = 2, EV_HIT = 4, EV_OUT = 8, EV_START = 16, EV_BIG_HIT = 32 };
enum { WEV_WAVE = 1, WEV_DART = 2, WEV_MSG = 4, WEV_LAND = 8, WEV_ENTER = 16 };

void world_init(world *w, int players, uint32_t seed);
void world_step(world *w, const pin in[MAX_PLAYERS]);
int  world_running(const world *w);             /* someone still plays */
int  world_all_out(const world *w);
int  world_team_score(const world *w);
int  isin1024(int a);                           /* sine, Q14 */
#define icos1024(a) isin1024((a) + 256)
int  aim_frame(int aim);                        /* the head's frame (0 .. HEAD_FRAMES - 1) */
/* the llama's geometry at an aim (stage px, Q16): the shoulder, the neck's top, the head's centre, the mouth */
void llama_neck(int p, int aim, int32_t *sx, int32_t *sy, int32_t *bx, int32_t *by);
void llama_head(int p, int aim, int32_t *x, int32_t *y);
void llama_mouth(int p, int aim, int32_t *x, int32_t *y);
int  llama_aim_at(int p, int x, int y);         /* the aim whose spit passes nearest stage point (x, y) */
int  hornet_radius(int kind);
hornet *hornet_spawn(world *w, int kind, int pat, int lane, int extra);   /* one hornet beyond the screen (NULL: full) */
enum { W_SINGLE, W_PAIR, W_DIVE, W_LOOP, W_DART, W_SWARM, W_BIG, W_KINDS };
int  world_wave(world *w, int kind);            /* a wave now (the director's; tests); returns its cost */
/* the bot: the input it gives player p (skill 0 plays as well as it can; 1..10 a human-like player, worse) */
pin  bot_decide(world *w, int p, int skill);

/* ---- draw.c -------------------------------------------------------------------------------------------------------- */
enum { ST_TITLE, ST_PLAY, ST_DEAD, ST_OVER };      /* no "get ready": the title is the only menu */
void draw_init(void);
void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused, int touch_hint);
void draw_state(void);
void draw_state_loaded(void);       /* widescreen: after a state saved at another width */

/* ---- sound (main.c) ------------------------------------------------------------------------------------------------ */
enum { SFX_SPIT, SFX_POP, SFX_STING, SFX_BIG_HIT, SFX_DART, SFX_LAND, SFX_HUM, SFX_SPLAT, SFX_COUNT };
#define SFX_HOUSE SFX_COUNT                                    /* first slot of the house sounds (ha_init) */

#endif
