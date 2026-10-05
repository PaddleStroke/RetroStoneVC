#include "game.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void empty(world *w){memset(w->shells,0,sizeof w->shells);w->shells[55]=(shell){20,1,0,48*Q16_ONE,Q16_ONE*35/1000};w->spawn_t=10000;}
static int shell_count(const world *w){int count=0;for(int i=0;i<SHELLS;i++)count+=w->shells[i].hp>0;return count;}
static void rain_checks(void){
 int start[4]={2},idle[4]={0};
 for(unsigned seed=1;seed<=12;seed++){
  world w;world_init(&w,4,seed);assert(shell_count(&w)==18);int xmin=320,xmax=0,ymin=240,ymax=0,fast=0,slow=Q16_ONE;
  for(int i=0;i<SHELLS;i++)if(w.shells[i].hp){
   shell *s=&w.shells[i];int y=s->y/Q16_ONE,same_height=0;
   if(s->x<xmin)xmin=s->x;
   if(s->x>xmax)xmax=s->x;
   if(y<ymin)ymin=y;
   if(y>ymax)ymax=y;
   if(s->fall<slow)slow=s->fall;
   if(s->fall>fast)fast=s->fall;
   for(int j=0;j<SHELLS;j++)if(w.shells[j].hp){
    same_height+=w.shells[j].y/Q16_ONE==y;
    if(i!=j)assert(abs(w.shells[j].x-s->x)>=29 || abs(w.shells[j].y/Q16_ONE-y)>=19);
   }
   assert(same_height<6);
  }
  assert(xmin<100 && xmax>220 && ymax-ymin>50 && fast>slow);
  world_step(&w,start);int count=shell_count(&w),serial=w.wave;w.spawn_t=1;
  world_step(&w,idle);assert(shell_count(&w)==count+1 && w.wave==serial+1);
  int incoming=0;for(int i=0;i<SHELLS;i++)incoming+=w.shells[i].hp && w.shells[i].y<0;
  assert(incoming==1 && w.spawn_t>0 && w.spawn_t<200);
  /* Clearing the field brings in a single falling target, never a new row. */
  memset(w.shells,0,sizeof w.shells);w.spawn_t=10000;
  world_step(&w,idle);assert(shell_count(&w)==1);
  shell *s=NULL;for(int i=0;i<SHELLS;i++)if(w.shells[i].hp)s=&w.shells[i];assert(s && s->y<0);
  int32_t before=s->y;world_step(&w,idle);assert(s->y-before==s->fall);
 }
 for(int n=1;n<=4;n++)for(int p=0;p<n;p++){
  assert(crab_y(n,p)+16<=240 && crab_y(n,p)-16>=184);
  assert(paddle_y(n,p)==crab_y(n,p)-15);
 }
}
int main(void){
 rain_checks();
 world w,a,b;int start[4]={2},idle[4]={0},left[4]={-1};world_init(&w,1,4);world_step(&w,start);empty(&w);
 w.h[0].bx=170*Q16_ONE;w.h[0].by=(PADDLE_Y-1)*Q16_ONE;w.h[0].bvx=0;w.h[0].bvy=4*Q16_ONE;world_step(&w,idle);assert(w.h[0].bvy<0 && w.h[0].bvx>0 && w.h[0].launches==1);
 w.h[0].bx=150*Q16_ONE;w.h[0].by=(PADDLE_Y-1)*Q16_ONE;w.h[0].bvx=0;w.h[0].bvy=4*Q16_ONE;world_step(&w,idle);assert(w.h[0].bvx<0 && w.h[0].launches==2);
 w.h[0].bx=(BALL_LEFT+1)*Q16_ONE;w.h[0].by=110*Q16_ONE;w.h[0].bvx=-4*Q16_ONE;w.h[0].bvy=0;world_step(&w,idle);assert(w.h[0].bvx>0);
 w.h[0].bx=160*Q16_ONE;w.h[0].by=(BALL_TOP+1)*Q16_ONE;w.h[0].bvx=0;w.h[0].bvy=-4*Q16_ONE;world_step(&w,idle);assert(w.h[0].bvy>0);
 w.shells[0]=(shell){160,1,0,90*Q16_ONE,Q16_ONE*35/1000};w.h[0].bx=160*Q16_ONE;w.h[0].by=102*Q16_ONE;w.h[0].bvx=0;w.h[0].bvy=-4*Q16_ONE;world_step(&w,idle);assert(w.shells[0].hp==0 && w.h[0].score>0 && w.h[0].bvy>0);
 w.h[0].bx=40*Q16_ONE;w.h[0].by=BALL_LOST*Q16_ONE;w.h[0].bvy=4*Q16_ONE;world_step(&w,idle);assert(w.h[0].state==HS_HIT && w.h[0].reason==1);
 world_init(&w,1,4);world_step(&w,start);w.shells[0].y=SHELL_LIMIT*Q16_ONE;w.h[0].score=77;
 world_step(&w,idle);assert(w.h[0].reason==2 && w.shells[0].hp>0 && w.events[0]==EV_HIT);
 int serial=w.wave,clock=w.t;int32_t before=w.shells[0].y;
 world_step(&w,idle);assert(w.shells[0].y>before && w.shells[0].hp>0 && !w.events[0]);
 for(int t=0;t<100;t++){
  if(w.shells[0].hp){assert(w.shells[0].y<248*Q16_ONE);}
  world_step(&w,idle);
  assert(w.wave==serial && w.t==clock && w.h[0].score==77);
 }
 assert(w.shells[0].hp==0 && w.shells[0].y>=248*Q16_ONE && shell_count(&w)==0);
 world_init(&w,1,4);world_step(&w,start);for(int t=0;t<60;t++)world_step(&w,left);assert(w.h[0].x==24);
 world_init(&w,1,4);world_step(&w,start);for(int t=0;t<900;t++)world_step(&w,idle);assert(world_all_down(&w));
 world_init(&w,2,4);world_step(&w,start);empty(&w);w.shells[0]=(shell){160,1,0,90*Q16_ONE,Q16_ONE*35/1000};
 for(int p=0;p<2;p++){w.h[p].bx=160*Q16_ONE;w.h[p].by=102*Q16_ONE;w.h[p].bvx=0;w.h[p].bvy=-4*Q16_ONE;}
 world_step(&w,idle);assert(w.shells[0].hp==0 && w.h[0].apples==1 && w.h[1].apples==0 && w.h[0].bvy>0 && w.h[1].bvy<0);
 assert(ball_speed(0)<2*Q16_ONE && ball_speed(3600)>ball_speed(1800) && ball_speed(7200)>ball_speed(3600) && ball_speed(9000)==ball_speed(7200));
 for(unsigned seed=1;seed<=12;seed++){
  world_init(&a,4,seed);world_init(&b,4,seed);int frames[4]={0};
  for(int t=0;t<7200;t++){
   int input[4];for(int p=0;p<4;p++)input[p]=bot_decide(&a,p);world_step(&a,input);world_step(&b,input);assert(!memcmp(&a,&b,sizeof a));
   for(int p=0;p<4;p++)if(a.h[p].state==HS_RUN)frames[p]++;
  }
  printf("seed %u: alive %d,%d,%d,%d, shells %d,%d,%d,%d, rebounds %d, score %d\n",seed,frames[0],frames[1],frames[2],frames[3],a.h[0].apples,a.h[1].apples,a.h[2].apples,a.h[3].apples,a.h[0].launches,a.h[0].score);
  for(int p=0;p<4;p++)assert(frames[p]==7200 && a.h[p].apples>10 && a.h[p].launches>20);
 }
 puts("scattered non-overlapping start, individual random shell arrivals, falling motion, full-size claw alignment, full court, gradual speed, shared shell hits, independent balls, rebounds and seeded multiplayer passed");return 0;
}
