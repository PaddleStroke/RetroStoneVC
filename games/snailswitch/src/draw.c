#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "house_tall.h"    /* a phone held upright: the title and the panels in the columns shown */
#include "game_text.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static int shown_state=-1,shown_best=-1,shown_x0=-1;
static rs_color line_col[240];
static const rs_color title_ramps[2][3]={
 {RS_HEX(0xffd2df),RS_HEX(0xee88bb),RS_HEX(0xab4b80)},
 {RS_HEX(0xddf4a4),RS_HEX(0xa9cf64),RS_HEX(0x5f954b)}};
/* A tall screen (a phone held upright; DESIGN.md "Tall screen"): rs_screen_zoom(ZOOM_H), the display's height filled
 * and its sides cropped; the stage TALL_ABOVE_NUM / 10 of the extra lines down; the lines past it are the raster's.
 * Above: the canopy goes on (BG3's lines TALL_CANOPY_Y .. + TALL_CANOPY, mirrored up from the stage's edge, then down:
 * no seam), darker toward the top (TALL_CANOPY_DARK), the HUD over it (the scores' digits; 3-4 players: BG1's top
 * HUD_LINES pinned there, the P1..P4 tags); below: a forest pool, the forest and the vine mirrored in it (BG3 and BG2
 * read upward from the stage's last line, their palettes tinted toward TALL_WATER, a ripple shifting every 4 lines of
 * 8). The UI is hidden there (window 1 over the whole width), and so are the sprites below the stage. Line 0 puts the
 * stage's settings back: the windows and the palettes always (a state saved on a tall screen may hold the margins'
 * settings), BG2's and BG3's scroll on a tall screen. tall_sx, tall_fx / tall_fy, hud_lines: BG3's scroll x, BG2's scroll
 * and the HUD's band of this frame, set by draw_frame before every picture. */
#define TALL_ABOVE_NUM 3        /* the stage's origin: 3/10 of the extra lines above it (96 at 560), 7/10 of pool */
#define HUD_LINES 32            /* 3-4 players: BG1's rows 0..3 (the tags of hu_score_tags(players, HUD_LOW_Y)) */
#define HUD_LOW_Y 20            /* 3-4 players: P3 and P4's chips under P1 and P2's, in the HUD at the top */
#define HUD_Y 6                 /* the HUD's band: this many lines below the screen's top (clear of the corner buttons) */
#define TALL_CANOPY_Y 0
#define TALL_CANOPY 64
#define TALL_WATER RS_HEX(0x34575a)
#define TALL_CANOPY_DARK RS_HEX(0x2e4a3c)
#define TALL_SHADE 120          /* the canopy's shade at the screen's top (of 256) */
static int tall_sx,tall_fx,tall_fy,hud_lines,hud_y;   /* hud_y: the pinned band's first screen line */
static const rs_color ground_pal[16]={0,RS_HEX(0x23352f),RS_HEX(0x466947),RS_HEX(0x619248),
 RS_HEX(0x94bd59),RS_HEX(0xd5e594),RS_HEX(0x573343),RS_HEX(0xa65162),RS_HEX(0xe88189),
 RS_HEX(0xffc6a7),RS_HEX(0x805034),RS_HEX(0xc08038),RS_HEX(0xf2ba50),
 RS_HEX(0xfbf0c6),RS_HEX(0x384d44),RS_HEX(0x63856a)};
