/* House presentation, per-player views and saveable renderer state. */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* A view: its map block (mx, my), the size its garden is drawn for (w x h: the 4:3 view's), ox: that garden's left
 * edge in the view (widescreen: a wider view centres the 4:3 garden, the scenery goes on at its sides). */
typedef struct view {int mx,my,w,h,small,scroll,ox;rs_color sky[240];} view;
static view V[4];static rs_viewport VP[4];static int nviews,shown_state=-1,shown_best=-1;
static int ppal(int p){return p?hu_player_pal(p):0;}
/* widescreen (DESIGN.md "Widescreen"): the 320-px stage centred (draw_init), the hills drawn 1024 px wide around it
 * (hills-wide.png, a 128-tile map) and the maps 128 tiles wide; 320 is the 4:3 game pixel for pixel */
static int wide(void){return rs_screen_width()>RS_SCREEN_W;}
static int map_w(void){return wide()?128:64;}
static int fdiv8(int v){return v>=0?v/8:-((7-v)/8);}      /* floor(v / 8) */
static int sx(const view *v,int x){return v->ox+8+x*(v->w-16)/320;}
static int sy(const view *v,int y){return v->small?40+(y-64)*(v->h-52)/160:y;}
static void spr(int id,int x,int y,int priority,int pal,const rs_viewport *vp){
 const gm_sprite_def *s=&gm_spr[id];int lo=nviews>1?0:rs_screen_left(),hi=nviews>1?vp->w:rs_screen_right();
 if(x<=lo-(int)s->w || x>=hi || y<=-(int)s->h || y>=vp->h)return;
 rs_spr(x,y,s->tile,s->w,s->h,pal<0?s->pal:pal,priority,0);
}
static void pixel(uint8_t *p,int x,int y,int color,const rs_viewport *vp){if(x>=0 && x<vp->w && y>=0 && y<vp->h)p[y*vp->w+x]=(uint8_t)color;}
static void rect(uint8_t *p,int x0,int y0,int x1,int y1,int color,const rs_viewport *vp){
 if(x0<0)x0=0;
 if(y0<0)y0=0;
 if(x1>=vp->w)x1=vp->w-1;
 if(y1>=vp->h)y1=vp->h-1;
 for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++)p[y*vp->w+x]=(uint8_t)color;
}
#include "theme.inc"
static void raster(int line,void *user){(void)user;int p=rs_viewport_current();if(p<0 || p>=nviews)p=0;int y=line-VP[p].y;if(y>=0 && y<240)rs_pal_set(0,V[p].sky[y]);}
/* widescreen: the decor tiles after the hills' (and their blank), its palette and the wide hills map (rows past the
 * hills: the blank); again after a state of a 320 screen was loaded (draw_relayout) */
static void wide_setup(void){
 if(!wide())return;
 rs_tiles_load(VR_BG3+gm_bg3_tile_count+1,gm_bg3w_tiles,gm_bg3w_tile_count);rs_pal_load(RS_PAL_BG(PAL_WIDE),gm_wide_pal,16);
 for(int r=0;r<32;r++)for(int c=0;c<HILLSW_W;c++)rs_bg_put(RS_BG3,c,r,r<HILLS_H?gm_hillsw_map[r*HILLSW_W+c]:RS_MAP(gm_bg3_tile_count,PAL_HILLS,0,0,0));
}
void draw_init(void){
 rs_screen_origin(rs_screen_margin());   /* widescreen: the 320-px stage centred, more garden on both sides */
 hu_config c=hu_defaults();c.obj_tile=KIT_OBJ_TILE;c.obj_vram=VR_OBJ;c.obj_pal=3;hu_init(&c);
 rs_bg_setup(RS_BG1,map_w(),32,VR_BG1);rs_bg_setup(RS_BG2,map_w(),32,VR_BG2);rs_bg_setup(RS_BG3,map_w(),32,VR_BG3);
 rs_tiles_load(VR_BG3,gm_bg3_tiles,gm_bg3_tile_count);uint8_t blank[64]={0};rs_tiles_load8(VR_BG2,blank,1);rs_tiles_load8(VR_BG3+gm_bg3_tile_count,blank,1);
 rs_pal_load(RS_PAL_BG(PAL_HILLS),gm_hills_pal,16);rs_pal_load(RS_PAL_BG(PAL_GROUND),theme_pal,16);
 rs_obj_base(VR_OBJ);rs_tiles_load(VR_OBJ,gm_obj_tiles,gm_obj_tile_count);rs_pal_load(RS_PAL_OBJ(0),gm_obj_pals,48);rs_pal_load(RS_PAL_OBJ(4),gm_obj_pals+64,32);
 for(int l=0;l<3;l++)rs_bg_enable(l,1);
 memset(V,0,sizeof V);memset(VP,0,sizeof VP);nviews=0;rs_raster(raster,NULL);
 wide_setup();
}
/* After a state was loaded: the width is not in a state; one saved at another width brings that width's maps (64 or
 * 128 tiles wide) and, from a 320 screen, no wide hills. The views, their tiles and the UI are drawn every frame. */
