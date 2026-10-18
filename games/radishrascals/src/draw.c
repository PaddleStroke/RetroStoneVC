/* House presentation, per-player views and saveable renderer state. */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include "house_tall.h"   /* a phone held upright: the title and the panels in the columns shown */
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
/* a tall screen (portrait, zoomed: DESIGN.md "Tall screen"): BG3 128 tiles wide, the lawn's decor in its columns
 * DECOR_COL.. (tall_decor), for the lines below the stage */
static int tall(void){return rs_screen_height()>RS_SCREEN_H;}
static int bg3_w(void){return tall()?128:map_w();}
/* the width a view's garden is drawn for: the 4:3 view's (320 or 159 px); one view on a tall screen: GARDEN_TALL_W */
static int garden_w(int n,const rs_viewport *vp){
 return n==1 || vp->w>=rs_screen_width()?(tall() && n==1?GARDEN_TALL_W:RS_SCREEN_W):(RS_SCREEN_W-RS_VIEW_DIVIDER)/2;
}
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
/* The views of the stage's 240 lines: rs_viewport_layout's, mapped back from a tall screen (it spans the whole screen
 * there) to the 240-line rectangles of a 4:3 one: the raster draws the lines past the stage. */
static int layout(int n,rs_viewport *out){
 int k=rs_viewport_layout(n,0,out),top=rs_screen_top(),hh=(RS_SCREEN_H-RS_VIEW_DIVIDER)/2;
 if(rs_screen_height()>RS_SCREEN_H)for(int i=0;i<k;i++){
  int full=out[i].h>=rs_screen_height();out[i].y=(int16_t)(out[i].y==top?0:RS_SCREEN_H-hh);out[i].h=(int16_t)(full?RS_SCREEN_H:hh);
 }
 return k;
}
/* A tall screen's lines past the 240 of the stage (DESIGN.md "Tall screen"): above, the sky going on, a little bluer
 * toward the screen's top, and the sprites (the score); below, more lawn and its decor (BG3 columns DECOR_COL..: the
 * pots and the carrot patch, then the vegetable rows and the wheelbarrow of the wide garden, tall_decor; 2-4 players:
 * the lawn's colour, in a viewport of its own). The UI and the beds are hidden there (window 1); line 0 puts the
 * stage's settings back, so a 240-line screen is unchanged. */
#define DECOR_COL 40
#define DECOR_A_X 240        /* chunk A: the wide garden's x 240..591 in the map's x 320..671 */
#define DECOR_B_X 672        /* chunk B: its x 672..1023 at the same map x */
#define BAND_A 96            /* lines below the stage from chunk A (its y 144..239), then chunk B (its y 104..) */
static void raster_out(int line){
 int top=rs_screen_top();
 if(line==top || line==RS_SCREEN_H){
  rs_window(0,rs_screen_left(),rs_screen_right());rs_bg_window(RS_BG1,RS_WIN1);rs_bg_window(RS_BG2,RS_WIN1);
  rs_bg_window(RS_BG3,line<0?RS_WIN1:0);rs_obj_window(line<0?0:RS_WIN1);
 }
 if(line<0){rs_pal_set(0,hu_lerp_color(V[0].sky[0],RS_RGB8(192,222,222),-line*256/-top));return;}
 int d=line-RS_SCREEN_H;
 if(d<BAND_A)rs_bg_scroll(RS_BG3,DECOR_COL*8,(144+d-line)&255);
 else rs_bg_scroll(RS_BG3,750,(104+d-BAND_A-line)&255);
 rs_pal_set(0,RS_RGB8(179,199,130));
}
/* a tall screen: BG3 128 tiles wide, the wide garden's decor tiles and palette (as widescreen; draw_init, and
 * draw_relayout after a state of another size was loaded) */
static void tall_setup(void){
 if(!tall())return;
 rs_bg_setup(RS_BG3,bg3_w(),32,VR_BG3);
 rs_tiles_load(VR_BG3+gm_bg3_tile_count+1,gm_bg3w_tiles,gm_bg3w_tile_count);rs_pal_load(RS_PAL_BG(PAL_WIDE),gm_wide_pal,16);
}
/* ... and the decor's two chunks in BG3's columns DECOR_COL..127, every frame (beside the views' columns 0..39) */
static void tall_decor(void){
 for(int r=0;r<HILLS_H;r++)for(int c=DECOR_COL;c<128;c++){
  int hc=c<DECOR_B_X/8?c-DECOR_COL+DECOR_A_X/8:c;
  rs_bg_put(RS_BG3,c,r,gm_hillsw_map[r*HILLSW_W+hc]);
 }
}
static void raster(int line,void *user){
 (void)user;
 if(line<0 || line>=RS_SCREEN_H){raster_out(line);return;}
 if(line==0){rs_bg_window(RS_BG1,0);rs_bg_window(RS_BG2,0);rs_bg_window(RS_BG3,0);rs_obj_window(0);   /* back */
  if(nviews==1)rs_bg_scroll(RS_BG3,VP[0].sx[2],VP[0].sy[2]);}
 int p=rs_viewport_current();if(p<0 || p>=nviews)p=0;int y=line-VP[p].y;if(y>=0 && y<240)rs_pal_set(0,V[p].sky[y]);}
