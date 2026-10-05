/* Arrival-time controls, independent multiplayer and fair changing rules. */
#include "game.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void step(world *w,int a,int b,int c,int d){int p[4]={a,b,c,d};world_step(w,p);}
static int active(const world *w){int n=0;for(int i=0;i<ITEM_MAX;i++)n+=w->items[i].active;for(int p=0;p<w->players;p++)n+=w->h[p].drop_t>0;return n;}
int main(void){
 world w;
 const int sides[3]={ROUTE_LEFT,ROUTE_CENTER,ROUTE_RIGHT};
 // Every permutation must send each species/colour to its pictured bin.
 for(int left=0;left<3;left++)for(int middle=0;middle<3;middle++)if(left!=middle){
  world_init(&w,1,7);w.bin_order[0]=left;w.bin_order[1]=middle;w.bin_order[2]=3-left-middle;
  for(int mode=0;mode<2;mode++){w.mode=mode;for(int item=0;item<ITEM_COUNT;item++){
   int category=mode==RULE_COLOR?item%3:item/3,b=0;while(w.bin_order[b]!=category)b++;
   assert(item_side(&w,item)==sides[b]);
  }}
 }
 assert(belt_speed(0)==256&&belt_speed(36)==384&&belt_speed(72)==512&&belt_speed(999)==512);
 assert(delivery_spacing(0)==96&&delivery_spacing(18)==68&&delivery_spacing(36)==48&&delivery_spacing(72)==36&&delivery_spacing(999)==36);
 // The geometric gap shrinks alongside the time interval, despite faster belt motion.
 for(int round=0;round<=72;round++){
  int ramp=round<36?round:36,gap=96-ramp*24/36;
  assert(delivery_spacing(round)*belt_speed(round)>=gap*256);
  assert(delivery_spacing(round)*belt_speed(round)<gap*256+belt_speed(round));
  assert(delivery_spacing(round)>ROUTE_FRAMES);
  if(round)assert(delivery_spacing(round)<=delivery_spacing(round-1));
 }
 world_init(&w,4,7);assert(w.items[0].y==BELT_START*256&&w.mode==RULE_SPECIES);
 w.items[0].item=ITEM_YELLOW_SNAIL;step(&w,1,0,0,0);assert(w.h[0].score==0&&w.h[0].side==1);
 for(int t=0;t<20;t++)step(&w,0,0,0,0);
 assert(w.h[0].side==1&&w.h[0].score==0);
 w.items[0].y=BELT_JOIN*256-1;step(&w,-1,1,ROUTE_CENTER,0);
 assert(w.h[0].score==0&&!w.h[0].alive&&w.h[0].state==HS_DOWN&&w.h[1].score==1&&w.h[1].alive);
 assert((w.events[0]&EV_HIT)&&!(w.events[0]&EV_SCORE));assert(!w.h[2].alive&&world_running(&w));
 assert(w.h[1].drop_t==ROUTE_FRAMES&&w.h[1].drop_side==1);
 step(&w,1,-1,0,0);assert(w.h[1].side==-1&&w.h[1].drop_side==1);
 w.items[0]=(parcel){1,ITEM_YELLOW_SNAIL,BELT_JOIN*256-1};step(&w,1,1,1,1);
 assert(w.h[1].score==2&&w.h[1].alive&&w.h[0].score==0&&!w.h[0].alive&&w.events[0]==0);
 // A single wrong sort ends a solo run immediately, with no success event.
 world_init(&w,1,7);w.items[0]=(parcel){1,ITEM_BLUE_SNAIL,BELT_JOIN*256-1};step(&w,-1,0,0,0);
 assert(!world_running(&w)&&world_all_down(&w)&&w.h[0].score==0&&w.events[0]==EV_HIT);
 // The third route is a persistent selection, not the no-input sentinel.
 world_init(&w,1,1);w.items[0].item=ITEM_BLUE_STAR;w.items[0].y=BELT_JOIN*256-400;
 step(&w,1,0,0,0);assert(w.h[0].score==0);step(&w,ROUTE_CENTER,0,0,0);assert(w.h[0].score==1&&w.h[0].side==ROUTE_CENTER);
 step(&w,0,0,0,0);assert(w.h[0].side==ROUTE_CENTER&&w.h[0].drop_side==ROUTE_CENTER);
 // No early scoring; a last-frame gate selection is valid and stays selected.
 world_init(&w,1,1);w.items[0].item=ITEM_WHITE_DUCK;w.items[0].y=BELT_JOIN*256-400;
 step(&w,1,0,0,0);assert(w.h[0].score==0);step(&w,-1,0,0,0);step(&w,0,0,0,0);assert(w.h[0].score==1);
 // A drained transition cannot switch while even one bird remains in a chute.
 world_init(&w,1,1);memset(w.items,0,sizeof w.items);w.draining=1;w.h[0].drop_t=5;
 for(int i=0;i<4;i++){step(&w,0,0,0,0);assert(w.switch_t==0&&w.mode==RULE_SPECIES);}
 step(&w,0,0,0,0);assert(w.switch_t==90);
 for(int i=0;i<90;i++){int mode=w.mode,order[3];memcpy(order,w.bin_order,sizeof order);step(&w,0,0,0,0);
  if(mode!=w.mode){assert(active(&w)==0);assert(w.events[0]&EV_RULE);assert(memcmp(order,w.bin_order,sizeof order));}
  else assert(!memcmp(order,w.bin_order,sizeof order));
 }
 assert(w.mode==RULE_COLOR&&w.batch==1&&w.items[0].y==-20*256);
 // All seeds exercise the full speed cap, both rule types and three destinations.
 unsigned permutations[2]={0};
 for(int seed=1;seed<=16;seed++){
  world a,b;world_init(&a,4,(uint32_t)seed);world_init(&b,4,(uint32_t)seed);int switches=0,arrivals=0,seen[ITEM_COUNT]={0},last_arrival=-9999;
  int pending[4]={0},delay[4]={0};
  for(int t=0;t<50000;t++){
   int press[4]={0};for(int p=0;p<4;p++){
    int desired=bot_decide(&a,p);
    if(desired&&desired!=pending[p]){pending[p]=desired;delay[p]=12;}
    if(pending[p]&&!--delay[p]){press[p]=pending[p];pending[p]=0;}
   }
   for(int i=0;i<ITEM_MAX;i++)if(a.items[i].active)seen[a.items[i].item]=1;
   int oldmode=a.mode,oldround=a.round,order[3];memcpy(order,a.bin_order,sizeof order);parcel previous[ITEM_MAX];memcpy(previous,a.items,sizeof previous);
   world_step(&a,press);world_step(&b,press);assert(memcmp(&a,&b,sizeof a)==0);
   if(a.mode!=oldmode){switches++;assert(active(&a)==0);assert(a.events[0]&EV_RULE);assert(a.switch_t==60);assert(memcmp(order,a.bin_order,sizeof order));}
   else assert(!memcmp(order,a.bin_order,sizeof order));
   int code=a.bin_order[0]*9+a.bin_order[1]*3+a.bin_order[2];permutations[a.mode]|=1u<<code;
   assert(a.bin_order[0]!=a.bin_order[1]&&a.bin_order[0]!=a.bin_order[2]&&a.bin_order[1]!=a.bin_order[2]);
   if(a.round!=oldround){assert(t-last_arrival>ROUTE_FRAMES);last_arrival=t;arrivals++;for(int p=0;p<4;p++)assert(a.events[p]&EV_SCORE);}
   for(int i=0;i<ITEM_MAX;i++)if(previous[i].active&&a.items[i].active)assert(a.items[i].y-previous[i].y>=256&&a.items[i].y-previous[i].y<=512);
  }
  assert(arrivals>400&&switches>30&&belt_speed(a.round)==512);
  for(int i=0;i<ITEM_COUNT;i++)assert(seen[i]);
  for(int p=0;p<4;p++)assert(a.h[p].alive&&a.h[p].score==arrivals);
  printf("seed %d: %d arrivals, %d fair rule switches, four players survive speed cap\n",seed,arrivals,switches);
 }
 unsigned all=(1u<<5)|(1u<<7)|(1u<<11)|(1u<<15)|(1u<<19)|(1u<<21);
 assert(permutations[0]==all&&permutations[1]==all);
 puts("conveyor sorting rules: passed");return 0;
}
