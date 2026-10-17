/* Raccoon Recycler: fixed full-screen sorting station, all players share a visible item. */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const rs_color title_ramps[2][3]={{RS_RGB8(230,222,174),RS_RGB8(191,168,120),RS_RGB8(118,93,79)},{RS_RGB8(207,237,154),RS_RGB8(127,188,100),RS_RGB8(64,127,98)}};
/* Widescreen (DESIGN.md "Widescreen"): a screen wider than 320 centres the 320-px stage (rs_screen_origin) and shows
 * more of the yard on both sides: the ground layer is then the 128-tile wide map (make_art.py WIDE_L; its new tiles
 * after the classic ones). The bin lips (BG2) and the UI layer stay inside the stage: their 64-tile maps suffice. */
static int ground_map_w(void){return rs_screen_width()>RS_SCREEN_W?128:64;}
static void ground_tiles(void){rs_tiles_load(VR_BG2,gm_bg2_tiles,ground_map_w()>64?gm_bg2_wide_tile_count:gm_bg2_tile_count);}
static void ground_fill(void){
 int mw=ground_map_w();
 for(int y=0;y<GROUND_H;y++)for(int x=0;x<mw;x++)rs_bg_put(RS_BG3,x,y,mw>64?gm_ground_wide_map[y*128+x]:gm_ground_map[y*64+x]);
}
static void spr(int id,int x,int y,int prio,int pal){const gm_sprite_def *d=&gm_spr[id];rs_spr(x,y,d->tile,d->w,d->h,pal<0?d->pal:pal,prio,0);}
static int ppal(int p){return p?hu_player_pal(p):0;}
static void object(int item,int cx,int cy,int priority){spr(SPR_ITEM_0+item,cx-16,cy-16,priority,-1);}
/* Tall screen (DESIGN.md "Tall screen"): rs_screen_zoom(ZOOM_H) (main.c); the stage at the screen's bottom, the lines
 * above it the sky carried on from the stage's top colour, bluer toward the top, clouds drifting behind the conveyor
 * (BG4, code-drawn), and the conveyor rising to the screen's top (BG3's top row repeated in the belt's columns, window
 * 2; BG1 and BG2 hidden there, window 1, but for the pinned HUD band), its treads and the toys still to come already
 * on it (upcoming). Line 0 puts the stage's settings back: a 240-line screen never gets here. */
