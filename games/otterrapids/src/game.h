/*
 * Otter Rapids: shared declarations.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/otterrapids/LICENSE.
 *
 * play.c is the game rules (no drawing, no sound: deterministic, testable alone); draw.c is the video,
 * main.c the flow (the title with the players joining, play, game over), input, save RAM, options and save states.
 * Made from games/_template (tools/new_game.py): the house style kit is games/common (docs/art-direction.md).
 */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "rs.h"

#define MAX_PLAYERS 4                   /* the house kit's HU_MAX_PLAYERS: 4 pads */

#define Q16_ONE 65536
#define PANEL_DELAY 40
#define RETRY_LOCK 36
#define HIT_FREEZE 8
#define BOT_START 20
#define MEDAL_SCORES {100,250,500,1000}
#define HERO_LINE 166
#define ROW_GAP 240
#define FIRST_ROW 340
enum hero_state { HS_READY, HS_RUN, HS_HIT, HS_DOWN, HS_OFF };
typedef struct hero {
 int state,t,x,score,fish,collected,next_row;
 int32_t xx,vx,y;
} hero;
typedef struct world {
 hero h[4]; int players,t,started,scroll; int64_t travel;
 uint32_t seed; int events[4]; struct {int active,x,distance,t;} bite[4];
} world;
enum { EV_JUMP=1,EV_SCORE=2,EV_HIT=4,EV_LAND=8,EV_START=16 };
enum { ROCK,LOG,WHIRLPOOL,CROCODILE };
typedef struct river_row {int id,distance,center,count,kind[3],x[3],rx[3],ry[3],at[3],fish;} river_row;
void world_init(world*,int,uint32_t);
void world_step(world*,const int[4]);
int world_running(const world*);
int world_all_down(const world*);
void course_row(const world*,int,river_row*);
int course_index(int);
int player_y(int,int);
int32_t river_speed(int);
int river_current(int,int);
void river_banks(const world*,int,int*,int*);
int river_island(const world*,int,int*);
int river_route(const world*,int,int);
int fish_allowed(const world*,int);
int hero_distance(const world*,int);
int bot_decide(const world*,int);

/* ---- draw.c -------------------------------------------------------------------------------------------------------- */
enum { ST_TITLE, ST_PLAY, ST_DEAD, ST_OVER };      /* no "get ready": the title is the only menu */
void draw_init(void);
void draw_frame(const world *w, int state, int st_t, int best, int new_best, int paused);
void draw_state(void);

/* ---- sound (main.c) ------------------------------------------------------------------------------------------------ */
enum { SFX_JUMP, SFX_SCORE, SFX_HIT, SFX_LAND, SFX_COUNT };     /* the game's own sounds; the house set follows */
#define SFX_HOUSE SFX_COUNT                                    /* first slot of the house sounds (ha_init) */

/* ---- bot (main.c): the test hook ----------------------------------------------------------------------------------- */
int bot_decide(const world *w, int player);

#endif

