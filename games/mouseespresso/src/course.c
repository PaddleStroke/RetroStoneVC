/*
 * Mouse Espresso: the course. Small cafe items (hanging, standing, floating, moving), their collisions, and the
 * seeded director that chains parametrised patterns along a rising difficulty target with waves and breathers
 * (DESIGN.md "Obstacles", "Patterns", "The director"). Integers only: the same course on every CPU.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/mouseespresso/LICENSE.
 */
#include "game.h"
#include "sine.h"
#include <stdlib.h>
#include <string.h>

#define MAX_EXTENT 100      /* px: no item reaches further than this from its x (the ladle's swing is the widest) */
#define GEN_MAX 64          /* items in one pattern */

/* ---- shapes ------------------------------------------------------------------------------------------------------ */
static int hang_body(int kind){return kind==IT_LAMP?12:kind==IT_TEABAG?13:kind==IT_CUPHANG?11:13;}
static int lamp_hw(const item *it){return it->size;}
static int stand_top(const item *it){
 if(it->kind==IT_CUPS)return COUNTER_Y-11*it->var-2;
 if(it->kind==IT_SUGAR)return COUNTER_Y-8*it->var;
 return COUNTER_Y-it->len;
}
static int stand_hw(const item *it){return it->kind==IT_CUPS?7:it->kind==IT_SUGAR?4*it->size:11;}
static void spoon_ends(const item *it,int *ax,int *ay,int *bx,int *by){
 int l=it->size,d=l*181/256;   /* the bowl at (bx, by) */
 if(it->var==0){*ax=it->x-l;*ay=it->y;*bx=it->x+l;*by=it->y;}
 else if(it->var==1){*ax=it->x-d;*ay=it->y+d;*bx=it->x+d;*by=it->y-d;}
 else{*ax=it->x-d;*ay=it->y-d;*bx=it->x+d;*by=it->y+d;}
}
void item_extent(const item *it,int *left,int *right){
 int e=12;
 switch(it->kind){
 case IT_LAMP:e=it->size;break;
 case IT_FILTER:e=10;break;
 case IT_CUPS:e=9;break;
 case IT_SUGAR:e=4*it->size;break;
 case IT_JUG:e=16;break;
 case IT_SPOON:e=it->size+4;break;
 case IT_LADLE:e=it->len*ss_sine[it->size&255]/32767+LADLE_BOWL+2;break;
 default:break;
 }
 *left=it->x-e;*right=it->x+e;
}

/* ---- motion ------------------------------------------------------------------------------------------------------ */
static int jet_height(const item *it,int t,int *warn,int *rise){
 int c=((t+it->phase)%it->period+it->period)%it->period,on=it->size,n=c-JET_TELEGRAPH;
 *warn=c<JET_TELEGRAPH;*rise=c+1;
 if(n<0 || n>=on)return 0;
 if(n<JET_RISE)return it->len*(n+1)/JET_RISE;
 if(n>=on-JET_RISE)return it->len*(on-n)/JET_RISE;
 return it->len;
}
static void ladle_tip(const item *it,int t,int *x,int *y){
 int c=((t+it->phase)%it->period+it->period)%it->period;
 int swing=ss_sine[(c*256/it->period)&255];                /* -32767..32767 */
 int angle=(it->size*swing/32767)&255;                      /* 0 = hanging straight down */
 *x=it->x+it->len*ss_sine[angle]/32767;*y=it->len*ss_sine[(angle+64)&255]/32767;
}
static int bounce_y(const item *it,int t){
 int c=((t+it->phase)%it->period+it->period)%it->period,p=it->period,top=COUNTER_Y-5-it->y-5;   /* the rise, px */
 return COUNTER_Y-5-(int)((int64_t)4*top*c*(p-c)/((int64_t)p*p));
}
void item_pose_at(const item *it,int t,item_pose *p){
 memset(p,0,sizeof *p);
 if(it->kind==IT_JET){int w,r;p->h=jet_height(it,t,&p->warn,&p->rise);p->old_h=jet_height(it,t-1,&w,&r);p->on=p->h>0 || p->old_h>0;}
 else if(it->kind==IT_LADLE){ladle_tip(it,t,&p->tx,&p->ty);ladle_tip(it,t-1,&p->old_tx,&p->old_ty);}
 else if(it->kind==IT_BOUNCE){p->tx=p->old_tx=it->x;p->ty=bounce_y(it,t);p->old_ty=bounce_y(it,t-1);}
 else if(it->kind==IT_FILTER && it->period){
  int start=it->len+hang_body(IT_FILTER),c=((t+it->phase)%it->period+it->period)%it->period;
  for(int k=0;k<3;k++){
   int age=c+k*it->period,y=start+age*DRIP_FALL;
   if(y>=COUNTER_Y-4)continue;
   p->dy[p->drops]=y;p->old_dy[p->drops]=age?y-DRIP_FALL:y;p->drops++;
  }
 }
}

