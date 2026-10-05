/* Spider Swing. MIT, (c) 2026 Pierre-Louis Boyer (8BCraft). */
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
#define SILK_LENGTH 88
#define FLOWER_CATCH_RADIUS 12
#define FLOWER_TYPES 5
#define FLOWER_COLORS 3
enum hero_state { HS_READY,HS_RUN,HS_HIT,HS_DOWN,HS_OFF };
enum { MODE_SWING,MODE_FLY,MODE_CATCH };
enum { INPUT_RELEASE=1,INPUT_LEFT=2,INPUT_RIGHT=4 };
enum { EV_JUMP=1,EV_SCORE=2,EV_HIT=4,EV_LAND=8,EV_START=16,EV_APPLE=32 };
enum { ST_TITLE,ST_PLAY,ST_DEAD,ST_OVER };
typedef struct hero {
    int state,t,x,score,air_t,land_t;
    int32_t y,vx,vy;
    int64_t wx;
    int grounded,held,combo,smooth_t,apples,apple_id,bonus,launches,spill;
    int mode,anchor,phase,rope,wait,web_t,swing_dir;
    int64_t silk_x;
    int32_t silk_y;
} hero;
typedef struct world {
    hero h[MAX_PLAYERS];
    int players,t,started,scroll;
    uint32_t seed;
    int events[MAX_PLAYERS];
} world;
void world_init(world *w,int players,uint32_t seed);
void world_step(world *w,const int held[MAX_PLAYERS]);
int world_running(const world *w);
int world_all_down(const world *w);
int bot_decide(const world *w,int p);
void anchor_pos(const world *w,int id,int *x,int *y);
void dew_pos(const world *w,int id,int *x,int *y);
void bee_pos(const world *w,int id,int t,int *x,int *y);
int trap_closed(int id,int t);
int flower_life(int anchor);
int flower_kind(const world *w,int anchor);
int flower_color(const world *w,int anchor);
void silk_segment(const hero *h,int *x,int *y,int *tip_x,int *tip_y);
int silk_catch(const world *w,const hero *h,int old_x,int old_y,int old_tip_x,int old_tip_y);
void draw_init(void);
void draw_frame(const world *w,int state,int st_t,int best,int new_best,int paused);
void draw_state(void);
void draw_relayout(void);
enum { SFX_JUMP,SFX_FLOWER,SFX_COUNT=SFX_FLOWER+FLOWER_TYPES };
#define SFX_HOUSE SFX_COUNT
#endif
