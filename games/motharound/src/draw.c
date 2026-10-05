/* Moonlit garden, one lantern and shared rings for every player. */
#include "game.h"
#include "assets.h"
#include "sine.h"
#include "house_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct view {int mx,my,cx,cy,zoom;rs_color sky[240];} view;
static view V[4];static rs_viewport VP[4];
static int nviews,shown_state=-1,shown_best=-1;
static const rs_color title_ramps[2][3]={
 {RS_HEX(0xffe8a0),RS_HEX(0xf1b750),RS_HEX(0x99623c)},
 {RS_HEX(0xebc8fc),RS_HEX(0xbb83d0),RS_HEX(0x76509a)}};
static void raster(int line,void *user) {
 (void)user;int p=rs_viewport_current();if(p<0 || p>=nviews)p=0;
 int y=line-VP[p].y;if(y>=0 && y<240)rs_pal_set(0,V[p].sky[y]);
}
void draw_init(void) {
 hu_config cfg=hu_defaults();cfg.obj_tile=KIT_OBJ_TILE;cfg.obj_vram=VR_OBJ;cfg.obj_pal=3;hu_init(&cfg);
 rs_bg_setup(RS_BG1,64,32,VR_BG1);rs_bg_setup(RS_BG2,64,32,VR_BG2);rs_bg_setup(RS_BG3,64,32,VR_BG3);
 rs_tiles_load(VR_BG3,gm_bg3_tiles,gm_bg3_tile_count);
 uint8_t blank[64]={0};rs_tiles_load8(VR_BG2,blank,1);rs_tiles_load8(VR_BG3+gm_bg3_tile_count,blank,1);
 rs_pal_load(RS_PAL_BG(PAL_HILLS),gm_hills_pal,16);
 static const rs_color ring_pal[16]={0,RS_HEX(0x263c46),RS_HEX(0x527766),RS_HEX(0x91b47d),
  RS_HEX(0xc0d499),RS_HEX(0x51607c),RS_HEX(0x8598b0),RS_HEX(0xbfd0cf),RS_HEX(0xf4dd9b),
  RS_HEX(0xbca172),RS_HEX(0x455371),RS_HEX(0x403e58),RS_HEX(0x57546b),0,0,0};
 rs_pal_load(RS_PAL_BG(PAL_GROUND),ring_pal,16);
 rs_obj_base(VR_OBJ);rs_tiles_load(VR_OBJ,gm_obj_tiles,gm_obj_tile_count);
 rs_pal_load(RS_PAL_OBJ(0),gm_obj_pals,48);rs_pal_load(RS_PAL_OBJ(4),gm_obj_pals+64,32);
 for(int l=0;l<3;l++)rs_bg_enable(l,1);
 memset(V,0,sizeof V);memset(VP,0,sizeof VP);nviews=0;rs_raster(raster,NULL);
}
static int ppal(int p) {return p?hu_player_pal(p):0;}
static void spr(int id,int x,int y,int priority,int pal,const rs_viewport *vp) {
 const gm_sprite_def *s=&gm_spr[id];
 if(x<=-(int)s->w || x>=vp->w || y<=-(int)s->h || y>=vp->h)return;
 rs_spr(x,y,s->tile,s->w,s->h,pal<0?s->pal:pal,priority,0);
}
static int px(const view *v,int radius,int angle) {return v->cx+radius*v->zoom/256*ss_sine[(angle+64)&255]/32767;}
static int py(const view *v,int radius,int angle) {return v->cy+radius*v->zoom/256*ss_sine[angle&255]/32767;}
static int visible_radius(const world *w,int radius){
 // Compress only the approach outside the flight paths so every opening is visible.
 // The collision zone and the moths keep their actual, unscaled radius.
 int edge=orbit_radius(w->players,w->players-1)+MOTH_RADIUS;
 return radius<=edge?radius:edge+(radius-edge)*(102-edge)/(radius-edge+48);
}
static void pixel(uint8_t *p,int x,int y,int color,const rs_viewport *vp) {
 if(x>=0 && x<vp->w && y>=0 && y<vp->h)p[y*320+x]=(uint8_t)color;
}
static void line(uint8_t *p,int ax,int ay,int bx,int by,int color,const rs_viewport *vp) {
 int dx=abs(bx-ax),sx=ax<bx?1:-1,dy=-abs(by-ay),sy=ay<by?1:-1,err=dx+dy;
 for(int k=0;k<700;k++) {
  pixel(p,ax,ay,color,vp);if(ax==bx && ay==by)break;
  int e=err*2;if(e>=dy){err+=dy;ax+=sx;}if(e<=dx){err+=dx;ay+=sy;}
 }
}
static void background(const view *v,rs_viewport *vp) {
 int rows=(vp->h+7)/8,cols=(vp->w+7)/8;
 for(int r=0;r<rows;r++)for(int c=0;c<cols;c++) {
  int yy=(r*8-(vp->h-96))/8;
  if(r*8<vp->h-96)yy=-1;
  uint16_t tile=yy<0?RS_MAP(gm_bg3_tile_count,PAL_HILLS,0,0,0):gm_hills_map[(yy<HILLS_H?yy:HILLS_H-1)*64+(c&63)];
  rs_bg_put(RS_BG3,v->mx+c,v->my+r,tile);
 }
 vp->sx[2]=(int16_t)(v->mx*8);vp->sy[2]=(int16_t)(v->my*8);
}
static void rings(const world *w,const view *v,const rs_viewport *vp,int title,int *next) {
 uint8_t pixels[320*240]={0};
 // Subtle halo and dotted flight path around the lantern.
 int glow=28*v->zoom/256;
 for(int y=-glow;y<=glow;y++)for(int x=-glow;x<=glow;x++) {
  int d=x*x+y*y;
  if(d<(glow-3)*(glow-3) || (d<glow*glow && ((x+y)&1)==0))pixel(pixels,v->cx+x,v->cy+y,11,vp);
 }
 if(!title)for(int a=0;a<256;a+=8)
  pixel(pixels,px(v,ORBIT,a),py(v,ORBIT,a),10,vp);
 int nearest=ring_target(w);
 for(int i=0;i<MAX_RINGS;i++) {
  const ring *r=&w->rings[i];if(!r->active || title)continue;
  int radius=visible_radius(w,r->radius>>16);
  for(int a=0;a<256;a++) {
   if(ring_opening(r,a,0)>=0)continue;
   if(!r->kind) {
    for(int d=-4;d<=4;d++) {
     int color=d==4 || d==-4?1:d<-1?4:d<2?3:2;
     if((a%12==0 || a%12==1) && d>=-2 && d<=2)color=1;
     if(i!=nearest && color>2)color--;
     int rr=radius+d;
     line(pixels,px(v,rr,a),py(v,rr,a),px(v,rr,a+1),py(v,rr,a+1),color,vp);
    }
   } else {
    for(int d=-4;d<=4;d+=4){int rr=radius+d;
     line(pixels,px(v,rr,a),py(v,rr,a),px(v,rr,a+1),py(v,rr,a+1),i==nearest?7:6,vp);
    }
    if(a%8==0)line(pixels,px(v,radius-4,a),py(v,radius-4,a),px(v,radius+4,a+2),py(v,radius+4,a+2),5,vp);
   }
  }
  // Gold tips make the opening easy to read against the garden.
  for(int k=0;k<(r->openings?r->openings:1);k++)for(int sign=-1;sign<=1;sign+=2) {
   int width=r->openings?r->width[k]:r->gap;
   int a=ring_center(r,k,0)+sign*(width/2+1);
   // Keep tips legible while the distant ring is compressed near the screen edge.
   int tip=radius;
   line(pixels,px(v,tip-5,a),py(v,tip-5,a),px(v,tip+5,a),py(v,tip+5,a),i==nearest?8:9,vp);
  }
 }
 int cols=(vp->w+7)/8,rows=(vp->h+7)/8;
 for(int r=0;r<rows;r++)for(int c=0;c<cols;c++) {
  uint8_t tile[64];int ink=0;
  for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
   int xx=c*8+x,yy=r*8+y;tile[y*8+x]=(uint8_t)(xx<vp->w && yy<vp->h?pixels[yy*320+xx]:0);ink|=tile[y*8+x];
  }
  int id=0;if(ink){id=(*next)++;rs_tiles_load8(VR_BG2+id,tile,1);}
  rs_bg_put(RS_BG2,v->mx+c,v->my+r,RS_MAP(id,PAL_GROUND,0,0,0));
 }
}
static void scene(const world *w,int t,const view *v,const rs_viewport *vp) {
 for(int p=0;p<w->players;p++){
 const hero *h=&w->h[p];int radius=orbit_radius(w->players,p);
 int x=px(v,radius,h->angle),y=py(v,radius,h->angle);
 if(h->state!=HS_OFF) {
  int id=SPR_HERO_ICON+(t/5)%2;
  spr(id,x-8,y-8,2,ppal(p),vp);
  if(h->smooth_t>0)hu_sparkle(x+8,y-8,w->t,2);
 }
 }
 spr(SPR_LANTERN+(w->t/12)%2,v->cx-16,v->cy-24,2,-1,vp);
 // Stars emerge as dusk gives way to moonlight.
 if(w->t>600)for(int k=0;k<3;k++)
  spr(SPR_STAR,16+k*(vp->w-32)/3,40+(k%2)*12,0,-1,vp);
}
static void icon(int p,int cx,int cy,int t,void *user) {
 (void)user;rs_viewport vp={0};vp.w=320;vp.h=240;
 spr(SPR_HERO_ICON+(t/10)%2,cx-8,cy-8+hu_bob(t+p*16,64,1),2,ppal(p),&vp);
}
static void ranking(const world *w,hu_standing *rank) {
 int scores[4];for(int p=0;p<w->players;p++)scores[p]=w->h[p].score;
 hu_rank(rank,w->players,scores,scores);
}
static rs_color skycolor(int night,int y,int height) {
 static const int dusk_top[3]={100,88,120},dusk_bottom[3]={146,149,128};
 static const int night_top[3]={30,43,81},night_bottom[3]={65,87,114};
 int color[3];
 for(int c=0;c<3;c++) {
  int a=dusk_top[c]+(dusk_bottom[c]-dusk_top[c])*y/height;
  int b=night_top[c]+(night_bottom[c]-night_top[c])*y/height;
  color[c]=(a*(256-night)+b*night)/256+((y&4)?(c==1?2:4):0);
 }
 return RS_RGB8(color[0],color[1],color[2]);
}
void draw_frame(const world *w,int state,int st_t,int best,int new_best,int paused) {
 int multi=w->players>1 && (state==ST_PLAY || state==ST_DEAD);
 nviews=rs_viewport_layout(1,0,VP);
 hu_clear();rs_oam_clear();rs_brightness(paused?9:15);int next=1;
 for(int p=0;p<nviews;p++) {
  view *v=&V[p];rs_viewport *vp=&VP[p];const hero *h=&w->h[p];
  v->mx=vp->x?32:0;v->my=vp->y?16:0;v->cx=vp->w/2;
  v->cy=vp->h<200?75:LANTERN_Y;v->zoom=vp->h<200?128:vp->w<240?192:256;
  if(state==ST_TITLE){v->cx=182;v->cy=102;}
  int night=w->t*256/2400;if(night>256)night=256;
  rs_pal_set(RS_PAL_BG(PAL_GROUND)+11,hu_lerp_color(RS_HEX(0xa19882),RS_HEX(0x777261),night));
  for(int y=0;y<vp->h;y++)v->sky[y]=skycolor(night,y,vp->h);
  background(v,vp);rings(w,v,vp,state==ST_TITLE || state==ST_OVER,&next);
  vp->sx[0]=vp->sx[1]=(int16_t)(v->mx*8);vp->sy[0]=vp->sy[1]=(int16_t)(v->my*8);vp->layers=7;vp->objs=1;
  hu_view_oam_begin(vp);
  if(state==ST_PLAY || state==ST_DEAD) {
   scene(w,st_t,v,vp);char text[40];
   if(multi) {
    for(int player=0;player<w->players;player++){
     snprintf(text,sizeof text,"P%d %d",player+1,w->h[player].score);hu_text(player*10,1,text);
    }
   } else {
    hu_number(h->score,vp->w/2,8,3);
    snprintf(text,sizeof text,"RINGS %d - %dS",h->anchor,h->alive_t/60);hu_text(hu_center(text,0),4,text);
   }
   if(paused)hu_big(v->mx+(vp->w/8-12)/2,v->my+vp->h/16,"PAUSED",HU_BIG_FREE);
  }
  hu_view_oam_end(vp);
 }
 if(!multi) {
  rs_viewport *vp=&VP[0];hu_standing rank;ranking(w,&rank);
  int slide=state==ST_OVER?hu_slide_in(st_t,20,200):0;
  if(state==ST_TITLE) {
   hu_logo("MOTH AROUND",title_ramps,2,2,0);hu_title_draw(st_t,best);
   hu_text(4,16,"LEFT / RIGHT: DODGE THE RINGS");hu_title_sprites(st_t,icon,NULL);
   spr(SPR_HERO_IDLE+(st_t/8)%2,112,87+hu_bob(st_t,64,3),2,0,vp);
   spr(SPR_LANTERN+(st_t/12)%2,166,78,2,-1,vp);
   spr(SPR_LEAF,68,101,1,-1,vp);spr(SPR_LEAF+1,242,96,1,-1,vp);
   spr(SPR_MOON,278,77,0,-1,vp);
  }
  if(state==ST_OVER) {
   if(w->players==1) {
    static const int medals[4]=MEDAL_SCORES;
    hu_banner(4,"GAME OVER");hu_gameover_panel(1,new_best,w->h[0].score,0);
    hu_gameover_sprites(1,w->h[0].score,0,best,hu_medal_of(w->h[0].score,medals),st_t,slide);
    hu_retry_line(st_t,RETRY_LOCK,"A: TITLE");
   } else {
    hu_results_panel(&rank,NULL);hu_results_sprites(&rank,st_t,slide);
    for(int i=0;i<rank.n;i++){int x,y;hu_results_icon_pos(&rank,i,&x,&y);spr(SPR_HERO_ICON,x-8,y-8+slide,3,ppal(rank.order[i]),vp);}
    hu_retry_line_at(hu_results_retry_row(&rank),st_t,RETRY_LOCK,"A: TITLE");
   }
  }
  rs_bg_scroll(RS_BG1,0,-slide);rs_bg_scroll(RS_BG2,0,0);rs_bg_scroll(RS_BG3,vp->sx[2],vp->sy[2]);rs_viewports(0,NULL,0);
 } else rs_viewports(nviews,VP,RS_HEX(0x252b43));
 shown_state=state;shown_best=best;
}
#define S(v) rs_state_var("draw." #v,&(v),sizeof(v))
void draw_state(void) {S(V);S(VP);S(nviews);S(shown_state);S(shown_best);RS_STATE_RASTER(raster);}
#undef S
