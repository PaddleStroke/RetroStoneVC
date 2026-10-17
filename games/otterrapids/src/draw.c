/* A river seen straight from above. Everything shares one world scroll. */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include "sine.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static rs_color water[240];
static const rs_color river_pal[16]={0,RS_RGB8(126,199,192),RS_RGB8(78,162,168),RS_RGB8(43,119,138),RS_RGB8(224,173,106),RS_RGB8(166,112,67),RS_RGB8(79,53,47),RS_RGB8(175,188,188),RS_RGB8(113,135,151),RS_RGB8(55,75,101),RS_RGB8(178,207,122),RS_RGB8(112,163,110),RS_RGB8(62,116,98),RS_RGB8(238,246,217),RS_RGB8(235,173,132),RS_RGB8(39,50,75)};
static const rs_color ramps[2][3]={{RS_RGB8(254,219,147),RS_RGB8(219,154,85),RS_RGB8(160,93,57)},{RS_RGB8(165,236,220),RS_RGB8(93,189,196),RS_RGB8(45,125,155)}};
/* A tall screen (rs_screen_zoom: a phone held upright; DESIGN.md "Tall screen"): the whole screen is the river. The
 * foreground (BG2: the banks, islands, reeds, ripples, rocks, logs, whirlpools and crocodiles, its tiles made each
 * frame) covers every line, its map TALL_ROWS rows tall, scrolled to the screen's top. The lines past the stage carry
 * the water's gradient on (the backdrop) and show BG1's empty bottom rows (its scroll set per line), but for its HUD
 * rows pinned at the screen's top (rs_bg_pin); line 0 puts BG1's scroll back. A 240-line screen never gets these
 * lines: the same pixels. */
#define TALL_ROWS 64
static int tall_screen(void){return rs_screen_height()>RS_SCREEN_H;}
static int fdiv(int a,int b){return a>=0?a/b:-((b-1-a)/b);}
static int bg1_sx,bg1_sy,hud_pin;       /* BG1's scroll as written (the panel slides, the pause in the columns shown),
                                         * its HUD lines pinned at the screen's top: set by draw_frame */
static void raster_out(int y){
 int e=y&4?3:0,c[3]={86+e+fdiv(y,24),169+e+fdiv(y,32),175+e+fdiv(y,40)};
 for(int i=0;i<3;i++)c[i]=c[i]<0?0:c[i]>255?255:c[i];
 rs_pal_set(0,RS_RGB8(c[0],c[1],c[2]));
 if(y-rs_screen_top()<hud_pin)rs_bg_scroll(RS_BG1,bg1_sx,0);    /* the pinned HUD: BG1's stage lines from 0 */
 else rs_bg_scroll(RS_BG1,bg1_sx,240+(y&15)-y);                 /* map rows 30-31: empty */
}
static void raster(int y,void*u){(void)u;
 if(y<0||y>=240){raster_out(y);return;}
 if(y==0&&tall_screen())rs_bg_scroll(RS_BG1,bg1_sx,bg1_sy);
 rs_pal_set(0,water[y]);}
void draw_init(void){hu_config c=hu_defaults();c.obj_tile=KIT_OBJ_TILE;c.obj_vram=VR_OBJ;c.obj_pal=3;hu_init(&c);
 /* a tall screen: the stage low, TALL_BELOW lines of river under it, the rest above (more of the river ahead) */
 if(tall_screen()){int m=rs_screen_height()-RS_SCREEN_H-TALL_BELOW;rs_screen_origin_v(m<0?0:m);}
 rs_bg_setup(RS_BG1,64,32,VR_BG1);rs_bg_setup(RS_BG2,64,tall_screen()?TALL_ROWS:32,VR_BG2);rs_bg_enable(RS_BG1,1);rs_bg_enable(RS_BG2,1);rs_bg_enable(RS_BG3,0);
 uint8_t empty[64]={0};rs_tiles_load8(VR_BG2,empty,1);rs_pal_load(RS_PAL_BG(1),river_pal,16);
 rs_obj_base(VR_OBJ);rs_tiles_load(VR_OBJ,gm_obj_tiles,gm_obj_tile_count);rs_pal_load(RS_PAL_OBJ(0),gm_obj_pals,48);rs_pal_load(RS_PAL_OBJ(4),gm_obj_pals+64,32);
 for(int y=0;y<240;y++){int d=(y&4)?3:0;water[y]=RS_RGB8(86+d+y/24,169+d+y/32,175+d+y/40);}rs_raster(raster,NULL);
 draw_layout(NULL);                      /* widescreen: the UI map and the land layer for the width */
}
/* A drawing surface: stage columns x0..x1-1, lines y0..y0+h-1 (the foreground: the stage and EXT columns on each
 * side; a tall screen: its lines from the top). */
