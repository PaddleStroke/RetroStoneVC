#ifndef GAME_H
#define GAME_H
#include <stdint.h>
#include "rs.h"
#define MAX_PLAYERS 4
#define Q16_ONE 65536
#define GROUND_Y 208
#define TRAY_Y 173
#define PILE_MAX 9
#define ITEM_MAX 8
#define PANEL_DELAY 40
#define RETRY_LOCK 36
#define BOT_START 20
#define MEDAL_SCORES {40,100,200,400}
enum hero_state {HS_READY,HS_RUN,HS_HIT,HS_DOWN,HS_OFF};
enum { IN_LEFT=1,IN_RIGHT=2,IN_START=8 };
enum {EV_JUMP=1,EV_SCORE=2,EV_HIT=4,EV_LAND=8,EV_START=16};
typedef struct biscuit {int32_t pos,vel; int type;} biscuit;
typedef struct hero {int state,t,x,score,lives,load,misses,tilt,catch_t,spill_t,spill_type; int32_t xx,vx,y,spill_x,spill_y,spill_vx; biscuit pile[PILE_MAX];} hero;
typedef struct drop {int active,id,x,age,bad,done,claimed; int32_t y,vy;} drop;
typedef struct world {hero h[4];drop items[ITEM_MAX];rs_rng rng;uint32_t seed;int players,t,started,scroll,next_drop,next_id,drop_x,drop_goal;int events[4];} world;
void world_init(world*,int,uint32_t);
void world_step(world*,const int[4]);
int world_running(const world*);
int world_all_down(const world*);
int pace_interval(int t);
int bot_decide(const world*,int);
enum {ST_TITLE,ST_PLAY,ST_DEAD,ST_OVER};
void draw_init(void);
void draw_frame(const world*,int,int,int,int,int);
void draw_state(void);
enum {SFX_JUMP,SFX_SCORE,SFX_HIT,SFX_LAND,SFX_COUNT};
#define SFX_HOUSE SFX_COUNT
#endif
