#include "game.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){world w,a,b;int idle[4]={0},start[4]={16},up[4]={1};world_init(&w,1,4);world_step(&w,start);
 for(int t=0;t<24;t++)world_step(&w,idle);
 assert(w.h[0].beds[0].state==BED_WARN);assert(w.h[0].hearts==1);
 assert(w.h[0].beds[0].duration==40&&w.h[0].spawn_t==54);
 for(int t=0;t<18;t++)world_step(&w,idle);
 assert(w.h[0].beds[0].state==BED_ACTIVE);world_step(&w,up);assert(w.h[0].score==12 && w.h[0].hearts==1);
 int score=w.h[0].score;for(int t=0;t<12;t++)world_step(&w,up);assert(w.h[0].score==score && w.h[0].hearts==1);
 world_init(&w,1,4);world_step(&w,start);w.h[0].beds[0]=(bed){BED_ACTIVE,0,1,72};world_step(&w,up);assert(w.h[0].hearts==0 && w.h[0].score==0&&w.h[0].state==HS_HIT);
 // One timeout ends only its own player, even if that player presses another correct bed.
 world_init(&w,2,4);world_step(&w,start);w.h[0].beds[0]=(bed){BED_ACTIVE,39,0,40};w.h[0].beds[1]=(bed){BED_ACTIVE,0,0,40};
 int right[4]={2,0};world_step(&w,right);
 assert(w.h[0].state==HS_HIT&&w.h[0].hearts==0&&w.h[0].score==0&&w.events[0]==EV_HIT);
 assert(w.h[1].state==HS_RUN&&w.h[1].hearts==1&&world_running(&w));
 // A wrong simultaneous direction cannot earn points after that fatal input.
 world_init(&w,1,4);world_step(&w,start);w.h[0].beds[1]=(bed){BED_ACTIVE,0,0,40};int both[4]={3,0};world_step(&w,both);
 assert(w.h[0].state==HS_HIT&&w.h[0].score==0&&!(w.events[0]&EV_SCORE));
 world_init(&w,1,4);world_step(&w,start);for(int t=0;t<900;t++)world_step(&w,idle);assert(world_all_down(&w));
 for(unsigned seed=1;seed<=12;seed++){world_init(&a,4,seed);world_init(&b,4,seed);
  for(int t=0;t<7200;t++){int input[4];for(int p=0;p<4;p++)input[p]=bot_decide(&a,p);world_step(&a,input);world_step(&b,input);assert(!memcmp(&a,&b,sizeof a));for(int p=0;p<4;p++)assert(a.h[p].state==HS_RUN && a.h[p].hearts==1);}
  assert(a.h[0].anchor>70 && a.h[0].apples>10);printf("seed %u: %d rabbits, %d friends, score %d\n",seed,a.h[0].anchor,a.h[0].apples,a.h[0].score);
 }
 world_init(&w,1,4);world_step(&w,start);w.h[0].wave=48;w.h[0].spawn_t=1;world_step(&w,idle);
 int seen=0;for(int i=0;i<4;i++)if(w.h[0].beds[i].state==BED_WARN){assert(w.h[0].beds[i].duration==32);seen++;}
 assert(seen==1&&w.h[0].spawn_t==38);
 // The new pace still permits a visible reaction delay, including correctly sparing friends.
 for(unsigned seed=1;seed<=24;seed++){
  world_init(&w,4,seed);world_step(&w,start);for(int p=0;p<4;p++)w.h[p].wave=48;
  for(int t=0;t<7200;t++){
   int input[4]={0};for(int p=0;p<4;p++)if(!w.h[p].held)for(int i=0;i<4;i++)if(w.h[p].beds[i].state==BED_ACTIVE&&!w.h[p].beds[i].friend&&w.h[p].beds[i].t>=12){input[p]=1<<i;break;}
   world_step(&w,input);for(int p=0;p<4;p++)assert(w.h[p].state==HS_RUN&&w.h[p].hearts==1);
  }
 }
 puts("old-cap opening, faster fixed cap, warnings, held/friendly input, reaction delay and four-player determinism passed");return 0;}