/* ---- collisions: the mouse's box (its centre swept from (x0, y0) to (x1, y1)) against solid rectangles ------------ */
static int rectangle(int left,int right,int top,int bottom,int x0,int x1,int y0,int y1){
 int lo=x0<x1?x0:x1,hi=x0>x1?x0:x1,up=y0<y1?y0:y1,down=y0>y1?y0:y1;
 return hi+HERO_RADIUS_X>=left && lo-HERO_RADIUS_X<=right && down+HERO_RADIUS_Y>=top && up-HERO_RADIUS_Y<=bottom;
}
static int near_line(int px,int py,int ax,int ay,int bx,int by,int radius){
 int64_t dx=bx-ax,dy=by-ay,x=px-ax,y=py-ay,len=dx*dx+dy*dy,dot=x*dx+y*dy,r=radius;
 if(dot<=0 || !len)return x*x+y*y<=r*r;
 if(dot>=len){x=px-bx;y=py-by;return x*x+y*y<=r*r;}
 int64_t cross=x*dy-y*dx;return cross*cross<=r*r*len;
}
int item_hits(const item *it,const item_pose *p,int x0,int x1,int y0,int y1){
 int x=it->x,l=it->len;
 switch(it->kind){
 case IT_LAMP:return rectangle(x-1,x+1,0,l,x0,x1,y0,y1) || rectangle(x-lamp_hw(it),x+lamp_hw(it),l,l+9,x0,x1,y0,y1) ||
  rectangle(x-3,x+3,l+9,l+12,x0,x1,y0,y1);
 case IT_TEABAG:return rectangle(x-1,x+1,0,l,x0,x1,y0,y1) || rectangle(x-5,x+5,l,l+13,x0,x1,y0,y1);
 case IT_CUPHANG:return rectangle(x-1,x+1,0,l,x0,x1,y0,y1) || rectangle(x-7,x+7,l,l+11,x0,x1,y0,y1);
 case IT_FILTER:
  if(rectangle(x-1,x+1,0,l,x0,x1,y0,y1) || rectangle(x-9,x+9,l,l+7,x0,x1,y0,y1) || rectangle(x-5,x+5,l+7,l+13,x0,x1,y0,y1))return 1;
  for(int k=0;k<p->drops;k++)if(rectangle(x-1,x+1,p->old_dy[k],p->dy[k]+4,x0,x1,y0,y1))return 1;
  return 0;
 case IT_CUPS:case IT_SUGAR:case IT_JUG:{
  int top=stand_top(it),hw=stand_hw(it);
  if(rectangle(x-hw,x+hw,top,COUNTER_Y,x0,x1,y0,y1))return 1;
  return it->kind==IT_JUG && rectangle(x+hw,x+hw+4,top+6,COUNTER_Y-8,x0,x1,y0,y1);
 }
 case IT_SPOON:{
  int ax,ay,bx,by;spoon_ends(it,&ax,&ay,&bx,&by);
  for(int k=0;k<=2;k++){
   int px=x0+(x1-x0)*k/2,py=y0+(y1-y0)*k/2;
   if(near_line(px,py,ax,ay,bx,by,2+HERO_RADIUS_Y) || near_line(px,py,bx,by,bx,by,4+HERO_RADIUS_Y))return 1;
  }
  return 0;
 }
 case IT_BEAN:return rectangle(x-it->size+1,x+it->size-1,it->y-3,it->y+3,x0,x1,y0,y1);
 case IT_CAKE:return rectangle(x-it->size+1,x+it->size-1,it->y-6,it->y+7,x0,x1,y0,y1);
 case IT_CROISSANT:return rectangle(x-it->size+1,x+it->size-1,it->y-5,it->y+5,x0,x1,y0,y1);
 case IT_JET:{
  int h=p->h>p->old_h?p->h:p->old_h;
  if(it->var==0){
   if(rectangle(x-6,x+6,COUNTER_Y-8,COUNTER_Y,x0,x1,y0,y1))return 1;
   return h>0 && rectangle(x-5,x+5,COUNTER_Y-8-h,COUNTER_Y-8,x0,x1,y0,y1);
  }
  if(rectangle(x-6,x+6,0,8,x0,x1,y0,y1))return 1;
  return h>0 && rectangle(x-5,x+5,8,8+h,x0,x1,y0,y1);
 }
 case IT_LADLE:
  /* the handle and the bowl, sampled along the motion of the ladle and the mouse together */
  for(int k=0;k<=4;k++){
   int px=x0+(x1-x0)*k/4,py=y0+(y1-y0)*k/4,tx=p->old_tx+(p->tx-p->old_tx)*k/4,ty=p->old_ty+(p->ty-p->old_ty)*k/4;
   if(near_line(px,py,x,0,tx,ty,2+HERO_RADIUS_Y) || near_line(px,py,tx,ty,tx,ty,LADLE_BOWL+HERO_RADIUS_Y))return 1;
  }
  return 0;
 case IT_BOUNCE:return rectangle(x-5,x+5,(p->ty<p->old_ty?p->ty:p->old_ty)-5,(p->ty>p->old_ty?p->ty:p->old_ty)+5,x0,x1,y0,y1);
 default:return 0;
 }
}