typedef struct canvas{uint8_t*px;int x0,x1,y0,h,stride;} canvas;
static void dot(const canvas*p,int x,int y,int c){if(x>=p->x0 && x<p->x1 && y>=p->y0 && y<p->y0+p->h)p->px[(y-p->y0)*p->stride+x-p->x0]=(uint8_t)c;}
static void rect(const canvas*p,int x0,int y0,int x1,int y1,int c){if(x0<p->x0)x0=p->x0;if(y0<p->y0)y0=p->y0;if(x1>p->x1-1)x1=p->x1-1;if(y1>p->y0+p->h-1)y1=p->y0+p->h-1;for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++)dot(p,x,y,c);}
static void line(const canvas*p,int x0,int y0,int x1,int y1,int c){int n=abs(x1-x0)>abs(y1-y0)?abs(x1-x0):abs(y1-y0);if(n<1)n=1;for(int i=0;i<=n;i++)dot(p,x0+(x1-x0)*i/n,y0+(y1-y0)*i/n,c);}
static void oval(const canvas*p,int x,int y,int rx,int ry,int c){for(int yy=-ry;yy<=ry;yy++)for(int xx=-rx;xx<=rx;xx++)if(xx*xx*ry*ry+yy*yy*rx*rx<=rx*rx*ry*ry)dot(p,x+xx,y+yy,c);}
static void poly(const canvas*p,int x,int y,const int*xy,int n,int c){int top=50,bottom=-50;for(int i=0;i<n;i++){if(xy[i*2+1]<top)top=xy[i*2+1];if(xy[i*2+1]>bottom)bottom=xy[i*2+1];}for(int yy=top;yy<=bottom;yy++){int l=100,r=-100;for(int i=0;i<n;i++){int j=(i+1)%n,x0=xy[i*2],y0=xy[i*2+1],x1=xy[j*2],y1=xy[j*2+1];if(yy<(y0<y1?y0:y1)||yy>(y0>y1?y0:y1))continue;if(y0==y1){if(x0<l)l=x0;if(x1<l)l=x1;if(x0>r)r=x0;if(x1>r)r=x1;}else{int xx=x0+(x1-x0)*(yy-y0)/(y1-y0);if(xx<l)l=xx;if(xx>r)r=xx;}}if(l<=r)rect(p,x+l,y+yy,x+r,y+yy,c);}}
static int world_y(const world*w,int d){return HERO_LINE+w->scroll-d;}
static void crocodile(const canvas*p,int x,int y,int age){
 /* The submerged silhouette is always visible before its lethal bite. */
 if(age<0){
  oval(p,x,y-4,12,19,3);oval(p,x,y+18,9,11,3);
  for(int k=0;k<20;k++)rect(p,x-(20-k)/5,y-24-k,x+(20-k)/5,y-24-k,3);
  for(int side=-1;side<=1;side+=2){oval(p,x+side*12,y-7,6,3,3);oval(p,x+side*11,y+11,5,3,3);}
  line(p,x-5,y-14,x-3,y-24,2);line(p,x+2,y+19,x+2,y+25,2);
  return;
 }
 int grow=age<8?age:8;
 oval(p,x,y,18+grow,28+grow,1);oval(p,x,y,16+grow,26+grow,2);
 if(age<5){crocodile(p,x,y,-1);return;}
 for(int k=0;k<22;k++)rect(p,x-(22-k)/5,y-21-k,x+(22-k)/5,y-21-k,12);
 oval(p,x,y-6,12,18,15);oval(p,x-1,y-7,10,16,12);oval(p,x-3,y-9,6,13,11);
 for(int side=-1;side<=1;side+=2){line(p,x+side*8,y-10,x+side*18,y-5,15);line(p,x+side*8,y+5,x+side*17,y+13,12);}
 for(int k=0;k<5;k++){rect(p,x-2,y-19+k*5,x+1,y-17+k*5,12);dot(p,x,y-19+k*5,10);}
 oval(p,x,y+14,11,12,15);oval(p,x-1,y+13,9,10,11);
 int open=age>=8&&age<27;
 if(open){oval(p,x,y+19,8,8,15);oval(p,x,y+22,4,3,14);for(int k=-6;k<=6;k+=4){line(p,x+k,y+13,x+k,y+16,13);line(p,x+k,y+24,x+k,y+22,13);}}
 else{line(p,x-7,y+21,x+7,y+21,15);for(int k=-5;k<=5;k+=5)dot(p,x+k,y+22,13);}
 for(int side=-1;side<=1;side+=2){oval(p,x+side*7,y+5,3,3,10);dot(p,x+side*7,y+6,15);}
 dot(p,x-4,y+10,13);
}
/* the rock (a river hazard; the land beyond the stage has the same rocks) */
static void rock(const canvas*p,int x,int y){
 static const int edge[]={-17,3,-15,-7,-8,-14,5,-13,14,-6,17,4,10,13,-9,12};
 static const int body[]={-15,3,-13,-6,-7,-12,4,-11,12,-5,15,3,9,11,-8,10};
 static const int lit[]={-12,-5,-6,-11,0,-7,-4,3,-13,2};
 poly(p,x,y,edge,8,15);poly(p,x,y,body,8,8);poly(p,x,y,lit,5,7);
 line(p,x+3,y-7,x+9,y+6,9);line(p,x+9,y+6,x+2,y+10,9);line(p,x-6,y+6,x-1,y+4,7);
}
static void hazard(const canvas*p,int x,int y,int kind,int t,int bite){
 if(y<p->y0-60 || y>p->y0+p->h+60)return;
 if(kind==CROCODILE){crocodile(p,x,y,bite);return;}
 if(kind==ROCK){
  oval(p,x,y+12,21,6,1);oval(p,x+2,y+10,18,4,3);rock(p,x,y);
  line(p,x-16,y+12,x-7,y+15,13);line(p,x+6,y+14,x+17,y+10,13);
 }else if(kind==LOG){
  oval(p,x,y+9,35,4,3);oval(p,x,y,32,10,6);oval(p,x-1,y-1,30,8,5);rect(p,x-24,y-7,x+24,y-5,4);
  oval(p,x-25,y,6,8,6);oval(p,x-25,y-1,4,6,4);oval(p,x-25,y,2,4,5);
  for(int k=0;k<3;k++)line(p,x-13+k*11,y-1+k%2,x-5+k*11,y+1+k%2,6);
  line(p,x+10,y-6,x+18,y-13,6);line(p,x+11,y-7,x+18,y-12,5);
  line(p,x-28,y+11,x-17,y+13,13);line(p,x+12,y+12,x+28,y+10,1);
 }else{
  oval(p,x,y,21,19,1);oval(p,x,y,18,16,2);oval(p,x,y,11,10,3);oval(p,x,y,4,4,15);
  for(int arm=0;arm<3;arm++)for(int k=0;k<44;k++){int a=(t*2+arm*85+k*2)&255,r=4+k/3;int xx=x+sine[a]*r/32767,yy=y+sine[(a+64)&255]*r*4/5/32767;dot(p,xx,yy,k>34?13:1);}
 }
}
static void spr(int id,int x,int y,int pal){const gm_sprite_def*s=&gm_spr[id];if(x<=rs_screen_left()-(int)s->w||x>=rs_screen_right()||y<=rs_screen_top()-(int)s->h||y>=rs_screen_bottom())return;rs_spr(x,y,s->tile,s->w,s->h,pal<0?s->pal:pal,2,0);}
/* The foreground (BG2) is drawn here each frame: the stage, and on a wide screen also EXT columns on each side, where
 * the bank's bushes overhang the stage's edges (the land layer, BG3, shows the land beyond: land() below). */
