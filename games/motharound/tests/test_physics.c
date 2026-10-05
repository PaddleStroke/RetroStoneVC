#include "game.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void reachability(void){
 /* Every starting angle can reach every representative pattern after a
    24-frame reaction delay, including narrow spinning gaps at the speed cap. */
 for(int count=1;count<=3;count++)for(int spin=-24;spin<=24;spin+=24)for(int fast=0;fast<2;fast++)for(int angle=0;angle<256;angle++){
  world w;world_init(&w,4,1);w.started=1;w.next_spawn=10000;
  ring *r=&w.rings[0];r->active=1;r->openings=count;r->angle=57*256;r->spin=spin;r->speed=ring_speed(fast?100:0);
  r->radius=(orbit_radius(4,3)+MOTH_RADIUS)*Q16_ONE+70*r->speed;
  for(int k=0;k<count;k++){r->offset[k]=256*k/count;r->width[k]=33;}
  for(int p=0;p<4;p++){w.h[p].state=HS_RUN;w.h[p].angle=angle;}
  for(int t=0;t<160;t++){
   int input[4]={0};if(t>=24)for(int p=0;p<4;p++)input[p]=bot_decide(&w,p);
   world_step(&w,input);for(int p=0;p<4;p++)assert(w.h[p].state==HS_RUN);
  }
  for(int p=0;p<4;p++)assert(w.h[p].anchor==1);
 }
 for(int n=1;n<=4;n++){
  world w;world_init(&w,n,7);int last_contact=-1000,clear=-1000,seen=0,last_id=-1,min_radius=10000,max_radius=0;
  unsigned counts=0,widths=0;
  for(int t=0;t<18000;t++){
   int input[4]={0};for(int p=0;p<n;p++)input[p]=bot_decide(&w,p);world_step(&w,input);
   for(int p=0;p<n;p++)assert(w.h[p].state==HS_RUN);
   for(int i=0;i<MAX_RINGS;i++){
    ring *r=&w.rings[i];if(!r->active)continue;
    if(r->id>last_id){
     last_id=r->id;counts|=1u<<r->openings;widths|=1u<<(r->width[0]%16);
     int radius=r->radius>>16;if(radius<min_radius)min_radius=radius;if(radius>max_radius)max_radius=radius;
     assert(r->speed==ring_speed(r->id));
    }
    int outer=(orbit_radius(n,n-1)+MOTH_RADIUS)*Q16_ONE,inner=(orbit_radius(n,0)-MOTH_RADIUS)*Q16_ONE;
    if(r->previous>outer && r->radius<=outer){assert(w.t-clear>=67);last_contact=w.t;seen++;}
    if(r->previous>=inner && r->radius<inner){assert(w.t>=last_contact);clear=w.t;}
   }
  }
  assert(counts==14 && (widths&(widths-1)) && max_radius-min_radius>50 && seen>140);
 }
 assert(ring_speed(0)<ring_speed(50) && ring_speed(50)<ring_speed(100));
 assert(ring_speed(100)==ring_speed(100000));
}
int main(void) {
 reachability();
 int idle[4]={0},start[4]={2,2,2,2},left[4]={-1},right[4]={1};world w,a,b;
 /* Identical controls give identical ring timing, collisions and scores in
    solo play and in every multiplayer slot, including at the speed cap. */
 world_init(&a,1,9);world_init(&b,4,9);
 for(int t=0;t<18000;t++){
  int input[4];for(int p=0;p<4;p++)input[p]=bot_decide(&a,0);
  world_step(&a,input);world_step(&b,input);
  for(int p=0;p<4;p++)assert(!memcmp(&a.h[0],&b.h[p],sizeof(hero)));
  for(int i=0;i<MAX_RINGS;i++){
   assert(a.rings[i].radius==b.rings[i].radius && a.rings[i].angle==b.rings[i].angle);
   assert(a.rings[i].active==b.rings[i].active);
  }
 }
 world_init(&w,1,4);for(int t=0;t<100;t++)world_step(&w,idle);assert(!w.started);
 world_step(&w,start);int angle=w.h[0].angle;world_step(&w,left);assert(w.h[0].angle==((angle-3)&255));
 world_step(&w,right);assert(w.h[0].angle==angle);
 world_init(&w,1,4);world_step(&w,start);for(int t=0;t<300;t++)world_step(&w,idle);assert(world_all_down(&w));
 for(unsigned seed=1;seed<=12;seed++) {
  world_init(&a,4,seed);world_init(&b,4,seed);
  for(int t=0;t<7200;t++) {
   int input[4];for(int p=0;p<4;p++)input[p]=bot_decide(&a,p);
   world_step(&a,input);world_step(&b,input);assert(!memcmp(&a,&b,sizeof a));
   for(int p=0;p<4;p++)if(a.h[p].state!=HS_RUN){
    printf("FAIL seed=%u frame=%d player=%d angle=%d\n",seed,t,p,a.h[p].angle);
    for(int i=0;i<MAX_RINGS;i++)if(a.rings[i].active){ring *r=&a.rings[i];printf("ring=%d radius=%d angle=%d openings=%d widths=%d,%d,%d offsets=%d,%d,%d\n",r->id,r->radius>>16,r->angle/256,r->openings,r->width[0],r->width[1],r->width[2],r->offset[0],r->offset[1],r->offset[2]);}
    return 1;
   }
  }
  printf("seed %u: %d rings, %d score, 4 players / 120 seconds\n",seed,a.h[0].anchor,a.h[0].score);
  assert(a.h[0].anchor>60 && a.h[0].score>700);
 }
 /* Crossing an entire barrier in one step still collides; passing through the opening succeeds. */
 world_init(&w,1,1);world_step(&w,start);memset(w.rings,0,sizeof w.rings);
 ring *r=&w.rings[0];r->active=1;r->radius=90*Q16_ONE;r->speed=60*Q16_ONE;r->gap=58;r->angle=64*256;
 world_step(&w,idle);assert(w.h[0].state==HS_HIT);
 world_init(&w,1,1);world_step(&w,start);memset(w.rings,0,sizeof w.rings);
 r=&w.rings[0];r->active=1;r->radius=90*Q16_ONE;r->speed=60*Q16_ONE;r->gap=58;r->angle=0;
 world_step(&w,idle);assert(w.h[0].state==HS_RUN && w.h[0].anchor==1 && w.h[0].apples==1);
 /* Secondary and third openings work for both safety checks and scoring. */
 for(int k=1;k<=2;k++){
  world_init(&w,1,1);world_step(&w,start);memset(w.rings,0,sizeof w.rings);
  r=&w.rings[0];r->active=1;r->radius=90*Q16_ONE;r->speed=60*Q16_ONE;r->angle=64*256;r->openings=3;
  for(int j=0;j<3;j++){r->offset[j]=j*85;r->width[j]=40+j*8;}
  w.h[0].angle=ring_center(r,k,0);assert(ring_opening(r,w.h[0].angle,6)==k);
  world_step(&w,idle);assert(w.h[0].state==HS_RUN && w.h[0].anchor==1 && w.h[0].apples==1);
 }
 world_init(&w,4,1);world_step(&w,start);memset(w.rings,0,sizeof w.rings);
 r=&w.rings[0];r->active=1;r->radius=(ORBIT+10)*Q16_ONE;r->speed=20*Q16_ONE;r->gap=58;r->angle=0;
 world_step(&w,idle);assert(r->passed==15);
 for(int p=0;p<4;p++)assert(w.h[p].anchor==1 && w.h[p].state==HS_RUN && w.h[p].score==w.h[0].score);
 /* A missed opening eliminates only that player on the shared crossing frame. */
 world_init(&w,4,1);world_step(&w,start);memset(w.rings,0,sizeof w.rings);
 r=&w.rings[0];r->active=1;r->radius=(ORBIT+10)*Q16_ONE;r->speed=20*Q16_ONE;r->gap=58;r->angle=0;w.h[2].angle=64;
 world_step(&w,idle);assert(w.h[2].state==HS_HIT && w.events[2]==EV_HIT && r->passed==11);
 for(int p=0;p<4;p++)if(p!=2)assert(w.h[p].state==HS_RUN && w.h[p].anchor==1);
 puts("equal solo/multiplayer orbit and results, independent failures, 1-3 varied openings, all-angle reachability with reaction delay at maximum speed, safe contact intervals, collision sweeps, controls, 12 courses and determinism passed");return 0;
}
