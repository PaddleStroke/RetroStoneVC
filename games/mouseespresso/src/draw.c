/* Mouse Espresso: the replaceable café theme. Flight/collision rules are in play.c.
 * Widescreen (DESIGN.md "Widescreen"): a side-scroller, so the café is revealed wider: a third of the extra width behind
 * the mice, two thirds ahead (the stage origin stays 0; the shared camera starts WIDE_LEFT(width - 320) px further
 * left: the mice fly that much further right, the hazards, biscuits and counter are drawn behind and ahead of them
 * (the same course: course.c, painted by paint.c). The maps are 128 tiles wide; the café's 512-px back wall goes on
 * with its mirror image (the same tiles h-flipped: nothing repeats on a 560-px screen). The UI is centred (the UI
 * layer scrolls by -hu_ui_x(), the game's UI sprites add it). */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include "paint.h"
#include "house_tall.h"    /* a phone held upright: the title and the panels in the columns shown */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct view {int cx,mx,my,zx,zy;rs_color sky[240];} view;
static view V[4];static rs_viewport VP[4];static int nviews,shown_state=-1,shown_best=-1;
static int laid_w;                     /* the width the maps were laid out for (a state of another width: relayout) */
/* a phone held upright (DESIGN.md "Tall screen"): the zoomed screen, ZOOM_H lines (rs_screen_zoom: 231 columns shown on
 * a 1080x2520 phone, 249 on a 1080x2340 one), the stage low (TALL_BELOW lines of the counter's front under it), the
 * wall's upper storey (TALL_UPPER lines) above it, the ceiling's beam with the HUD at the screen's top (HUD_LINES);
 * the columns shown start TALL_BEHIND px behind the first mouse (the most of the course ahead) */
#define TALL_BELOW 40
#define TALL_UPPER 112
#define TALL_GAP 24                     /* planks between the upper storey and the stage */
#define HUD_LINES 44
#define TALL_BEHIND 32
static int tall(void) {return rs_screen_height()>RS_SCREEN_H;}
static int bg3_rows(void) {return tall()?64:32;}   /* a tall screen: the upper storey in rows 32..45 */
/* the furniture layer's pixels (foreground(): per-frame scratch, state_audit.txt), as wide as the widest screen */
#define PIX_W RS_SCREEN_W_MAX
static uint8_t pixels[PIX_W*240];
static int wide(void) {return rs_screen_width()>RS_SCREEN_W;}
/* widescreen: the columns the camera shows behind the mice, on the left (a third of the extra width, game.h) */
int draw_wide_left(void) {return WIDE_LEFT(rs_screen_width()-RS_SCREEN_W);}
static int p1_drawn_x;                 /* tests (draw_view_log): the screen x of P1's sprite centre, as drawn */
static const rs_color title_ramps[2][3]={
 {RS_HEX(0xffd8ba),RS_HEX(0xea9868),RS_HEX(0xa65951)},
 {RS_HEX(0xe1eddd),RS_HEX(0x9bc1bd),RS_HEX(0x567f91)}};
/* A tall screen (rs_screen_zoom: a phone held upright; DESIGN.md "Tall screen"): a taller café, zoomed. The stage sits
 * low (TALL_BELOW lines of the counter's front under it). Above it: the back wall's upper storey (BG3's map rows 32..45:
 * the wall's top 112 lines, windows and boards, half a period along: boards over the windows, windows over the boards),
 * then its planks (BG3's row 14, between the windows and the rail, repeated) up to the ceiling's beam at the screen's
 * top, which holds the HUD (BG1's top HUD_LINES pinned there while playing); the strings of the hanging things carried
 * up to the beam (BG2's line 0 repeated: only strings there); the wall a little darker toward the top. Below: the
 * counter's wooden front, its panels' grooves every 40 lines, darker with depth. The UI is hidden past the stage (but
 * the HUD); line 0 puts the stage's settings back (a 240-line screen: the same values, so the same pixels). */