#define EXT 16
#define FW (RS_SCREEN_W+2*EXT)
#define FG_LINES (ZOOM_H+8)             /* a tall screen's lines, whole tiles */
static uint8_t fg_px[FW*FG_LINES];         /* the foreground drawn (draw_frame's scratch) */
static void tiles_tall(const canvas*p);
static void foreground(const world*w){
 /* a tall screen: every line, from the top (whole tiles: the map's row 0 at the screen's top) */
 int top=tall_screen()?rs_screen_top():0,lines=tall_screen()?(rs_screen_bottom()-top+7)/8*8:240;
 if(lines>FG_LINES)lines=FG_LINES;
 memset(fg_px,0,(size_t)(FW*lines));const canvas cv={fg_px,-EXT,RS_SCREEN_W+EXT,top,lines,FW},*p=&cv;
 int dlo=HERO_LINE+w->scroll-(top+lines),dhi=HERO_LINE+w->scroll-top;   /* the river distances shown */
 for(int y=top;y<top+lines;y++){int d=HERO_LINE+w->scroll-y,left,right;river_banks(w,d,&left,&right);
  rect(p,0,y,left,y,12);rect(p,0,y,left-5,y,11);
  rect(p,left-3,y,left-1,y,4);dot(p,left,y,13);dot(p,left+1,y,3);dot(p,left+2,y,1);
  rect(p,right,y,319,y,12);rect(p,right+5,y,319,y,11);
  rect(p,right+1,y,right+3,y,4);dot(p,right,y,13);dot(p,right-1,y,3);dot(p,right-2,y,1);
  if((d%23+23)%23==0){line(p,left-10,y,left-6,y,10);line(p,right+6,y,right+10,y,10);}
  int center,width=river_island(w,d,&center);
  if(width){
   rect(p,center-width-2,y,center+width+2,y,3);
   rect(p,center-width-1,y,center+width+1,y,13);
   rect(p,center-width,y,center+width,y,4);
   if(width>3)rect(p,center-width+2,y,center+width-2,y,11);
   if(width>8)rect(p,center-width+5,y,center+width-5,y,12);
   if(width>8 && d%19==0)line(p,center-width+3,y,center-width+7,y,10);
  }
 }
 // Island foliage follows exactly the same world coordinates as its shoreline.
 int island_base=w->scroll/85,k0=island_base-3,k1=island_base+5;if(tall_screen()){k0=fdiv(dlo-30,85);k1=fdiv(dhi+30,85)+1;}
 for(int k=k0;k<=k1;k++){
  int d=k*85,center,width=river_island(w,d,&center);
  if(width<22)continue;
  int x=center+(k&1?8:-8),y=world_y(w,d);
  oval(p,x+2,y+4,12,10,12);oval(p,x,y,12,10,11);oval(p,x-3,y-3,9,7,10);
  for(int j=0;j<3;j++){line(p,x-3+j*3,y+13,x-5+j*3,y+6,10);line(p,x-3+j*3,y+13,x+j*3,y+7,11);}
 }
 // Reeds, grassy banks and rounded foliage move with the river, never with a separate camera.
 int base=w->scroll/70;
 int i0=base-3,i1=base+5;if(tall_screen()){i0=fdiv(dlo-40,70);i1=fdiv(dhi+40,70)+1;}   /* (a tall screen: every line) */
 for(int i=i0;i<i1;i++){int y=world_y(w,i*70),left,right;river_banks(w,i*70,&left,&right);int side=(i&1)?right+12:left-12;
  oval(p,side,y,13,12,12);oval(p,side-2,y-3,11,9,11);oval(p,side-4,y-6,7,5,10);
  for(int k=0;k<3;k++){int x=side+(i&1?-6:6)+k*2;line(p,x,y+24,x-3,y+12-k*2,12);line(p,x,y+24,x+2,y+14-k*2,10);}
 }
 // Surface ripples and current streaks. Contrast stays below rocks and players.
 int r0=w->scroll/24-4,r1=w->scroll/24+9;if(tall_screen()){r0=fdiv(dlo-8,24);r1=fdiv(dhi+8,24)+1;}
 for(int row=r0;row<r1;row++)for(int col=0;col<5;col++){
  int d=row*24,yy=world_y(w,d),x=45+col*51+((row*13+col*7)&15),center,left,right;
  river_banks(w,d,&left,&right);if(x<left+12||x>right-22)continue;
  int width=river_island(w,d,&center);if(width&&abs(x-center)<width+15)continue;
  int current=river_current(d,x),lean=current>0?9:current<0?-9:0;
  line(p,x,yy,x+8+lean,yy-1,1);line(p,x+4,yy+4,x+9+lean,yy+3,2);
  if(lean){int end=x+8+lean;line(p,end,yy-1,end-(lean>0?4:-4),yy-4,1);}
 }
 int first=course_index(w->scroll-180),last=first+4;if(tall_screen()){first=course_index(dlo-140);last=course_index(dhi+140)+1;}
 for(int id=first>0?first-1:0;id<=last;id++){
  river_row r;course_row(w,id,&r);
  for(int k=0;k<r.count;k++){
   int bite=-1;for(int j=0;j<4;j++)if(w->bite[j].active&&w->bite[j].x==r.x[k]&&w->bite[j].distance==r.at[k])bite=w->bite[j].t;
   hazard(p,r.x[k],world_y(w,r.at[k]),r.kind[k],w->t,bite);
  }
 }
 for(int k=0;k<w->players;k++){const hero*h=&w->h[k];if(h->state==HS_OFF)continue;int y=player_y(w->players,k);
  for(int j=0;j<3;j++){int yy=y+17+j*7;line(p,h->x-7-j*2,yy,h->x-4-j*2,yy+2,1);line(p,h->x+7+j*2,yy,h->x+4+j*2,yy+2,1);}
  if(h->state==HS_HIT || h->state==HS_DOWN){int rr=12+h->t/2;oval(p,h->x,y,rr,rr/2,1);oval(p,h->x,y,rr-2,rr/2-1,2);}
 }
 if(tall_screen()){tiles_tall(p);return;}
 /* the tiles: the stage's 40 columns (in the same order at 320), on a wide screen also EXT/8 columns on each side */
 int wide=rs_screen_width()>RS_SCREEN_W,c0=wide?-EXT/8:0,c1=wide?40+EXT/8:40,mw=rs_bg_map_w(RS_BG2);
 int next=1;for(int y=0;y<30;y++)for(int x=c0;x<c1;x++){uint8_t tile[64];int ink=0;for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++){tile[yy*8+xx]=fg_px[(y*8+yy)*FW+x*8+xx+EXT];ink|=tile[yy*8+xx];}int id=0;if(ink && next<1024){id=next++;rs_tiles_load8(VR_BG2+id,tile,1);}rs_bg_put(RS_BG2,x&(mw-1),y,RS_MAP(id,1,0,0,0));}
}
/* A tall screen's tiles: the map's rows from the screen's top, the columns shown (and one more on each side; none
 * past them), each different tile once (the banks' grass, the open water: the same tiles). */
