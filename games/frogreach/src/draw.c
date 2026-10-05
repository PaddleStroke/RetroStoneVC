/* Frog Reach: swamp, lily pads and tongue lines and independent split-screen cameras.
 * MIT, (c) 2026 Pierre-Louis Boyer (8BCraft). */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
typedef struct view { int cx,cy,mx,my; rs_color sky[240]; } view;
static view V[4];
static rs_viewport VP[4];
static int nviews,shown_state=-1,shown_best=-1;
static const rs_color title_ramps[2][3]={
 {RS_RGB8(216,241,125),RS_RGB8(135,201,80),RS_RGB8(65,124,69)},
 {RS_RGB8(255,213,194),RS_RGB8(232,138,156),RS_RGB8(150,79,116)}};
static void raster(int line,void *user) {
 (void)user; int p=rs_viewport_current(); if(p<0 || p>=nviews) p=0;
 int y=line-VP[p].y; if(y>=0 && y<240) rs_pal_set(0,V[p].sky[y]);
}
void draw_init(void) {
 hu_config cfg=hu_defaults();cfg.obj_tile=KIT_OBJ_TILE;cfg.obj_vram=VR_OBJ;cfg.obj_pal=3;hu_init(&cfg);
 rs_bg_setup(RS_BG1,64,32,VR_BG1);rs_bg_setup(RS_BG2,64,32,VR_BG2);rs_bg_setup(RS_BG3,64,32,VR_BG3);
 rs_tiles_load(VR_BG3,gm_bg3_tiles,gm_bg3_tile_count);
 uint8_t blank[64]={0};rs_tiles_load8(VR_BG2,blank,1);
 rs_tiles_load8(VR_BG3+gm_bg3_tile_count,blank,1);
 rs_pal_load(RS_PAL_BG(PAL_HILLS),gm_hills_pal,16);
 static const rs_color field_pal[16]={0,RS_HEX(0x994f74),RS_HEX(0xffbab5),RS_HEX(0xea7894),RS_HEX(0xd9ef7c),RS_HEX(0x87c84f),RS_HEX(0x4c8f46),RS_HEX(0x2a583f),RS_HEX(0x232a3a),RS_HEX(0x5d9ea6),RS_HEX(0x8ec0bc),RS_HEX(0x4f8f9c),RS_HEX(0x72afb0),RS_HEX(0x4a818c),RS_HEX(0xbbe6df),0};
 rs_pal_load(RS_PAL_BG(PAL_GROUND),field_pal,16);
 rs_obj_base(VR_OBJ);rs_tiles_load(VR_OBJ,gm_obj_tiles,gm_obj_tile_count);
 rs_pal_load(RS_PAL_OBJ(0),gm_obj_pals,48);rs_pal_load(RS_PAL_OBJ(4),gm_obj_pals+64,32);
 for(int l=0;l<3;l++) rs_bg_enable(l,1);
 memset(V,0,sizeof V);memset(VP,0,sizeof VP);nviews=0;
 rs_raster(raster,NULL);
}
static int floor8(int y) { return y>=0?y/8:(y-7)/8; }
static int ppal(int p) { return p?hu_player_pal(p):0; }
static int zoom_x(const rs_viewport *vp) { return vp->w<240?128:256; }
static int zoom_y(const rs_viewport *vp) { return vp->h<200?128:256; }
static int sx(const view *v,const rs_viewport *vp,int x) { return (x-v->cx)*zoom_x(vp)/256; }
static int sy(const view *v,const rs_viewport *vp,int y) { return (y-v->cy)*zoom_y(vp)/256; }
static void spr(int id,int x,int y,int prio,int pal,const rs_viewport *vp) {
 const gm_sprite_def *s=&gm_spr[id];
 if(x<=-(int)s->w || x>=vp->w || y<=-(int)s->h || y>=vp->h) return;
 rs_spr(x,y,s->tile,s->w,s->h,pal<0?s->pal:pal,prio,0);
}
static int hero_frame(const hero *h,int t) {
 if(h->state==HS_HIT || h->state==HS_DOWN) return SPR_HERO_HIT;
 if(h->land_t<5) return SPR_HERO_SQUASH;
 if(h->mode==MODE_LEAP) return h->vy<0?SPR_HERO_STRETCH:SPR_HERO_AIR;
 return SPR_HERO_IDLE+(t/8)%2;
}
static void background(const view *v,rs_viewport *vp) {
 int xoff=v->cx/4,fx=floor8(xoff),dx=xoff-fx*8;
 int cols=(vp->w+dx+7)/8,rows=(vp->h+7)/8;
 for(int r=0;r<rows;r++) for(int c=0;c<cols;c++) {
  int yy=floor8(r*8-(sy(v,vp,180)-64));
  uint16_t tile=yy<0?RS_MAP(gm_bg3_tile_count,PAL_HILLS,0,0,0):gm_hills_map[(yy<HILLS_H?yy:HILLS_H-1)*64+((fx+c)&63)];
  rs_bg_put(RS_BG3,v->mx+c,v->my+r,tile);
 }
 vp->sx[2]=(int16_t)(v->mx*8+dx);vp->sy[2]=(int16_t)(v->my*8);
}
static void pixel(uint8_t *p,int x,int y,int color,const rs_viewport *vp) {
 if(x>=0 && x<vp->w && y>=0 && y<vp->h) p[y*320+x]=(uint8_t)color;
}
static void line(uint8_t *p,int ax,int ay,int bx,int by,const rs_viewport *vp) {
 int dx=abs(bx-ax),sx=ax<bx?1:-1,dy=-abs(by-ay),sy=ay<by?1:-1,err=dx+dy;
 for(int k=0;k<800;k++) {
  pixel(p,ax,ay+1,1,vp);pixel(p,ax,ay,3,vp);pixel(p,ax,ay-1,2,vp);
  if(ax==bx && ay==by) break;
  int e=err*2;if(e>=dy){err+=dy;ax+=sx;}if(e<=dx){err+=dx;ay+=sy;}
 }
}
static void web(const world *w,int player,const view *v,const rs_viewport *vp,int *next) {
 uint8_t pixels[320*240]={0};const hero *h=&w->h[player];int small=vp->w<240,water=sy(v,vp,180);
 /* Every pad sits inside the pond, including the title's true initial scene. */
 for(int y=water;y<vp->h;y++)for(int x=0;x<vp->w;x++){
  int wx=x+v->cx/2;int color=9;
  if(y==water)color=10;
  else if((y+player*3)%11==0 && (wx+w->t/4+y*7)%79<19)color=12;
  else if(y%17==3 && (wx-w->t/7-y*3)%107<14)color=10;
  pixel(pixels,x,y,color,vp);
 }
 for(int id=h->anchor-1;id<=h->anchor+3;id++)if(id>=0){
  int x,width;perch_pos(w,id,w->t,&x,&width);int cx=sx(v,vp,x),cy=sy(v,vp,192)+3,rx=width*zoom_x(vp)/512,ry=small?3:width>=56?5:4;
  if(rx<5)rx=5;
  for(int yy=-ry-2;yy<=ry+2;yy++)for(int xx=-rx-5;xx<=rx+5;xx++){
   int outer=xx*xx*(ry+2)*(ry+2)+yy*yy*(rx+5)*(rx+5),limit=(rx+5)*(rx+5)*(ry+2)*(ry+2);
   if(outer<=limit && outer>limit*3/4)pixel(pixels,cx+xx,cy+yy+3,12,vp);
  }
  for(int yy=-ry-1;yy<=ry+1;yy++)for(int xx=-rx-1;xx<=rx+1;xx++){
   int inside=xx*xx*ry*ry+yy*yy*rx*rx;
   if(inside>(rx+1)*(rx+1)*(ry+1)*(ry+1))continue;
   int color=inside>rx*rx*ry*ry?8:yy<-1?4:yy<1?5:yy<3?6:7;
   if(xx>0 && abs(yy)*rx<xx*ry/2)color=9; /* The characteristic V-shaped lily-leaf notch. */
   else if(xx<0 && yy==xx*ry/rx/2)color=6;
   if(id==h->anchor && h->wait>pad_life(id)*2/3 && color>=4 && color<7)color++;
   pixel(pixels,cx+xx,cy+yy,color,vp);
  }
 }
 if(h->state==HS_RUN && (h->mode==MODE_CHARGE || h->mode==MODE_LEAP)) {
  int target=h->mode==MODE_LEAP?h->goal_x:(int)(h->wx>>16)+h->length;
  if(h->length*zoom_x(vp)/256>11 || h->mode==MODE_LEAP)
   line(pixels,sx(v,vp,(int)(h->wx>>16))+(small?5:10),sy(v,vp,h->y>>16)-(small?6:11),sx(v,vp,target),sy(v,vp,192)-(small?6:11),vp);
 }
 int cols=(vp->w+7)/8,rows=(vp->h+7)/8;
 for(int r=0;r<rows;r++) for(int c=0;c<cols;c++) {
  uint8_t tile[64];int ink=0;
  for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
   int xx=c*8+x,yy=r*8+y;
   tile[y*8+x]=(uint8_t)(xx<vp->w && yy<vp->h?pixels[yy*320+xx]:0);ink|=tile[y*8+x];
  }
  int id=0;if(ink){id=(*next)++;rs_tiles_load8(VR_BG2+id,tile,1);}
  rs_bg_put(RS_BG2,v->mx+c,v->my+r,RS_MAP(id,PAL_GROUND,0,0,0));
 }
}
static void scene(const world *w,int p,int t,const view *v,const rs_viewport *vp) {
 const hero *h=&w->h[p];int small=vp->w<240;
 int x=sx(v,vp,(int)(h->wx>>16)),y=sy(v,vp,h->y>>16);
 spr(small?SPR_HERO_ICON:hero_frame(h,w->t),x-(small?8:16),y-(small?15:30),2,ppal(p),vp);
 if(h->smooth_t>0)hu_sparkle(x-(small?10:20),y-(small?8:17),w->t,2);
 for(int a=h->anchor+1;a<=h->anchor+3;a++){
  int pad_x,width;perch_pos(w,a,w->t,&pad_x,&width);
  if(a==h->anchor+1 && h->mode==MODE_LEAP && h->perfect)continue;
  spr((small?SPR_FLY_SMALL:SPR_FLY)+(w->t/5)%2,sx(v,vp,pad_x)-(small?4:8),sy(v,vp,192)-(small?11:19)+hu_bob(w->t+a*11,64,1),1,-1,vp);
 }
 if(h->state==HS_RUN && h->mode==MODE_CHARGE && h->length*zoom_x(vp)/256>11)
  spr(SPR_TIP,sx(v,vp,(int)(h->wx>>16)+h->length)-4,sy(v,vp,192)-(small?10:15),2,-1,vp);
 if(h->state==HS_HIT && h->t<24)spr(SPR_SPLASH+(h->t/8)%3,x-12,sy(v,vp,192)-7,2,-1,vp);
}

