#include "game.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static void start(world*w){int in[4]={RS_BTN_A,0,0,0};world_step(w,in);}
static int count(const world*w){int n=0;for(int i=0;i<MAX_BALLS;i++)n+=!!w->balls[i].active;return n;}
static void clean(world*w){world_init(w,1,4);start(w);memset(w->balls,0,sizeof w->balls);w->spawn_wait=999999;w->empty_frames=-999999;w->h[0].shot=0;w->h[0].shot_wait=0;}
int main(void){
 world w,s;int zero[4]={0};
 world_init(&w,4,7);s=w;for(int i=0;i<300;i++)world_step(&w,zero);assert(!memcmp(&w,&s,sizeof w));start(&w);assert(w.players==4&&w.started&&w.t==1);for(int p=0;p<4;p++){assert(w.h[p].x==s.h[p].x);assert(w.h[p].state==HS_RUN);}puts("frozen title and same starting coordinates: passed");
 clean(&w);int hold[4]={RS_BTN_A};for(int i=0;i<220;i++)world_step(&w,hold);assert(w.h[0].shot>0);assert(w.h[0].score==0);assert(w.h[0].x==100);for(int i=0;i<100;i++)world_step(&w,zero);assert(!w.h[0].shot);puts("holding fires successive limited-lived ropes, release stops repeats: passed");
 for(int size=0;size<4;size++){
  clean(&w);int r=ball_radius(size);w.balls[0]=(marble){.active=1,.size=size,.x=250*Q16_ONE,.y=(GROUND_Y-r)*Q16_ONE,.vx=Q16_ONE};
  int top=GROUND_Y;for(int i=0;i<120;i++){world_step(&w,zero);assert(w.balls[0].active);int y=w.balls[0].y/Q16_ONE;if(y<top)top=y;assert(w.balls[0].x>=r*Q16_ONE&&w.balls[0].x<=(320-r)*Q16_ONE);assert(y+r<=GROUND_Y);}
  assert(top+r<GROUND_Y-30);
 }puts("all four sizes bounce high enough to run underneath and reflect inside the arena: passed");
 clean(&w);w.balls[0]=(marble){.active=1,.size=3,.x=100*Q16_ONE,.y=155*Q16_ONE};w.h[0].shot=15;w.h[0].shot_x=100;w.h[0].shot_y=50;world_step(&w,zero);assert(w.h[0].score==10&&count(&w)==2&&ball_mass(&w)==8);for(int i=0;i<MAX_BALLS;i++)if(w.balls[i].active)assert(w.balls[i].size==2&&w.balls[i].grace>=41);puts("rope body catches the ball, mass-conserving split and visible nonlethal grace: passed");
 clean(&w);w.balls[0]=(marble){.active=1,.size=0,.x=100*Q16_ONE,.y=188*Q16_ONE,.vx=5*Q16_ONE};world_step(&w,zero);assert(w.h[0].state==HS_HIT);assert(w.events[0]&EV_HIT);puts("swept landing contact kills immediately: passed");
 clean(&w);w.balls[0]=(marble){.active=1,.size=0,.x=100*Q16_ONE,.y=194*Q16_ONE,.grace=30};world_step(&w,zero);assert(w.h[0].state==HS_RUN);puts("fresh split cannot instantly hit a nearby mole: passed");
 clean(&w);w.spawn_wait=0;world_step(&w,zero);assert(w.warning==90&&count(&w)==0);for(int i=0;i<89;i++)world_step(&w,zero);assert(w.warning==1&&count(&w)==0);world_step(&w,zero);assert(count(&w)==1&&w.spawns==1&&w.h[0].state==HS_RUN);puts("ninety-frame warning precedes off-floor entry: passed");
 world_init(&w,4,1);start(&w);w.h[0].shot=0;memset(w.balls,0,sizeof w.balls);w.spawn_wait=999999;w.balls[0]=(marble){.active=1,.size=0,.x=w.h[0].x*Q16_ONE,.y=194*Q16_ONE};world_step(&w,zero);assert(w.h[0].state==HS_HIT&&w.h[1].state==HS_RUN);for(int i=0;i<60;i++)world_step(&w,zero);assert(w.h[0].state==HS_DOWN&&w.h[1].state==HS_RUN&&w.t>40);puts("individual hits, shared arena survivors continue: passed");
 /* Measure the actual opening velocity and apices against the previous physics. */
 world_init(&w,1,7);assert(w.balls[0].size==0);assert(w.pace==700);
 assert(abs(w.balls[0].vx)==173015*7/10);
 for(int size=0;size<3;size++){
  clean(&w);int radius=ball_radius(size),floor=GROUND_Y-radius;
  w.balls[0]=(marble){.active=1,.size=size,.x=250*Q16_ONE,.y=floor*Q16_ONE,.vy=marble_bounce(size,700),.grace=999999};
  int apex=floor;for(int t=0;t<160;t++){world_step(&w,zero);int y=w.balls[0].y/Q16_ONE;if(y<apex)apex=y;}
  static const int old_v[3]={275251,314573,360448};int32_t y=floor*Q16_ONE,vy=-old_v[size];int old_apex=floor;
  for(int t=0;t<100;t++){vy+=10486;y+=vy;if(y>floor*Q16_ONE)break;if(y/Q16_ONE<old_apex)old_apex=y/Q16_ONE;}
  int height=floor-apex,old_height=floor-old_apex;
  assert(height*100>=old_height*145&&height*100<=old_height*165);
  printf("size %d: new bounce %d px, previous %d px (about 50%% higher)\n",size,height,old_height);
 }
 clean(&w);w.balls[0]=(marble){.active=1,.size=0,.x=240*Q16_ONE,.y=100*Q16_ONE,.vx=Q16_ONE,.grace=999999};
 world_step(&w,zero);int opening_speed=abs(w.balls[0].vx);
 w.h[0].score=450;world_step(&w,zero);int middle_speed=abs(w.balls[0].vx);
 w.h[0].score=1500;world_step(&w,zero);assert(abs(w.balls[0].vx)>middle_speed&&middle_speed>opening_speed);
 int capped_speed=abs(w.balls[0].vx);w.h[0].score=9000;world_step(&w,zero);assert(abs(w.balls[0].vx)==capped_speed);
 puts("30-percent slower opening, existing balls accelerate with score, speed cap: passed");
 /* A living opening ball must not block timed new arrivals. Keep this scheduler fixture harmless. */
 world_init(&w,1,42);start(&w);int original_active=0,last_wave=0,shortest=9999,longest=0;
 for(int frame=0;frame<1150;frame++){
  for(int i=0;i<MAX_BALLS;i++)if(w.balls[i].active)w.balls[i].grace=999999;
  int previous=w.spawns;world_step(&w,zero);assert(w.h[0].state==HS_RUN);
  if(w.spawns!=previous){int span=w.t-last_wave;if(last_wave){if(span<shortest)shortest=span;if(span>longest)longest=span;}last_wave=w.t;}
  original_active|=w.spawns>0&&w.balls[0].active;
  for(int i=0;i<MAX_BALLS;i++)if(w.balls[i].active)assert(w.balls[i].size==0);
 }
 assert(w.spawns>=3&&original_active&&shortest>=270&&longest<=391&&longest!=shortest);
 puts("randomized steady arrivals while existing balls remain, all opening arrivals tiny: passed");
 clean(&w);w.empty_frames=0;world_step(&w,zero);assert(w.warning==60);
 for(int t=0;t<59;t++)world_step(&w,zero);
 assert(count(&w)==0&&w.warning==1);
 world_step(&w,zero);assert(count(&w)==1&&w.spawns==1);puts("empty arena refills after exactly one second: passed");
 int singles=0,doubles=0,triples=0,sizes[4]={0};
 for(int seed=1;seed<=40;seed++){
  clean(&w);w.rng=seed;w.h[0].score=700;w.spawn_wait=0;
  world_step(&w,zero);assert(w.warning==90);
  singles+=w.warning_count==1;doubles+=w.warning_count==2;triples+=w.warning_count==3;
  for(int i=0;i<w.warning_count;i++)sizes[w.incoming[i].size]++;
 }
 assert(singles&&doubles&&triples&&sizes[0]&&sizes[1]&&sizes[2]&&sizes[3]);
 puts("later randomized one/two/three-ball groups and all four sizes: passed");
 int hits=0,spawns=0,moves=0;
 for(int seed=1;seed<=12;seed++){
  world_init(&w,4,(uint32_t)seed);s=w;
  for(int frame=0;frame<18000&&world_running(&w);frame++){
   int in[4];for(int p=0;p<4;p++){in[p]=bot_decide(&w,p);if(in[p]&(RS_BTN_LEFT|RS_BTN_RIGHT))moves++;}world_step(&w,in);world_step(&s,in);assert(!memcmp(&w,&s,sizeof w));assert(ball_mass(&w)<=20);assert(count(&w)<=20);
  }
  hits+=w.total_hits;spawns+=w.spawns;printf("seed %d: %d frames, %d hits, %d warned entries, scores %d %d %d %d\n",seed,w.t,w.total_hits,w.spawns,w.h[0].score,w.h[1].score,w.h[2].score,w.h[3].score);
 }
 assert(hits>200&&spawns>40&&moves>1000);puts("ordinary-input multiplayer dodge/shoot endurance, deterministic replay and density limits: all passed");
 return 0;
}