static uint8_t tall_tile[1024][64];     /* the tiles loaded this frame (draw_frame's scratch) */
static uint16_t tall_hash[4096];        /* ... their ids by hash (0: none) */
static void tiles_tall(const canvas*p){
 int x0=rs_screen_visible_x0()-rs_screen_origin_x(),x1=rs_screen_visible_x1()-rs_screen_origin_x();
 int c0=fdiv(x0,8)-1,c1=fdiv(x1+7,8)+1,mw=rs_bg_map_w(RS_BG2),mh=rs_bg_map_h(RS_BG2),next=1;
 if(c0<0)c0=0;
 if(c1>40)c1=40;
 memset(tall_hash,0,sizeof tall_hash);
 for(int y=0;y<mh;y++)for(int x=0;x<40;x++){
  int id=0;
  if(y<p->h/8 && x>=c0 && x<c1){uint8_t tile[64];int ink=0;uint32_t h=2166136261u;
   for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++){uint8_t v=p->px[(y*8+yy)*p->stride+x*8+xx+EXT];tile[yy*8+xx]=v;ink|=v;h=(h^v)*16777619u;}
   if(ink){int k=(int)(h&4095);
    while(tall_hash[k] && memcmp(tall_tile[tall_hash[k]],tile,64))k=(k+1)&4095;
    if(tall_hash[k])id=tall_hash[k];
    else if(next<1024){id=next++;tall_hash[k]=(uint16_t)id;memcpy(tall_tile[id],tile,64);rs_tiles_load8(VR_BG2+id,tile,1);}
   }
  }
  rs_bg_put(RS_BG2,x&(mw-1),y,RS_MAP(id,1,0,0,0));
 }
}

