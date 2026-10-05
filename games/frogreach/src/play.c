/* Hold to extend a tongue, release onto a deterministic moving lily pad. */
#include "game.h"
#include "sine.h"
#include <string.h>
#include <stdlib.h>
static uint32_t mix(uint32_t n) {
 n^=n>>16;n*=0x7feb352du;n^=n>>15;n*=0x846ca68bu;return n^(n>>16);
}
void perch_pos(const world *w,int id,int t,int *x,int *width) {
 *x=80+id*160+(id?(int)(mix(w->seed+(uint32_t)id*733u)%49)-24:0);
 uint32_t r=mix(w->seed+(uint32_t)id*733u);
 if(id>=4 && id%3==1) *x+=ss_sine[(t*(1+(int)((r>>5)&1))+id*39)&255]*(8+(int)((r>>8)%9))/32768;
 *width=id==0?64:id==1?56:64-(int)((r>>12)%5)*8;
 if(id>=12)*width-=8;
 if(*width<24)*width=24;
}
int pad_life(int anchor) {int n=330-anchor*2;return n<180?180:n;}
int tongue_speed(int anchor) {return anchor<12?4:anchor<30?5:6;}
void world_init(world *w,int players,uint32_t seed) {
 memset(w,0,sizeof *w);w->seed=seed;w->players=players<1?1:players>4?4:players;
 for(int p=0;p<4;p++) {
  hero *h=&w->h[p];h->state=p<w->players?HS_READY:HS_OFF;
  h->wx=80LL*Q16_ONE;h->x=80;h->y=192*Q16_ONE;h->grounded=1;h->land_t=99;
 }
}
int world_running(const world *w) {
 for(int p=0;p<w->players;p++)if(w->h[p].state==HS_RUN)return 1;
 return 0;
}
int world_all_down(const world *w) {
 for(int p=0;p<w->players;p++)if(w->h[p].state!=HS_DOWN)return 0;
 return 1;
}
static void hit(world *w,int p) {
 hero *h=&w->h[p];h->state=HS_HIT;h->t=0;h->vy=0;h->combo=0;w->events[p]|=EV_HIT;
}
int bot_decide(const world *w,int p) {
 const hero *h=&w->h[p];
 if(h->state==HS_READY)return 1;
 if(h->state!=HS_RUN || h->mode==MODE_LEAP)return 0;
 if(h->mode==MODE_WAIT)return 1;
 int x,width;perch_pos(w,h->anchor+1,w->t+1,&x,&width);
 int length=x-(int)(h->wx>>16);
 return h->length<length-tongue_speed(h->anchor)/2;
}
void world_step(world *w,const int held[4]) {
 memset(w->events,0,sizeof w->events);
 if(!w->started) {
  int start=0;for(int p=0;p<w->players;p++)start|=held[p];if(!start)return;
  w->started=1;for(int p=0;p<w->players;p++){w->h[p].state=HS_RUN;w->h[p].held=held[p]!=0;w->h[p].start_guard=held[p]?2:0;w->events[p]=EV_START;}
  return; /* Ignore the brief starting tap; a continuing hold charges after two frames. */
 }
 w->t++;
 for(int p=0;p<w->players;p++) {
  hero *h=&w->h[p];h->t++;h->air_t++;h->land_t++;
  if(h->smooth_t>0)h->smooth_t--;
  int release=h->held && !held[p];h->held=held[p]!=0;
  if(h->state==HS_HIT) {
   h->vy+=Q16_ONE/3;h->y+=h->vy;
   if(h->t>35){h->state=HS_DOWN;h->t=0;}
   continue;
  }
  if(h->state!=HS_RUN)continue;
  if(h->start_guard>0){h->start_guard--;if(!held[p])h->start_guard=0;continue;}
  if(h->mode==MODE_LEAP) {
   h->leap_t++;int t=h->leap_t,duration=28;
   if(h->good){int target,width;perch_pos(w,h->anchor+1,w->t,&target,&width);h->goal_x=target+h->goal_offset;}
   int old=(int)(h->wx>>16);int32_t oy=h->y;
   h->wx=(int64_t)(h->start_x+(h->goal_x-h->start_x)*t/duration)*Q16_ONE;
   h->y=(192-4*42*t*(duration-t)/(duration*duration))*Q16_ONE;
   h->vx=((int)(h->wx>>16)-old)*Q16_ONE;h->vy=h->y-oy;
   if(t>=duration) {
    if(!h->good){hit(w,p);continue;}
    h->anchor++;h->pad_offset=h->goal_offset;h->launches++;h->score+=10;h->grounded=1;h->land_t=0;
    h->mode=MODE_WAIT;h->length=0;h->wait=0;w->events[p]|=EV_LAND|EV_SCORE;
    if(h->perfect){h->apples++;h->score+=5;h->combo++;h->smooth_t=65;w->events[p]|=EV_APPLE;}
    else h->combo=0;
   }
  } else {
   int pad_x,pad_width;perch_pos(w,h->anchor,w->t,&pad_x,&pad_width);
   int old=(int)(h->wx>>16);h->wx=(int64_t)(pad_x+h->pad_offset)*Q16_ONE;h->vx=((int)(h->wx>>16)-old)*Q16_ONE;
   h->wait++;
   if(h->wait>=pad_life(h->anchor)){hit(w,p);continue;}
   if(held[p]) {
    h->mode=MODE_CHARGE;h->length+=tongue_speed(h->anchor);if(h->length>280)h->length=280;
   }
   if(release && h->mode==MODE_CHARGE) {
    int x,width;perch_pos(w,h->anchor+1,w->t,&x,&width);
    int tip=(int)(h->wx>>16)+h->length;
    h->good=tip>=x-width/2+2 && tip<=x+width/2-2;
    h->perfect=h->good && abs(tip-x)<=4;
    h->start_x=(int)(h->wx>>16);h->goal_offset=h->good?tip-x:0;h->goal_x=tip;
    if(h->goal_x<h->start_x+40)h->goal_x=h->start_x+40;
    h->mode=MODE_LEAP;h->leap_t=0;h->grounded=0;h->air_t=0;w->events[p]|=EV_JUMP;
   }
  }
  h->x=(int)(h->wx>>16);
  if(h->x-80>w->scroll)w->scroll=h->x-80;
 }
}
