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
/* Widescreen (DESIGN.md "Widescreen"): the share of the extra width (the columns past 320) shown BEHIND the snails, on
 * the left: one third (the owner's decision), the other two thirds ahead. The stage origin is WIDE_LEFT(width - 320):
 * 36 px at 427, 67 at 520, 80 at 560, 0 at 320 (rounded). */
#define WIDE_LEFT_NUM 1
#define WIDE_LEFT_DEN 3
#define WIDE_LEFT(extra) (((extra)*WIDE_LEFT_NUM*2+WIDE_LEFT_DEN)/(2*WIDE_LEFT_DEN))
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
void scenery_pixels(const world*,uint8_t*,int);   /* the canvas width: the screen's (320 at 4:3) */
void draw_init(void);
void draw_frame(const world*,int,int,int,int,int);
void draw_state(void);
void draw_state_loaded(void);   /* widescreen: a state saved at another width */
void draw_view_log(const world*);   /* tests (--opt view=1): the view at exit, "view: width= left= p1_x= next_x=" */
#endif

