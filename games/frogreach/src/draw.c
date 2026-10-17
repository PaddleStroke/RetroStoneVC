/* Frog Reach: swamp, lily pads and tongue lines and independent split-screen cameras.
 * Widescreen (DESIGN.md "Widescreen"): a crossing seen from the side, so the swamp is revealed wider: a third of each
 * view's extra width behind its frog, two thirds ahead (the stage origin stays 0; each camera starts view_left() px
 * further left, so the frog sits that much further right): the pads behind are drawn, the pads ahead with their
 * flies as far as the view goes (the same course: perch_pos). The views split the whole width (2 players: top and
 * bottom, 3: a top half and two lower panels, 4: quadrants); a panel is compact (the 16-px frog, the half zoom) when it
 * is narrower than 3/4 of the screen, as at 320. The scenery maps are 128 tiles wide: the reeds' panorama goes on
 * with its mirror image (the same tiles h-flipped: nothing repeats on a 560-px view), the right-hand views use map
 * columns 64 on; each view's text sits in the UI columns of its place on the screen. The UI is centred (the kit).
 * MIT, (c) 2026 Pierre-Louis Boyer (8BCraft). */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include "sine.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
typedef struct view { int cx,cy,mx,my; rs_color sky[240]; } view;
static view V[4];
static rs_viewport VP[4];
static int nviews,shown_state=-1,shown_best=-1;
static int laid_w;                     /* the width the maps were laid out for (a state of another width: relayout) */
/* the pond layer's pixels, one view at a time (web(): per-frame scratch, state_audit.txt), as wide as the widest
 * screen */
#define PIX_W RS_SCREEN_W_MAX
static uint8_t pixels[PIX_W*(240+TALL_BELOW)];   /* a tall screen: the pond down to the screen's bottom */
static int wide(void) { return rs_screen_width()>RS_SCREEN_W; }
static const rs_color title_ramps[2][3]={
 {RS_RGB8(216,241,125),RS_RGB8(135,201,80),RS_RGB8(65,124,69)},
 {RS_RGB8(255,213,194),RS_RGB8(232,138,156),RS_RGB8(150,79,116)}};
static const rs_color field_pal[16]={0,RS_HEX(0x994f74),RS_HEX(0xffbab5),RS_HEX(0xea7894),RS_HEX(0xd9ef7c),RS_HEX(0x87c84f),RS_HEX(0x4c8f46),RS_HEX(0x2a583f),RS_HEX(0x232a3a),RS_HEX(0x5d9ea6),RS_HEX(0x8ec0bc),RS_HEX(0x4f8f9c),RS_HEX(0x72afb0),RS_HEX(0x4a818c),RS_HEX(0xbbe6df),0};
/* A tall screen (rs_screen_zoom: a phone held upright; DESIGN.md "Tall screen"): the lines past the stage's 240. Above:
 * the sky carried on, deeper toward the screen's top. Below: the pond, deeper and darker. The other layers are hidden
 * there (their maps would wrap); the split screen: two more viewports over the margins, the backdrop only (the same
 * sky, the water's colour). Line 0 puts the layers and the pond's palette back. */
#define SKY_TOP_C RS_HEX(0x87b4cd)
/* The zoomed screen (v2, game.h ZOOM_H): one view (the title, a run, the panels) draws the pond's canvas down to the
 * screen's bottom (web: the ripples go on, no repeat), BG1's top HUD_LINES lines are pinned at the screen's top in a
 * run (hud_lines), and BG4 holds the sky's decor (decor_init): a faded far range of snowy mountains rising behind the
 * hills, their colours hazier toward their foot line by line (decor_line), and clouds above them in a band of their
 * own scroll; birds fly across the columns shown (sprites, draw_birds). */
static int tall(void) { return rs_screen_height()>RS_SCREEN_H; }
static int hud_lines,canvas_h=240,decor_t,decor_x,cloud_x;
#define VR_BG4 (VR_BG3+192)       /* after the hills' 185 tiles and their blank: 832 tiles up to VR_OBJ */
#define PAL_DECOR 7               /* BG4: the range (1-4, set per line: the haze), the clouds (5-7) */
#define OBJ_PAL_BIRD 7
#define BIRD_TILE 640             /* the birds' OBJ tiles, after the kit's (512 + HU_OBJ_TILES) */
#define DEC_OFF 320               /* BG4's map line 0 is stage line -DEC_OFF */
#define CLOUD_Y1 (-118)           /* the clouds' band ends above the range's highest peak (-112) */
#define RANGE_TOP (-112)
#define RANGE_FOOT 184
static rs_color sky_at(int line) {
 int top=rs_screen_top();
 if(line>=0) return V[0].sky[line<240?line:239];
 int k=top<0?-line*256/-top:0;k+=(line&1)?16:-16;   /* dithered over two lines: no bands */
 return hu_lerp_color(SKY_TOP_C,RS_HEX(0x4f86b4),k<0?0:k>256?256:k);
}
/* the range's colours on this line: the rock and the snow, lit and in shade, faded into the sky's colour behind them,
 * more toward their foot (the haze) */
