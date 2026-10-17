/* The full cavern is shared. Runtime ball/rope tiles over code-drawn background and heroes. */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
static const rs_color ramps[2][3]={{RS_RGB8(255,225,157),RS_RGB8(227,174,85),RS_RGB8(161,101,60)},{RS_RGB8(170,247,221),RS_RGB8(82,210,174),RS_RGB8(42,141,147)}};
static const rs_color field_pal[16]={0,RS_RGB8(29,29,55),RS_RGB8(255,238,194),RS_RGB8(246,255,248),RS_RGB8(255,181,210),RS_RGB8(222,99,157),RS_RGB8(117,53,119),RS_RGB8(174,246,205),RS_RGB8(74,193,155),RS_RGB8(35,109,121),RS_RGB8(168,220,254),RS_RGB8(91,156,232),RS_RGB8(57,80,154),RS_RGB8(255,226,121),RS_RGB8(199,140,67),RS_RGB8(33,34,57)};
static rs_color lines[240];
/* A tall screen (rs_screen_tall: a phone held upright; DESIGN.md "Tall screen"): the arena needs every column (the
 * marbles bounce from wall to wall), so the app fits the width and the lines past the stage show the mine in cross
 * section, on BG4 (tiles drawn in code, tall_init): above, the meadow's sky (the backdrop, bluer toward the top), the
 * grass, the earth in two strata and the rock roof the cavern's pillars hold up; below, the floor's bricks once more,
 * then the bedrock with its gems (the app's touch buttons over it). Each cell takes one of a few variants by a hash,
 * so nothing repeats in stripes. The UI (but for the pinned HUD band), marbles' and cavern layers are hidden there
 * (window 1 over the whole width: their 32-row maps would repeat); BG4 is hidden on the stage; line 0 puts the
 * stage's settings back, so a 240-line screen is unchanged (BG4 is only set up on a tall one). */
#define T4 600                  /* BG4's tiles: VR_BG3 + T4 .. (after the cavern's and the ground's, wide or not) */
#define PAL_MINE 6
#define HUD_PIN_Y 4             /* play: BG1's top HUD_LINES (the player tags) at this screen line, the score too */
#define HUD_LINES 24
enum{M_BLANK,M_GRASS,M_EARTH_HI,M_EARTH_LO=M_EARTH_HI+4,M_ROCK=M_EARTH_LO+4,M_ROOF=M_ROCK+3,
     M_BED=M_ROOF+1,M_GEM=M_BED+3,M_TILES=M_GEM+2};