static void icon(int p,int cx,int cy,int t,void *user) {
 (void)user;rs_viewport vp={0};vp.w=320;vp.h=240;
 spr(SPR_HERO_ICON,cx-8,cy-8+hu_bob(t+p*16,64,1),2,ppal(p),&vp);
}
static void ranking(const world *w,hu_standing *rank) {
 int scores[4];for(int p=0;p<w->players;p++) scores[p]=w->h[p].score;
 hu_rank(rank,w->players,scores,scores);
}
void draw_frame(const world *w,int state,int st_t,int best,int new_best,int paused) {
 int multi=w->players>1 && (state==ST_PLAY || state==ST_DEAD);
 int fresh=nviews!=(multi?w->players:1) || shown_state==ST_TITLE;
 nviews=rs_viewport_layout(multi?w->players:1,RS_LAYOUT_HSPLIT,VP);
 hu_clear();rs_oam_clear();rs_brightness(paused?9:15);
 int next=1;
 for(int p=0;p<nviews;p++) {
  view *v=&V[p];rs_viewport *vp=&VP[p];const hero *h=&w->h[p];
  v->mx=vp->x?32:0;v->my=vp->y?16:0;
  int cx=(int)(h->wx>>16)-(vp->w>=240?56:32)*256/zoom_x(vp);
  int cy=192-(vp->h>=200?196:vp->h-26)*256/zoom_y(vp);
  /* Title and gameplay use the same camera and the same world positions. */
  if(fresh || !paused){v->cx=cx;v->cy=cy;}
  for(int y=0;y<vp->h;y++) {
   rs_color top=RS_HEX(0x87b4cd),bottom=RS_HEX(0xe8e4b6);
   v->sky[y]=hu_lerp_color(top,bottom,y*256/vp->h);
  }
  background(v,vp);web(w,p,v,vp,&next);
  vp->sx[0]=vp->sx[1]=(int16_t)(v->mx*8);vp->sy[0]=vp->sy[1]=(int16_t)(v->my*8);
  vp->layers=7;vp->objs=1;
  hu_view_oam_begin(vp);
  if(state==ST_TITLE)scene(w,0,0,v,vp);
  if(state==ST_PLAY || state==ST_DEAD) {
   scene(w,p,st_t,v,vp);
   if(!multi) hu_number(h->score,vp->w/2,8,3);
   char text[40];
   if(multi) {
    snprintf(text,sizeof text,"P%d %d",p+1,h->score);
    hu_text(v->mx+1,v->my+1,text);
   } else {
    int left=pad_life(h->anchor)-h->wait;if(left<0)left=0;
    const char *message=h->mode==MODE_LEAP?(h->good?"NICE CATCH!":"OH NO! MISSED THE PAD"):
       h->mode==MODE_CHARGE?"RELEASE ABOVE THE LILY PAD":"HOLD TO REACH THE NEXT PAD";
    hu_text(hu_center(message,0),4,message);
    snprintf(text,sizeof text,"FLIES %d - PAD %dS",h->apples,(left+59)/60);hu_text(hu_center(text,0),6,text);
   }
   if(paused) hu_big(v->mx+(vp->w/8-12)/2,v->my+vp->h/16,"PAUSED",HU_BIG_FREE);
  }
  hu_view_oam_end(vp);
 }
 if(!multi) {
  rs_viewport *vp=&VP[0];hu_standing rank;ranking(w,&rank);
  int slide=state==ST_OVER?hu_slide_in(st_t,20,200):0;
  if(state==ST_TITLE) {
   hu_logo("FROG REACH",title_ramps,2,2,0);hu_title_draw(st_t,best);
   hu_text(3,16,"HOLD TO EXTEND. RELEASE TO LEAP.");hu_title_sprites(st_t,icon,NULL);

  }
  if(state==ST_OVER) {
   if(w->players==1) {
    static const int medals[4]=MEDAL_SCORES;
    hu_banner(4,"GAME OVER");hu_gameover_panel(1,new_best,w->h[0].score,0);
    hu_gameover_sprites(1,w->h[0].score,0,best,hu_medal_of(w->h[0].score,medals),st_t,slide);
    hu_retry_line(st_t,RETRY_LOCK,"A: TITLE");
   } else {
    hu_results_panel(&rank,NULL);hu_results_sprites(&rank,st_t,slide);
    for(int i=0;i<rank.n;i++) {int x,y;hu_results_icon_pos(&rank,i,&x,&y);spr(SPR_HERO_ICON,x-8,y-8+slide,3,ppal(rank.order[i]),vp);}
    hu_retry_line_at(hu_results_retry_row(&rank),st_t,RETRY_LOCK,"A: TITLE");
   }
  }
  rs_bg_scroll(RS_BG1,0,-slide);rs_bg_scroll(RS_BG2,0,0);rs_bg_scroll(RS_BG3,vp->sx[2],vp->sy[2]);
  rs_viewports(0,NULL,0);
 } else rs_viewports(nviews,VP,RS_HEX(0x254f62));
 shown_state=state;shown_best=best;
}
#define S(v) rs_state_var("draw." #v,&(v),sizeof(v))
void draw_state(void) { S(V);S(VP);S(nviews);S(shown_state);S(shown_best);RS_STATE_RASTER(raster); }
#undef S