static void decor_line(int line,rs_color sky) {
 static const rs_color base[4]={RS_HEX(0x7486b4),RS_HEX(0x56648f),RS_HEX(0xf2f5fb),RS_HEX(0xb4c0dc)};
 if(line<RANGE_TOP-2 || line>=RANGE_FOOT) return;
 int h=112+(line-RANGE_TOP)*120/(RANGE_FOOT-RANGE_TOP)+((line&1)?12:-12);   /* dithered over two lines */
 for(int i=0;i<4;i++) rs_pal_set(RS_PAL_BG(PAL_DECOR)+1+i,hu_lerp_color(base[i],sky,i>=2?h*3/4:h));
}
static void raster_out(int line) {
 int top=rs_screen_top(),bot=rs_screen_bottom();
 if(line==top || line==RS_SCREEN_H){rs_bg_enable(RS_BG1,line==top && hud_lines);rs_bg_enable(RS_BG3,0);rs_bg_enable(RS_BG2,line==RS_SCREEN_H);}
 if(hud_lines && line==top+hud_lines) rs_bg_enable(RS_BG1,0);
 if(line<0){
  rs_color c=sky_at(line);rs_pal_set(0,c);
  if(line==top) {rs_bg_enable(RS_BG4,1);rs_bg_scroll(RS_BG4,cloud_x,DEC_OFF);}
  if(nviews<=1){
   if(line==CLOUD_Y1) rs_bg_scroll(RS_BG4,decor_x,DEC_OFF);
   decor_line(line,c);
  } else if(line==CLOUD_Y1) rs_bg_enable(RS_BG4,0);   /* the split screen: the clouds over its views, not the range */
  return;
 }
 int d=line-RS_SCREEN_H,n=bot-RS_SCREEN_H,k=n>0?d*96/n:0;
 if(line==RS_SCREEN_H) rs_bg_enable(RS_BG4,0);
 /* one view: the pond's canvas goes on (web); its blues darker with depth, from the stage's own (no seam) */
 rs_pal_set(0,hu_scale_color(field_pal[9],256-k));
 if((d&1)==0)for(int i=9;i<15;i++)rs_pal_set(RS_PAL_BG(PAL_GROUND)+i,hu_scale_color(field_pal[i],256-k));
}
static void raster(int line,void *user) {
 if(line<0 || line>=RS_SCREEN_H){raster_out(line);return;}
 if(line==0 && tall()){       /* back from raster_out: the stage's own settings */
  for(int l=0;l<3;l++) rs_bg_enable(l,1);
  rs_pal_load(RS_PAL_BG(PAL_GROUND),field_pal,16);
 }
 (void)user; int p=rs_viewport_current(); if(p<0 || p>=nviews) p=0;
 int y=line-VP[p].y; if(y>=0 && y<240) rs_pal_set(0,V[p].sky[y]);
 if(tall() && nviews<=1) decor_line(line,V[0].sky[line]);
}
/* ---- the sky's decor (a tall screen: BG4, generated at start-up) ---- */
/* the far range's peaks: x on BG4's 512-px map, the top's stage y, the slope (8ths of a line per column) */
static const int16_t PEAKS[][3]={{36,-96,7},{120,-30,9},{196,-112,6},{270,-14,10},{338,-82,7},{412,-50,8},{470,6,11}};
#define NPEAKS ((int)(sizeof PEAKS/sizeof PEAKS[0]))
/* the clouds: puffs (cloud, x, stage y, radius) over each cloud's flat base */
static const int16_t PUFFS[][4]={
 {0,52,-157,9},{0,66,-163,13},{0,82,-159,10},{0,95,-155,6},
 {1,234,-134,7},{1,246,-140,11},{1,260,-137,8},
 {2,370,-181,8},{2,384,-188,12},{2,400,-185,10},{2,414,-180,7},
 {3,463,-142,5},{3,473,-144,6}};
