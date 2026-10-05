/* Momentum-preserving releases: the visible free silk is the catch surface. */
#include "game.h"
#include "sine.h"
#include <string.h>
#include <stdlib.h>
#define GRAVITY (Q16_ONE*13/100)
#define AIR_PUSH (Q16_ONE*18/100)
#define MAX_AIR (Q16_ONE*65/10)
static uint32_t mix(uint32_t n){n^=n>>16;n*=0x7feb352du;n^=n>>15;n*=0x846ca68bu;return n^(n>>16);}
static int64_t root(uint64_t n){uint64_t result=0,bit=(uint64_t)1<<62;while(bit>n)bit>>=2;while(bit){if(n>=result+bit){n-=result+bit;result=(result>>1)+bit;}else result>>=1;bit>>=2;}return (int64_t)result;}
void anchor_pos(const world *w,int id,int *x,int *y){
 uint32_t r=mix(w->seed+(uint32_t)id*317u);int phase=(id*37+(int)(w->seed%256))&255;
 *x=160+ss_sine[phase]*65/32768+(int)(r%25)-12;
 *y=150-id*50+(id?(int)((r>>8)%15)-7:0);
}
void dew_pos(const world *w,int id,int *x,int *y){int a,b,c,d;anchor_pos(w,id,&a,&b);anchor_pos(w,id+1,&c,&d);*x=(a+c)/2;*y=d+48;}
void bee_pos(const world *w,int id,int t,int *x,int *y){int a,b,c,d;anchor_pos(w,id,&a,&b);anchor_pos(w,id+1,&c,&d);int side=(a+c)/2<160?264:56;*x=side+ss_sine[(t+id*31)&255]*18/32768;*y=b+12+ss_sine[(t+id*43)&255]*9/32768;}
int trap_closed(int id,int t){(void)id;(void)t;return 0;}
int flower_life(int anchor){int n=420-anchor*2;return n<300?300:n;}
int flower_kind(const world *w,int anchor){return mix(w->seed+(uint32_t)anchor*1297u)%FLOWER_TYPES;}
int flower_color(const world *w,int anchor){return mix(w->seed^((uint32_t)anchor*733u+0xa17u))%FLOWER_COLORS;}
static int hazard(const world *w,const hero *h,int x,int y,int t){
 if(h->anchor<12 || h->anchor%5!=3)return 0;
 int bx,by;bee_pos(w,h->anchor,t,&bx,&by);return abs(x-bx)<13 && abs(y-by)<11;
}
void silk_segment(const hero *h,int *x,int *y,int *tip_x,int *tip_y){
 *x=(int)(h->wx/Q16_ONE);*y=h->y/Q16_ONE;*tip_x=(int)(h->silk_x/Q16_ONE);*tip_y=h->silk_y/Q16_ONE;
}
static int near_segment(int px,int py,int ax,int ay,int bx,int by){
 int64_t dx=bx-ax,dy=by-ay,x=px-ax,y=py-ay,len=dx*dx+dy*dy,dot=x*dx+y*dy,r=FLOWER_CATCH_RADIUS;
 if(dot<=0 || !len)return x*x+y*y<=r*r;
 if(dot>=len){x=px-bx;y=py-by;return x*x+y*y<=r*r;}
 int64_t cross=x*dy-y*dx;return cross*cross<=r*r*len;
}
static int64_t cross(int ax,int ay,int bx,int by,int px,int py){return (int64_t)(bx-ax)*(py-ay)-(int64_t)(by-ay)*(px-ax);}
static int in_triangle(int px,int py,int ax,int ay,int bx,int by,int cx,int cy){
 if(!cross(ax,ay,bx,by,cx,cy))return 0;
 int64_t a=cross(ax,ay,bx,by,px,py),b=cross(bx,by,cx,cy,px,py),c=cross(cx,cy,ax,ay,px,py);
 return (a>=0 && b>=0 && c>=0)||(a<=0 && b<=0 && c<=0);
}
int silk_catch(const world *w,const hero *h,int old_x,int old_y,int old_tip_x,int old_tip_y){
 int x,y,tx,ty;silk_segment(h,&x,&y,&tx,&ty);
 for(int id=h->anchor+1;id<=h->anchor+6;id++){
  int ax,ay;anchor_pos(w,id,&ax,&ay);
  /* Sweep the entire rendered strand, including both endpoints, so a quick crossing cannot miss a flower. */
  if(near_segment(ax,ay,x,y,tx,ty) || near_segment(ax,ay,old_x,old_y,old_tip_x,old_tip_y) ||
     near_segment(ax,ay,old_x,old_y,x,y) || near_segment(ax,ay,old_tip_x,old_tip_y,tx,ty) ||
     in_triangle(ax,ay,old_x,old_y,old_tip_x,old_tip_y,tx,ty) || in_triangle(ax,ay,old_x,old_y,tx,ty,x,y))return id;
 }
 return -1;
}
static int direction(int input){return (input&INPUT_LEFT)?((input&INPUT_RIGHT)?0:-1):(input&INPUT_RIGHT)?1:0;}
static void fly(hero *h,int input){int dir=direction(input);h->vx+=dir*AIR_PUSH;if(h->vx>MAX_AIR)h->vx=MAX_AIR;if(h->vx<-MAX_AIR)h->vx=-MAX_AIR;h->wx+=h->vx;h->vy+=GRAVITY;h->y+=h->vy;h->air_t++;
 /* The loose end lifts above the spider and trails its horizontal movement. It never disappears in flight. */
 h->silk_x+=(h->wx-(int64_t)h->vx*4-h->silk_x)/4;
 h->silk_y+=(h->y-SILK_LENGTH*Q16_ONE-h->silk_y)/4;
 int64_t dx=h->silk_x-h->wx,dy=(int64_t)h->silk_y-h->y,length=(int64_t)SILK_LENGTH*Q16_ONE,distance=root((uint64_t)(dx*dx+dy*dy));
 if(distance>length){h->silk_x=h->wx+dx*length/distance;h->silk_y=(int32_t)(h->y+dy*length/distance);}
}
static void hit(world *w,int p){hero *h=&w->h[p];h->state=HS_HIT;h->t=0;h->combo=0;w->events[p]|=EV_HIT;}
static void rope_step(const world *w,hero *h,int drive){
 int ax,ay;anchor_pos(w,h->anchor,&ax,&ay);int64_t ox=h->wx;int32_t oy=h->y;
 int64_t dx=h->wx-(int64_t)ax*Q16_ONE,dy=(int64_t)h->y-(int64_t)ay*Q16_ONE;
 int64_t distance=root((uint64_t)(dx*dx+dy*dy));if(distance<Q16_ONE)distance=Q16_ONE;
 if(h->rope<76)h->rope++;
 else if(h->rope>76)h->rope--;
 /* A light automatic pump keeps a newly caught web swinging; release timing and air steering choose the path. */
 if(drive){int64_t tangent=((int64_t)h->vx*dy-(int64_t)h->vy*dx)/distance;
  if(abs((int)tangent)>Q16_ONE/4)h->swing_dir=tangent>0?1:-1;
  if(abs((int)tangent)<Q16_ONE*58/10 && (abs((int)tangent)>Q16_ONE*7/10 || h->wait<30)){int push=Q16_ONE*10/100*h->swing_dir;h->vx+=(int32_t)(dy*push/distance);h->vy-=(int32_t)(dx*push/distance);}
 }
 h->wx+=h->vx;h->vy+=GRAVITY;h->y+=h->vy;
 dx=h->wx-(int64_t)ax*Q16_ONE;dy=(int64_t)h->y-(int64_t)ay*Q16_ONE;distance=root((uint64_t)(dx*dx+dy*dy));
 int64_t length=(int64_t)h->rope*Q16_ONE;
 if(distance>length){h->wx=(int64_t)ax*Q16_ONE+dx*length/distance;h->y=(int32_t)((int64_t)ay*Q16_ONE+dy*length/distance);}
 h->vx=(int32_t)(h->wx-ox);h->vy=h->y-oy;
}
static void attach(const world *w,hero *h){int ax,ay,nx,ny;anchor_pos(w,h->anchor,&ax,&ay);anchor_pos(w,h->anchor+1,&nx,&ny);
 int64_t dx=h->wx-(int64_t)ax*Q16_ONE,dy=(int64_t)h->y-(int64_t)ay*Q16_ONE;
 h->rope=(int)(root((uint64_t)(dx*dx+dy*dy))/Q16_ONE)+1;
 if(h->rope<16)h->rope=16;
 h->mode=MODE_CATCH;h->grounded=1;h->wait=0;h->land_t=0;h->web_t=0;h->swing_dir=nx>=ax?1:-1;
 /* The strand has already touched this blossom. Pin it immediately, preserving the body and momentum. */
}
void world_init(world *w,int players,uint32_t seed){memset(w,0,sizeof *w);w->seed=seed;w->players=players<1?1:players>4?4:players;
 for(int p=0;p<4;p++){hero *h=&w->h[p];h->state=p<w->players?HS_READY:HS_OFF;h->rope=76;h->grounded=1;h->land_t=99;h->apple_id=-1;
  int x,y,nx,ny;anchor_pos(w,0,&x,&y);anchor_pos(w,1,&nx,&ny);h->swing_dir=nx>x?1:-1;h->wx=(int64_t)x*Q16_ONE;h->y=(y+76)*Q16_ONE;h->vx=h->swing_dir*Q16_ONE*58/10;h->x=x;
 }}