static rs_color tall_scale(rs_color c,int k){   /* darker by k/256, rounded (soft steps) */
 return RS_RGB(((c&31)*k+128)/256,(((c>>5)&31)*k+128)/256,(((c>>10)&31)*k+128)/256);
}
#define TALL_PLANKS 112                 /* the back wall's row of planks alone (between the windows and the rail) */
static int hud_pinned;                  /* the HUD's lines pinned at the screen's top (draw_frame; not saved) */
/* a tall screen showing fewer than 320 columns: the title's logo in the UI rows LOGO_ROW.. (the band behind the mice
 * on the counter, else unused), pinned above the stage: logo_pin its first line (a stage line, < 0; 0: none) */
#define LOGO_ROW 18
static int logo_pin,logo_lines;         /* draw_frame, before every picture; not saved (logo_lines: its glyphs' lines) */
static void raster_out(int line){
 int top=rs_screen_top(),beam=top+HUD_LINES;
 if(logo_pin && line==logo_pin)rs_bg_enable(RS_BG1,1);
 if(logo_pin && line==logo_pin+logo_lines)rs_bg_enable(RS_BG1,0);
 if(line==top){rs_bg_enable(RS_BG1,hud_pinned);rs_bg_enable(RS_BG2,0);rs_bg_enable(RS_BG3,0);}
 if(line==beam && line<0){rs_bg_enable(RS_BG1,0);rs_bg_enable(RS_BG2,1);rs_bg_enable(RS_BG3,1);}
 if(line==RS_SCREEN_H)for(int l=0;l<3;l++)rs_bg_enable(l,0);
 if(line<0 && line<beam){               /* the ceiling's beam: dark wood, a lit edge, its shadow on the wall */
  int d=beam-line;
  rs_pal_set(0,d==1?RS_HEX(0x423a4b):d==2?RS_HEX(0xb48a62):d==3?RS_HEX(0x906b56):(line&4)?RS_HEX(0x5a4d53):RS_HEX(0x584449));
  return;
 }
 if(line<0){
  int k=top<0?-line*40/-top:0,u=line+TALL_UPPER+TALL_GAP,up=u>=0 && u<TALL_UPPER;
  rs_pal_set(0,tall_scale(V[0].sky[up?u:(line&4)?4:0],256-k));
  rs_bg_scroll(RS_BG2,0,-line);
  rs_bg_scroll(RS_BG3,VP[0].sx[2],up?32*8+u-line:TALL_PLANKS+(line&7)-line);
  return;
 }
 int d=line-RS_SCREEN_H,n=rs_screen_bottom()-RS_SCREEN_H,k=n>0?d*72/n:0,g=d%40,dd=(line&4)?4:0;
 rs_color c=d<3?RS_HEX(0x423a4b):g==38?RS_HEX(0x584449):g==39?RS_HEX(0xb48a62):RS_RGB8(144+dd,107+dd/2,86+dd);
 rs_pal_set(0,tall_scale(c,256-k));
}
static void raster(int line,void *user) {
 (void)user;
 if(line<0 || line>=RS_SCREEN_H){raster_out(line);return;}
 if(line==0){
  for(int l=0;l<3;l++)rs_bg_enable(l,1);
  rs_bg_scroll(RS_BG2,0,0);rs_bg_scroll(RS_BG3,VP[0].sx[2],VP[0].sy[2]);
 }
 int p=rs_viewport_current();if(p<0 || p>=nviews)p=0;
 int y=line-VP[p].y;if(y>=0 && y<240)rs_pal_set(0,V[p].sky[y]);
}
/* the maps of this width: 64 tiles at 320, 128 wider */
static void maps_setup(void) {
 int mw=wide()?128:64;
 rs_bg_setup(RS_BG1,mw,32,VR_BG1);rs_bg_setup(RS_BG2,mw,32,VR_BG2);rs_bg_setup(RS_BG3,mw,bg3_rows(),VR_BG3);
 laid_w=rs_screen_width();
}
/* After a state was loaded (rs_game.state_loaded): one saved at another width brings that width's maps; set this
 * width's up (each frame draws the whole screen again). Same width: nothing. */
