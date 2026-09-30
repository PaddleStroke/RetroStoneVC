/*
 * @NAME@: shared declarations.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/@ID@/LICENSE.
 *
 * play.c is the game rules (no drawing, no sound: deterministic, testable alone); draw.c is the video,
 * main.c the flow (title, get ready, play, game over), input, save RAM, options and save states.
 * Made from games/_template (tools/new_game.py): the house style kit is games/common (docs/art-direction.md).
 */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "rs.h"

#define MAX_PLAYERS 2

/* ---- tuning: every gameplay number is here ------------------------------------------------------------------- */
#define GROUND_Y      192       /* the ground line (feet), screen y */
#define HERO_X        96        /* player 1's centre, screen x */
#define P2_OFFSET_X   (-40)     /* player 2 runs this far behind */
#define GRAVITY       (Q16_ONE * 30 / 100)
#define JUMP_VY       (-Q16_ONE * 55 / 10)
#define SCROLL_SPEED  2         /* px per frame */
#define BLOCK_W       16
#define BLOCK_H       16
#define BLOCK_GAP_MIN 110       /* px between blocks */
#define BLOCK_GAP_RND 120
#define BLOCK_MAX     8
#define PANEL_DELAY   40        /* frames between the hit and the game-over panel */
#define RETRY_LOCK    36        /* frames the panel ignores the button (no accidental retry) */
#define HIT_FREEZE    8         /* hit-stop frames */
#define READY_BOB     4         /* get-ready / title bob amplitude, px */
#define MEDAL_SCORES  {10, 20, 30, 40}           /* bronze, silver, gold, pearl */

#define Q16_ONE 65536

/* ---- play.c: the rules ------------------------------------------------------------------------------------------ */
enum hero_state { HS_READY, HS_RUN, HS_HIT, HS_DOWN, HS_OFF };
typedef struct hero {
    int state, t;               /* state, frames in it */
    int32_t y, vy;              /* Q16: feet y, vertical speed */
    int x;                      /* screen x of the centre */
    int score, next_block;      /* points; the next block to score */
    int air_t, land_t;          /* frames since the jump / the landing (squash and stretch) */
} hero;

typedef struct world {
    int32_t scroll;             /* px travelled */
    int bx[BLOCK_MAX];          /* the blocks' world x (left edge), in order */
    int bidx[BLOCK_MAX];        /* their index along the course */
    int nb, next_index, next_x;
    rs_rng rng;
    uint32_t seed;
    hero h[MAX_PLAYERS];
    int players, t, started;
    int events[MAX_PLAYERS];    /* EV_* of the last step */
} world;
enum { EV_JUMP = 1, EV_SCORE = 2, EV_HIT = 4, EV_LAND = 8, EV_START = 16 };

void world_init(world *w, int players, uint32_t seed);
void world_step(world *w, const int press[MAX_PLAYERS]);
int  world_running(const world *w);            /* someone still runs */
int  world_all_down(const world *w);
int  world_block_ahead(const world *w, int player);   /* px to the next block's left edge, -1 = none */
int  hero_on_ground(const hero *h);

/* ---- draw.c -------------------------------------------------------------------------------------------------------- */
enum { ST_TITLE, ST_READY, ST_PLAY, ST_DEAD, ST_OVER };
void draw_init(void);
void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused);
void draw_state(void);

/* ---- sound (main.c) ------------------------------------------------------------------------------------------------ */
enum { SFX_JUMP, SFX_SCORE, SFX_HIT, SFX_LAND, SFX_COUNT };     /* the game's own sounds; the house set follows */
#define SFX_HOUSE SFX_COUNT                                    /* first slot of the house sounds (ha_init) */

/* ---- bot (main.c): the test hook ----------------------------------------------------------------------------------- */
int bot_decide(const world *w, int player);

#endif
