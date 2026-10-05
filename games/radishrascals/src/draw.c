/* House presentation, per-player views and saveable renderer state. */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct view {int mx,my,w,h,small,scroll;rs_color sky[240];} view;
static view V[4];static rs_viewport VP[4];static int nviews,shown_state=-1,shown_best=-1;
static int ppal(int p){return p?hu_player_pal(p):0;}
static int sx(const view *v,int x){return 8+x*(v->w-16)/320;}
static int sy(const view *v,int y){return v->small?40+(y-64)*(v->h-52)/160:y;}
static void spr(int id,int x,int y,int priority,int pal,const rs_viewport *vp){
 const gm_sprite_def *s=&gm_spr[id];if(x<=-(int)s->w || x>=vp->w || y<=-(int)s->h || y>=vp->h)return;
 rs_spr(x,y,s->tile,s->w,s->h,pal<0?s->pal:pal,priority,0);
}
static void pixel(uint8_t *p,int x,int y,int color,const rs_viewport *vp){if(x>=0 && x<vp->w && y>=0 && y<vp->h)p[y*320+x]=(uint8_t)color;}
static void rect(uint8_t *p,int x0,int y0,int x1,int y1,int color,const rs_viewport *vp){
 if(x0<0)x0=0;
 if(y0<0)y0=0;
 if(x1>=vp->w)x1=vp->w-1;
 if(y1>=vp->h)y1=vp->h-1;
 for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++)p[y*320+x]=(uint8_t)color;
}
#include "theme.inc"
static void raster(int line,void *user){(void)user;int p=rs_viewport_current();if(p<0 || p>=nviews)p=0;int y=line-VP[p].y;if(y>=0 && y<240)rs_pal_set(0,V[p].sky[y]);}
void draw_init(void){
 hu_config c=hu_defaults();c.obj_tile=KIT_OBJ_TILE;c.obj_vram=VR_OBJ;c.obj_pal=3;hu_init(&c);
 rs_bg_setup(RS_BG1,64,32,VR_BG1);rs_bg_setup(RS_BG2,64,32,VR_BG2);rs_bg_setup(RS_BG3,64,32,VR_BG3);
 rs_tiles_load(VR_BG3,gm_bg3_tiles,gm_bg3_tile_count);uint8_t blank[64]={0};rs_tiles_load8(VR_BG2,blank,1);rs_tiles_load8(VR_BG3+gm_bg3_tile_count,blank,1);
 rs_pal_load(RS_PAL_BG(PAL_HILLS),gm_hills_pal,16);rs_pal_load(RS_PAL_BG(PAL_GROUND),theme_pal,16);
 rs_obj_base(VR_OBJ);rs_tiles_load(VR_OBJ,gm_obj_tiles,gm_obj_tile_count);rs_pal_load(RS_PAL_OBJ(0),gm_obj_pals,48);rs_pal_load(RS_PAL_OBJ(4),gm_obj_pals+64,32);
 for(int l=0;l<3;l++)rs_bg_enable(l,1);
 memset(V,0,sizeof V);memset(VP,0,sizeof VP);nviews=0;rs_raster(raster,NULL);
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
static void foreground(const world *w,int p,const view *v,const rs_viewport *vp,int title,int *next){
 uint8_t pixels[320*240]={0};theme_field(w,p,v,vp,title,pixels);
 int cols=(vp->w+7)/8,rows=(vp->h+7)/8;
 for(int r=0;r<rows;r++)for(int c=0;c<cols;c++){
  uint8_t tile[64];int ink=0;
  for(int y=0;y<8;y++)for(int x=0;x<8;x++){int xx=c*8+x,yy=r*8+y;tile[y*8+x]=(uint8_t)(xx<vp->w && yy<vp->h?pixels[yy*320+xx]:0);ink|=tile[y*8+x];}
  int id=0;if(ink){id=(*next)++;rs_tiles_load8(VR_BG2+id,tile,1);}rs_bg_put(RS_BG2,v->mx+c,v->my+r,RS_MAP(id,PAL_GROUND,0,0,0));
 }
}
static void icon(int p,int cx,int cy,int t,void *user){(void)user;rs_viewport vp={0};vp.w=320;vp.h=240;spr(SPR_HERO_ICON,cx-8,cy-8+hu_bob(t+p*16,64,1),2,ppal(p),&vp);}
void draw_frame(const world *w,int state,int st_t,int best,int new_best,int paused){
 int multi=w->players>1 && (state==ST_PLAY || state==ST_DEAD);nviews=rs_viewport_layout(multi?w->players:1,0,VP);
 hu_clear();rs_oam_clear();rs_brightness(paused?9:15);int next=1;
 for(int p=0;p<nviews;p++){
  view *v=&V[p];rs_viewport *vp=&VP[p];const hero *h=&w->h[p];
  /* the right-hand views use map columns 20..39: the kit's UI area (its text checks), the same pixels */
  v->mx=vp->x?20:0;v->my=vp->y?16:0;v->w=vp->w;v->h=vp->h;v->small=vp->h<200;v->scroll=theme_scroll(h);
  for(int y=0;y<vp->h;y++)v->sky[y]=theme_sky(w,y,vp->h);
  background(w,v,vp);foreground(w,p,v,vp,state==ST_TITLE || state==ST_OVER,&next);
  vp->sx[0]=vp->sx[1]=(int16_t)(v->mx*8);vp->sy[0]=vp->sy[1]=(int16_t)(v->my*8);vp->layers=7;vp->objs=1;hu_view_oam_begin(vp);
  if(state==ST_PLAY || state==ST_DEAD){
   theme_scene(w,p,st_t,v,vp);
   if(multi){char score[64],tag[16];rs_strf(tag,sizeof tag,HTR(PLAYER_TAG),p+1);snprintf(score,sizeof score,"%s %d",tag,h->score);hu_text(v->mx+(vp->w/8-hu_width(score,0))/2,v->my+1,score);}
   else hu_number(h->score,vp->w/2,8,3);
   if(paused){const char *s=HTR(PAUSED);int cols=vp->w/8;  /* 2x, or 1x when 2x is wider than the view */
    if(hu_width(s,1)<=cols)hu_big(v->mx+(cols-hu_width(s,1))/2,v->my+vp->h/16,s,HU_BIG_FREE);else hu_text(v->mx+(cols-hu_width(s,0))/2,v->my+vp->h/16+1,s);}
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