void draw_relayout(void) {if(laid_w!=rs_screen_width() || rs_bg_map_h(RS_BG3)!=bg3_rows())maps_setup();}
void draw_init(void) {
 int below=rs_screen_height()-RS_SCREEN_H>2*TALL_BELOW?TALL_BELOW:(rs_screen_height()-RS_SCREEN_H)/2;
 if(tall())rs_screen_origin_v(rs_screen_height()-RS_SCREEN_H-below);   /* the stage low (DESIGN.md) */
 hu_config cfg=hu_defaults();cfg.obj_tile=KIT_OBJ_TILE;cfg.obj_vram=VR_OBJ;cfg.obj_pal=3;hu_init(&cfg);
 maps_setup();
 rs_tiles_load(VR_BG3,gm_bg3_tiles,gm_bg3_tile_count);
 uint8_t blank[64]={0};rs_tiles_load8(VR_BG2,blank,1);rs_tiles_load8(VR_BG3+gm_bg3_tile_count,blank,1);
 rs_pal_load(RS_PAL_BG(PAL_HILLS),gm_hills_pal,16);
 static const rs_color scenery[16]={0,RS_HEX(0x23233b),RS_HEX(0x486580),RS_HEX(0x8baabb),RS_HEX(0xd4e5e5),
  RS_HEX(0xfff2da),RS_HEX(0x584449),RS_HEX(0x906b56),RS_HEX(0xc39568),RS_HEX(0xe0b780),
  RS_HEX(0xf4ce7e),RS_HEX(0x956450),RS_HEX(0xd5a77a),RS_HEX(0xf49568),RS_HEX(0x5a4d53),RS_HEX(0x423a4b)};
 rs_pal_load(RS_PAL_BG(PAL_GROUND),scenery,16);
 rs_obj_base(VR_OBJ);rs_tiles_load(VR_OBJ,gm_obj_tiles,gm_obj_tile_count);
 rs_pal_load(RS_PAL_OBJ(0),gm_obj_pals,48);rs_pal_load(RS_PAL_OBJ(4),gm_obj_pals+64,32);
 for(int l=0;l<3;l++)rs_bg_enable(l,1);
 memset(V,0,sizeof V);memset(VP,0,sizeof VP);nviews=0;rs_raster(raster,NULL);
}
static int ppal(int p) {return p?hu_player_pal(p):0;}
static int floor8(int x) {return x>=0?x/8:(x-7)/8;}
static int sx(const view *v,int x) {return (x-v->cx)*v->zx/256;}
static void spr(int id,int x,int y,int priority,int pal,const rs_viewport *vp) {
 const gm_sprite_def *s=&gm_spr[id];
 if(x<=-(int)s->w || x>=vp->w || y<=-(int)s->h || y>=vp->h)return;
 rs_spr(x,y,s->tile,s->w,s->h,pal<0?s->pal:pal,priority,0);
}
static void background(const view *v,rs_viewport *vp) {
 int offset=v->cx>=0?v->cx/4:-((3-v->cx)/4),fx=floor8(offset),dx=offset-fx*8;
 int cols=(vp->w+dx+7)/8,rows=(vp->h+7)/8;
 for(int r=0;r<rows;r++)for(int c=0;c<cols;c++) {
  int world_y=v->zy==256?r*8:(r*8-12)*2;
  int yy=floor8(world_y),xx=(fx+c*(v->zx==128?2:1))&(wide()?127:63);
  /* widescreen: a 1024-px period, the back wall then its mirror image (h-flipped tiles: seamless at both joins) */
  uint16_t tile=yy<0 || yy>=HILLS_H?RS_MAP(gm_bg3_tile_count,PAL_HILLS,0,0,0):xx<64?gm_hills_map[yy*64+xx]:
   (uint16_t)(gm_hills_map[yy*64+127-xx]^RS_MAP_HFLIP);
  rs_bg_put(RS_BG3,v->mx+c,v->my+r,tile);
 }
 /* a tall screen: the wall's upper storey in rows 32..45, its top 112 lines half a period (128 px) along */
 if(tall())for(int r=0;r<TALL_UPPER/8;r++)for(int c=0;c<cols;c++)
  rs_bg_put(RS_BG3,v->mx+c,32+r,gm_hills_map[r*64+((fx+c+16)&63)]);
 vp->sx[2]=(int16_t)(v->mx*8+dx);vp->sy[2]=(int16_t)(v->my*8);
}
static void rect(uint8_t *p,int x0,int y0,int x1,int y1,int color,const rs_viewport *vp) {
 if(x0<0)x0=0;
 if(y0<0)y0=0;
 if(x1>=vp->w)x1=vp->w-1;
 if(y1>=vp->h)y1=vp->h-1;
 for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++)p[y*PIX_W+x]=(uint8_t)color;
}
/* the playfield layer's tiles, one per inked 8x8 cell; identical cells (strings, sugar cubes, cup stacks) share one */
#define TILE_SLOTS 2048
static uint16_t tile_slot[TILE_SLOTS];          /* per frame (state_audit.txt): tile id + 1 */
static uint8_t tile_seen[1024][64];             /* per frame: the tiles loaded this frame */
static int tile_id(const uint8_t *tile,int *next){
 uint32_t h=2166136261u;for(int i=0;i<64;i++)h=(h^tile[i])*16777619u;
 for(uint32_t i=h&(TILE_SLOTS-1);;i=(i+1)&(TILE_SLOTS-1)){
  int s=tile_slot[i];
  if(!s){
   if(*next>=1023)return 0;                     /* (never: the validator's densest courses use far fewer) */
   int id=(*next)++;memcpy(tile_seen[id],tile,64);rs_tiles_load8(VR_BG2+id,tile,1);tile_slot[i]=(uint16_t)(id+1);return id;
  }
  if(!memcmp(tile_seen[s-1],tile,64))return s-1;
 }
}
/* the counter and the café's items (paint.c) on the playfield layer, over the whole width */
static void foreground(const world *w,const view *v,const rs_viewport *vp,int *next){
 memset(pixels,0,sizeof pixels);int r1=vp->w-1;      /* the counter: the whole width (widescreen) */
 rect(pixels,0,232,r1,239,6,vp);rect(pixels,0,232,r1,233,9,vp);rect(pixels,0,236,r1,237,15,vp);
 for(int x=0;x<vp->w;x++)if(((x+v->cx)>>5)&1)rect(pixels,x,238,x,239,14,vp);
 canvas c={pixels,vp->w,COUNTER_Y,PIX_W};paint_course(&c,&w->c,v->cx,w->t);
 memset(tile_slot,0,sizeof tile_slot);
 int cols=(vp->w+7)/8;
 for(int r=0;r<30;r++)for(int col=0;col<cols;col++){
  uint8_t tile[64];int ink=0;for(int y=0;y<8;y++)for(int x=0;x<8;x++){tile[y*8+x]=pixels[(r*8+y)*PIX_W+col*8+x];ink|=tile[y*8+x];}
  int id=ink?tile_id(tile,next):0;rs_bg_put(RS_BG2,col,r,RS_MAP(id,PAL_GROUND,0,0,0));
 }
}
static void scene(const world *w,int t,const view *v,const rs_viewport *vp){
 // Heroes win OAM overlap against their steam and the biscuits.
 for(int p=0;p<w->players;p++){
  const hero *h=&w->h[p];int x=sx(v,(int)(h->wx>>16)),y=h->y>>16;
  if(h->state==HS_OFF)continue;
  int frame=h->state==HS_HIT || h->state==HS_DOWN?SPR_HERO_HIT:h->land_t<5?SPR_HERO_SQUASH:
   h->grounded?SPR_HERO_IDLE+(t/5)%2:h->held?SPR_HERO_STRETCH:SPR_HERO_AIR;
  int n=rs_oam_next();spr(frame,x-16,y-16,2,ppal(p),vp);
  if(p==0)p1_drawn_x=rs_oam_next()>n?rs_oam(n)->x+16+rs_screen_origin_x():-999;
  if(h->held && h->state==HS_RUN && !h->grounded)spr(SPR_STEAM+(t/4)%3,x-15,y+10,2,-1,vp);
  if(h->smooth_t>0)hu_sparkle(x+18-hu_ui_x(),y-10,w->t,2);   /* kit sprites: UI coordinates */
  if(h->state==HS_HIT && h->t<20)spr(SPR_PUFF+(h->t/7)%3,x-8,y+4,2,-1,vp);
 }
 /* the biscuits: gone once every mouse still flying has taken it */
 for(uint32_t i=(uint32_t)course_first_near(&w->c,v->cx-16);i<w->c.n;i++){
  const item *it=course_item(&w->c,i);if(it->x-16>v->cx+vp->w)break;
  if(it->kind!=IT_BISCUIT)continue;
  int taken=1;for(int p=0;p<w->players;p++)if(w->h[p].state==HS_RUN || w->h[p].state==HS_READY)taken&=it->taken>>p&1;
  if(!taken)spr(SPR_BISCUIT+(w->t/12+(int)(i&1))%2,sx(v,it->x)-8,it->y-8,2,-1,vp);
 }
}
static void icon(int p,int cx,int cy,int t,void *user) {
 (void)user;rs_viewport vp={0};vp.w=(int16_t)rs_screen_width();vp.h=240;
 spr(SPR_HERO_ICON,cx-8+hu_ui_x(),cy-8+hu_bob(t+p*16,64,1),2,ppal(p),&vp);   /* the kit's UI coordinates -> stage */
}
static void ranking(const world *w,hu_standing *rank) {
 int scores[4];for(int p=0;p<w->players;p++)scores[p]=w->h[p].score;hu_rank(rank,w->players,scores,scores);
}
static void title_ui(int t,int best){
 hu_logo("MOUSE ESPRESSO",title_ramps,2,2,0);hu_title_draw(t,best);
 // Move the join line above the action, leaving the starting counter unobscured.
 hu_clear_rows(26,1);hu_clear_rows(28,1);char text[160],who[64]="";int n=hu_players();
 for(int p=n;p<4;p++){size_t k=strlen(who);if(p>n)k+=(size_t)snprintf(who+k,sizeof who-k," / ");if(k<sizeof who)rs_strf(who+k,sizeof who-k,HTR(PLAYER_TAG),p+1);}
 if(n<4)rs_strf(text,sizeof text,n>1?TR(JOIN_LEAVE_SHORT):TR(JOIN_SHORT),who);
 else rs_strf(text,sizeof text,"%s",TR(FULL_LEAVE));
 hu_text(hu_center(text,0),12,text);hu_copyright(29);
 /* the hint, centred (half a column left of the true centre: the English keeps its column 4) */
 int hx=(38-hu_width(TR(HINT_TITLE),0))/2;hu_text(hx<0?0:hx,16,TR(HINT_TITLE));hu_title_sprites(t,icon,NULL);
}
/* the rows s takes wrapped in the columns shown (maxw: its lines' width), with the row above its first line when that
 * line's accents or CJK glyphs take it */
