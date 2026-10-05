/* A seeded river with scattered hazards, local currents and two fair fork routes. */
#include "game.h"
#include <string.h>
#include <stdlib.h>
static uint32_t mix(uint32_t v){v^=v>>16;v*=0x7feb352du;v^=v>>15;v*=0x846ca68bu;return v^(v>>16);}
static int clamp(int v,int a,int b){return v<a?a:v>b?b:v;}
static int phase(int d){int k=d%2400;return k<0?k+2400:k;}
static int section(int d,int span){return d>=0?d/span:(d-span+1)/span;}
static int smooth(int t,int span){int64_t q=(int64_t)clamp(t,0,span)*1024/span;return (int)(q*q*(3072-2*q)/1048576);}
/* Broad bends and independently wandering banks; the narrowest river is 218px. */
void river_banks(const world*w,int d,int*left,int*right){
 int n=section(d,640),t=d-n*640,s=smooth(t,640);
 uint32_t a=mix(w->seed+(uint32_t)n*43891u+73u),b=mix(w->seed+(uint32_t)(n+1)*43891u+73u);
 int l0=12+(int)(a%40),l1=12+(int)(b%40),r0=269+(int)((a>>9)%40),r1=269+(int)((b>>9)%40);
 *left=l0+(l1-l0)*s/1024;*right=r0+(r1-r0)*s/1024;
}
static void island_shape(const world*w,int d,int*start,int*end,int*radius,int*offset){
 uint32_t h=mix(w->seed+(uint32_t)section(d,2400)*3517u+117u);
 *start=950+(int)(h%201);*end=*start+350+(int)((h>>8)%301);
 *radius=23+(int)((h>>17)%21);*offset=(int)((h>>23)%41)-20;
}
int player_y(int n,int p){return HERO_LINE+(p*2-n+1)*14;}
int hero_distance(const world*w,int p){return w->scroll+HERO_LINE-player_y(w->players,p);}
int course_index(int d){return d<FIRST_ROW?0:(d-FIRST_ROW)/ROW_GAP;}
int32_t river_speed(int d){return Q16_ONE*24/10+(int64_t)clamp(d,0,20000)*Q16_ONE*14/10/20000;}
int river_current(int d,int x){int k=(d/480)%6;
 if(k==1&&x<210)return Q16_ONE*42/100;
 if(k==2&&x>110)return -Q16_ONE*36/100;
 if(k==4&&x>150)return -Q16_ONE*46/100;
 if(k==5&&x<170)return Q16_ONE*38/100;
 return 0;
}
int fish_allowed(const world*w,int d){int start,end,radius,offset;island_shape(w,d,&start,&end,&radius,&offset);int k=phase(d);return k<start-480||k>end+500;}
int river_island(const world*w,int d,int*center){
 int start,end,radius,offset,left,right,k=phase(d);island_shape(w,d,&start,&end,&radius,&offset);river_banks(w,d,&left,&right);
 *center=clamp((left+right)/2+offset,left+radius+64,right-radius-64);
 if(k<=start||k>=end)return 0;
 int taper=(end-start)/3,a=smooth(k-start,taper),b=smooth(end-k,taper);
 int width=radius*(a<b?a:b)/1024;
 return width;
}
static void waypoint(const world*w,int id,int*at,int*x){
 uint32_t h=mix(w->seed+(uint32_t)id*7919u);*at=FIRST_ROW+id*ROW_GAP+(int)((h>>20)%41)-20;
 int group=id/4,k=id%4;
 int a=96+(int)(mix(w->seed+(uint32_t)group*17929u)%129);
 int b=96+(int)(mix(w->seed+(uint32_t)(group+1)*17929u)%129);
 *x=(a*(4-k)+b*k)/4;
}
static int normal_route(const world*w,int d){
 int id=course_index(d),at,x,bt,bx;waypoint(w,id,&at,&x);
 if(d<at&&id>0){id--;waypoint(w,id,&at,&x);}waypoint(w,id+1,&bt,&bx);
 if(d<=at)return x;
 return x+(bx-x)*clamp(d-at,0,bt-at)/(bt-at);
}
int river_route(const world*w,int d,int side){
 int start,end,radius,offset,left,right;island_shape(w,d,&start,&end,&radius,&offset);river_banks(w,d,&left,&right);
 int normal=normal_route(w,d),k=phase(d),fork=side<0?left+24:right-24,blend=0;
 if(k>=start-480&&k<start-60)blend=(k-start+480)*256/420;
 else if(k>=start-60&&k<=end+60)blend=256;
 else if(k>end+60&&k<end+500)blend=(end+500-k)*256/440;
 return (normal*(256-blend)+fork*blend)/256;
}
static struct {uint32_t seed;int id,valid;river_row row;} row_cache[16];
void course_row(const world*w,int id,river_row*r){
 /* Pure seeded geometry: caching never depends on time, players or saved state. */
 int slot=(unsigned)id&15;
 if(row_cache[slot].valid&&row_cache[slot].seed==w->seed&&row_cache[slot].id==id){*r=row_cache[slot].row;return;}
 memset(r,0,sizeof *r);r->id=id;waypoint(w,id,&r->distance,&r->center);r->fish=fish_allowed(w,r->distance)?r->center:-1;
 uint32_t h=mix(w->seed+(uint32_t)id*9323u);int wanted=1+(int)(h%3);
 for(int j=0;j<wanted;j++)for(int trial=0;trial<32;trial++){
  h=mix(h+0x9e3779b9u);int kind=(int)(h%4),x=42+(int)((h>>4)%237),d=r->distance+(int)((h>>16)%201)-100;
  int rx=kind==LOG?28:kind==CROCODILE?17:kind==ROCK?14:17;
  int ry=kind==LOG?9:kind==CROCODILE?27:kind==ROCK?13:16,clear=1,center;
  /* Test the whole obstacle depth, not just its centre, against land and routes. */
  for(int offset=-ry-12;offset<=ry+12;offset+=ry+12){
   int dd=d+offset,left,right;river_banks(w,dd,&left,&right);
   int width=river_island(w,dd,&center);
   if(x-rx-5<=left||x+rx+5>=right||(width&&abs(x-center)<width+rx+8))clear=0;
   if(abs(x-river_route(w,dd,-1))<rx+24||abs(x-river_route(w,dd,1))<rx+24)clear=0;
  }
  for(int k=0;k<r->count;k++)if(abs(d-r->at[k])<ry+r->ry[k]+12&&abs(x-r->x[k])<rx+r->rx[k]+12)clear=0;
  if(!clear)continue;
  int k=r->count++;r->kind[k]=kind;r->x[k]=x;r->at[k]=d;r->rx[k]=rx;r->ry[k]=ry;break;
 }
 row_cache[slot].seed=w->seed;row_cache[slot].id=id;row_cache[slot].valid=1;row_cache[slot].row=*r;
}
void world_init(world*w,int n,uint32_t seed){memset(w,0,sizeof *w);w->players=clamp(n,1,4);w->seed=seed;
 for(int p=0;p<w->players;p++){hero*h=&w->h[p];h->state=HS_READY;h->x=160+(p*2-w->players+1)*14;h->xx=h->x*Q16_ONE;h->y=player_y(w->players,p)*Q16_ONE;}}
