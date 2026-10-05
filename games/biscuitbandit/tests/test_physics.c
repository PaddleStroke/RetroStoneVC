#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void tick(world*w){int in[4]={0};for(int p=0;p<w->players;p++)in[p]=bot_decide(w,p);world_step(w,in);}
static void forced(world*w,int x,int bad){memset(w->items,0,sizeof w->items);drop*d=&w->items[0];d->active=1;d->x=x;d->y=(TRAY_Y-1)*Q16_ONE;d->age=73;d->vy=2*Q16_ONE;d->bad=bad;}
int main(void){
 world w,v;int start[4]={IN_START},none[4]={0};world_init(&w,4,5);v=w;world_step(&w,none);CHECK(!memcmp(&w,&v,sizeof w));CHECK(w.items[0].active&&w.items[0].age==16);world_step(&w,start);CHECK(w.t==1&&w.h[0].x==v.h[0].x);
 CHECK(pace_interval(0)==80&&pace_interval(3600)==60&&pace_interval(7200)==40&&pace_interval(1000000)==40);
 world_init(&w,4,5);world_step(&w,start);for(int p=0;p<4;p++){w.h[p].x=160;w.h[p].xx=160*Q16_ONE;}
 forced(&w,160,0);world_step(&w,none);for(int p=0;p<4;p++)CHECK(w.h[p].load==1&&w.h[p].score==10&&w.events[p]&EV_SCORE);
 puts("actual frozen cookie on title, same starting scene, independent immediate catches: passed");
 world_init(&w,1,5);world_step(&w,start);
 for(int i=0;i<1000;i++){forced(&w,160,0);world_step(&w,none);CHECK(w.h[0].state==HS_RUN);}
 CHECK(w.h[0].load==1000&&w.h[0].score==10000);
 w.h[0].load=INT_MAX;w.h[0].score=INT_MAX-5;forced(&w,160,0);world_step(&w,none);CHECK(w.h[0].load==INT_MAX&&w.h[0].score==INT_MAX);
 puts("no capacity or delivery limit; 1000 cookies accumulated, score counted immediately, overflow safe: passed");
 world_init(&w,1,5);world_step(&w,start);world_step(&w,(int[4]){IN_RIGHT});CHECK(w.h[0].vx>0&&w.h[0].vx<222822);for(int i=0;i<20;i++)world_step(&w,(int[4]){IN_RIGHT});world_step(&w,(int[4]){IN_LEFT});CHECK(w.h[0].vx>0&&w.h[0].tilt<0);
 world_init(&w,1,5);world_step(&w,start);forced(&w,160,1);world_step(&w,none);CHECK(w.h[0].state==HS_HIT&&w.events[0]&EV_HIT);
 world_init(&w,1,5);world_step(&w,start);w.h[0].load=1;w.h[0].pile[0].pos=20*Q16_ONE;w.h[0].pile[0].vel=4*Q16_ONE;world_step(&w,none);CHECK(w.h[0].spill_t==45&&w.h[0].state==HS_HIT&&w.events[0]&EV_HIT);int y=w.h[0].spill_y;world_step(&w,none);CHECK(w.h[0].spill_y>y);
 world_init(&w,4,5);world_step(&w,start);forced(&w,w.h[1].x,0);world_step(&w,none);CHECK(w.h[0].state==HS_HIT&&w.h[1].state==HS_RUN&&w.h[1].load==1&&w.h[2].state==HS_HIT&&w.h[3].state==HS_HIT);
 world_init(&w,1,5);world_step(&w,start);forced(&w,260,0);world_step(&w,none);CHECK(w.h[0].state==HS_HIT&&w.h[0].misses==1&&w.events[0]&EV_HIT&&!(w.events[0]&EV_SCORE));y=w.items[0].y;world_step(&w,none);CHECK(w.items[0].y>y);for(int t=0;t<45;t++)world_step(&w,none);CHECK(!w.items[0].active);
 puts("single missed/spilled cookie or caught cup immediately ends only that player's run: passed");
 for(int seed=1;seed<=100;seed++){
  world_init(&w,4,seed);v=w;w.t=v.t=7200;w.next_drop=v.next_drop=40;
  int last_id=w.next_id,last_spawn=0,left=0,right=0,spread=0,previous=w.drop_x;
  for(int f=1;f<=18000;f++){
   tick(&w);tick(&v);CHECK(!memcmp(&w,&v,sizeof w));int visible=0;
   for(int i=0;i<ITEM_MAX;i++)visible+=w.items[i].active;
   CHECK(visible>0);
   if(w.next_id!=last_id){
    CHECK(w.next_id==last_id+1);CHECK(f-last_spawn==40);
    CHECK(w.drop_x>=44&&w.drop_x<=276&&abs(w.drop_x-previous)<=56);
    for(int i=0;i<ITEM_MAX;i++)if(w.items[i].active&&w.items[i].id==w.next_id-1&&!w.items[i].bad){
     if(w.drop_x<94)left++;
     if(w.drop_x>226)right++;
     if(abs(w.drop_x-160)>=65)spread++;
    }
    previous=w.drop_x;last_spawn=f;last_id=w.next_id;
   }
   for(int p=0;p<4;p++)CHECK(w.h[p].state==HS_RUN);
  }
  for(int p=0;p<4;p++)CHECK(w.h[p].load>300&&w.h[p].score==w.h[p].load*10&&w.h[p].misses==0);
  CHECK(left>25&&right>25&&spread>100);
  printf("cap seed %d: %d cookies, left %d, right %d, no idle conveyor or missed drops\n",seed,w.h[0].load,left,right);
 }
 // The entire opening and speed ramp must also be playable from all four start positions.
 for(int seed=1;seed<=30;seed++){
  world_init(&w,4,seed);
  for(int f=0;f<18000;f++){tick(&w);for(int p=0;p<4;p++)CHECK(w.h[p].state==HS_RUN);}
  CHECK(w.drop_x>=44&&w.drop_x<=276);
  for(int p=0;p<4;p++)CHECK(w.h[p].load>250&&w.h[p].misses==0);
 }
 puts("30 seeded openings and full speed ramps: every player catches all cookies");
 // Allow a short reaction pause after each catch before steering to the next item.
 for(int seed=1;seed<=20;seed++){
  world_init(&w,4,seed);w.t=7200;w.next_drop=40;
  for(int f=0;f<18000;f++){
   int in[4]={0};for(int p=0;p<4;p++)if(!w.h[p].catch_t)in[p]=bot_decide(&w,p);
   world_step(&w,in);for(int p=0;p<4;p++)CHECK(w.h[p].state==HS_RUN);
  }
 }
 puts("20 cap courses allow an eight-frame reaction pause after every catch");
 puts("Biscuit Bandit continuous-stream physics: all passed");return 0;
}