static int hud_pinned;          /* set by draw_frame before every picture (not saved) */
static int sky_lines;           /* the lines of sky at the screen's top (tall_init, from the height: a setting) */
static uint32_t mix(uint32_t a){a^=a>>16;a*=0x7feb352du;a^=a>>15;a*=0x846ca68bu;a^=a>>16;return a;}
static rs_color sky(int y){
 int top=rs_screen_top(),k=sky_lines>8?(y-top)*256/sky_lines:256,d=(y&4)?1:0;k=k>256?256:k;   /* the house dither */
 return RS_RGB8(84+(170-84)*k/256+2*d,140+(214-140)*k/256+2*d,214+(236-214)*k/256+2*d);
}
static void mine_tile(int id,uint8_t*t){
 for(int y=0;y<8;y++)for(int x=0;x<8;x++){
  uint32_t h=mix((uint32_t)(id*64+y*8+x)*2654435761u+7u);int c=0,n=(int)(h%16);
  if(id==M_GRASS){int blade=(x*5+3)%7;c=y<3?(y>=blade%3+1&&x%3!=1?2:0):y<5?(n<4?1:2):y<6?3:(n<5?4:5);}
  else if(id>=M_EARTH_HI&&id<M_EARTH_LO+4){int lo=id>=M_EARTH_LO,v=(id-M_EARTH_HI)%4;c=lo?6:5;
   if(n==0)c=lo?5:4;else if(n==15)c=lo?11:6;
   if(v==1&&(x-4)*(x-4)+(y-4)*(y-4)*2<7)c=(x+y<7)?7:8;                       /* a pebble */
   if(v==2&&(y==((x*3)/4+2)%8||y==(x+5)/2))c=12;                              /* a root */
   if(v==3&&x>=5&&y<=2)c=lo?6:5;}
  else if(id>=M_ROCK&&id<M_ROOF+1){c=n<2?9:n<14?10:11;if(id==M_ROCK+1&&(x==y||x==y+1))c=11;
   if(id==M_ROCK+2&&y==5)c=9;
   if(id==M_ROOF&&y>=6)c=y==7?15:11;}
  else if(id>=M_BED&&id<M_GEM+2){c=n<2?10:n<15?11:15;if(id==M_BED+1&&y==3&&x>1)c=10;
   if(id>=M_GEM){int dx=x-3,dy=y-4,r=dx*dx+dy*dy;if(r<=4)c=id==M_GEM?13:14;if(dx==-1&&dy==-1)c=7;if(r==5)c=15;}}   /* a gem */
  t[y*8+x]=(uint8_t)c;}
}
static void tall_init(void){
 static const rs_color pal[16]={0,RS_RGB8(150,206,96),RS_RGB8(92,164,70),RS_RGB8(54,112,58),RS_RGB8(164,114,72),
  RS_RGB8(134,90,58),RS_RGB8(104,68,46),RS_RGB8(178,166,150),RS_RGB8(116,104,98),RS_RGB8(96,92,108),RS_RGB8(70,68,86),
  RS_RGB8(48,48,66),RS_RGB8(78,52,36),RS_RGB8(255,140,200),RS_RGB8(120,236,226),RS_RGB8(29,29,55)};
 uint8_t px[M_TILES*64];
 for(int i=0;i<M_TILES;i++)mine_tile(i,px+i*64);
 rs_tiles_load8(VR_BG3+T4,px,M_TILES);rs_pal_load(RS_PAL_BG(PAL_MINE),pal,16);
 rs_bg_setup(RS_BG4,64,64,VR_BG3);
 /* above: rows 0..29 (row 29 just over the stage; BG4 scrolled so map line = screen line + 240) */
 int above=-rs_screen_top(),rows=(above+7)/8,earth=rows-rows*3/10;            /* three tenths of sky */
 sky_lines=above-(earth-1)*8;   /* (down to the grass) */
 for(int r=0;r<30;r++)for(int x=0;x<64;x++){int k=29-r,e=M_BLANK;uint32_t h=mix((uint32_t)(r*64+x)*31u+1u);
  if(k<earth-1){e=k==0?M_ROOF:k<3?M_ROCK+(int)(h%3):(k<earth/2+(int)(h>>8&1)?M_EARTH_LO:M_EARTH_HI)+((h%8)<4?0:(int)(h%4));}
  else if(k==earth-1)e=M_GRASS;
  rs_bg_put(RS_BG4,x,r,RS_MAP(T4+e,PAL_MINE,0,0,0));}
 /* below: rows 32..63 (line 240 at row 32): the floor's bricks (the ground map's last two rows), the bedrock */
 int n=rs_screen_width()>RS_SCREEN_W?gm_hills_wide_tile_count:gm_bg3_tile_count;
 for(int r=32;r<64;r++)for(int x=0;x<64;x++){uint32_t h=mix((uint32_t)(r*64+x)*17u+5u);
  uint16_t e=r<34?(uint16_t)(gm_ground_map[(r-30)*64+x]+n):RS_MAP(T4+(h%23==0?M_GEM+(int)(h/23%2):M_BED+(int)(h%3)),PAL_MINE,0,0,0);
  rs_bg_put(RS_BG4,x,r,e);}
 rs_bg_enable(RS_BG4,1);
}
static void raster_out(int y){
 int top=rs_screen_top();
 if(y==top||y==RS_SCREEN_H){rs_window(0,rs_screen_left(),rs_screen_right());rs_bg_window(RS_BG2,RS_WIN1);rs_bg_window(RS_BG3,RS_WIN1);
  rs_bg_window(RS_BG4,0);rs_bg_scroll(RS_BG4,0,y==top?240:16);}
 rs_bg_window(RS_BG1,hud_pinned&&y>=top+HUD_PIN_Y&&y<top+HUD_PIN_Y+HUD_LINES?0:RS_WIN1);
 rs_pal_set(0,y<0?sky(y):lines[RS_SCREEN_H-1]);
}
static void raster(int y,void*user){(void)user;
 if(y<0||y>=RS_SCREEN_H){raster_out(y);return;}
 if(y==0&&rs_screen_height()>RS_SCREEN_H){rs_bg_window(RS_BG1,0);rs_bg_window(RS_BG2,0);rs_bg_window(RS_BG3,0);rs_bg_window(RS_BG4,RS_WIN1);rs_bg_scroll(RS_BG3,0,0);}
 rs_pal_set(0,lines[y]);}