static int tall(void){return rs_screen_height()>RS_SCREEN_H;}
static void raster_out(int y){
 int top=rs_screen_top();
 if(y==top||y==RS_SCREEN_H){
  rs_window(0,rs_screen_left(),rs_screen_right());
  rs_bg_window(RS_BG1,RS_WIN1);rs_bg_window(RS_BG2,y==top?RS_WIN1:0);
  rs_obj_window(y==top?0:RS_WIN1);
 }
 if(hud_lines&&(y==top+hud_y||y==top+hud_y+hud_lines))rs_bg_window(RS_BG1,y==top+hud_y?0:RS_WIN1);   /* the HUD band */
 if(y<0){   /* the canopy: the band read down, up, down... (no seam), darker toward the screen's top every 8 lines */
  int d=-1-y,ph=d%(2*TALL_CANOPY),k=top<0?d*TALL_SHADE/-top:0;
  rs_bg_scroll(RS_BG3,tall_sx,TALL_CANOPY_Y+(ph<TALL_CANOPY?ph:2*TALL_CANOPY-1-ph)-y);
  if(y==top||(d&7)==7)for(int i=1;i<16;i++)rs_pal_set(RS_PAL_BG(PAL_HILLS)+i,hu_lerp_color(gm_hills_pal[i],TALL_CANOPY_DARK,k));
  rs_pal_set(0,hu_lerp_color(line_col[0],TALL_CANOPY_DARK,k));
  return;
 }
 if(y==RS_SCREEN_H)for(int i=1;i<16;i++){
  rs_pal_set(RS_PAL_BG(PAL_HILLS)+i,hu_lerp_color(gm_hills_pal[i],TALL_WATER,112));
  rs_pal_set(RS_PAL_BG(PAL_GROUND)+i,hu_lerp_color(ground_pal[i],TALL_WATER,176));
 }
 int m=2*RS_SCREEN_H-1-y;if(m<0)m=0;
 int ripple=((y+(int)(rs_frame_count()>>3))&7)<4?0:1;
 rs_bg_scroll(RS_BG3,tall_sx+ripple,m-y);rs_bg_scroll(RS_BG2,tall_fx+ripple,m-y);
 rs_pal_set(0,hu_lerp_color(line_col[m],TALL_WATER,112));
}
static void raster(int y,void*u){(void)u;
 if(y<0||y>=RS_SCREEN_H){raster_out(y);return;}
 if(y==0){
  rs_bg_window(RS_BG1,0);rs_bg_window(RS_BG2,0);rs_obj_window(0);rs_pal_load(RS_PAL_BG(PAL_HILLS),gm_hills_pal,16);
  rs_pal_load(RS_PAL_BG(PAL_GROUND),ground_pal,16);
  if(tall()){rs_bg_scroll(RS_BG3,tall_sx,0);rs_bg_scroll(RS_BG2,tall_fx,tall_fy);}
 }
 rs_pal_set(0,line_col[y]);}
/* Widescreen (DESIGN.md "Widescreen"): the maps are 64 tiles (512 px) wide up to 512 columns, 128 beyond. The misty
 * forest (BG3) is a 512-px panorama: on a 128-tile map its second half is the first one mirrored (the same tiles
 * h-flipped: no VRAM, seamless at both joins), so a screen wider than 512 never shows a stretch twice. The foreground
 * (BG2) is drawn for the screen's columns every frame; the UI layer (BG1) is as wide as the screen (corner tags). */
static int redraw_pause;                  /* transient: a state of another width was loaded (state_audit.txt) */
static int map_w(void){return rs_screen_width()>512?128:64;}
static void maps_setup(void){
 int mw=map_w();
 rs_bg_setup(RS_BG1,mw,32,VR_BG1);rs_bg_setup(RS_BG2,mw,32,VR_BG2);rs_bg_setup(RS_BG3,mw,32,VR_BG3);
 for(int y=0;y<HILLS_H;y++)for(int x=0;x<64;x++){
  uint16_t e=gm_hills_map[y*64+x];rs_bg_put(RS_BG3,x,y,e);
  if(mw>64)rs_bg_put(RS_BG3,127-x,y,(uint16_t)(e^RS_MAP_HFLIP));
 }
}
void draw_init(void){
 hu_config cfg=hu_defaults();cfg.obj_tile=KIT_OBJ_TILE;cfg.obj_vram=VR_OBJ;cfg.obj_pal=3;hu_init(&cfg);
 rs_tiles_load(VR_BG3,gm_bg3_tiles,gm_bg3_tile_count);
 rs_pal_load(RS_PAL_BG(PAL_HILLS),gm_hills_pal,16);
 /* widescreen: a third of the extra width behind the snails (game.h WIDE_LEFT): the stage origin; 0 at 320 */
 rs_screen_origin(WIDE_LEFT(rs_screen_width()-RS_SCREEN_W));
 /* a tall screen: the stage TALL_ABOVE_NUM / 10 of the extra lines down (the HUD and the canopy above, the pool below) */
 rs_screen_origin_v((rs_screen_height()-RS_SCREEN_H)*TALL_ABOVE_NUM/10);
 maps_setup();
 rs_pal_load(RS_PAL_BG(PAL_GROUND),ground_pal,16);
 rs_obj_base(VR_OBJ);rs_tiles_load(VR_OBJ,gm_obj_tiles,gm_obj_tile_count);
 rs_pal_load(RS_PAL_OBJ(0),gm_obj_pals,48);rs_pal_load(RS_PAL_OBJ(4),gm_obj_pals+64,32);
 for(int y=0;y<240;y++)line_col[y]=RS_RGB8(199-y/12,221-y/12,183-y/16);
 for(int l=0;l<3;l++)rs_bg_enable(l,1);
 rs_raster(raster,NULL);
}
/* A canvas: the pixels and their width (the screen's columns, rounded up to whole tiles; 320 at 4:3). */
typedef struct canvas {uint8_t*px;int w;} canvas;
static void pixel(const canvas*p,int x,int y,int c){if(x>=0&&x<p->w&&y>=0&&y<240)p->px[y*p->w+x]=c;}
static void rect(const canvas*p,int x0,int y0,int x1,int y1,int c){for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++)pixel(p,x,y,c);}
static void oval(const canvas*p,int cx,int cy,int rx,int ry,int a,int b,int c){
 for(int y=-ry-1;y<=ry+1;y++)for(int x=-rx-1;x<=rx+1;x++){
 int d=x*x*ry*ry+y*y*rx*rx;if(d>(rx+1)*(rx+1)*(ry+1)*(ry+1))continue;
 pixel(p,cx+x,cy+y,d>rx*rx*ry*ry?1:y<-ry/3?a:x<0?b:c);}}