/* ---- the ring -------------------------------------------------------------------------------------------------------- */
const item *course_item(const course *c,uint32_t i){return &c->it[i%COURSE_N];}
const pattern_rec *course_pattern(const course *c,uint32_t i){return &c->pat[i%PATTERN_N];}
int course_first_near(const course *c,int32_t x){
 uint32_t lo=c->first,hi=c->n;
 while(lo<hi){uint32_t mid=lo+(hi-lo)/2;if(course_item(c,mid)->x<x-MAX_EXTENT)lo=mid+1;else hi=mid;}
 return (int)lo;
}
int course_hits(const course *c,int t,int x0,int x1,int y0,int y1){
 int lo=x0<x1?x0:x1,hi=x0>x1?x0:x1;
 for(uint32_t i=(uint32_t)course_first_near(c,lo-HERO_RADIUS_X);i<c->n;i++){
  const item *it=course_item(c,i);
  if(it->x-MAX_EXTENT>hi+HERO_RADIUS_X)break;
  if(it->kind==IT_BISCUIT)continue;
  item_pose p;item_pose_at(it,t,&p);
  if(item_hits(it,&p,x0,x1,y0,y1))return 1;
 }
 return 0;
}
void course_prune(course *c,int32_t behind){
 while(c->first<c->n && course_item(c,c->first)->x+MAX_EXTENT<behind)c->first++;
}

/* ---- the director's tools ------------------------------------------------------------------------------------------- */
static uint32_t rnd(course *c){uint32_t v=c->rng;v^=v<<13;v^=v>>17;v^=v<<5;return c->rng=v;}
static int rr(course *c,int lo,int hi){return hi<=lo?lo:lo+(int)(rnd(c)%(uint32_t)(hi-lo+1));}
static int lerpk(int a,int b,int k){return a+(b-a)*k/256;}
static int clampi(int v,int lo,int hi){return v<lo?lo:v>hi?hi:v;}
int course_speed(int distance){
 if(distance<0)distance=0;
 if(distance>SPEED_RAMP_PX)distance=SPEED_RAMP_PX;
 return SPEED_START+(int)((int64_t)(SPEED_MAX-SPEED_START)*distance/SPEED_RAMP_PX);
}
int course_target_k(int32_t x,int32_t wave_x){
 int d=x-HAZARD_FIRST;if(d<0)d=0;
 int base=K_START+(int)((int64_t)(K_END-K_START)*d/(d+K_HALF_PX));
 int pos=clampi((x-wave_x)*256/WAVE_PX,0,256),k=base+WAVE_AMP*(pos-128)/256;
 if(d<EASY_PX && k>K_EASY)k=K_EASY;
 return clampi(k,0,255);
}
typedef struct gen {
 course *c;item buf[GEN_MAX];int n,k,v,d,drip;int32_t x;  /* v: the speed there (Q16); d: the distance (unlocks);
                                                       drip: a filter hung now drips (only over a wide gap) */
} gen;

/* frames to move dy px with the flight's own physics, starting while drifting the wrong way at MOVE_AWAY, plus
 * MOVE_SETTLE to pass the gate; px travelled meanwhile, with the slack of difficulty k */
