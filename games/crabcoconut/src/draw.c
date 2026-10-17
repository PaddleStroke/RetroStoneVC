/* One full-screen beach court with shared shells and individual coconuts.
 * Widescreen (DESIGN.md "Widescreen"): a screen wider than 320 centres the 320-px court (rs_screen_origin) and shows
 * more of the beach on both sides: the sea, sandbanks and palm islands go on (BG3: a 128-tile map of the 1024-px
 * beach, its own tiles), the sand too, with a beach umbrella and a towel on the left, a sandcastle and a bucket on
 * the right (theme.inc theme_decor). The walls stay where they are: the coconuts never leave the court. */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct view {int mx,my,w,h,small,scroll;rs_color sky[240];} view;
static view V[4];static rs_viewport VP[4];static int nviews,shown_state=-1,shown_best=-1;
static int spr_dx;                     /* the sprites' x offset: -the viewport's x in the multiplayer frames (they are
                                        * relative to its top-left), 0 otherwise (set before each use) */
static int laid_w;                     /* the width the maps were laid out for (a state of another width: relayout) */
/* the sand layer's pixels, one view at a time (foreground(): per-frame scratch, state_audit.txt); its rows are as wide
 * as the widest screen */
#define PIX_W RS_SCREEN_W_MAX
static uint8_t pixels[PIX_W*240];
static int wide(void){return rs_screen_width()>RS_SCREEN_W;}
static int ppal(int p){return p?hu_player_pal(p):0;}
static void spr(int id,int x,int y,int priority,int pal,const rs_viewport *vp){
 const gm_sprite_def *s=&gm_spr[id];if(x<=vp->x-(int)s->w || x>=vp->x+vp->w || y<=-(int)s->h || y>=vp->h)return;
 rs_spr(x+spr_dx,y,s->tile,s->w,s->h,pal<0?s->pal:pal,priority,0);
}
static void pixel(uint8_t *p,int x,int y,int color,const rs_viewport *vp){if(x>=0 && x<vp->w && y>=0 && y<vp->h)p[y*PIX_W+x]=(uint8_t)color;}
static void rect(uint8_t *p,int x0,int y0,int x1,int y1,int color,const rs_viewport *vp){
 if(x0<0)x0=0;
 if(y0<0)y0=0;
 if(x1>=vp->w)x1=vp->w-1;
 if(y1>=vp->h)y1=vp->h-1;
 for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++)p[y*PIX_W+x]=(uint8_t)color;
}
#include "theme.inc"
/* A tall screen (rs_screen_tall: a phone held upright; DESIGN.md "Tall screen"): the court's 240 lines in the middle,
 * drawn on the whole screen (no viewport); the lines past them are the raster's. Above: the sky carried on, deeper
 * toward the top; below: the sand, BG2's last 16 lines (the grains and the side walls) repeated. The UI, the beach and
 * the sprites are hidden there (window 1 over the whole width); line 0 puts the stage's settings back. */
static int tall(void){return rs_screen_height()>RS_SCREEN_H;}
static int bg2_sx(void){return (V[0].mx*8-VP[0].x)&(rs_bg_map_w(RS_BG2)*8-1);}
static void raster_out(int line){
 int top=rs_screen_top(),bot=rs_screen_bottom();
 if(line==top || line==RS_SCREEN_H){
  rs_window(0,rs_screen_left(),rs_screen_right());
  for(int l=0;l<3;l++)rs_bg_window(l,l==RS_BG2 && line>0?0:RS_WIN1);
  rs_obj_window(RS_WIN1);
 }
 if(line<0){int k=top<0?-line*256/-top:0;rs_pal_set(0,hu_lerp_color(V[0].sky[0],RS_RGB8(104,170,214),k));return;}
 int d=line-RS_SCREEN_H,n=bot-RS_SCREEN_H;
 rs_pal_set(0,hu_lerp_color(theme_pal[2],theme_pal[3],n>0?d*96/n:0));
 rs_bg_scroll(RS_BG2,bg2_sx(),RS_SCREEN_H-16+(d&15)-line);
}
static void raster(int line,void *user){(void)user;
 if(line<0 || line>=RS_SCREEN_H){raster_out(line);return;}
 if(line==0 && tall()){for(int l=0;l<3;l++)rs_bg_window(l,0);rs_obj_window(0);rs_bg_scroll(RS_BG2,bg2_sx(),0);}
 int p=rs_viewport_current();if(p<0 || p>=nviews)p=0;int y=line-VP[p].y;if(y>=0 && y<240)rs_pal_set(0,V[p].sky[y]);}
/* the maps of this width: 64 tiles at 320; 128 wider (the UI's corner tags, the sand as wide as the screen, the wide
 * beach and its own tiles, the blank tile after them) */