static void line(const canvas*p,int x0,int y0,int x1,int y1,int r,int c){
 int n=abs(x1-x0)>abs(y1-y0)?abs(x1-x0):abs(y1-y0);if(!n)n=1;
 for(int i=0;i<=n;i++)rect(p,x0+(x1-x0)*i/n-r,y0+(y1-y0)*i/n-r,x0+(x1-x0)*i/n+r,y0+(y1-y0)*i/n+r,c);}
/* Widescreen (DESIGN.md "Widescreen"): the foreground is world content, drawn up to the canvas' right edge (cw
 * columns: the real screen's; draw.c gives it a camera from the screen's real left edge); the leaves and the hazards are culled against it, so a wider screen simply shows more
 * of the same vine. At 320 it is the 4:3 picture, pixel for pixel. */
void scenery_pixels(const world*w,uint8_t*out,int cw){
 canvas cv={out,cw},*px=&cv;
 memset(out,0,(size_t)cw*240);
 /* Every branch and obstacle shares the same world-space vine coordinate. */
 for(int x=0;x<cw;x++){int yy=vine_y(x+w->scroll);
 for(int y=-8;y<=8;y++)pixel(px,x,yy+y,abs(y)==8?1:y<-5?5:y<-2?4:y<4?3:2);
 if((x+w->scroll)%13==0)pixel(px,x,yy+3,4);}
 /* Ovate, asymmetrical leaves: curved contour, tapering tip and branching veins. */
 /* Include complete shapes whose roots are outside the view; clip only their pixels. */
 for(int cell=w->scroll/110-2;cell<=(w->scroll+cw+96)/110;cell++){
 unsigned r=(unsigned)cell*747796405u+2891336453u;r=(r^(r>>16))*2246822519u;
 int wx=cell*110+(int)(r%39)-19,x=wx-w->scroll,y=vine_y(wx),side=(r>>8)&1?1:-1;
 int dx=(r>>10)&1?14:-14,dy=side*(23+(r%12));
 line(px,x,y,x+dx,y+dy,2,1);line(px,x,y,x+dx,y+dy,1,3);
 int len=24+(r>>13)%10,tilt=((r>>17)&1?1:-1)*(10+(r>>19)%9),lx=x+dx,ly=y+dy;
 for(int u=0;u<=len;u++){
  static const int contour[11]={0,5,8,9,9,8,7,5,3,1,0};
  int step=u*10/len,fraction=u*10%len;
  int width=step>=10?0:contour[step]+(contour[step+1]-contour[step])*fraction/len;
  int bend=(curve_sine(u*512/len)*1)>>15;
  for(int v=-width-1;v<=width+1;v++){
   int xx=lx+(dx<0?-u:u),yy=ly+u*tilt/len+bend+v;
   pixel(px,xx,yy,abs(v)>width?1:v<-width/3?5:v<width/2?4:3);
  }
 }
 int direction=dx<0?-1:1;
 for(int u=2;u<len;u++){
  int bend=(curve_sine(u*512/len)*1)>>15;
  pixel(px,lx+direction*u,ly+u*tilt/len+bend,2);
  if(u%7==0){int width=(curve_sine(u*512/len)*6)>>15;
   line(px,lx+direction*u,ly+u*tilt/len+bend,lx+direction*(u+3),ly+u*tilt/len+bend-width,0,4);
   line(px,lx+direction*u,ly+u*tilt/len+bend,lx+direction*(u+3),ly+u*tilt/len+bend+width,0,2);}
 }
 }
 /* The course (DESIGN.md "The course"): its hazards and fruits, each on its side, culled at the canvas' edges (the
  * course buffer holds them from 160 px behind the scroll to 800 px ahead: every width's picture is covered). */
 for(int i=0;i<w->nitems;i++){
 const item*it=&w->it[i];int s=it->side?1:-1;
 if(it->kind==IT_FRUIT){
  /* a fruit stays drawn until every snail still running has caught it */
  int bx=it->x-w->scroll,by=vine_y(it->x),taken=1;
  if(bx<-16||bx>cw+16)continue;
  for(int p=0;p<w->players;p++)if(w->h[p].state==HS_RUN||w->h[p].state==HS_READY)taken &= w->h[p].berry>=it->id;
  if(taken)continue;
  line(px,bx,by,bx,by+s*16,0,2);
  oval(px,bx,by+s*21,6,6,9,8,7);
  line(px,bx-3,by+s*16,bx,by+s*18,0,2);line(px,bx+3,by+s*16,bx,by+s*18,0,2);
  pixel(px,bx-2,by+s*21-2,13);
  continue;
 }
 obstacle o;item_at(it,w->t,&o);int x=o.x-w->scroll,y=vine_y(o.x);
 if(x<-50||x>cw+128)continue;
 if(o.type==IT_THORN){
  /* Woody hooked thorns sprout directly from the vine, with shaded broad bases. */
  for(int j=0;j<3;j++){
   int tx=x+(j-1)*10,height=j==1?19:14;
   for(int k=0;k<height;k++){
    int center=tx+k*k/95,width=(height-k)*4/height;
    for(int q=-width-1;q<=width+1;q++)pixel(px,center+q,vine_y(center+q+w->scroll)+s*(7+k),abs(q)>width?1:q<0?12:q<2?11:10);
   }
  }
 }else if(o.type==IT_CATER){
  /* Two inchworm poses: extended / arched. Face follows its actual patrol direction. */
  int phase=(w->t/12+it->phase)%2,dir=o.dir;
  for(int j=0;j<5;j++){
   int tx=x+dir*(j*6-12),arch=phase?(j==2?5:j==1||j==3?3:0):0;
   oval(px,tx,y+s*(12+arch),4,5,5,4,3);
   pixel(px,tx,y+s*(11+arch),2);
   if(!arch){line(px,tx-1,y+s*16,tx-2,y+s*9,0,1);}
  }
  int hx=x+dir*15,hy=y+s*14;
  oval(px,hx,hy,6,6,5,4,3);
  rect(px,hx+dir*2-1,hy+s*2-1,hx+dir*2+1,hy+s*2+1,13);pixel(px,hx+dir*3,hy+s*2,1);
  pixel(px,hx+dir*5,hy,1);
  line(px,hx+dir*2,hy+s*5,hx+dir*4,hy+s*9,0,1);
  pixel(px,hx+dir*4,hy+s*9,12);
 }else{
  /* Sticky amber droplet: narrow neck, rounded teardrop and long wet glint. */
  for(int k=7;k<=32;k++){
   int width=1;
   if(k>=15){width=0;while((width+1)*(width+1)+(k-24)*(k-24)<=64)width++;}
   if(k>=15 && (k-24)*(k-24)>64)continue;
   for(int q=-width-1;q<=width+1;q++)pixel(px,x+q,y+s*k,abs(q)>width?1:q<-2?12:q<2?11:10);
  }
  line(px,x-3,y+s*20,x-3,y+s*25,0,13);pixel(px,x-2,y+s*16,13);
 }
 }
}
static uint8_t fg_px[RS_SCREEN_W_MAX*240];     /* scratch: redrawn in full each frame (state_audit.txt) */
/* The foreground is drawn for the screen's columns from its real left edge: the canvas' column 0 is stage x
 * rs_screen_left() (widescreen: a third of the extra width behind the snails; 0 at 320), the camera scroll + that. */
