/*
 * Unicorn Hop: shared declarations.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/unicornhop/LICENSE.
 *
 * play.c is the game rules (no drawing, no sound: deterministic, testable alone; the bot too); draw.c is the video,
 * main.c the flow (the title with the players joining, play, game over), input, sound, save RAM, options and states.
 * Made from games/_template (tools/new_game.py): the house style kit is games/common (docs/art-direction.md).
 *
 * Heights: an "alt" is a height in pixels above the start cloud's top (up = +). A level is one cloud: level k's top
 * is at alt k * LEVEL_H. The camera puts the altitude cam_px() at the screen line STAND_Y.
 */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "rs.h"

#define MAX_PLAYERS 4                   /* the house kit's HU_MAX_PLAYERS: 4 pads */
#define Q16_ONE 65536

/* ---- tuning: every gameplay number is here (DESIGN.md "Rules") ------------------------------------------------ */
/* the jump */
#define LEVEL_H       64        /* px between two clouds: a normal jump reaches exactly the next one */
#define GRAVITY       13107     /* Q16 px/frame^2 (0.20) */
#define JUMP_VY       370727    /* Q16 px/frame (5.66): the apex 80 px up (a level + 16), it lands 41 frames later */
#define BIG_JUMP_VY   497385    /* Q16 (7.59): from a pink cloud, the apex 144 px up (two levels + 16), 51 frames */
#define LAND_MARGIN   5         /* px: the hooves' centre may be this far past a cloud's end and still land */
#define SETTLE_IN     6         /* px: after landing, the unicorn steps (1 px a frame) to stand this far inside */
#define FALL_FRAMES   90        /* a fall lasts at most this long (it usually leaves the screen sooner) */
#define HIT_FREEZE    8         /* the zap's hit-stop, frames */
/* the play band: the clouds' ends stay inside the central 320 columns (a wide screen only shows more sky) */
#define BAND_L        8
#define BAND_R        312
#define CENTER_X      160
/* the camera */
#define STAND_Y       200       /* the screen line of the camera's cloud top (the leader's, 1 player) */
#define CAM_LAG       2         /* 2-4 players: the leader stands at most this many levels above the camera's */
#define CAM_EASE      20        /* /256 of the distance each frame: the camera eases up after a landing */
#define OUT_BELOW     44        /* px: a unicorn whose hooves are this far below the camera line is off the screen */
#define TITLE_CAM     32        /* px: the title looks a little higher (the castle sinks under the player slots) */
/* a phone held upright (DESIGN.md "Tall screen"): a screen of ZOOM_H lines (rs_screen_zoom), its height filling the
 * display's, the sides cropped (222 columns shown on a 9:21 phone, 240 on a 9:19.5 one) and centred on the unicorn
 * (1 player); the stage low, TALL_BELOW lines under it, the levels ahead above it up to the screen's top */
#define ZOOM_H        520
#define TALL_BELOW    40
/* the clouds: widths (32 + 16 px per middle piece) and speeds, by level */
#define START_W       160       /* the start cloud (level 0), still */
#define FIRST_W       128       /* level 1: wide, right above the start (the start press is the first jump) */
#define EASY_LEVELS   10        /* levels 1-9: 112 or 96 px wide, slow (a 7-year-old climbs them) */
#define RAMP_LEVELS   60        /* the difficulty ramps up to level 60, then stays */
#define SPEED_MIN     29491     /* Q16 px/frame (0.45) at level 1 */
#define SPEED_EASY    45875     /* (0.70) at level 10 */
#define SPEED_MAX     111411    /* (1.70) at level 60 */
#define SPEED_ALT     22        /* +-%: the levels alternate slower and faster (never two in step) ... */
#define SPEED_JITTER  8         /* +-%: ... and each cloud has its own speed; the directions alternate too */
/* the special clouds and the gaps */
#define PINK_FROM     8         /* the pink cotton-candy cloud: jumping from it bounces two levels */
#define PINK_CHANCE   7         /* 1 level in N (never two specials in a row) */
#define RAIN_FROM     15        /* the grey rain cloud: drips once landed on, melts RAIN_MELT frames later */
#define RAIN_CHANCE   6
#define RAIN_MELT     90        /* 1.5 s (the cloud above it moves with it: jump soon!) */
#define RAIN_REFORM   150       /* frames after melting before it puffs back (for the players behind) */
#define THUNDER_FROM  20        /* the thunder cloud drifting in a gap: touching it while jumping = a zap */
#define THUNDER_P0    16        /* its chance in % at THUNDER_FROM ... */
#define THUNDER_P1    34        /* ... rising to this at RAMP_LEVELS */
#define THUNDER_SPEED 26214     /* Q16 px/frame (0.40) +- jitter */
#define GAP_Y         38        /* px above a cloud top: the middle of the gap (stars, thunder clouds) */
#define STAR_FROM     2
#define STAR_CHANCE   35        /* % of the gaps without thunder */
#define STAR_POINTS   2         /* a star's bonus */
#define RAINBOW_EVERY 25        /* levels: a rainbow celebration ... */
#define RAINBOW_BONUS 10        /* ... and its bonus */
#define RAINBOW_SHOW  300       /* frames the rainbow stays */
/* the flow */
#define PANEL_DELAY   40        /* frames between the last unicorn's fall and the game-over panel */
#define RETRY_LOCK    36        /* frames the panel ignores the button (no accidental retry) */
#define READY_BOB     4         /* the title unicorn's bob amplitude, px */
#define BOT_START     20        /* frames on the title before the bot starts the run */
#define BOT_SAFE      6         /* px: the bot lands at least this far inside a cloud */
#define MEDAL_SCORES  {10, 30, 60, 120}          /* bronze, silver, gold, pearl (1 player; tuned with botskill) */