/* the marbles' layer is drawn in pixels (stage coordinates), one screen wide: fb_w columns, stage x -fb_ox .. */
static int fb_w=320,fb_ox;
static uint8_t fbuf[RS_SCREEN_W_MAX*240];
static void pixel(uint8_t*p,int x,int y,int c){x+=fb_ox;if(x>=0&&x<fb_w&&y>=0&&y<240)p[y*fb_w+x]=(uint8_t)c;}
static void rect(uint8_t*p,int x0,int y0,int x1,int y1,int c){if(x0<-fb_ox)x0=-fb_ox;if(y0<0)y0=0;if(x1>fb_w-1-fb_ox)x1=fb_w-1-fb_ox;if(y1>239)y1=239;for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++)pixel(p,x,y,c);}
static void ellipse(uint8_t*p,int cx,int cy,int rx,int ry,int c){for(int y=-ry;y<=ry;y++)for(int x=-rx;x<=rx;x++)if(x*x*ry*ry+y*y*rx*rx<=rx*rx*ry*ry)pixel(p,cx+x,cy+y,c);}
static void spr(int id,int x,int y,int pal,int flip){const gm_sprite_def*d=&gm_spr[id];if(x<=rs_screen_left()-(int)d->w||x>=rs_screen_right()||y<=-(int)d->h||y>=240)return;rs_spr(x,y,d->tile,d->w,d->h,pal,2,flip?RS_SPR_HFLIP:0);}
/* Widescreen (DESIGN.md "Widescreen"): the stage stays centred (rs_screen_origin) and the arena is the whole screen
 * (main.c world_arena), its walls two timbers at the screen's edges (timbers()). BG3 becomes a 128-tile map of the
 * 1024-px cavern (its own tiles and palette, the ground's tiles after them) with a lantern, a mine cart and a ladder
 * as background art; the UI layer is 128 tiles wide; the marbles' layer (BG2) is drawn one screen wide. */
static void timber_init(void);
static int laid_w;     /* the width the maps were laid out for (a state of another width: draw_relayout) */
static void maps_setup(void){
 int wd=rs_screen_width()>RS_SCREEN_W,mw=wd?128:64,n=wd?gm_hills_wide_tile_count:gm_bg3_tile_count,cols=wd?128:40;
 rs_bg_setup(RS_BG1,mw,32,VR_BG1);rs_bg_setup(RS_BG2,rs_screen_width()>512?128:64,32,VR_BG2);rs_bg_setup(RS_BG3,mw,32,VR_BG3);
 rs_tiles_load(VR_BG3,wd?gm_hills_wide_tiles:gm_bg3_tiles,n);rs_tiles_load(VR_BG3+n,gm_bg2_tiles,gm_bg2_tile_count);
 rs_pal_load(RS_PAL_BG(PAL_HILLS),wd?gm_hills_wide_pal:gm_hills_pal,16);
 for(int y=0;y<26;y++)for(int x=0;x<cols;x++)rs_bg_put(RS_BG3,x,y,wd?gm_hills_wide_map[y*128+x]:gm_hills_map[y*64+x]);
 for(int y=0;y<4;y++)for(int x=0;x<cols;x++)rs_bg_put(RS_BG3,x,y+26,(uint16_t)(gm_ground_map[y*64+(x&63)]+n));
 laid_w=rs_screen_width();
}
/* After a state was loaded (rs_game.state_loaded): one saved at another width brings that width's maps; set this
 * width's up (every frame draws the UI and the marbles again). Same width: nothing. */
void draw_relayout(void){if(laid_w!=rs_screen_width())maps_setup();}
void draw_init(void){
 rs_screen_origin(rs_screen_margin());   /* widescreen: the 320-px arena centred, the cavern wider */
 hu_config c=hu_defaults();c.obj_tile=KIT_OBJ_TILE;c.obj_vram=VR_OBJ;c.obj_pal=3;hu_init(&c);
 maps_setup();
 rs_pal_load(RS_PAL_BG(PAL_GROUND),gm_ground_pal,16);rs_pal_load(RS_PAL_BG(4),field_pal,16);
 uint8_t zero[64]={0};rs_tiles_load8(VR_BG2,zero,1);
 for(int i=0;i<3;i++)rs_bg_enable(i,1);
 rs_obj_base(VR_OBJ);rs_tiles_load(VR_OBJ,gm_obj_tiles,gm_obj_tile_count);rs_pal_load(RS_PAL_OBJ(0),gm_obj_pals,48);rs_pal_load(RS_PAL_OBJ(4),gm_obj_pals+64,32);
 if(rs_screen_width()>RS_SCREEN_W)timber_init();
 for(int y=0;y<240;y++)lines[y]=RS_RGB8(26+y/40,30+y/40,50+y/30);
 if(rs_screen_height()>RS_SCREEN_H)tall_init();
 rs_raster(raster,NULL);
}
/* Widescreen: the arena is the whole screen; its walls are two squared mine timbers at the screen's edges (WALL px of
 * them in view; main.c keeps the marbles and moles inside), sprites on palette 7: an 8x64 strip with an iron band,
 * drawn at runtime (OBJ tiles TIMBER_TILE..+7, after the game's and the kit's sprites). */