static void foreground(const world*w){
 int cols=(rs_screen_width()+7)/8,pw=cols*8;   /* the screen's tile columns: 40 at 4:3, 54 at 427, 65 at 520 */
 world cam=*w;cam.scroll+=rs_screen_left();
 scenery_pixels(&cam,fg_px,pw);
 int next=1;uint8_t empty[64]={0};rs_tiles_load8(VR_BG2,empty,1);
 for(int r=0;r<30;r++)for(int c=0;c<cols;c++){
  uint8_t tile[64];int ink=0;for(int y=0;y<8;y++)for(int x=0;x<8;x++){tile[y*8+x]=fg_px[(r*8+y)*pw+c*8+x];ink|=tile[y*8+x];}
  int id=0;if(ink){id=next++;rs_tiles_load8(VR_BG2+id,tile,1);}rs_bg_put(RS_BG2,c,r,RS_MAP(id,PAL_GROUND,0,0,0));
 }
}
static int player_pal(int p){return p?hu_player_pal(p):0;}
static void spr(int id,int x,int y,int prio,int pal){
 const gm_sprite_def*d=&gm_spr[id];rs_spr(x,y,d->tile,d->w,d->h,pal<0?d->pal:pal,prio,0);}
static int p1_drawn_x=-999;   /* tests (draw_view_log): the screen x of P1's sprite centre, as drawn */
static void draw_heroes(const world*w,int t,int sx,int sy){
 for(int p=0;p<w->players;p++){const hero*h=&w->h[p];if(h->state==HS_OFF)continue;
 int id=h->state==HS_HIT||h->state==HS_DOWN?SPR_HERO_HIT:h->flip?SPR_HERO_SQUASH:SPR_HERO_IDLE+(t/10)%2;
 const gm_sprite_def*d=&gm_spr[id];
 int flip=h->flip?(h->flip<11?h->side:h->from):h->side;
 int cx=h->x+(h->flip?(curve_sine((22-h->flip)*512/22)*9>>15):0);
 /* The same full-size snail grips each surface; the eye stalks hang below on the underside. */
 int n=rs_oam_next();
 rs_spr(cx-16+sx,(h->y>>16)-16+sy,d->tile,d->w,d->h,player_pal(p),2,flip?RS_SPR_VFLIP:0);
 if(p==0)p1_drawn_x=rs_oam_next()>n?rs_oam(n)->x+16+rs_screen_origin_x():-999;
 }}
