/* Fixed-step orbit movement and shrinking rings. No renderer or floating point. */
#include "game.h"
#include "sine.h"
#include <string.h>
#include <stdlib.h>
static uint32_t mix(uint32_t v) {v^=v>>16;v*=0x7feb352du;v^=v>>15;v*=0x846ca68bu;return v^(v>>16);}
int angle_diff(int a,int b) {return ((a-b+128)&255)-128;}
int ring_speed(int id){if(id<0)id=0;if(id>100)id=100;return 68813+id*819;}
int ring_center(const ring *r,int opening,int ticks){return (((r->angle+r->spin*ticks)&65535)/256+r->offset[opening])&255;}
int ring_opening(const ring *r,int angle,int safety){
 int count=r->openings?r->openings:1;
 for(int k=0;k<count;k++)if(abs(angle_diff(angle,ring_center(r,k,0)))<=(r->openings?r->width[k]:r->gap)/2-safety)return k;
 return -1;
}
static int launch_lead(uint32_t seed,int id){return 70+(int)(mix(seed+(uint32_t)id*2654435761u)%25);}
int orbit_radius(int players,int p){(void)players;(void)p;return ORBIT;}
static void position(hero *h,int radius) {
 h->x=160+radius*ss_sine[(h->angle+64)&255]/32767;
 h->y=(LANTERN_Y+radius*ss_sine[h->angle&255]/32767)*Q16_ONE;
}
static void spawn(world *w,int first) {
 int slot=-1;for(int i=0;i<MAX_RINGS;i++)if(!w->rings[i].active){slot=i;break;}
 if(slot<0)return;
 int id=w->ring_id++,level=id/15;if(level>5)level=5;
 uint32_t v=mix(w->seed+(uint32_t)id*2654435761u);
 int gap=id==0?56+(int)(v%64):(w->last_gap+((v&1)?1:-1)*(24+(int)(v%47)))&255;
 w->last_gap=gap;
 ring *r=&w->rings[slot];memset(r,0,sizeof *r);
 r->active=1;r->id=id;r->angle=gap*256;r->kind=id&1;
 r->openings=id==0?1:1+(int)((v>>8)%3);r->gap=48+(int)((v>>12)%21)-level*2;r->speed=ring_speed(id);
 for(int k=0;k<r->openings;k++){
  r->offset[k]=k?256*k/r->openings+(int)((v>>(k*5))%17)-8:0;
  r->width[k]=k?38+(int)((v>>(k*7))%19)-level:r->gap;
 }
 r->spin=level>=2?((v&2)?24:-24):0;
 (void)first;
 int outer=orbit_radius(w->players,w->players-1),inner=orbit_radius(w->players,0);
 r->radius=(outer+MOTH_RADIUS)*Q16_ONE+launch_lead(w->seed,id)*r->speed;r->previous=r->radius;
 /* After every player clears the shared orbit, the next contact leaves
    69 frames: enough for a half-turn (43 frames) plus reaction/rotation margin.
    Account for the following ring's varied launch radius and faster speed. */
 int clear=(r->radius-(inner-MOTH_RADIUS)*Q16_ONE+r->speed-1)/r->speed;
 int next_lead=launch_lead(w->seed,id+1);
 w->next_spawn=clear+69-next_lead;
}
void world_init(world *w,int players,uint32_t seed) {
 memset(w,0,sizeof *w);w->players=players<1?1:players>4?4:players;w->seed=seed;
 for(int p=0;p<w->players;p++){w->h[p].state=HS_READY;position(&w->h[p],orbit_radius(w->players,p));}
}
static void hit(world *w,int p) {
 hero *h=&w->h[p];h->state=HS_HIT;h->t=0;w->events[p]|=EV_HIT;
}
void world_step(world *w,const int input[4]) {
 memset(w->events,0,sizeof w->events);
 if(!w->started) {
  int start=0;for(int p=0;p<w->players;p++)start|=input[p]!=0;
  if(!start)return;
  w->started=1;spawn(w,1);
  for(int p=0;p<w->players;p++)w->h[p].state=HS_RUN;
  w->events[0]|=EV_START;
 }
 w->t++;w->scroll=w->t;
 if(--w->next_spawn<=0 && world_running(w))spawn(w,0);
 for(int i=0;i<MAX_RINGS;i++)if(w->rings[i].active) {
  ring *r=&w->rings[i];r->previous=r->radius;r->radius-=r->speed;r->angle=(r->angle+r->spin)&65535;
 }
 for(int p=0;p<w->players;p++) {
  hero *h=&w->h[p];h->t++;
  if(h->smooth_t>0)h->smooth_t--;
  if(h->state==HS_RUN) {
   int direction=input[p]==-1?-1:input[p]==1?1:0;
   int orbit=orbit_radius(w->players,p);
   h->angle=(h->angle+direction*3)&255;h->vx=direction;position(h,orbit);h->alive_t++;
   for(int i=0;i<MAX_RINGS;i++) {
    ring *r=&w->rings[i];if(!r->active || (r->passed&(1<<p)))continue;
    if(r->previous>=(orbit-MOTH_RADIUS)*Q16_ONE && r->radius<=(orbit+MOTH_RADIUS)*Q16_ONE) {
     int opening=ring_opening(r,h->angle,6);
     if(opening<0){hit(w,p);break;}
     int delta=abs(angle_diff(h->angle,ring_center(r,opening,0)));
     if(r->radius<(orbit-MOTH_RADIUS)*Q16_ONE) {
      r->passed|=1<<p;h->anchor++;h->launches++;h->smooth_t=16;w->events[p]|=EV_SCORE;
      if(delta<=4){h->apples++;w->events[p]|=EV_APPLE;}
     }
    }
   }
   h->score=h->anchor*10+h->apples*3+h->alive_t/60;
  } else if(h->state==HS_HIT && h->t>=20)h->state=HS_DOWN;
  else if(h->state==HS_DOWN && h->t>=40)h->state=HS_OFF;
 }
 for(int i=0;i<MAX_RINGS;i++)if(w->rings[i].radius<12*Q16_ONE)w->rings[i].active=0;
}
int world_running(const world *w) {for(int p=0;p<w->players;p++)if(w->h[p].state==HS_RUN || w->h[p].state==HS_READY)return 1;return 0;}
int world_all_down(const world *w) {for(int p=0;p<w->players;p++)if(w->h[p].state!=HS_OFF)return 0;return 1;}
static int player_target(const world *w,int p){
 int orbit=orbit_radius(w->players,p),found=-1;
 for(int i=0;i<MAX_RINGS;i++)if(w->rings[i].active && !(w->rings[i].passed&(1<<p)) && w->rings[i].radius>=(orbit-MOTH_RADIUS)*Q16_ONE)
  if(found<0 || w->rings[i].radius<w->rings[found].radius)found=i;
 return found;
}
int ring_target(const world *w){
 int found=-1;
 for(int p=0;p<w->players;p++)if(w->h[p].state==HS_READY || w->h[p].state==HS_RUN){
  int i=player_target(w,p);if(i>=0 && (found<0 || w->rings[i].radius<w->rings[found].radius))found=i;
 }
 return found;
}
int bot_decide(const world *w,int p) {
 if(!w->started)return 2;
 if(w->h[p].state!=HS_RUN)return 0;
 int i=player_target(w,p);if(i<0)return 0;
 const ring *r=&w->rings[i];
 int remaining=(r->radius-(orbit_radius(w->players,p)+MOTH_RADIUS)*Q16_ONE)/r->speed;if(remaining<0)remaining=0;
 int target=ring_center(r,0,remaining+1),closest=abs(angle_diff(target,w->h[p].angle));
 for(int k=1;k<r->openings;k++){
  int candidate=ring_center(r,k,remaining+1),delta=abs(angle_diff(candidate,w->h[p].angle));
  if(delta<closest){target=candidate;closest=delta;}
 }
 int delta=angle_diff(target,w->h[p].angle);
 return delta>2?1:delta<-2?-1:0;
}
