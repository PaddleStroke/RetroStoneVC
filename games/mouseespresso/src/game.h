/* Theme-independent steam-flight rules; artwork lives in draw.c and tools/. */
#ifndef GAME_H
#define GAME_H
#include <stdint.h>
#include "rs.h"
#define MAX_PLAYERS 4
#define Q16_ONE 65536
#define FLIGHT_TOP 16
#define FLIGHT_BOTTOM 218
#define HERO_RADIUS_X 8
#define HERO_RADIUS_Y 7
#define HAZARD_FIRST 360
#define HAZARD_SPACING 360
#define PANEL_DELAY 40
#define RETRY_LOCK 36
#define BOT_START 20
#define PAW_WARNING_FRAMES 72
#define PAW_ATTACK_FRAMES 48
#define MEDAL_SCORES {100,250,500,1000}
/* Widescreen (DESIGN.md "Widescreen"): the share of the extra width (the columns past 320) shown BEHIND the mice, on
 * the left: one third (the owner's decision), the other two thirds ahead. WIDE_LEFT(extra) rounds it: 36 px at 427,
 * 67 at 520, 80 at 560, 0 at 320. */
#define WIDE_LEFT_NUM 1
#define WIDE_LEFT_DEN 3
#define WIDE_LEFT(extra) (((extra)*WIDE_LEFT_NUM*2+WIDE_LEFT_DEN)/(2*WIDE_LEFT_DEN))
enum hero_state { HS_READY,HS_RUN,HS_HIT,HS_DOWN,HS_OFF };
enum { EV_JUMP=1,EV_SCORE=2,EV_HIT=4,EV_LAND=8,EV_START=16,EV_APPLE=32 };
enum { ST_TITLE,ST_PLAY,ST_DEAD,ST_OVER };
enum { OB_STOOL,OB_POP,OB_SHELF,OB_TRAP,OB_PAW,OB_APPLIANCE,OB_TABLE,OB_COUNT };
typedef struct hero {
 int state,t,x,score,held,grounded,land_t,air_t,smooth_t,apples,launches,anchor,distance,steam_sound_t;
 int collectible_group;uint32_t collectible_bits;
 int32_t y,vx,vy;int64_t wx;
} hero;
typedef struct world {hero h[4];int players,t,started,scroll;int64_t travel;uint32_t seed;int events[4];} world;
typedef struct toast_shot {int x,y,old_x,old_y,active,age;} toast_shot;
typedef struct moving_blade {int x,y,tip_x,tip_y,old_tip_x,old_tip_y;} moving_blade;
typedef struct obstacle {
 int type,id,x,tip,side,phase,active,variant,period,width,warning;
 int reach,old_reach,tip_x,old_tip_x,old_tip,effect,old_effect,jet_x,jet_y,spin;
 toast_shot shot[2];moving_blade blade[4];
} obstacle;
void world_init(world *w,int players,uint32_t seed);
void world_step(world *w,const int held[4]);
int world_running(const world *w);
int world_all_down(const world *w);
int bot_decide(const world *w,int p);
int course_speed(int distance);
int hero_start_x(int players,int p);
void obstacle_at(const world *w,int id,int t,obstacle *o);
void collectible_at(const world *w,int group,int index,int *x,int *y);
int collectible_taken(const hero *h,int group,int index);
int hazard_hits(const obstacle *o,int old_x,int x,int old_y,int y);
void draw_init(void);
void draw_frame(const world *w,int state,int st_t,int best,int new_best,int paused);
void draw_state(void);
void draw_relayout(void);   /* after a state of another width was loaded: this width's maps (widescreen) */
void draw_view_log(const world *w);   /* tests (--opt view=1): the view at exit, "view: width= left= p1_x= next_x=" */
int draw_wide_left(void);   /* widescreen: the columns shown behind the mice (WIDE_LEFT of the extra width) */
enum { SFX_JUMP,SFX_SCORE,SFX_HIT,SFX_LAND,SFX_COUNT };
#define SFX_HOUSE SFX_COUNT
#endif