void draw_relayout(void){
 if(rs_bg_map_w(RS_BG3)==map_w())return;
 rs_bg_setup(RS_BG1,map_w(),32,VR_BG1);rs_bg_setup(RS_BG2,map_w(),32,VR_BG2);rs_bg_setup(RS_BG3,map_w(),32,VR_BG3);
 wide_setup();
}
static void background(const world *w,const view *v,rs_viewport *vp){
 int cols=(vp->w+7)/8,rows=(vp->h+7)/8,offset=v->scroll/64;
 int start=BG_START(vp);
 for(int r=0;r<rows;r++)for(int c=0;c<cols;c++){
  int yy=(r*8-start)/8;if(r*8<start)yy=-1;
  uint16_t tile=yy<0 || yy>=HILLS_H?RS_MAP(gm_bg3_tile_count,PAL_HILLS,0,0,0):gm_hills_map[yy*64+((c+offset)&63)];
  rs_bg_put(RS_BG3,v->mx+c,v->my+r,tile);
 }vp->sx[2]=(int16_t)(v->mx*8);vp->sy[2]=(int16_t)(v->my*8);
}
/* widescreen: the views' identical tiles share one VRAM tile within a frame (the decor beside the stage takes tiles) */
static uint8_t fg_tile[1024][64];static uint16_t fg_slot[2048];
static int fg_find(const uint8_t *t,int *next){
 uint32_t hsh=2166136261u;for(int i=0;i<64;i++)hsh=(hsh^t[i])*16777619u;
 for(int s=(int)(hsh&2047);;s=(s+1)&2047){
  int id=fg_slot[s];if(id && !memcmp(fg_tile[id],t,64))return id;
  if(!id){id=(*next)++;memcpy(fg_tile[id],t,64);fg_slot[s]=(uint16_t)id;rs_tiles_load8(VR_BG2+id,t,1);return id;}
 }
}
static void foreground(const world *w,int p,const view *v,const rs_viewport *vp,int title,int *next){
 /* the view's pixels, vp->w per line (<= 320 x 240: a view wider than 320 is 119 lines high) */
 uint8_t pixels[320*240]={0};theme_field(w,p,v,vp,title,pixels);
 int cols=(vp->w+7)/8,rows=(vp->h+7)/8;
 for(int r=0;r<rows;r++)for(int c=0;c<cols;c++){
  uint8_t tile[64];int ink=0;
  for(int y=0;y<8;y++)for(int x=0;x<8;x++){int xx=c*8+x,yy=r*8+y;tile[y*8+x]=(uint8_t)(xx<vp->w && yy<vp->h?pixels[yy*vp->w+xx]:0);ink|=tile[y*8+x];}
  int id=0;if(ink && wide())id=fg_find(tile,next);else if(ink){id=(*next)++;rs_tiles_load8(VR_BG2+id,tile,1);}rs_bg_put(RS_BG2,v->mx+c,v->my+r,RS_MAP(id,PAL_GROUND,0,0,0));
 }
}
static void icon(int p,int cx,int cy,int t,void *user){(void)user;rs_viewport vp={0};vp.w=320;vp.h=240;spr(SPR_HERO_ICON,cx-8,cy-8+hu_bob(t+p*16,64,1),2,ppal(p),&vp);}
void draw_frame(const world *w,int state,int st_t,int best,int new_best,int paused){
 int multi=w->players>1 && (state==ST_PLAY || state==ST_DEAD);nviews=rs_viewport_layout(multi?w->players:1,0,VP);
 if(nviews==1){VP[0].x=0;VP[0].w=RS_SCREEN_W;}       /* one view: the 320-px stage (widescreen: centred) */
 hu_clear();rs_oam_clear();rs_brightness(paused?9:15);int next=1;
 if(wide()){rs_bg_fill(RS_BG2,0);memset(fg_slot,0,sizeof fg_slot);}
 /* widescreen, views side by side: the kit checks text in the screen's UI columns and writes it from column 0 on, so
  * each column of views shows its own block of UI columns (their texts never in another view's): the left views'
  * widest text from column 0, the right views' block just after it. PAUSED in 2x when it fits in 20 columns. */
 int wd=0,pbig=1;
 if(multi && wide()){
  for(int p=0;p<nviews;p++){char score[64],tag[16];rs_strf(tag,sizeof tag,HTR(PLAYER_TAG),p+1);snprintf(score,sizeof score,"%s %d",tag,w->h[p].score);if(hu_width(score,0)>wd)wd=hu_width(score,0);}
  if(paused){const char *s=HTR(PAUSED);pbig=hu_width(s,1)<=20;int pw=hu_width(s,pbig);if(pw>wd)wd=pw;}
 }
 for(int p=0;p<nviews;p++){
  view *v=&V[p];rs_viewport *vp=&VP[p];const hero *h=&w->h[p];
  /* the right-hand views use map columns 20..39: the kit's UI area (its text checks), the same pixels; widescreen:
   * columns 64.. of the 128-tile maps. A view draws the garden of its 4:3 size (320 or 159 px wide) centred. */
  int cw=nviews==1 || vp->w>=rs_screen_width()?RS_SCREEN_W:(RS_SCREEN_W-RS_VIEW_DIVIDER)/2;
  v->mx=nviews>1 && vp->x>rs_screen_left()?(wide()?64:20):0;v->my=vp->y?16:0;v->w=cw;v->ox=(vp->w-cw)/2;v->h=vp->h;v->small=vp->h<200;v->scroll=theme_scroll(h);
  for(int y=0;y<vp->h;y++)v->sky[y]=theme_sky(w,y,vp->h);
  if(!wide())background(w,v,vp);
  else{vp->sx[2]=(int16_t)(-v->ox&(HILLSW_W*8-1));vp->sy[2]=(int16_t)-BG_START(vp);}  /* the 4:3 hills' x 0 at the garden's */
  foreground(w,p,v,vp,state==ST_TITLE || state==ST_OVER,&next);
  int ux=v->mx;      /* BG1: the view's first column */
  if(multi && wide())ux=vp->w>=rs_screen_width()?fdiv8(vp->x):-((vp->w/8-wd)/2)+(vp->x>rs_screen_left()?(vp->w+7)/8:0);
  vp->sx[0]=(int16_t)(wide()?ux*8&(map_w()*8-1):ux*8);vp->sx[1]=(int16_t)(v->mx*8);vp->sy[0]=vp->sy[1]=(int16_t)(v->my*8);vp->layers=7;vp->objs=1;hu_view_oam_begin(vp);
  if(state==ST_PLAY || state==ST_DEAD){
   theme_scene(w,p,st_t,v,vp);
   if(multi){char score[64],tag[16];rs_strf(tag,sizeof tag,HTR(PLAYER_TAG),p+1);snprintf(score,sizeof score,"%s %d",tag,h->score);hu_text(ux+(vp->w/8-hu_width(score,0))/2,v->my+1,score);}
   else hu_number(h->score,vp->w/2,8,3);
   if(paused){const char *s=HTR(PAUSED);int cols=vp->w/8;  /* 2x, or 1x when 2x is wider than the view */
    if(pbig && hu_width(s,1)<=cols)hu_big(ux+(cols-hu_width(s,1))/2,v->my+vp->h/16,s,HU_BIG_FREE);else hu_text(ux+(cols-hu_width(s,0))/2,v->my+vp->h/16+1,s);}
  }hu_view_oam_end(vp);
 }
 if(!multi){
  rs_viewport *vp=&VP[0];int scores[4];for(int p=0;p<w->players;p++)scores[p]=w->h[p].score;hu_standing rank;hu_rank(&rank,w->players,scores,scores);
  int slide=state==ST_OVER?hu_slide_in(st_t,20,200):0;
  if(state==ST_TITLE){hu_logo(TITLE,title_ramps,2,2,0);hu_title_draw(st_t,best);hu_text(hu_center(TR(HINT),0),9,TR(HINT));hu_title_sprites(st_t,icon,NULL);theme_title(w,st_t,&V[0],vp);}
  if(state==ST_OVER){
   if(w->players==1){static const int medals[4]=MEDAL_SCORES;hu_banner(4,HTR(GAME_OVER));hu_gameover_panel(1,new_best,w->h[0].score,0);hu_gameover_sprites(1,w->h[0].score,0,best,hu_medal_of(w->h[0].score,medals),st_t,slide);hu_retry_line(st_t,RETRY_LOCK,HTR(BACK_TITLE));}
   else{hu_results_panel(&rank,NULL);hu_results_sprites(&rank,st_t,slide);for(int i=0;i<rank.n;i++){int x,y;hu_results_icon_pos(&rank,i,&x,&y);spr(SPR_HERO_ICON,x-8,y-8+slide,3,ppal(rank.order[i]),vp);}hu_retry_line_at(hu_results_retry_row(&rank),st_t,RETRY_LOCK,HTR(BACK_TITLE));}
  }rs_bg_scroll(RS_BG1,0,-slide);rs_bg_scroll(RS_BG2,0,0);rs_bg_scroll(RS_BG3,vp->sx[2],vp->sy[2]);rs_viewports(0,NULL,0);
 }else rs_viewports(nviews,VP,RS_HEX(0x29364a));
 shown_state=state;shown_best=best;
}
#define S(v) rs_state_var("draw." #v,&(v),sizeof(v))
void draw_state(void){S(V);S(VP);S(nviews);S(shown_state);S(shown_best);RS_STATE_RASTER(raster);}
#undef S
