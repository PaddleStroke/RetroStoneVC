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
#define SHELLS 56
#define BALL_LEFT 8
#define BALL_RIGHT 312
#define BALL_TOP 8
#define PADDLE_Y 209
#define CRAB_Y 224
#define BALL_LOST 246
#define SHELL_LIMIT 196
enum {HS_READY,HS_RUN,HS_HIT,HS_DOWN,HS_OFF};
enum {EV_JUMP=1,EV_SCORE=2,EV_HIT=4,EV_LAND=8,EV_START=16,EV_APPLE=32};
enum {ST_TITLE,ST_PLAY,ST_DEAD,ST_OVER};
typedef struct shell {int x,hp,kind;int32_t y,fall;} shell;
typedef struct hero {int state,t,x,score,apples,launches,anchor,smooth_t,combo,reason;int32_t xx,y,vx,vy,bx,by,bvx,bvy;} hero;
typedef struct world {hero h[4];shell shells[SHELLS];int players,t,started,scroll,wave,spawn_t;uint32_t seed;int events[4];} world;
void world_init(world *w,int n,uint32_t seed);
void world_step(world *w,const int input[4]);
void world_fallout(world *w);
int world_running(const world *w);
int world_all_down(const world *w);
int bot_decide(const world *w,int p);
int ball_speed(int t);
int crab_y(int players,int p);
int paddle_y(int players,int p);
void draw_init(void);
void draw_frame(const world *w,int state,int st_t,int best,int new_best,int paused);
void draw_state(void);
void draw_relayout(void);   /* after a state of another width was loaded: this width's maps (widescreen) */
enum {SFX_JUMP,SFX_SCORE,SFX_HIT,SFX_LAND,SFX_COUNT};
#define SFX_HOUSE SFX_COUNT
#endif
