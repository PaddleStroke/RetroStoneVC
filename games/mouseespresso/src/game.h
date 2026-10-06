/* Theme-independent steam-flight rules; artwork lives in draw.c and tools/. */
#ifndef GAME_H
#define GAME_H
#include <stdint.h>
#include "rs.h"
#include "tuning.h"
#define MAX_PLAYERS 4
#define Q16_ONE 65536
#define PANEL_DELAY 40
#define RETRY_LOCK 36
#define BOT_START 20
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
/* The cafe's obstacles (DESIGN.md "Obstacles"): hanging from the ceiling, standing on the counter, floating, moving. */
enum {
 IT_LAMP,IT_TEABAG,IT_CUPHANG,IT_FILTER,          /* hanging on strings (IT_FILTER drips) */
 IT_CUPS,IT_SUGAR,IT_JUG,                          /* standing on the counter */
 IT_SPOON,IT_BEAN,IT_CAKE,IT_CROISSANT,            /* floating */
 IT_JET,IT_LADLE,IT_BOUNCE,                        /* moving: a steam jet, a swinging ladle, a bouncing sugar cube */
 IT_BISCUIT,                                       /* the collectible */
 IT_KINDS
};
/* The patterns the director chains (DESIGN.md "Patterns"). */
enum { PK_BREATHER,PK_ALTERNATE,PK_NARROW,PK_STAIRS,PK_ZIGZAG,PK_CLUSTER,PK_CORRIDOR,PK_JETS,PK_LADLES,PK_BOUNCERS,
 PK_COUNT };
/* One course item. x: world x of its centre. Hanging items: len = the string, bottom = their lowest solid row;
 * standing items: top; floating items: (x, y) is the centre; moving items: period and phase (frames). */
typedef struct item {
 int32_t x;
 int16_t y,len,size,period,phase;
 uint8_t kind,var,taken,pat;      /* taken: the biscuit's players (bit p); pat: its pattern (ring index) */
} item;
/* One pattern of the course: its span, kind, difficulty and the height where it lets the mouse out (< 0: anywhere) */
typedef struct pattern_rec { int32_t x0,x1; uint8_t kind,k; int16_t exit_y; } pattern_rec;
typedef struct course {
 uint32_t rng,n,first,npat;       /* items generated so far, the oldest kept (ring), patterns generated */
 int32_t gen_x,wave_x;            /* the end of the generated course; where this wave began */
 int16_t exit_y,waves;            /* the safe height where the last pattern ends; waves completed */
 uint8_t recent[3];               /* the last pattern kinds (no repeats) */
 uint8_t pad;
 item it[COURSE_N];
 pattern_rec pat[PATTERN_N];
} course;
/* An item's moving parts at frame t (and at t - 1: collisions sweep the motion between frames). */
typedef struct item_pose {
 int on,warn,rise;                /* a jet: firing (harmful) / its harmless telegraph, frames into it */
 int h,old_h;                     /* a jet's height */
 int tx,ty,old_tx,old_ty;         /* the ladle's bowl, the bouncing cube's centre */
 int drops,dy[3],old_dy[3];       /* the filter's falling drips (their top y) */
} item_pose;
typedef struct hero {
 int state,t,x,score,held,grounded,land_t,air_t,smooth_t,apples,launches,anchor,distance,steam_sound_t;
 int32_t y,vx,vy;int64_t wx;
} hero;
typedef struct world {hero h[4];int players,t,started,scroll;int64_t travel;uint32_t seed;int events[4];course c;} world;
/* play.c: the flight */
void world_init(world *w,int players,uint32_t seed);
void world_step(world *w,const int held[4]);
int world_running(const world *w);
int world_all_down(const world *w);
int course_speed(int distance);
int hero_start_x(int players,int p);
void hero_fly(int32_t *y,int32_t *vy,int held,int *grounded);
/* course.c: the items, their collisions and the director */
void course_init(course *c,uint32_t seed);
void course_fill(course *c,int32_t upto);       /* generate the course up to world x upto */
void course_prune(course *c,int32_t behind);    /* drop the items entirely left of world x behind */
const item *course_item(const course *c,uint32_t i);
const pattern_rec *course_pattern(const course *c,uint32_t i);
void item_pose_at(const item *it,int t,item_pose *p);
void item_extent(const item *it,int *left,int *right);
int item_hits(const item *it,const item_pose *p,int x0,int x1,int y0,int y1);
int course_hits(const course *c,int t,int x0,int x1,int y0,int y1);
int course_first_near(const course *c,int32_t x);  /* the first ring index whose item may reach world x */
int course_target_k(int32_t x,int32_t wave_x);
/* bot.c: the test bot (screen only) */
int bot_decide(const world *w,int p);
void bot_state(void);
/* draw.c */
void draw_init(void);
void draw_frame(const world *w,int state,int st_t,int best,int new_best,int paused);
void draw_state(void);
void draw_relayout(void);   /* after a state of another width was loaded: this width's maps (widescreen) */
void draw_view_log(const world *w);   /* tests (--opt view=1): the view at exit, "view: width= left= p1_x= next_x=" */
int draw_wide_left(void);   /* widescreen: the columns shown behind the mice (WIDE_LEFT of the extra width) */
enum { SFX_JUMP,SFX_SCORE,SFX_HIT,SFX_LAND,SFX_COUNT };
#define SFX_HOUSE SFX_COUNT
#endif