static int narrow_rows(const char *s,int maxw){
 char l[HT_MAXL][HT_LINE];int n=ht_wrap(s,maxw,l,HT_MAXL),r=n>0 && hu_tall(l[0]);
 for(int i=0;i<n;i++)r+=i?ht_line_rows(l[i]):1;
 return r;
}
/* The title on a tall screen showing fewer than 320 columns (house_tall.h), in the zoomed view of the run: the logo on
 * two lines in the UI rows LOGO_ROW .. 28 (the band behind the mice on the counter), pinned above the stage
 * (draw_frame); in the stage's rows 0..17, from the top: the hint, the slots, the prompt, BEST, the join line (wrapped),
 * the credits on two lines, up to 2 rows between them while they fit (the last ones first); the counter and the mice left clear */
static int title_narrow(int t,int best){
 int rows=ht_logo("MOUSE ESPRESSO",title_ramps,2,LOGO_ROW-1,0);
 /* the kit centres the logo on the 320 columns: its rows turned to centre it in the columns shown (whole tiles) */
 int mw=rs_bg_map_w(RS_BG1),dc=ht_fdiv8(ht_mid_x()-RS_SCREEN_W/2+4);
 for(int r=LOGO_ROW;dc && r<LOGO_ROW+rows-1;r++){
  uint16_t row[128];for(int c=0;c<mw;c++)row[c]=rs_bg_get(RS_BG1,c,r);
  for(int c=0;c<mw;c++)rs_bg_put(RS_BG1,c,r,row[((c-dc)%mw+mw)%mw]);
 }
 char join[160],who[64]="",bs[64]="",prompt[96];int n=hu_players(),cols=ht_ncols();
 for(int p=n;p<4;p++){size_t k=strlen(who);if(p>n)k+=(size_t)snprintf(who+k,sizeof who-k," / ");if(k<sizeof who)rs_strf(who+k,sizeof who-k,HTR(PLAYER_TAG),p+1);}
 if(n<4)rs_strf(join,sizeof join,n>1?TR(JOIN_LEAVE_SHORT):TR(JOIN_SHORT),who);
 else rs_strf(join,sizeof join,"%s",TR(FULL_LEAVE));
 rs_strf(prompt,sizeof prompt,hu_str(HS_PRESS_TO),TR(VERB));
 if(best>0)rs_strf(bs,sizeof bs,hu_str(HS_BEST_N),best);
 /* the items and their rows: the hint, the slots, the prompt (the A glyph left of it), BEST, the join line, the credits */
 const char *text[6]={TR(HINT_TITLE),NULL,prompt,bs,join,NULL};
 int h[6]={narrow_rows(text[0],cols),1,narrow_rows(prompt,cols-3),best>0?narrow_rows(bs,cols):0,narrow_rows(join,cols),2};
 int sum=0,gaps=-1;for(int i=0;i<6;i++)if(h[i]){sum+=h[i];gaps++;}
 int spare=LOGO_ROW-sum,base=spare<=0?0:spare/gaps>2?2:spare/gaps,extra=base<2 && spare>0?spare-base*gaps:0;
 ht_title tc={TR(VERB),MAX_PLAYERS,0,0};ht_title_out o;memset(&o,0,sizeof o);
 int x0,x1;ht_px(&x0,&x1);
 for(int i=0,r=0,g=0;i<6;r+=h[i]?h[i]+base+(g++>=gaps-extra):0,i++){   /* the extra rows: the last gaps first */
  if(!h[i])continue;
  if(i==5){const char *cr1="(C) 2026 8BCRAFT",*cr2="RETROSTONE VC";hu_text(ht_center(cr1,0),r,cr1);hu_text(ht_center(cr2,0),r+1,cr2);continue;}
  if(i==1){                                                 /* the slots, their tags when there is room */
   tc.top=r;ht_slots(&tc,&o);
   if((x1-x0)/4>=44)for(int p=0;p<n;p++){char s[16];rs_strf(s,sizeof s,HTR(PLAYER_TAG),p+1);hu_text(ht_fdiv8(o.slot_cx[p]-28),r,s);}
   continue;
  }
  char l[HT_MAXL][HT_LINE];int nl=ht_wrap(text[i],i==2?cols-3:cols,l,HT_MAXL),first=r+hu_tall(l[0]);
  if(i!=2){ht_text(first,text[i],0);continue;}
  int px=ht_center_w(hu_width(l[0],0)+2);                   /* the prompt, blinking; the A glyph (16 px) left of it */
  o.glyph_x=px*8-4;o.glyph_y=first*8-4;
  if(hu_blink(t))for(int k=0,rr=first;k<nl;k++){if(k && hu_tall(l[k]))rr++;hu_text(k?ht_center(l[k],0):px+2,rr,l[k]);rr++;}
 }
 ht_title_sprites(&tc,&o,t,icon,NULL);
 return rows-1;
}
/* ht_gameover_panel (house_tall.h) with the panel wide enough for the retry line too (ht_over_w leaves it out);
 * returns the retry line's row */