static const int16_t CLOUD_BASE[4]={-149,-131,-175,-139};
static int wrap512(int d) { d&=511;return d>=256?d-512:d; }
static int ridge(int x,int *pk) {
 int best=9999;
 for(int i=0;i<NPEAKS;i++){int y=PEAKS[i][1]+abs(wrap512(x-PEAKS[i][0]))*PEAKS[i][2]/8;if(y<best){best=y;*pk=i;}}
 return best+ss_sine[(x*5)&255]*3/32768+ss_sine[(x*13+40)&255]*2/32768;
}
static int decor_pix(int x,int y,const int (*valley)[2]) {
 int sy=y-DEC_OFF,pk=0,r=ridge(x,&pk);
 if(sy>=r) {
  int dx=wrap512(x-PEAKS[pk][0]),d=sy-PEAKS[pk][1];
  /* the face away from the light: right of a jagged line down from the peak, above a fold rising to the valley on its right */
  int shade=dx*3>d+ss_sine[(sy*9+PEAKS[pk][0])&255]*4/32768 && sy<valley[pk][1]+wrap512(valley[pk][0]-x)*3/2+ss_sine[(x*7)&255]*3/32768;
  int snow=PEAKS[pk][1]<-60 && d<16+ss_sine[(x*11)&255]*5/32768+ss_sine[(x*29)&255]*2/32768;
  return snow?(shade?4:3):(shade?2:1);
 }
 if(sy>=CLOUD_Y1) return 0;
 for(int i=0;i<(int)(sizeof PUFFS/sizeof PUFFS[0]);i++) {
  const int16_t *p=PUFFS[i];int dx=wrap512(x-p[1]),dy=sy-p[2];
  if(sy>CLOUD_BASE[p[0]] || dx*dx+dy*dy>p[3]*p[3]) continue;
  if(sy>CLOUD_BASE[p[0]]-3) return 7;                                 /* the shaded base */
  int hx=dx+p[3]/3,hy=dy+p[3]/3;
  return hx*hx+hy*hy<p[3]*p[3]*9/16?5:6;                              /* lit from the top left */
 }
 return 0;
}
static void decor_init(void) {
 enum { MAXT=VR_OBJ-VR_BG4 };
 uint8_t (*uniq)[64]=malloc((size_t)MAXT*64);if(!uniq) return;
 int n=1;memset(uniq[0],0,64);
 int valley[NPEAKS][2];   /* the foot of each peak's shaded face: the lowest point of its ridge on its right (x, y) */
 for(int i=0;i<NPEAKS;i++){valley[i][0]=PEAKS[i][0];valley[i][1]=PEAKS[i][1];}
 for(int x=0;x<512;x++){int pk=0,r=ridge(x,&pk);if(wrap512(x-PEAKS[pk][0])>0 && r>valley[pk][1]){valley[pk][0]=x;valley[pk][1]=r;}}
 rs_bg_setup(RS_BG4,64,64,VR_BG4);
 for(int r=0;r<64;r++) for(int c=0;c<64;c++) {
  uint8_t t[64];int id=0;
  for(int y=0;y<8;y++) for(int x=0;x<8;x++) t[y*8+x]=(uint8_t)decor_pix(c*8+x,r*8+y,valley);
  while(id<n && memcmp(uniq[id],t,64)) id++;
  if(id==n){if(n<MAXT) memcpy(uniq[n++],t,64); else id=0;}
  rs_bg_put(RS_BG4,c,r,RS_MAP(id,PAL_DECOR,0,0,0));
 }
 rs_tiles_load8(VR_BG4,uniq[0],n);free(uniq);
 static const rs_color clouds[3]={RS_HEX(0xfbfbf6),RS_HEX(0xe2ebf2),RS_HEX(0xbccfe0)};
 rs_pal_load(RS_PAL_BG(PAL_DECOR)+5,clouds,3);
 /* the birds: 16x8, wings up and wings down, a slate silhouette */
 static const char *bird[2][8]={
  {"................","..1..........1..","...11......11...","....211..112....","......2112......",".......22.......","................","................"},
  {"................","................","......2112......","....112..211....","...11......11...","..1..........1..","................","................"}};
 uint8_t px[4*64];
 for(int f=0;f<2;f++) for(int y=0;y<8;y++) for(int x=0;x<16;x++) {
  char ch=bird[f][y][x];px[(f*2+x/8)*64+y*8+x%8]=(uint8_t)(ch=='1'?1:ch=='2'?2:0);
 }
 rs_tiles_load8(VR_OBJ+BIRD_TILE,px,4);
 static const rs_color birdc[2]={RS_HEX(0x3e4c68),RS_HEX(0x2c3650)};
 rs_pal_load(RS_PAL_OBJ(OBJ_PAL_BIRD)+1,birdc,2);
}
/* two flocks crossing the columns shown, slowly, already on screen at the start; frozen while paused */
static void draw_birds(int dy) {
 static const int8_t flock[5][3]={{0,0,0},{-12,-5,3},{-23,-9,5},{-10,6,2},{-19,11,6}};
 int x0=rs_screen_visible_x0()-rs_screen_origin_x(),vw=rs_screen_visible_x1()-rs_screen_visible_x0();
 for(int f=0;f<2;f++) {
  int span=vw+80,x=x0-40+(decor_t/(f?4:3)+(f?span/5:span*3/5))%span,y=f?-96:-176;
  int n=f?2:5;
  for(int i=0;i<n;i++) {
   int bx=x+flock[i][0]*(f?-1:1),by=y+flock[i][1]+hu_bob(decor_t+i*13,90,2),fr=((decor_t+flock[i][2]*5)/9)%2;
   if((decor_t/120+i)%3==0) fr=0;                                      /* a glide now and then */
   if(bx>x0-16 && bx<x0+vw) rs_spr(bx,by-dy,BIRD_TILE+fr*2,16,8,OBJ_PAL_BIRD,1,f?RS_SPR_HFLIP:0);
  }
 }
}
/* a message on the HUD rows of a zoomed screen: centred on the columns shown, on two rows when wider than them */
static void hud_message(int row,const char *s) {
 int cols=(rs_screen_visible_x1()-rs_screen_visible_x0())/8-1;
 if(hu_width(s,0)<=cols){hu_text(hu_center(s,0),row,s);return;}
 char a[160],b[160];int best=-1,bw=9999;size_t len=strlen(s);if(len>=sizeof a) len=sizeof a-1;
 for(size_t i=1;i<len;i++) if(s[i]==' ') {
  memcpy(a,s,i);a[i]=0;int wa=hu_width(a,0),wb=hu_width(s+i+1,0),w=wa>wb?wa:wb;
  if(w<bw){bw=w;best=(int)i;}
 }
 if(best<0){hu_text(hu_center(s,0),row,s);return;}
 memcpy(a,s,(size_t)best);a[best]=0;snprintf(b,sizeof b,"%s",s+best+1);
 hu_text(hu_center(a,0),row,a);hu_text(hu_center(b,0),row+1,b);
}
/* the maps of this width: 64 tiles at 320, 128 wider (views of up to 560 columns, the right-hand ones from column 64) */
static void maps_setup(void) {
 int mw=wide()?128:64;
 rs_bg_setup(RS_BG1,mw,32,VR_BG1);rs_bg_setup(RS_BG2,mw,tall()?64:32,VR_BG2);   /* a tall screen: the pond's canvas down to its bottom */
 rs_bg_setup(RS_BG3,mw,32,VR_BG3);
 laid_w=rs_screen_width();
}
/* After a state was loaded (rs_game.state_loaded): one saved at another width brings that width's maps; set this
 * width's up (each frame draws every view again). Same width: nothing. */