#define TIMBER_TILE 1000
#define TIMBER_PAL 7
static void timber_init(void){
 static const rs_color pal[6]={0,RS_RGB8(150,104,62),RS_RGB8(112,74,44),RS_RGB8(78,50,34),RS_RGB8(97,114,132),RS_RGB8(60,65,85)};
 rs_pal_load(RS_PAL_OBJ(TIMBER_PAL),pal,6);
 uint8_t px[8*64];
 for(int y=0;y<64;y++)for(int x=0;x<8;x++)px[y*8+x]=(uint8_t)(y==30?4:y==31?5:x==0?1:x==7?3:2);
 rs_tiles_load8(VR_OBJ+TIMBER_TILE,px,8);
}
static void timbers(void){
 if(rs_screen_width()<=RS_SCREEN_W)return;
 int l=rs_screen_left()-2,r=rs_screen_right()-6;   /* 6 of the 8 px in view: the walls are 6 px in (main.c WALL) */
 for(int y=0;y<192;y+=64){rs_spr(l,y,TIMBER_TILE,8,64,TIMBER_PAL,2,0);rs_spr(r,y,TIMBER_TILE,8,64,TIMBER_PAL,2,RS_SPR_HFLIP);}
 rs_spr(l,192,TIMBER_TILE,8,16,TIMBER_PAL,2,0);rs_spr(r,192,TIMBER_TILE,8,16,TIMBER_PAL,2,RS_SPR_HFLIP);
}
static void ball_draw(uint8_t*p,const marble*b){
 int x=b->x/Q16_ONE,y=b->y/Q16_ONE,r=ball_radius(b->size),base=4+3*b->hue;
 /* Ground shadow locates every bounce without competing with the ball itself. */
 int shadow=3+r*(y-24)/(GROUND_Y-24);ellipse(p,x,GROUND_Y-2,shadow,2,15);
 ellipse(p,x,y,r,r,1);
 for(int yy=-r+1;yy<r;yy++)for(int xx=-r+1;xx<r;xx++)if(xx*xx+yy*yy<(r-1)*(r-1)){
  int c=xx+yy<-r/3?base:xx+yy>r/2?base+2:base+1;
  /* Subtle cut gemstone facets, not rectangular collision edges. */
  if(yy>r/3&&xx<0)c=base+2;
  if(b->grace && (b->grace/6)%2==0 && c==base+1)c=base;
  pixel(p,x+xx,y+yy,c);
 }
 rect(p,x-r/3-2,y-r/2,x-r/3+1,y-r/2+1,3);pixel(p,x-r/3-2,y-r/2+2,base);pixel(p,x-r/3+3,y-r/2-2,3);

}
static void field(const world*w){
 fb_w=rs_screen_width();fb_ox=rs_screen_origin_x();uint8_t*pixels=fbuf;memset(fbuf,0,(size_t)fb_w*240);
 for(int p=0;p<w->players;p++){
  const hero*h=&w->h[p];if(h->state==HS_RUN&&h->shot){int c=p==0?13:p==1?10:p==2?4:7,x=h->shot_x,y=h->shot_y;
   for(int yy=y+5;yy<GROUND_Y-10;yy++){pixel(pixels,x-1,yy,1);pixel(pixels,x,yy,c);pixel(pixels,x+1,yy,yy%4?2:c);}
   for(int yy=0;yy<6;yy++)rect(pixels,x-yy/2,y+yy,x+yy/2,y+yy,yy<2?3:14);
   pixel(pixels,x-3,y+6,1);pixel(pixels,x+3,y+6,1);
  }
 }
 for(int i=0;i<MAX_BALLS;i++)if(w->balls[i].active)ball_draw(pixels,&w->balls[i]);
 for(int i=0;i<MAX_FX;i++)if(w->fx[i].t){const fragment*f=&w->fx[i];if(f->t>8||w->t%2)rect(pixels,f->x,f->y,f->x+1,f->y+1,4+3*f->hue);}
 /* a marble about to drop: its oval on the ceiling */
 if(w->warning)for(int n=0;n<w->warning_count;n++){int x=w->incoming[n].x;ellipse(pixels,x,30,ball_radius(w->incoming[n].size)+3,3,14);}
 int next=1;
 int cols=(fb_w+7)/8;   /* BG2 is scrolled so that its column 0 is the screen's */
 for(int row=0;row<30;row++)for(int col=0;col<cols;col++){
  uint8_t tile[64];int ink=0;for(int y=0;y<8;y++)for(int x=0;x<8;x++){int bx=col*8+x;tile[y*8+x]=bx<fb_w?pixels[(row*8+y)*fb_w+bx]:0;ink|=tile[y*8+x];}
  int id=0;if(ink&&next<512){id=next++;rs_tiles_load8(VR_BG2+id,tile,1);}rs_bg_put(RS_BG2,col,row,RS_MAP(id,4,0,0,0));
 }
}
static void icon(int p,int x,int y,int t,void*user){(void)t;(void)user;spr(SPR_HERO_ICON,x-8,y-8,hu_player_pal(p),0);}
void draw_frame(const world*w,int state,int t,int best,int new_best,int paused){
 hu_clear();rs_oam_clear();rs_brightness(paused?9:15);field(w);
 /* a tall screen: the HUD at the screen's top in play (BG1's top HUD_LINES pinned, the score's digits there too) */
 hud_pinned=rs_screen_height()>RS_SCREEN_H&&(state==ST_PLAY||state==ST_DEAD);
 rs_bg_pin(RS_BG1,0,hud_pinned?HUD_LINES:0,HUD_PIN_Y);int hy=hud_pinned?rs_screen_top()+HUD_PIN_Y:0;
 for(int p=0;p<w->players;p++){const hero*h=&w->h[p];if(h->state==HS_DOWN&&h->t>70)continue;int id=h->state==HS_HIT||h->state==HS_DOWN?SPR_HERO_HIT:SPR_HERO_IDLE+(h->state==HS_RUN&&((w->t/8)%2));
  spr(id,h->x-16,GROUND_Y-31,hu_player_pal(p),h->facing<0);
 }
 timbers();   /* widescreen: the walls, behind the moles */
 int scores[4];for(int p=0;p<w->players;p++)scores[p]=w->h[p].score;hu_standing rank;hu_rank(&rank,w->players,scores,scores);int slide=state==ST_OVER?hu_slide_in(t,20,200):0;
 if(state==ST_TITLE){
  hu_logo("MOLE MARBLES",ramps,2,2,0);hu_title_draw(t,best);hu_title_sprites(t,icon,NULL);
  hu_text(hu_center(TR(HINT_CONTROLS),0),16,TR(HINT_CONTROLS));
 }else if(state==ST_PLAY||state==ST_DEAD){
  if(w->players==1)hu_number(scores[0],160,8+hy,3);
  else for(int p=0;p<w->players;p++){char s[48],tag[16];rs_strf(tag,sizeof tag,HTR(PLAYER_TAG),p+1);snprintf(s,sizeof s,"%s %d",tag,scores[p]);hu_text(p*10,1,s);}
  if(paused)hu_big(hu_center(HTR(PAUSED),1),13,HTR(PAUSED),HU_BIG_FREE);
 }else{
  if(w->players==1){static const int medals[4]=MEDAL_SCORES;hu_banner(4,HTR(GAME_OVER));hu_gameover_panel(1,new_best,scores[0],0);hu_gameover_sprites(1,scores[0],0,best,hu_medal_of(scores[0],medals),t,slide);hu_retry_line(t,RETRY_LOCK,HTR(BACK_TITLE));}
  else{hu_results_panel(&rank,NULL);hu_results_sprites(&rank,t,slide);for(int i=0;i<rank.n;i++){int x,y;hu_results_icon_pos(&rank,i,&x,&y);spr(SPR_HERO_ICON,x-8,y-8+slide,hu_player_pal(rank.order[i]),0);}hu_retry_line_at(hu_results_retry_row(&rank),t,RETRY_LOCK,HTR(BACK_TITLE));}
 }
 rs_bg_scroll(RS_BG1,0,-slide);rs_bg_scroll(RS_BG2,rs_screen_margin(),0);rs_bg_scroll(RS_BG3,0,0);
}
void draw_state(void){rs_state_var("draw.lines",lines,sizeof lines);rs_state_var("draw.laid_w",&laid_w,sizeof laid_w);RS_STATE_RASTER(raster);}

