/* Frog Reach. MIT, (c) 2026 Pierre-Louis Boyer (8BCraft). */
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
/* Widescreen (DESIGN.md "Widescreen"): the share of the extra width of a view (its columns past those it has at 320)
 * shown BEHIND its frog, on the left: one third (the owner's decision), the other two thirds ahead. WIDE_LEFT(extra)
 * rounds it: 36 px on a 427-px view, 67 at 520, 80 at 560, 0 at 320. */
#define WIDE_LEFT_NUM 1
#define WIDE_LEFT_DEN 3
#define WIDE_LEFT(extra) (((extra)*WIDE_LEFT_NUM*2+WIDE_LEFT_DEN)/(2*WIDE_LEFT_DEN))
/* A phone held upright (DESIGN.md "Tall screen"): a screen of ZOOM_H lines (rs_screen_zoom), its height filling the
 * display's, the sides cropped (246 columns shown on a 9:21 phone, 265 on a 9:19.5 one: the frog 20 px from the left
 * edge and a next pad up to 224 px ahead in them); the stage low, TALL_BELOW lines of pond under it; the HUD, BG1's
 * top HUD_LINES lines, pinned at the screen's top. */
#define ZOOM_H 576
#define TALL_BELOW 64
#define HUD_LINES 72
enum hero_state { HS_READY,HS_RUN,HS_HIT,HS_DOWN,HS_OFF };
enum { MODE_WAIT,MODE_CHARGE,MODE_LEAP };
enum { EV_JUMP=1,EV_SCORE=2,EV_HIT=4,EV_LAND=8,EV_START=16,EV_APPLE=32 };
enum { ST_TITLE,ST_PLAY,ST_DEAD,ST_OVER };
typedef struct hero {
 int state,t,x,score,air_t,land_t;
 int32_t y,vx,vy;
 int64_t wx;
 int grounded,held,combo,smooth_t,apples,apple_id,bonus,launches,spill;
 int mode,anchor,length,wait,goal_x,start_x,good,perfect,leap_t,pad_offset,goal_offset,start_guard;
} hero;
typedef struct world {
 hero h[4]; int players,t,started,scroll;uint32_t seed;int events[4];
} world;
void world_init(world *w,int players,uint32_t seed);
void world_step(world *w,const int held[4]);
int world_running(const world *w);
int world_all_down(const world *w);
int bot_decide(const world *w,int p);
void perch_pos(const world *w,int id,int t,int *x,int *width);
int pad_life(int anchor);
int tongue_speed(int anchor);
void draw_init(void);
void draw_frame(const world *w,int state,int st_t,int best,int new_best,int paused);
void draw_state(void);
void draw_relayout(void);   /* after a state of another width was loaded: this width's maps (widescreen) */
void draw_view_log(const world *w);   /* tests (--opt view=1): the view at exit, "view: width= left= p1_x= next_x=" */
enum { SFX_JUMP,SFX_SCORE,SFX_HIT,SFX_LAND,SFX_COUNT };
#define SFX_HOUSE SFX_COUNT
#endif
