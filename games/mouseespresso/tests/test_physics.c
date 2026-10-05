#include "game.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int find_obstacle(world *w,int type,int variant,int side){
 for(int id=0;id<150;id++){obstacle o;obstacle_at(w,id,0,&o);if(o.type==type && (variant<0 || variant==o.variant) && (side<0 || side==o.side))return id;}
 assert(0);return -1;
}
static void at_phase(world *w,int id,int phase,int offset,obstacle *o){
 obstacle_at(w,id,0,o);int t=(phase-id*offset%o->period+o->period)%o->period;obstacle_at(w,id,t,o);
}
static void active_obstacles(void){
 world w;world_init(&w,1,4);obstacle o;
 for(int side=0;side<=1;side++){
  int id=find_obstacle(&w,OB_PAW,-1,side);at_phase(&w,id,0,47,&o);
  assert(!o.reach && !o.old_reach && !hazard_hits(&o,o.x,o.x,80,80));int period=o.period;
  at_phase(&w,id,period-56,47,&o);assert(o.warning && o.reach>=24 && !o.active);
  assert(!hazard_hits(&o,o.x,o.x,side?FLIGHT_BOTTOM:FLIGHT_TOP,side?FLIGHT_BOTTOM:FLIGHT_TOP));
  at_phase(&w,id,period-48,47,&o);int start=o.reach;at_phase(&w,id,period-42,47,&o);
  assert(o.active && o.reach>120 && o.reach>start+60);
  int palm=side?o.tip+13:o.tip-13;assert(hazard_hits(&o,o.tip_x,o.tip_x,palm,palm));
  assert(!hazard_hits(&o,o.x,o.x,side?40:200,side?40:200));
  at_phase(&w,id,period-18,47,&o);assert(o.reach>0 && o.reach<start*4);
  at_phase(&w,id,period-4,47,&o);assert(!o.reach && !o.old_reach && !hazard_hits(&o,o.x,o.x,palm,palm));
 }
 /* Every difficulty level keeps a readable, harmless lead-in on both edges. */
 int sides=0,fast=0;
 for(int id=0;id<150;id++){
  obstacle_at(&w,id,0,&o);if(o.type!=OB_PAW)continue;
  sides|=1<<o.side;fast|=o.period==136;
  int period=o.period,attack=period-PAW_ATTACK_FRAMES,previous=0,turns=0,direction=0;
  at_phase(&w,id,attack-PAW_WARNING_FRAMES-1,47,&o);assert(!o.warning && !o.reach);
  for(int frame=0;frame<PAW_WARNING_FRAMES;frame++){
   at_phase(&w,id,attack-PAW_WARNING_FRAMES+frame,47,&o);
   assert(o.warning && !o.active && o.reach>0 && o.reach<=34);
   if(frame>=12)assert(o.reach>=24);
   int palm=o.side?o.tip+13:o.tip-13;
   assert(!hazard_hits(&o,o.tip_x,o.tip_x,palm,palm));
   assert(!hazard_hits(&o,o.x,o.x,o.side?FLIGHT_BOTTOM:FLIGHT_TOP,o.side?FLIGHT_BOTTOM:FLIGHT_TOP));
   int change=o.reach-previous,sign=(change>0)-(change<0);
   if(frame>12 && sign && direction && sign!=direction)turns++;
   if(sign)direction=sign;
   previous=o.reach;
  }
  assert(turns>=5);
  at_phase(&w,id,attack,47,&o);assert(!o.warning && o.active && o.reach>previous);
  int palm=o.side?o.tip+13:o.tip-13;
  assert(hazard_hits(&o,o.tip_x,o.tip_x,palm,palm));
 }
 assert(sides==3 && fast);
 for(int variant=0;variant<=2;variant+=2){
  int id=find_obstacle(&w,OB_APPLIANCE,variant,-1);at_phase(&w,id,0,41,&o);int period=o.period;
  assert(!o.effect && !hazard_hits(&o,o.jet_x,o.jet_x,o.jet_y-40,o.jet_y-40));
  at_phase(&w,id,period-72,41,&o);assert(o.warning && !o.effect);
  at_phase(&w,id,period-52,41,&o);assert(o.active && o.effect==72);
  assert(hazard_hits(&o,o.jet_x,o.jet_x,o.jet_y-40,o.jet_y-40));
  assert(!hazard_hits(&o,o.jet_x,o.jet_x,o.jet_y-100,o.jet_y-100));
  at_phase(&w,id,period-4,41,&o);assert(!o.effect && !hazard_hits(&o,o.jet_x,o.jet_x,o.jet_y-40,o.jet_y-40));
 }
 memset(&o,0,sizeof o);o.type=OB_APPLIANCE;o.variant=1;o.x=300;o.width=23;o.tip=150;
 o.blade[0]=(moving_blade){282,190,244,190,244,190};
 assert(hazard_hits(&o,250,250,190,190));assert(!hazard_hits(&o,244,244,160,160));
 o.blade[0]=(moving_blade){282,190,244,220,244,160};assert(hazard_hits(&o,250,250,190,190));
 int id=find_obstacle(&w,OB_APPLIANCE,1,-1);obstacle_at(&w,id,0,&o);moving_blade first=o.blade[0];obstacle_at(&w,id,16,&o);
 assert(o.blade[0].tip_x!=first.tip_x || o.blade[0].tip_y!=first.tip_y);
 memset(&o,0,sizeof o);o.type=OB_TABLE;o.x=300;o.width=104;o.tip=128;
 assert(hazard_hits(&o,150,450,133,133));
 assert(!hazard_hits(&o,150,450,95,95) && !hazard_hits(&o,150,450,178,178));
 puts("cat warning/swat/full retreat, steam cycles, rotating blade sweeps and both table routes passed");
}
int main(void) {
 active_obstacles();
 world a,b,w;int idle[4]={0},held[4]={1,1,1,1};
 world_init(&w,1,4);for(int i=0;i<200;i++)world_step(&w,idle);assert(!w.started);
 world_step(&w,held);int first=w.h[0].vy;for(int i=0;i<5;i++)world_step(&w,held);
 assert(w.h[0].vy<first && w.h[0].y<FLIGHT_BOTTOM*Q16_ONE);
 for(int i=0;i<10;i++)world_step(&w,idle);
 assert(w.h[0].vy>0);
 world_init(&w,1,4);world_step(&w,held);for(int i=0;i<900;i++)world_step(&w,idle);assert(world_all_down(&w));
 world_init(&w,1,4);for(int i=0;i<7200;i++)world_step(&w,held);assert(world_all_down(&w));
 for(unsigned seed=1;seed<=12;seed++) {
  world_init(&a,4,seed);world_init(&b,4,seed);
  for(int t=0;t<7200;t++) {
   int input[4];for(int p=0;p<4;p++)input[p]=bot_decide(&a,p);
   world_step(&a,input);world_step(&b,input);assert(!memcmp(&a,&b,sizeof a));
   for(int p=0;p<4;p++) {
    if(a.h[p].state!=HS_RUN){printf("FAIL seed %u frame %d x %lld y %d hazard %d\n",seed,t,(long long)(a.h[p].wx>>16),a.h[p].y>>16,a.h[p].anchor);return 1;}
    assert(a.h[p].y>=FLIGHT_TOP*Q16_ONE && a.h[p].y<=FLIGHT_BOTTOM*Q16_ONE);
    assert(a.h[p].wx-a.travel==(int64_t)hero_start_x(4,p)*Q16_ONE);
    assert(a.h[p].vx==a.h[0].vx);
   }
  }
  printf("seed %u: %d obstacles, %d biscuits, %d score; 4 players / 120 seconds\n",seed,a.h[0].anchor,a.h[0].apples,a.h[0].score);
  assert(a.h[0].anchor>50 && a.h[0].apples>10 && a.h[0].score>1500);
 }
 obstacle o={0};o.type=OB_SHELF;o.x=300;o.tip=100;o.width=34;
 assert(hazard_hits(&o,240,360,80,80));assert(!hazard_hits(&o,240,360,180,180));
 o.type=OB_POP;o.shot[0]=(toast_shot){300,120,300,126,1,20};
 assert(hazard_hits(&o,290,310,120,120));assert(!hazard_hits(&o,290,310,176,176));
 int kinds=0;world_init(&w,1,4);for(int id=0;id<60;id++){obstacle_at(&w,id,0,&o);kinds|=1<<o.type;}assert(kinds==(1<<OB_COUNT)-1);
 obstacle_at(&w,1,0,&o);int launch=(o.period-37%o.period)%o.period;
 obstacle_at(&w,1,launch,&o);assert(o.shot[0].active && o.shot[0].y==213 && o.shot[0].x==o.x-5);
 int origin=o.x;obstacle_at(&w,1,launch+10,&o);assert(o.shot[0].x<origin && o.shot[1].x>origin && o.shot[0].y<200);
 obstacle_at(&w,1,launch+38,&o);assert(o.shot[0].y<110);
 assert(course_speed(20000)>course_speed(0));
 world_init(&w,1,4);w.h[0].y=FLIGHT_TOP*Q16_ONE;
 for(int i=0;i<30;i++){world_step(&w,held);assert(w.h[0].y==FLIGHT_TOP*Q16_ONE);}
 puts("continuous thrust/coast, boundaries, idle/held failures, collision sweeps, courses and determinism passed");return 0;
}
