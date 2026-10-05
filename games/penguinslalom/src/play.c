#include "game.h"
#include "sine.h"
#include <stdlib.h>
#include <string.h>
static uint32_t mix(uint32_t v){v^=v>>16;v*=0x7feb352du;v^=v>>15;v*=0x846ca68bu;return v^(v>>16);}
int player_y(int players,int p){return HERO_LINE+p*24-(players-1)*12;}
int player_offset(int players,int p){return (player_y(players,p)-HERO_LINE)*5/3;}
int course_speed(int distance){if(distance<0)distance=0;if(distance>24000)distance=24000;return 216269+(int)((int64_t)distance*91750/24000);}
void course_row(const world *w,int id,row *r){
 uint32_t v=mix(w->seed+(uint32_t)id*2654435761u);int level=id/12;if(level>5)level=5;
 /* Seeded short/long pairs have a fixed total, giving O(1) endless-course lookup without a fixed gate grid. */
 int first_gap=GATE_SPACING+((int)(mix(w->seed+(uint32_t)(id/2)*733u)%9)-4)*14;
 r->id=id;r->distance=GATE_FIRST+(id/2)*(GATE_SPACING*2)+(id&1?first_gap:0);
 r->center=id==0?100:160+ss_sine[(id*43+(int)(w->seed&255))&255]*62/32767+(int)(v%25)-12;
 r->gap=id==0?132:104+(int)((v>>8)%5)*12-level*6;r->kind=id&1;r->rx=4;r->ry=5;
 if(r->center<24+r->gap/2)r->center=24+r->gap/2;
 if(r->center>296-r->gap/2)r->center=296-r->gap/2;
 r->left=r->center-r->gap/2;r->right=r->center+r->gap/2;
}
int course_index(const world *w,int distance){
 if(distance<GATE_FIRST)return 0;
 int id=(distance-GATE_FIRST)/(GATE_SPACING*2)*2;row r;course_row(w,id+1,&r);return distance>=r.distance?id+1:id;
}
void course_obstacle(const world *w,int id,int travel,int t,slope_obstacle *o){
 row a,b;course_row(w,id,&a);course_row(w,id+1,&b);uint32_t r=mix(w->seed+(uint32_t)id*977u);
 memset(o,0,sizeof *o);o->id=id;o->kind=id<PROP_COUNT?id:(int)((r>>8)%PROP_COUNT);
 int spacing=b.distance-a.distance,path=(a.center+b.center)/2,turn=b.center-a.center;
 int side=turn>12?-1:turn<-12?1:(r&1?1:-1);
 int central=id>=4 && spacing>=300 && abs(turn)<=28 && r%3==0;
 o->x=path+(central?0:side*55);if(o->x<46)o->x=46;if(o->x>274)o->x=274;
 o->distance=(a.distance+b.distance)/2;
 o->pass_side=b.center>o->x?1:b.center<o->x?-1:(r&2?1:-1);
 if(o->x<68)o->pass_side=1;
 if(o->x>252)o->pass_side=-1;
 if(o->kind==PROP_ROCK){o->rx=16;o->ry=18;}
 else if(o->kind==PROP_TREE){o->rx=15;o->ry=32;}
 else if(o->kind==PROP_HOLE){o->rx=18;o->ry=15;}
 else{
  o->rx=18;o->ry=22;o->x+=ss_sine[(t*2+id*31)&255]*16/32767;
 }
 (void)travel;
}
void bear_obstacle(const world *w,int slot,int ticks,slope_obstacle *o){
 const sliding_bear *b=&w->bears[slot];memset(o,0,sizeof *o);
 o->id=b->id;o->kind=PROP_BEAR;o->x=b->x;o->rx=22;o->ry=24;
 o->distance=(int)((b->distance+(int64_t)ticks*(course_speed(w->scroll)+3*Q16_ONE))>>16);
 o->pass_side=o->x<160?1:-1;
}
static void bears_step(world *w,int speed){
 for(int i=0;i<MAX_BEARS;i++)if(w->bears[i].active){
  sliding_bear *b=&w->bears[i];b->previous=b->distance;
  b->distance+=speed+3*Q16_ONE;
  if(b->distance-w->travel>280*Q16_ONE)b->active=0;
 }
 if(--w->bear_wait>0)return;
 int slot=-1;for(int i=0;i<MAX_BEARS;i++)if(!w->bears[i].active){slot=i;break;}
 if(slot<0){w->bear_wait=15;return;}
 uint32_t r=mix(w->seed+(uint32_t)w->bear_id*733u);
 int future=w->scroll+(int)((int64_t)course_speed(w->scroll)*80/Q16_ONE);
 int first=course_index(w,future-220),last=course_index(w,future+220)+1,low=296,high=24;
 // Keep the passing lane clear of the gates encountered during the overtake.
 // Players can stay on the slalom line rather than being trapped against a pole.
 for(int id=first;id<=last;id++){row gate;course_row(w,id,&gate);if(gate.center<low)low=gate.center;if(gate.center>high)high=gate.center;}
 int margin=52+(int)((r>>16)%25),left=low-margin,right=high+margin;
 if(left<40)left=40;
 if(right>280)right=280;
 int left_clear=320,right_clear=320;
 for(int id=first;id<=last;id++){
  row gate,next;course_row(w,id,&gate);course_row(w,id+1,&next);slope_obstacle o;course_obstacle(w,id,w->scroll,w->t+80,&o);
  int route=next.center,room=o.rx+SKI_RADIUS+14;
  if(abs(route-o.x)<room)route=o.x+o.pass_side*room;
  int lc=abs(gate.center-left);if(abs(route-left)<lc)lc=abs(route-left);
  int rc=abs(gate.center-right);if(abs(route-right)<rc)rc=abs(route-right);
  if(lc<left_clear)left_clear=lc;
  if(rc<right_clear)right_clear=rc;
 }
 if(left_clear<44 && right_clear<44){w->bear_wait=30;return;}
 int x=left_clear>right_clear?left:right_clear>left_clear?right:(r&1?right:left);
 sliding_bear *bear=&w->bears[slot];bear->active=1;bear->id=w->bear_id++;bear->x=x;
 bear->distance=w->travel-240*Q16_ONE;bear->previous=bear->distance;
 w->bear_wait=330+(int)((r>>8)%151);
}
int obstacle_hits(const slope_obstacle *before,const slope_obstacle *after,int x0,int x1,int d0,int d1){
 /* A swept ellipse checks relative penguin/obstacle motion, including the sliding bear. */
 int64_t rx=after->rx+SKI_RADIUS,ry=after->ry+SKI_FORWARD_RADIUS;
 int64_t ax=(x0-before->x)*ry,ay=(d0-before->distance)*rx,bx=(x1-after->x)*ry,by=(d1-after->distance)*rx;
 int64_t dx=bx-ax,dy=by-ay,len=dx*dx+dy*dy,dot=-ax*dx-ay*dy,radius=rx*ry;
 if(dot<=0 || !len)return ax*ax+ay*ay<=radius*radius;
 if(dot>=len)return bx*bx+by*by<=radius*radius;
 int64_t cross=ax*dy-ay*dx;return cross*cross<=radius*radius*len;
}
void world_init(world *w,int n,uint32_t seed){
 memset(w,0,sizeof *w);w->seed=seed;w->players=n<1?1:n>4?4:n;w->bear_wait=300;
 for(int p=0;p<w->players;p++){
  hero *h=&w->h[p];h->state=HS_READY;h->x=160+(p*2-(w->players-1))*12;h->xx=h->x*Q16_ONE;
  h->y=player_y(w->players,p)*Q16_ONE;h->wx=(int64_t)player_offset(w->players,p)*Q16_ONE;h->distance=player_offset(w->players,p);
 }
}
static void hit(world *w,int p,int reason){hero *h=&w->h[p];h->state=HS_HIT;h->t=0;h->vx=0;h->combo=0;h->reason=reason;w->events[p]=EV_HIT;}
void world_step(world *w,const int input[4]){
 memset(w->events,0,sizeof w->events);
 if(!w->started){
  int start=0;for(int p=0;p<w->players;p++)start|=input[p]!=0;if(!start)return;
  w->started=1;for(int p=0;p<w->players;p++)w->h[p].state=HS_RUN;w->events[0]=EV_START;return;
 }
 if(world_running(w)){int speed=course_speed((int)(w->travel>>16));w->t++;w->travel+=speed;w->scroll=(int)(w->travel>>16);bears_step(w,speed);}
 for(int p=0;p<w->players;p++){
  hero *h=&w->h[p];h->t++;if(h->smooth_t>0)h->smooth_t--;
  if(h->state==HS_RUN){
   int old_x=h->x,dir=input[p]==-1?-1:input[p]==1?1:0;
   h->vx=dir?h->vx+dir*39322:h->vx*4/5;if(h->vx>262144)h->vx=262144;if(h->vx<-262144)h->vx=-262144;
   h->xx+=h->vx;if(h->xx<24*Q16_ONE){h->xx=24*Q16_ONE;h->vx=0;}if(h->xx>296*Q16_ONE){h->xx=296*Q16_ONE;h->vx=0;}h->x=h->xx>>16;
   int old=h->distance;h->wx=w->travel+(int64_t)player_offset(w->players,p)*Q16_ONE;h->distance=(int)(h->wx>>16);
   if(!(w->t&3)){int k=h->trail_head;h->trail_x[k]=h->x;h->trail_d[k]=h->distance;h->trail_head=(k+1)&31;if(h->trail_count<32)h->trail_count++;}
   int base=course_index(w,h->distance);
   for(int id=base>0?base-1:0;id<=base+1;id++){
    slope_obstacle previous,current;int previous_travel=old-player_offset(w->players,p);
    course_obstacle(w,id,previous_travel,w->t-1,&previous);course_obstacle(w,id,w->scroll,w->t,&current);
    if(obstacle_hits(&previous,&current,old_x,h->x,old,h->distance)){hit(w,p,2);break;}
   }
   if(h->state!=HS_RUN)continue;
   for(int i=0;i<MAX_BEARS;i++)if(w->bears[i].active){
    slope_obstacle previous,current;bear_obstacle(w,i,0,&current);previous=current;previous.distance=(int)(w->bears[i].previous>>16);
    if(obstacle_hits(&previous,&current,old_x,h->x,old,h->distance)){hit(w,p,2);break;}
   }
   if(h->state!=HS_RUN)continue;
   for(int id=base>0?base-1:0;id<=base+1;id++){
    row r;course_row(w,id,&r);int dy=r.distance-h->distance;
    if(abs(dy)<=r.ry+SKI_RADIUS && (abs(h->x-r.left)<r.rx+SKI_RADIUS || abs(h->x-r.right)<r.rx+SKI_RADIUS)){hit(w,p,1);break;}
    if(old<r.distance && h->distance>=r.distance){
     // Missing either side fails on this exact crossing frame, before any success event.
     if(h->x<=r.left+r.rx+SKI_RADIUS || h->x>=r.right-r.rx-SKI_RADIUS){hit(w,p,1);break;}
     h->anchor++;w->events[p]|=EV_SCORE;
     if(abs(h->x-r.center)<=16){h->apples++;h->smooth_t=12;w->events[p]|=EV_APPLE;}
     int near=abs(h->x-r.left)<abs(h->x-r.right)?abs(h->x-r.left):abs(h->x-r.right);
     if(near<24){if(h->combo<4)h->combo++;h->bonus+=5*h->combo;h->smooth_t=16;}else h->combo=0;
    }
   }
   if(h->state==HS_RUN)h->score=w->scroll/12+h->apples*5+h->bonus;
  }else if(h->state==HS_HIT && h->t>=20)h->state=HS_DOWN;
  else if(h->state==HS_DOWN && h->t>=40)h->state=HS_OFF;
 }
}
int world_running(const world *w){for(int p=0;p<w->players;p++)if(w->h[p].state==HS_RUN || w->h[p].state==HS_READY)return 1;return 0;}
int world_all_down(const world *w){for(int p=0;p<w->players;p++)if(w->h[p].state!=HS_OFF)return 0;return 1;}
int bot_decide(const world *w,int p){
 if(!w->started)return 2;
 const hero *h=&w->h[p];if(h->state!=HS_RUN)return 0;
 int id=h->distance<GATE_FIRST+18?0:course_index(w,h->distance-18)+1;row r;course_row(w,id,&r);int target=r.center;
 // Choose the appropriate side of a gate before a bear and the following
 // obstacle require the same escape route. This still uses ordinary steering.
 if(r.distance-h->distance<120){
  slope_obstacle future;course_obstacle(w,id,w->scroll,w->t,&future);
  int ticks=(future.distance-h->distance)*Q16_ONE/course_speed(w->scroll);if(ticks<0)ticks=0;
  course_obstacle(w,id,w->scroll+(int)((int64_t)ticks*course_speed(w->scroll)/Q16_ONE),w->t+ticks,&future);
  slope_obstacle next_hazards[MAX_BEARS+1];next_hazards[0]=future;int next_count=1;
  for(int i=0;i<MAX_BEARS;i++)if(w->bears[i].active){slope_obstacle bear;bear_obstacle(w,i,0,&bear);
   int separation=h->distance-bear.distance;
   if(separation>-70 && separation<240)next_hazards[next_count++]=bear;
  }
  if(next_count>1){
   row following;course_row(w,id+1,&following);int best=100000,planned=following.center;
   for(int x=30;x<=290;x++){
    int safe=1;for(int i=0;i<next_count;i++)if(abs(x-next_hazards[i].x)<next_hazards[i].rx+SKI_RADIUS+14){safe=0;break;}
    if(!safe)continue;
    int cost=abs(x-following.center)*4+abs(x-r.center);if(cost<best){best=cost;planned=x;}
   }
   target=planned;if(target<r.left+17)target=r.left+17;if(target>r.right-17)target=r.right-17;
  }
 }
 slope_obstacle hazards[MAX_BEARS+1];int count=0;
 if(id>0){slope_obstacle o;course_obstacle(w,id-1,w->scroll,w->t,&o);
  if(h->distance<o.distance+o.ry+SKI_FORWARD_RADIUS+8){
   int ticks=(o.distance-h->distance)*Q16_ONE/course_speed(w->scroll);if(ticks<0)ticks=0;
   course_obstacle(w,id-1,w->scroll+(int)((int64_t)ticks*course_speed(w->scroll)/Q16_ONE),w->t+ticks,&o);
   hazards[count++]=o;
  }
 }
 for(int i=0;i<MAX_BEARS;i++)if(w->bears[i].active){
  slope_obstacle o;bear_obstacle(w,i,0,&o);int separation=h->distance-o.distance;
  // The bear overtakes us at three world pixels per frame. Aim around its fixed lane.
  if(separation>-(o.ry+SKI_FORWARD_RADIUS+16) && separation<120){
   hazards[count++]=o;
  }
 }
 if(count){
  int desired=target,best=100000,near_gate=r.distance-h->distance<(int)((int64_t)course_speed(w->scroll)*12/Q16_ONE)+18;
  for(int x=30;x<=290;x++){
   if(near_gate && (x<=r.left+r.rx+SKI_RADIUS+3 || x>=r.right-r.rx-SKI_RADIUS-3))continue;
   int safe=1;for(int i=0;i<count;i++){
    const slope_obstacle *o=&hazards[i];
    if(abs(x-o->x)<o->rx+SKI_RADIUS+14){safe=0;break;}
    if(o->kind!=PROP_BEAR && abs(h->x-o->x)>=o->rx+SKI_RADIUS+9 && abs(o->distance-h->distance)<o->ry+SKI_FORWARD_RADIUS+80 && (x-o->x)*(h->x-o->x)<0){safe=0;break;}
   }
   if(!safe)continue;
   int cost=abs(x-desired)*4+abs(x-h->x);if(cost<best){best=cost;target=x;}
  }
  if(best==100000){
   // Finish the immediate fixed-obstacle passage before steering away from
   // a bear which is still vertically separated from the penguin.
   for(int x=30;x<=290;x++){
    if(near_gate && (x<=r.left+r.rx+SKI_RADIUS+3 || x>=r.right-r.rx-SKI_RADIUS-3))continue;
    int safe=1;for(int i=0;i<count;i++){
     const slope_obstacle *o=&hazards[i];if(o->kind==PROP_BEAR)continue;
     if(abs(x-o->x)<o->rx+SKI_RADIUS+14 || (abs(h->x-o->x)>=o->rx+SKI_RADIUS+9 && (x-o->x)*(h->x-o->x)<0)){safe=0;break;}
    }
    if(!safe)continue;
    int cost=abs(x-desired)*4+abs(x-h->x);if(cost<best){best=cost;target=x;}
   }
  }
 }
 int forecast=h->xx+h->vx*6,delta=target*Q16_ONE-forecast;return delta>2*Q16_ONE?1:delta<-2*Q16_ONE?-1:0;
}