/* the kit gives UI coordinates: + hu_ui_x() (the UI is centred on a wide screen, whatever the stage origin) */
static void slot_icon(int p,int cx,int cy,int t,void*u){(void)t;(void)u;spr(SPR_HERO_ICON,cx-8+hu_ui_x(),cy-8,2,player_pal(p));}
static void standing(const world*w,hu_standing*s){int a[4];for(int p=0;p<w->players;p++)a[p]=w->h[p].score;hu_rank(s,w->players,a,a);}
/* A zoomed tall screen showing fewer than 320 columns (house_tall.h ht_narrow): the title and the panels laid out in
 * the columns shown (DESIGN.md "Tall screen"). The title's logo on BG1's rows 0 .. LOGO_ROWS - 1, SNAIL over SWITCH,
 * centred in the columns shown, the hint under it: those rows pinned over the canopy above the stage (rs_bg_pin,
 * draw_frame), clear of the vine and of the corner buttons; the slots from row TITLE_SLOTS, under the snails. */
#define TITLE_SLOTS 18
#define LOGO_ROWS 12
#define LOGO_SPLIT 19           /* hu_logo("SNAIL SWITCH") at 4x: SNAIL in UI columns 3..17, SWITCH in 19..36 */
/* the kit's logo (its colours: SNAIL pink, SWITCH green), its words' tiles moved by whole columns to two lines,
 * each centred in the columns shown (centred at x 84 and 226 in the kit's one-line logo) */
