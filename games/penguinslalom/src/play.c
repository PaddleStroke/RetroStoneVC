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
 o->distance=(int)((b->distance+(int64_t)ticks*(course_speed(w->scroll)+BEAR_SLIDE*Q16_ONE))>>16);
 o->pass_side=o->x<160?1:-1;
}
static int ramp(int scroll,int early,int late){
 if(scroll<0)scroll=0;
 if(scroll>BEAR_RAMP)scroll=BEAR_RAMP;
 return early+(late-early)*scroll/BEAR_RAMP;
}
int bear_warn_time(int scroll){int t=ramp(scroll,BEAR_WARN_EARLY,BEAR_WARN_LATE);return t<BEAR_WARN_MIN?BEAR_WARN_MIN:t;}
int bear_land(int players){int land=player_offset(players,0)-BEAR_LAND_GAP;return land<BEAR_LAND_TOP?BEAR_LAND_TOP:land;}
/* ---- fairness ------------------------------------------------------------------------------------------------------
 * A new bear is placed only if it is fair to every player: a small reachability check over the course's columns
 * (FAIR_CELL px), steps (FAIR_STEP frames) and speeds (columns per step, changing by FAIR_ACCEL at most: inertia),
 * from the moment the player has reacted to the marker (BEAR_REACT) until FAIR_AFTER frames after the bear has
 * passed the last penguin. Every "comfortable" state (one that can follow the course and the bears already warned at
 * FAIR_EASY columns per step at most) must keep a dodge of the new bear at FAIR_DODGE, the bear FAIR_BEAR_MARGIN px
 * wider and FAIR_BEAR_STEPS steps longer at both ends, ending in a comfortable state again (the course FAIR_MARGIN px
 * wider in both, swept through). tests/test_fair.c
 * proves the same of every bear on the exact steering (world_step's integers, 4-frame decisions). */
#define FAIR_CELLS ((296-24)/FAIR_CELL+1)
#define FAIR_STEPS ((FAIR_FRAMES-BEAR_REACT)/FAIR_STEP)
typedef uint8_t fair_mask[FAIR_STEPS][FAIR_CELLS];
static int isqrt64(int64_t v){int64_t r=0;for(int64_t b=(int64_t)1<<24;b;b>>=1)if((r+b)*(r+b)<=v)r+=b;return (int)r;}
static void block_span(uint8_t *cells,int lo,int hi,uint8_t bit){
 int c0=lo<=24?0:(lo-24+FAIR_CELL-1)/FAIR_CELL,c1=hi>=296?FAIR_CELLS-1:hi<24?-1:(hi-24)/FAIR_CELL;
 for(int c=c0;c<=c1;c++)cells[c]|=bit;
}
/* a hazard's x span on the penguin's line (an ellipse, or a pole's reach), widened by each margin: bit 1 for m[0],
 * bit 2 for m[1] when it is not negative */
static void block_both(uint8_t *cells,int lo,int hi,const int *m){
 block_span(cells,lo-m[0],hi+m[0],1);
 if(m[1]>=0)block_span(cells,lo-m[1],hi+m[1],2);
}
static void block_ellipse(uint8_t *cells,int ox,int od,int rx,int ry,int pd,const int *m){
 int dy=abs(od-pd);if(dy>ry)return;
 int hw=isqrt64((int64_t)rx*rx*((int64_t)ry*ry-(int64_t)dy*dy)/((int64_t)ry*ry))+1;
 block_both(cells,ox-hw,ox+hw,m);
}
/* a bear's distance from the camera `k` frames from now (BEAR_UNSEEN while it is still only a marker) */
#define BEAR_UNSEEN (1<<20)
static int bear_rel(const sliding_bear *b,int64_t travel,int k){
 if(b->warn>0)return k<b->warn?BEAR_UNSEEN:b->land+BEAR_SLIDE*(k-b->warn);
 return (int)((b->distance-travel)>>16)+BEAR_SLIDE*k;
}
/* the hazards for a player `o` px from the camera, per step: the course (what lies at most `seen` px down the slope)
 * and the bears already warned, or only `bear`; widened by margin m0 (bit 1) and, unless it is negative, m1 (bit 2) */
