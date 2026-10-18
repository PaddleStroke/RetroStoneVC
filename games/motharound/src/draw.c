/* Moonlit garden, one lantern and shared rings for every player.
 * Widescreen (DESIGN.md "Widescreen"): a screen wider than 320 centres the 320-px garden (rs_screen_origin): the
 * lantern, the rings, the moths and the HUD keep their places; the screen shows more of the garden on both sides
 * (BG3: a 128-tile map of the 1024 x 160 garden, its own tiles: the hills, fence and hedge going on, foxgloves and a
 * rose arch on the left, a birdhouse and a sunflower on the right) and more stars in the wider sky. The one full view
 * starts at stage x -margin: the ring layer's pixels are drawn in the view's columns (stage x - vp->x), the sprites
 * in stage coordinates. */
#include "game.h"
#include "assets.h"
#include "sine.h"
#include "house_ui.h"
#include "house_tall.h"
#include "game_text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct view {int mx,my,cx,cy,zoom;rs_color sky[240];} view;
static view V[4];static rs_viewport VP[4];
static int nviews,shown_state=-1,shown_best=-1;
static int spr_dx;                     /* the sprites' x offset: -the viewport's x in the multiplayer frames (they are
                                        * relative to its top-left), 0 otherwise (set before each use) */
static int laid_w;                     /* the width the maps were laid out for (a state of another width: relayout) */
/* the ring layer's pixels, one view at a time (rings(): per-frame scratch, state_audit.txt), as wide as the widest
 * screen */
#define PIX_W RS_SCREEN_W_MAX
static uint8_t pixels[PIX_W*240];
static int wide(void){return rs_screen_width()>RS_SCREEN_W;}
/* a tall screen (portrait, zoomed: DESIGN.md "Tall screen") */
static int tall(void){return rs_screen_height()>RS_SCREEN_H;}
static int hud_lines;                  /* the HUD's band of BG1 at the screen's top (rs_bg_pin; set before each picture) */
static const rs_color title_ramps[2][3]={
 {RS_HEX(0xffe8a0),RS_HEX(0xf1b750),RS_HEX(0x99623c)},
 {RS_HEX(0xebc8fc),RS_HEX(0xbb83d0),RS_HEX(0x76509a)}};
/* A tall screen's lines past the 240 of the stage (DESIGN.md "Tall screen"): above, the sky going on, deeper toward
 * the screen's top, with the HUD's band of BG1 at the top (rs_bg_pin) and the sprites (the HUD's numbers, the moon,
 * the stars); below, the hedge's shade, darker with depth. The other layers are hidden there (window 1); line 0 puts
 * the stage's settings back, so a 240-line screen is unchanged. */
static void raster_out(int line) {
 int top=rs_screen_top(),bot=rs_screen_bottom();
 if(line==top || line==RS_SCREEN_H) {
  rs_window(0,rs_screen_left(),rs_screen_right());
  for(int l=RS_BG1;l<=RS_BG3;l++)rs_bg_window(l,RS_WIN1);
  if(line==top && hud_lines)rs_bg_window(RS_BG1,0);
  rs_obj_window(line==top?0:RS_WIN1);
 }
 if(hud_lines && line==top+hud_lines)rs_bg_window(RS_BG1,RS_WIN1);   /* past the HUD's band */
 if(line<0)rs_pal_set(0,hu_scale_color(V[0].sky[0],256-(-line*100/-top)-((line&4)?4:0)));   /* the house dither */
 else {
  int d=line-RS_SCREEN_H,n=bot-RS_SCREEN_H,k=n>0?d*120/n:0;
  rs_pal_set(0,hu_scale_color(RS_RGB8(36,61,74),236-k-((line&4)?6:0)));
 }
}
static void raster(int line,void *user) {
 (void)user;
 if(line<0 || line>=RS_SCREEN_H){raster_out(line);return;}
 if(line==0){for(int l=RS_BG1;l<=RS_BG3;l++)rs_bg_window(l,0);rs_obj_window(0);}   /* back from raster_out */
 int p=rs_viewport_current();if(p<0 || p>=nviews)p=0;
 int y=line-VP[p].y;if(y>=0 && y<240)rs_pal_set(0,V[p].sky[y]);
}
/* the maps of this width: 64 tiles at 320; 128 wider (the UI's corner tags, the rings' layer as wide as the screen,
 * the wide garden and its own tiles, the blank tile after them) */