void draw_relayout(void) { if(laid_w!=rs_screen_width()) maps_setup(); }
void draw_init(void) {
 hu_config cfg=hu_defaults();cfg.obj_tile=KIT_OBJ_TILE;cfg.obj_vram=VR_OBJ;cfg.obj_pal=3;hu_init(&cfg);
 maps_setup();
 rs_tiles_load(VR_BG3,gm_bg3_tiles,gm_bg3_tile_count);
 uint8_t blank[64]={0};rs_tiles_load8(VR_BG2,blank,1);
 rs_tiles_load8(VR_BG3+gm_bg3_tile_count,blank,1);
 rs_pal_load(RS_PAL_BG(PAL_HILLS),gm_hills_pal,16);
  rs_pal_load(RS_PAL_BG(PAL_GROUND),field_pal,16);
 rs_obj_base(VR_OBJ);rs_tiles_load(VR_OBJ,gm_obj_tiles,gm_obj_tile_count);
 rs_pal_load(RS_PAL_OBJ(0),gm_obj_pals,48);rs_pal_load(RS_PAL_OBJ(4),gm_obj_pals+64,32);
 /* a tall screen (portrait, game.h ZOOM_H): the stage low, TALL_BELOW lines of pond under it; the sky's decor */
 if(tall()){
  int below=rs_screen_height()-RS_SCREEN_H>=2*TALL_BELOW?TALL_BELOW:(rs_screen_height()-RS_SCREEN_H)/2;
  rs_screen_origin_v(rs_screen_height()-RS_SCREEN_H-below);decor_init();
 }
 for(int l=0;l<3;l++) rs_bg_enable(l,1);
 memset(V,0,sizeof V);memset(VP,0,sizeof VP);nviews=0;
 rs_raster(raster,NULL);
}
static int floor8(int y) { return y>=0?y/8:(y-7)/8; }
static int ppal(int p) { return p?hu_player_pal(p):0; }
/* the UI layer's columns of a split view: a left-hand view from column 0, a right-hand view right after the left one's
 * (at 320: 159 px, 20 columns, so at 20), where the kit allows 19 or 20 columns of text at every width (its text may not
 * go left of column 0 or past the screen's right edge, -margin .. 320 + margin in its centred UI coordinates) */
