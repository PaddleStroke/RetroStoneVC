#ifndef GAME_H
#define GAME_H
#include <stdint.h>
#include "rs.h"
#include "tuning.h"
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
/* A polar bear: warned first (warn > 0 frames left: only the marker at its landing spot, `land` px from the camera;
 * harmless), then sliding (warn == 0: `age` frames since it landed). */
typedef struct sliding_bear {int active,id,x,warn,warn_len,land,age;int64_t distance,previous;} sliding_bear;
/* bear_warned / bear_landed: the x of a bear warned / landed this frame (for its sound), 0 if none */
typedef struct world {hero h[4];sliding_bear bears[MAX_BEARS];int players,t,started,scroll,bear_wait,bear_id,bear_warned,bear_landed;int64_t travel;uint32_t seed;int events[4];} world;
typedef struct row {int id,distance,center,gap,kind,left,right,rx,ry;} row;
typedef struct slope_obstacle {int id,kind,x,distance,rx,ry,pass_side;} slope_obstacle;
void course_row(const world *w,int id,row *r);
int course_index(const world *w,int distance);
void course_obstacle(const world *w,int id,int travel,int t,slope_obstacle *o);
void bear_obstacle(const world *w,int slot,int ticks,slope_obstacle *o);
int bear_land(int players);                                /* where bears land: px from the camera (< 0: uphill) */
int bear_warn_time(int scroll);                            /* the warning's length at this distance */
/* would a bear warned now at x (for `warn` frames, landing at `land`) be fair? Every state from which the player can
 * follow the course and the bears already warned comfortably keeps a dodge (tuning.h "fairness") */
int bear_fair(const world *w,int x,int warn,int land);
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
void draw_relayout(void);                                  /* widescreen: after a state saved at another width */
enum {SFX_JUMP,SFX_SCORE,SFX_HIT,SFX_LAND,SFX_RUMBLE,SFX_WHUMP,SFX_COUNT};   /* RUMBLE: a bear warned, WHUMP: it lands */
#define SFX_HOUSE SFX_COUNT
#endif
