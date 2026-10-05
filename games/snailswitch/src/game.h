#ifndef GAME_H
#define GAME_H
#include "rs.h"
#include <stdint.h>
#define MAX_PLAYERS 4
#define OBSTACLE_SPACING 260
#define Q16_ONE 65536
#define PANEL_DELAY 40
#define RETRY_LOCK 36
#define HIT_FREEZE 8
#define BOT_START 20
#define MEDAL_SCORES {100,250,500,1000}
enum hero_state {HS_READY,HS_RUN,HS_HIT,HS_DOWN,HS_OFF};
typedef struct hero {int state,t,x,score,next_block,side,flip,from,berry,target;int32_t y,vy;int air_t,land_t;} hero;
typedef struct world {int scroll;int64_t travel;hero h[4];int players,t,started;uint32_t seed;int events[4];} world;
enum {CMD_TOGGLE=1,CMD_UP=2,CMD_DOWN=3};
typedef struct obstacle {int x,anchor,side,type,id,dir;} obstacle;
enum {EV_JUMP=1,EV_SCORE=2,EV_HIT=4,EV_LAND=8,EV_START=16};
enum {ST_TITLE,ST_PLAY,ST_DEAD,ST_OVER};
enum {SFX_JUMP,SFX_SCORE,SFX_HIT,SFX_LAND,SFX_COUNT};
#define SFX_HOUSE SFX_COUNT
void world_init(world*,int,uint32_t);
void world_step(world*,const int[4]);
int world_running(const world*);
int world_all_down(const world*);
int vine_y(int);
int curve_sine(int);
int32_t course_speed(int);
void obstacle_at(const world*,int,obstacle*);
int bot_decide(const world*,int);
void scenery_pixels(const world*,uint8_t*);
void draw_init(void);
void draw_frame(const world*,int,int,int,int,int);
void draw_state(void);
#endif