static void maps_setup(void){
 int mw=wide()?128:64,n=wide()?gm_hills_wide_tile_count:gm_bg3_tile_count;
 rs_bg_setup(RS_BG1,mw,32,VR_BG1);rs_bg_setup(RS_BG2,mw,32,VR_BG2);rs_bg_setup(RS_BG3,mw,32,VR_BG3);
 rs_tiles_load(VR_BG3,wide()?gm_hills_wide_tiles:gm_bg3_tiles,n);uint8_t blank[64]={0};rs_tiles_load8(VR_BG2,blank,1);rs_tiles_load8(VR_BG3+n,blank,1);
 rs_pal_load(RS_PAL_BG(PAL_HILLS),wide()?gm_hills_wide_pal:gm_hills_pal,16);
 laid_w=rs_screen_width();
}
void draw_init(void){
 rs_screen_origin(rs_screen_margin());   /* widescreen: the 320-px court centred, the beach wider */
 hu_config c=hu_defaults();c.obj_tile=KIT_OBJ_TILE;c.obj_vram=VR_OBJ;c.obj_pal=3;hu_init(&c);
 maps_setup();rs_pal_load(RS_PAL_BG(PAL_GROUND),theme_pal,16);
 rs_obj_base(VR_OBJ);rs_tiles_load(VR_OBJ,gm_obj_tiles,gm_obj_tile_count);rs_pal_load(RS_PAL_OBJ(0),gm_obj_pals,48);rs_pal_load(RS_PAL_OBJ(4),gm_obj_pals+64,32);
 for(int l=0;l<3;l++)rs_bg_enable(l,1);
 memset(V,0,sizeof V);memset(VP,0,sizeof VP);nviews=0;rs_raster(raster,NULL);
}
/* After a state was loaded (rs_game.state_loaded): one saved at another width brings that width's maps; set this
 * width's up (each frame draws the whole screen again). Same width: nothing. */