static int move_frames(int dy){
 int32_t y=0,vy=dy<0?MOVE_AWAY:-MOVE_AWAY,target=dy*Q16_ONE;int f=0;
 while(dy && f<240){
  vy+=dy<0?-THRUST:GRAVITY;if(vy<-VY_UP_MAX)vy=-VY_UP_MAX;
  if(vy>VY_DOWN_MAX)vy=VY_DOWN_MAX;
  y+=vy;f++;
  if(dy<0?y<=target:y>=target)break;
 }
 return f+MOVE_SETTLE;
}
static int travel_px(const gen *g,int dy){
 int frames=move_frames(dy)*lerpk(SLACK_EASY,SLACK_HARD,g->k)/256;
 return (int)((int64_t)frames*g->v/Q16_ONE);
}
/* the lead-in from the height `from` to `to` (from < 0: the mouse may be anywhere, at the ceiling or on the counter) */
static int lead_px(const gen *g,int from,int to){
 if(from>=0)return travel_px(g,to-from);
 int a=travel_px(g,to-FLIGHT_BOTTOM),b=travel_px(g,to-FLIGHT_TOP);return a>b?a:b;
}
static item *add(gen *g,int kind,int32_t x){
 if(g->n>=GEN_MAX)return NULL;
 item *it=&g->buf[g->n++];memset(it,0,sizeof *it);it->kind=(uint8_t)kind;it->x=x;return it;
}
/* a hanging item whose lowest solid row is `bottom` (nothing if the ceiling is free there) */
static void hang(gen *g,int32_t x,int bottom){
 if(bottom<10)return;
 int kinds[4]={IT_LAMP,IT_TEABAG,IT_CUPHANG,IT_FILTER},kind=kinds[rr(g->c,0,3)];
 if(kind==IT_FILTER && bottom<40)kind=IT_LAMP;
 int len=bottom-hang_body(kind);
 if(len<2){kind=IT_CUPHANG;len=bottom-hang_body(kind);}
 if(len<2)len=2;
 item *it=add(g,kind,x);if(!it)return;
 it->len=(int16_t)len;
 if(kind==IT_LAMP)it->size=(int16_t)rr(g->c,7,12);
 if(kind==IT_FILTER && g->drip){it->period=(int16_t)rr(g->c,100,130);it->phase=(int16_t)rr(g->c,0,99);}
}
/* a standing item whose top is at or below `top` (the gap is never narrower than asked) */
static void stand(gen *g,int32_t x,int top){
 int h=COUNTER_Y-top;if(h<8)return;
 /* cups stack up to 12, sugar cubes up to 11 (two wide up to 5), a jug is 22-38 px tall */
 int ok[3]={h>=13 && h<=11*12+2,h<=88 || h>11*12+2,h>=22 && h<=42},pick=rr(g->c,0,2);
 while(!ok[pick])pick=(pick+1)%3;
 item *it=add(g,pick==0?IT_CUPS:pick==1?IT_SUGAR:IT_JUG,x);if(!it)return;
 if(pick==0)it->var=(uint8_t)((h-2)/11);
 else if(pick==1){it->var=(uint8_t)(h/8);it->size=(int16_t)(h<=40?rr(g->c,1,2):1);}
 else it->len=(int16_t)(h>38?38:h);
}
/* a floating item of about `half` px half-height centred at (x, y) */
static void floater(gen *g,int32_t x,int y){
 int kinds[4]={IT_SPOON,IT_BEAN,IT_CAKE,IT_CROISSANT},kind=kinds[rr(g->c,0,3)];
 item *it=add(g,kind,x);if(!it)return;
 it->y=(int16_t)y;
 if(kind==IT_SPOON){it->size=(int16_t)rr(g->c,9,14);it->var=(uint8_t)rr(g->c,0,2);}
 else if(kind==IT_BEAN)it->size=(int16_t)rr(g->c,4,6);
 else if(kind==IT_CAKE)it->size=(int16_t)rr(g->c,9,11);
 else it->size=(int16_t)rr(g->c,10,12);
}
static int floater_half(int kind){return kind==IT_SPOON?15:kind==IT_BEAN?4:kind==IT_CAKE?7:6;}
static void biscuit(gen *g,int32_t x,int y){item *it=add(g,IT_BISCUIT,x);if(it)it->y=(int16_t)clampi(y,FLIGHT_TOP+4,FLIGHT_BOTTOM-6);}
/* a gate: the free band [top, bottom] at x (top <= 0: open to the ceiling; bottom >= COUNTER_Y: open to the floor) */
static void gate(gen *g,int32_t x,int top,int bottom){
 hang(g,x+rr(g->c,-2,2),top);
 if(bottom<COUNTER_Y-8)stand(g,x+rr(g->c,-2,2),bottom);
}
static int gap_size(const gen *g,int easy,int hard){return clampi(lerpk(easy,hard,g->k)+rr(g->c,-5,5),GAP_MIN,200);}
/* the free band around a centre: hugging the ceiling or the counter when it is close */
static void band(int centre,int gap,int *top,int *bottom){
 *top=centre-gap/2;*bottom=*top+gap;
 if(*top<FLIGHT_TOP+6){*top=0;if(*bottom<FLIGHT_TOP+gap/2)*bottom=FLIGHT_TOP+gap/2;}
 if(*bottom>FLIGHT_BOTTOM-4){*bottom=COUNTER_Y;if(*top>FLIGHT_BOTTOM-gap/2)*top=FLIGHT_BOTTOM-gap/2;}
}

