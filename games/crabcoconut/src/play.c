/* A shared shell board with one independently controlled crab and coconut per player. */
#include "game.h"
#include <stdlib.h>
#include <string.h>
static uint32_t mix(uint32_t v){v^=v>>16;v*=0x7feb352du;v^=v>>15;v*=0x846ca68bu;return v^(v>>16);}
static uint32_t root(uint64_t n){
 uint64_t result=0,bit=1ULL<<62;while(bit>n)bit>>=2;
 while(bit){if(n>=result+bit){n-=result+bit;result=(result>>1)+bit;}else result>>=1;bit>>=2;}return (uint32_t)result;
}
int ball_speed(int t){if(t>7200)t=7200;if(t<0)t=0;return Q16_ONE*18/10+(int)((int64_t)t*Q16_ONE*28/10/7200);}
int crab_y(int players,int p){return CRAB_Y-(players-1-p)*3;}
int paddle_y(int players,int p){return crab_y(players,p)-(CRAB_Y-PADDLE_Y);}
static int rain_shell(world *w,int initial){
 int slot=-1;for(int i=0;i<SHELLS;i++)if(!w->shells[i].hp){slot=i;break;}if(slot<0)return 0;
 uint32_t r=mix(w->seed+(uint32_t)w->wave*2654435761u);int x=0,y=0,placed=0;
 for(int attempt=0;attempt<40;attempt++){
  uint32_t candidate=mix(r+(uint32_t)attempt*977u);x=18+(int)(candidate%285);y=initial?40+(int)((candidate>>9)%87):-10-(int)((candidate>>9)%9);
  int clear=1;for(int i=0;i<SHELLS;i++)if(w->shells[i].hp && abs(w->shells[i].x-x)<29 && abs(w->shells[i].y/Q16_ONE-y)<19){clear=0;break;}
  if(clear){placed=1;break;}
 }
 w->wave++;if(!placed)return 0;
 shell *s=&w->shells[slot];s->x=x;s->y=y*Q16_ONE;s->hp=w->t>3600 && (r>>5)%8==0?2:1;
 s->kind=(int)((r>>12)&1);s->fall=Q16_ONE*(35+(int)((r>>16)%21))/1000;return 1;
}
static int rain_interval(const world *w,int sparse){
 uint32_t r=mix(w->seed+(uint32_t)w->wave*733u);int level=w->t/900;if(level>5)level=5;
 return sparse?18+(int)(r%21):85+(int)(r%61)-level*6;
}
static void aim(hero *h,int speed,int offset){
 if(offset>20)offset=20;
 if(offset<-20)offset=-20;
 h->bvx=(int32_t)((int64_t)speed*offset/28);
 int minimum=speed/5;if(abs(h->bvx)<minimum)h->bvx=h->bvx<0?-minimum:minimum;
 h->bvy=-(int32_t)root((uint64_t)((int64_t)speed*speed-(int64_t)h->bvx*h->bvx));
}
void world_init(world *w,int n,uint32_t seed){
 memset(w,0,sizeof *w);w->players=n<1?1:n>4?4:n;w->seed=seed;
 for(int i=0;i<18;i++)rain_shell(w,1);
 w->spawn_t=rain_interval(w,0);
 for(int p=0;p<w->players;p++){
  hero *h=&w->h[p];h->state=HS_READY;h->x=w->players==1?160:48+p*224/(w->players-1);h->xx=h->x*Q16_ONE;
  h->y=crab_y(w->players,p)*Q16_ONE;h->bx=h->xx;h->by=(paddle_y(w->players,p)-8)*Q16_ONE;aim(h,ball_speed(0),(p&1)?-11:10+p*2);
 }
}
static void hit(world *w,int p,int reason){hero *h=&w->h[p];h->state=HS_HIT;h->t=0;h->reason=reason;w->events[p]|=EV_HIT;}
static void ball_step(world *w,int p){
 hero *h=&w->h[p];int paddle=paddle_y(w->players,p),speed=ball_speed(w->t);uint32_t old_speed=root((uint64_t)((int64_t)h->bvx*h->bvx+(int64_t)h->bvy*h->bvy));
 if(old_speed){h->bvx=(int32_t)((int64_t)h->bvx*speed/old_speed);h->bvy=(int32_t)((int64_t)h->bvy*speed/old_speed);}
 for(int step=0;step<4;step++){
  int32_t oldx=h->bx,oldy=h->by;h->bx+=h->bvx/4;h->by+=h->bvy/4;
  if(h->bx<BALL_LEFT*Q16_ONE){h->bx=BALL_LEFT*Q16_ONE;h->bvx=abs(h->bvx);}
  if(h->bx>BALL_RIGHT*Q16_ONE){h->bx=BALL_RIGHT*Q16_ONE;h->bvx=-abs(h->bvx);}
  if(h->by<BALL_TOP*Q16_ONE){h->by=BALL_TOP*Q16_ONE;h->bvy=abs(h->bvy);}
  for(int i=0;i<SHELLS;i++){
   shell *s=&w->shells[i];if(!s->hp)continue;
   int32_t left=(s->x-13)*Q16_ONE,right=(s->x+13)*Q16_ONE,top=s->y-6*Q16_ONE,bottom=s->y+6*Q16_ONE,radius=5*Q16_ONE;
   if(h->bx+radius<left || h->bx-radius>right || h->by+radius<top || h->by-radius>bottom)continue;
   if(oldy+radius<=top){h->by=top-radius-Q16_ONE/4;h->bvy=-abs(h->bvy);}
   else if(oldy-radius>=bottom){h->by=bottom+radius+Q16_ONE/4;h->bvy=abs(h->bvy);}
   else if(oldx<s->x*Q16_ONE){h->bx=left-radius-Q16_ONE/4;h->bvx=-abs(h->bvx);}
   else{h->bx=right+radius+Q16_ONE/4;h->bvx=abs(h->bvx);}
   s->hp--;h->smooth_t=10;
   if(!s->hp){h->apples++;if(h->combo<5)h->combo++;h->score+=10+h->combo*2;w->events[p]|=EV_SCORE;}
   else w->events[p]|=EV_LAND;
   break;
  }
  if(h->bvy>0 && oldy<=paddle*Q16_ONE && h->by>=paddle*Q16_ONE && abs(h->bx-h->xx)<=24*Q16_ONE){
   int offset=(h->bx-h->xx)/Q16_ONE;h->by=paddle*Q16_ONE;aim(h,speed,offset);h->launches++;h->combo=0;w->events[p]|=EV_JUMP;
  }
  if(h->by>BALL_LOST*Q16_ONE){hit(w,p,1);return;}
 }
}
void world_fallout(world *w){
 /* After a round ends, let the remaining rain drop naturally past the sand.
    A 16-pixel shell is retired only once its top is below the screen. */
 for(int i=0;i<SHELLS;i++)if(w->shells[i].hp){
  shell *s=&w->shells[i];s->fall+=Q16_ONE/4;s->y+=s->fall;
  if(s->y>=248*Q16_ONE)s->hp=0;
 }
}
void world_step(world *w,const int input[4]){
 memset(w->events,0,sizeof w->events);
 if(!w->started){
  int start=0;for(int p=0;p<w->players;p++)start|=input[p]!=0;if(!start)return;
  w->started=1;for(int p=0;p<w->players;p++)w->h[p].state=HS_RUN;w->events[0]|=EV_START;return;
 }
 if(world_running(w)){
  w->t++;w->scroll=w->t;
  if(--w->spawn_t<=0){
   rain_shell(w,0);int count=0;for(int i=0;i<SHELLS;i++)count+=w->shells[i].hp>0;
   w->spawn_t=rain_interval(w,count<10);
  }
  int low=0;
  for(int i=0;i<SHELLS;i++)if(w->shells[i].hp){w->shells[i].y+=w->shells[i].fall;if(w->shells[i].y>=SHELL_LIMIT*Q16_ONE)low=1;}
  if(low)for(int p=0;p<w->players;p++)if(w->h[p].state==HS_RUN)hit(w,p,2);
 }else world_fallout(w);
 for(int p=0;p<w->players;p++){
  hero *h=&w->h[p];h->t++;if(h->smooth_t>0)h->smooth_t--;
  if(h->state==HS_RUN){
   int dir=input[p]==-1?-1:input[p]==1?1:0;
   if(dir){h->vx+=dir*(Q16_ONE*9/10);if(h->vx>Q16_ONE*48/10)h->vx=Q16_ONE*48/10;if(h->vx<-Q16_ONE*48/10)h->vx=-Q16_ONE*48/10;}
   else h->vx=h->vx*3/4;
   h->xx+=h->vx;if(h->xx<24*Q16_ONE){h->xx=24*Q16_ONE;h->vx=0;}if(h->xx>296*Q16_ONE){h->xx=296*Q16_ONE;h->vx=0;}h->x=h->xx/Q16_ONE;
   ball_step(w,p);
  }else if(h->state==HS_HIT && h->t>=20)h->state=HS_DOWN;
  else if(h->state==HS_DOWN && h->t>=40)h->state=HS_OFF;
 }
 int remaining=0;for(int i=0;i<SHELLS;i++)remaining+=w->shells[i].hp>0;
 if(world_running(w)){
  if(!remaining){rain_shell(w,0);w->spawn_t=rain_interval(w,1);}
  else if(remaining<10 && w->spawn_t>38)w->spawn_t=rain_interval(w,1);
 }
}
int world_running(const world *w){for(int p=0;p<w->players;p++)if(w->h[p].state==HS_RUN || w->h[p].state==HS_READY)return 1;return 0;}
int world_all_down(const world *w){for(int p=0;p<w->players;p++)if(w->h[p].state!=HS_OFF)return 0;return 1;}
int bot_decide(const world *w,int p){
 if(!w->started)return 2;
 const hero *h=&w->h[p];if(h->state!=HS_RUN)return 0;
 int x=h->bx/Q16_ONE,target=x,paddle=paddle_y(w->players,p);
 if(h->bvy>0){
  int32_t frames=(paddle*Q16_ONE-h->by)/h->bvy;if(frames<0)frames=0;target=x+(int)((int64_t)h->bvx*frames/Q16_ONE);
  while(target<BALL_LEFT || target>BALL_RIGHT){if(target<BALL_LEFT)target=2*BALL_LEFT-target;else target=2*BALL_RIGHT-target;}
  int chosen=-1,best=100000;
  for(int i=0;i<SHELLS;i++)if(w->shells[i].hp){const shell *s=&w->shells[i];int penalty=abs(s->x-target)+(SHELL_LIMIT-s->y/Q16_ONE);if(penalty<best){best=penalty;chosen=i;}}
  if(chosen>=0){const shell *s=&w->shells[chosen];int height=paddle-s->y/Q16_ONE;if(height<16)height=16;
   int aim=(s->x-target)*28/height;if(aim>18)aim=18;if(aim<-18)aim=-18;target-=aim;
  }
 }
 if(target<24)target=24;
 if(target>296)target=296;
 int forecast=h->x+h->vx*3/Q16_ONE;if(forecast<target-2)return 1;if(forecast>target+2)return -1;return 0;
}
