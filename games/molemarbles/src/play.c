/* Endless Pang-like cavern. Integer fixed step, one shared world for all four pads. */
#include "game.h"
#include <string.h>
#include <stdlib.h>
static int clamp(int x,int a,int b){return x<a?a:x>b?b:x;}
static uint32_t rnd(world*w){w->rng=w->rng*1664525u+1013904223u;return w->rng;}
int ball_radius(int s){static const int r[]={6,10,15,22};return r[clamp(s,0,3)];}
static int difficulty_score(const world*w){int score=0;for(int p=0;p<w->players;p++)if(w->h[p].score>score)score=w->h[p].score;return score;}
int marble_pace(const world*w){return 700+clamp(difficulty_score(w),0,1500)/3;}
int32_t marble_bounce(int s,int pace){static const int v[]={275251,314573,360448,357142};return -(int32_t)((int64_t)v[s]*pace/1000);}
int32_t marble_gravity(int pace){return (int32_t)((int64_t)10486*pace*pace/1500000);}
int32_t marble_speed(int s,int pace){return (int32_t)((int64_t)(173015-s*19661)*pace/1000);}
int ball_mass(const world*w){int mass=0;for(int i=0;i<MAX_BALLS;i++)if(w->balls[i].active)mass+=1<<w->balls[i].size;return mass;}
static int add(world*w,int s,int x,int y,int direction,int hue,int grace){
 for(int i=0;i<MAX_BALLS;i++)if(!w->balls[i].active){marble*b=&w->balls[i];memset(b,0,sizeof *b);b->active=1;b->size=s;b->x=b->px=x*Q16_ONE;b->y=b->py=y*Q16_ONE;b->vx=direction*marble_speed(s,w->pace);b->vy=marble_bounce(s,w->pace);b->hue=hue;b->grace=grace;return i;}
 return -1;
}
void world_init(world*w,int players,uint32_t seed){
 memset(w,0,sizeof *w);w->players=clamp(players,1,4);w->seed=seed;w->rng=seed?seed:1;w->spawn_wait=300;w->pace=700;
 for(int p=0;p<4;p++){hero*h=&w->h[p];h->state=p<w->players?HS_READY:HS_OFF;h->x=w->players==1?100:56+p*64;h->xx=h->x*Q16_ONE;h->y=GROUND_Y*Q16_ONE;h->facing=1;}
 /* Exactly this frozen arrangement appears behind the title. */
 add(w,0,246,126,-1,0,60);w->balls[0].vy=0;
}
int world_running(const world*w){for(int p=0;p<w->players;p++)if(w->h[p].state==HS_RUN||w->h[p].state==HS_READY)return 1;return 0;}
int world_all_down(const world*w){for(int p=0;p<w->players;p++)if(w->h[p].state!=HS_DOWN)return 0;return 1;}
static int circle_box(int x,int y,int r,int hx){int dx=x-clamp(x,hx-8,hx+8),dy=y-clamp(y,GROUND_Y-23,GROUND_Y-1);return dx*dx+dy*dy<r*r;}
static int swept_hero(const marble*b,const hero*h){int x0=b->px/Q16_ONE,y0=b->py/Q16_ONE,x1=b->x/Q16_ONE,y1=b->y/Q16_ONE;int n=abs(x1-x0)+abs(y1-y0)+abs(h->x-h->old_x)+1,r=ball_radius(b->size);for(int k=0;k<=n;k++)if(circle_box(x0+(x1-x0)*k/n,y0+(y1-y0)*k/n,r,h->old_x+(h->x-h->old_x)*k/n))return 1;return 0;}
static int rope_hit(const marble*b,const hero*h){int x0=b->px/Q16_ONE,y0=b->py/Q16_ONE,x1=b->x/Q16_ONE,y1=b->y/Q16_ONE,r=ball_radius(b->size),n=abs(x1-x0)+abs(y1-y0)+1;for(int k=0;k<=n;k++){int x=x0+(x1-x0)*k/n,y=y0+(y1-y0)*k/n,dx=x-h->shot_x,dy=y-clamp(y,h->shot_y,GROUND_Y-11);if(dx*dx+dy*dy<=r*r)return 1;}return 0;}
static void shatter(world*w,int idx,int player){
 marble old=w->balls[idx];w->balls[idx].active=0;int x=old.x/Q16_ONE,y=old.y/Q16_ONE;hero*h=&w->h[player];h->score+=old.size?10:15;w->events[player]|=EV_SCORE;w->total_hits++;h->shot=0;h->shot_wait=8;
 for(int n=0;n<6;n++)for(int i=0;i<MAX_FX;i++)if(!w->fx[i].t){w->fx[i]=(fragment){22,x,y,(n%3-1)*2,(n/3?-3:-1),old.hue};break;}
 if(old.size){int s=old.size-1,r=ball_radius(s);int yy=clamp(y,28+r,GROUND_Y-r);add(w,s,clamp(x-r/2,r,320-r),yy,-1,old.hue,42);add(w,s,clamp(x+r/2,r,320-r),yy,1,(old.hue+1)%3,42);}
}
static int next_interval(world*w){return 330-clamp(difficulty_score(w),0,900)*120/900+(int)(rnd(w)%121)-60;}
static int prepare_wave(world*w,int warning){
 int score=difficulty_score(w),stage=score<60?0:score<180?1:score<420?2:3;
 int budget=6+clamp(score,0,840)/60; /* Splits conserve mass; at most twenty tiny marbles. */
 int room=budget-ball_mass(w),count=score<45?1:score<150?1+(rnd(w)%3==0):1+(int)(rnd(w)%3);
 w->warning_count=0;
 for(int n=0;n<count && room>0;n++){
  int roll=(int)(rnd(w)%100),size=stage==0?0:stage==1?(roll<65?0:1):stage==2?(roll<30?0:roll<75?1:2):(roll<20?0:roll<55?1:roll<87?2:3);
  while((1<<size)>room)size--;
  int x=count==1?32+(int)(rnd(w)%257):count==2?30+n*155+(int)(rnd(w)%106):35+n*100+(int)(rnd(w)%51);
  w->incoming[w->warning_count++]=(arrival){size,x,(int)(rnd(w)%3)};room-=1<<size;
 }
 if(!w->warning_count){w->spawn_wait=next_interval(w);return 0;}
 w->warning=warning;
 w->warning_size=w->incoming[0].size;w->warning_x=w->incoming[0].x;w->warning_hue=w->incoming[0].hue;
 return 1;
}
static void spawn_step(world*w){
 if(w->spawn_wait>0)w->spawn_wait--;
 if(w->warning){
  if(--w->warning==0){
   for(int n=0;n<w->warning_count;n++){
    arrival*a=&w->incoming[n];int radius=ball_radius(a->size);
    int i=add(w,a->size,a->x,28+radius,a->x<160?1:-1,a->hue,0);
    if(i>=0){w->balls[i].vy=Q16_ONE*w->pace/1000;w->spawns++;}
   }
   w->warning_count=0;w->empty_frames=0;w->spawn_wait=next_interval(w);
  }
  return;
 }
 if(!ball_mass(w)){
  if(++w->empty_frames==1){prepare_wave(w,60);return;} /* Exactly one second to an empty-arena refill. */
 }else w->empty_frames=0;
 if(w->spawn_wait<=90)prepare_wave(w,90);
}
void world_step(world*w,const int input[4]){
 memset(w->events,0,sizeof w->events);
 if(!w->started){int start=0;for(int p=0;p<w->players;p++)start|=input[p];if(!start)return;w->started=1;for(int p=0;p<w->players;p++){w->h[p].state=HS_RUN;w->events[p]=EV_START;}}
 for(int p=0;p<w->players;p++){hero*h=&w->h[p];if(h->state==HS_HIT){if(++h->t>=PANEL_DELAY)h->state=HS_DOWN;}else if(h->state==HS_DOWN)h->t++;}
 if(!world_running(w))return;
 if(w->freeze){w->freeze--;return;}
 w->t++;w->scroll=w->t;
 int previous_pace=w->pace;w->pace=marble_pace(w);int32_t gravity=marble_gravity(w->pace);
 for(int i=0;i<MAX_BALLS;i++)if(w->balls[i].active){
  marble*b=&w->balls[i];b->vx=(b->vx<0?-1:1)*marble_speed(b->size,w->pace);
  if(previous_pace!=w->pace)b->vy=(int32_t)((int64_t)b->vy*w->pace/previous_pace);
 }
 for(int i=0;i<MAX_FX;i++)if(w->fx[i].t){fragment*f=&w->fx[i];f->t--;f->x+=f->vx;f->y+=f->vy;if(w->t%4==0)f->vy++;}
 for(int p=0;p<w->players;p++){
  hero*h=&w->h[p];if(h->state!=HS_RUN)continue;int dir=!!(input[p]&RS_BTN_RIGHT)-!!(input[p]&RS_BTN_LEFT);h->old_x=h->x;h->xx=clamp(h->xx+dir*176947,12*Q16_ONE,308*Q16_ONE);h->x=h->xx/Q16_ONE;if(dir)h->facing=dir;
  if(h->shot_wait)h->shot_wait--;
  if(h->shot){h->shot_y-=6;if(h->shot_y<24)h->shot_y=24;if(++h->shot>56){h->shot=0;h->shot_wait=8;}}
  else if((input[p]&RS_BTN_A)&&!h->shot_wait){h->shot=1;h->shot_x=h->x;h->shot_y=GROUND_Y-12;w->events[p]|=EV_JUMP;}
 }
 for(int i=0;i<MAX_BALLS;i++){
  marble*b=&w->balls[i];if(!b->active)continue;int r=ball_radius(b->size);b->px=b->x;b->py=b->y;
  if(b->grace)b->grace--;
  b->x+=b->vx;b->vy+=gravity;b->y+=b->vy;
  if(b->x<r*Q16_ONE){b->x=r*Q16_ONE+(r*Q16_ONE-b->x);b->vx=abs(b->vx);}else if(b->x>(320-r)*Q16_ONE){b->x=(320-r)*Q16_ONE-(b->x-(320-r)*Q16_ONE);b->vx=-abs(b->vx);}
  if(b->y>(GROUND_Y-r)*Q16_ONE){b->y=(GROUND_Y-r)*Q16_ONE;b->vy=marble_bounce(b->size,w->pace);}
  if(b->y<(24+r)*Q16_ONE){b->y=(24+r)*Q16_ONE;b->vy=abs(b->vy);}
  int shot=0;for(int p=0;p<w->players;p++)if(w->h[p].state==HS_RUN&&w->h[p].shot&&b->grace<30&&rope_hit(b,&w->h[p])){shatter(w,i,p);shot=1;break;}if(shot)continue;
  if(!b->grace)for(int p=0;p<w->players;p++){hero*h=&w->h[p];if(h->state==HS_RUN&&swept_hero(b,h)){h->state=HS_HIT;h->t=0;h->shot=0;w->events[p]|=EV_HIT;w->freeze=8;}}
 }
 spawn_step(w);
}
/* Ordinary d-pad lookahead: choose the safest reachable position, keep firing. */
int bot_decide(const world*w,int player){
 const hero*h=&w->h[player];if(h->state==HS_READY)return RS_BTN_A;if(h->state!=HS_RUN)return 0;
 int best=0,cost=2147483647;
 for(int dir=-1;dir<=1;dir++){
  int c=0,x=h->x;marble balls[MAX_BALLS];memcpy(balls,w->balls,sizeof balls);
  for(int t=1;t<=38;t++){
   x=clamp(h->x+dir*27*t/10,12,308);
   for(int i=0;i<MAX_BALLS;i++){marble*b=&balls[i];if(!b->active)continue;int r=ball_radius(b->size);b->x+=b->vx;b->vy+=marble_gravity(w->pace);b->y+=b->vy;if(b->x<r*Q16_ONE){b->x=r*Q16_ONE;b->vx=abs(b->vx);}if(b->x>(320-r)*Q16_ONE){b->x=(320-r)*Q16_ONE;b->vx=-abs(b->vx);}if(b->y>(GROUND_Y-r)*Q16_ONE){b->y=(GROUND_Y-r)*Q16_ONE;b->vy=marble_bounce(b->size,w->pace);}if(b->y<(24+r)*Q16_ONE){b->y=(24+r)*Q16_ONE;b->vy=abs(b->vy);}
    if(b->grace>t)continue;
    int dx=abs(b->x/Q16_ONE-x),yy=b->y/Q16_ONE;
    if(circle_box(b->x/Q16_ONE,yy,r+3,x))c+=(40-t)*200;
    else if(yy>GROUND_Y-40-r&&dx<r+24)c+=(r+24-dx)*(40-t);
   }
  }
  c+=dir?20:0;c+=abs(x-160)/12;if(c<cost){cost=c;best=dir;}
 }
 return RS_BTN_A|(best<0?RS_BTN_LEFT:best>0?RS_BTN_RIGHT:0);
}


