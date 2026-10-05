/* MIT (c) 2026 8BCraft. Arrival-time sorting, continuous fixed-point conveyor. */
#include "game.h"
#include <string.h>
#include <limits.h>
static uint32_t rnd(world *w){uint32_t x=w->rng;x^=x<<13;x^=x>>17;x^=x<<5;return w->rng=x;}
int item_side(const world*w,int item){
 static const int route[3]={ROUTE_LEFT,ROUTE_CENTER,ROUTE_RIGHT};
 int category=w->mode==RULE_COLOR?item%3:item/3;
 for(int b=0;b<3;b++)if(w->bin_order[b]==category)return route[b];
 return 0;
}
static void shuffle_bins(world*w){
 static const int orders[6][3]={{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
 int previous=0;for(int i=0;i<6;i++)if(!memcmp(w->bin_order,orders[i],sizeof w->bin_order)){previous=i;break;}
 int next=(previous+1+(int)(rnd(w)%5))%6;
 memcpy(w->bin_order,orders[next],sizeof w->bin_order);
}
int belt_speed(int round){int ramp=round<72?round:72;return 256+ramp*256/72;}
int delivery_spacing(int round){
 /* Tighten the physical gap too: rising belt speed must not cancel the ramp. */
 int ramp=round<36?round:36,gap=96-ramp*24/36,speed=belt_speed(round);
 return (gap*256+speed-1)/speed;
}
static void spawn(world *w){for(int i=0;i<ITEM_MAX;i++)if(!w->items[i].active){w->items[i]=(parcel){1,(int)(rnd(w)%ITEM_COUNT),-20*256};w->batch++;return;}}
void world_init(world *w,int players,uint32_t seed){
 memset(w,0,sizeof *w);w->players=players<1?1:players>4?4:players;w->seed=seed;w->rng=seed?seed:0x51eedu;
 for(int b=0;b<3;b++)w->bin_order[b]=b;
 spawn(w);w->items[0].y=BELT_START*256;w->spawn_t=delivery_spacing(0);
 static const int stations[4]={48,272,80,240};
 for(int p=0;p<4;p++){hero *h=&w->h[p];h->state=p<w->players?HS_RUN:HS_OFF;h->alive=p<w->players;h->side=ROUTE_LEFT;h->x=stations[p];}
}
int world_running(const world *w){for(int p=0;p<w->players;p++)if(w->h[p].alive)return 1;return 0;}
int world_all_down(const world *w){return !world_running(w);}
static void arrive(world *w,int item){
 for(int p=0;p<w->players;p++){hero *h=&w->h[p];if(!h->alive)continue;
  h->drop_t=ROUTE_FRAMES;h->drop_item=item;h->drop_side=h->side;h->t=0;
  if(h->side==item_side(w,item)){if(h->score<INT_MAX)h->score++;if(h->combo<INT_MAX)h->combo++;h->feedback=1;w->events[p]|=EV_SCORE;}
  else{h->alive=0;h->combo=0;h->feedback=-1;w->events[p]|=EV_HIT;h->state=HS_DOWN;}
 }
 if(w->round<INT_MAX)w->round++;
}
void world_step(world *w,const int press[4]){
 memset(w->events,0,sizeof w->events);w->t++;w->started=1;
 if(world_running(w)&&!w->switch_t)w->scroll=(w->scroll+belt_speed(w->round))%(12*256);
 int active=0;
 for(int p=0;p<w->players;p++){hero *h=&w->h[p];h->t++;if(h->drop_t>0)h->drop_t--;if(h->feedback&&h->t>38)h->feedback=0;
  if(h->alive&&(press[p]==ROUTE_LEFT||press[p]==ROUTE_RIGHT||press[p]==ROUTE_CENTER)&&h->side!=press[p]){h->side=press[p];w->events[p]|=EV_JUMP;}
  if(h->drop_t)active++;
 }
 /* Delivered objects continue through their chute after the final player's loss. */
 if(!world_running(w))return;
 if(w->switch_t){
  w->switch_t--;
  if(w->switch_t==60){w->mode^=1;shuffle_bins(w);w->events[0]|=EV_RULE;}
  if(!w->switch_t){w->draining=0;w->batch=0;spawn(w);w->spawn_t=delivery_spacing(w->round);}
  return;
 }
 for(int i=0;i<ITEM_MAX;i++){parcel *a=&w->items[i];if(!a->active)continue;
  a->y+=belt_speed(w->round);
  if(a->y>=BELT_JOIN*256){a->active=0;arrive(w,a->item);active++;}else active++;
 }
 if(w->draining){if(!active)w->switch_t=RULE_HOLD;return;}
 if(--w->spawn_t<=0){spawn(w);w->spawn_t=delivery_spacing(w->round);if(w->batch>=RULE_BATCH)w->draining=1;}
}
/* Ordinary directional inputs aim at the next visible object. */
int bot_decide(const world *w,int player){
 int target=-1,best=-99999;for(int i=0;i<ITEM_MAX;i++)if(w->items[i].active&&w->items[i].y>best){target=i;best=w->items[i].y;}
 if(target<0||best<40*256)return 0;
 int side=item_side(w,w->items[target].item);return w->h[player].side==side?0:side;
}
