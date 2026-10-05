/* MIT (c) 2026 8BCraft. Shared conveyor sorter declarations. */
#ifndef GAME_H
#define GAME_H
#include <stdint.h>
#include "rs.h"
#define MAX_PLAYERS 4
#define PANEL_DELAY 40
#define RETRY_LOCK 36
#define BOT_START 20
#define MEDAL_SCORES {15,40,80,150}
#define ITEM_MAX 6
#define ROUTE_FRAMES 32
#define RULE_BATCH 12
#define BELT_JOIN 160
#define BELT_START 48
#define RULE_HOLD 90
enum {HS_READY,HS_RUN,HS_HIT,HS_DOWN,HS_OFF};
enum {ST_TITLE,ST_PLAY,ST_DEAD,ST_OVER};
enum {EV_JUMP=1,EV_SCORE=2,EV_HIT=4,EV_LAND=8,EV_RULE=16};
enum {ITEM_YELLOW_DUCK,ITEM_WHITE_DUCK,ITEM_BLUE_DUCK,ITEM_YELLOW_STAR,ITEM_WHITE_STAR,ITEM_BLUE_STAR,ITEM_YELLOW_SNAIL,ITEM_WHITE_SNAIL,ITEM_BLUE_SNAIL,ITEM_COUNT};
enum {ROUTE_LEFT=-1,ROUTE_RIGHT=1,ROUTE_CENTER=2};
enum {RULE_SPECIES,RULE_COLOR};
typedef struct hero {int state,t,x,score,alive,combo,side,feedback,drop_t,drop_item,drop_side;} hero;
typedef struct parcel {int active,item,y;} parcel;
typedef struct world {hero h[4];parcel items[ITEM_MAX];int players,t,started,scroll,round,mode,bin_order[3],spawn_t,batch,draining,switch_t;uint32_t seed,rng;int events[4];} world;
int item_side(const world*,int item);
int belt_speed(int round);
int delivery_spacing(int round);
void world_init(world *w,int players,uint32_t seed);
void world_step(world *w,const int press[4]);
int world_running(const world *w);
int world_all_down(const world *w);
int bot_decide(const world *w,int player);
void draw_init(void);
void draw_frame(const world *w,int state,int st_t,int best,int new_best,int paused);
void draw_state(void);
void draw_relayout(void);   /* after a state load: the ground map of this width (widescreen) */
enum {SFX_JUMP,SFX_SCORE,SFX_HIT,SFX_LAND,SFX_RULE,SFX_COUNT};
#define SFX_HOUSE SFX_COUNT
#endif
