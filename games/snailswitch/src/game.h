#ifndef GAME_H
#define GAME_H
#include "rs.h"
#include "tuning.h"
#include <stdint.h>
#define MAX_PLAYERS 4
#define Q16_ONE 65536
#define PANEL_DELAY 40
#define RETRY_LOCK 36
#define HIT_FREEZE 8
#define BOT_START 20
#define MEDAL_SCORES {100,250,500,1000}
/* a phone held upright (DESIGN.md "Tall screen"): rs_screen_zoom(ZOOM_H): 258 columns shown on a 1080x2340 phone, 240
 * on a 1080x2520 one (the snails 4.5x, as in landscape) */
#define ZOOM_H 560
/* Widescreen (DESIGN.md "Widescreen"): the share of the extra width (the columns past 320) shown BEHIND the snails, on
 * the left: one third (the owner's decision), the other two thirds ahead. The stage origin is WIDE_LEFT(width - 320):
 * 36 px at 427, 67 at 520, 80 at 560, 0 at 320 (rounded). */
#define WIDE_LEFT_NUM 1
#define WIDE_LEFT_DEN 3
#define WIDE_LEFT(extra) (((extra)*WIDE_LEFT_NUM*2+WIDE_LEFT_DEN)/(2*WIDE_LEFT_DEN))
enum hero_state {HS_READY,HS_RUN,HS_HIT,HS_DOWN,HS_OFF};
typedef struct hero {int state,t,x,score,next_block,side,flip,from,berry,target;int32_t y,vy;int air_t,land_t;} hero;
/* The course (DESIGN.md "The course"): things on the vine, in world x order. A hazard (thorn, caterpillar, sap) or a
 * fruit, on one surface (side 0 the upper, 1 the underside). Fruits are numbered in course order (id): a snail's
 * berry is the last one it caught. A caterpillar patrols around x from its phase. */
enum {IT_THORN,IT_CATER,IT_SAP,IT_FRUIT};
typedef struct item {int32_t x,id;uint8_t kind,side,phase,seg;} item;
/* the director's memory (course.c): its generator, where the next segment starts, what it made lately */
typedef struct director {
 uint32_t rng;int32_t x,fruits,segs;
 int8_t last,prev,recent[RECENT_MEMORY],pairs[PAIR_MEMORY][2];   /* last: the last kind but a breather */
 int16_t npairs,load,load_max,greedy_cool,phrase;   /* phrase: segments left in this phrase */
 uint32_t sig;            /* the last segment's kind and parameters, hashed (tests: the variety metric) */
} director;
typedef struct world {int scroll;int64_t travel;hero h[4];int players,t,started;uint32_t seed;int events[4];
 director dir;int nitems;item it[ITEM_MAX];} world;
enum {CMD_TOGGLE=1,CMD_UP=2,CMD_DOWN=3};
/* one hazard as seen at time t: where it is (a caterpillar wanders), which way it faces */
typedef struct obstacle {int x,anchor,side,type,id,dir;} obstacle;
enum {EV_JUMP=1,EV_SCORE=2,EV_HIT=4,EV_LAND=8,EV_START=16};
enum {ST_TITLE,ST_PLAY,ST_DEAD,ST_OVER};
enum {SFX_JUMP,SFX_SCORE,SFX_HIT,SFX_LAND,SFX_COUNT};
#define SFX_HOUSE SFX_COUNT
/* the segment kinds (course.c; DESIGN.md "The course") */
enum {SEG_BREATHER,SEG_LONE,SEG_HAZARD,SEG_TRAIL,SEG_ZIGZAG,SEG_AFTER,SEG_SLALOM,SEG_CATERS,SEG_SAP,SEG_GREEDY,
      SEG_CLASSIC,SEG_FORK,SEG_COUNT};
extern const char*const seg_names[SEG_COUNT];
/* play.c: the rules */
void world_init(world*,int,uint32_t);
void world_step(world*,const int[4]);
int world_running(const world*);
int world_all_down(const world*);
int vine_y(int);
int curve_sine(int);
int32_t course_speed(int);
void course_fill(world*);                          /* the course buffer for the scroll (world_step does it) */
int hero_offset(const hero*);                      /* px from the vine's centre line: -21 upper .. +21 underside */
/* a stretch of course as the rules see it: its items (sorted by x), the time (caterpillars) and the scroll */
typedef struct course_view {const item*it;int n,t,scroll;} course_view;
int hero_step(hero*,int cmd,const course_view*);  /* a running snail's frame: its events (EV_HIT: it is out) */
void item_at(const item*,int t,obstacle*);        /* a hazard at time t */
/* course.c: the director */
void director_start(director*,uint32_t seed);
int  director_segment(director*,item*out,int max);  /* the next segment's items (sorted by x): how many */
int  difficulty_at(int32_t x);                      /* 0..256 */
/* plan.c: the test bot (--opt bot=N: from what a 4:3 screen shows; the planner, plan.h, is shared with tests/test_course.c) */
int bot_decide(const world*,int);
void scenery_pixels(const world*,uint8_t*,int);   /* the canvas width: the screen's (320 at 4:3) */
void draw_init(void);
void draw_frame(const world*,int,int,int,int,int);
void draw_state(void);
void draw_state_loaded(void);   /* widescreen: a state saved at another width */
void draw_view_log(const world*);   /* tests (--opt view=1): the view at exit, "view: width= left= p1_x= next_x=" */
#endif