static void title_logo(void){
 uint16_t m[6][40];
 hu_logo("SNAIL SWITCH",title_ramps,2,0,0);
 for(int y=0;y<6;y++)for(int x=0;x<40;x++)m[y][x]=rs_bg_get(RS_BG1,x,y);
 uint16_t blank=m[0][0];
 hu_clear_rows(0,8);
 int mid=ht_mid_x(),d1=ht_fdiv8(mid-84+4),d2=ht_fdiv8(mid-226+4);
 for(int y=0;y<6;y++)for(int x=0;x<40;x++){
  int second=x>=LOGO_SPLIT,nx=x+(second?d2:d1);
  if(m[y][x]!=blank&&nx>=0&&nx<40)rs_bg_put(RS_BG1,nx,y+(second?6:0),m[y][x]);
 }
}
/* ht_gameover_panel with the panel wide enough for the retry line too (ht_over_w leaves it out); its labels */
static void over_panel(int new_best,const char*retry){
 int w=ht_over_w(new_best,NULL),rw=hu_width(retry,0)+2,cols=ht_ncols(),r=ht_gameover_retry_row(NULL);
 if(rw>w)w=rw>cols?cols:rw;
 int x=ht_center_w(w),bottom=r+ht_text_rows(retry,1)+1;
 hu_panel(x,9,w,bottom-9+1);
 const char*lab[3]={hu_str(HS_SCORE),hu_str(new_best?HS_NEW_BEST:HS_BEST),hu_str(HS_MEDAL)};
 for(int i=0;i<3;i++)hu_box_text(ht_center(lab[i],0),ht_over_label_row(i),lab[i]);
}
/* returns the last row of the zoomed title's hint (0: none) */
static int screen_text(const world *w, int state, int st_t, int best, int new_best, const hu_standing *rs,
                       ht_title *tc, ht_title_out *to)
{
    int screen = state == ST_DEAD ? ST_PLAY : state, narrow = ht_narrow(), x0 = rs_screen_visible_x0();
    if (screen != shown_state || (state == ST_OVER && st_t == RETRY_LOCK) || best != shown_best || x0 != shown_x0) {
        hu_clear();
        if (state == ST_TITLE && narrow) title_logo();                   /* pinned: draw_frame */
        else if (state == ST_TITLE) hu_logo("SNAIL SWITCH", title_ramps, 2, 2, 0);
        if (screen == ST_PLAY && w->players >= 3) hu_score_tags(w->players, tall() ? HUD_LOW_Y : 0);    /* P1..P4 by the corner chips */
        if (state == ST_OVER && narrow) {
            if (w->players == 1) {
                ht_banner(4, HTR(GAME_OVER));
                over_panel(new_best, HTR(BACK_TITLE));
            } else {
                ht_results_panel(rs, NULL);
            }
        } else if (state == ST_OVER) {
            if (w->players == 1) {
                hu_banner(4, HTR(GAME_OVER));
                hu_gameover_panel(1, new_best, w->h[0].score, 0);
            } else {
                hu_results_panel(rs, NULL);                                     /* P2 WINS! and the ranking */
            }
        }
        shown_state = screen;
        shown_best = best;
        shown_x0 = x0;
    }
    if (state == ST_TITLE && narrow) {
        /* the hint wrapped under the canopy's logo, then the slots, the prompt, BEST, the join line, the credits */
        const char *goal = TR(HINT_GOAL);
        int r = ht_text(LOGO_ROWS + 1, TR(HINT_MOVE), 0) + 1, last;
        if (hu_tall(goal)) r++;
        last = r = ht_text(r, goal, 0);
        tc->verb = HTR(PLAY); tc->max_players = MAX_PLAYERS; tc->top = r + 2 > TITLE_SLOTS ? r + 2 : TITLE_SLOTS;
        tc->prompt_row = 0;
        ht_title_draw(tc, st_t, best, to);
        return last;
    }
    if (state == ST_OVER && narrow) {
        ht_retry_line_at(w->players == 1 ? ht_gameover_retry_row(NULL) : hu_results_retry_row(rs), st_t, RETRY_LOCK,
                         HTR(BACK_TITLE));
        return 0;
    }
    /* the title, every frame: the prompt (blinking), the player slots, BEST, the join line, the credits */
    if (state == ST_TITLE) {
        /* the hint's two lines; line 2 one row lower when it has accents or tall glyphs (they take the row above) */
        const char *goal = TR(HINT_GOAL);
        int row = 10;
        for (const char *c = goal; *c; c++) if ((unsigned char)*c >= 0x80) row = 11;
        hu_title_draw(st_t, best);
        hu_text(hu_center(TR(HINT_MOVE), 0), 9, TR(HINT_MOVE));
        hu_text(hu_center(goal, 0), row, goal);
    }
    if (state == ST_OVER) {
        if (w->players == 1) hu_retry_line(st_t, RETRY_LOCK, HTR(BACK_TITLE));
        else hu_retry_line_at(hu_results_retry_row(rs), st_t, RETRY_LOCK, HTR(BACK_TITLE));
    }
    return 0;
}

