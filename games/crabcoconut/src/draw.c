/* One full-screen beach court with shared shells and individual coconuts.
 * Widescreen (DESIGN.md "Widescreen"): a screen wider than 320 centres the 320-px court (rs_screen_origin) and shows
 * more of the beach on both sides: the sea, sandbanks and palm islands go on (BG3: a 128-tile map of the 1024-px
 * beach, its own tiles), the sand too, with a beach umbrella and a towel on the left, a sandcastle and a bucket on
 * the right (theme.inc theme_decor). The walls stay where they are: the coconuts never leave the court. */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "house_tall.h"            /* a phone held upright: the title and the panels in the house's portrait layout */
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
/* A tall screen (a phone held upright; DESIGN.md "Tall screen"). The court is the whole 320 columns (its walls at
 * the screen's edges, the coconuts bouncing from x 8 to 312), so the game keeps rs_screen_tall (every column shown)
 * rather than the zoom, and composes the portrait around it: the court's 240 lines drawn on the whole screen (no
 * viewport), TALL_ABOVE_NUM / 5 of the extra lines above it, the rest below; the lines past it are the raster's.
 * - Above: the sky carried on from the court's top colour, deeper toward the top, the HUD at the top (the score's
 *   digits; several players: the scores' row of BG1, pinned there: rs_bg_pin), the sun, clouds and gulls (BG4).
 * - Below: the court's sand carried on (the backdrop, flat: no band), and the beach life drawn once into BG4
 *   (tall_decor): the umbrella and the towel, the sandcastle and the bucket, starfish, footprints, a beach ball.
 * BG1-3 and the sprites are hidden there (window 1 over the whole width; shown in the HUD's band: no shell above);
 * BG4 shows only there (its map: the sky's things at lines 256 - above .. 255, the sand's from 256). Line 0 puts the
 * court's settings back on every screen (a state saved on a tall screen may hold the margins' windows). */
#define TALL_ABOVE_NUM 2        /* 2/5 of the extra lines above the court (180 of 452 on a 1080x2340 phone) */
#define HUD_Y 10                /* the HUD's band: this many lines below the screen's top (clear of the corner buttons) */
#define HUD_LINES 24            /* the band where the sprites show above the court (the score's digits; no shell) */
#define VR_BG4 (VR_BG3+128)     /* the tall screen's decor tiles, after the beach's (at most 896) */
#define BG4_MAX 896
static int hud_lines;           /* the HUD's band of BG1 at the top (several players), set by draw_frame each picture */
static int ui_pin;              /* the UI layer pinned on the screen (house_tall.h ht_pin: the title, the panels), set by draw_frame */
static int laid_h;              /* the height BG4's decor was drawn for (a state of another height: drawn again) */
static int tall(void){return rs_screen_height()>RS_SCREEN_H;}
static int bg2_sx(void){return (V[0].mx*8-VP[0].x)&(rs_bg_map_w(RS_BG2)*8-1);}
static void raster_out(int line){
 int top=rs_screen_top();
 if(line==top || line==RS_SCREEN_H){
  rs_window(0,rs_screen_left(),rs_screen_right());
  for(int l=0;l<3;l++)rs_bg_window(l,RS_WIN1);
  rs_bg_window(RS_BG4,0);rs_bg_scroll(RS_BG4,0,line==top?256:256-RS_SCREEN_H);
  rs_obj_window(RS_WIN1);
 }
 if(line==top+HUD_Y){rs_obj_window(0);if(hud_lines)rs_bg_window(RS_BG1,0);}   /* the HUD's band: its digits, the scores' row */
 if(hud_lines && line==top+HUD_Y+hud_lines)rs_bg_window(RS_BG1,RS_WIN1);
 if(line==top+HUD_Y+HUD_LINES)rs_obj_window(RS_WIN1);
 if(ui_pin){rs_bg_window(RS_BG1,ht_pin_line(ui_pin,line)?0:RS_WIN1);rs_obj_window(ht_pin_line(ui_pin,line)?0:RS_WIN1);}   /* the pinned UI, its sprites */
 if(line<0){int k=top<0?-line*256/-top:0;rs_pal_set(0,hu_lerp_color(V[0].sky[0],RS_RGB8(104,170,214),k));return;}
 rs_pal_set(0,theme_pal[2]);   /* the court's sand, flat (a gradient would band) */
}
static void raster(int line,void *user){(void)user;
 if(line<0 || line>=RS_SCREEN_H){raster_out(line);return;}
 if(line==0){   /* always: a state saved on a tall screen may hold the margins' windows */
  for(int l=0;l<3;l++)rs_bg_window(l,0);
  rs_bg_window(RS_BG4,RS_WIN1);
  rs_obj_window(0);
 }
 int p=rs_viewport_current();if(p<0 || p>=nviews)p=0;int y=line-VP[p].y;if(y>=0 && y<240)rs_pal_set(0,V[p].sky[y]);}
