/*
 * Hamster Havoc: shared declarations.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/hamsterhavoc/LICENSE.
 *
 * play.c is the game rules (no drawing, no sound: deterministic, testable alone; the bot too); draw.c is the video,
 * main.c the flow (the title with the players joining, play, game over), input (pads and touch), sound, save RAM,
 * options and states. Made from games/_template (tools/new_game.py): the house style kit is games/common
 * (docs/art-direction.md).
 *
 * Coordinates: stage pixels (the central 320 columns, y down); positions that move by fractions are Q16.
 */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "rs.h"

#define MAX_PLAYERS 4                   /* the house kit's HU_MAX_PLAYERS: 4 pads */
#define Q16_ONE 65536

/* ---- tuning: every gameplay number is here (DESIGN.md "Rules") ------------------------------------------------ */
/* the hamsters' ships */
#define SHIP_Y        192       /* the ship sprite's top line (32x24); its hit box: centre x -10..+10, lines 198..213 */
#define SHIP_HALF     10
#define SHIP_MIN_X    20        /* the ship's centre stays in 20..300 */
#define SHIP_MAX_X    300
#define SHIP_SPEED    98304     /* Q16 px/frame (1.5): the d-pad, and a touch target's slide */
#define SHIP_GAP      48        /* 2-4 players: the ships start this far apart */
/* the sunflower seeds */
#define SEED_SPEED    327680    /* Q16 px/frame (5) up */
#define SEED_TOP      22        /* a seed above this line is gone */
#define MAX_SEEDS     3         /* per player: 1 on screen (the classic rule); double seeds 2, rapid seeds 3 */
#define DOUBLE_DX     5         /* double seeds: two side by side, this far from the centre */
#define RAPID_SPEED   458752    /* rapid seeds: 7 px/frame ... */
#define RAPID_GAP     10        /* ... and at most one every 10 frames */
/* the formation of space mice */
#define ROWS          5
#define COLS          11
#define COL_W         20        /* px between two columns (a mouse is a 16x16 sprite, its body 12x12) */
#define ROW_H         16        /* px between two rows */
#define FORM_X0       52        /* the formation's left column at the start (centred: 11 columns = 216 px) */
#define FORM_Y0       48        /* its top row at wave 1 */
#define WAVE_LOWER    6         /* each new wave starts this much lower ... */
#define WAVE_LOWER_MAX 30       /* ... at most */
#define BAND_L        8         /* the formation turns and drops when a mouse would leave 8..312 */
#define BAND_R        312
#define MARCH_DX      2         /* px per step sideways */
#define MARCH_DY      8         /* px per drop at an edge */
#define MARCH_FULL    32        /* frames between two steps with every mouse of the wave there ... */
#define MARCH_MIN     2         /* ... down to this with a few left (in proportion to the mice left) */
#define MARCH_LAST_DX 3         /* the very last mouse: 3 px a step, a step every frame */
#define MARCH_WAVE    5         /* % faster each wave ... */
#define MARCH_WAVE_MAX 8        /* ... for 8 waves (then 40% faster than wave 1) */
#define INVADE_Y      192       /* a mouse's body reaching the ships' line: the mice landed, game over */
#define ROW_POINTS    {30, 20, 20, 10, 10}       /* by row, top to bottom (three kinds of mice) */
/* the mice's crumbs and peas */
#define BOMB_MAX      10        /* slots (the formation and the mothership) */
#define BOMB_LIMIT    3         /* the formation's shots on screen at once, +1 per extra player */
#define BOMB_RELOAD   48        /* frames between two drops at wave 1 ... */
#define BOMB_RELOAD_WAVE 3      /* ... 3 fewer each wave ... */
#define BOMB_RELOAD_MIN 24      /* ... down to this */
#define BOMB_SPEEDS   {65536, 104858, 81920}     /* Q16 px/frame: the crumb 1.0 (aimed), the pea 1.6, the jelly 1.25 */
#define BOMB_WAVE     4         /* % faster each wave, for 10 waves */
#define MOON_Y        214       /* a shot reaching this line splats on the moon */
/* the cheese shelters */
#define SHELTERS      4
#define SHELTER_W     24
#define SHELTER_H     16
#define SHELTER_Y     168
#define SHELTER_XS    {40, 112, 184, 256}        /* left edges (8-aligned: drawn on the moon's BG layer) */
#define CRATER_R      2         /* a shot bites a crater of this radius (plus two nibbles) */
/* lives */
#define LIVES         3
#define LIVES_MAX     9
#define EXTRA_LIFE_AT 1500      /* points: one extra life (each player, once) */
#define HIT_FREEZE    8         /* the hit-stop, frames */
#define POP_FRAMES    60        /* the popped ship's bonk ... */
#define HIT_PAUSE     60        /* ... the mice hold their fire and march meanwhile (their shots pop) */
#define RESPAWN       110       /* frames from the hit to the new ship ... */
#define INVUL_FRAMES  120       /* ... which blinks, untouchable, this long */
/* the bonus saucer */
#define SAUCER_Y      30
#define SAUCER_FIRST  900       /* frames into a wave before the first saucer (15 s) ... */
#define SAUCER_EVERY  1200      /* ... then one every 20 s, while 8 mice or more are left */
#define SAUCER_MIN_MICE 8
#define SAUCER_SPEED  65536     /* Q16 px/frame (1) */
#define SAUCER_POINTS {50, 100, 150, 300}        /* the mystery points ... */
#define SAUCER_ODDS   {30, 40, 20, 10}           /* ... and their odds in % */
#define POWER_CHANCE  50        /* % of the shot saucers that drop a power-up */
#define POWER_SPEED   49152     /* Q16 px/frame (0.75): the capsule falls; a ship catches it */
#define POWER_FRAMES  600       /* double or rapid seeds last 10 s */
#define SHIELD_FRAMES 1200      /* the bubble shield: one hit, or 20 s */
/* the mothership (every 4th wave) */
#define BOSS_EVERY    4
#define BOSS_HP       24        /* hits, +8 per extra player */
#define BOSS_HP_PLAYER 8
#define BOSS_Y        40        /* its sprite's top line (64x32), bobbing 3 px */
#define BOSS_MIN_X    48        /* its centre sails in 48..272 */
#define BOSS_MAX_X    272
#define BOSS_SPEED    49152     /* Q16 px/frame (0.75) */
#define BOSS_FIRE     60        /* frames between two spreads of three peas ... */
#define BOSS_FIRE_ANGRY 42      /* ... below half its hits (and an aimed crumb every other spread) */
#define BOSS_SPREAD   32768     /* Q16 px/frame (0.5): the side peas' drift */
#define BOSS_BOMB_VY  81920     /* Q16 px/frame (1.25) */
#define BOSS_HIT_POINTS 10
#define BOSS_POINTS   500       /* to the hamster who pops it */
/* the waves */
#define INTRO_FRAMES  90        /* WAVE n: the formation flies in, no seeds yet */
#define CLEAR_FRAMES  90        /* the pause after a wave */
#define FX_MAX        24        /* the pops, splats and points shown */
/* the flow */
#define PANEL_DELAY   40        /* frames between the last ship's pop and the game-over panel */
#define RETRY_LOCK    36        /* frames the panel ignores the button (no accidental retry) */
#define READY_BOB     4         /* the title ship's bob amplitude, px */
#define BOT_START     20        /* frames on the title before the bot starts the run */
#define MSG_FRAMES    120       /* EXTRA LIFE!, the power-ups' names */
#define MEDAL_SCORES  {1500, 4000, 8000, 15000}  /* bronze, silver, gold, pearl (1 player; tuned with botskill) */

