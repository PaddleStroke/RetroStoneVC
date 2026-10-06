/*
 * Snail Switch: the rules. The vine, the course speed, a snail's curl from one surface to the other, its hazards and
 * fruits (hero_step: the same function for the game, the bot's planner and the tests' solver), and the course buffer
 * the director (course.c) writes ahead of the snails.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/snailswitch/LICENSE.
 */
#include "game.h"
#include <string.h>
#include <stdlib.h>
int curve_sine(int a){a&=1023;int sign=a>512?-1:1;if(a>512)a-=512;int64_t k=(int64_t)a*(512-a);return (int)(sign*16*k*32767/(5*512*512-4*k));}
int vine_y(int x){return 127+(curve_sine((x*3)&1023)*15>>15)+(curve_sine((x*7+180)&1023)*4>>15);}
int32_t course_speed(int d){if(d>SPEED_DIST)d=SPEED_DIST;return 157286+(int64_t)d*91750/SPEED_DIST;}
/* Slow, continuous patrol. The visible body and collision use this same position. */
void item_at(const item*it,int t,obstacle*o){
 o->id=it->id;o->x=o->anchor=it->x;o->side=it->side;o->type=it->kind;o->dir=1;
 if(it->kind==IT_CATER){int k=(t+it->phase)%PATROL_T,half=PATROL_T/2,patrol=k<half?k:PATROL_T-k;
  o->x+=patrol*2*PATROL/half-PATROL;o->dir=k<half?1:-1;}
}
/* the course buffer: drop what every snail has left behind, then let the director write ahead (never the width) */
void course_fill(world*w){
 for(;;){
  int drop=0;while(drop<w->nitems&&w->it[drop].x<w->scroll-KEEP_BEHIND)drop++;
  if(drop){w->nitems-=drop;memmove(w->it,w->it+drop,(size_t)w->nitems*sizeof w->it[0]);
   memset(w->it+w->nitems,0,(size_t)drop*sizeof w->it[0]);}
  if(w->dir.x>=w->scroll+FILL_AHEAD)break;
  if(w->nitems+SEG_MAX_ITEMS>ITEM_MAX)break; /* never with the tuning (tests: ~20 held at most); the next frame again */
  w->nitems+=director_segment(&w->dir,w->it+w->nitems,SEG_MAX_ITEMS);
 }
}
void world_init(world*w,int n,uint32_t seed){
 memset(w,0,sizeof *w);w->players=n;w->seed=seed;
 for(int p=0;p<4;p++){hero*h=&w->h[p];h->state=p<n?HS_READY:HS_OFF;h->x=116-p*27;h->berry=-1;h->y=(vine_y(h->x)-21)*Q16_ONE;}
 director_start(&w->dir,seed);course_fill(w);
}
int world_running(const world*w){for(int p=0;p<w->players;p++)if(w->h[p].state==HS_RUN || w->h[p].state==HS_READY)return 1;return 0;}
int world_all_down(const world*w){for(int p=0;p<w->players;p++)if(w->h[p].state!=HS_DOWN)return 0;return 1;}
int hero_offset(const hero*h){
 if(!h->flip)return h->side?21:-21;
 int progress=FLIP_FRAMES-h->flip;
 return (-21+42*progress/FLIP_FRAMES)*(h->from?-1:1);
}
static int out(hero*h){h->state=HS_HIT;h->t=0;h->vy=-(Q16_ONE*2);return EV_HIT;}
int hero_step(hero*h,int cmd,const course_view*c){
 int ev=0;
 if(cmd==CMD_UP)h->target=0;
 else if(cmd==CMD_DOWN)h->target=1;
 else if(cmd==CMD_TOGGLE && !h->flip)h->target=1-h->side;
 if(h->target!=h->side && !h->flip){h->from=h->side;h->side=h->target;h->flip=FLIP_FRAMES;ev|=EV_JUMP;}
 if(h->flip)h->flip--;
 int offset=hero_offset(h),wx=c->scroll+h->x;h->y=(vine_y(wx)+offset)*Q16_ONE;
 for(int i=0;i<c->n;i++){
  const item*it=&c->it[i];
  if(it->x<wx-64)continue;
  if(it->x>wx+64)break;
  if(it->kind==IT_FRUIT){
   /* Every fruit must be caught on its side, in course order; one passed uncaught is fatal. */
   if(it->id<=h->berry)continue;
   if(abs(it->x-wx)<FRUIT_HALF && (it->side?offset>FRUIT_REACH:offset< -FRUIT_REACH)){h->berry=it->id;h->score+=5;ev|=EV_SCORE;}
   else if(wx>it->x+FRUIT_HALF)return out(h);
  }else{
   obstacle o;item_at(it,c->t,&o);
   if(abs(o.x-wx)<HAZ_HALF && (o.side?offset>HAZ_EXPOSED:offset< -HAZ_EXPOSED))return out(h);
  }
 }
 int distance=c->scroll/24;if(distance>h->next_block){h->score+=distance-h->next_block;h->next_block=distance;}
 return ev;
}
void world_step(world*w,const int press[4]){
 memset(w->events,0,sizeof w->events);
 if(!w->started){int go=0;for(int p=0;p<w->players;p++)go|=press[p];if(!go)return;w->started=1;for(int p=0;p<w->players;p++){w->h[p].state=HS_RUN;w->events[p]=EV_START;}return;}
 w->t++;if(world_running(w)){w->travel+=course_speed(w->scroll);w->scroll=(int)(w->travel>>16);course_fill(w);}
 course_view c={w->it,w->nitems,w->t,w->scroll};
 for(int p=0;p<w->players;p++){
  hero*h=&w->h[p];h->t++;
  if(h->state==HS_HIT){if(h->t>HIT_FREEZE){h->vy+=Q16_ONE/3;h->y+=h->vy;if(h->y/Q16_ONE>270 || h->y/Q16_ONE< -50){h->state=HS_DOWN;h->t=0;}}continue;}
  if(h->state!=HS_RUN)continue;
  int ev=hero_step(h,press[p],&c);
  w->events[p]=ev&EV_HIT?EV_HIT:w->events[p]|ev;
 }
}