static int tall(void){return rs_screen_height()>RS_SCREEN_H;}
#define BELT_X0 137                 /* the belt's columns (make_art.py: ground) */
#define BELT_X1 184
#define HUD_PIN_Y 4                 /* play: BG1's top HUD_LINES (the player tags) at this screen line, the digits too */
#define HUD_LINES 24
#define PAL_CLOUD 7
#define CLOUD_TILE 64               /* BG4's tiles: the clouds after the bin lips' (VR_BG3) */
static int hud_pinned;              /* set by draw_frame before every picture (not saved) */
static rs_color stage_back;         /* CGRAM 0 on the stage (draw_init) */
static rs_color sky_line(int y){
 static const int lo[3]={222,236,216},hi[3]={150,198,226};
 int top=rs_screen_top(),k=top<0?(-y)*256/(-top):0,c[3],d=(y&4)?1:0;     /* house rule: a 2-line dither step */
 for(int i=0;i<3;i++)c[i]=(lo[i]*(256-k)+hi[i]*k)/256;
 return k?RS_RGB8(c[0]+2*d,c[1]+2*d,c[2]+2*d):RS_RGB8(lo[0],lo[1],lo[2]);
}
static void raster(int y,void*user){(void)user;
 if(!tall()||y>0)return;
 if(y==0){rs_bg_window(RS_BG1,0);rs_bg_window(RS_BG2,0);rs_bg_window(RS_BG3,0);rs_bg_scroll(RS_BG3,0,0);rs_pal_set(0,stage_back);return;}
 int top=rs_screen_top();
 if(y==top){rs_window(0,rs_screen_left(),rs_screen_right());rs_window(1,BELT_X0,BELT_X1);rs_bg_window(RS_BG2,RS_WIN1);rs_bg_window(RS_BG3,RS_WIN2_OUT);}
 rs_bg_window(RS_BG1,hud_pinned&&y>=top+HUD_PIN_Y&&y<top+HUD_PIN_Y+HUD_LINES?0:RS_WIN1);
 rs_bg_scroll(RS_BG3,0,((y%8)+8)%8-y);                 /* map lines 0..7: the sky and the belt */
 rs_pal_set(0,sky_line(y));
}
/* two puffy clouds, drawn in code: 8x3 and 5x2 tiles (BG4 at VR_BG3 + CLOUD_TILE, palette PAL_CLOUD) */
static void clouds_init(void){
 static const rs_color pal[4]={0,RS_RGB8(255,255,248),RS_RGB8(240,247,240),RS_RGB8(208,226,222)};
 static const int puffs[2][4][4]={{{14,15,11,7},{30,11,14,10},{46,14,11,8},{55,17,7,5}},{{11,9,9,6},{22,7,10,7},{31,10,7,5},{0,0,0,0}}};
 static const int dims[2][3]={{8,3,21},{5,2,13}};       /* tiles wide, high; the flat bottom's line */
 uint8_t px[(24+10)*64];
 memset(px,0,sizeof px);
 int base=0;
 for(int c=0;c<2;c++){int tw=dims[c][0],th=dims[c][1],bot=dims[c][2];
  for(int y=0;y<th*8;y++)for(int x=0;x<tw*8;x++){int col=0;
   for(int p=0;p<4;p++){const int*q=puffs[c][p];if(!q[2])continue;int dx=x-q[0],dy=y-q[1];
    if(dx*dx*q[3]*q[3]+dy*dy*q[2]*q[2]<=q[2]*q[2]*q[3]*q[3]&&y<=bot){int v=(dx<0&&dy*2<-q[3])?1:2;if(!col||v<col)col=v;}}
   if(col&&y>=bot-2)col=3;
   px[(base+(y/8)*tw+x/8)*64+(y%8)*8+x%8]=(uint8_t)col;}
  base+=tw*th;}
 rs_tiles_load8(VR_BG3+CLOUD_TILE,px,base);
 rs_pal_load(RS_PAL_BG(PAL_CLOUD),pal,4);
 rs_bg_setup(RS_BG4,64,32,VR_BG3);
 for(int y=0;y<32;y++)for(int x=0;x<64;x++)rs_bg_put(RS_BG4,x,y,0);
 static const int at[6][3]={{0,2,2},{1,15,7},{0,27,10},{1,44,3},{0,50,16},{1,6,19}};   /* cloud, map column, row */
 for(int i=0;i<6;i++){int c=at[i][0],tw=dims[c][0],th=dims[c][1],t0=CLOUD_TILE+(c?24:0);
  for(int y=0;y<th;y++)for(int x=0;x<tw;x++)rs_bg_put(RS_BG4,(at[i][1]+x)&63,at[i][2]+y,RS_MAP(t0+y*tw+x,PAL_CLOUD,0,0,0));}
 rs_bg_enable(RS_BG4,1);
}
/* the toys still to come, on the belt above the stage: the world played forward on a copy (every player routing
 * right, so the copy runs on), each new toy drawn where it is now (its spawn line less the belt's run until then) */
static void upcoming(const world *w,int top){
 world f=*w;
 for(int p=0;p<f.players;p++)f.h[p].alive=1;
 int run=0,need=(-20-top+16)*256;
 for(int n=0;n<600&&run<need;n++){
  int next=-1;for(int i=0;i<ITEM_MAX;i++)if(f.items[i].active&&(next<0||f.items[i].y>f.items[next].y))next=i;
  for(int p=0;p<f.players;p++)f.h[p].side=next>=0?item_side(&f,f.items[next].item):ROUTE_LEFT;
  int act[ITEM_MAX],y0[ITEM_MAX],press[4]={0,0,0,0};
  for(int i=0;i<ITEM_MAX;i++){act[i]=f.items[i].active;y0[i]=f.items[i].y;}
  if(!f.switch_t)run+=belt_speed(f.round);
  world_step(&f,press);
  for(int i=0;i<ITEM_MAX;i++)if(f.items[i].active&&f.items[i].y==-20*256&&(!act[i]||y0[i]!=-20*256)){
   int y=(-20*256-run)/256;if(y>top-32)object(f.items[i].item,160,y,2);}
 }
}
void draw_init(void){
 rs_screen_origin(rs_screen_margin());
 if(tall())rs_screen_origin_v(rs_screen_height()-RS_SCREEN_H);   /* a tall screen: the stage at the bottom, the sky above */
 hu_config hc=hu_defaults();hc.obj_tile=KIT_OBJ_TILE;hc.obj_vram=VR_OBJ;hc.obj_pal=3;hu_init(&hc);
 rs_bg_setup(RS_BG1,64,32,VR_BG1);rs_bg_setup(RS_BG2,64,32,VR_BG3);rs_bg_setup(RS_BG3,ground_map_w(),32,VR_BG2);
 ground_tiles();rs_tiles_load(VR_BG3,gm_bg3_tiles,gm_bg3_tile_count);
 rs_pal_load(RS_PAL_BG(PAL_GROUND),gm_ground_pal,16);rs_pal_load(RS_PAL_BG(PAL_HILLS),gm_hills_pal,16);
 ground_fill();
 for(int y=0;y<HILLS_H;y++)for(int x=0;x<64;x++)rs_bg_put(RS_BG2,x,y,gm_hills_map[y*64+x]|(1u<<13));
 for(int l=0;l<3;l++)rs_bg_enable(l,1);
 if(tall())clouds_init();
 stage_back=rs_pal_get(0);rs_raster(raster,NULL);
 rs_obj_base(VR_OBJ);rs_tiles_load(VR_OBJ,gm_obj_tiles,gm_obj_tile_count);rs_pal_load(RS_PAL_OBJ(0),gm_obj_pals,48);rs_pal_load(RS_PAL_OBJ(4),gm_obj_pals+64,32);
}
/* the bins: their 32-px badges at stage x BIN_X(b) (b 0..2: the left, centre and right branches' ends) */
#define BIN_X(b) (80+(b)*64)
/* Touch (DESIGN.md "Controls"): the branch of the bin nearest stage column x, the whole height (the screen in three
 * columns cut halfway between the bins' centres: the conveyor and its gate are the centre's, each margin a side's) */
