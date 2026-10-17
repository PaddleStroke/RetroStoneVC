/* Moth Around. MIT, (c) 2026 Pierre-Louis Boyer (8BCraft). */
#ifndef GAME_H
#define GAME_H
#include <stdint.h>
#include "rs.h"
#define MAX_PLAYERS 4
#define MAX_RINGS 6
#define MAX_OPENINGS 3
#define Q16_ONE 65536
#define ORBIT 44
#define LANTERN_Y 132
/* a phone held upright (DESIGN.md "Tall screen"): a screen of ZOOM_H lines (rs_screen_zoom), its height filling the
 * display's, the sides cropped (the rings, 107 px around the lantern at most, inside the 218 columns of a 9:21 phone);
 * the stage low, TALL_BELOW lines of the hedge's shade under it; the HUD (BG1's top HUD_LINES) at the screen's top */
#define ZOOM_H 512
#define TALL_BELOW 40
#define HUD_LINES 48
#define MOTH_RADIUS 6
#define PANEL_DELAY 40
#define RETRY_LOCK 36
#define BOT_START 20
#define MEDAL_SCORES {100,250,500,1000}
enum hero_state { HS_READY,HS_RUN,HS_HIT,HS_DOWN,HS_OFF };
enum { EV_JUMP=1,EV_SCORE=2,EV_HIT=4,EV_LAND=8,EV_START=16,EV_APPLE=32 };
enum { ST_TITLE,ST_PLAY,ST_DEAD,ST_OVER };
typedef struct hero {
 int state,t,x,score,angle,alive_t,anchor,apples,launches,smooth_t;
 int32_t y,vx,vy;
} hero;
typedef struct ring {
 int active,id,angle,spin,gap,kind,passed,openings,offset[MAX_OPENINGS],width[MAX_OPENINGS];
 int32_t radius,previous,speed;
} ring;
typedef struct world {
 hero h[4];ring rings[MAX_RINGS];
 int players,t,started,scroll,next_spawn,ring_id,last_gap;uint32_t seed;int events[4];
} world;
void world_init(world *w,int players,uint32_t seed);
void world_step(world *w,const int input[4]);
int world_running(const world *w);
int world_all_down(const world *w);
int bot_decide(const world *w,int p);
int angle_diff(int a,int b);
int ring_opening(const ring *r,int angle,int safety);
int ring_center(const ring *r,int opening,int ticks);
int ring_speed(int id);
int ring_target(const world *w);
int orbit_radius(int players,int p);
void draw_init(void);
void draw_frame(const world *w,int state,int st_t,int best,int new_best,int paused);
void draw_state(void);
void draw_relayout(void);   /* after a state of another width was loaded: this width's maps (widescreen) */
enum { SFX_JUMP,SFX_SCORE,SFX_HIT,SFX_LAND,SFX_COUNT };
#define SFX_HOUSE SFX_COUNT
#endif