int world_running(const world*w){for(int p=0;p<w->players;p++)if(w->h[p].state<=HS_RUN)return 1;return 0;}
int world_all_down(const world*w){for(int p=0;p<w->players;p++)if(w->h[p].state!=HS_OFF)return 0;return 1;}
static int ellipse(int x,int y,int rx,int ry){return (int64_t)x*x*ry*ry+(int64_t)y*y*rx*rx<=(int64_t)rx*rx*ry*ry;}
static void hit(world*w,int p){w->h[p].state=HS_HIT;w->h[p].t=0;w->events[p]=EV_HIT;}
void world_step(world*w,const int input[4]){
 memset(w->events,0,sizeof w->events);w->t++;
 for(int p=0;p<4;p++)if(w->bite[p].active){if(w->bite[p].t<60)w->bite[p].t++;}
 if(!w->started){w->started=1;for(int p=0;p<w->players;p++)w->h[p].state=HS_RUN;}
 if(world_running(w)){w->travel+=river_speed(w->scroll);w->scroll=(int)(w->travel>>16);}
 for(int p=0;p<w->players;p++){hero*h=&w->h[p];h->t++;
  if(h->state==HS_HIT){if(h->t>=HIT_FREEZE){h->state=HS_DOWN;h->t=0;}continue;}
  if(h->state==HS_DOWN){if(h->t>=28){h->state=HS_OFF;h->t=0;}continue;}
  if(h->state!=HS_RUN)continue;
  int d=hero_distance(w,p),dir=input[p]<0?-1:input[p]>0?1:0;
  int32_t target=dir*Q16_ONE*27/10;h->vx+=(target-h->vx)/5;
  int left,right;river_banks(w,d,&left,&right);
  h->xx+=h->vx+river_current(d,h->x);h->xx=clamp(h->xx,(left+9)*Q16_ONE,(right-9)*Q16_ONE);h->x=(int)(h->xx>>16);
  int center,width=river_island(w,d,&center),bad=width&&abs(h->x-center)<width+7;
  int base=course_index(d);
  for(int id=base>0?base-1:0;id<=base+1;id++){river_row r;course_row(w,id,&r);
   for(int k=0;k<r.count;k++)if(ellipse(h->x-r.x[k],r.at[k]-d,r.rx[k]+7,r.ry[k]+9)){
    bad=1;if(r.kind[k]==CROCODILE){w->bite[p].active=1;w->bite[p].x=r.x[k];w->bite[p].distance=r.at[k];w->bite[p].t=0;}
   }
  }
  if(bad){hit(w,p);continue;}
  river_row r;course_row(w,h->next_row,&r);int dy=r.distance-d;
  if(r.fish>=0&&dy<=14&&dy>=-14&&abs(h->x-r.fish)<18&&h->collected!=r.id+1){h->collected=r.id+1;h->fish++;w->events[p]|=EV_SCORE;}
  if(dy<-14){if(r.fish>=0&&h->collected!=r.id+1){hit(w,p);continue;}h->next_row++;}
  h->score=w->scroll/14+h->fish*15;
 }
}
int bot_decide(const world*w,int p){const hero*h=&w->h[p];int d=hero_distance(w,p),target=river_route(w,d+12,(p&1)?1:-1);
 int predicted=h->x+(int)(h->vx*4/Q16_ONE);return predicted<target-2?1:predicted>target+2?-1:0;}