/* The view (a tall screen, DESIGN.md "Tall screen"): ONE zoomed view for the play, the title and the panels, the
 * columns shown at the stage's right edge (the things ahead come in there, as at 4:3: every snail sees as far ahead).
 * When a snail would not fit in them with a margin behind (more players, a less tall display), the play shows every
 * column (the warning ahead stays that of 4:3), and the title pans left to show every joined snail, still zoomed. */
static void view(const world*w,int state){
 int play=state==ST_PLAY||state==ST_DEAD;
 if(!tall()){if(!play)rs_screen_fill(0);return;}
 int back=RS_SCREEN_W;for(int p=0;p<w->players;p++)if(w->h[p].x-24<back)back=w->h[p].x-24;
 rs_screen_fill(1);rs_screen_focus_x(RS_SCREEN_W);
 int x0=rs_screen_visible_x0();
 if(x0<=back)return;
 if(play)rs_screen_fill(0);
 else if(state==ST_TITLE)rs_screen_focus_x(back+(rs_screen_visible_x1()-x0)/2);
}
/* The play's scores: a tall screen's at its top, 3-4 players in two rows there (the tags pinned: BG1's top HUD_LINES).
 * A 240-line screen: the corner chips. */
static void tall_hud(const world*w){
 int sc[4];for(int p=0;p<w->players;p++)sc[p]=w->h[p].score;
 if(!tall()){hu_score_chips(w->players,sc,0);return;}
 int x0=rs_screen_visible_x0(),vw=rs_screen_visible_x1()-x0,top=rs_screen_top();
 hud_lines=w->players>=3?HUD_LINES:0;hud_y=HUD_Y;rs_bg_pin(RS_BG1,0,hud_lines,HUD_Y);
 if(w->players<=2){for(int p=0;p<w->players;p++)hu_number(sc[p],x0+vw*(2*p+1)/(2*w->players),top+12,3);return;}
 for(int p=0;p<w->players;p++){hu_chip c=hu_score_chip(w->players,p,HUD_LOW_Y);hu_number_at(sc[p],c.x,c.y+top+HUD_Y,c.align,3);}
}
void draw_frame(const world*w,int state,int st_t,int best,int new_best,int paused){
 static const int medals[4]=MEDAL_SCORES;
 int sx=hu_shake_x(),sy=hu_shake_y();hu_standing rank;standing(w,&rank);
 foreground(w);rs_bg_scroll(RS_BG2,rs_screen_origin_x()-sx,-sy);   /* the canvas from the screen's left edge */rs_bg_scroll(RS_BG3,w->scroll/4,0);tall_sx=w->scroll/4;
 view(w,state);
 int narrow=ht_narrow();ht_title tc={0};ht_title_out to={0};
 int hint=screen_text(w,state,st_t,best,new_best,&rank,&tc,&to);
 int slide=state==ST_OVER?hu_slide_in(st_t,20,200):0;
 rs_bg_scroll(RS_BG1,-hu_ui_x(),-slide);       /* the UI centred on the screen (widescreen) */
 if(redraw_pause&&paused)hu_pause(0,13);       /* the UI layer was set up again (a state of another width) */
 redraw_pause=0;hu_pause(paused,13);rs_oam_clear();
 if(paused&&narrow){hu_clear_rows(13,2);ht_big(13,HTR(PAUSED),HU_BIG_FREE);}   /* zoomed: centred in the columns shown */
 tall_fx=rs_screen_origin_x()-sx;tall_fy=-sy;
 int play=state==ST_PLAY||state==ST_DEAD;
 if(play)tall_hud(w);
 else if(state==ST_TITLE&&narrow){   /* the logo's and the hint's rows over the canopy (centred in it when they fit) */
  hud_lines=(hint+1)*8;hud_y=(-rs_screen_top()-hud_lines)/2;if(hud_y<0)hud_y=0;rs_bg_pin(RS_BG1,0,hud_lines,hud_y);
 }else{hud_lines=0;rs_bg_pin(RS_BG1,0,0,0);}
 if(state==ST_TITLE&&narrow)ht_title_sprites(&tc,&to,st_t,slot_icon,NULL);
 else if(state==ST_TITLE)hu_title_sprites(st_t,slot_icon,NULL);
 if(state!=ST_OVER)draw_heroes(w,w->t,sx,sy);
 if(state==ST_OVER&&narrow){
 if(w->players==1)ht_gameover_sprites(w->h[0].score,best,hu_medal_of(w->h[0].score,medals),st_t,slide);
 else{ht_results_sprites(&rank,st_t,slide);for(int i=0;i<rank.n;i++){int x,y;ht_results_icon_pos(&rank,i,&x,&y);spr(SPR_HERO_ICON,x-8+hu_ui_x(),y-8+slide,3,player_pal(rank.order[i]));}}
 }else if(state==ST_OVER){
 if(w->players==1)hu_gameover_sprites(1,w->h[0].score,0,best,hu_medal_of(w->h[0].score,medals),st_t,slide);
 else{hu_results_sprites(&rank,st_t,slide);for(int i=0;i<rank.n;i++){int x,y;hu_results_icon_pos(&rank,i,&x,&y);spr(SPR_HERO_ICON,x-8+hu_ui_x(),y-8+slide,3,player_pal(rank.order[i]));}}
 }}
