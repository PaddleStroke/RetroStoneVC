/* Mouse Espresso: the flight, the cafe items' collisions and motion, the director's course and determinism.
 * (The passability and difficulty of the generated courses: tests/validate.c.) */
#include "game.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static item make(int kind,int x,int y,int len,int size,int var){
 item it;memset(&it,0,sizeof it);it.kind=(uint8_t)kind;it.x=x;it.y=(int16_t)y;it.len=(int16_t)len;it.size=(int16_t)size;it.var=(uint8_t)var;return it;
}
static int hits_at(const item *it,int t,int x,int y){item_pose p;item_pose_at(it,t,&p);return item_hits(it,&p,x,x,y,y);}
static void items(void){
 /* hanging: the string and the item are solid, below is free */
 item lamp=make(IT_LAMP,300,0,60,10,0);
 assert(hits_at(&lamp,0,300,30) && hits_at(&lamp,0,300,66) && hits_at(&lamp,0,310,64));
 assert(!hits_at(&lamp,0,300,82) && !hits_at(&lamp,0,322,40));
 item bag=make(IT_TEABAG,300,0,40,0,0),cup=make(IT_CUPHANG,300,0,40,0,0);
 assert(hits_at(&bag,0,300,50) && !hits_at(&bag,0,300,62) && hits_at(&cup,0,300,48) && !hits_at(&cup,0,300,60));
 /* standing: cups of 11 px, sugar cubes of 8, the jug */
 item cups=make(IT_CUPS,300,0,0,0,4),sugar=make(IT_SUGAR,300,0,0,1,6),jug=make(IT_JUG,300,0,30,0,0);
 assert(hits_at(&cups,0,300,COUNTER_Y-44) && !hits_at(&cups,0,300,COUNTER_Y-55));
 assert(hits_at(&sugar,0,300,COUNTER_Y-50) && !hits_at(&sugar,0,300,COUNTER_Y-57) && !hits_at(&sugar,0,314,200));
 assert(hits_at(&jug,0,300,COUNTER_Y-34) && !hits_at(&jug,0,300,COUNTER_Y-40));
 /* floating: the spoon's handle and bowl, the bean, the cake, the croissant; space around them is free */
 item spoon=make(IT_SPOON,300,120,0,12,0);
 assert(hits_at(&spoon,0,290,128) && hits_at(&spoon,0,312,131) && !hits_at(&spoon,0,300,138) && !hits_at(&spoon,0,300,100));
 item diag=make(IT_SPOON,300,120,0,12,1);assert(hits_at(&diag,0,300,120) && !hits_at(&diag,0,284,96));
 item bean=make(IT_BEAN,300,120,0,5,0),cake=make(IT_CAKE,300,120,0,10,0),croissant=make(IT_CROISSANT,300,120,0,11,0);
 assert(hits_at(&bean,0,300,128) && !hits_at(&bean,0,300,132) && hits_at(&cake,0,300,134) && !hits_at(&cake,0,300,99));
 assert(hits_at(&croissant,0,318,120) && !hits_at(&croissant,0,321,120));
 /* the steam jet: harmless telegraph, then it rises, holds and falls back; the nozzle is always solid */
 item jet=make(IT_JET,300,0,100,50,0);jet.period=120;
 item_pose p;item_pose_at(&jet,0,&p);assert(p.warn && !p.on && !hits_at(&jet,0,300,150));
 assert(hits_at(&jet,0,300,COUNTER_Y-6));
 item_pose_at(&jet,JET_TELEGRAPH+JET_RISE+5,&p);assert(p.on && p.h==100 && hits_at(&jet,JET_TELEGRAPH+JET_RISE+5,300,150));
 assert(!hits_at(&jet,JET_TELEGRAPH+JET_RISE+5,300,COUNTER_Y-8-100-HERO_RADIUS_Y-3));
 item_pose_at(&jet,JET_TELEGRAPH+60,&p);assert(!p.on && !p.warn && !hits_at(&jet,JET_TELEGRAPH+60,300,150));
 int rising=0;for(int t=JET_TELEGRAPH;t<JET_TELEGRAPH+JET_RISE;t++){item_pose_at(&jet,t,&p);rising|=p.h>0 && p.h<100;}
 assert(rising);
 item cjet=make(IT_JET,300,0,80,50,1);cjet.period=120;assert(hits_at(&cjet,JET_TELEGRAPH+20,300,60) && !hits_at(&cjet,JET_TELEGRAPH+20,300,110));
 /* the ladle swings: the bowl moves, the handle is solid, the space it left is free */
 item ladle=make(IT_LADLE,300,0,80,30,0);ladle.period=120;
 item_pose a,b;item_pose_at(&ladle,0,&a);item_pose_at(&ladle,30,&b);assert(a.tx!=b.tx);
 assert(hits_at(&ladle,30,b.tx,b.ty) && hits_at(&ladle,30,300,10));
 assert(!hits_at(&ladle,30,300+(300-b.tx),b.ty) || abs(300-b.tx)<16);
 assert(!hits_at(&ladle,0,300,80+LADLE_BOWL+HERO_RADIUS_Y+3));
 /* the bouncing sugar cube: on the counter, up to its apex, back */
 item cube=make(IT_BOUNCE,300,100,0,0,0);cube.period=90;
 item_pose_at(&cube,0,&p);assert(p.ty==COUNTER_Y-5);item_pose_at(&cube,45,&p);assert(p.ty==105);
 assert(hits_at(&cube,45,300,105) && !hits_at(&cube,45,300,85) && !hits_at(&cube,0,300,105));
 /* the dripping filter: a drop falls; the column is free between drops */
 item filt=make(IT_FILTER,300,0,40,0,0);filt.period=60;
 int dropped=0,free_col=0;for(int t=0;t<60;t++){item_pose_at(&filt,t,&p);dropped|=p.drops>0;free_col|=!hits_at(&filt,t,300,150);}
 assert(dropped && free_col);
 /* swept collisions: a thin string between two frames is hit */
 item s2=make(IT_TEABAG,300,0,150,0,0);item_pose_at(&s2,0,&p);assert(item_hits(&s2,&p,280,320,100,100));
 puts("items: strings, stacks, floating shapes, jets (telegraph, rise, hold), ladle swings, bouncing cubes, drips: passed");
}
static void courses(void){
 static course a,b;
 for(uint32_t seed=1;seed<=40;seed++){
  course_init(&a,seed);course_init(&b,seed);
  /* the same course whether it is generated in small steps (the game) or at once */
  for(int x=0;x<20000;x+=97){course_prune(&a,x-KEEP_BEHIND);course_fill(&a,x+GEN_AHEAD);
   assert(a.n-a.first<COURSE_N-48);
   for(uint32_t i=a.first+1;i<a.n;i++)assert(course_item(&a,i)->x>=course_item(&a,i-1)->x);}
  course_fill(&b,1000);
  int first=-1;for(uint32_t i=0;i<b.n;i++)if(course_item(&b,i)->kind!=IT_BISCUIT){first=course_item(&b,i)->x;break;}
  assert(abs(first-HAZARD_FIRST)<=2);
  for(uint32_t i=b.first;i<b.n && course_item(&b,i)->x<a.gen_x;i++){
   uint32_t j=i;if(j<a.first)continue;
   assert(!memcmp(course_item(&a,j),course_item(&b,j),sizeof(item)));
  }
 }
 /* every item and pattern appears; dynamic ones only after their unlocks */
 int kinds=0,pats=0;
 for(uint32_t seed=1;seed<=20;seed++){
  course_init(&a,seed);
  for(int x=0;x<24000;x+=200){course_prune(&a,x-KEEP_BEHIND);uint32_t n0=a.n,p0=a.npat;course_fill(&a,x+GEN_AHEAD);
   for(uint32_t i=n0;i<a.n;i++){const item *it=course_item(&a,i);kinds|=1<<it->kind;
    if(it->kind==IT_LADLE)assert(it->x>=HAZARD_FIRST+UNLOCK_LADLES);
    if(it->kind==IT_BOUNCE)assert(it->x>=HAZARD_FIRST+UNLOCK_BOUNCERS);
    if(it->kind==IT_JET)assert(it->x>=HAZARD_FIRST+UNLOCK_JETS);}
   for(uint32_t i=p0;i<a.npat;i++)pats|=1<<course_pattern(&a,i)->kind;}
 }
 assert(kinds==(1<<IT_KINDS)-1 && pats==(1<<PK_COUNT)-1);
 /* the target rises with distance, waves aside; the start stays easy */
 assert(course_target_k(HAZARD_FIRST+600,HAZARD_FIRST)<=K_EASY);
 assert(course_target_k(HAZARD_FIRST+20000,HAZARD_FIRST+20000)>course_target_k(HAZARD_FIRST+5000,HAZARD_FIRST+5000));
 assert(course_speed(SPEED_RAMP_PX*2)==SPEED_MAX && course_speed(0)==SPEED_START && course_speed(9000)>SPEED_START);
 puts("courses: the same in steps or at once, sorted, the first obstacle at HAZARD_FIRST, every item and pattern, unlocks, the ramp: passed");
}
int main(void){
 items();courses();
 world a,b,w;int idle[4]={0},held[4]={1,1,1,1};
 world_init(&w,1,4);for(int i=0;i<200;i++)world_step(&w,idle);assert(!w.started);
 world_step(&w,held);int first=w.h[0].vy;for(int i=0;i<5;i++)world_step(&w,held);
 assert(w.h[0].vy<first && w.h[0].y<FLIGHT_BOTTOM*Q16_ONE);
 for(int i=0;i<10;i++)world_step(&w,idle);
 assert(w.h[0].vy>0);
 /* idle on the counter, or held against the ceiling: the course stops the mouse */
 world_init(&w,1,4);world_step(&w,held);for(int i=0;i<1800;i++)world_step(&w,idle);assert(world_all_down(&w));
 world_init(&w,1,4);for(int i=0;i<3600;i++)world_step(&w,held);assert(world_all_down(&w));
 world_init(&w,1,4);w.h[0].y=FLIGHT_TOP*Q16_ONE;world_step(&w,held);
 for(int i=0;i<30;i++){world_step(&w,held);assert(w.h[0].y==FLIGHT_TOP*Q16_ONE || w.h[0].state!=HS_RUN);}
 /* four mice, the same speed and camera, independent heights; deterministic */
 for(unsigned seed=1;seed<=8;seed++){
  world_init(&a,4,seed);world_init(&b,4,seed);uint32_t r=seed*977u;
  for(int t=0;t<3000;t++){
   int input[4];for(int p=0;p<4;p++){r=r*1664525u+1013904223u;input[p]=(int)((r>>(8+p*5))&15)<7+p;}
   world_step(&a,input);world_step(&b,input);assert(!memcmp(&a,&b,sizeof a));
   for(int p=0;p<4;p++){
    if(a.h[p].state==HS_RUN)assert(a.h[p].y>=FLIGHT_TOP*Q16_ONE && a.h[p].y<=FLIGHT_BOTTOM*Q16_ONE);
    if(a.h[p].state==HS_RUN){assert(a.h[p].wx-a.travel==(int64_t)hero_start_x(4,p)*Q16_ONE);assert(a.h[p].vx==a.h[0].vx || a.h[0].state!=HS_RUN);}
   }
  }
 }
 /* biscuits: each mouse takes its own */
 world_init(&w,2,4);world_step(&w,held);
 for(uint32_t i=w.c.first;i<w.c.n;i++){item *it=&w.c.it[i%COURSE_N];if(it->kind!=IT_BISCUIT)continue;
  int64_t need=(int64_t)(it->x-hero_start_x(2,0))*Q16_ONE;
  while(w.travel+course_speed((int)(w.travel>>16))<need){w.travel+=course_speed((int)(w.travel>>16));}
  w.h[0].y=w.h[1].y=it->y*Q16_ONE;w.h[0].vy=w.h[1].vy=0;w.h[0].state=w.h[1].state=HS_RUN;
  for(int p=0;p<2;p++)w.h[p].wx=(int64_t)hero_start_x(2,p)*Q16_ONE+w.travel;   /* no sweep from where it was */
  int before=w.h[0].apples,b1=w.h[1].apples;world_step(&w,idle);
  assert(w.h[0].apples==before+1 && (it->taken&1) && w.h[1].apples==b1 && !(it->taken&2));break;}
 puts("continuous thrust/coast, boundaries, idle/held failures, four mice, determinism and biscuits passed");return 0;
}