/* the tall screen's decor (BG4, raster_out), drawn once for this height: a canvas of BG4's 512 map lines (stage y
 * above the court: line 256 + y; below it: line 256 + y - 240), cut into tiles (the same tile once) */
static uint8_t *tcv;
static void tpx(int x,int y,int c){int m=(y<0?256+y:16+y)&511;if(x>=0 && x<RS_SCREEN_W)tcv[m*RS_SCREEN_W+x]=(uint8_t)c;}
static void tdisc(int cx,int cy,int rx,int ry,int c){
 for(int y=-ry;y<=ry;y++)for(int x=-rx;x<=rx;x++)if(x*x*ry*ry+y*y*rx*rx<=rx*rx*ry*ry)tpx(cx+x,cy+y,c);}
static void tcloud(int x,int y,int w){   /* puffs, a flat shaded base */
 for(int i=0;i<w;i++){int r=5+((i*7+x)%4);tdisc(x+i*9,y-(i%2)*3,r+2,r,1);}
 for(int xx=x-4;xx<=x+(w-1)*9+4;xx++){tpx(xx,y+5,5);tpx(xx,y+4,5);}}
static void tgull(int x,int y){for(int k=0;k<4;k++){tpx(x-k,y-(k<2?k:3-k+1),7);tpx(x+k,y-(k<2?k:3-k+1),7);}tpx(x,y+1,7);}
static void tall_decor(void){
 laid_h=rs_screen_height();
 if(!tall())return;
 int above=-rs_screen_top(),below=rs_screen_bottom()-RS_SCREEN_H,ty=-above;
 tcv=calloc(RS_SCREEN_W*512,1);if(!tcv)return;
 /* the sky: the sun, clouds, gulls (between the HUD and the court) */
 tdisc(256,ty+54,17,17,8);tdisc(256,ty+54,13,13,1);
 tcloud(40,ty+76,5);tcloud(170,ty+108,4);tcloud(236,ty+150>-24?-24:ty+150,3);tcloud(86,-30,3);
 tgull(132,ty+46);tgull(150,ty+54);tgull(60,ty+120);tgull(206,-52);
 /* the sand: the grains (a pattern of 32 x 32: a few tiles), then the beach's things */
 for(int y=0;y<below;y++)for(int x=0;x<RS_SCREEN_W;x++){int a=x&31,b=y&31;if((a*5+b*11)%37==0 && ((a^b)&1))tpx(x,240+y,3);}
 rs_viewport vp={0};vp.w=RS_SCREEN_W;vp.h=240;memset(pixels,0,sizeof pixels);
 theme_decor(pixels,110,&vp);theme_decor(pixels,-130,&vp);   /* the umbrella, towel and starfish; the castle and bucket */
 int y0=below>200?40:16;                                       /* where the umbrella's top goes below the court */
 for(int y=130;y<236 && y0+y-130<below;y++)for(int x=0;x<RS_SCREEN_W;x++)if(pixels[y*PIX_W+x])tpx(x,240+y0+y-130,pixels[y*PIX_W+x]);
 for(int i=0;i<9;i++){int fx=48+i*24,fy=240+below-28-i*9+(i&1)*5;     /* footprints toward the sea */
  if(fy<240+y0+110)break;
  tdisc(fx,fy,2,3,4);tdisc(fx+4,fy-5,2,3,4);}
 if(below>200){int bx=228,by=240+below-50;tdisc(bx,by,11,11,1);       /* a beach ball */
  for(int y=-11;y<=11;y++)for(int x=-11;x<=11;x++)if(x*x+y*y<=121){int s=(x*3+y*2+60)/9%3;if(s)tpx(bx+x,by+y,s==1?9:10);}
  tdisc(bx-4,by-4,2,2,1);for(int x=-9;x<=9;x++)tpx(bx+x,by+12,4);}
 for(int k=-3;k<=3;k++){tpx(150+k,240+below-20,9);tpx(150,240+below-20+k,9);}   /* a second starfish */
 /* the tiles: tile 0 blank; each other the same tile once */
 rs_bg_setup(RS_BG4,64,64,VR_BG4);
 static const uint8_t blank[64];rs_tiles_load8(VR_BG4,blank,1);
 uint8_t *seen=malloc(BG4_MAX*64);int n=1;
 if(seen)memset(seen,0,64);
 for(int r=0;r<64 && seen;r++)for(int c=0;c<64;c++){
  uint8_t t[64];int ink=0;
  for(int y=0;y<8;y++)for(int x=0;x<8;x++){int xx=c*8+x;t[y*8+x]=xx<RS_SCREEN_W?tcv[(r*8+y)*RS_SCREEN_W+xx]:0;ink|=t[y*8+x];}
  int id=0;
  if(ink){for(id=1;id<n && memcmp(seen+id*64,t,64);id++);if(id==n){if(n==BG4_MAX)id=0;else{memcpy(seen+n*64,t,64);rs_tiles_load8(VR_BG4+n,t,1);n++;}}}
  rs_bg_put(RS_BG4,c,r,RS_MAP(id,PAL_GROUND,0,0,0));
 }
 free(seen);free(tcv);tcv=NULL;
}
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
 /* a tall screen: the court TALL_ABOVE_NUM / 5 of the extra lines down, the sky and the beach's decor (BG4) */
 rs_screen_origin_v((rs_screen_height()-RS_SCREEN_H)*TALL_ABOVE_NUM/5);tall_decor();
 for(int l=0;l<3;l++)rs_bg_enable(l,1);
 rs_bg_enable(RS_BG4,tall());
 memset(V,0,sizeof V);memset(VP,0,sizeof VP);nviews=0;rs_raster(raster,NULL);
}
/* After a state was loaded (rs_game.state_loaded): one saved at another width brings that width's maps; set this
 * width's up (each frame draws the whole screen again). Same width: nothing. */