int draw_route_at(int x){
 static const int route[3]={ROUTE_LEFT,ROUTE_CENTER,ROUTE_RIGHT};int best=0;
 for(int b=1;b<3;b++)if(abs(x-(BIN_X(b)+16))<abs(x-(BIN_X(best)+16)))best=b;
 return route[best];
}
void draw_frame(const world *w,int state,int st_t,int best,int new_best,int paused){
 hu_clear();rs_oam_clear();rs_brightness(paused?9:15);rs_bg_scroll(RS_BG2,0,0);rs_bg_scroll(RS_BG3,0,0);
 int score[4];for(int p=0;p<w->players;p++)score[p]=w->h[p].score;hu_standing rank;hu_rank(&rank,w->players,score,score);
 int slide=state==ST_OVER?hu_slide_in(st_t,20,200):0;rs_bg_scroll(RS_BG1,0,-slide);
 /* a tall screen: one player fills the display's height (zoomed, the sides cropped); the title, the panels and 2-4
  * players show every column; the HUD at the screen's top in play (BG1's tags pinned, the digits at the top) */
 int play=state==ST_PLAY||state==ST_DEAD,zoom=play&&w->players==1,top=rs_screen_top();
 hud_pinned=tall()&&play;rs_screen_fill(zoom);rs_bg_pin(RS_BG1,0,hud_pinned?HUD_LINES:0,HUD_PIN_Y);
 int hy=hud_pinned?top+HUD_PIN_Y:0,vx0=rs_screen_visible_x0()-rs_screen_origin_x(),vx1=rs_screen_visible_x1()-rs_screen_origin_x();
 if(tall())rs_bg_scroll(RS_BG4,(-(int)rs_frame_count()/6)&511,(-top)&255);   /* the clouds drift */
 /* Foreground signs and HUD get the first OAM slots, above all moving objects. */
 if(state==ST_PLAY||state==ST_DEAD)for(int p=0;p<w->players;p++){
  char label[32];const hero*h=&w->h[p];int cx=(p*2+1)*160/w->players;
  if(w->players>1){rs_strf(label,sizeof label,HTR(PLAYER_TAG),p+1);hu_text((cx-hu_width(label,0)*4)/8,0,label);}
  hu_number(h->score,cx,(w->players>1?10:2)+hy,3);
 }
 /* Bin badges are the complete rule: neutral animal silhouettes or plain colour swatches. */
 for(int b=0;b<3;b++){
  int badge=w->mode==RULE_SPECIES?SPR_BADGE_DUCK+w->bin_order[b]:SPR_BADGE_YELLOW+w->bin_order[b];
  spr(badge,BIN_X(b),197,3,-1);
  if(w->switch_t>0 && ((w->switch_t/6)&1))spr(SPR_LAMP,BIN_X(b)+12,178,3,-1);
 }
 // Same real workstations on title and during play; full-size raccoons stand on the platform.
 for(int p=0;p<w->players;p++){const hero *h=&w->h[p];int id=SPR_HERO_IDLE;
  if(h->feedback<0||!h->alive)id=SPR_HERO_HIT;else if(h->drop_t&&h->drop_side!=ROUTE_CENTER)id=h->drop_side<0?SPR_HERO_LEFT:SPR_HERO_RIGHT;
  int x=zoom&&h->x<vx0+18?vx0+18:zoom&&h->x>vx1-18?vx1-18:h->x;   /* (zoomed: on the columns shown) */
  spr(id,x-16,126,2,ppal(p));
 }
 /* A single fixed conveyor; each player's gate chooses a continuous outgoing route. */
 for(int i=0;i<ITEM_MAX;i++)if(w->items[i].active)object(w->items[i].item,160,w->items[i].y/256,2);
 if(top<0)upcoming(w,top);
 for(int p=0;p<w->players;p++){
  const hero *h=&w->h[p];
  if(h->drop_t){int k=ROUTE_FRAMES-h->drop_t;
   int along=k<20?k:20,side=h->drop_side==ROUTE_CENTER?0:h->drop_side;
   int xx=160+side*64*along/20;
   int yy=k<=20?BELT_JOIN+25*k/20:185+(k-20)*2;
   object(h->drop_item,xx+(p*2-w->players+1),yy,1);
  }
 }
 for(int p=0;p<w->players;p++)if(w->h[p].alive)spr(w->h[p].side==ROUTE_CENTER?SPR_GATE_CENTER:w->h[p].side<0?SPR_GATE_LEFT:SPR_GATE_RIGHT,144,BELT_JOIN-18+p*2,1,ppal(p));
 /* OAM order wins before sprite priority: items must be emitted before treads. */
 int y0=-12+(w->scroll/256%12);while(y0>top-12)y0-=12;      /* (a tall screen: up to its top) */
 for(int yy=y0;yy<BELT_JOIN;yy+=12)spr(SPR_BELT_MARK,146,yy,1,-1);
 for(int yy=y0;yy<BELT_JOIN;yy+=12)spr(SPR_BELT_MARK,166,yy,1,-1);
 if(state==ST_TITLE){
  hu_logo("RACCOON RECYCLER",title_ramps,2,2,0);hu_text(hu_center(TR(HINT),0),8,TR(HINT));
  for(int p=0;p<w->players;p++){char label[32];rs_strf(label,sizeof label,HTR(PLAYER_TAG),p+1);hu_text((w->h[p].x-hu_width(label,0)*4)/8,20,label);}
  /* the join line: the free player tags ("P2/P3/P4") in the line of the player count */
  char tags[96]="",join[160];int used[4]={1,0,0,0};for(int p=0;p<w->players;p++)used[hu_player_pad(p)]=1;
  int len=0;for(int p=1;p<4;p++)if(!used[p]){char tag[32];rs_strf(tag,sizeof tag,HTR(PLAYER_TAG),p+1);len+=snprintf(tags+len,sizeof tags-(size_t)len,"%s%s",len?"/":"",tag);}
  if(len)rs_strf(join,sizeof join,w->players>1?TR(JOIN_LEAVE):TR(JOIN),tags);else rs_strf(join,sizeof join,"%s",TR(FULL));
  char prompt[128];rs_strf(prompt,sizeof prompt,HTR(PRESS_TO),TR(VERB));
  hu_text(hu_center(join,0),22,join);int glyph=hu_prompt(10,prompt,st_t);if(hu_blink(st_t))hu_glyph(HU_BTN_A,glyph,77,3,0);hu_copyright(29);
  /* BEST on the top row; text with accents or tall glyphs one row lower (they take the row above) */
  if(best){char b[64];rs_strf(b,sizeof b,HTR(BEST_N),best);int row=0;for(const char *c=b;*c;c++)if((unsigned char)*c>=0x80)row=1;hu_text(hu_center(b,0),row,b);}
 }else if(state==ST_OVER){
  if(w->players==1){static const int medals[4]=MEDAL_SCORES;hu_banner(4,HTR(GAME_OVER));hu_gameover_panel(1,new_best,score[0],0);hu_gameover_sprites(1,score[0],0,best,hu_medal_of(score[0],medals),st_t,slide);hu_retry_line(st_t,RETRY_LOCK,HTR(BACK_TITLE));}
  else{hu_results_panel(&rank,NULL);hu_results_sprites(&rank,st_t,slide);for(int i=0;i<rank.n;i++){int xx,yy;hu_results_icon_pos(&rank,i,&xx,&yy);spr(SPR_HERO_ICON,xx-8,yy-8+slide,3,ppal(rank.order[i]));}hu_retry_line_at(hu_results_retry_row(&rank),st_t,RETRY_LOCK,HTR(BACK_TITLE));}
 }
 hu_pause(paused,13);
}
void draw_state(void){RS_STATE_RASTER(raster);}
/* After a state was loaded: the width is not in a state, and one saved at another width brings that width's ground
 * map; lay out this width's (static scenery: nothing else depends on the width). */
void draw_relayout(void){
 if(rs_bg_map_w(RS_BG3)==ground_map_w())return;
 rs_bg_setup(RS_BG3,ground_map_w(),32,VR_BG2);ground_tiles();ground_fill();
}
