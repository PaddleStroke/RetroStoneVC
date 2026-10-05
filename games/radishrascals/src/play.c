#include "game.h"
#include <string.h>
static uint32_t mix(uint32_t v){v^=v>>16;v*=0x7feb352du;v^=v>>15;v*=0x846ca68bu;return v^(v>>16);}
void bed_position(int id,int *x,int *y){static const int xs[4]={160,252,160,68},ys[4]={90,145,202,145};*x=xs[id&3];*y=ys[id&3];}
void world_init(world *w,int n,uint32_t seed){memset(w,0,sizeof *w);w->players=n<1?1:n>4?4:n;w->seed=seed;for(int p=0;p<w->players;p++){hero *h=&w->h[p];h->state=HS_READY;h->x=160;h->y=146*Q16_ONE;h->hearts=1;h->spawn_t=25;h->aim=-1;}}
static void damage(world *w,int p){hero *h=&w->h[p];if(h->state!=HS_RUN)return;h->hearts=0;h->combo=0;h->flash=12;w->events[p]|=EV_HIT;h->state=HS_HIT;h->t=0;}
static void spawn(world *w,int p){hero *h=&w->h[p];uint32_t v=mix(w->seed+(uint32_t)h->wave*2654435761u);int slot=h->wave==0?0:(v&3),level=h->wave/12;if(level>4)level=4;
 /* Start at the former level-eight pace (roughly score1300), then ramp modestly. */
 for(int k=0;k<4;k++){int b=(slot+k)&3;if(h->beds[b].state==BED_EMPTY){bed *e=&h->beds[b];e->state=BED_WARN;e->t=0;e->friend=h->wave%5==3;e->duration=40-level*2;h->wave++;h->spawn_t=54-level*4;return;}}
 h->spawn_t=10;
}
void world_step(world *w,const int input[4]){
 memset(w->events,0,sizeof w->events);if(!w->started){int start=0;for(int p=0;p<w->players;p++)start|=input[p]!=0;if(!start)return;w->started=1;for(int p=0;p<w->players;p++)w->h[p].state=HS_RUN;w->events[0]|=EV_START;}
 w->t++;w->scroll=w->t;
 for(int p=0;p<w->players;p++){hero *h=&w->h[p];h->t++;if(h->smooth_t>0)h->smooth_t--;if(h->flash>0)h->flash--;
  if(h->state==HS_RUN){
   if(--h->spawn_t<=0)spawn(w,p);
   for(int i=0;i<4;i++){bed *b=&h->beds[i];b->t++;
    if(b->state==BED_WARN && b->t>=18){b->state=BED_ACTIVE;b->t=0;}
    else if(b->state==BED_ACTIVE && b->t>=b->duration){b->t=0;if(b->friend){b->state=BED_SAFE;h->apples++;h->score+=5;w->events[p]|=EV_APPLE;}else{b->state=BED_MISS;damage(w,p);break;}}
    else if(b->state>=BED_SHOO && b->t>=20){b->state=BED_EMPTY;b->t=0;}
   }
   if(h->state!=HS_RUN)continue;
   int edge=(input[p]&15)&~h->held;h->held=input[p]&15;
   for(int i=0;i<4;i++)if(edge&(1<<i)){
    bed *b=&h->beds[i];h->aim=i;h->launches++;h->smooth_t=12;w->events[p]|=EV_JUMP;
    if(b->state==BED_ACTIVE && !b->friend){b->state=BED_SHOO;b->t=0;if(h->combo<5)h->combo++;h->anchor++;h->score+=10+h->combo*2;w->events[p]|=EV_SCORE;}
    else{if(b->state==BED_ACTIVE || b->state==BED_WARN){b->state=BED_MISS;b->t=0;}damage(w,p);break;}
   }
  }else if(h->state==HS_HIT && h->t>=20)h->state=HS_DOWN;
  else if(h->state==HS_DOWN && h->t>=40)h->state=HS_OFF;
 }
}
int world_running(const world *w){for(int p=0;p<w->players;p++)if(w->h[p].state==HS_RUN || w->h[p].state==HS_READY)return 1;return 0;}
int world_all_down(const world *w){for(int p=0;p<w->players;p++)if(w->h[p].state!=HS_OFF)return 0;return 1;}
int bot_decide(const world *w,int p){if(!w->started)return 16;const hero *h=&w->h[p];if(h->state!=HS_RUN || h->held)return 0;
 for(int i=0;i<4;i++)if(h->beds[i].state==BED_ACTIVE && !h->beds[i].friend && h->beds[i].t>=3)return 1<<i;
 return 0;}