int world_running(const world *w){for(int p=0;p<w->players;p++)if(w->h[p].state==HS_RUN)return 1;return 0;}
int world_all_down(const world *w){for(int p=0;p<w->players;p++)if(w->h[p].state!=HS_DOWN)return 0;return 1;}
static int air_policy(const world *w,const hero *h){int ax,ay;anchor_pos(w,h->anchor+1,&ax,&ay);int forecast=(int)(h->wx/Q16_ONE)+h->vx*6/Q16_ONE;if(forecast<ax-3)return INPUT_RIGHT;if(forecast>ax+3)return INPUT_LEFT;return 0;}
int bot_decide(const world *w,int p){const hero *h=&w->h[p];if(h->state==HS_READY)return INPUT_RELEASE;if(h->state!=HS_RUN)return 0;if(h->mode==MODE_FLY)return air_policy(w,h);
 if(h->mode!=MODE_SWING || h->held || h->wait<18)return 0;
 hero test=*h;test.air_t=0;int ax,ay;anchor_pos(w,h->anchor,&ax,&ay);test.silk_x=(int64_t)ax*Q16_ONE;test.silk_y=ay*Q16_ONE;
 for(int t=1;t<=110;t++){int ox,oy,otx,oty;silk_segment(&test,&ox,&oy,&otx,&oty);fly(&test,air_policy(w,&test));int x=(int)(test.wx/Q16_ONE),y=test.y/Q16_ONE;
  if(hazard(w,h,x,y,w->t+t) || x<-24 || x>344 || y>ay+180)return 0;
  if(silk_catch(w,&test,ox,oy,otx,oty)>=0)return INPUT_RELEASE;
 }
 return 0;
}
void world_step(world *w,const int input[MAX_PLAYERS]){
 memset(w->events,0,sizeof w->events);if(!w->started){int start=0;for(int p=0;p<w->players;p++)start|=input[p];if(!start)return;w->started=1;for(int p=0;p<w->players;p++){w->h[p].state=HS_RUN;w->events[p]=EV_START;}}
 w->t++;
 for(int p=0;p<w->players;p++){hero *h=&w->h[p];h->t++;h->land_t++;if(h->smooth_t>0)h->smooth_t--;int tap=(input[p]&INPUT_RELEASE) && !h->held;h->held=(input[p]&INPUT_RELEASE)!=0;
  if(h->state==HS_HIT){h->vy+=Q16_ONE/3;h->y+=h->vy;if(h->t>40){h->state=HS_DOWN;h->t=0;}continue;}if(h->state!=HS_RUN)continue;
  if(h->mode==MODE_SWING){
   h->wait++;
   if(h->wait>=flower_life(h->anchor)){hit(w,p);continue;}
   if(tap && h->wait>10){int ax,ay;anchor_pos(w,h->anchor,&ax,&ay);h->mode=MODE_FLY;h->grounded=0;h->air_t=0;h->silk_x=(int64_t)ax*Q16_ONE;h->silk_y=ay*Q16_ONE;h->web_t=0;w->events[p]|=EV_JUMP;}
   else rope_step(w,h,1);
  }else if(h->mode==MODE_CATCH){
   rope_step(w,h,0);h->web_t++;
   if(h->web_t>=6){h->mode=MODE_SWING;h->wait=0;}
  }else{
   int ox,oy,otx,oty;silk_segment(h,&ox,&oy,&otx,&oty);fly(h,input[p]);
   int x=(int)(h->wx/Q16_ONE),y=h->y/Q16_ONE,dx,dy;dew_pos(w,h->anchor,&dx,&dy);
   if(h->apple_id<h->anchor && abs(x-dx)<13 && abs(y-dy)<13){h->apple_id=h->anchor;h->apples++;h->score+=3;w->events[p]|=EV_APPLE;}
   int ax,ay;anchor_pos(w,h->anchor,&ax,&ay);
   if(hazard(w,h,x,y,w->t) || x<-24 || x>344 || y>ay+180){hit(w,p);continue;}
   int caught=silk_catch(w,h,ox,oy,otx,oty);
   if(caught>=0){h->anchor=caught;h->launches++;h->score+=10;h->combo++;h->smooth_t=30;attach(w,h);w->events[p]|=EV_LAND|EV_SCORE;}
  }
  h->x=(int)(h->wx/Q16_ONE);if(h->anchor*50>w->scroll)w->scroll=h->anchor*50;
 }
}