/* ---- the patterns: each starts at g->x (after its own lead-in from the entry height) and returns its exit height,
 * leaving g->x at its end ------------------------------------------------------------------------------------------- */
static const int LEVELS[3]={52,118,184};
/* gaps at the top, middle or bottom, alternating */
static int pat_alternate(gen *g,int entry){
 int n=3+g->k*3/256+rr(g->c,0,1),level=rr(g->c,0,2),y=entry,gap=gap_size(g,GAP_EASY,GAP_HARD);
 if(entry>=0 && abs(LEVELS[level]-entry)<30)level=(level+1+rr(g->c,0,1))%3;
 int prize=rr(g->c,0,n-1);
 for(int i=0;i<n;i++){
  int centre=clampi(LEVELS[level]+rr(g->c,-10,10),FLIGHT_TOP+gap/2-6,FLIGHT_BOTTOM-gap/2+4);
  int span=lead_px(g,y,centre)+(i?14:6);
  /* a long move up or down: a half gate halfway, on the side the mouse leaves (it must really move) */
  if(i && abs(centre-y)>=60 && span>=70){
   int mid=(centre+y)/2;
   if(centre>y)hang(g,g->x+span/2+rr(g->c,-6,6),mid-gap/2-rr(g->c,0,8));
   else stand(g,g->x+span/2+rr(g->c,-6,6),mid+gap/2+rr(g->c,0,8));
  }
  g->x+=span;
  if(!g->c->npat && !i)g->x=HAZARD_FIRST;       /* the very first obstacle: the same place on every course */
  int top,bottom;band(centre,gap,&top,&bottom);
  g->drip=g->d>=UNLOCK_DRIPS && bottom-top>=92;gate(g,g->x,top,bottom);g->drip=0;
  if(i==prize)biscuit(g,g->x,(top>0?top:FLIGHT_TOP)+((bottom<COUNTER_Y?bottom:FLIGHT_BOTTOM)-(top>0?top:FLIGHT_TOP))/2);
  y=centre;int next=(level+1+rr(g->c,0,1))%3;level=next;
 }
 g->x+=14;return y;
}
/* narrow passages between a ceiling item and a counter item, one to three, each one or two items deep */
static int pat_narrow(gen *g,int entry){
 int n=1+rr(g->c,0,g->k>96?2:1),y=entry,gap=gap_size(g,GAP_NARROW_EASY,GAP_NARROW_HARD);
 for(int i=0;i<n;i++){
  int centre=clampi((y<0?118:y)+rr(g->c,-70,70),56,180);
  g->x+=lead_px(g,y,centre)+(i?16:8);
  int top=centre-gap/2,bottom=top+gap,deep=rr(g->c,0,1);
  gate(g,g->x,top,bottom);
  if(deep){g->x+=rr(g->c,16,22);gate(g,g->x,top,bottom);}
  biscuit(g,g->x+(deep?-9:0),centre+rr(g->c,-4,4));
  y=centre;
 }
 g->x+=14;return y;
}
/* a staircase: counter items climbing (or ceiling items descending), with a parallel line at higher difficulty */
static int pat_stairs(gen *g,int entry){
 int n=4+rr(g->c,0,2)+g->k/128,up=entry>118?0:1;
 if(rr(g->c,0,3)==0)up=!up;
 int step=rr(g->c,16,22)+g->k/32,gap=gap_size(g,GAP_EASY+10,GAP_HARD+14),both=g->k>80 && rr(g->c,0,2)>0;
 int rate=lerpk(130,190,g->k);                      /* the climb: px per frame x 100 */
 int centre=up?clampi(FLIGHT_BOTTOM-gap/2+2,0,FLIGHT_BOTTOM):clampi(FLIGHT_TOP+gap/2-2,FLIGHT_TOP,999);
 if(n*step>140)step=140/n;
 g->x+=lead_px(g,entry,centre)+8;
 for(int i=0;i<n;i++){
  int top=centre-gap/2,bottom=centre+gap/2;
  if(up){stand(g,g->x,bottom);if(both)hang(g,g->x+rr(g->c,-3,3),top);}
  else{hang(g,g->x,top);if(both)stand(g,g->x+rr(g->c,-3,3),bottom);}
  if(i==n/2)biscuit(g,g->x,centre);
  if(i<n-1){centre+=up?-step:step;g->x+=clampi((int)((int64_t)step*g->v*100/Q16_ONE/rate),18,80);}
 }
 g->x+=14;return centre;
}
/* a zigzag corridor: up then down (or down then up) */
static int pat_zigzag(gen *g,int entry){
 int gap=gap_size(g,GAP_EASY-6,GAP_HARD+16),amp=rr(g->c,40,70)+g->k/8,m=rr(g->c,2,3),step=amp/m;
 int dir=entry>118?-1:1,centre=clampi(entry<0?118:entry,FLIGHT_TOP+gap/2+amp/2,FLIGHT_BOTTOM-gap/2-amp/2);
 if(dir<0 && centre-amp<FLIGHT_TOP+gap/2)centre=FLIGHT_TOP+gap/2+amp;
 if(dir>0 && centre+amp>FLIGHT_BOTTOM-gap/2)centre=FLIGHT_BOTTOM-gap/2-amp;
 int rate=lerpk(120,160,g->k);
 g->x+=lead_px(g,entry,centre)+8;
 int legs=2+rr(g->c,0,g->k>128?1:0),prize=rr(g->c,1,legs*m-1),i=0;
 for(int leg=0;leg<legs;leg++){
  for(int s=0;s<m;s++,i++){
   int top,bottom;band(centre,gap,&top,&bottom);gate(g,g->x,top,bottom);
   if(i==prize)biscuit(g,g->x,centre);
   centre+=dir*step;g->x+=clampi((int)((int64_t)step*g->v*100/Q16_ONE/rate),18,90);
  }
  dir=-dir;g->x+=lerpk(26,18,g->k);     /* the turn */
 }
 int top,bottom;band(centre,gap,&top,&bottom);gate(g,g->x,top,bottom);
 g->x+=14;return centre;
}
/* a cluster of floating items with one safe line through it */
static int pat_cluster(gen *g,int entry){
 int len=rr(g->c,140,200)+g->k/2,gap=gap_size(g,GAP_EASY-10,GAP_HARD+16);
 int e=entry<0?118:entry,exit=clampi(e+rr(g->c,-40,40),FLIGHT_TOP+gap/2+10,FLIGHT_BOTTOM-gap/2-10);
 int start=clampi(e,FLIGHT_TOP+gap/2+10,FLIGHT_BOTTOM-gap/2-10);
 g->x+=lead_px(g,entry,start)+10;
 int32_t x0=g->x;int cols=len/26,prize=rr(g->c,0,cols-1);
 for(int i=0;i<=cols;i++){
  int32_t x=x0+i*len/cols+rr(g->c,-3,3);int line=start+(exit-start)*i/cols;
  int up=line-gap/2,down=line+gap/2;
  /* above the line: one or two floating items between the ceiling and the line, or a hanging one */
  int ya=up-floater_half(IT_SPOON)-rr(g->c,0,10),edge=i==0 || i==cols;   /* the ends are sealed: in by the line */
  if(ya>FLIGHT_TOP+4 && rr(g->c,0,3) && !edge){floater(g,x,ya);item *it=&g->buf[g->n-1];int h=floater_half(it->kind);if(it->y+h>up)it->y=(int16_t)(up-h);
   if(ya-60>FLIGHT_TOP && rr(g->c,0,1))floater(g,x+rr(g->c,-8,8),ya-rr(g->c,34,60));}
  else hang(g,x,up);
  int yb=down+floater_half(IT_SPOON)+rr(g->c,0,10);
  if(yb<FLIGHT_BOTTOM-4 && rr(g->c,0,3) && !edge){floater(g,x+rr(g->c,-5,5),yb);item *it=&g->buf[g->n-1];int h=floater_half(it->kind);if(it->y-h<down)it->y=(int16_t)(down+h);
   if(yb+60<FLIGHT_BOTTOM && rr(g->c,0,1))floater(g,x+rr(g->c,-8,8),yb+rr(g->c,34,60));}
  else stand(g,x,down);
  if(i==prize)biscuit(g,x+13,line);
 }
 g->x=x0+len+16;return exit;
}
/* a short corridor: a tunnel of ceiling and counter items */
static int pat_corridor(gen *g,int entry){
 int gap=gap_size(g,GAP_EASY-14,GAP_HARD),len=rr(g->c,60,110)+g->k/3;
 int centre=clampi((entry<0?118:entry)+rr(g->c,-50,50),FLIGHT_TOP+gap/2+4,FLIGHT_BOTTOM-gap/2-4);
 g->x+=lead_px(g,entry,centre)+8;
 int32_t end=g->x+len;int first=1;
 while(g->x<=end){int top,bottom;band(centre,gap,&top,&bottom);gate(g,g->x,top,bottom);
  if(first)biscuit(g,g->x+len/2,centre);
  first=0;g->x+=rr(g->c,18,26);
 }
 g->x+=6;return centre;
}
/* steam jets from the counter and the ceiling, firing in turn; their lanes alternate */
static int pat_jets(gen *g,int entry){
 int n=2+rr(g->c,0,1)+g->k/160,ceil_first=entry>118?1:0,y=entry,period=lerpk(150,108,g->k)+rr(g->c,-8,8);
 int lane=lerpk(76,52,g->k),phase=rr(g->c,0,period-1);
 for(int i=0;i<n;i++){
  int ceil=(i+ceil_first)&1,h=COUNTER_Y-8-FLIGHT_TOP-lane;   /* the jet leaves a lane of `lane` px when it fires */
  h=clampi(h-rr(g->c,0,40),60,170);
  int safe=ceil?clampi(8+h+lane/2,0,FLIGHT_BOTTOM):clampi(COUNTER_Y-8-h-lane/2,FLIGHT_TOP,999);
  g->x+=lead_px(g,y,safe)+12;
  item *it=add(g,IT_JET,g->x);
  if(it){it->var=(uint8_t)ceil;it->len=(int16_t)h;it->period=(int16_t)period;it->size=(int16_t)lerpk(44,64,g->k);
   it->phase=(int16_t)((phase+i*period/n)%period);}
  if(g->k>110 && rr(g->c,0,1)){if(ceil)stand(g,g->x+rr(g->c,-4,4),COUNTER_Y-rr(g->c,10,30));else hang(g,g->x+rr(g->c,-4,4),rr(g->c,10,30));}
  if(i==n-1)biscuit(g,g->x,ceil?COUNTER_Y-8-h/2:8+h/2);   /* inside the jet's column: only while it rests */
  y=safe;
 }
 g->x+=14;return -1;              /* the mouse may have crossed a resting jet anywhere */
}
/* swinging ladles from the ceiling; the counter keeps a lane under their swing */
static int pat_ladles(gen *g,int entry){
 int n=1+rr(g->c,0,1)+g->k/170,y=entry,lane=lerpk(78,54,g->k);
 for(int i=0;i<n;i++){
  int len=rr(g->c,52,96)+g->k/12,amp=rr(g->c,22,34),period=lerpk(150,104,g->k)+rr(g->c,-10,10);
  int below=len+LADLE_BOWL;int floor_top=clampi(below+lane,below+lane,COUNTER_Y);
  int safe=clampi(below+lane/2,FLIGHT_TOP,FLIGHT_BOTTOM);
  int reach=len*ss_sine[amp]/32767+LADLE_BOWL;
  g->x+=lead_px(g,y,safe)+reach+10;
  item *it=add(g,IT_LADLE,g->x);
  if(it){it->len=(int16_t)len;it->size=(int16_t)amp;it->period=(int16_t)period;it->phase=(int16_t)rr(g->c,0,period-1);}
  if(floor_top<COUNTER_Y-10 && g->k>60)stand(g,g->x+rr(g->c,-6,6),floor_top);
  if(rr(g->c,0,1))biscuit(g,g->x,len+LADLE_BOWL-2);       /* where the bowl passes: grab it behind the swing */
  g->x+=reach+6;y=safe;
 }
 g->x+=14;return -1;              /* or behind the swing */
}
/* sugar cubes bouncing on the counter; a ceiling line keeps a lane above them */
static int pat_bouncers(gen *g,int entry){
 int n=1+rr(g->c,0,2),y=entry,lane=lerpk(80,66,g->k);
 int apex=clampi(lerpk(150,96,g->k)+rr(g->c,-14,14),FLIGHT_TOP+lane+6,190),safe=apex-lane/2;
 g->x+=lead_px(g,y,safe)+10;
 for(int i=0;i<n;i++){
  item *it=add(g,IT_BOUNCE,g->x);
  int rise=COUNTER_Y-10-apex;
  if(it){it->y=(int16_t)apex;it->period=(int16_t)(48+rise/3+rr(g->c,0,10));it->phase=(int16_t)(i*it->period/n+rr(g->c,0,8));}
  if(g->k>90 && rr(g->c,0,1))hang(g,g->x+rr(g->c,-4,4),apex-lane);
  if(i==0)biscuit(g,g->x,apex+rr(g->c,10,30));
  g->x+=rr(g->c,40,60);
 }
 g->x+=6;return -1;                /* or between the bounces */
}
/* a breather: open floor, a little item, a biscuit arc */
static int pat_breather(gen *g,int entry){
 int len=rr(g->c,220,300),y=clampi(entry<0?118:entry,60,180);
 int32_t x0=g->x;
 if(rr(g->c,0,1))stand(g,x0+len/3,COUNTER_Y-rr(g->c,14,26));else hang(g,x0+len/3,rr(g->c,14,30));
 int arc=rr(g->c,0,1)?-1:1;
 for(int i=0;i<4;i++)biscuit(g,x0+len/2+i*18,y+arc*(ss_sine[(i*40)&255]*24/32767));
 g->x=x0+len;return -1;            /* open: the mouse may be anywhere */
}
typedef int (*pattern_fn)(gen *g,int entry);
static const pattern_fn PATTERNS[PK_COUNT]={pat_breather,pat_alternate,pat_narrow,pat_stairs,pat_zigzag,pat_cluster,
 pat_corridor,pat_jets,pat_ladles,pat_bouncers};