static void fair_hazards(const world *w,const int *fs,int o,int first,int steps,int m0,int m1,const sliding_bear *bear,int seen,fair_mask mask){
 const int m[2]={m0,m1};
 memset(mask,0,sizeof(fair_mask));
 for(int f=first+1;f<=first+steps*FAIR_STEP;f++){
  uint8_t *cells=mask[(f-first-1)/FAIR_STEP];int pd=fs[f]+o,pd0=fs[f-1]+o,base=course_index(w,pd);
  if(bear){
   int rel=bear_rel(bear,w->travel,f);
   if(rel<BEAR_UNSEEN)block_ellipse(cells,bear->x,fs[f]+rel,22+SKI_RADIUS,24+SKI_FORWARD_RADIUS,pd,m);
   continue;
  }
  for(int id=base>0?base-1:0;id<=base+1;id++){
   slope_obstacle ob;course_obstacle(w,id,fs[f],w->t+f,&ob);row r;course_row(w,id,&r);
   if(r.distance>seen)break;
   block_ellipse(cells,ob.x,ob.distance,ob.rx+SKI_RADIUS,ob.ry+SKI_FORWARD_RADIUS,pd,m);
   int reach=r.rx+SKI_RADIUS;
   if(abs(r.distance-pd)<=r.ry+SKI_RADIUS){block_both(cells,r.left-reach+1,r.left+reach-1,m);block_both(cells,r.right-reach+1,r.right+reach-1,m);}
   if(pd0<r.distance && pd>=r.distance){block_both(cells,-400,r.left+reach,m);block_both(cells,r.right-reach,700,m);}
  }
  for(int i=0;i<MAX_BEARS;i++)if(w->bears[i].active){
   int rel=bear_rel(&w->bears[i],w->travel,f);
   if(rel<BEAR_UNSEEN)block_ellipse(cells,w->bears[i].x,fs[f]+rel,22+SKI_RADIUS,24+SKI_FORWARD_RADIUS,pd,m);
  }
 }
}
/* backward reachability over (column, speed in columns per step, -top..top): out[s][c] has bit v+FAIR_TOP when the
 * cell is free and a speed change of at most FAIR_ACCEL leads to a state viable at the next step (the walls stop the
 * penguin), sweeping columns clear of the hazards whose mask bits are in `sweep`; at the last step, the free states
 * which are in `last` (all of them without it). Bit-parallel: each next speed marks all the speeds it can come from. */
#define FAIR_BITS(top) ((unsigned)(((1u<<(2*(top)+1))-1)<<(FAIR_TOP-(top))))
typedef uint16_t fair_set[FAIR_STEPS][FAIR_CELLS];
static void fair_viable(fair_mask mask,int steps,int top,int sweep,fair_set last,fair_set out){
 unsigned all=FAIR_BITS(top),from[2*FAIR_TOP+1];
 for(int nv=-top;nv<=top;nv++){int lo=nv-FAIR_ACCEL+FAIR_TOP;unsigned m=(1u<<(2*FAIR_ACCEL+1))-1;from[nv+FAIR_TOP]=(lo<0?m>>-lo:m<<lo)&all;}
 for(int c=0;c<FAIR_CELLS;c++)out[steps-1][c]=(uint16_t)(mask[steps-1][c]?0:all&(last?last[steps-1][c]:0xffffu));
 for(int s=steps-2;s>=0;s--){
  uint8_t swept[FAIR_CELLS+1];swept[0]=0;                 /* swept cells of the next step, counted */
  for(int k=0;k<FAIR_CELLS;k++)swept[k+1]=(uint8_t)(swept[k]+((mask[s+1][k]&sweep)!=0));
  for(int c=0;c<FAIR_CELLS;c++){
   unsigned bits=0;
   if(!mask[s][c])for(int nv=-top;nv<=top;nv++){
    int nc=c+nv,tv=nv;if(nc<0){nc=0;tv=0;}
    if(nc>FAIR_CELLS-1){nc=FAIR_CELLS-1;tv=0;}
    if(!(out[s+1][nc]>>(tv+FAIR_TOP)&1) || swept[(c>nc?c:nc)+1]!=swept[c<nc?c:nc])continue;
    bits|=from[nv+FAIR_TOP];
   }
   out[s][c]=(uint16_t)bits;
  }
 }
}
/* A plan for one warning: the window it checks and, once worked out, each penguin's comfortable states (shared by
 * the spots tried) */
