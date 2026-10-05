#ifndef GAME_H
#define GAME_H
#include <stdint.h>
#include "rs.h"
#define MAX_PLAYERS 4
#define MAX_BALLS 24
#define MAX_FX 24
#define Q16_ONE 65536
#define GROUND_Y 208
#define PANEL_DELAY 40
#define RETRY_LOCK 36
#define BOT_START 20
#define MEDAL_SCORES {50,150,300,600}
enum hero_state {HS_READY,HS_RUN,HS_HIT,HS_DOWN,HS_OFF};
typedef struct hero {int state,t,x,old_x,score;int32_t xx,y,vy;int shot,shot_x,shot_y,shot_wait,facing,air_t,land_t;} hero;
typedef struct marble {int active,size,grace,hue;int32_t x,y,vx,vy,px,py;} marble;
typedef struct fragment {int t,x,y,vx,vy,hue;} fragment;
typedef struct arrival {int size,x,hue;} arrival;
typedef struct world {hero h[4];marble balls[MAX_BALLS];fragment fx[MAX_FX];uint32_t seed,rng;int players,t,started,scroll;int events[4];int spawn_wait,warning,warning_x,warning_size,warning_hue,freeze,total_hits,spawns,pace,empty_frames,warning_count;arrival incoming[3];} world;
enum {EV_JUMP=1,EV_SCORE=2,EV_HIT=4,EV_LAND=8,EV_START=16};
void world_init(world*,int,uint32_t);
void world_step(world*,const int[4]);
int world_running(const world*);
int world_all_down(const world*);
int ball_radius(int);
int ball_mass(const world*);
int marble_pace(const world*);
int32_t marble_speed(int,int);
int32_t marble_bounce(int,int);
int32_t marble_gravity(int);
int bot_decide(const world*,int);
enum {ST_TITLE,ST_PLAY,ST_DEAD,ST_OVER};
void draw_init(void);
void draw_frame(const world*,int,int,int,int,int);
void draw_state(void);
enum {SFX_JUMP,SFX_SCORE,SFX_HIT,SFX_LAND,SFX_COUNT};
#define SFX_HOUSE SFX_COUNT
#endif

