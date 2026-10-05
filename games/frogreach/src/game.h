/* Frog Reach. MIT, (c) 2026 Pierre-Louis Boyer (8BCraft). */
#ifndef GAME_H
#define GAME_H
#include <stdint.h>
#include "rs.h"
#define MAX_PLAYERS 4
#define Q16_ONE 65536
#define PANEL_DELAY 40
#define RETRY_LOCK 36
#define BOT_START 20
#define MEDAL_SCORES {100,250,500,1000}
enum hero_state { HS_READY,HS_RUN,HS_HIT,HS_DOWN,HS_OFF };
enum { MODE_WAIT,MODE_CHARGE,MODE_LEAP };
enum { EV_JUMP=1,EV_SCORE=2,EV_HIT=4,EV_LAND=8,EV_START=16,EV_APPLE=32 };
enum { ST_TITLE,ST_PLAY,ST_DEAD,ST_OVER };
typedef struct hero {
 int state,t,x,score,air_t,land_t;
 int32_t y,vx,vy;
 int64_t wx;
 int grounded,held,combo,smooth_t,apples,apple_id,bonus,launches,spill;
 int mode,anchor,length,wait,goal_x,start_x,good,perfect,leap_t,pad_offset,goal_offset,start_guard;
} hero;
typedef struct world {
 hero h[4]; int players,t,started,scroll;uint32_t seed;int events[4];
} world;
void world_init(world *w,int players,uint32_t seed);
void world_step(world *w,const int held[4]);
int world_running(const world *w);
int world_all_down(const world *w);
int bot_decide(const world *w,int p);
void perch_pos(const world *w,int id,int t,int *x,int *width);
int pad_life(int anchor);
int tongue_speed(int anchor);
void draw_init(void);
void draw_frame(const world *w,int state,int st_t,int best,int new_best,int paused);
void draw_state(void);
void draw_relayout(void);   /* after a state of another width was loaded: this width's maps (widescreen) */
enum { SFX_JUMP,SFX_SCORE,SFX_HIT,SFX_LAND,SFX_COUNT };
#define SFX_HOUSE SFX_COUNT
#endif
