/* Spider Swing: greenhouse, web threads and independent split-screen cameras.
 * MIT, (c) 2026 Pierre-Louis Boyer (8BCraft). */
#include "game.h"
#include "assets.h"
#include "sine.h"
#include "house_ui.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
typedef struct view { int cx,cy,mx,my; rs_color sky[240]; } view;
static view V[4];
static rs_viewport VP[4];
static int nviews,shown_state=-1,shown_best=-1;
static const rs_color title_ramps[2][3]={
 {RS_RGB8(237,194,255),RS_RGB8(182,122,226),RS_RGB8(115,63,158)},
 {RS_RGB8(255,234,153),RS_RGB8(232,182,93),RS_RGB8(155,100,64)}};
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
 static const rs_color greens[9]={0,RS_HEX(0x345951),RS_HEX(0xfff6dc),RS_HEX(0x4d8064),RS_HEX(0xc3dc98),RS_HEX(0x83b475),RS_HEX(0xdde9b0),RS_HEX(0xa9c8af),RS_HEX(0xd4edf0)};
 rs_pal_load(RS_PAL_BG(PAL_GROUND),greens,9);
 rs_obj_base(VR_OBJ);rs_tiles_load(VR_OBJ,gm_obj_tiles,gm_obj_tile_count);
 rs_pal_load(RS_PAL_OBJ(0),gm_obj_pals,48);rs_pal_load(RS_PAL_OBJ(4),gm_obj_pals+64,32);
 rs_pal_load(RS_PAL_OBJ(6),gm_flower_pals,32);
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
 if(h->mode==MODE_FLY) return h->vy<0?SPR_HERO_STRETCH:SPR_HERO_AIR;
 return SPR_HERO_IDLE+(t/8)%2;
}
static void blossom(const world *w,int anchor,int frame,int x,int y,int small,const rs_viewport *vp){
 static const int normal[FLOWER_TYPES]={SPR_FLOWER,SPR_FLOWER_BELL,SPR_FLOWER_STAR,SPR_FLOWER_ROSETTE,SPR_FLOWER_ORCHID};
 static const int compact[FLOWER_TYPES]={SPR_FLOWER_SMALL,SPR_FLOWER_BELL_SMALL,SPR_FLOWER_STAR_SMALL,SPR_FLOWER_ROSETTE_SMALL,SPR_FLOWER_ORCHID_SMALL};
 int kind=flower_kind(w,anchor),color=flower_color(w,anchor),size=small?16:24;
 spr((small?compact[kind]:normal[kind])+frame,x-size/2,y-size/2,1,color?5+color:2,vp);
}
static void background(const view *v,rs_viewport *vp) {
 int xoff=v->cx*zoom_x(vp)/256,yoff=v->cy*zoom_y(vp)/256,fx=floor8(xoff),fy=floor8(yoff);
 int dx=xoff-fx*8,dy=yoff-fy*8;
 int cols=(vp->w+dx+7)/8,rows=(vp->h+dy+7)/8;
 for(int r=0;r<rows;r++) for(int c=0;c<cols;c++) {
  int yy=(fy+r)%HILLS_H;if(yy<0) yy+=HILLS_H;
  rs_bg_put(RS_BG3,v->mx+c,v->my+r,gm_hills_map[yy*64+((fx+c)&63)]);
 }
 vp->sx[2]=(int16_t)(v->mx*8+dx);vp->sy[2]=(int16_t)(v->my*8+dy);
}
static void pixel(uint8_t *p,int x,int y,int color,const rs_viewport *vp) {
 if(x>=0 && x<vp->w && y>=0 && y<vp->h) p[y*320+x]=(uint8_t)color;
}
static void stroke(uint8_t *p,int ax,int ay,int bx,int by,int color,int width,const rs_viewport *vp){
 int dx=abs(bx-ax),xx=ax<bx?1:-1,dy=-abs(by-ay),yy=ay<by?1:-1,err=dx+dy;
 for(int k=0;k<1000;k++){
  for(int i=-width;i<=width;i++)pixel(p,ax+i,ay,color,vp);
  if(ax==bx && ay==by)break;
  int e=err*2;if(e>=dy){err+=dy;ax+=xx;}if(e<=dx){err+=dx;ay+=yy;}
 }
}
static void webline(uint8_t *p,int ax,int ay,int bx,int by,const rs_viewport *vp){stroke(p,ax+1,ay+1,bx+1,by+1,1,0,vp);stroke(p,ax,ay,bx,by,2,0,vp);}
static void leaf(uint8_t *pixels,int x,int y,int side,int scale,const rs_viewport *vp){
 int length=scale?7:12,height=scale?3:5;
 for(int t=0;t<length;t++){int thick=height*(length-t)*t*4/(length*length);int xx=x+side*t,yy=y-t/2;
  for(int k=-thick;k<=thick;k++)pixel(pixels,xx,yy+k,k<0?4:3,vp);
  pixel(pixels,xx,yy,5,vp);
 }
}
static void branch(uint8_t *pixels,int ax,int ay,int bx,int by,int small,const rs_viewport *vp){
 int px=ax,py=ay,side=bx>ax?1:-1;
 for(int k=1;k<=32;k++){int inv=32-k,x=(inv*inv*ax+2*inv*k*(ax+side*12)+k*k*bx)/1024,y=(inv*inv*ay+2*inv*k*(by-25)+k*k*by)/1024;
  stroke(pixels,px+1,py+1,x+1,y+1,1,small?0:1,vp);stroke(pixels,px,py,x,y,5,small?0:1,vp);stroke(pixels,px-1,py,x-1,y,4,0,vp);
  if(k==12 || k==23){leaf(pixels,x,y,-side,small,vp);leaf(pixels,x,y+3,side,small,vp);}px=x;py=y;
 }
}
static void web(const world *w,int player,const view *v,const rs_viewport *vp,int *next,int state){
 uint8_t pixels[320*240]={0};const hero *h=&w->h[player];int small=vp->h<200;
 if(state==ST_TITLE || state==ST_OVER){
  branch(pixels,-14,60,89,102,0,vp);branch(pixels,334,60,231,102,0,vp);
 }else{
  /* The foreground stems share the flowers' world coordinates, so their connections never slide apart. */
  for(int side=0;side<2;side++){
   int previous_x=0,previous_y=0;
   int top=floor8(v->cy-48)*8,bottom=v->cy+vp->h*256/zoom_y(vp)+48;
   for(int yy=top;yy<=bottom;yy+=4){int xx=(side?294:26)+ss_sine[(yy+side*91)&255]*7/32768,x=sx(v,vp,xx),y=sy(v,vp,yy);
    if(yy!=top){stroke(pixels,previous_x+1,previous_y,x+1,y,1,small?1:2,vp);stroke(pixels,previous_x,previous_y,x,y,5,small?0:1,vp);stroke(pixels,previous_x-1,previous_y,x-1,y,4,0,vp);}
    if(yy%40==0){leaf(pixels,x,y,side?-1:1,small,vp);leaf(pixels,x,y+5,side?1:-1,small,vp);}
    previous_x=x;previous_y=y;
   }
  }
  for(int id=h->anchor-2;id<=h->anchor+6;id++)if(id>=0){int ax,ay;anchor_pos(w,id,&ax,&ay);int side=ax<160?0:1,stem_y=ay-52,stem_x=(side?294:26)+ss_sine[(stem_y+side*91)&255]*7/32768;
   branch(pixels,sx(v,vp,stem_x),sy(v,vp,stem_y),sx(v,vp,ax),sy(v,vp,ay)-(small?7:11),small,vp);
  }
 }
 if(h->state==HS_RUN){int ax,ay;anchor_pos(w,h->anchor,&ax,&ay);int x=sx(v,vp,(int)(h->wx/Q16_ONE)),y=sy(v,vp,h->y/Q16_ONE);
  if(h->mode==MODE_SWING || h->mode==MODE_CATCH)webline(pixels,sx(v,vp,ax),sy(v,vp,ay),x,y,vp);
  else {int bx,by,tx,ty;silk_segment(h,&bx,&by,&tx,&ty);webline(pixels,sx(v,vp,tx),sy(v,vp,ty),x,y,vp);}
  if(h->mode==MODE_FLY)for(int k=1;k<=3;k++){int xx=x-h->vx*k*3/Q16_ONE*zoom_x(vp)/256,yy=y-h->vy*k*3/Q16_ONE*zoom_y(vp)/256;pixel(pixels,xx,yy,8,vp);}
 }
 int cols=(vp->w+7)/8,rows=(vp->h+7)/8;
 for(int r=0;r<rows;r++)for(int c=0;c<cols;c++){
  uint8_t tile[64];int ink=0;
  for(int y=0;y<8;y++)for(int x=0;x<8;x++){int xx=c*8+x,yy=r*8+y;tile[y*8+x]=(uint8_t)(xx<vp->w && yy<vp->h?pixels[yy*320+xx]:0);ink|=tile[y*8+x];}
  int id=0;if(ink){id=(*next)++;rs_tiles_load8(VR_BG2+id,tile,1);}rs_bg_put(RS_BG2,v->mx+c,v->my+r,RS_MAP(id,PAL_GROUND,0,0,0));
 }
}
static void scene(const world *w,int p,int t,const view *v,const rs_viewport *vp){
 const hero *h=&w->h[p];int small=vp->h<200,size=small?16:32,x=sx(v,vp,(int)(h->wx/Q16_ONE)),y=sy(v,vp,h->y/Q16_ONE);
 spr(small?SPR_HERO_ICON:hero_frame(h,t),x-size/2,y-size/2,2,ppal(p),vp);
 if(h->smooth_t>0){int ax,ay;anchor_pos(w,h->anchor,&ax,&ay);hu_sparkle(sx(v,vp,ax)-4,sy(v,vp,ay)-4,w->t,2);}
 for(int a=h->anchor-2;a<=h->anchor+6;a++)if(a>=0){
  int ax,ay;anchor_pos(w,a,&ax,&ay);int frame=a==h->anchor && h->wait>flower_life(a)*2/3;
  int fs=small?16:24;
  blossom(w,a,frame,sx(v,vp,ax),sy(v,vp,ay),small,vp);
  if(a==h->anchor+1)hu_sparkle(sx(v,vp,ax)+fs/2+2,sy(v,vp,ay)-fs/2,w->t,2);
  if(a>=h->anchor && h->apple_id<a){dew_pos(w,a,&ax,&ay);spr(SPR_DEW,sx(v,vp,ax)-4,sy(v,vp,ay)-4+hu_bob(w->t+a,80,1),1,-1,vp);}
  if(a>=12 && a%5==3){bee_pos(w,a,w->t,&ax,&ay);spr((small?SPR_BEE_SMALL:SPR_BEE)+(w->t/5)%2,sx(v,vp,ax)-(small?8:12),sy(v,vp,ay)-(small?4:8),2,-1,vp);}
 }
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
 nviews=rs_viewport_layout(multi?w->players:1,0,VP);
 hu_clear();rs_oam_clear();rs_brightness(paused?9:15);
 int next=1;
 for(int p=0;p<nviews;p++) {
  view *v=&V[p];rs_viewport *vp=&VP[p];const hero *h=&w->h[p];
  v->mx=vp->x?32:0;v->my=vp->y?16:0;
  int ax,ay;anchor_pos(w,h->anchor+1,&ax,&ay);
  int cx=((int)(h->wx>>16)+ax)/2-vp->w*256/zoom_x(vp)/2;
  int cy=((h->y>>16)+ay)/2-vp->h*256/zoom_y(vp)*55/100;
  if(state==ST_TITLE || state==ST_OVER) cx=cy=0;
  if(fresh) {v->cx=cx;v->cy=cy;}
  else if(!paused) {v->cx+=(cx-v->cx)/10;v->cy+=(cy-v->cy)/10;}
  if((state==ST_PLAY || state==ST_DEAD) && !paused) {
   int x=sx(v,vp,(int)(h->wx>>16)),y=sy(v,vp,h->y>>16),top=multi?52:72;
   if(x<20) v->cx=(int)(h->wx>>16)-20*256/zoom_x(vp);
   if(x>vp->w-20) v->cx=(int)(h->wx>>16)-(vp->w-20)*256/zoom_x(vp);
   if(y<top) v->cy=(h->y>>16)-top*256/zoom_y(vp);
   if(y>vp->h-20) v->cy=(h->y>>16)-(vp->h-20)*256/zoom_y(vp);
  }
  for(int y=0;y<vp->h;y++) {
   rs_color top=RS_HEX(0x80bbaa),bottom=RS_HEX(0xf2e8bb);
   v->sky[y]=hu_lerp_color(top,bottom,y*256/vp->h);
  }
  background(v,vp);web(w,p,v,vp,&next,state);
  vp->sx[0]=vp->sx[1]=(int16_t)(v->mx*8);vp->sy[0]=vp->sy[1]=(int16_t)(v->my*8);
  vp->layers=7;vp->objs=1;
  hu_view_oam_begin(vp);
  if(state==ST_PLAY || state==ST_DEAD) {
   scene(w,p,st_t,v,vp);
   if(!multi) hu_number(h->score,vp->w/2,8,3);
   char text[40];int left=flower_life(h->anchor)-h->wait;if(left<0)left=0;
   if(multi) {
    hu_panel(v->mx,v->my,(vp->w+7)/8,4);
    snprintf(text,sizeof text,"P%d %d  D%d",p+1,h->score,h->apples);hu_box_text(v->mx+1,v->my+1,text);
    if(h->state!=HS_RUN) snprintf(text,sizeof text,"OUT");
    else if(h->mode==MODE_FLY) snprintf(text,sizeof text,"SILK TOUCH: CATCH");
    else if(h->mode==MODE_CATCH) snprintf(text,sizeof text,"CAUGHT!");
    else snprintf(text,sizeof text,"A: RELEASE %dS",(left+59)/60);
    hu_box_text(v->mx+1,v->my+2,text);
   } else {
    snprintf(text,sizeof text,"%s",h->mode==MODE_FLY?"STEER SILK INTO A FLOWER":h->mode==MODE_CATCH?"CAUGHT!":"A: RELEASE ON THE UPSWING");
    hu_text(hu_center(text,0),4,text);
    snprintf(text,sizeof text,"DEW %d - FLOWER %dS",h->apples,(left+59)/60);hu_text(hu_center(text,0),6,text);
   }
   if(paused) hu_big(v->mx+(vp->w/8-12)/2,v->my+vp->h/16,"PAUSED",HU_BIG_FREE);
  }
  hu_view_oam_end(vp);
 }
 if(!multi) {
  rs_viewport *vp=&VP[0];hu_standing rank;ranking(w,&rank);
  int slide=state==ST_OVER?hu_slide_in(st_t,20,200):0;
  if(state==ST_TITLE) {
   hu_logo("SPIDER SWING",title_ramps,2,2,0);hu_title_draw(st_t,best);
   hu_text(6,16,"A: RELEASE - L/R: AIR STEER");hu_title_sprites(st_t,icon,NULL);
   spr(SPR_HERO_IDLE+(st_t/24)%2,144,92+hu_bob(st_t,64,4),2,0,vp);
   blossom(w,0,0,89,113,0,vp);blossom(w,1,0,231,113,0,vp);
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
 } else rs_viewports(nviews,VP,RS_HEX(0x284d42));
 shown_state=state;shown_best=best;
}
#define S(v) rs_state_var("draw." #v,&(v),sizeof(v))
void draw_state(void) { S(V);S(VP);S(nviews);S(shown_state);S(shown_best);RS_STATE_RASTER(raster); }
#undef S