/* ---- play.c: the rules ------------------------------------------------------------------------------------------ */
enum hero_state { SH_READY, SH_PLAY, SH_POP, SH_GONE, SH_DOWN, SH_OFF };
enum { PW_NONE, PW_DOUBLE, PW_RAPID, PW_SHIELD, PW_KINDS };
typedef struct hero {
    int state, t;               /* state, frames in it */
    int32_t x;                  /* Q16 centre x */
    int lives, score, extra;    /* extra: the extra life was given */
    int invul;                  /* frames left blinking (untouchable) */
    int power, power_t;         /* PW_DOUBLE or PW_RAPID, frames left */
    int shield_t;               /* the bubble shield's frames left (0: none) */
    int gap;                    /* frames before the next rapid seed */
    int face, move;             /* the last direction; this frame's (-1, 0, 1) */
    int fire_t;                 /* frames since the last seed (the launcher's recoil) */
    int shots, pops;            /* seeds fired, mice popped */
} hero;
typedef struct seed { int32_t x, y, vy; int on; } seed;
enum { BK_CRUMB, BK_PEA, BK_JELLY, BK_KINDS };
typedef struct bomb { int32_t x, y, vx, vy; int on, kind; } bomb;
enum { FX_POP, FX_SPLAT, FX_POINTS, FX_BIGPOP, FX_BUBBLE };
typedef struct effect { int16_t x, y, value; uint8_t kind, t, on, pad; } effect;
/* one player's input for a frame: the d-pad's direction (-1, 0, 1), fire held, a touch target (stage x; -1: none) */
typedef struct pin { int dir, fire, target; } pin;