/* widescreen: the decor tiles after the hills' (and their blank), its palette and the wide hills map (rows past the
 * hills: the blank); again after a state of a 320 screen was loaded (draw_relayout) */
static void wide_setup(void){
 if(!wide())return;
 rs_tiles_load(VR_BG3+gm_bg3_tile_count+1,gm_bg3w_tiles,gm_bg3w_tile_count);rs_pal_load(RS_PAL_BG(PAL_WIDE),gm_wide_pal,16);
 for(int r=0;r<32;r++)for(int c=0;c<HILLSW_W;c++)rs_bg_put(RS_BG3,c,r,r<HILLS_H?gm_hillsw_map[r*HILLSW_W+c]:RS_MAP(gm_bg3_tile_count,PAL_HILLS,0,0,0));
}
void draw_init(void){
 rs_screen_origin(rs_screen_margin());   /* widescreen: the 320-px stage centred, more garden on both sides */
 rs_screen_origin_v(rs_screen_height()-RS_SCREEN_H>2*TALL_ABOVE?TALL_ABOVE:(rs_screen_height()-RS_SCREEN_H)/2);   /* tall: the sky and the score above, the lawn below */
 hu_config c=hu_defaults();c.obj_tile=KIT_OBJ_TILE;c.obj_vram=VR_OBJ;c.obj_pal=3;hu_init(&c);
 rs_bg_setup(RS_BG1,map_w(),32,VR_BG1);rs_bg_setup(RS_BG2,map_w(),32,VR_BG2);rs_bg_setup(RS_BG3,map_w(),32,VR_BG3);
 rs_tiles_load(VR_BG3,gm_bg3_tiles,gm_bg3_tile_count);uint8_t blank[64]={0};rs_tiles_load8(VR_BG2,blank,1);rs_tiles_load8(VR_BG3+gm_bg3_tile_count,blank,1);
 rs_pal_load(RS_PAL_BG(PAL_HILLS),gm_hills_pal,16);rs_pal_load(RS_PAL_BG(PAL_GROUND),theme_pal,16);
 rs_obj_base(VR_OBJ);rs_tiles_load(VR_OBJ,gm_obj_tiles,gm_obj_tile_count);rs_pal_load(RS_PAL_OBJ(0),gm_obj_pals,48);rs_pal_load(RS_PAL_OBJ(4),gm_obj_pals+64,32);
 for(int l=0;l<3;l++)rs_bg_enable(l,1);
 memset(V,0,sizeof V);memset(VP,0,sizeof VP);nviews=0;rs_raster(raster,NULL);
 wide_setup();tall_setup();
}
/* After a state was loaded: the size is not in a state; one saved at another size brings that size's maps (BG1 / BG3:
 * 64 / 64 tiles wide at 320, 128 / 128 wider, 64 / 128 on a tall screen) and, from a 320 or a tall screen, no wide
 * hills (a tall one: its decor in BG3). A state of this size: nothing to do (the same bytes). The views, their tiles
 * and the UI are drawn every frame. */