static void maps_setup(void) {
 int mw=wide()?128:64,n=wide()?gm_hills_wide_tile_count:gm_bg3_tile_count;
 rs_bg_setup(RS_BG1,mw,32,VR_BG1);rs_bg_setup(RS_BG2,mw,32,VR_BG2);rs_bg_setup(RS_BG3,mw,32,VR_BG3);
 rs_tiles_load(VR_BG3,wide()?gm_hills_wide_tiles:gm_bg3_tiles,n);
 uint8_t blank[64]={0};rs_tiles_load8(VR_BG2,blank,1);rs_tiles_load8(VR_BG3+n,blank,1);
 rs_pal_load(RS_PAL_BG(PAL_HILLS),wide()?gm_hills_wide_pal:gm_hills_pal,16);
 laid_w=rs_screen_width();
}
/* After a state was loaded (rs_game.state_loaded): one saved at another width brings that width's maps; set this
 * width's up (each frame draws the whole screen again). Same width: nothing. */
void draw_relayout(void) {if(laid_w!=rs_screen_width())maps_setup();}
void draw_init(void) {
 rs_screen_origin(rs_screen_margin());   /* widescreen: the 320-px garden centred, more of it on both sides */
 /* a tall screen (portrait, zoomed): the stage low, TALL_BELOW lines of the hedge's shade under it, the sky above */
 {int extra=rs_screen_height()-RS_SCREEN_H;rs_screen_origin_v(extra>TALL_BELOW?extra-TALL_BELOW:extra/2);}
 hu_config cfg=hu_defaults();cfg.obj_tile=KIT_OBJ_TILE;cfg.obj_vram=VR_OBJ;cfg.obj_pal=3;hu_init(&cfg);
 maps_setup();
 static const rs_color ring_pal[16]={0,RS_HEX(0x263c46),RS_HEX(0x527766),RS_HEX(0x91b47d),
  RS_HEX(0xc0d499),RS_HEX(0x51607c),RS_HEX(0x8598b0),RS_HEX(0xbfd0cf),RS_HEX(0xf4dd9b),
  RS_HEX(0xbca172),RS_HEX(0x455371),RS_HEX(0x403e58),RS_HEX(0x57546b),0,0,0};
 rs_pal_load(RS_PAL_BG(PAL_GROUND),ring_pal,16);
 rs_obj_base(VR_OBJ);rs_tiles_load(VR_OBJ,gm_obj_tiles,gm_obj_tile_count);
 rs_pal_load(RS_PAL_OBJ(0),gm_obj_pals,48);rs_pal_load(RS_PAL_OBJ(4),gm_obj_pals+64,32);
 for(int l=0;l<3;l++)rs_bg_enable(l,1);
 memset(V,0,sizeof V);memset(VP,0,sizeof VP);nviews=0;rs_raster(raster,NULL);
}
/* the zoomed title (DESIGN.md "Tall screen"): the house layout (house_tall.h) takes the stage's rows, so the moth,
 * the lantern and the leaves move up into the sky above the stage, by this many lines (the lantern mid-sky) */