enum { PH_READY, PH_INTRO, PH_PLAY, PH_CLEAR, PH_OVER };
enum { MSG_NONE, MSG_EXTRA, MSG_DOUBLE, MSG_RAPID, MSG_SHIELD };
typedef struct world {
    /* the formation */
    uint8_t alive[ROWS][COLS];
    int fx, fy, fdir, fstep_t, fframe, count0, alive_n, layout, beat;
    /* the shelters: bit x (0..23) of row y */
    uint32_t shelter[SHELTERS][SHELTER_H];
    seed seeds[MAX_PLAYERS][MAX_SEEDS];
    bomb bombs[BOMB_MAX];
    effect fxs[FX_MAX];
    int fxs_next;
    int bomb_t, bomb_k, jelly_col;
    /* the saucer and its power-up capsule */
    int saucer_on, saucer_t, saucer_dir, saucer_n;
    int32_t saucer_x;
    int power_on, power_kind;
    int32_t power_x, power_y;
    /* the mothership */
    int boss_on, boss_hp, boss_hp0, boss_dir, boss_fire_t, boss_flash, boss_t, boss_volley;
    int32_t boss_x;
    /* the run */
    int wave, phase, phase_t, pause_t, freeze, landed;
    rs_rng rng;
    uint32_t seed;
    hero h[MAX_PLAYERS];
    int players, t, started;
    int msg, msg_t, msg_player;
    int events[MAX_PLAYERS];    /* EV_* of the last step */
    int wevents;                /* WEV_* of the last step */
} world;
enum { EV_FIRE = 1, EV_POP = 2, EV_HIT = 4, EV_DOWN = 8, EV_EXTRA = 16, EV_POWER = 32, EV_SAUCER = 64,
       EV_SHIELD = 128, EV_BOSS_HIT = 256, EV_BOSS_DOWN = 512, EV_START = 1024 };
enum { WEV_BEAT = 1, WEV_BOMB = 2, WEV_SAUCER = 4, WEV_WAVE = 8, WEV_CLEAR = 16, WEV_LANDED = 32, WEV_SPLAT = 64,
       WEV_SHELTER = 128, WEV_BOSS_FIRE = 256 };

void world_init(world *w, int players, uint32_t seed);
void world_start_wave(world *w, int wave);      /* tests, --opt wave=N */
void world_step(world *w, const pin in[MAX_PLAYERS]);
int  world_running(const world *w);             /* someone still plays */
int  world_all_down(const world *w);
int  world_is_boss(const world *w);
int  march_interval(const world *w);            /* frames between two steps now */
int  mouse_x(const world *w, int col);          /* a mouse's sprite left edge */
int  mouse_y(const world *w, int row);          /* its top line */
int  mouse_points(int row);
int  shelter_solid(const world *w, int x, int y);   /* a stage pixel of a shelter is still there */
int  boss_y(const world *w);
int  bomb_speed(const world *w, int kind);
/* the bot: the input it gives player p (skill 0 plays as well as it can; 1..10 a human-like player, worse) */
pin  bot_decide(const world *w, int p, int skill);

/* ---- draw.c -------------------------------------------------------------------------------------------------------- */
enum { ST_TITLE, ST_PLAY, ST_DEAD, ST_OVER };      /* no "get ready": the title is the only menu */
void draw_init(void);
void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused);
void draw_state(void);
void draw_state_loaded(void);       /* widescreen: after a state saved at another width */

/* ---- sound (main.c) ------------------------------------------------------------------------------------------------ */
enum { SFX_SEED, SFX_POP, SFX_HIT, SFX_BEAT, SFX_BOMB, SFX_SAUCER, SFX_SAUCER_POP, SFX_POWER, SFX_SHIELD, SFX_SPLAT,
       SFX_BOSS_HIT, SFX_BOSS_DOWN, SFX_COUNT };
#define SFX_HOUSE SFX_COUNT                                    /* first slot of the house sounds (ha_init) */

#endif
