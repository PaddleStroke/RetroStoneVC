/* Fixed-step Q16 tray inertia. Common drops are independently judged for each player. */
#include "game.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
static int clamp(int n,int a,int b){return n<a?a:n>b?b:n;}
int pace_interval(int t){return 80-clamp(t,0,7200)*40/7200;}
static void spawn(world*);
void world_init(world*w,int players,uint32_t seed){
 memset(w,0,sizeof *w);w->players=clamp(players,1,4);w->seed=seed;rs_rng_seed(&w->rng,seed);w->next_drop=pace_interval(0);w->drop_x=w->drop_goal=160;
 for(int p=0;p<w->players;p++){hero*h=&w->h[p];h->state=HS_READY;h->x=160+(p*2-w->players+1)*30;h->xx=h->x*Q16_ONE;h->y=GROUND_Y*Q16_ONE;h->lives=1;}
 spawn(w);w->items[0].age=16; /* The frozen title already shows the first cookie on the belt. */
}
int world_running(const world*w){for(int p=0;p<w->players;p++)if(w->h[p].state<=HS_RUN)return 1;return 0;}
int world_all_down(const world*w){for(int p=0;p<w->players;p++)if(w->h[p].state<HS_DOWN)return 0;return 1;}
static void damage(world*w,int p){hero*h=&w->h[p];if(h->state!=HS_RUN)return;if(--h->lives<=0){h->state=HS_HIT;h->t=0;w->events[p]|=EV_HIT;}}
static void spawn(world*w){
 for(int i=0;i<ITEM_MAX;i++)if(!w->items[i].active){drop*d=&w->items[i];memset(d,0,sizeof*d);d->active=1;d->id=w->next_id++;d->bad=d->id%5==4;
  /* 72 frames on the conveyor, then at least 44 frames of visible falling. */
  /* Seeded journeys between outer zones distribute catches across the whole cafe.
     The step budget shrinks with the time available between consecutive landings. */
  int span=56+(pace_interval(w->t)-40)*6/5;
  if(d->bad){
   d->x=w->drop_x;
   /* The next journey follows the safe, inward dodge away from this cup. */
   w->drop_goal=w->drop_x<160?220+(int)(rs_rng_next(&w->rng)%57):44+(int)(rs_rng_next(&w->rng)%57);
  }else if(d->id%5==0 && d->id>0)d->x=w->drop_x+(w->drop_x<160?56:-56);
  else{
   if(abs(w->drop_goal-w->drop_x)<8){
    int right=w->drop_x<160 || (w->drop_x==160&&(rs_rng_next(&w->rng)&1));
    w->drop_goal=right?220+(int)(rs_rng_next(&w->rng)%57):44+(int)(rs_rng_next(&w->rng)%57);
   }
   int distance=span*3/4+(int)(rs_rng_next(&w->rng)%(span/4+1));
   d->x=w->drop_x+clamp(w->drop_goal-w->drop_x,-distance,distance);
  }
  d->x=clamp(d->x,44,276);w->drop_x=d->x;d->y=52*Q16_ONE;d->vy=155000+clamp(w->t,0,14400)*24000/14400;return;}
}
void world_step(world*w,const int input[4]){
 memset(w->events,0,sizeof w->events);
 if(!w->started){int yes=0;for(int p=0;p<w->players;p++)yes|=input[p];if(!yes)return;w->started=1;for(int p=0;p<w->players;p++)w->h[p].state=HS_RUN;}
 if(world_running(w)){w->t++;w->scroll=w->t;if(--w->next_drop<=0){spawn(w);w->next_drop=pace_interval(w->t);}}
 for(int p=0;p<w->players;p++){
  hero*h=&w->h[p];h->t++;if(h->catch_t)h->catch_t--;
  if(h->spill_t){h->spill_t--;h->spill_x+=h->spill_vx;h->spill_y+=(45-h->spill_t)*13000;}
  if(h->state==HS_HIT){if(h->t>=8){h->state=HS_DOWN;h->t=0;}continue;}
  if(h->state!=HS_RUN)continue;
  int dir=(!!(input[p]&IN_RIGHT))-(!!(input[p]&IN_LEFT));int32_t old=h->vx,target=dir*222822;
  if(h->vx<target)h->vx+=11796;
  if(h->vx>target)h->vx-=11796;
  if(abs(h->vx-target)<11796)h->vx=target;
  h->xx+=h->vx;if(h->xx<24*Q16_ONE){h->xx=24*Q16_ONE;h->vx=0;}if(h->xx>296*Q16_ONE){h->xx=296*Q16_ONE;h->vx=0;}
  h->x=h->xx>>16;h->tilt=clamp((h->vx-old)*8/11796,-8,8);
  for(int j=0;j<h->load && j<PILE_MAX;j++){
   biscuit*b=&h->pile[j];b->vel-=(h->vx-old)*40/100;b->vel=b->vel*82/100;b->pos+=b->vel;
   if(abs(b->pos)>21*Q16_ONE){h->spill_t=45;h->spill_type=b->type;b->type=-1;h->spill_x=h->xx+b->pos;h->spill_y=(TRAY_Y-8-(j/3)*6)*Q16_ONE;h->spill_vx=h->vx+b->vel;damage(w,p);h->catch_t=12;break;}
  }
 }
 for(int i=0;i<ITEM_MAX;i++){
  drop*d=&w->items[i];if(!d->active)continue; /* Failed drops still fall out naturally during the loss animation. */
  d->age++;if(d->age<=72)continue;int old=d->y>>16;d->y+=d->vy;int y=d->y>>16;
  if(!d->done&&old<TRAY_Y&&y>=TRAY_Y){
   d->done=1;
   for(int p=0;p<w->players;p++){
    hero*h=&w->h[p];if(h->state!=HS_RUN)continue;
    int near=abs(h->x-d->x)<24;
    if(d->bad){if(near)damage(w,p);continue;}
    if(near){
     int slot=h->load%PILE_MAX;
     biscuit*b=&h->pile[slot];b->pos=clamp(d->x-h->x+(slot%3-1)*5,-8,8)*Q16_ONE;b->vel=0;b->type=d->id%3;
     if(h->load<INT_MAX)h->load++;
     h->score=h->score>INT_MAX-10?INT_MAX:h->score+10;
     d->claimed|=1<<p;h->catch_t=8;w->events[p]|=EV_SCORE;
    }else{h->misses++;damage(w,p);}
   }
   if(!d->bad&&d->claimed)d->active=0;
  }
  if(y>250)d->active=0;
 }
}
int bot_decide(const world*w,int p){
 const hero*h=&w->h[p];if(h->state==HS_READY)return IN_START;if(h->state!=HS_RUN)return 0;
 int target=160,soon=999;const drop*next=0;
 for(int i=0;i<ITEM_MAX;i++){const drop*d=&w->items[i];if(!d->active||d->done)continue;int frames=72-d->age+(TRAY_Y-(d->y>>16))*Q16_ONE/d->vy;if(d->age>72)frames=(TRAY_Y-(d->y>>16))*Q16_ONE/d->vy;
  if(frames<soon){soon=frames;next=d;}}
 if(next){target=next->x;if(next->bad){target=h->x;if(abs(h->x-next->x)<44)target=next->x+(next->x<160?56:-56);}}

 int forecast=h->x+(h->vx*7>>16);if(forecast<target-3)return IN_RIGHT;if(forecast>target+3)return IN_LEFT;return 0;
}