void draw_relayout(void){if(laid_w!=rs_screen_width())maps_setup();}
static void background(const world *w,const view *v,rs_viewport *vp){
 if(wide()){   /* the whole wide beach, in stage coordinates (stage x = picture x & 1023): the view shows its part */
  for(int r=0;r<32;r++)for(int c=0;c<128;c++)
   rs_bg_put(RS_BG3,c,r,r<HILLS_H?gm_hills_wide_map[r*128+c]:RS_MAP(gm_hills_wide_tile_count,PAL_HILLS,0,0,0));
  vp->sx[2]=(int16_t)(vp->x&1023);vp->sy[2]=0;return;
 }
 int cols=(vp->w+7)/8,rows=(vp->h+7)/8,offset=v->scroll/64;
 int start=BG_START(vp);
 for(int r=0;r<rows;r++)for(int c=0;c<cols;c++){
  int yy=(r*8-start)/8;if(r*8<start)yy=-1;
  uint16_t tile=yy<0 || yy>=HILLS_H?RS_MAP(gm_bg3_tile_count,PAL_HILLS,0,0,0):gm_hills_map[yy*64+((c+offset)&63)];
  rs_bg_put(RS_BG3,v->mx+c,v->my+r,tile);
 }vp->sx[2]=(int16_t)(v->mx*8);vp->sy[2]=(int16_t)(v->my*8);
}
static void foreground(const world *w,int p,const view *v,const rs_viewport *vp,int title,int *next){
 memset(pixels,0,sizeof pixels);theme_field(w,p,v,vp,title,pixels);
 int cols=(vp->w+7)/8,rows=(vp->h+7)/8;
 for(int r=0;r<rows;r++)for(int c=0;c<cols;c++){
  uint8_t tile[64];int ink=0;
  for(int y=0;y<8;y++)for(int x=0;x<8;x++){int xx=c*8+x,yy=r*8+y;tile[y*8+x]=(uint8_t)(xx<vp->w && yy<vp->h?pixels[yy*PIX_W+xx]:0);ink|=tile[y*8+x];}
  int id=0;if(ink){id=(*next)++;rs_tiles_load8(VR_BG2+id,tile,1);}rs_bg_put(RS_BG2,v->mx+c,v->my+r,RS_MAP(id,PAL_GROUND,0,0,0));
 }
}
static void icon(int p,int cx,int cy,int t,void *user){(void)user;rs_viewport vp={0};vp.w=320;vp.h=240;spr(SPR_HERO_ICON,cx-8,cy-8+hu_bob(t+p*16,64,1),2,ppal(p),&vp);}
void draw_frame(const world *w,int state,int st_t,int best,int new_best,int paused){
 int multi=w->players>1 && (state==ST_PLAY || state==ST_DEAD);nviews=rs_viewport_layout(1,0,VP);
 if(tall()){VP[0].y=0;VP[0].h=RS_SCREEN_H;}   /* a tall screen: the view is the court's 240 lines (raster_out) */
 hu_clear();rs_oam_clear();rs_brightness(paused?9:15);int next=1;
 for(int p=0;p<nviews;p++){
  view *v=&V[p];rs_viewport *vp=&VP[p];const hero *h=&w->h[p];
  /* a view's own map area (the views right of or below the first); widescreen: the full view starts left of 0 */
  v->mx=vp->x>0?32:0;v->my=vp->y>0?16:0;v->w=vp->w;v->h=vp->h;v->small=vp->h<200;v->scroll=theme_scroll(h);
  for(int y=0;y<vp->h;y++)v->sky[y]=theme_sky(w,y,vp->h);
  background(w,v,vp);foreground(w,p,v,vp,state==ST_TITLE || state==ST_OVER,&next);
  /* the UI layer in stage coordinates (widescreen: the view's left edge is left of 0, round the map) */
  vp->sx[0]=(int16_t)((v->mx*8+vp->x)&(rs_bg_map_w(RS_BG1)*8-1));vp->sx[1]=(int16_t)(v->mx*8);vp->sy[0]=vp->sy[1]=(int16_t)(v->my*8);vp->layers=7;vp->objs=1;hu_view_oam_begin(vp);
  spr_dx=multi?-vp->x:0;
  if(state==ST_PLAY || state==ST_DEAD){
   theme_scene(w,p,st_t,v,vp);char text[128];theme_hud(h,text,multi?(size_t)(vp->w/8-1):sizeof text);
   if(multi){for(int player=0;player<w->players;player++){char score[48],tag[16];rs_strf(tag,sizeof tag,HTR(PLAYER_TAG),player+1);snprintf(score,sizeof score,"%s %d",tag,w->h[player].score);hu_text(player*10,1,score);}}
   else{hu_number(h->score,vp->x+vp->w/2,8,3);}
   if(paused)hu_big(v->mx+((2*vp->x+vp->w)/8-hu_width(HTR(PAUSED),1))/2,v->my+vp->h/16,HTR(PAUSED),HU_BIG_FREE);
  }else if(state==ST_OVER)theme_shells(w,vp);
  hu_view_oam_end(vp);
 }
 spr_dx=0;
 if(!multi){
  rs_viewport *vp=&VP[0];int scores[4];for(int p=0;p<w->players;p++)scores[p]=w->h[p].score;hu_standing rank;hu_rank(&rank,w->players,scores,scores);
  int slide=state==ST_OVER?hu_slide_in(st_t,20,200):0;
  if(state==ST_TITLE){hu_logo(TITLE,title_ramps,2,2,0);hu_title_draw(st_t,best);hu_text(hu_center(TR(HINT),0),16,TR(HINT));hu_title_sprites(st_t,icon,NULL);theme_title(st_t,vp);}
  if(state==ST_OVER){
   if(w->players==1){static const int medals[4]=MEDAL_SCORES;hu_banner(4,HTR(GAME_OVER));hu_gameover_panel(1,new_best,w->h[0].score,0);hu_gameover_sprites(1,w->h[0].score,0,best,hu_medal_of(w->h[0].score,medals),st_t,slide);hu_retry_line(st_t,RETRY_LOCK,HTR(BACK_TITLE));}
   else{hu_results_panel(&rank,NULL);hu_results_sprites(&rank,st_t,slide);for(int i=0;i<rank.n;i++){int x,y;hu_results_icon_pos(&rank,i,&x,&y);spr(SPR_HERO_ICON,x-8,y-8+slide,3,ppal(rank.order[i]),vp);}hu_retry_line_at(hu_results_retry_row(&rank),st_t,RETRY_LOCK,HTR(BACK_TITLE));}
  }
  /* the global scrolls: the sand layer's view column 0 at the view's left edge, the beach in stage coordinates */
  rs_bg_scroll(RS_BG1,0,-slide);rs_bg_scroll(RS_BG2,(V[0].mx*8-vp->x)&(rs_bg_map_w(RS_BG2)*8-1),0);
  rs_bg_scroll(RS_BG3,(vp->sx[2]-vp->x)&(rs_bg_map_w(RS_BG3)*8-1),vp->sy[2]);rs_viewports(0,NULL,0);
 }else if(tall()){   /* a tall screen (320 wide: the view at x 0, the same scrolls): drawn whole, for raster_out */
  rs_bg_scroll(RS_BG1,0,0);rs_bg_scroll(RS_BG2,bg2_sx(),0);
  rs_bg_scroll(RS_BG3,(VP[0].sx[2]-VP[0].x)&(rs_bg_map_w(RS_BG3)*8-1),VP[0].sy[2]);rs_viewports(0,NULL,0);
 }else rs_viewports(nviews,VP,RS_HEX(0x29364a));
 shown_state=state;shown_best=best;
}
#define S(v) rs_state_var("draw." #v,&(v),sizeof(v))
void draw_state(void){S(V);S(VP);S(nviews);S(shown_state);S(shown_best);S(laid_w);S(spr_dx);RS_STATE_RASTER(raster);}
#undef S