/* ---- widescreen: the land beyond the stage (BG3) ------------------------------------------------------------------
 * The banks never leave the stage, so on a wider screen both margins are land: the meadow by the river, then the
 * forest. Code-drawn tiles (once, at VR_LAND) in the style of the banks' bushes, the river's rocks and logs; a map
 * written each frame from the world's tile rows, so it scrolls with the river. The land is cut in cells (the meadow:
 * 6 columns, then the forest: bands of 7, each band's rows shifted; 6 rows) that hold at most one object each (seeded
 * by the course and the cell: the same land at every width), single-tile tufts, flowers and pebbles in the free
 * tiles. The EXT columns next to the stage are plain grass under the overhanging bushes. */
#define VR_LAND 2048                    /* after BG2's tiles (VR_BG2 + 1..1023), before the sprites (VR_OBJ) */
#define CELL 6
#define MEADOW 6
#define FOREST 7
enum { L_EMPTY, L_FLAT, L_TUFT, L_TUFT2, L_FLOWER, L_PEBBLE, L_OBJ };
enum { O_BUSH, O_TREE, O_BIGTREE, O_ROCK, O_STONES, O_LOG, O_POND, O_REEDS, O_STUMP, O_FLOWERS, O_TWIN, O_COUNT };
static const uint8_t obj_w[O_COUNT]={4,5,6,5,3,6,6,3,3,3,7},obj_h[O_COUNT]={4,5,6,5,2,3,4,4,3,2,6};
/* the objects' odds: the meadow (the first cell from the stage), the forest (further out); the last: none */
static const uint8_t odds[2][O_COUNT+1]={{3,0,0,1,2,1,1,2,1,3,0,2},{1,4,3,1,0,0,0,0,1,0,3,0}};
static int obj_first(int o){int t=L_OBJ;for(int i=0;i<o;i++)t+=obj_w[i]*obj_h[i];return t;}
static uint32_t hash4(uint32_t a,int b,int c,int d){
 uint32_t v=a*0x9e3779b1u^(uint32_t)b*0x85ebca77u^(uint32_t)c*0xc2b2ae3du^(uint32_t)d*0x27d4eb2fu;
 v^=v>>16;v*=0x7feb352du;v^=v>>15;v*=0x846ca68bu;return v^(v>>16);
}
/* a round canopy seen from above: lumps around a dark rim, the light from the top left */
static void canopy(const canvas*p,int x,int y,int r){
 int ring=r*3/4,lump=r/2;
 for(int a=0;a<256;a+=32)oval(p,x+1+sine[(a+64)&255]*ring/32767,y+2+sine[a]*ring*9/10/32767,lump,lump*7/8,12);
 oval(p,x+1,y+2,r-2,r-3,12);
 for(int a=16;a<256;a+=32)oval(p,x-1+sine[(a+64)&255]*(ring-2)/32767,y-1+sine[a]*(ring-2)*9/10/32767,lump-3,(lump-3)*7/8,11);
 oval(p,x-1,y-1,r-5,r-6,11);
 oval(p,x-r/3,y-r/3,r/3,r/4,10);oval(p,x+r/4,y-r/2,r/6,r/7+1,10);oval(p,x-r/2,y+r/6,r/6,r/7+1,10);
 for(int k=0;k<6;k++)dot(p,x+sine[(k*43+20)&255]*r/2/32767,y+sine[(k*43+84)&255]*r/2/32767,12);
}
static void stone(const canvas*p,int x,int y,int rx,int ry){
 oval(p,x+1,y+2,rx,ry,12);oval(p,x,y,rx,ry,15);oval(p,x,y,rx-1,ry-1,8);oval(p,x-1,y-1,rx-3>0?rx-3:1,ry-2>0?ry-2:1,7);
}
static void reed(const canvas*p,int x,int y,int k){line(p,x,y,x-3,y-14+k%3*3,12);line(p,x,y,x+2,y-12+k%2*3,10);}
static void land_art(const canvas*p,int what){
 switch(what){
 case L_TUFT:line(p,1,4,5,4,10);dot(p,6,5,12);break;
 case L_TUFT2:line(p,2,7,1,3,12);line(p,3,7,4,2,10);line(p,5,7,6,4,12);break;
 case L_FLOWER:dot(p,2,3,13);dot(p,4,3,13);dot(p,3,2,13);dot(p,3,4,13);dot(p,3,3,4);dot(p,6,6,14);dot(p,5,6,10);break;
 case L_PEBBLE:oval(p,4,5,2,1,12);oval(p,4,4,2,1,8);dot(p,3,3,7);break;
 case L_OBJ+O_BUSH:oval(p,16,15,13,12,12);oval(p,14,12,11,9,11);oval(p,12,9,7,5,10);break;
 case L_OBJ+O_TREE:canopy(p,19,18,17);break;
 case L_OBJ+O_BIGTREE:canopy(p,23,22,21);break;
 case L_OBJ+O_ROCK:oval(p,21,29,18,5,12);rock(p,20,18);break;
 case L_OBJ+O_STONES:stone(p,7,7,5,4);stone(p,17,9,4,3);break;
 case L_OBJ+O_LOG:
  oval(p,25,17,20,4,12);oval(p,24,11,19,7,6);oval(p,23,10,17,5,5);rect(p,10,6,36,7,4);
  oval(p,7,11,4,6,6);oval(p,7,10,3,4,4);oval(p,7,11,1,2,5);
  for(int k=0;k<3;k++)line(p,14+k*9,10+k%2,20+k*9,12+k%2,6);
  line(p,31,5,36,1,6);line(p,32,5,36,2,5);break;
 case L_OBJ+O_POND:
  oval(p,25,17,21,12,12);oval(p,24,16,20,11,4);oval(p,24,16,18,9,2);oval(p,25,17,14,6,3);
  line(p,13,12,21,11,1);line(p,27,22,33,22,1);oval(p,33,12,3,2,11);dot(p,32,11,10);
  for(int k=0;k<4;k++)reed(p,5+k*2,27,k);
  break;
 case L_OBJ+O_REEDS:
  oval(p,12,28,9,3,12);for(int k=0;k<5;k++)reed(p,5+k*3,29,k);
  oval(p,4,13,1,3,6);oval(p,13,12,1,3,6);oval(p,20,15,1,2,6);break;
 case L_OBJ+O_STUMP:
  oval(p,13,14,8,7,12);line(p,5,13,1,17,6);line(p,19,15,22,19,6);
  oval(p,12,12,8,7,6);oval(p,12,12,6,5,5);oval(p,12,12,4,3,4);oval(p,12,12,2,1,5);dot(p,12,12,6);break;
 case L_OBJ+O_FLOWERS:
  line(p,2,12,4,12,10);line(p,16,13,20,13,10);line(p,9,6,12,6,10);
  for(int k=0;k<5;k++){int x=3+k*4+(k&1),y=4+(k*5)%9;dot(p,x-1,y,k&1?14:13);dot(p,x+1,y,k&1?14:13);dot(p,x,y-1,k&1?14:13);dot(p,x,y+1,k&1?14:13);dot(p,x,y,4);}
  break;
 case L_OBJ+O_TWIN:canopy(p,19,19,16);canopy(p,38,29,13);break;
 }
}
static void land_tiles(void){
 for(int what=0;what<L_OBJ+O_COUNT;what++){
  int o=what-L_OBJ,tw=o>=0?obj_w[o]:1,th=o>=0?obj_h[o]:1,first=o>=0?obj_first(o):what;
  uint8_t px[56*48];const canvas cv={px,0,tw*8,0,th*8,tw*8};
  memset(px,what==L_EMPTY?0:11,sizeof px);land_art(&cv,what);
  for(int ty=0;ty<th;ty++)for(int tx=0;tx<tw;tx++){uint8_t tile[64];
   for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)tile[yy*8+xx]=px[(ty*8+yy)*tw*8+tx*8+xx];
   rs_tiles_load8(VR_LAND+first+ty*tw+tx,tile,1);}
 }
}
static int pick(int band,uint32_t h){
 const uint8_t*od=odds[band>0];int sum=0,o=0;for(int j=0;j<=O_COUNT;j++)sum+=od[j];
 int r=(int)(h%(uint32_t)sum);while(r>=od[o]){r-=od[o];o++;}
 return o;
}
/* the land's tile at stage column c and world tile row k (k grows downstream: the row's screen y is 8k + HERO_LINE +
 * scroll) */
