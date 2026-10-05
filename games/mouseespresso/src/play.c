/* Continuous one-button flight, a shared course and seeded kitchen hazards. */
#include "game.h"
#include "sine.h"
#include <stdlib.h>
#include <string.h>
static uint32_t mix(uint32_t v){v^=v>>16;v*=0x7feb352du;v^=v>>15;v*=0x846ca68bu;return v^(v>>16);}
int course_speed(int distance){int level=distance/2200;if(level>5)level=5;return 150733+level*15729;}
int hero_start_x(int players,int p){return players==1?72:48+p*28;}
static void toast_position(int x,int age,int duration,int height,int drift,int slot,int *px,int *py){
 *px=x+slot+age*drift/256;*py=213-4*height*age*(duration-age)/(duration*duration);
}
static int paw_reach(int phase,int period,int maximum){
 int attack=period-PAW_ATTACK_FRAMES,n=phase-attack;
 if(n<-PAW_WARNING_FRAMES)return 0;
 if(n<0){
  int peek=n+PAW_WARNING_FRAMES;
  if(peek<12)return 24*(peek+1)/12;
  /* Three small tentative strokes, with the palm clearly inside the screen. */
  int stroke=(peek-12)%20;
  return 24+(stroke<=10?stroke:20-stroke);
 }
 if(n<6)return 24+(maximum-24)*(n+1)/6;
 if(n<18)return maximum;
 if(n<40)return maximum*(40-n)/22;
 return 0; /* Fully withdrawn, with a long pause between swats. */
}
static int steam_height(int phase,int period){
 int n=phase-(period-66);
 if(n<0)return 0;
 if(n<6)return 72*(n+1)/6;
 if(n<36)return 72;
 if(n<54)return 72*(54-n)/18;
 return 0;
}
void obstacle_at(const world *w,int id,int t,obstacle *o){
 uint32_t r=mix(w->seed+(uint32_t)id*2654435761u),block=mix(w->seed+(uint32_t)(id/OB_COUNT)*733u);
 memset(o,0,sizeof *o);o->id=id;o->x=HAZARD_FIRST+id*HAZARD_SPACING;
 o->type=id<OB_COUNT?id:((id%OB_COUNT)*((block&1)?1:OB_COUNT-1)+(int)(block%OB_COUNT))%OB_COUNT;
 o->variant=(r>>8)%3;o->side=r&1;
 if(id==5)o->variant=0; /* Introduce the espresso machine before its variants. */
 int level=(HAZARD_FIRST+id*HAZARD_SPACING)/2200;if(level>5)level=5;
 if(o->type==OB_STOOL){o->tip=146-(int)(r%3)*12;o->width=24;}
 else if(o->type==OB_SHELF){o->tip=76+(int)(r%3)*16;o->width=34;}
 else if(o->type==OB_TABLE){o->tip=112+(int)(r%4)*12;o->width=76+(int)((r>>4)%3)*14;}
 else if(o->type==OB_APPLIANCE){
  o->tip=150-(int)(r%3)*12;o->width=23;
  if(o->variant==1){
   o->spin=(t*(2+level)+id*31)&255;
   for(int j=0;j<4;j++){moving_blade *b=&o->blade[j];int angle=(o->spin+j*64)&255,old=(angle-2-level)&255;
    b->x=o->x-18;b->y=o->tip+40;
    b->tip_x=b->x+ss_sine[(angle+64)&255]*38/32767;b->tip_y=b->y+ss_sine[angle]*38/32767;
    b->old_tip_x=b->x+ss_sine[(old+64)&255]*38/32767;b->old_tip_y=b->y+ss_sine[old]*38/32767;
   }
  }else{
   o->period=184-level*10;o->phase=(t+id*41)%o->period;
   int start=o->period-66;o->warning=o->phase>=start-18 && o->phase<start;
   o->effect=steam_height(o->phase,o->period);o->old_effect=steam_height((o->phase+o->period-1)%o->period,o->period);o->active=o->effect>0;
   o->jet_x=o->x-19;o->jet_y=o->tip+20;
  }
 }
 else if(o->type==OB_PAW){
  o->period=196-level*12;o->phase=(t+id*47)%o->period;o->width=19;
  int maximum=122+(int)((r>>11)%19),previous=(o->phase+o->period-1)%o->period;
  o->reach=paw_reach(o->phase,o->period,maximum);o->old_reach=paw_reach(previous,o->period,maximum);
  o->tip=o->side?232-o->reach:o->reach;o->old_tip=o->side?232-o->old_reach:o->old_reach;
  o->tip_x=o->x-o->reach*16/maximum;o->old_tip_x=o->x-o->old_reach*16/maximum;
  int attack=o->period-PAW_ATTACK_FRAMES;
  o->warning=o->phase>=attack-PAW_WARNING_FRAMES && o->phase<attack;
  o->active=o->phase>=attack && o->phase<attack+40;
 }else if(o->type==OB_TRAP){
  o->period=170-level*9;o->phase=(t+id*31)%o->period;o->width=26;
  o->warning=o->phase>=o->period-22;o->active=o->phase<14;
  o->tip=o->phase<6?218-o->phase*32/6:o->phase<14?186+(o->phase-6)*32/8:218;
 }else if(o->type==OB_POP){
  o->period=114-level*6;o->phase=(t+id*37)%o->period;o->width=20;
  o->warning=o->phase>=o->period-18;
  uint32_t launch_r=mix(r+(uint32_t)((t+id*37)/o->period)*733u);
  int duration=76-level*4,height=112+(int)((launch_r>>3)%25);
  for(int i=0;i<2;i++){
   toast_shot *s=&o->shot[i];s->age=o->phase-i*8;s->active=s->age>=0 && s->age<=duration+6;
   if(!s->active)continue;
   int drift=(i?1:-1)*(150+(int)((launch_r>>(i*5+7))%90)+level*14),slot=i?5:-5;
   toast_position(o->x,s->age,duration,height,drift,slot,&s->x,&s->y);
   toast_position(o->x,s->age>0?s->age-1:0,duration,height,drift,slot,&s->old_x,&s->old_y);
   if(s->y>248)s->active=0; /* Fall beyond the counter before the slice disappears. */
  }
 }
}
static int safe_height(const obstacle *o){
 switch(o->type){
 case OB_STOOL:return o->tip-36;
 case OB_APPLIANCE:return o->variant==1?o->tip-30:o->jet_y-72-28;
 case OB_TABLE:return o->side?o->tip+40:o->tip-30;
 case OB_SHELF:return 180;
 case OB_TRAP:return 146;
 case OB_PAW:return o->side?64:190;
 default:return 42; /* Fly above the toaster's complete visible arc. */
 }
}
void collectible_at(const world *w,int group,int index,int *x,int *y){
 *x=210+group*HAZARD_SPACING+index*21;obstacle o;obstacle_at(w,group,0,&o);
 int destination=safe_height(&o);*y=146+(destination-146)*(index+1)/5+ss_sine[(group*27+index*18)&255]*5/32767;
}
int collectible_taken(const hero *h,int group,int index){int bit=(group-h->collectible_group)*5+index;return bit<0?1:bit>=30?0:(h->collectible_bits>>bit)&1;}
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
int hazard_hits(const obstacle *o,int x0,int x1,int y0,int y1){
 if(o->type==OB_TABLE)return rectangle(o->x-o->width,o->x+o->width,o->tip,o->tip+13,x0,x1,y0,y1);
 if(o->type==OB_TRAP)return rectangle(o->x-26,o->x+26,219,232,x0,x1,y0,y1) ||
  rectangle(o->x-24,o->x+24,o->tip,o->tip+3,x0,x1,y0,y1) ||
  rectangle(o->x-24,o->x-15,o->tip,219,x0,x1,y0,y1) || rectangle(o->x+15,o->x+24,o->tip,219,x0,x1,y0,y1);
 if(o->type==OB_STOOL)return rectangle(o->x-24,o->x+24,o->tip,o->tip+(o->variant==2?38:13),x0,x1,y0,y1) ||
  rectangle(o->x-25,o->x-12,o->tip+8,232,x0,x1,y0,y1) || rectangle(o->x+12,o->x+25,o->tip+8,232,x0,x1,y0,y1) ||
  rectangle(o->x-18,o->x+18,206,210,x0,x1,y0,y1);
 if(o->type==OB_APPLIANCE){
  if(rectangle(o->x-o->width,o->x+o->width,o->tip,232,x0,x1,y0,y1))return 1;
  if(o->variant!=1){int height=o->effect>o->old_effect?o->effect:o->old_effect;return height>0 && rectangle(o->jet_x-10,o->jet_x+10,o->jet_y-height-6,o->jet_y,x0,x1,y0,y1);}
  /* Sample the rotating blades and mouse together, rather than hitting their empty bounding boxes. */
  for(int j=0;j<4;j++)for(int k=0;k<=4;k++){
   const moving_blade *b=&o->blade[j];int px=x0+(x1-x0)*k/4,py=y0+(y1-y0)*k/4;
   int tx=b->old_tip_x+(b->tip_x-b->old_tip_x)*k/4,ty=b->old_tip_y+(b->tip_y-b->old_tip_y)*k/4;
   if(near_line(px,py,b->x,b->y,tx,ty,HERO_RADIUS_X+5))return 1;
  }
  return 0;
 }
 if(o->type==OB_SHELF)return rectangle(o->x-o->width,o->x+o->width,o->tip-26,o->tip+10,x0,x1,y0,y1);
 if(o->type==OB_PAW){
  if(o->warning)return 0; /* Tentative movements signal danger, never cause a hit. */
  if(!o->reach && !o->old_reach)return 0;
  for(int k=0;k<=4;k++){
   int reach=o->old_reach+(o->reach-o->old_reach)*k/4;if(!reach)continue;
   int tx=o->old_tip_x+(o->tip_x-o->old_tip_x)*k/4,tip=o->old_tip+(o->tip-o->old_tip)*k/4,cy=o->side?tip+13:tip-13;
   int px=x0+(x1-x0)*k/4,py=y0+(y1-y0)*k/4,edge=o->side?232:0;
   int64_t dx=px-tx,dy=py-cy,rx=19+HERO_RADIUS_X,ry=13+HERO_RADIUS_Y;
   if(dx*dx*ry*ry+dy*dy*rx*rx<=rx*rx*ry*ry)return 1;
   if(reach>13 && near_line(px,py,o->x,edge,tx,cy,12+HERO_RADIUS_X))return 1;
  }
  return 0;
 }
 if(rectangle(o->x-20,o->x+20,210,232,x0,x1,y0,y1))return 1;
 for(int i=0;i<2;i++){
  const toast_shot *s=&o->shot[i];if(!s->active)continue;
  int left=s->x<s->old_x?s->x:s->old_x,right=s->x>s->old_x?s->x:s->old_x;
  int top=s->y<s->old_y?s->y:s->old_y,bottom=s->y>s->old_y?s->y:s->old_y;
  if(rectangle(left-10,right+10,top-6,bottom+6,x0,x1,y0,y1))return 1;
 }
 return 0;
}
void world_init(world *w,int players,uint32_t seed){
 memset(w,0,sizeof *w);w->players=players<1?1:players>4?4:players;w->seed=seed;
 for(int p=0;p<w->players;p++){
  hero *h=&w->h[p];h->state=HS_READY;h->x=hero_start_x(w->players,p);h->wx=(int64_t)h->x*Q16_ONE;
  h->y=FLIGHT_BOTTOM*Q16_ONE;h->grounded=1;h->land_t=20;
 }
}
void world_step(world *w,const int held[4]){
 memset(w->events,0,sizeof w->events);
 if(!w->started){
  int start=0;for(int p=0;p<w->players;p++)start|=held[p];if(!start)return;
  w->started=1;for(int p=0;p<w->players;p++)w->h[p].state=HS_RUN;w->events[0]|=EV_START;return;
 }
 w->t++;int speed=course_speed((int)(w->travel>>16));w->travel+=speed;w->scroll=(int)(w->travel>>16);
 for(int p=0;p<w->players;p++){
  hero *h=&w->h[p];h->t++;h->land_t++;if(h->smooth_t>0)h->smooth_t--;
  if(h->steam_sound_t>0)h->steam_sound_t--;
  if(h->state==HS_RUN){
   int old_x=(int)(h->wx>>16),old_y=h->y>>16,was_grounded=h->grounded;
   if(held[p] && !h->held){h->launches++;if(!h->steam_sound_t){w->events[p]|=EV_JUMP;h->steam_sound_t=20;}}
   h->held=!!held[p];h->vx=speed;h->wx=(int64_t)hero_start_x(w->players,p)*Q16_ONE+w->travel;
   h->vy+=h->held?-18022:11796;if(h->vy<-229376)h->vy=-229376;if(h->vy>249036)h->vy=249036;
   h->y+=h->vy;h->grounded=0;
   if(h->y<=FLIGHT_TOP*Q16_ONE){h->y=FLIGHT_TOP*Q16_ONE;h->vy=0;}
   if(h->y>=FLIGHT_BOTTOM*Q16_ONE){h->y=FLIGHT_BOTTOM*Q16_ONE;h->vy=0;h->grounded=1;}
   if(h->grounded && !was_grounded){h->land_t=0;w->events[p]|=EV_LAND;}
   int x=(int)(h->wx>>16),y=h->y>>16,base=(x-HAZARD_FIRST)/HAZARD_SPACING;if(base<0)base=0;
   for(int id=base>0?base-1:0;id<=base+1;id++){
    obstacle o;obstacle_at(w,id,w->t,&o);
    if(hazard_hits(&o,old_x,x,old_y,y)){h->state=HS_HIT;h->t=0;h->held=0;w->events[p]|=EV_HIT;break;}
   }
   h->x=x-w->scroll;h->distance=(int)(w->travel>>16);
   int group=(x-210)/HAZARD_SPACING;if(group<0)group=0;
   if(group>h->collectible_group){int shift=(group-h->collectible_group)*5;h->collectible_bits=shift<32?h->collectible_bits>>shift:0;h->collectible_group=group;}
   if(h->state==HS_RUN)for(int g=group;g<=group+1;g++)for(int j=0;j<5;j++){
    int cx,cy;collectible_at(w,g,j,&cx,&cy);
    if(!collectible_taken(h,g,j) && abs(cx-x)<=13 && abs(cy-y)<=12){
     h->collectible_bits|=1u<<((g-h->collectible_group)*5+j);h->apples++;h->smooth_t=12;w->events[p]|=EV_APPLE;
    }
   }
   if(h->state==HS_RUN){obstacle next;obstacle_at(w,h->anchor,w->t,&next);
    int edge=next.type==OB_TABLE?next.width+12:next.type==OB_POP?100:44;
    if(x>next.x+edge){h->anchor++;w->events[p]|=EV_SCORE;}
   }
   h->score=h->distance/12+h->apples*5;
  }else if(h->state==HS_HIT || h->state==HS_DOWN){
   h->vy+=11796;h->y+=h->vy;if(h->y>FLIGHT_BOTTOM*Q16_ONE){h->y=FLIGHT_BOTTOM*Q16_ONE;h->vy=0;}
   if(h->t>=20)h->state=HS_DOWN;
   if(h->t>=40)h->state=HS_OFF;
  }
 }
}
int world_running(const world *w){for(int p=0;p<w->players;p++)if(w->h[p].state==HS_RUN || w->h[p].state==HS_READY)return 1;return 0;}
int world_all_down(const world *w){for(int p=0;p<w->players;p++)if(w->h[p].state!=HS_OFF)return 0;return 1;}
int bot_decide(const world *w,int p){
 if(!w->started)return 1;
 const hero *h=&w->h[p];if(h->state!=HS_RUN)return 0;
 int x=(int)(h->wx>>16),id=x<HAZARD_FIRST?0:(x-HAZARD_FIRST)/HAZARD_SPACING;
 obstacle o;obstacle_at(w,id,w->t,&o);
 if(x>o.x+(o.type==OB_POP?100:o.type==OB_TABLE?o.width+12:44))id++;
 int speed=course_speed(h->distance),ticks=((HAZARD_FIRST+id*HAZARD_SPACING-x)*Q16_ONE)/speed;
 if(ticks<0)ticks=0;
 obstacle_at(w,id,w->t+ticks,&o);int target=safe_height(&o);
 int forecast=h->y+h->vy*9;return forecast>target*Q16_ONE;
}