static int title_sky_dy(void) {return rs_screen_top()/2+8-102;}
static int ppal(int p) {return p?hu_player_pal(p):0;}
static void spr(int id,int x,int y,int priority,int pal,const rs_viewport *vp) {
 const gm_sprite_def *s=&gm_spr[id];
 int y0=tall()?rs_screen_top():0,y1=tall()?rs_screen_bottom():vp->h;   /* (a tall screen: the screen's lines) */
 if(x<=vp->x-(int)s->w || x>=vp->x+vp->w || y<=y0-(int)s->h || y>=y1)return;
 rs_spr(x+spr_dx,y,s->tile,s->w,s->h,pal<0?s->pal:pal,priority,0);
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
 if(x>=0 && x<vp->w && y>=0 && y<vp->h)p[y*PIX_W+x]=(uint8_t)color;
}
static void line(uint8_t *p,int ax,int ay,int bx,int by,int color,const rs_viewport *vp) {
 int dx=abs(bx-ax),sx=ax<bx?1:-1,dy=-abs(by-ay),sy=ay<by?1:-1,err=dx+dy;
 for(int k=0;k<700;k++) {
  pixel(p,ax,ay,color,vp);if(ax==bx && ay==by)break;
  int e=err*2;if(e>=dy){err+=dy;ax+=sx;}if(e<=dx){err+=dx;ay+=sy;}
 }
}
static void background(const view *v,rs_viewport *vp) {
 if(wide()) {   /* the whole wide garden, in stage coordinates (stage x = picture x & 1023), at the screen's foot */
  int r0=30-HILLS_WIDE_H;
  for(int r=0;r<32;r++)for(int c=0;c<128;c++)
   rs_bg_put(RS_BG3,c,r,r>=r0 && r<30?gm_hills_wide_map[(r-r0)*128+c]:RS_MAP(gm_hills_wide_tile_count,PAL_HILLS,0,0,0));
  vp->sx[2]=(int16_t)(vp->x&1023);vp->sy[2]=0;return;
 }
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
 memset(pixels,0,sizeof pixels);
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
   int xx=c*8+x,yy=r*8+y;tile[y*8+x]=(uint8_t)(xx<vp->w && yy<vp->h?pixels[yy*PIX_W+xx]:0);ink|=tile[y*8+x];
  }
  int id=0;if(ink){id=(*next)++;rs_tiles_load8(VR_BG2+id,tile,1);}
  rs_bg_put(RS_BG2,v->mx+c,v->my+r,RS_MAP(id,PAL_GROUND,0,0,0));
 }
}
/* A tall screen's sky above the stage, in the columns shown: the moon at stage y moon_y, `lit` stars from stage y top
 * down (none inside the stage lines skip0 .. skip1 - 1: the zoomed title's logo), the falling leaves */