/* Tests (--opt view=1, main.c at exit): the view of the last frame, "view: width=W left=L p1_x=X next_x=N": L the columns
 * shown behind the snails (the stage origin), X the screen x of P1's sprite centre as drawn, N the screen x of the next
 * hazard ahead of P1 (its anchor; a fruit when the course buffer holds no hazard ahead; tests/wide_test.sh: X and N - X
 * against 320) */
void draw_view_log(const world*w){
 int hx=w->scroll+w->h[0].x,next=w->dir.x,fruit=-1;
 for(int i=w->nitems-1;i>=0&&w->it[i].x>hx;i--){
  if(w->it[i].kind==IT_FRUIT)fruit=w->it[i].x;else next=w->it[i].x;
 }
 if(next==w->dir.x&&fruit>=0)next=fruit;
 rs_log("view: width=%d left=%d p1_x=%d next_x=%d",rs_screen_width(),rs_screen_origin_x(),p1_drawn_x,
        next-w->scroll+rs_screen_origin_x());
}
#define S(v) rs_state_var("draw." #v,&(v),sizeof(v))
void draw_state(void){S(line_col);S(shown_state);S(shown_best);S(shown_x0);RS_STATE_RASTER(raster);}
#undef S
/* A state saved at another width brings the maps of that width (64 or 128 tiles): set them up for this one (the
 * forest again; the foreground is redrawn every frame; the UI and PAUSED on the next frame). Same width: nothing. */
void draw_state_loaded(void){
 /* the foreground columns past this screen's (a 427 state at 320): empty, as at start-up (a shake shows one) */
 for(int r=0;r<32;r++)for(int c=(rs_screen_width()+7)/8;c<rs_bg_map_w(RS_BG2);c++)rs_bg_put(RS_BG2,c,r,0);
 if(rs_bg_map_w(RS_BG1)==map_w()&&rs_bg_map_w(RS_BG2)==map_w()&&rs_bg_map_w(RS_BG3)==map_w())return;
 maps_setup();
 shown_state=-1;                          /* screen_text clears the UI layer and draws it again */
 redraw_pause=1;                          /* ... and PAUSED, if it shows */
}