void draw_relayout(void){
 if(rs_bg_map_w(RS_BG3)==bg3_w() && rs_bg_map_w(RS_BG1)==map_w())return;
 rs_bg_setup(RS_BG1,map_w(),32,VR_BG1);rs_bg_setup(RS_BG2,map_w(),32,VR_BG2);rs_bg_setup(RS_BG3,map_w(),32,VR_BG3);
 wide_setup();tall_setup();
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
/* zoomed (ht_narrow): the logo as RADISH over RASCALS when it does not fit on one line, each word in its own ramp
 * (house_tall.h's ht_logo draws the second word as a title of its own, so its ramp takes the first word's palette
 * entries: here it is " RASCALS ", the kit's second word, centred). Returns the rows it takes. */
static int tall_logo(int ty){
 if((int)strlen(TITLE)*17-2<=ht_ncols()*8){hu_logo(TITLE,title_ramps,2,ty,0);return 6;}
 hu_config *c=(hu_config*)hu_cfg();int base=c->logo_tile;
 c->logo_tile=base+hu_logo("RADISH",title_ramps,2,ty,0);hu_logo(" RASCALS ",title_ramps,2,ty+6,0);c->logo_tile=base;
 return 12;
}
static void icon(int p,int cx,int cy,int t,void *user){(void)user;rs_viewport vp={0};vp.w=320;vp.h=240;spr(SPR_HERO_ICON,cx-8,cy-8+hu_bob(t+p*16,64,1),2,ppal(p),&vp);}
void draw_frame(const world *w,int state,int st_t,int best,int new_best,int paused){
 int multi=w->players>1 && (state==ST_PLAY || state==ST_DEAD);nviews=layout(multi?w->players:1,VP);
 if(nviews==1){VP[0].x=0;VP[0].w=RS_SCREEN_W;}       /* one view: the 320-px stage (widescreen: centred) */
 /* a tall screen (DESIGN.md "Tall screen"): one view stays zoomed (the play, the title, the panels: one look, the
  * UI laid out in the columns shown, house_tall.h); the split play (2-4 views) shows every column. Set before the UI
  * is drawn: the visible columns (ht_narrow) follow it. */
 rs_screen_fill(nviews==1 && (state==ST_PLAY || state==ST_DEAD || tall()));
 int narrow=nviews==1 && ht_narrow();
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
  int cw=garden_w(nviews,vp);   /* (a tall screen's one view: GARDEN_TALL_W, the beds in the columns shown) */
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
   else hu_number(h->score,vp->w/2,8+(tall()?rs_screen_top():0),3);   /* (a tall screen: at the screen's top) */
   if(paused){const char *s=HTR(PAUSED);int cols=vp->w/8;  /* 2x, or 1x when 2x is wider than the view */
    if(narrow)ht_big(v->my+vp->h/16,s,HU_BIG_FREE);   /* (zoomed: in the columns shown) */
    else if(pbig && hu_width(s,1)<=cols)hu_big(ux+(cols-hu_width(s,1))/2,v->my+vp->h/16,s,HU_BIG_FREE);else hu_text(ux+(cols-hu_width(s,0))/2,v->my+vp->h/16+1,s);}
  }hu_view_oam_end(vp);
 }
 if(!multi){
  rs_viewport *vp=&VP[0];int scores[4];for(int p=0;p<w->players;p++)scores[p]=w->h[p].score;hu_standing rank;hu_rank(&rank,w->players,scores,scores);
  int slide=state==ST_OVER?hu_slide_in(st_t,20,200):0;
  if(state==ST_TITLE && narrow){
   /* zoomed: RADISH over RASCALS, the hint wrapped under them, the slots, the prompt and the rest below */
   int r=ht_text(2+tall_logo(1),TR(HINT),0);
   ht_title tc={TR(VERB),MAX_PLAYERS,r+2,0};ht_title_out at;ht_title_draw(&tc,st_t,best,&at);ht_title_sprites(&tc,&at,st_t,icon,NULL);
   theme_title(w,st_t,&V[0],vp);
  }else if(state==ST_TITLE){hu_logo(TITLE,title_ramps,2,2,0);hu_title_draw(st_t,best);hu_text(hu_center(TR(HINT),0),9,TR(HINT));hu_title_sprites(st_t,icon,NULL);theme_title(w,st_t,&V[0],vp);}
  if(state==ST_OVER && narrow){   /* zoomed: the panels in the columns shown */
   if(w->players==1){static const int medals[4]=MEDAL_SCORES;ht_banner(4,HTR(GAME_OVER));ht_gameover_panel(new_best,NULL,HTR(BACK_TITLE));ht_gameover_sprites(w->h[0].score,best,hu_medal_of(w->h[0].score,medals),st_t,slide);ht_retry_line_at(ht_gameover_retry_row(NULL),st_t,RETRY_LOCK,HTR(BACK_TITLE));}
   else{ht_results_panel(&rank,NULL);ht_results_sprites(&rank,st_t,slide);for(int i=0;i<rank.n;i++){int x,y;ht_results_icon_pos(&rank,i,&x,&y);spr(SPR_HERO_ICON,x-8,y-8+slide,3,ppal(rank.order[i]),vp);}ht_retry_line_at(hu_results_retry_row(&rank),st_t,RETRY_LOCK,HTR(BACK_TITLE));}
  }else if(state==ST_OVER){
   if(w->players==1){static const int medals[4]=MEDAL_SCORES;hu_banner(4,HTR(GAME_OVER));hu_gameover_panel(1,new_best,w->h[0].score,0);hu_gameover_sprites(1,w->h[0].score,0,best,hu_medal_of(w->h[0].score,medals),st_t,slide);hu_retry_line(st_t,RETRY_LOCK,HTR(BACK_TITLE));}
   else{hu_results_panel(&rank,NULL);hu_results_sprites(&rank,st_t,slide);for(int i=0;i<rank.n;i++){int x,y;hu_results_icon_pos(&rank,i,&x,&y);spr(SPR_HERO_ICON,x-8,y-8+slide,3,ppal(rank.order[i]),vp);}hu_retry_line_at(hu_results_retry_row(&rank),st_t,RETRY_LOCK,HTR(BACK_TITLE));}
  }rs_bg_scroll(RS_BG1,0,-slide);rs_bg_scroll(RS_BG2,0,0);rs_bg_scroll(RS_BG3,vp->sx[2],vp->sy[2]);rs_viewports(0,NULL,0);
 }else if(rs_screen_height()>RS_SCREEN_H){
  /* a tall screen: three more viewports past the stage, no sprite: the raster's sky above (no layer), the lawn's
   * decor below (BG3: its two bands, as raster_out shows them with one view) */
  rs_viewport all[7];memcpy(all,VP,sizeof(rs_viewport)*nviews);int n=nviews,top=rs_screen_top(),bot=rs_screen_bottom();
  memset(&all[n],0,3*sizeof(rs_viewport));
  for(int i=n;i<n+3;i++){all[i].x=(int16_t)rs_screen_left();all[i].w=(int16_t)rs_screen_width();}
  all[n].y=(int16_t)top;all[n].h=(int16_t)-top;
  int ha=bot-RS_SCREEN_H<BAND_A?bot-RS_SCREEN_H:BAND_A;
  all[n+1].y=RS_SCREEN_H;all[n+1].h=(int16_t)ha;all[n+1].layers=4;all[n+1].sx[2]=DECOR_COL*8;all[n+1].sy[2]=144;
  all[n+2].y=(int16_t)(RS_SCREEN_H+ha);all[n+2].h=(int16_t)(bot-RS_SCREEN_H-ha);all[n+2].layers=4;all[n+2].sx[2]=DECOR_B_X+32;   /* (every column shown: inside chunk B) */all[n+2].sy[2]=104;
  rs_viewports(all[n+2].h>0?n+3:n+2,all,RS_HEX(0x29364a));
 }else rs_viewports(nviews,VP,RS_HEX(0x29364a));
 /* a tall screen (DESIGN.md "Tall screen"): one garden fills the display's height (the sides cropped), the lawn's
  * decor below it (rs_screen_fill above) */
 if(tall())tall_decor();
 shown_state=state;shown_best=best;
}
/* Touch (DESIGN.md "Controls"): the four beds of player 1's view are the tap zones; a touch acts on the nearest bed
 * (the garden cut halfway between the beds: any width works, the margins belong to the side beds). The view is
 * draw_frame's for that player count (rs_viewport_layout only computes rectangles); x, y: stage coordinates.
 * With 2-4 players a touch outside P1's view acts on no bed (the other gardens are played with pads). */
int draw_bed_at(int players,int x,int y){
 rs_viewport vps[RS_VIEW_MAX];int n=layout(players>1?players:1,vps);rs_viewport *vp=&vps[0];view v;
 if(n==1){vp->x=0;vp->w=RS_SCREEN_W;}
 else if(x<vp->x || x>=vp->x+vp->w || y<vp->y || y>=vp->y+vp->h)return -1;
 memset(&v,0,sizeof v);v.w=garden_w(n,vp);v.ox=(vp->w-v.w)/2;v.h=vp->h;v.small=vp->h<200;
 int best=-1;long bd=0;
 for(int i=0;i<4;i++){int bx,by;bed_position(i,&bx,&by);long dx=x-(vp->x+sx(&v,bx)),dy=y-(vp->y+sy(&v,by)),d=dx*dx+dy*dy;if(best<0 || d<bd){best=i;bd=d;}}
 return best;
}
#define S(v) rs_state_var("draw." #v,&(v),sizeof(v))
void draw_state(void){S(V);S(VP);S(nviews);S(shown_state);S(shown_best);RS_STATE_RASTER(raster);}
#undef S
