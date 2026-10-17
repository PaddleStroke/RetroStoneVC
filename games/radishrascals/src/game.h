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
/* a phone held upright (DESIGN.md "Tall screen"): a screen of ZOOM_H lines (rs_screen_zoom), its height filling the
 * display's, the sides cropped (204 columns on a 9:21 phone); one garden is drawn GARDEN_TALL_W px wide there (the
 * beds closer together, inside the columns shown), the stage TALL_ABOVE lines below the screen's top (the sky and
 * the score), more lawn and its decor below it */
#define ZOOM_H 480
#define GARDEN_TALL_W 248
#define TALL_ABOVE 40
enum {HS_READY,HS_RUN,HS_HIT,HS_DOWN,HS_OFF};
enum {EV_JUMP=1,EV_SCORE=2,EV_HIT=4,EV_LAND=8,EV_START=16,EV_APPLE=32};
enum {ST_TITLE,ST_PLAY,ST_DEAD,ST_OVER};
enum {BED_EMPTY,BED_WARN,BED_ACTIVE,BED_SHOO,BED_SAFE,BED_MISS};
typedef struct bed {int state,t,friend,duration;} bed;
typedef struct hero {int state,t,x,score,apples,launches,anchor,smooth_t,combo,hearts,held,spawn_t,wave,flash,aim;int32_t y,vx,vy;bed beds[4];} hero;
typedef struct world {hero h[4];int players,t,started,scroll;uint32_t seed;int events[4];} world;
void bed_position(int id,int *x,int *y);
void world_init(world *w,int n,uint32_t seed);
void world_step(world *w,const int input[4]);
int world_running(const world *w);
int world_all_down(const world *w);
int bot_decide(const world *w,int p);
void draw_init(void);
void draw_frame(const world *w,int state,int st_t,int best,int new_best,int paused);
void draw_state(void);
void draw_relayout(void);  /* widescreen: after a state of another width was loaded (draw.c) */
int draw_bed_at(int players,int x,int y);  /* touch: P1's bed nearest stage point x, y (in its view), -1: none (draw.c) */
enum {SFX_JUMP,SFX_SCORE,SFX_HIT,SFX_LAND,SFX_COUNT};
#define SFX_HOUSE SFX_COUNT
#endif