static int over_panel(int new_best,const char *retry){
 int w=ht_over_w(new_best,NULL),rw=hu_width(retry,0)+2,cols=ht_ncols(),r=ht_gameover_retry_row(NULL);
 if(rw>w)w=rw>cols?cols:rw;
 int x=ht_center_w(w),bottom=r+ht_text_rows(retry,1)+1;
 hu_panel(x,9,w,bottom-9+1);
 const char *lab[3]={hu_str(HS_SCORE),hu_str(new_best?HS_NEW_BEST:HS_BEST),hu_str(HS_MEDAL)};
 for(int i=0;i<3;i++)hu_box_text(ht_center(lab[i],0),ht_over_label_row(i),lab[i]);
 return r;
}
void draw_frame(const world *w,int state,int st_t,int best,int new_best,int paused){
 nviews=rs_viewport_layout(1,0,VP);hu_clear();rs_oam_clear();rs_brightness(paused?9:15);
 view *v=&V[0];rs_viewport *vp=&VP[0];v->mx=v->my=0;v->zx=v->zy=256;v->cx=w->scroll-draw_wide_left();
 /* behind the panel: where the run ended (the course behind it is no longer kept) */
 for(int y=0;y<240;y++){int k=y*256/240,d=(y&4)?4:0;v->sky[y]=RS_RGB8(192+14*k/256+d,172+16*k/256+d/2,150+12*k/256+d);}
 background(v,vp);int next=1;foreground(w,v,vp,&next);
 vp->sx[0]=vp->sx[1]=vp->sy[0]=vp->sy[1]=0;vp->layers=7;vp->objs=1;
 /* a tall screen: ONE zoomed view, the display's height, the columns shown from TALL_BEHIND px behind the first mouse,
  * on the title and the panels too (laid out in the columns shown: house_tall.h); the HUD on the ceiling's beam while
  * the mice fly (BG1's top HUD_LINES pinned at the screen's top); the title's logo pinned above the stage */
 int run=state==ST_PLAY || state==ST_DEAD,hud=tall() && run;
 rs_screen_fill(run || tall());
 if(tall())rs_screen_focus_x(hero_start_x(w->players,0)-TALL_BEHIND+(rs_screen_visible_x1()-rs_screen_visible_x0())/2);
 else rs_screen_focus_x(-1);
 int narrow=ht_narrow();             /* the zoomed screen shows fewer columns than the kit's 320 */
 logo_pin=0;
 if(state==ST_TITLE && narrow){       /* the logo's glyphs (1 or 2 lines) 16 lines above the stage, under the beam */
  logo_lines=title_narrow(st_t,best)*8;
  logo_pin=-(logo_lines+16);if(logo_pin<=rs_screen_top()+HUD_LINES)logo_pin=rs_screen_top()+HUD_LINES+1;
  rs_bg_pin(RS_BG1,LOGO_ROW*8,logo_lines,logo_pin+rs_screen_origin_y());
 }else{
  rs_bg_pin(RS_BG1,0,hud?HUD_LINES:0,0);
  if(state==ST_TITLE)title_ui(st_t,best);
 }
 hud_pinned=hud;
 int vx0=rs_screen_visible_x0()-rs_screen_origin_x(),vx1=rs_screen_visible_x1()-rs_screen_origin_x();
 int hy=hud?rs_screen_top():0,hc=hud?((vx0+vx1)/2-160)/8:0;   /* the HUD's top, its centre's offset in columns */
 if(run){
  if(w->players==1){hu_number(w->h[0].score,160+hc*8,8+hy,3);char text[128];rs_strf(text,sizeof text,TR(HUD_BISCUITS_DIST),w->h[0].apples,w->scroll/12);hu_text(hu_center(text,0)+hc,4,text);}
  else for(int p=0;p<w->players;p++){
   char text[48],tag[16];rs_strf(tag,sizeof tag,HTR(PLAYER_TAG),p+1);snprintf(text,sizeof text,"%s %d",tag,w->h[p].score);
   if(hud)hu_text((vx0+4)/8+p*((vx1-vx0)/8)/4,4,text);   /* (a tall screen: under the corner buttons, in the columns shown) */
   else hu_text(p*10,1,text);
  }
  if(paused && narrow)ht_big(13,HTR(PAUSED),HU_BIG_FREE);    /* centred in the columns shown */
  else if(paused)hu_big(hu_center(HTR(PAUSED),1),13,HTR(PAUSED),HU_BIG_FREE);
 }
 if(state==ST_TITLE || state==ST_PLAY || state==ST_DEAD)scene(w,w->t,v,vp);
 if(state==ST_OVER){
  hu_standing rank;ranking(w,&rank);int slide=hu_slide_in(st_t,20,200);
  if(narrow && w->players==1){         /* zoomed: the panels narrower, in the columns shown (house_tall.h) */
   static const int medals[4]=MEDAL_SCORES;
   ht_banner(4,HTR(GAME_OVER));int r=over_panel(new_best,HTR(BACK_TITLE));
   ht_gameover_sprites(w->h[0].score,best,hu_medal_of(w->h[0].score,medals),st_t,slide);
   ht_retry_line_at(r,st_t,RETRY_LOCK,HTR(BACK_TITLE));
  }else if(narrow){
   ht_results_panel(&rank,NULL);ht_results_sprites(&rank,st_t,slide);
   for(int i=0;i<rank.n;i++){int x,y;ht_results_icon_pos(&rank,i,&x,&y);spr(SPR_HERO_ICON,x-8+hu_ui_x(),y-8+slide,3,ppal(rank.order[i]),vp);}
   ht_retry_line_at(hu_results_retry_row(&rank),st_t,RETRY_LOCK,HTR(BACK_TITLE));
  }else if(w->players==1){
   static const int medals[4]=MEDAL_SCORES;hu_banner(4,HTR(GAME_OVER));hu_gameover_panel(1,new_best,w->h[0].score,0);
   hu_gameover_sprites(1,w->h[0].score,0,best,hu_medal_of(w->h[0].score,medals),st_t,slide);hu_retry_line(st_t,RETRY_LOCK,HTR(BACK_TITLE));
  }else{
   hu_results_panel(&rank,NULL);hu_results_sprites(&rank,st_t,slide);
   for(int i=0;i<rank.n;i++){int x,y;hu_results_icon_pos(&rank,i,&x,&y);spr(SPR_HERO_ICON,x-8+hu_ui_x(),y-8+slide,3,ppal(rank.order[i]),vp);}
   hu_retry_line_at(hu_results_retry_row(&rank),st_t,RETRY_LOCK,HTR(BACK_TITLE));
  }
  rs_bg_scroll(RS_BG1,-hu_ui_x()&(rs_bg_map_w(RS_BG1)*8-1),-slide);   /* the UI centred (widescreen) */
 }else rs_bg_scroll(RS_BG1,-hu_ui_x()&(rs_bg_map_w(RS_BG1)*8-1),0);
 rs_bg_scroll(RS_BG2,0,0);rs_bg_scroll(RS_BG3,vp->sx[2],vp->sy[2]);rs_viewports(0,NULL,0);
 shown_state=state;shown_best=best;
}
/* Tests (--opt view=1, main.c at exit): the view of the last frame, "view: width=W left=L p1_x=X next_x=N": L the columns
 * shown behind the mice (a third of the extra width), X the screen x of P1's sprite centre as drawn, N the screen x of
 * the next item ahead of P1 (tests/wide_test.sh: X and N - X against 320) */
void draw_view_log(const world *w) {
 const view *v=&V[0];int hx=(int)(w->h[0].wx>>16),nx=hx+9999;
 for(uint32_t i=w->c.first;i<w->c.n;i++){const item *it=course_item(&w->c,i);if(it->kind!=IT_BISCUIT && it->x>hx){nx=it->x;break;}}
 rs_log("view: width=%d left=%d p1_x=%d next_x=%d",rs_screen_width(),draw_wide_left(),p1_drawn_x,sx(v,nx));
}
#define S(v) rs_state_var("draw." #v,&(v),sizeof(v))
void draw_state(void) {S(V);S(VP);S(nviews);S(shown_state);S(shown_best);S(laid_w);RS_STATE_RASTER(raster);}
#undef S