static int land_tile(const world*w,int c,int k){
 if(c>=0 && c<40)return L_EMPTY;                       /* the stage: the river and its banks (BG2) */
 int e=c<0?-1-c:c-40;                                  /* columns from the stage's edge */
 if(e<EXT/8)return L_FLAT;
 e-=EXT/8;
 int side=c<0,band=e<MEADOW?0:1+(e-MEADOW)/FOREST,cw=band?FOREST:MEADOW,e0=band?MEADOW+(band-1)*FOREST:0;
 int cl=side?-EXT/8-e0-cw:40+EXT/8+e0,lx=c-cl,i=fdiv(k+band*3,CELL),ly=k+band*3-i*CELL;
 /* the cell's object; drawn again when the cell above drew the same one (no rows of the same stones) */
 uint32_t h=hash4(w->seed,side,band,i);int o=pick(band,h);
 if(o==pick(band,hash4(w->seed,side,band,i-1))){h=hash4(w->seed^0x2545f491u,side,band,i);o=pick(band,h);}
 if(o<O_COUNT){int ox=(int)((h>>12)%(uint32_t)(cw-obj_w[o]+1)),oy=(int)((h>>18)%(uint32_t)(CELL-obj_h[o]+1));
  if(lx>=ox && lx<ox+obj_w[o] && ly>=oy && ly<oy+obj_h[o])return obj_first(o)+(ly-oy)*obj_w[o]+lx-ox;}
 uint32_t s=hash4(w->seed^0x51edu,c,k,band);
 if((int)(s%100)<(band?8:16))return L_TUFT+(int)((s>>8)%4);
 return L_FLAT;
}
static void land(const world*w){
 int base=HERO_LINE+w->scroll,mw=rs_bg_map_w(RS_BG3);
 rs_bg_scroll(RS_BG3,0,((-base)%256+256)%256);
 /* all 32 rows of the map (the screen shows 31 of them): the map depends only on the scroll (save states) */
 int k0=fdiv(-base,8),c0=fdiv(rs_screen_left(),8),c1=fdiv(rs_screen_right()-1,8);
 for(int k=k0;k<k0+32;k++)for(int c=c0;c<=c1;c++)rs_bg_put(RS_BG3,c&(mw-1),k&31,RS_MAP(land_tile(w,c,k),1,0,0,0));
}
/* the layers for the screen's width: at start-up and after a state (perhaps saved at another width) was loaded */
void draw_layout(const world*w){
 int sw=rs_screen_width(),mw=sw>512?128:64;
 if(rs_bg_map_w(RS_BG1)!=mw)rs_bg_setup(RS_BG1,mw,32,VR_BG1);  /* the UI layer: as wide as the screen */
 if(rs_bg_map_h(RS_BG2)!=(tall_screen()?TALL_ROWS:32))rs_bg_setup(RS_BG2,64,tall_screen()?TALL_ROWS:32,VR_BG2);   /* (a tall screen) */
 if(sw<=RS_SCREEN_W){rs_bg_enable(RS_BG3,0);return;}
 rs_bg_setup(RS_BG3,mw,32,VR_LAND);rs_bg_enable(RS_BG3,1);land_tiles();
 if(w)land(w);
}
static void scene(const world*w){int first=course_index(w->scroll-120);for(int id=first>0?first-1:0;id<=first+4;id++){river_row r;course_row(w,id,&r);int visible=0;for(int p=0;p<w->players;p++)if(w->h[p].collected!=id+1 && w->h[p].next_row<=id+1)visible=1;if(visible&&r.fish>=0)spr(SPR_FISH,r.fish-8,world_y(w,r.distance)-8,-1);}
 for(int p=w->players-1;p>=0;p--){const hero*h=&w->h[p];if(h->state==HS_OFF)continue;int frame=h->state==HS_HIT||h->state==HS_DOWN?SPR_HERO_HIT:SPR_HERO_IDLE+(w->t/9)%2;
  if(h->state==HS_DOWN && h->t>14 && (h->t&2))continue;
  spr(frame,h->x-16,player_y(w->players,p)-16,hu_player_pal(p));}
}
/* the text needs the row above it (accents, wide glyphs): any character beyond ASCII */
static int tall(const char*s){for(;*s;s++)if((unsigned char)*s>=128)return 1;return 0;}
/* the column of a title line: its English column x0, or the centred one when that is further left (a longer text;
 * lead: columns before the text, the prompt's A glyph) */
