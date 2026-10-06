/* Continuous one-button flight over a shared, seeded course of cafe items (course.c). */
#include "game.h"
#include <stdlib.h>
#include <string.h>
int hero_start_x(int players,int p){return players==1?72:48+p*28;}
/* one frame of flight: thrust while held, gravity otherwise, the ceiling and the counter stop the mouse */
void hero_fly(int32_t *y,int32_t *vy,int held,int *grounded){
 *vy+=held?-THRUST:GRAVITY;if(*vy<-VY_UP_MAX)*vy=-VY_UP_MAX;if(*vy>VY_DOWN_MAX)*vy=VY_DOWN_MAX;
 *y+=*vy;*grounded=0;
 if(*y<=FLIGHT_TOP*Q16_ONE){*y=FLIGHT_TOP*Q16_ONE;*vy=0;}
 if(*y>=FLIGHT_BOTTOM*Q16_ONE){*y=FLIGHT_BOTTOM*Q16_ONE;*vy=0;*grounded=1;}
}
/* the course is generated ahead of the travel (never of the screen: the same course at every width) */
static void keep_course(world *w){
 course_fill(&w->c,(int32_t)(w->travel>>16)+GEN_AHEAD);
 course_prune(&w->c,(int32_t)(w->travel>>16)-KEEP_BEHIND);
}
void world_init(world *w,int players,uint32_t seed){
 memset(w,0,sizeof *w);w->players=players<1?1:players>4?4:players;w->seed=seed;
 course_init(&w->c,seed);keep_course(w);
 for(int p=0;p<w->players;p++){
  hero *h=&w->h[p];h->state=HS_READY;h->x=hero_start_x(w->players,p);h->wx=(int64_t)h->x*Q16_ONE;
  h->y=FLIGHT_BOTTOM*Q16_ONE;h->grounded=1;h->land_t=20;
 }
}
static void collect(world *w,int p,int x,int y){
 hero *h=&w->h[p];
 for(uint32_t i=(uint32_t)course_first_near(&w->c,x-13);i<w->c.n;i++){
  item *it=&w->c.it[i%COURSE_N];
  if(it->x>x+13+100)break;
  if(it->kind!=IT_BISCUIT || (it->taken>>p&1) || abs(it->x-x)>13 || abs(it->y-y)>12)continue;
  it->taken|=(uint8_t)(1<<p);h->apples++;h->smooth_t=12;w->events[p]|=EV_APPLE;
 }
}
void world_step(world *w,const int held[4]){
 memset(w->events,0,sizeof w->events);
 if(!w->started){
  int start=0;for(int p=0;p<w->players;p++)start|=held[p];if(!start)return;
  w->started=1;for(int p=0;p<w->players;p++)w->h[p].state=HS_RUN;w->events[0]|=EV_START;return;
 }
 w->t++;int speed=course_speed((int)(w->travel>>16));w->travel+=speed;w->scroll=(int)(w->travel>>16);
 keep_course(w);
 for(int p=0;p<w->players;p++){
  hero *h=&w->h[p];h->t++;h->land_t++;if(h->smooth_t>0)h->smooth_t--;
  if(h->steam_sound_t>0)h->steam_sound_t--;
  if(h->state==HS_RUN){
   int old_x=(int)(h->wx>>16),old_y=h->y>>16,was_grounded=h->grounded;
   if(held[p] && !h->held){h->launches++;if(!h->steam_sound_t){w->events[p]|=EV_JUMP;h->steam_sound_t=20;}}
   h->held=!!held[p];h->vx=speed;h->wx=(int64_t)hero_start_x(w->players,p)*Q16_ONE+w->travel;
   hero_fly(&h->y,&h->vy,h->held,&h->grounded);
   if(h->grounded && !was_grounded){h->land_t=0;w->events[p]|=EV_LAND;}
   int x=(int)(h->wx>>16),y=h->y>>16;
   if(course_hits(&w->c,w->t,old_x,x,old_y,y)){h->state=HS_HIT;h->t=0;h->held=0;w->events[p]|=EV_HIT;}
   h->x=x-w->scroll;h->distance=(int)(w->travel>>16);
   if(h->state==HS_RUN)collect(w,p,x,y);
   if(h->state==HS_RUN && h->anchor<(int)w->c.npat && x>course_pattern(&w->c,(uint32_t)h->anchor)->x1){h->anchor++;w->events[p]|=EV_SCORE;}
   h->score=h->distance/SCORE_PX+h->apples*BISCUIT_POINTS;
  }else if(h->state==HS_HIT || h->state==HS_DOWN){
   h->vy+=GRAVITY;h->y+=h->vy;if(h->y>FLIGHT_BOTTOM*Q16_ONE){h->y=FLIGHT_BOTTOM*Q16_ONE;h->vy=0;}
   if(h->t>=20)h->state=HS_DOWN;
   if(h->t>=40)h->state=HS_OFF;
  }
 }
}
int world_running(const world *w){for(int p=0;p<w->players;p++)if(w->h[p].state==HS_RUN || w->h[p].state==HS_READY)return 1;return 0;}
int world_all_down(const world *w){for(int p=0;p<w->players;p++)if(w->h[p].state!=HS_OFF)return 0;return 1;}
