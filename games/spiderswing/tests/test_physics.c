#include "game.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static world run(int seed,int policy,int players){world w;world_init(&w,players,(uint32_t)seed);int input[4]={1,1,1,1};world_step(&w,input);
 for(int t=0;t<7200 && !world_all_down(&w);t++){for(int p=0;p<players;p++)input[p]=policy==8?bot_decide(&w,p):policy;hero before=w.h[0];world_step(&w,input);
  if(w.h[0].state==HS_RUN){assert(w.h[0].wx-before.wx<12*Q16_ONE && before.wx-w.h[0].wx<12*Q16_ONE);assert(w.h[0].y-before.y<14*Q16_ONE && before.y-w.h[0].y<14*Q16_ONE);}
 }return w;}
static void strand_fixture(world *w,int id,int offset){
 world_init(w,1,4);w->started=1;hero *h=&w->h[0];h->state=HS_RUN;h->mode=MODE_FLY;
 int ax,ay;anchor_pos(w,id,&ax,&ay);h->wx=(int64_t)(ax+offset)*Q16_ONE;h->y=(ay+70)*Q16_ONE;
 h->silk_x=h->wx;h->silk_y=(ay-10)*Q16_ONE;h->vx=h->vy=0;h->air_t=0;
}
static int touching(const world *w){int x,y,tx,ty;silk_segment(&w->h[0],&x,&y,&tx,&ty);return silk_catch(w,&w->h[0],x,y,tx,ty);}
static void catch_checks(void){
 world w;int idle[4]={0};strand_fixture(&w,1,0);assert(touching(&w)==1);
 /* The body is far below the blossom, but the visible strand crosses it. No hidden flight timer is required. */
 int64_t x=w.h[0].wx;int32_t y=w.h[0].y;world_step(&w,idle);
 assert(w.h[0].anchor==1 && w.h[0].mode==MODE_CATCH && (w.events[0]&EV_LAND));
 assert(w.h[0].wx==x && w.h[0].y>y && w.h[0].y-y<Q16_ONE && w.h[0].score==10);
 strand_fixture(&w,1,FLOWER_CATCH_RADIUS);assert(touching(&w)==1);
 strand_fixture(&w,1,FLOWER_CATCH_RADIUS+1);assert(touching(&w)<0);
 /* Crossing between frames must catch even when both individual frames miss. */
 int ax,ay;anchor_pos(&w,1,&ax,&ay);w.h[0].wx=(int64_t)(ax+20)*Q16_ONE;w.h[0].y=(ay+20)*Q16_ONE;
 w.h[0].silk_x=w.h[0].wx;w.h[0].silk_y=(ay-20)*Q16_ONE;assert(touching(&w)<0);
 assert(silk_catch(&w,&w.h[0],ax-20,ay+20,ax-20,ay-20)==1);
 /* Any higher blossom that the thread touches works, including a skipped target. */
 strand_fixture(&w,2,0);anchor_pos(&w,2,&ax,&ay);w.h[0].y=(ay+20)*Q16_ONE;assert(touching(&w)==2);
 w.h[0].anchor=2;assert(touching(&w)<0); /* Never reattach to the released flower. */
}
static int opening_window(int seed){
 int wins=0,streak=0,longest=0;
 for(int release=18;release<=200;release++){
  world w;world_init(&w,1,seed);int input[4]={INPUT_RELEASE};world_step(&w,input);input[0]=0;
  for(int t=1;t<release;t++)world_step(&w,input);
  input[0]=INPUT_RELEASE;world_step(&w,input);
  for(int t=0;t<110 && w.h[0].state==HS_RUN && !w.h[0].anchor;t++){
   int ax,ay;anchor_pos(&w,1,&ax,&ay);int x=(int)(w.h[0].wx/Q16_ONE);
   input[0]=x<ax-6?INPUT_RIGHT:x>ax+6?INPUT_LEFT:0;world_step(&w,input);
  }
  if(w.h[0].anchor){wins++;streak++;if(streak>longest)longest=streak;}else streak=0;
 }
 printf("seed %d: %d/183 simple release timings succeed, longest window %d frames\n",seed,wins,longest);
 assert(wins>=75 && longest>=18);return wins;
}
int main(void){
 catch_checks();
 world w,a,b;world_init(&w,1,4);int start[4]={1},idle[4]={0},left[4]={INPUT_LEFT},right[4]={INPUT_RIGHT};world_step(&w,start);
 for(int i=0;i<30;i++)world_step(&w,idle);
 a=w;int64_t x=w.h[0].wx;int32_t y=w.h[0].y,vx=w.h[0].vx,vy=w.h[0].vy;world_step(&w,start);
 assert(w.h[0].mode==MODE_FLY && w.h[0].wx==x && w.h[0].y==y && w.h[0].vx==vx && w.h[0].vy==vy);
 a=w;b=w;world_step(&a,left);world_step(&b,right);assert(a.h[0].vx<b.h[0].vx && a.h[0].wx<b.h[0].wx);
 int continued=0;for(int id=0;id<30;id++){int ax,ay,bx,by,cx,cy;anchor_pos(&w,id,&ax,&ay);anchor_pos(&w,id+1,&bx,&by);anchor_pos(&w,id+2,&cx,&cy);assert(by<ay && abs(bx-ax)<110);if((bx-ax)*(cx-bx)>0)continued++;}
 assert(continued>8); /* Includes runs on the same side rather than forced left/right alternation. */
 for(int seed=1;seed<=12;seed++){opening_window(seed);world idle_run=run(seed,0,1),hold=run(seed,INPUT_RELEASE,1),dpad=run(seed,INPUT_RIGHT,1),skill=run(seed,8,1);
  printf("seed %d: idle=%d hold=%d dpad=%d skill=%d anchors=%d state=%d wait=%d rope=%d\n",seed,idle_run.h[0].score,hold.h[0].score,dpad.h[0].score,skill.h[0].score,skill.h[0].anchor,skill.h[0].state,skill.h[0].wait,skill.h[0].rope);fflush(stdout);
  assert(idle_run.h[0].state==HS_DOWN && hold.h[0].state==HS_DOWN && dpad.h[0].state==HS_DOWN);
  assert(skill.h[0].state==HS_RUN && skill.h[0].score>500 && skill.h[0].launches>50);
  world repeat=run(seed,8,1);assert(!memcmp(&skill,&repeat,sizeof skill));
 }
 world four=run(4,8,4);for(int p=0;p<4;p++)assert(four.h[p].score>100);
 puts("visible strand crossings, forgiving timing, momentum, air steering and seeded four-player courses passed");return 0;}
