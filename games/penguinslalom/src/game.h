#ifndef GAME_H
#define GAME_H
#include <stdint.h>
#include "rs.h"
#define MAX_PLAYERS 4
#define MAX_BEARS 2
#define Q16_ONE 65536
#define PANEL_DELAY 40
#define RETRY_LOCK 36
#define BOT_START 20
#define HERO_LINE 112
#define GATE_FIRST 340
#define GATE_SPACING 275
#define SKI_RADIUS 8
#define SKI_FORWARD_RADIUS 14
#define MEDAL_SCORES {100,250,500,1000}
enum {HS_READY,HS_RUN,HS_HIT,HS_DOWN,HS_OFF};
enum {EV_JUMP=1,EV_SCORE=2,EV_HIT=4,EV_LAND=8,EV_START=16,EV_APPLE=32};
enum {ST_TITLE,ST_PLAY,ST_DEAD,ST_OVER};
enum {PROP_ROCK,PROP_TREE,PROP_HOLE,PROP_GOAT,PROP_COUNT,PROP_BEAR=PROP_COUNT};
typedef struct hero {int state,t,x,score,apples,launches,anchor,air_t,smooth_t,distance,pressure,combo,bonus,reason;int32_t xx,y,vx,vy;int64_t wx;int trail_x[32],trail_d[32],trail_head,trail_count;} hero;
typedef struct sliding_bear {int active,id,x;int64_t distance,previous;} sliding_bear;
typedef struct world {hero h[4];sliding_bear bears[MAX_BEARS];int players,t,started,scroll,bear_wait,bear_id;int64_t travel;uint32_t seed;int events[4];} world;
typedef struct row {int id,distance,center,gap,kind,left,right,rx,ry;} row;
typedef struct slope_obstacle {int id,kind,x,distance,rx,ry,pass_side;} slope_obstacle;
void course_row(const world *w,int id,row *r);
int course_index(const world *w,int distance);
void course_obstacle(const world *w,int id,int travel,int t,slope_obstacle *o);
void bear_obstacle(const world *w,int slot,int ticks,slope_obstacle *o);
int obstacle_hits(const slope_obstacle *before,const slope_obstacle *after,int x0,int x1,int d0,int d1);
int player_y(int players,int p);
int player_offset(int players,int p);
int course_speed(int distance);
void world_init(world *w,int n,uint32_t seed);
void world_step(world *w,const int input[4]);
int world_running(const world *w);
int world_all_down(const world *w);
int bot_decide(const world *w,int p);
void draw_init(void);
void draw_frame(const world *w,int state,int st_t,int best,int new_best,int paused);
int title_slot_row(void);                                  /* the title's player slots: row 25, 24 when its text needs more room */
void draw_state(void);
enum {SFX_JUMP,SFX_SCORE,SFX_HIT,SFX_LAND,SFX_COUNT};
#define SFX_HOUSE SFX_COUNT
#endif