static int view_ux(const rs_viewport *vp) { return vp->x?(vp->x-RS_VIEW_DIVIDER+7)/8:0; }
/* the column of a word of n columns centred on a view's window (ux, cols), kept within the kit's columns */
static int fit_col(int ux,int cols,int n) {
 int lim=(hu_ui_right()+7)/8,x=ux+(cols-n)/2;
 if(x+n>lim) x=lim-n;
 if(x<ux) x=ux;
 return x;
}
/* a panel narrower than 3/4 of the screen is compact (at 320: narrower than 240 columns) */
static int compact(const rs_viewport *vp) { return vp->w*RS_SCREEN_W<240*rs_screen_width(); }
static int zoom_x(const rs_viewport *vp) { return compact(vp)?128:256; }
static int zoom_y(const rs_viewport *vp) { return vp->h<200?128:256; }
/* widescreen: the columns a view shows behind its frog, a third of its extra width (its width less the one it has at
 * 320: 320 for a full view, 159 for a half); 0 at 320 */
static int view_left(const rs_viewport *vp) { return WIDE_LEFT(vp->w-vp->w*RS_SCREEN_W/rs_screen_width()); }
static int p1_drawn_x=-999,p1_view_left;   /* tests (draw_view_log): P1's sprite centre on the screen, as drawn */
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
 int xoff=v->cx>=0?v->cx/4:-((3-v->cx)/4),fx=floor8(xoff),dx=xoff-fx*8;
 int cols=(vp->w+dx+7)/8,rows=(vp->h+7)/8;
 for(int r=0;r<rows;r++) for(int c=0;c<cols;c++) {
  int yy=floor8(r*8-(sy(v,vp,180)-64));
  int row=(yy<HILLS_H?yy:HILLS_H-1)*64,col=(fx+c)&(wide()?127:63);
  /* widescreen: a 1024-px period, the panorama then its mirror image (h-flipped tiles: seamless at both joins) */
  uint16_t tile=yy<0?RS_MAP(gm_bg3_tile_count,PAL_HILLS,0,0,0):col<64?gm_hills_map[row+col]:
   (uint16_t)(gm_hills_map[row+127-col]^RS_MAP_HFLIP);
  rs_bg_put(RS_BG3,v->mx+c,v->my+r,tile);
 }
 vp->sx[2]=(int16_t)(v->mx*8+dx);vp->sy[2]=(int16_t)(v->my*8);
}
/* widescreen: the pads beyond the third ahead, while the view still shows them (at 320 the three ahead, as always) */
static int pad_seen(const world *w,int id,const view *v,const rs_viewport *vp) {
 if(!wide()) return 0;
 int x,width;perch_pos(w,id,w->t,&x,&width);
 return sx(v,vp,x)-width*zoom_x(vp)/512-8<vp->w;
}
static void pixel(uint8_t *p,int x,int y,int color,const rs_viewport *vp) {
 if(x>=0 && x<vp->w && y>=0 && y<canvas_h) p[y*PIX_W+x]=(uint8_t)color;
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
 memset(pixels,0,sizeof pixels);const hero *h=&w->h[player];int small=compact(vp),water=sy(v,vp,180);
 /* Every pad sits inside the pond, including the title's true initial scene. */
 for(int y=water;y<canvas_h;y++)for(int x=0;x<vp->w;x++){
  int wx=x+v->cx/2;int color=9;
  if(y==water)color=10;
  else if((y+player*3)%11==0 && (wx+w->t/4+y*7)%79<19)color=12;
  else if(y%17==3 && (wx-w->t/7-y*3)%107<14)color=10;
  pixel(pixels,x,y,color,vp);
 }
 /* widescreen: the pads behind the frog too, as far as the view shows them (pixel() clips) */
 for(int id=h->anchor-1-(view_left(vp)?2:0);id<=h->anchor+3 || (id<=h->anchor+16 && pad_seen(w,id,v,vp));id++)if(id>=0){
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
 int cols=(vp->w+7)/8,rows=(canvas_h+7)/8;
 for(int r=0;r<rows;r++) for(int c=0;c<cols;c++) {
  uint8_t tile[64];int ink=0;
  for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
   int xx=c*8+x,yy=r*8+y;
   tile[y*8+x]=(uint8_t)(xx<vp->w && yy<canvas_h?pixels[yy*PIX_W+xx]:0);ink|=tile[y*8+x];
  }
  int id=0;if(ink){id=(*next)++;rs_tiles_load8(VR_BG2+id,tile,1);}
  rs_bg_put(RS_BG2,v->mx+c,v->my+r,RS_MAP(id,PAL_GROUND,0,0,0));
 }
}
static void scene(const world *w,int p,int t,const view *v,const rs_viewport *vp) {
 const hero *h=&w->h[p];int small=compact(vp);
 int x=sx(v,vp,(int)(h->wx>>16)),y=sy(v,vp,h->y>>16);
 int n=rs_oam_next();
 spr(small?SPR_HERO_ICON:hero_frame(h,w->t),x-(small?8:16),y-(small?15:30),2,ppal(p),vp);
 if(p==0)p1_drawn_x=rs_oam_next()>n?vp->x+rs_screen_origin_x()+rs_oam(n)->x+(small?8:16):-999;
 if(h->smooth_t>0)hu_sparkle(x-(small?10:20)-hu_ui_x(),y-(small?8:17),w->t,2);   /* kit sprites: UI coordinates */
 for(int a=h->anchor+1;a<=h->anchor+3 || (a<=h->anchor+16 && pad_seen(w,a,v,vp));a++){
  int pad_x,width;perch_pos(w,a,w->t,&pad_x,&width);
  if(a==h->anchor+1 && h->mode==MODE_LEAP && h->perfect)continue;
  spr((small?SPR_FLY_SMALL:SPR_FLY)+(w->t/5)%2,sx(v,vp,pad_x)-(small?4:8),sy(v,vp,192)-(small?11:19)+hu_bob(w->t+a*11,64,1),1,-1,vp);
 }
 if(h->state==HS_RUN && h->mode==MODE_CHARGE && h->length*zoom_x(vp)/256>11)
  spr(SPR_TIP,sx(v,vp,(int)(h->wx>>16)+h->length)-4,sy(v,vp,192)-(small?10:15),2,-1,vp);
 if(h->state==HS_HIT && h->t<24)spr(SPR_SPLASH+(h->t/8)%3,x-12,sy(v,vp,192)-7,2,-1,vp);
}

