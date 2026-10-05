/* Snow slope viewed from above: one shared course, planted flags and ski trails. */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "sine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct view {int scroll;rs_color sky[240];} view;
static view V;static int shown_state=-1,shown_best=-1;
#include "theme.inc"
static int ppal(int p){return p?hu_player_pal(p):0;}
static int course_y(int distance){return HERO_LINE+(distance-V.scroll)*3/5;}
static void raster(int line,void *user){(void)user;if(line>=0 && line<240)rs_pal_set(0,V.sky[line]);}
void draw_init(void){
 hu_config c=hu_defaults();c.obj_tile=KIT_OBJ_TILE;c.obj_vram=VR_OBJ;c.obj_pal=3;hu_init(&c);
 rs_bg_setup(RS_BG1,64,32,VR_BG1);rs_bg_setup(RS_BG2,64,32,VR_BG2);rs_bg_setup(RS_BG3,64,32,VR_BG3);
 uint8_t blank[64]={0};rs_tiles_load8(VR_BG2,blank,1);rs_tiles_load8(VR_BG3,blank,1);
 rs_pal_load(RS_PAL_BG(PAL_GROUND),theme_pal,16);
 rs_obj_base(VR_OBJ);rs_tiles_load(VR_OBJ,gm_obj_tiles,gm_obj_tile_count);
 rs_pal_load(RS_PAL_OBJ(0),gm_obj_pals,48);rs_pal_load(RS_PAL_OBJ(4),gm_obj_pals+64,32);
 rs_bg_enable(RS_BG1,1);rs_bg_enable(RS_BG2,1);rs_bg_enable(RS_BG3,0);
 memset(&V,0,sizeof V);rs_raster(raster,NULL);
}
static void spr(int id,int x,int y,int pal){
 const gm_sprite_def *s=&gm_spr[id];if(x<=-(int)s->w || x>=320 || y<=-(int)s->h || y>=240)return;
 rs_spr(x,y,s->tile,s->w,s->h,pal<0?s->pal:pal,2,0);
}
static void pixel(uint8_t *p,int x,int y,int color){if(x>=0 && x<320 && y>=0 && y<240)p[y*320+x]=(uint8_t)color;}
static void rect(uint8_t *p,int x0,int y0,int x1,int y1,int color){
 if(x0<0)x0=0;
 if(y0<0)y0=0;
 if(x1>319)x1=319;
 if(y1>239)y1=239;
 for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++)p[y*320+x]=(uint8_t)color;
}
static void line(uint8_t *p,int x0,int y0,int x1,int y1,int color){
 int n=abs(x1-x0)>abs(y1-y0)?abs(x1-x0):abs(y1-y0);if(!n)n=1;
 for(int i=0;i<=n;i++)pixel(p,x0+(x1-x0)*i/n,y0+(y1-y0)*i/n,color);
}
static void oval(uint8_t *p,int cx,int cy,int rx,int ry,int color){
 for(int y=-ry;y<=ry;y++)for(int x=-rx;x<=rx;x++)if(x*x*ry*ry+y*y*rx*rx<=rx*rx*ry*ry)pixel(p,cx+x,cy+y,color);
}
static void pine(uint8_t *p,int x,int y,int size){
 oval(p,x+3,y+4,size+4,4,3);rect(p,x-2,y-12,x+2,y,5);
 for(int tier=0;tier<3;tier++){
  int top=y-size*3+tier*size/2,height=size+8,width=size-4+tier*2;
  for(int dy=0;dy<height;dy++){
   int rx=1+width*dy/height;rect(p,x-rx,top+dy,x+rx,top+dy,5);
   rect(p,x-rx+1,top+dy,x,top+dy,7);rect(p,x+1,top+dy,x+rx-1,top+dy,6);
   if(dy>height-5 && x-rx<x)rect(p,x-rx+1,top+dy,x+rx-1,top+dy,dy<height-2?1:8);
  }
 }
}
static void shaded_oval(uint8_t *p,int x,int y,int rx,int ry,int light,int mid,int dark){
 oval(p,x,y,rx,ry,5);oval(p,x,y,rx-1,ry-1,dark);
 oval(p,x-1,y-1,rx-2,ry-2,mid);oval(p,x-3,y-ry/3,rx*2/3,ry/3,light);
}
static void polygon(uint8_t *p,int cx,int cy,const int *xy,int n,int color){
 int top=40,bottom=-40;for(int i=0;i<n;i++){if(xy[i*2+1]<top)top=xy[i*2+1];if(xy[i*2+1]>bottom)bottom=xy[i*2+1];}
 for(int y=top;y<=bottom;y++){
  int left=100,right=-100;
  for(int i=0;i<n;i++){int j=(i+1)%n,x0=xy[i*2],y0=xy[i*2+1],x1=xy[j*2],y1=xy[j*2+1];
   if(y< (y0<y1?y0:y1) || y>(y0>y1?y0:y1))continue;
   if(y0==y1){if(x0<left)left=x0;if(x1<left)left=x1;if(x0>right)right=x0;if(x1>right)right=x1;}
   else{int x=x0+(x1-x0)*(y-y0)/(y1-y0);if(x<left)left=x;if(x>right)right=x;}
  }
  if(left<=right)rect(p,cx+left,cy+y,cx+right,cy+y,color);
 }
}
static void obstacle(uint8_t *p,const slope_obstacle *o,int t){
 int x=o->x,y=course_y(o->distance);if(y<-70 || y>290)return;
 if(o->kind==PROP_ROCK){
  static const int edge[]={-17,2,-14,-8,-5,-16,7,-14,14,-8,17,5,12,10,-8,9};
  static const int face[]={-15,2,-12,-7,-4,-14,6,-12,12,-7,15,5,11,8,-7,7};
  static const int light[]={-12,-5,-4,-12,1,-8,-4,5,-14,2};
  static const int dark[]={5,-9,12,-6,15,5,9,7,5,1};
  static const int snow[]={-14,-8,-10,-13,-5,-17,6,-15,12,-10,14,-6,7,-6,4,-8,-2,-6,-7,-9};
  oval(p,x+3,y+10,21,4,3);polygon(p,x,y,edge,8,5);polygon(p,x,y,face,8,4);
  polygon(p,x,y,light,5,3);polygon(p,x,y,dark,5,5);line(p,x-3,y+4,x+1,y-2,5);
  polygon(p,x,y+2,snow,10,2);polygon(p,x,y,snow,10,1);
  rect(p,x-7,y-7,x-5,y-4,1);rect(p,x+7,y-7,x+9,y-4,2);
  oval(p,x-8,y+9,10,2,1);oval(p,x+11,y+9,8,2,2);
 }else if(o->kind==PROP_TREE){pine(p,x,y+15,13);
 }else if(o->kind==PROP_HOLE){
  oval(p,x+2,y+2,21,11,3);oval(p,x,y,20,10,8);oval(p,x,y-1,18,8,1);
  oval(p,x,y+1,17,7,5);oval(p,x,y+3,12,3,4);line(p,x-13,y-4,x-6,y-6,2);
  line(p,x+9,y+7,x+15,y+4,1);
 }else if(o->kind==PROP_GOAT){
  // A mountain goat trots sideways along one fixed contour of the slope.
  int dir=ss_sine[(t*2+o->id*31+64)&255]>=0?1:-1,kick=ss_sine[(t*12)&255]*2/32767;
  oval(p,x+2,y+12,21,4,3);
  for(int leg=0;leg<4;leg++){int xx=x-10+leg*6,dy=(leg&1)?kick:-kick;
   rect(p,xx,y+3,xx+2,y+11+dy,4);rect(p,xx-1,y+10+dy,xx+3,y+12+dy,5);
  }
  shaded_oval(p,x-2*dir,y,13,8,1,2,3);line(p,x-14*dir,y-3,x-18*dir,y-7,5);
  int head=x+12*dir;shaded_oval(p,head,y-4,7,6,1,2,3);oval(p,head+5*dir,y-1,4,3,3);
  line(p,head,y-9,head-3*dir,y-17,5);line(p,head-3*dir,y-17,head-9*dir,y-19,4);line(p,head-9*dir,y-19,head-11*dir,y-15,5);
  line(p,head+4*dir,y-8,head+2*dir,y-15,5);line(p,head+2*dir,y-15,head-4*dir,y-18,4);
  oval(p,head-5*dir,y-7,4,2,3);rect(p,head+2*dir,y-6,head+2*dir,y-5,5);pixel(p,head+7*dir,y-1,5);
  for(int dy=0;dy<4;dy++)line(p,head+3*dir,y+3+dy,head+3*dir+(dir>0?3-dy:dy-3),y+3+dy,5);
 }else{
  // A belly-sliding polar bear leaves its wake uphill, attached to its world position.
  for(int k=0;k<3;k++){int dy=21+k*8;line(p,x-8-k*3,y-dy,x-11-k*3,y-dy-5,2);line(p,x+8+k*3,y-dy,x+11+k*3,y-dy-5,2);}
  oval(p,x+2,y+13,24,4,3);int kick=ss_sine[(t*8)&255]*2/32767;
  shaded_oval(p,x-11,y-10+kick,6,5,15,1,2);shaded_oval(p,x+11,y-10-kick,6,5,15,1,2);
  shaded_oval(p,x,y-2,19,11,15,1,2);
  shaded_oval(p,x-19,y+5-kick,5,6,15,1,3);shaded_oval(p,x+19,y+5+kick,5,6,15,1,3);
  shaded_oval(p,x-9,y,4,4,1,2,3);shaded_oval(p,x+9,y,4,4,1,2,3);
  shaded_oval(p,x,y+7,12,10,15,1,2);oval(p,x,y+11,7,4,2);oval(p,x,y+9,3,2,5);
  rect(p,x-6,y+5,x-5,y+7,5);rect(p,x+5,y+5,x+6,y+7,5);pixel(p,x-6,y+5,15);pixel(p,x+5,y+5,15);
  line(p,x,y+11,x,y+14,5);line(p,x-3,y+15,x+3,y+15,5);
  for(int side=-1;side<=1;side+=2)for(int k=0;k<3;k++)pixel(p,x+side*(17+k),y+8+(side<0?-kick:kick),5);
 }
}
static void flag(uint8_t *p,int x,int y,int dir,int blue,int t){
 // The base and its shadow stay attached to the gate's physical crossing line.
 oval(p,x+4,y+3,9,3,3);oval(p,x,y,4,2,1);
 rect(p,x-2,y-25,x+2,y-1,5);rect(p,x-1,y-25,x,y-1,15);
 int light=blue?13:10,mid=blue?12:9,dark=blue?14:11;
 for(int k=1;k<=16;k++){
  int wave=ss_sine[(t*5+k*8)&255]*2/32767,xx=x+dir*k,top=y-23+wave;
  rect(p,xx,top,xx,top+10,k==16?5:mid);pixel(p,xx,top,light);pixel(p,xx,top+9,dark);
 }
}
static void foreground(const world *w){
 uint8_t pixels[320*240]={0};
 for(int y=0;y<240;y++){
  int bend=ss_sine[(y/2+V.scroll/12)&255]*2/32767;
  rect(pixels,0,y,10+bend,y,2);rect(pixels,309+bend,y,319,y,2);
  pixel(pixels,11+bend,y,1);pixel(pixels,308+bend,y,1);
 }
 // Rounded powder drifts travel with the gates, with no mountain horizon or ice glints.
 int first=(V.scroll-200)/70;if(first<0)first=0;
 for(int id=first;id<first+12;id++){
  int y=course_y(id*70),side=(id&1)?202:38,span=42+(id*17)%31;
  for(int x=0;x<span;x++){
   int yy=y+x*x/(span*6);pixel(pixels,side+x,yy,2);pixel(pixels,side+x,yy-1,1);
  }
 }
 int tree_first=(V.scroll-240)/135;if(tree_first<0)tree_first=0;
 for(int id=tree_first;id<tree_first+8;id++){
  int y=course_y(id*135),x=(id&1)?315:5;pine(pixels,x,y,10+id%3);
 }
 for(int p=0;p<w->players;p++){
  const hero *h=&w->h[p];
  for(int i=1;i<h->trail_count;i++){
   int a=(h->trail_head-h->trail_count+i-1+32)&31,b=(a+1)&31;
   int y0=course_y(h->trail_d[a]),y1=course_y(h->trail_d[b]);
   for(int side=-1;side<=1;side+=2)line(pixels,h->trail_x[a]+side*6,y0,h->trail_x[b]+side*6,y1,i<h->trail_count/3?2:3);
  }
  if(h->state!=HS_OFF)oval(pixels,h->x+2,player_y(w->players,p)+14,14,4,3);
 }
 int base=course_index(w,V.scroll);
 for(int id=base>0?base-1:0;id<=base+2;id++){
  slope_obstacle o;course_obstacle(w,id,V.scroll,w->t,&o);obstacle(pixels,&o,w->t);
 }
 for(int i=0;i<MAX_BEARS;i++)if(w->bears[i].active){slope_obstacle o;bear_obstacle(w,i,0,&o);obstacle(pixels,&o,w->t);}
 for(int id=base>0?base-1:0;id<=base+2;id++){
  row r;course_row(w,id,&r);int y=course_y(r.distance);if(y<-30 || y>275)continue;
  flag(pixels,r.left,y,1,r.kind,w->t);flag(pixels,r.right,y,-1,r.kind,w->t);
 }
 int next=1;
 for(int r=0;r<30;r++)for(int c=0;c<40;c++){
  uint8_t tile[64];int ink=0;for(int y=0;y<8;y++)for(int x=0;x<8;x++){tile[y*8+x]=pixels[(r*8+y)*320+c*8+x];ink|=tile[y*8+x];}
  int id=0;if(ink){id=next++;rs_tiles_load8(VR_BG2+id,tile,1);}rs_bg_put(RS_BG2,c,r,RS_MAP(id,PAL_GROUND,0,0,0));
 }
}
static void scene(const world *w){
 // Draw the downhill penguins first when their silhouettes overlap.
 for(int p=w->players-1;p>=0;p--){
  const hero *h=&w->h[p];if(h->state==HS_OFF)continue;int y=player_y(w->players,p);
  int frame=h->state==HS_HIT || h->state==HS_DOWN?SPR_HERO_HIT:SPR_HERO_IDLE+(w->t/8)%2;
  spr(frame,h->x-16,y-16,ppal(p));
  if(h->state==HS_RUN && abs(h->vx)>Q16_ONE/2){
   spr(SPR_SNOW+(w->t/5)%2,h->x-14,y-8,-1);spr(SPR_SNOW+(w->t/5+1)%2,h->x+8,y-11,-1);
  }
  if(h->smooth_t>0)hu_sparkle(h->x+17,y-7,w->t,2);
 }
 int base=course_index(w,V.scroll);
 for(int id=base>0?base-1:0;id<=base+2;id++){
  row r;course_row(w,id,&r);int visible=0;
  for(int p=0;p<w->players;p++)if(w->h[p].state==HS_READY || (w->h[p].state==HS_RUN && w->h[p].distance<r.distance))visible=1;
  if(visible)spr(SPR_FISH,r.center-8,course_y(r.distance)-8,-1);
 }
}
static void icon(int p,int cx,int cy,int t,void *user){(void)user;spr(SPR_HERO_ICON,cx-8,cy-8+hu_bob(t+p*16,64,1),ppal(p));}
static void title_ui(int t,int best){
 hu_logo(TITLE,title_ramps,2,1,0);hu_title_draw(t,best);
 hu_clear_rows(24,1);hu_clear_rows(26,1);hu_clear_rows(28,1);
 char text[48],who[24]="";int n=hu_players();
 if(best>0){snprintf(text,sizeof text,"BEST %d",best);hu_text(hu_center(text,0),22,text);}
 hu_text(hu_center(HINT,0),23,HINT);
 for(int p=n;p<4;p++){size_t k=strlen(who);snprintf(who+k,sizeof who-k,"%sP%d",p>n?" / ":"",p+1);}
 if(n<4)snprintf(text,sizeof text,n>1?"%s: A JOIN - B LEAVE":"%s: A TO JOIN",who);
 else snprintf(text,sizeof text,"4 PLAYERS - B TO LEAVE");
 hu_text(hu_center(text,0),27,text);hu_copyright(29);hu_title_sprites(t,icon,NULL);
}
void draw_frame(const world *w,int state,int st_t,int best,int new_best,int paused){
 hu_clear();rs_oam_clear();rs_brightness(paused?9:15);V.scroll=w->scroll;
 for(int y=0;y<240;y++){int d=(y&4)?2:0;V.sky[y]=RS_RGB8(232+y*8/240+d,241+y*5/240+d/2,238+y*4/240+d/2);}
 foreground(w);
 if(state==ST_TITLE){
  title_ui(st_t,best);
  // A live starting course replaces the detached decorative character and props.
  scene(w);
 }else if(state==ST_PLAY || state==ST_DEAD){
  if(w->players==1){
   hu_number(w->h[0].score,160,8,3);char text[40];
   if(w->h[0].state==HS_RUN)snprintf(text,sizeof text,"GATES %d - FISH %d",w->h[0].anchor,w->h[0].apples);
   else snprintf(text,sizeof text,"%s",w->h[0].reason==2?"CRASH!":"MISSED GATE");
   hu_text(hu_center(text,0),4,text);
  }else for(int p=0;p<w->players;p++){char text[24];snprintf(text,sizeof text,"P%d %d",p+1,w->h[p].score);hu_text(p*10,1,text);}
  if(paused)hu_big(14,13,"PAUSED",HU_BIG_FREE);
  scene(w);
 }else if(state==ST_OVER){
  int slide=hu_slide_in(st_t,20,200);
  if(w->players==1){static const int medals[4]=MEDAL_SCORES;hu_banner(4,"GAME OVER");hu_gameover_panel(1,new_best,w->h[0].score,0);
   hu_gameover_sprites(1,w->h[0].score,0,best,hu_medal_of(w->h[0].score,medals),st_t,slide);hu_retry_line(st_t,RETRY_LOCK,"A: TITLE");
  }else{
   int scores[4];for(int p=0;p<w->players;p++)scores[p]=w->h[p].score;hu_standing rank;hu_rank(&rank,w->players,scores,scores);
   hu_results_panel(&rank,NULL);hu_results_sprites(&rank,st_t,slide);
   for(int i=0;i<rank.n;i++){int x,y;hu_results_icon_pos(&rank,i,&x,&y);spr(SPR_HERO_ICON,x-8,y-8+slide,ppal(rank.order[i]));}
   hu_retry_line_at(hu_results_retry_row(&rank),st_t,RETRY_LOCK,"A: TITLE");
  }
  rs_bg_scroll(RS_BG1,0,-slide);
 }
 if(state!=ST_OVER)rs_bg_scroll(RS_BG1,0,0);
 rs_bg_scroll(RS_BG2,0,0);rs_viewports(0,NULL,0);shown_state=state;shown_best=best;
}
#define S(v) rs_state_var("draw." #v,&(v),sizeof(v))
void draw_state(void){S(V);S(shown_state);S(shown_best);RS_STATE_RASTER(raster);}
#undef S
