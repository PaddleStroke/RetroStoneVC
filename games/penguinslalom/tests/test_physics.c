#include "game.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void course_checks(void){
 world w;world_init(&w,1,4);unsigned widths=0,spacings=0,kinds=0;row previous;
 course_row(&w,0,&previous);
 for(int id=1;id<240;id++){
  row r;course_row(&w,id,&r);int gap=r.distance-previous.distance;
  assert(gap>=219 && gap<=331 && r.left>=24 && r.right<=296);
  assert(course_index(&w,r.distance)==id && course_index(&w,r.distance-1)==id-1);
  if(id<12)widths|=1u<<((r.gap-104)/12);
  spacings|=1u<<((gap-219)/14);previous=r;
  slope_obstacle o;course_obstacle(&w,id,0,0,&o);kinds|=1u<<o.kind;
 }
 assert(widths==31 && spacings==511 && kinds==15);
 assert(course_speed(0)<course_speed(12000) && course_speed(12000)<course_speed(24000));
 assert(course_speed(0)>Q16_ONE*328/100 && course_speed(0)<Q16_ONE*332/100);
 assert(course_speed(24000)==course_speed(999999));
 for(int d=0;d<24000;d++)assert(course_speed(d+1)-course_speed(d)<=7);
 int idle[4]={0},start[4]={2};
 for(int kind=0;kind<PROP_COUNT;kind++){
  world_init(&w,1,4);world_step(&w,start);slope_obstacle o;course_obstacle(&w,kind,0,0,&o);assert(o.kind==kind);
  assert(obstacle_hits(&o,&o,o.x,o.x,o.distance-120,o.distance+120));
  assert(!obstacle_hits(&o,&o,o.x+o.rx+SKI_RADIUS+2,o.x+o.rx+SKI_RADIUS+2,o.distance-120,o.distance+120));
  w.scroll=o.distance-1;w.travel=(int64_t)w.scroll*Q16_ONE;
  course_obstacle(&w,kind,w.scroll,0,&o);w.scroll=o.distance-1;w.travel=(int64_t)w.scroll*Q16_ONE;
  w.h[0].distance=w.scroll;w.h[0].wx=w.travel;w.h[0].x=o.x;w.h[0].xx=o.x*Q16_ONE;w.h[0].score=77;
  world_step(&w,idle);assert(w.h[0].state==HS_HIT && w.h[0].reason==2 && w.events[0]==EV_HIT);
  assert(w.h[0].anchor==0 && w.h[0].apples==0 && w.h[0].score==77);
 }
 slope_obstacle a,b;row r;course_row(&w,3,&r);
 course_obstacle(&w,3,r.distance-180,0,&a);course_obstacle(&w,3,r.distance,25,&b);
 assert(b.distance==a.distance && b.x!=a.x && a.kind==PROP_GOAT);
 assert(obstacle_hits(&a,&b,a.x,b.x,a.distance,b.distance));
 world_init(&w,1,4);world_step(&w,start);w.bear_wait=1;
 world_step(&w,idle);assert(w.bears[0].active);int64_t first=w.bears[0].distance-w.travel;
 assert(first==-240*Q16_ONE);
 int x=w.bears[0].x;
 for(int t=0;t<160;t++){
  int input[4]={bot_decide(&w,0)};world_step(&w,input);
  assert(w.bears[0].active && w.bears[0].x==x);
  assert(w.bears[0].distance-w.travel==first+(int64_t)(t+1)*3*Q16_ONE);
 }
 assert(w.bears[0].distance>w.travel+200*Q16_ONE);
 for(int t=0;t<20;t++){int input[4]={bot_decide(&w,0)};world_step(&w,input);}
 assert(!w.bears[0].active);
 /* An overtaking bear uses swept collision and never awards a gate ding. */
 world_init(&w,1,4);world_step(&w,start);w.bear_wait=10000;w.h[0].score=77;
 w.bears[0]=(sliding_bear){1,0,160,-40*Q16_ONE,-40*Q16_ONE};
 world_step(&w,idle);assert(w.h[0].state==HS_HIT && w.h[0].reason==2 && w.events[0]==EV_HIT && w.h[0].score==77);
 int64_t stopped=w.bears[0].distance;for(int t=0;t<50;t++)world_step(&w,idle);assert(w.bears[0].distance==stopped);
}
int main(void){world a,b,w;int idle[4]={0},start[4]={2},left[4]={-1},right[4]={1};
 course_checks();
 world_init(&w,1,4);for(int t=0;t<60;t++)world_step(&w,idle);assert(!w.started);
 world_step(&w,start);for(int t=0;t<5;t++)world_step(&w,left);assert(w.h[0].x<160);
 world_init(&w,1,4);world_step(&w,start);for(int t=0;t<5;t++)world_step(&w,right);assert(w.h[0].x>160);
 world_init(&w,1,4);world_step(&w,start);for(int t=0;t<900;t++)world_step(&w,idle);assert(world_all_down(&w));
 for(unsigned seed=1;seed<=12;seed++){
  world_init(&a,4,seed);world_init(&b,4,seed);
  for(int t=0;t<7200;t++){int input[4];for(int p=0;p<4;p++)input[p]=bot_decide(&a,p);
   world_step(&a,input);world_step(&b,input);assert(!memcmp(&a,&b,sizeof a));
   for(int p=0;p<4;p++)if(a.h[p].state!=HS_RUN){
    printf("FAIL seed %u t %d p %d d %d x %d reason %d\n",seed,t,p,a.h[p].distance,a.h[p].x,a.h[p].reason);
    int base=course_index(&a,a.h[p].distance);for(int id=base>0?base-1:0;id<=base+1;id++){slope_obstacle o;row gate;course_row(&a,id,&gate);course_obstacle(&a,id,a.scroll,a.t,&o);printf("obstacle %d kind %d x %d d %d; gate %d center %d left %d right %d\n",id,o.kind,o.x,o.distance,gate.distance,gate.center,gate.left,gate.right);}
    for(int i=0;i<MAX_BEARS;i++)if(a.bears[i].active){slope_obstacle o;bear_obstacle(&a,i,0,&o);printf("bear x %d d %d\n",o.x,o.distance);}
    return 1;
   }}
  for(int p=0;p<4;p++)assert(a.h[p].wx-a.travel==(int64_t)player_offset(4,p)*Q16_ONE);
  assert(a.h[0].anchor>70 && a.h[0].apples>10);printf("seed %u: %d gates, %d fish, score %d\n",seed,a.h[0].anchor,a.h[0].apples,a.h[0].score);
 }
 row r;world_init(&w,1,4);world_step(&w,start);course_row(&w,0,&r);
 w.travel=(int64_t)(r.distance-1)*Q16_ONE;w.h[0].wx=w.travel;w.h[0].distance=r.distance-1;
 w.h[0].x=r.right+35;w.h[0].xx=w.h[0].x*Q16_ONE;w.h[0].score=77;
 world_step(&w,idle);assert(w.h[0].state==HS_HIT && w.events[0]==EV_HIT);
 assert(w.h[0].anchor==0 && w.h[0].apples==0 && w.h[0].score==77);
 int stopped=w.scroll,time=w.t;for(int t=0;t<50;t++)world_step(&w,idle);
 assert(w.scroll==stopped && w.t==time && world_all_down(&w));
 world_init(&w,1,4);world_step(&w,start);w.travel=(int64_t)(r.distance-1)*Q16_ONE;w.h[0].wx=w.travel;w.h[0].distance=r.distance-1;
 w.h[0].x=r.center;w.h[0].xx=r.center*Q16_ONE;world_step(&w,idle);
 assert(w.h[0].state==HS_RUN && w.h[0].anchor==1 && (w.events[0]&EV_SCORE) && (w.events[0]&EV_APPLE));
 puts("1000-point starting pace, fixed-height sideways goats, top-to-bottom overtaking bears and swept collisions, variable gates, smooth acceleration, fixed obstacle collisions, steering, immediate silent gate misses, frozen course, valid gates, shared speed and 12 seeded four-player courses passed");return 0;
}