static void icon(int p,int cx,int cy,int t,void *user) {
 (void)user;rs_viewport vp={0};vp.w=(int16_t)rs_screen_width();vp.h=240;
 spr(SPR_HERO_ICON,cx-8+hu_ui_x(),cy-8+hu_bob(t+p*16,64,1),2,ppal(p),&vp);   /* the kit's UI coordinates -> stage */
}
static void ranking(const world *w,hu_standing *rank) {
 int scores[4];for(int p=0;p<w->players;p++) scores[p]=w->h[p].score;
 hu_rank(rank,w->players,scores,scores);
}
void draw_frame(const world *w,int state,int st_t,int best,int new_best,int paused) {
 int multi=w->players>1 && (state==ST_PLAY || state==ST_DEAD);
 int fresh=nviews!=(multi?w->players:1) || shown_state==ST_TITLE;
 nviews=rs_viewport_layout(multi?w->players:1,RS_LAYOUT_HSPLIT,VP);
 if(rs_screen_height()>RS_SCREEN_H)       /* a tall screen: the layout of the 240-line stage (the margins: raster_out) */
  for(int p=0;p<nviews;p++){
   int first=VP[p].y==rs_screen_top(),hh=(RS_SCREEN_H-RS_VIEW_DIVIDER)/2;
   VP[p].h=(int16_t)(VP[p].h==rs_screen_height()?RS_SCREEN_H:hh);VP[p].y=(int16_t)(first?0:RS_SCREEN_H-hh);
  }
 hu_clear();rs_oam_clear();rs_brightness(paused?9:15);
 /* a tall screen (game.h ZOOM_H, DESIGN.md "Tall screen"): a run of one frog fills the display's height, the sides
  * cropped (the columns shown centred: the frog 20 px from their left edge on a 9:21 phone), the HUD pinned at the
  * screen's top; the title, the panels and the split screens show every column */
 int hud=tall() && !multi && (state==ST_PLAY || state==ST_DEAD);
 rs_screen_fill(hud);hud_lines=hud?HUD_LINES:0;rs_bg_pin(RS_BG1,0,hud_lines,0);
 if(!paused) decor_t++;
 int next=1;
 for(int p=0;p<nviews;p++) {
  view *v=&V[p];rs_viewport *vp=&VP[p];const hero *h=&w->h[p];
  v->mx=vp->x?(wide()?64:32):0;v->my=vp->y?16:0;
  canvas_h=vp->h+(tall() && nviews==1?rs_screen_bottom()-RS_SCREEN_H:0);   /* one view: the pond to the screen's bottom */
  /* the UI layer's columns of the view (view_ux; the scenery layers need up to 71 columns per view: the right-hand
   * views' at 64 on a wide screen, 32 at 320) */
  int ux=view_ux(vp);
  int cx=(int)(h->wx>>16)-((compact(vp)?32:56)+view_left(vp))*256/zoom_x(vp);   /* widescreen: a third behind */
  if(p==0)p1_view_left=view_left(vp);
  int cy=192-(vp->h>=200?196:vp->h-26)*256/zoom_y(vp);
  /* Title and gameplay use the same camera and the same world positions. */
  if(fresh || !paused){v->cx=cx;v->cy=cy;}
  for(int y=0;y<vp->h;y++) {
   rs_color top=RS_HEX(0x87b4cd),bottom=RS_HEX(0xe8e4b6);
   int k=y*256/vp->h;if(tall() && nviews==1) k+=(y&1)?10:-10;   /* a tall screen: dithered over two lines */
   v->sky[y]=hu_lerp_color(top,bottom,k<0?0:k>256?256:k);
  }
  background(v,vp);web(w,p,v,vp,&next);
  vp->sx[0]=(int16_t)(ux*8);vp->sx[1]=(int16_t)(v->mx*8);vp->sy[0]=vp->sy[1]=(int16_t)(v->my*8);
  vp->layers=7;vp->objs=1;
  hu_view_oam_begin(vp);
  if(state==ST_TITLE)scene(w,0,0,v,vp);
  if(state==ST_PLAY || state==ST_DEAD) {
   scene(w,p,st_t,v,vp);
   if(!multi) hu_number(h->score,vp->w/2-hu_ui_x(),hud?rs_screen_top()+8:8,3);
   char text[128];
   if(multi) {
    char tag[16];rs_strf(tag,sizeof tag,HTR(PLAYER_TAG),p+1);
    snprintf(text,sizeof text,"%s %d",tag,h->score);
    hu_text(ux+1,v->my+1,text);
   } else {
    int left=pad_life(h->anchor)-h->wait;if(left<0)left=0;
    const char *message=h->mode==MODE_LEAP?(h->good?TR(MSG_NICE_CATCH):TR(MSG_MISSED)):
       h->mode==MODE_CHARGE?TR(MSG_RELEASE):TR(MSG_HOLD);
    rs_strf(text,sizeof text,TR(HUD_FLIES_PAD),h->apples,(left+59)/60);
    if(hud){hud_message(4,message);hu_text(hu_center(text,0),7,text);}   /* (pinned at the screen's top) */
    else {hu_text(hu_center(message,0),4,message);hu_text(hu_center(text,0),6,text);}
   }
   if(paused) {
    /* centred on the view's columns; 1x when the 2x word is wider than the view (or than the kit's columns of a
     * right-hand view on a wide screen) */
    const char *s=HTR(PAUSED);int cols=multi?vp->w/8:40,n=hu_width(s,1),big=n<=cols && fit_col(ux,cols,n)==ux+(cols-n)/2;
    if(big) hu_big(fit_col(ux,cols,n),v->my+vp->h/16,s,HU_BIG_FREE);
    else hu_text(fit_col(ux,cols,hu_width(s,0)),v->my+vp->h/16+1,s);
   }
  }
  hu_view_oam_end(vp);
 }
 if(!multi) {
  rs_viewport *vp=&VP[0];hu_standing rank;ranking(w,&rank);
  int slide=state==ST_OVER?hu_slide_in(st_t,20,200):0;
  if(state==ST_TITLE) {
   hu_logo("FROG REACH",title_ramps,2,2,0);hu_title_draw(st_t,best);
   /* the hint, centred (half a column to the left, as the English always was) */
   int hw=hu_width(TR(HINT_TITLE),0);hu_text(hw<39?(39-hw)/2:0,16,TR(HINT_TITLE));hu_title_sprites(st_t,icon,NULL);

  }
  if(state==ST_OVER) {
   if(w->players==1) {
    static const int medals[4]=MEDAL_SCORES;
    hu_banner(4,HTR(GAME_OVER));hu_gameover_panel(1,new_best,w->h[0].score,0);
    hu_gameover_sprites(1,w->h[0].score,0,best,hu_medal_of(w->h[0].score,medals),st_t,slide);
    hu_retry_line(st_t,RETRY_LOCK,HTR(BACK_TITLE));
   } else {
    hu_results_panel(&rank,NULL);hu_results_sprites(&rank,st_t,slide);
    for(int i=0;i<rank.n;i++) {int x,y;hu_results_icon_pos(&rank,i,&x,&y);spr(SPR_HERO_ICON,x-8+hu_ui_x(),y-8+slide,3,ppal(rank.order[i]),vp);}
    hu_retry_line_at(hu_results_retry_row(&rank),st_t,RETRY_LOCK,HTR(BACK_TITLE));
   }
  }
  rs_bg_scroll(RS_BG1,-hu_ui_x()&(rs_bg_map_w(RS_BG1)*8-1),-slide);   /* the UI centred (widescreen) */
  rs_bg_scroll(RS_BG2,0,0);rs_bg_scroll(RS_BG3,vp->sx[2],vp->sy[2]);
  if(tall()){   /* the sky's decor: the far range slower than the hills, the clouds drifting; the birds */
   decor_x=(V[0].cx>=0?V[0].cx/8:-((7-V[0].cx)/8))&511;cloud_x=((V[0].cx>=0?V[0].cx/12:0)+decor_t/16)&511;
   draw_birds(0);
  }
  rs_viewports(0,NULL,0);
 } else if(rs_screen_height()>RS_SCREEN_H) {   /* a tall screen: the margins' viewports */
  rs_viewport all[6];memcpy(all,VP,sizeof(rs_viewport)*(size_t)nviews);
  rs_viewport *m=&all[nviews];memset(m,0,2*sizeof *m);
  m[0].x=m[1].x=(int16_t)rs_screen_left();m[0].w=m[1].w=(int16_t)rs_screen_width();
  m[0].y=(int16_t)rs_screen_top();m[0].h=(int16_t)-rs_screen_top();
  m[1].y=RS_SCREEN_H;m[1].h=(int16_t)(rs_screen_bottom()-RS_SCREEN_H);
  /* the sky above the views: the clouds (BG4, the stage lines past the range hidden: raster_out) and the birds */
  cloud_x=(decor_t/16)&511;
  m[0].layers=8;m[0].sx[3]=(int16_t)cloud_x;m[0].sy[3]=(int16_t)(DEC_OFF+m[0].y);m[0].objs=1;
 hu_view_oam_begin(&m[0]);draw_birds(m[0].y);hu_view_oam_end(&m[0]);
  rs_viewports(nviews+2,all,RS_HEX(0x254f62));
 } else rs_viewports(nviews,VP,RS_HEX(0x254f62));
 shown_state=state;shown_best=best;
}
/* Tests (--opt view=1, main.c at exit): P1's view of the last frame, "view: width=W left=L p1_x=X next_x=N": L the
 * columns shown behind the frog (a third of the view's extra width), X the screen x of P1's sprite centre as drawn, N
 * the screen x of the next pad's centre (tests/wide_test.sh: X and N - X against 320) */
void draw_view_log(const world *w) {
 const hero *h=&w->h[0];int x,width;perch_pos(w,h->anchor+1,w->t,&x,&width);
 rs_log("view: width=%d left=%d p1_x=%d next_x=%d",rs_screen_width(),p1_view_left,p1_drawn_x,
        VP[0].x+rs_screen_origin_x()+sx(&V[0],&VP[0],x));
}
#define S(v) rs_state_var("draw." #v,&(v),sizeof(v))
void draw_state(void) { S(V);S(VP);S(nviews);S(shown_state);S(shown_best);S(laid_w);S(hud_lines);S(canvas_h);S(decor_t);S(decor_x);S(cloud_x);RS_STATE_RASTER(raster); }
#undef S