static const int UNLOCK[PK_COUNT]={0,0,0,0,UNLOCK_ZIGZAG,UNLOCK_CLUSTER,0,UNLOCK_JETS,UNLOCK_LADLES,UNLOCK_BOUNCERS};
static const int WEIGHT[PK_COUNT]={0,10,7,6,6,7,5,6,5,5};
static void mix_dynamics(gen *g){
 /* past UNLOCK_MIXED, a counter item may become a steam jet of its height, or a bouncing cube */
 if(g->d<UNLOCK_MIXED)return;
 for(int i=0;i<g->n;i++){
  item *it=&g->buf[i];
  if((it->kind!=IT_CUPS && it->kind!=IT_SUGAR) || (int)(rnd(g->c)%256)>=g->k/3)continue;
  int top=stand_top(it),h=COUNTER_Y-8-top;
  if(h<30)continue;
  int32_t x=it->x;memset(it,0,sizeof *it);it->x=x;
  it->kind=IT_JET;it->len=(int16_t)h;it->period=(int16_t)rr(g->c,100,140);it->size=(int16_t)rr(g->c,40,60);it->phase=(int16_t)rr(g->c,0,99);
 }
}
static void append(course *c,gen *g,int kind,int k,int exit){
 /* sorted by x (the ring is searched by x) */
 for(int i=1;i<g->n;i++){item t=g->buf[i];int j=i;while(j>0 && g->buf[j-1].x>t.x){g->buf[j]=g->buf[j-1];j--;}g->buf[j]=t;}
 pattern_rec *p=&c->pat[c->npat%PATTERN_N];
 for(int i=0;i<g->n;i++){g->buf[i].pat=(uint8_t)(c->npat%PATTERN_N);c->it[c->n%COURSE_N]=g->buf[i];c->n++;}
 p->x0=c->gen_x;p->x1=g->x;p->kind=(uint8_t)kind;p->k=(uint8_t)k;p->exit_y=(int16_t)exit;
 c->npat++;c->gen_x=g->x;c->exit_y=(int16_t)exit;
}
static void director_next(course *c){
 gen g;memset(&g,0,sizeof g);g.c=c;g.x=c->gen_x;g.d=c->gen_x-HAZARD_FIRST;g.v=course_speed(c->gen_x-48);
 int kind;
 if(c->npat==0){kind=PK_ALTERNATE;g.k=0;g.x=0;}           /* its first gate stands at HAZARD_FIRST */
 else if(c->gen_x-c->wave_x>=WAVE_PX){kind=PK_BREATHER;g.k=course_target_k(c->gen_x,c->wave_x);}
 else{
  g.k=clampi(course_target_k(c->gen_x,c->wave_x)+rr(c,-14,14),0,K_GEOM_MAX);
  int total=0,w[PK_COUNT];
  for(int i=1;i<PK_COUNT;i++){
   w[i]=g.d>=UNLOCK[i]?WEIGHT[i]:0;
   if(i==c->recent[0] || i==c->recent[1])w[i]=0;
   if(g.d>=UNLOCK[i] && g.d<UNLOCK[i]+900 && UNLOCK[i])w[i]*=4;       /* a new hazard arrives: show it */
   total+=w[i];
  }
  int pick=(int)(rnd(c)%(uint32_t)total);kind=1;
  while(pick>=w[kind]){pick-=w[kind];kind++;}
 }
 int exit=PATTERNS[kind](&g,c->exit_y);
 mix_dynamics(&g);
 append(c,&g,kind,g.k,exit);
 if(kind==PK_BREATHER){c->wave_x=c->gen_x;c->waves++;}
 c->recent[2]=c->recent[1];c->recent[1]=c->recent[0];c->recent[0]=(uint8_t)kind;
}
void course_init(course *c,uint32_t seed){
 memset(c,0,sizeof *c);
 c->rng=seed*2654435761u^0x6d2b79f5u;if(!c->rng)c->rng=1;
 for(int i=0;i<4;i++)rnd(c);
 c->gen_x=HAZARD_FIRST;c->wave_x=HAZARD_FIRST;c->exit_y=FLIGHT_BOTTOM;
 c->recent[0]=c->recent[1]=c->recent[2]=PK_COUNT;
}
void course_fill(course *c,int32_t upto){
 while(c->gen_x<upto && c->n-c->first<COURSE_N-GEN_MAX)director_next(c);
}
