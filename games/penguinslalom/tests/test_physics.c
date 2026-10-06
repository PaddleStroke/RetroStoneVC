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
 /* A bear is warned first: the marker keeps its spot on the screen (land px from the camera), harmless, for the
  * warning's frames; then the bear lands there and slides down at BEAR_SLIDE px per frame relative, to the bottom. */
 assert(bear_warn_time(0)==BEAR_WARN_EARLY && bear_warn_time(1<<30)==BEAR_WARN_LATE && BEAR_WARN_LATE>=BEAR_WARN_MIN);
 for(int d=0;d<40000;d+=500)assert(bear_warn_time(d+500)<=bear_warn_time(d) && bear_warn_time(d)>=BEAR_WARN_MIN);
 for(int n=1;n<=4;n++)assert(bear_land(n)<=player_offset(n,0)-SKI_FORWARD_RADIUS-24-BEAR_SLIDE*4 && bear_land(n)>=BEAR_LAND_TOP);
 world_init(&w,1,4);world_step(&w,start);w.bear_wait=1;
 world_step(&w,idle);assert(w.bears[0].active && w.bear_warned==w.bears[0].x && !w.bear_landed);
 const int warn=w.bears[0].warn,x=w.bears[0].x,land=bear_land(1);
 assert(warn==BEAR_WARN_EARLY && w.bears[0].warn_len==warn && x>=BEAR_X_MIN && x<=BEAR_X_MAX);
 assert(w.bears[0].distance-w.travel==(int64_t)land*Q16_ONE);
 for(int t=1;t<=warn;t++){
  w.h[0].state=HS_RUN;world_step(&w,idle);
  assert(w.bears[0].active && w.bears[0].x==x && w.bears[0].warn==warn-t && !w.bear_warned);
  assert(w.bears[0].distance-w.travel==(int64_t)land*Q16_ONE && !!w.bear_landed==(t==warn));
 }
 assert(w.bear_landed==x && w.bears[0].age==0);
 for(int t=1;w.bears[0].active;t++){
  w.h[0].state=HS_RUN;world_step(&w,idle);
  if(w.bears[0].active)assert(w.bears[0].x==x && w.bears[0].age==t && w.bears[0].distance-w.travel==(int64_t)(land+BEAR_SLIDE*t)*Q16_ONE);
  else assert(land+BEAR_SLIDE*t>BEAR_RETIRE && land+BEAR_SLIDE*(t-1)<=BEAR_RETIRE);
 }
 /* The marker is harmless, even right on the penguin; the bear is not, once it lands. An overtaking bear uses swept
  * collision and never awards a gate ding. */
 world_init(&w,1,4);world_step(&w,start);w.bear_wait=10000;w.h[0].score=77;
 w.bears[0]=(sliding_bear){.active=1,.x=160,.warn=2,.warn_len=2,.land=0,.distance=w.travel,.previous=w.travel};
 world_step(&w,idle);assert(w.h[0].state==HS_RUN && w.bears[0].warn==1);
 world_step(&w,idle);assert(w.bears[0].warn==0 && w.bear_landed==160 && w.h[0].state==HS_HIT && w.h[0].reason==2);
 world_init(&w,1,4);world_step(&w,start);w.bear_wait=10000;w.h[0].score=77;
 w.bears[0]=(sliding_bear){.active=1,.x=160,.distance=-40*Q16_ONE,.previous=-40*Q16_ONE};
 world_step(&w,idle);assert(w.h[0].state==HS_HIT && w.h[0].reason==2 && w.events[0]==EV_HIT && w.h[0].score==77);
 int64_t stopped=w.bears[0].distance;for(int t=0;t<50;t++)world_step(&w,idle);assert(w.bears[0].distance==stopped);
 /* Warned bears land across the whole course, the first ones within a few seconds; the spots are the seed's. */
 unsigned spots=0;int bears=0;
 for(unsigned seed=1;seed<=8;seed++){
  world c;world_init(&w,2,seed);world_init(&c,2,seed);world_step(&w,start);world_step(&c,start);
  for(int t=0;t<3000;t++){
   for(int p=0;p<2;p++){w.h[p].state=HS_RUN;c.h[p].state=HS_RUN;}
   world_step(&w,idle);world_step(&c,idle);assert(w.bear_warned==c.bear_warned);
   if(w.bear_warned){bears++;spots|=1u<<(w.bear_warned-24)*8/273;}
  }
 }
 assert(spots==255 && bears>=40);
}
int main(void){world a,b,w;int idle[4]={0},start[4]={2},left[4]={-1},right[4]={1};
 course_checks();
 world_init(&w,1,4);for(int t=0;t<60;t++)world_step(&w,idle);assert(!w.started);
 world_step(&w,start);for(int t=0;t<5;t++)world_step(&w,left);assert(w.h[0].x<160);
 world_init(&w,1,4);world_step(&w,start);for(int t=0;t<5;t++)world_step(&w,right);assert(w.h[0].x>160);
 world_init(&w,1,4);world_step(&w,start);for(int t=0;t<900;t++)world_step(&w,idle);assert(world_all_down(&w));
 /* Twelve seeded four-player courses, two minutes each, with four ordinary-input bots reading the screen (the
  * bears' markers included): replays are identical; bears land on the racing line now, so a bot may fall, but
  * nearly all of them finish and every one clears gates and fish first. */
 int finished=0;
 for(unsigned seed=1;seed<=12;seed++){
  world_init(&a,4,seed);world_init(&b,4,seed);
  for(int t=0;t<7200;t++){int input[4];for(int p=0;p<4;p++)input[p]=bot_decide(&a,p);
   world_step(&a,input);world_step(&b,input);assert(!memcmp(&a,&b,sizeof a));
  }
  printf("seed %2u:",seed);
  for(int p=0;p<4;p++){
   const hero *h=&a.h[p];printf(" P%d %s %d gates %d fish %d;",p+1,h->state==HS_RUN?"ran":"fell after",h->anchor,h->apples,h->score);
   if(h->state==HS_RUN){finished++;assert(h->wx-a.travel==(int64_t)player_offset(4,p)*Q16_ONE && h->anchor>90 && h->apples>60);}
   else assert(h->anchor>=20 && h->reason);
  }
  printf("\n");
 }
 printf("%d of 48 bots finished the two minutes\n",finished);assert(finished>=40);
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
 puts("1000-point starting pace, fixed-height sideways goats, warned bears landing across the course and sliding over the lineup, swept collisions, variable gates, smooth acceleration, fixed obstacle collisions, steering, immediate silent gate misses, frozen course, valid gates, shared speed and 12 seeded four-player courses passed");return 0;
}