/* ---- play.c: the rules ------------------------------------------------------------------------------------------ */
enum cloud_type { CL_START, CL_NORMAL, CL_PINK, CL_RAIN };
#define RING 16                 /* the levels kept (a ring: the screen shows about 5) */
typedef struct cloud {
    int32_t index;              /* the level this slot holds */
    int32_t x, vx;              /* Q16 centre x (stage), speed */
    int16_t w;                  /* px: 32 + 16 per middle piece */
    uint8_t type, look;         /* CL_*; the middle pieces' pattern */
    int16_t rain_t;             /* rain: frames since the first landing (-1: dry) */
    int16_t gone_t;             /* rain: frames since it melted (-1: there) */
    /* the gap above this cloud (between it and the next level) */
    uint8_t thunder, star, star_taken, pad;
    int32_t tx, tvx;            /* the thunder cloud: Q16 centre x, speed */
    int16_t sx, star_by;        /* the star's x; who took it */
    int32_t star_t;             /* the world frame it was taken */
} cloud;

enum hero_state { HS_READY, HS_STAND, HS_AIR, HS_ZAP, HS_FALL, HS_DOWN, HS_OFF };
typedef struct hero {
    int state, t;               /* state, frames in it */
    int level, target;          /* the cloud it stands on (or jumped from); the level it aims at */
    int32_t dy, vy;             /* Q16: the hooves' height above level's cloud top; vertical speed (up +) */
    int32_t x, ox;              /* Q16: centre x; while riding, the offset from the cloud's centre */
    int score, height, stars, rainbows;
    int air_t, land_t, face;    /* frames since the take-off / the landing; facing (1 right, -1 left) */
    int big;                    /* this jump is a pink bounce */
    int why;                    /* why it fell: FALL_MISS, FALL_ZAP, FALL_MELT, FALL_OUT */
} hero;
enum { FALL_MISS = 1, FALL_ZAP, FALL_MELT, FALL_OUT };

typedef struct world {
    cloud c[RING];
    int top;                    /* levels made: 0..top-1 */
    int32_t cam;                /* Q8 px: the altitude shown at STAND_Y */
    int cam_level;              /* the level the camera aims at */
    rs_rng rng;
    uint32_t seed;
    hero h[MAX_PLAYERS];
    int players, t, started;
    int last_special;           /* the level of the last special cloud (generation) */
    int rainbow_level, rainbow_t;   /* the last rainbow reached; frames since (-1: none showing) */
    int events[MAX_PLAYERS];    /* EV_* of the last step */
    int wevents;                /* WEV_* of the last step */
} world;
enum { EV_JUMP = 1, EV_LAND = 2, EV_STAR = 4, EV_MISS = 8, EV_ZAP = 16, EV_MELT = 32, EV_RAINBOW = 64,
       EV_START = 128, EV_BIG = 256, EV_OUT = 512, EV_DRIP = 1024 };
enum { WEV_MELT = 1, WEV_REFORM = 2 };

void world_init(world *w, int players, uint32_t seed);
void world_step(world *w, const int press[MAX_PLAYERS]);
int  world_running(const world *w);            /* someone still climbs */
int  world_all_down(const world *w);
const cloud *world_cloud(const world *w, int level);    /* NULL: not kept */
int  cloud_there(const cloud *c);               /* not melted */
int  hero_alt(const hero *h);                   /* the hooves' altitude, px */
int  cam_px(const world *w);
int  jump_frames(int big);                      /* frames from the take-off to the landing */
/* the bot: 1 = press now; stop = 1: miss on purpose (the tests' botstop); err: px it misjudges the landing by */
int  bot_decide(const world *w, int player, int stop, int err);

/* ---- draw.c -------------------------------------------------------------------------------------------------------- */
enum { ST_TITLE, ST_PLAY, ST_DEAD, ST_OVER };      /* no "get ready": the title is the only menu */
void draw_init(void);
void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused);
void draw_state(void);
void draw_state_loaded(void);       /* widescreen: after a state saved at another width */

/* ---- sound (main.c) ------------------------------------------------------------------------------------------------ */
enum { SFX_JUMP, SFX_BIG, SFX_LAND, SFX_STAR, SFX_MISS, SFX_ZAP, SFX_DRIP, SFX_MELT, SFX_RAINBOW, SFX_COUNT };
#define SFX_HOUSE SFX_COUNT                                    /* first slot of the house sounds (ha_init) */

#endif