static void sky(const world *w,const rs_viewport *vp,int top,int moon_y,int lit,int skip0,int skip1,int leaves) {
 int x0=rs_screen_visible_x0(),cw=rs_screen_visible_x1()-x0;
 spr(SPR_MOON,x0+cw-44,moon_y,0,-1,vp);
 // Stars emerge as dusk gives way to moonlight: a few first, all of them at night.
 static const int16_t stars[14][2]={{14,30},{60,8},{98,52},{150,22},{186,70},{36,96},{122,118},
  {170,150},{20,170},{80,196},{140,214},{196,236},{52,262},{110,286}};
 for(int k=0;k<lit;k++){int y=top+stars[k][1];if(y<70 && !(y+8>skip0 && y<skip1))spr(SPR_STAR,x0+stars[k][0]*(cw-16)/208,y,0,-1,vp);}
 if(!leaves)return;
 // Three leaves drift down from the screen's top, swaying, behind the rings, into the hills.
 int fall=150-rs_screen_top()+16;
 for(int k=0;k<3;k++){
  int y=rs_screen_top()-16+(w->t*(2+k%2)/5+k*fall/3)%fall,sway=(w->t/3+k*40)%64,dx=sway<32?sway:64-sway;
  spr(SPR_LEAF+(k&1),x0+4+k*(cw-40)/3+(y-rs_screen_top())/12+dx/2,y,0,-1,vp);
 }
}
static void scene(const world *w,int t,const view *v,const rs_viewport *vp) {
 int sx=vp->x;   /* the view's columns -> stage x */
 for(int p=0;p<w->players;p++){
 const hero *h=&w->h[p];int radius=orbit_radius(w->players,p);
 int x=px(v,radius,h->angle)+sx,y=py(v,radius,h->angle);
 if(h->state!=HS_OFF) {
  int id=SPR_HERO_ICON+(t/5)%2;
  spr(id,x-8,y-8,2,ppal(p),vp);
  if(h->smooth_t>0)hu_sparkle(x+8+spr_dx,y-8,w->t,2);
 }
 }
 spr(SPR_LANTERN+(w->t/12)%2,v->cx+sx-16,v->cy-24,2,-1,vp);
 if(tall()){   /* a tall screen: the moon and the stars over the sky above the stage, in the columns shown */
  int top=rs_screen_top()+HUD_LINES;sky(w,vp,top,top+18,w->t<300?4:w->t<600?7:w->t<1200?10:14,0,0,1);
  return;
 }
 // Stars emerge as dusk gives way to moonlight (widescreen: the same three over the 320 columns, more beyond them).
 if(w->t>600){
  int sw=vp->w>RS_SCREEN_W?RS_SCREEN_W:vp->w,x0=vp->w>RS_SCREEN_W?0:sx;
  for(int k=0;k<3;k++)spr(SPR_STAR,x0+16+k*(sw-32)/3,40+(k%2)*12,0,-1,vp);
  static const int16_t far[6][2]={{-36,58},{-82,34},{-112,66},{348,36},{394,62},{430,30}};
  if(vp->w>RS_SCREEN_W)for(int k=0;k<6;k++)spr(SPR_STAR,far[k][0],far[k][1],0,-1,vp);
 }
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
 int multi=w->players>1 && (state==ST_PLAY || state==ST_DEAD),play=state==ST_PLAY || state==ST_DEAD;
 /* a tall screen (DESIGN.md "Tall screen"): one zoomed look, the run filling the display's height (the sides
  * cropped), the title and the panels too, laid out in the columns shown (house_tall.h). Set before the UI is drawn:
  * the visible columns (ht_narrow) follow it. */
 rs_screen_fill(play || tall());
 int narrow=ht_narrow();
 nviews=rs_viewport_layout(1,0,VP);
 VP[0].y=0;VP[0].h=RS_SCREEN_H;      /* a tall screen: the view is the stage's 240 lines (the raster draws the rest) */
 hu_clear();rs_oam_clear();rs_brightness(paused?9:15);int next=1;
 for(int p=0;p<nviews;p++) {
  view *v=&V[p];rs_viewport *vp=&VP[p];const hero *h=&w->h[p];
  /* a view's own map area (the views right of or below the first); widescreen: the full view starts left of 0.
   * cx, cy: the lantern in the view's columns (stage x 160 at any width) */
  v->mx=vp->x>0?32:0;v->my=vp->y>0?16:0;v->cx=vp->w/2;
  v->cy=vp->h<200?75:LANTERN_Y;v->zoom=vp->h<200?128:vp->w<240?192:256;
  if(state==ST_TITLE){v->cx=182-vp->x;v->cy=102+(narrow?title_sky_dy():0);}   /* (zoomed: the halo off the stage) */
  int night=w->t*256/2400;if(night>256)night=256;
  rs_pal_set(RS_PAL_BG(PAL_GROUND)+11,hu_lerp_color(RS_HEX(0xa19882),RS_HEX(0x777261),night));
  for(int y=0;y<vp->h;y++)v->sky[y]=skycolor(night,y,vp->h);
  background(v,vp);rings(w,v,vp,state==ST_TITLE || state==ST_OVER,&next);
  /* the UI layer in stage coordinates (widescreen: the view's left edge is left of 0, round the map) */
  vp->sx[0]=(int16_t)((v->mx*8+vp->x)&(rs_bg_map_w(RS_BG1)*8-1));vp->sx[1]=(int16_t)(v->mx*8);vp->sy[0]=vp->sy[1]=(int16_t)(v->my*8);vp->layers=7;vp->objs=1;
  hu_view_oam_begin(vp);
  spr_dx=multi?-vp->x:0;
  if(state==ST_PLAY || state==ST_DEAD) {
   scene(w,st_t,v,vp);char text[128];
   if(multi) {
    /* a tall screen: two tags a row (rows 1 and 3), in the columns shown, at the screen's top (rs_bg_pin) */
    int c0=(rs_screen_visible_x0()+7)/8+1,half=(rs_screen_visible_x1()/8-c0)/2;
    for(int player=0;player<w->players;player++){
     char tag[16];rs_strf(tag,sizeof tag,HTR(PLAYER_TAG),player+1);snprintf(text,sizeof text,"%s %d",tag,w->h[player].score);
     if(tall())hu_text(c0+(player&1)*half,1+(player>>1)*2,text);else hu_text(player*10,1,text);
    }
   } else {
    hu_number(h->score,vp->x+vp->w/2,8+(tall()?rs_screen_top():0),3);   /* (a tall screen: at the screen's top) */
    rs_strf(text,sizeof text,TR(HUD_RINGS_TIME),h->anchor,h->alive_t/60);hu_text(hu_center(text,0),4,text);
   }
   /* (zoomed: in the columns shown; not ht_pause: hu_pause draws PAUSED only when it changes, and this UI is cleared
    * and drawn again every frame) */
   if(paused && narrow)ht_big(v->my+vp->h/16,HTR(PAUSED),HU_BIG_FREE);
   else if(paused)hu_big(v->mx+((2*vp->x+vp->w)/8-hu_width(HTR(PAUSED),1))/2,v->my+vp->h/16,HTR(PAUSED),HU_BIG_FREE);
  }
  hu_view_oam_end(vp);
 }
 spr_dx=0;
 if(!multi) {
  rs_viewport *vp=&VP[0];hu_standing rank;ranking(w,&rank);
  int slide=state==ST_OVER?hu_slide_in(st_t,20,200):0;
  if(state==ST_TITLE && narrow) {
   /* zoomed: the house layout (house_tall.h, its default rows), the hint under the slots; the moth, the lantern and
    * the leaves up in the sky above the stage (decor), the moon and the stars around them */
   ht_logo("MOTH AROUND",title_ramps,2,2,0);
   ht_title tc={TR(VERB),MAX_PLAYERS,15,0};ht_title_out at;
   ht_title_draw(&tc,st_t,best,&at);ht_text(17,TR(HINT_TITLE),0);ht_title_sprites(&tc,&at,st_t,icon,NULL);
   int dy=title_sky_dy();
   spr(SPR_HERO_IDLE+(st_t/8)%2,112,87+dy+hu_bob(st_t,64,3),2,0,vp);
   spr(SPR_LANTERN+(st_t/12)%2,166,78+dy,2,-1,vp);
   spr(SPR_LEAF,68,101+dy,1,-1,vp);spr(SPR_LEAF+1,242,96+dy,1,-1,vp);
   int top=rs_screen_top()+HUD_LINES;sky(w,vp,top,top+18,7,0,RS_SCREEN_H,0);
  } else if(state==ST_TITLE) {
   hu_logo("MOTH AROUND",title_ramps,2,2,0);hu_title_draw(st_t,best);
   /* the hint, centred (half a column left of the true centre: the English keeps its column 4) */
   int hx=(38-hu_width(TR(HINT_TITLE),0))/2;hu_text(hx<0?0:hx,16,TR(HINT_TITLE));hu_title_sprites(st_t,icon,NULL);
   spr(SPR_HERO_IDLE+(st_t/8)%2,112,87+hu_bob(st_t,64,3),2,0,vp);
   spr(SPR_LANTERN+(st_t/12)%2,166,78,2,-1,vp);
   spr(SPR_LEAF,68,101,1,-1,vp);spr(SPR_LEAF+1,242,96,1,-1,vp);
   spr(SPR_MOON,278,77,0,-1,vp);
  }
  if(state==ST_OVER && tall()){int top=rs_screen_top()+HUD_LINES;sky(w,vp,top,top+18,w->t<300?4:w->t<600?7:w->t<1200?10:14,0,0,0);}
  if(state==ST_OVER && narrow) {   /* zoomed: the panels in the columns shown (house_tall.h) */
   if(w->players==1) {
    static const int medals[4]=MEDAL_SCORES;
    ht_banner(4,HTR(GAME_OVER));ht_gameover_panel(new_best,NULL,HTR(BACK_TITLE));
    ht_gameover_sprites(w->h[0].score,best,hu_medal_of(w->h[0].score,medals),st_t,slide);
    ht_retry_line_at(ht_gameover_retry_row(NULL),st_t,RETRY_LOCK,HTR(BACK_TITLE));
   } else {
    ht_results_panel(&rank,NULL);ht_results_sprites(&rank,st_t,slide);
    for(int i=0;i<rank.n;i++){int x,y;ht_results_icon_pos(&rank,i,&x,&y);spr(SPR_HERO_ICON,x-8,y-8+slide,3,ppal(rank.order[i]),vp);}
    ht_retry_line_at(hu_results_retry_row(&rank),st_t,RETRY_LOCK,HTR(BACK_TITLE));
   }
  } else if(state==ST_OVER) {
   if(w->players==1) {
    static const int medals[4]=MEDAL_SCORES;
    hu_banner(4,HTR(GAME_OVER));hu_gameover_panel(1,new_best,w->h[0].score,0);
    hu_gameover_sprites(1,w->h[0].score,0,best,hu_medal_of(w->h[0].score,medals),st_t,slide);
    hu_retry_line(st_t,RETRY_LOCK,HTR(BACK_TITLE));
   } else {
    hu_results_panel(&rank,NULL);hu_results_sprites(&rank,st_t,slide);
    for(int i=0;i<rank.n;i++){int x,y;hu_results_icon_pos(&rank,i,&x,&y);spr(SPR_HERO_ICON,x-8,y-8+slide,3,ppal(rank.order[i]),vp);}
    hu_retry_line_at(hu_results_retry_row(&rank),st_t,RETRY_LOCK,HTR(BACK_TITLE));
   }
  }
  /* the global scrolls: the ring layer's view column 0 at the view's left edge, the garden in stage coordinates */
  rs_bg_scroll(RS_BG1,0,-slide);rs_bg_scroll(RS_BG2,(V[0].mx*8-vp->x)&(rs_bg_map_w(RS_BG2)*8-1),0);
  rs_bg_scroll(RS_BG3,(vp->sx[2]-vp->x)&(rs_bg_map_w(RS_BG3)*8-1),vp->sy[2]);rs_viewports(0,NULL,0);
 } else if(rs_screen_height()>RS_SCREEN_H) {
  /* a tall screen (320 columns: the view is the stage, x 0, y 0): the view's scrolls as the screen's, no viewport,
   * so the raster draws the lines past the stage */
  for(int l=RS_BG1;l<=RS_BG3;l++)rs_bg_scroll(l,VP[0].sx[l],VP[0].sy[l]);
  rs_viewports(0,NULL,0);
 } else rs_viewports(nviews,VP,RS_HEX(0x252b43));
 /* a tall screen: the HUD's band of BG1 at the screen's top during a run */
 hud_lines=tall() && play?HUD_LINES:0;
 rs_bg_pin(RS_BG1,0,hud_lines,0);
 shown_state=state;shown_best=best;
}
#define S(v) rs_state_var("draw." #v,&(v),sizeof(v))
void draw_state(void) {S(V);S(VP);S(nviews);S(shown_state);S(shown_best);S(laid_w);S(spr_dx);RS_STATE_RASTER(raster);}
#undef S