typedef struct fair_plan {int warn,land,dodge,steps,done;int fs[FAIR_FRAMES+1];fair_mask course[MAX_PLAYERS];fair_set comfortable[MAX_PLAYERS];} fair_plan;
static int fair_setup(const world *w,int warn,int land,fair_plan *f){
 int lowest=player_offset(w->players,w->players-1);
 int pass=warn+(lowest-land+24+SKI_FORWARD_RADIUS+FAIR_BEAR_MARGIN+BEAR_SLIDE-1)/BEAR_SLIDE+1;
 f->warn=warn;f->land=land;f->done=0;
 f->dodge=(pass-BEAR_REACT+FAIR_STEP-1)/FAIR_STEP;f->steps=f->dodge+FAIR_AFTER/FAIR_STEP;
 if(warn<BEAR_WARN_MIN || f->steps>FAIR_STEPS)return 0;
 int64_t travel=w->travel;f->fs[0]=w->scroll;
 for(int k=1;k<=FAIR_FRAMES;k++){travel+=course_speed((int)(travel>>16));f->fs[k]=(int)(travel>>16);}
 return 1;
}
static int fair_try(const world *w,fair_plan *f,int x){
 if(x<BEAR_X_MIN || x>BEAR_X_MAX)return 0;
 int64_t at=w->travel+(int64_t)f->land*Q16_ONE;sliding_bear bear={1,-1,x,f->warn,f->warn,f->land,0,at,at};
 fair_mask hard;fair_set safe;
 for(int p=0;p<w->players;p++){
  int o=player_offset(w->players,p);
  // The same course for both (FAIR_MARGIN px wider, swept through): the comfortable states, at FAIR_EASY, and the
  // dodge, at FAIR_DODGE, where the new bear is drawn larger than it is, by FAIR_BEAR_MARGIN px and FAIR_BEAR_STEPS
  // steps at both ends: the coarse steps must never let a dodge through which the exact steering could not take.
  if(!(f->done>>p&1)){
   fair_hazards(w,f->fs,o,BEAR_REACT,f->steps,FAIR_MARGIN,-1,NULL,1<<30,f->course[p]);fair_viable(f->course[p],f->steps,FAIR_EASY,1,NULL,f->comfortable[p]);
   f->done|=1<<p;
  }
  fair_hazards(w,f->fs,o,BEAR_REACT,f->dodge,FAIR_BEAR_MARGIN,-1,&bear,1<<30,hard);
  for(int c=0;c<FAIR_CELLS;c++){
   uint8_t raw[FAIR_STEPS];for(int s=0;s<f->dodge;s++)raw[s]=hard[s][c];
   for(int s=0;s<f->dodge;s++){
    uint8_t near=0;for(int d=-FAIR_BEAR_STEPS;d<=FAIR_BEAR_STEPS;d++)if(s+d>=0 && s+d<f->dodge)near|=raw[s+d];
    hard[s][c]=(uint8_t)(f->course[p][s][c]|near<<1);
   }
  }
  fair_viable(hard,f->dodge,FAIR_DODGE,3,f->comfortable[p],safe);
  for(int c=0;c<FAIR_CELLS;c++)if(f->comfortable[p][0][c]&~safe[0][c])return 0;
 }
 return 1;
}
int bear_fair(const world *w,int x,int warn,int land){fair_plan f;return fair_setup(w,warn,land,&f) && fair_try(w,&f,x);}
/* the racing line (the flags' line) at a distance down the slope */
static int racing_line(const world *w,int d){
 int id=course_index(w,d);row a,b;course_row(w,id,&a);course_row(w,id+1,&b);
 if(d<=a.distance)return a.center;
 return a.center+(b.center-a.center)*(d-a.distance)/(b.distance-a.distance);
}
static void bears_step(world *w,int speed){
 for(int i=0;i<MAX_BEARS;i++)if(w->bears[i].active){
  sliding_bear *b=&w->bears[i];
  if(b->warn>0){
   // The marker keeps its place on the screen; the bear lands there when it runs out.
   b->distance=w->travel+(int64_t)b->land*Q16_ONE;b->previous=b->distance;
   if(--b->warn==0){b->age=0;w->bear_landed=b->x;}
  }else{
   b->previous=b->distance;b->distance+=speed+BEAR_SLIDE*Q16_ONE;b->age++;
   if(b->distance-w->travel>BEAR_RETIRE*Q16_ONE)b->active=0;
  }
 }
 if(--w->bear_wait>0)return;
 int slot=-1;for(int i=0;i<MAX_BEARS;i++)if(!w->bears[i].active){slot=i;break;}
 if(slot<0){w->bear_wait=15;return;}
 int warn=bear_warn_time(w->scroll),land=bear_land(w->players);fair_plan plan;
 if(!fair_setup(w,warn,land,&plan)){w->bear_wait=BEAR_RETRY;return;}
 // Aimed bears land on the racing line where they will cross the penguins' line; the others anywhere across.
 int64_t travel=w->travel;for(int f=0;f<warn-land/BEAR_SLIDE;f++)travel+=course_speed((int)(travel>>16));
 int line=racing_line(w,(int)(travel>>16));
 int aimed=ramp(w->scroll,BEAR_LINE_EARLY,BEAR_LINE_LATE),spread=ramp(w->scroll,BEAR_SPREAD_EARLY,BEAR_SPREAD_LATE);
 // One draw per warning decides whether it is aimed. An aimed bear tries the racing line first (within the spread),
 // then spots further out on alternate sides, BEAR_STEP_OUT px at a time: it lands as close to the line as is fair.
 uint32_t pick=mix(w->seed+(uint32_t)w->bear_id*733u);int aim=(int)(pick%100)<aimed,side=pick>>16&1?1:-1;
 for(int k=0;k<BEAR_TRIES;k++){
  uint32_t r=mix(w->seed+(uint32_t)w->bear_id*733u+(uint32_t)(k+1)*2246822519u);
  int out=(k+1)/2*BEAR_STEP_OUT*(k&1?side:-side);
  int x=aim && k<BEAR_TRIES-2?line+out+(int)(r%(uint32_t)(spread*2+1))-spread:BEAR_X_MIN+(int)(r%(BEAR_X_MAX-BEAR_X_MIN+1));
  if(x<BEAR_X_MIN)x=BEAR_X_MIN;
  if(x>BEAR_X_MAX)x=BEAR_X_MAX;
  if(!fair_try(w,&plan,x))continue;
  sliding_bear *bear=&w->bears[slot];int64_t at=w->travel+(int64_t)land*Q16_ONE;
  *bear=(sliding_bear){1,w->bear_id++,x,warn,warn,land,0,at,at};
  w->bear_warned=x;
  w->bear_wait=ramp(w->scroll,BEAR_GAP_EARLY,BEAR_GAP_LATE)+(int)((r>>20)%(uint32_t)(ramp(w->scroll,BEAR_GAP_RAND_EARLY,BEAR_GAP_RAND_LATE)+1));
  return;
 }
 w->bear_id++;w->bear_wait=BEAR_RETRY;
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
 memset(w,0,sizeof *w);w->seed=seed;w->players=n<1?1:n>4?4:n;w->bear_wait=BEAR_FIRST;
 for(int p=0;p<w->players;p++){
  hero *h=&w->h[p];h->state=HS_READY;h->x=160+(p*2-(w->players-1))*12;h->xx=h->x*Q16_ONE;
  h->y=player_y(w->players,p)*Q16_ONE;h->wx=(int64_t)player_offset(w->players,p)*Q16_ONE;h->distance=player_offset(w->players,p);
 }
}
static void hit(world *w,int p,int reason){hero *h=&w->h[p];h->state=HS_HIT;h->t=0;h->vx=0;h->combo=0;h->reason=reason;w->events[p]=EV_HIT;}
void world_step(world *w,const int input[4]){
 memset(w->events,0,sizeof w->events);w->bear_warned=w->bear_landed=0;
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
   for(int i=0;i<MAX_BEARS;i++)if(w->bears[i].active && !w->bears[i].warn){
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
/* ---- the bot (a test hook): it plays from what the screen shows --------------------------------------------------
 * The gates and obstacles down to the bottom of the screen, the bears' markers (where each bear lands and, by the
 * marker's size, when) and the sliding bears, over the next BOT_STEPS steps, on the fairness check's columns, steps
 * and speeds (inertia included). It plans the cheapest way through them: near the racing line (where the fish are),
 * steering little, keeping BOT_ROOM px from every hazard where it can; then it holds Left / Right towards the plan. */
#define BOT_STEPS 22                /* 88 frames: the course as far as the screen shows it, the bears all along */
#define BOT_ROOM 14                 /* px of room it likes to keep */
#define BOT_CLOSE 120               /* the cost of a step closer than that */
#define BOT_TOP 3                   /* the fastest it plans to go (columns per step): 1 px per frame to spare */
#define BOT_FAR 30000
static int bot_steer(const hero *h,int x){int d=x*Q16_ONE-(h->xx+h->vx*6);return d>2*Q16_ONE?1:d<-2*Q16_ONE?-1:0;}
int bot_decide(const world *w,int p){
 if(!w->started)return 2;
 const hero *h=&w->h[p];if(h->state!=HS_RUN)return 0;
 int o=player_offset(w->players,p),fs[BOT_STEPS*FAIR_STEP+1],pref[BOT_STEPS];int64_t travel=w->travel;fs[0]=w->scroll;
 for(int f=1;f<=BOT_STEPS*FAIR_STEP;f++){travel+=course_speed((int)(travel>>16));fs[f]=(int)(travel>>16);}
 for(int s=0;s<BOT_STEPS;s++)pref[s]=racing_line(w,fs[(s+1)*FAIR_STEP]+o);
 int here=(h->x-24+FAIR_CELL/2)/FAIR_CELL,speed=(h->vx+(h->vx>=0?Q16_ONE/2:-Q16_ONE/2))/Q16_ONE,want=0,best=BOT_FAR;
 if(here>FAIR_CELLS-1)here=FAIR_CELLS-1;
 fair_mask mask;int16_t cost[BOT_STEPS][FAIR_CELLS][2*FAIR_TOP+1];int8_t next[BOT_STEPS][FAIR_CELLS][2*FAIR_TOP+1];
 int seen=w->scroll+(240-HERO_LINE)*5/3;                /* the bottom of the screen */
 for(int k=0;k<2 && best>=BOT_FAR;k++){                  /* at 3 columns per step with room; else at 4, closer */
  int top=k?FAIR_TOP:BOT_TOP;
  fair_hazards(w,fs,o,0,BOT_STEPS,k?1:4,BOT_ROOM,NULL,seen,mask);   /* bit 1: blocked, bit 2: too close */
  for(int s=BOT_STEPS-1;s>=0;s--){
   int swept[FAIR_CELLS+1];swept[0]=0;                   /* the next step's blocked cells, counted */
   if(s<BOT_STEPS-1)for(int m=0;m<FAIR_CELLS;m++)swept[m+1]=swept[m]+(mask[s+1][m]&1);
   for(int c=0;c<FAIR_CELLS;c++){
    int val[2*FAIR_TOP+1],add=abs(24+c*FAIR_CELL-pref[s])/2+(mask[s][c]&2?BOT_CLOSE:0);
    if(s<BOT_STEPS-1)for(int nv=-top;nv<=top;nv++){      /* each next speed: its cost, or blocked on the way */
     int nc=c+nv,tv=nv;if(nc<0){nc=0;tv=0;}
     if(nc>FAIR_CELLS-1){nc=FAIR_CELLS-1;tv=0;}
     val[nv+FAIR_TOP]=swept[(c>nc?c:nc)+1]!=swept[c<nc?c:nc]?BOT_FAR:cost[s+1][nc][tv+FAIR_TOP]+abs(tv);
    }
    for(int v=-FAIR_TOP;v<=FAIR_TOP;v++){
     int e=BOT_FAR,n=-99;
     if(!(mask[s][c]&1)){
      if(s==BOT_STEPS-1)e=0;
      else for(int nv=v-FAIR_ACCEL;nv<=v+FAIR_ACCEL;nv++)if(nv>=-top && nv<=top && val[nv+FAIR_TOP]<e){e=val[nv+FAIR_TOP];n=nv;}
      if(e<BOT_FAR)e+=add;
     }
     cost[s][c][v+FAIR_TOP]=(int16_t)e;next[s][c][v+FAIR_TOP]=(int8_t)n;
    }
   }
  }
  // From the penguin's own column and speed: the first step's best speed.
  for(int nv=speed-FAIR_ACCEL;nv<=speed+FAIR_ACCEL;nv++){
   if(nv<-top || nv>top)continue;
   int nc=here+nv,tv=nv,clear=1;if(nc<0){nc=0;tv=0;}
   if(nc>FAIR_CELLS-1){nc=FAIR_CELLS-1;tv=0;}
   for(int m=here<nc?here:nc;m<=(here<nc?nc:here) && clear;m++)clear=!(mask[0][m]&1);
   int e=clear?cost[0][nc][tv+FAIR_TOP]+abs(tv):BOT_FAR;
   if(e<best){best=e;want=nv;}
  }
 }
 if(best>=BOT_FAR)return bot_steer(h,pref[1]);           /* no way through: back to the line */
 // Steer towards where the plan is two steps on, as the penguin will be 6 frames on at its speed.
 int c=here+want,v=want;if(c<0){c=0;v=0;}
 if(c>FAIR_CELLS-1){c=FAIR_CELLS-1;v=0;}
 if(next[0][c][v+FAIR_TOP]>-99)c+=next[0][c][v+FAIR_TOP];
 return bot_steer(h,24+c*FAIR_CELL);
}
