#include "game.h"
#include <string.h>
#include <stdlib.h>
static uint32_t hash(uint32_t x){x^=x>>16;x*=0x7feb352d;x^=x>>15;x*=0x846ca68b;x^=x>>16;return x;}
int curve_sine(int a){a&=1023;int sign=a>512?-1:1;if(a>512)a-=512;int64_t k=(int64_t)a*(512-a);return (int)(sign*16*k*32767/(5*512*512-4*k));}
int vine_y(int x){return 127+(curve_sine((x*3)&1023)*15>>15)+(curve_sine((x*7+180)&1023)*4>>15);}
int32_t course_speed(int d){if(d>24000)d=24000;return 157286+(int64_t)d*91750/24000;}
void obstacle_at(const world*w,int id,obstacle*o){
 uint32_t r=hash(w->seed+(uint32_t)id*24593u);o->id=id;o->x=480+id*OBSTACLE_SPACING+(r%61)-30;
 o->side=(r>>8)&1;o->type=(r>>12)%3;o->anchor=o->x;o->dir=1;
 /* Slow, continuous patrol. The visible body and collision use this same position. */
 if(o->type==1){int t=(w->t+(int)(r%240))%240;int patrol=t<120?t:240-t;o->x+=patrol*36/120-18;o->dir=t<120?1:-1;}
}
void world_init(world*w,int n,uint32_t seed){
 memset(w,0,sizeof *w);w->players=n;w->seed=seed;
 for(int p=0;p<4;p++){hero*h=&w->h[p];h->state=p<n?HS_READY:HS_OFF;h->x=116-p*27;h->berry=-1;h->y=(vine_y(h->x)-21)*Q16_ONE;}
}
int world_running(const world*w){for(int p=0;p<w->players;p++)if(w->h[p].state==HS_RUN || w->h[p].state==HS_READY)return 1;return 0;}
int world_all_down(const world*w){for(int p=0;p<w->players;p++)if(w->h[p].state!=HS_DOWN)return 0;return 1;}
void world_step(world*w,const int press[4]){
 memset(w->events,0,sizeof w->events);
 if(!w->started){int go=0;for(int p=0;p<w->players;p++)go|=press[p];if(!go)return;w->started=1;for(int p=0;p<w->players;p++){w->h[p].state=HS_RUN;w->events[p]=EV_START;}return;}
 w->t++;if(world_running(w)){w->travel+=course_speed(w->scroll);w->scroll=(int)(w->travel>>16);}
 for(int p=0;p<w->players;p++){
  hero*h=&w->h[p];h->t++;
  if(h->state==HS_HIT){if(h->t>HIT_FREEZE){h->vy+=Q16_ONE/3;h->y+=h->vy;if(h->y/Q16_ONE>270 || h->y/Q16_ONE< -50){h->state=HS_DOWN;h->t=0;}}continue;}
  if(h->state!=HS_RUN)continue;
  if(press[p]==CMD_UP)h->target=0;
  else if(press[p]==CMD_DOWN)h->target=1;
  else if(press[p]==CMD_TOGGLE && !h->flip)h->target=1-h->side;
  int target=h->target;
  if(target!=h->side && !h->flip){h->from=h->side;h->side=target;h->flip=22;w->events[p]|=EV_JUMP;}
  if(h->flip)h->flip--;
  int progress=h->flip?22-h->flip:22;
  int offset=h->flip?(-21+42*progress/22)*(h->from?-1:1):(h->side?21:-21);
  int wx=w->scroll+h->x;h->y=(vine_y(wx)+offset)*Q16_ONE;
  int base=(wx-480)/OBSTACLE_SPACING;if(base<0)base=0;
  for(int i=base>0?base-1:0;i<=base+2;i++){
   obstacle o;obstacle_at(w,i,&o);int dx=o.x-wx;
   if(abs(dx)<21 && ((o.side==0 && offset<-6)||(o.side==1 && offset>6))){
    h->state=HS_HIT;h->t=0;h->vy=-(Q16_ONE*2);w->events[p]=EV_HIT;break;
   }
   int bx=o.anchor-80;
   if(h->berry<i && abs(bx-wx)<15 && ((o.side==0 && offset<-12)||(o.side==1 && offset>12))){
    h->berry=i;h->score+=5;w->events[p]|=EV_SCORE;
   }else if(h->berry<i && wx>bx+15){
    /* Each player must collect it before its catch window passes. */
    h->state=HS_HIT;h->t=0;h->vy=-(Q16_ONE*2);w->events[p]=EV_HIT;break;
   }
  }
  if(h->state==HS_RUN){int distance=w->scroll/24;if(distance>h->next_block){h->score+=distance-h->next_block;h->next_block=distance;}}
 }
}