void draw_relayout(void){if(laid_w!=rs_screen_width())maps_setup();if(laid_h!=rs_screen_height())tall_decor();}
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
/* player p's icon, its top-left at stage x, y: anywhere on the screen (a tall screen: the title and the panels pinned) */
static void icon_at(int p,int x,int y,int prio){const gm_sprite_def *s=&gm_spr[SPR_HERO_ICON];rs_spr(x,y,s->tile,s->w,s->h,ppal(p),prio,0);}
static void icon(int p,int cx,int cy,int t,void *user){(void)user;icon_at(p,cx-8,cy-8+hu_bob(t+p*16,64,1),2);}
void draw_frame(const world *w,int state,int st_t,int best,int new_best,int paused){
 int multi=w->players>1 && (state==ST_PLAY || state==ST_DEAD);nviews=rs_viewport_layout(1,0,VP);
 if(tall()){VP[0].y=0;VP[0].h=RS_SCREEN_H;}   /* a tall screen: the view is the court's 240 lines (raster_out) */
 /* a tall screen: the beach's decor in the margins (BG4), the scores' row of several players at the top (BG1 row 1) */
 rs_bg_enable(RS_BG4,tall());hud_lines=tall() && multi?8:0;rs_bg_pin(RS_BG1,8,hud_lines,HUD_Y);
 ui_pin=!ht_narrow() || multi?HT_PIN_OFF:state==ST_TITLE?HT_PIN_TITLE:state==ST_OVER?HT_PIN_PANEL:HT_PIN_OFF;
 if(ui_pin)ht_pin(ui_pin);   /* the title and the panels at the same place on every game's screen */
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
   if(multi){for(int player=0;player<w->players;player++){char score[48],tag[16];rs_strf(tag,sizeof tag,HTR(PLAYER_TAG),player+1);snprintf(score,sizeof score,"%s %d",tag,w->h[player].score);hu_text(player*10+(tall()?1:0),1,score);}}   /* (a tall screen: clear of the corner buttons) */
   else{hu_number(h->score,vp->x+vp->w/2,tall()?rs_screen_top()+HUD_Y+6:8,3);}   /* (a tall screen: at its top) */
   if(paused)hu_big(v->mx+((2*vp->x+vp->w)/8-hu_width(HTR(PAUSED),1))/2,v->my+vp->h/16,HTR(PAUSED),HU_BIG_FREE);
  }else if(state==ST_OVER)theme_shells(w,vp);
  hu_view_oam_end(vp);
 }
 spr_dx=0;
 if(!multi){
  rs_viewport *vp=&VP[0];int scores[4];for(int p=0;p<w->players;p++)scores[p]=w->h[p].score;hu_standing rank;hu_rank(&rank,w->players,scores,scores);
  int slide=state==ST_OVER?hu_slide_in(st_t,20,200):0;
  if(state==ST_TITLE && tall()){   /* a tall screen: the house's portrait title (house_tall.h, its standard rows), the crab under the slots */
   ht_logo(TITLE,title_ramps,2,HT_ROW_LOGO,0);ht_title tc={HTR(PLAY),MAX_PLAYERS,HT_ROW_SLOTS,0,TR(HINT)};ht_title_out to;
   ht_title_draw(&tc,st_t,best,&to);ht_title_sprites(&tc,&to,st_t,icon,NULL);theme_title(st_t,vp,to.gap_y1-124,to.gap_y0);
  }else if(state==ST_TITLE){hu_logo(TITLE,title_ramps,2,2,0);hu_title_draw(st_t,best);hu_text(hu_center(TR(HINT),0),16,TR(HINT));hu_title_sprites(st_t,icon,NULL);theme_title(st_t,vp,0,0);}
  if(state==ST_OVER && tall()){   /* a tall screen: the house's portrait panels (house_tall.h) */
   if(w->players==1){static const int medals[4]=MEDAL_SCORES;ht_banner(4,HTR(GAME_OVER));ht_gameover_panel(new_best,NULL,HTR(BACK_TITLE));ht_gameover_sprites(w->h[0].score,best,hu_medal_of(w->h[0].score,medals),st_t,slide);ht_retry_line_at(ht_gameover_retry_row(NULL),st_t,RETRY_LOCK,HTR(BACK_TITLE));}
   else{ht_results_panel(&rank,NULL);ht_results_sprites(&rank,st_t,slide);for(int i=0;i<rank.n;i++){int x,y;ht_results_icon_pos(&rank,i,&x,&y);icon_at(rank.order[i],x-8,y-8+slide,3);}ht_retry_line_at(ht_results_retry_row(&rank),st_t,RETRY_LOCK,HTR(BACK_TITLE));}
  }else if(state==ST_OVER){
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
void draw_state(void){S(V);S(VP);S(nviews);S(shown_state);S(shown_best);S(laid_w);S(laid_h);S(spr_dx);RS_STATE_RASTER(raster);}
#undef S