static int left_x(const char*s,int x0,int lead){int c=(40-hu_width(s,0)-lead)/2+lead;return c<x0?(c<lead?lead:c):x0;}
static void title(const world*w,int t,int best){
 hu_logo("OTTER RAPIDS",ramps,2,1,0);
 /* The lines under the logo: the hint, the join line, the prompt (after its A glyph), the goal. Each starts at its
  * English column, or further left (centred) when it is longer. A line with accents or wide glyphs takes the row
  * above it too: then the lines move apart (the prompt always has a free row above, for its A glyph), up to the
  * logo's empty bottom row (8) so that the goal stays above the first player's tag (row 15 with 4 players). */
 int n=hu_players();char b[160],who[64]="",prompt[128];
 for(int p=n;p<4;p++){size_t k=strlen(who);if(p>n)k+=(size_t)snprintf(who+k,sizeof who-k," / ");if(k<sizeof who)rs_strf(who+k,sizeof who-k,HTR(PLAYER_TAG),p+1);}
 if(n<4)rs_strf(b,sizeof b,n>1?TR(JOIN_LEAVE_SHORT):TR(JOIN_SHORT),who);else rs_strf(b,sizeof b,"%s",TR(FULL_LEAVE));
 rs_strf(prompt,sizeof prompt,HTR(PRESS_TO),TR(VERB));
 const char*hint=TR(HINT_MOVE),*goal=TR(HINT_GOAL);
 int r_hint=9,r_join=r_hint+1+tall(b),r_prompt=r_join+2,r_goal=r_prompt+1+tall(goal),up=r_goal>14?r_goal-14:0;
 r_hint-=up;r_join-=up;r_prompt-=up;r_goal-=up;
 hu_text(left_x(hint,5,0),r_hint,hint);
 hu_text(left_x(goal,7,0),r_goal,goal);
 if(hu_blink(t)){int x=left_x(prompt,9,3);hu_text(x,r_prompt,prompt);hu_glyph(HU_BTN_A,x*8-24,r_prompt*8-4,0,3);}
 for(int p=0;p<n && p<4;p++){char tag[16];rs_strf(tag,sizeof tag,HTR(PLAYER_TAG),p+1);hu_text((w->h[p].x+16)/8,(player_y(n,p)-2)/8,tag);}
 if(best){char s[64];rs_strf(s,sizeof s,HTR(BEST_N),best);hu_text(1,25,s);}
 hu_text(hu_center(b,0),r_join,b);hu_copyright(29);
}
void draw_frame(const world*w,int state,int t,int best,int fresh,int paused){hu_clear();rs_oam_clear();rs_brightness(paused?9:15);
 /* a tall screen (DESIGN.md "Tall screen"): one otter's run fills the display's height, the view on the otter (the
  * sides cropped); the title, the panels and 2-4 otters show every column. The score at the screen's top (1 player:
  * sprites, centred in the columns shown; more: BG1's top rows pinned), the pause text in the columns shown. */
 int solo=w->players==1&&(state==ST_PLAY||state==ST_DEAD),hx=160,hy=8;bg1_sx=0;hud_pin=0;
 if(tall_screen()){
  /* the view halfway between the otter and the river's middle: the otter always in it, the bank by it in view */
  rs_screen_fill(solo);rs_screen_focus_x(solo?(w->h[0].x+160)/2+rs_screen_origin_x():-1);
  if(solo){hx=(rs_screen_visible_x0()+rs_screen_visible_x1())/2-rs_screen_origin_x();hy=rs_screen_top()+8;bg1_sx=160-hx;}
  else if(state==ST_PLAY||state==ST_DEAD)hud_pin=16;
  rs_bg_pin(RS_BG1,0,hud_pin,0);
 }
 foreground(w);
 if(rs_screen_width()>RS_SCREEN_W)land(w);
 if(state==ST_TITLE){title(w,t,best);scene(w);}else if(state==ST_PLAY||state==ST_DEAD){if(w->players==1)hu_number(w->h[0].score,hx,hy,3);else for(int p=0;p<w->players;p++){char b[48],tag[16];rs_strf(tag,sizeof tag,HTR(PLAYER_TAG),p+1);snprintf(b,sizeof b,"%s %d",tag,w->h[p].score);hu_text(p*10,1,b);}scene(w);if(paused)hu_big(hu_center(HTR(PAUSED),1),13,HTR(PAUSED),HU_BIG_FREE);
 }else{int slide=hu_slide_in(t,20,200);if(w->players==1){static const int med[4]=MEDAL_SCORES;hu_banner(4,HTR(GAME_OVER));hu_gameover_panel(1,fresh,w->h[0].score,0);hu_gameover_sprites(1,w->h[0].score,0,best,hu_medal_of(w->h[0].score,med),t,slide);hu_retry_line(t,RETRY_LOCK,HTR(BACK_TITLE));}else{int scores[4];for(int p=0;p<w->players;p++)scores[p]=w->h[p].score;hu_standing s;hu_rank(&s,w->players,scores,scores);hu_results_panel(&s,NULL);hu_results_sprites(&s,t,slide);for(int i=0;i<s.n;i++){int x,y;hu_results_icon_pos(&s,i,&x,&y);spr(SPR_HERO_ICON,x-8,y-8+slide,hu_player_pal(s.order[i]));}hu_retry_line_at(hu_results_retry_row(&s),t,RETRY_LOCK,HTR(BACK_TITLE));}rs_bg_scroll(RS_BG1,0,-slide);}
 if(state!=ST_OVER)rs_bg_scroll(RS_BG1,bg1_sx,0);
 rs_bg_scroll(RS_BG2,0,tall_screen()?-rs_screen_top()&(TALL_ROWS*8-1):0);rs_viewports(0,NULL,0);   /* (a tall screen: map row 0 at the top) */
 bg1_sy=state==ST_OVER?-hu_slide_in(t,20,200):0;   /* (a tall screen: raster_out, line 0) */
}
void draw_state(void){rs_state_var("draw.water",water,sizeof water);RS_STATE_RASTER(raster);}
