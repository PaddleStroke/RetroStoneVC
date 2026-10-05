#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static int shown_state=-1,shown_best=-1;
static rs_color line_col[240];
static const rs_color title_ramps[2][3]={
 {RS_HEX(0xffd2df),RS_HEX(0xee88bb),RS_HEX(0xab4b80)},
 {RS_HEX(0xddf4a4),RS_HEX(0xa9cf64),RS_HEX(0x5f954b)}};
static void raster(int y,void*u){(void)u;rs_pal_set(0,line_col[y]);}
void draw_init(void){
 hu_config cfg=hu_defaults();cfg.obj_tile=KIT_OBJ_TILE;cfg.obj_vram=VR_OBJ;cfg.obj_pal=3;hu_init(&cfg);
 rs_bg_setup(RS_BG1,64,32,VR_BG1);rs_bg_setup(RS_BG2,64,32,VR_BG2);rs_bg_setup(RS_BG3,64,32,VR_BG3);
 rs_tiles_load(VR_BG3,gm_bg3_tiles,gm_bg3_tile_count);
 rs_pal_load(RS_PAL_BG(PAL_HILLS),gm_hills_pal,16);
 for(int y=0;y<HILLS_H;y++)for(int x=0;x<64;x++)rs_bg_put(RS_BG3,x,y,gm_hills_map[y*64+x]);
 static const rs_color pal[16]={0,RS_HEX(0x23352f),RS_HEX(0x466947),RS_HEX(0x619248),
 RS_HEX(0x94bd59),RS_HEX(0xd5e594),RS_HEX(0x573343),RS_HEX(0xa65162),RS_HEX(0xe88189),
 RS_HEX(0xffc6a7),RS_HEX(0x805034),RS_HEX(0xc08038),RS_HEX(0xf2ba50),
 RS_HEX(0xfbf0c6),RS_HEX(0x384d44),RS_HEX(0x63856a)};
 rs_pal_load(RS_PAL_BG(PAL_GROUND),pal,16);
 rs_obj_base(VR_OBJ);rs_tiles_load(VR_OBJ,gm_obj_tiles,gm_obj_tile_count);
 rs_pal_load(RS_PAL_OBJ(0),gm_obj_pals,48);rs_pal_load(RS_PAL_OBJ(4),gm_obj_pals+64,32);
 for(int y=0;y<240;y++)line_col[y]=RS_RGB8(199-y/12,221-y/12,183-y/16);
 for(int l=0;l<3;l++)rs_bg_enable(l,1);
 rs_raster(raster,NULL);
}
static void pixel(uint8_t*p,int x,int y,int c){if(x>=0&&x<320&&y>=0&&y<240)p[y*320+x]=c;}
static void rect(uint8_t*p,int x0,int y0,int x1,int y1,int c){for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++)pixel(p,x,y,c);}
static void oval(uint8_t*p,int cx,int cy,int rx,int ry,int a,int b,int c){
 for(int y=-ry-1;y<=ry+1;y++)for(int x=-rx-1;x<=rx+1;x++){
 int d=x*x*ry*ry+y*y*rx*rx;if(d>(rx+1)*(rx+1)*(ry+1)*(ry+1))continue;
 pixel(p,cx+x,cy+y,d>rx*rx*ry*ry?1:y<-ry/3?a:x<0?b:c);}}
static void line(uint8_t*p,int x0,int y0,int x1,int y1,int r,int c){
 int n=abs(x1-x0)>abs(y1-y0)?abs(x1-x0):abs(y1-y0);if(!n)n=1;
 for(int i=0;i<=n;i++)rect(p,x0+(x1-x0)*i/n-r,y0+(y1-y0)*i/n-r,x0+(x1-x0)*i/n+r,y0+(y1-y0)*i/n+r,c);}
void scenery_pixels(const world*w,uint8_t*px){
 memset(px,0,320*240);
 /* Every branch and obstacle shares the same world-space vine coordinate. */
 for(int x=0;x<320;x++){int yy=vine_y(x+w->scroll);
 for(int y=-8;y<=8;y++)pixel(px,x,yy+y,abs(y)==8?1:y<-5?5:y<-2?4:y<4?3:2);
 if((x+w->scroll)%13==0)pixel(px,x,yy+3,4);}
 /* Ovate, asymmetrical leaves: curved contour, tapering tip and branching veins. */
 /* Include complete shapes whose roots are outside the view; clip only their pixels. */
 for(int cell=w->scroll/110-2;cell<=(w->scroll+320+96)/110;cell++){
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
 int base=(w->scroll-480)/OBSTACLE_SPACING;if(base<0)base=0;
 for(int i=base>0?base-1:0;i<=base+3;i++){
 obstacle o;obstacle_at(w,i,&o);int x=o.x-w->scroll,y=vine_y(o.x),s=o.side?1:-1;
 if(x<-50||x>448)continue; /* Include the fruit stem even when its patrolling hazard is farther outside. */
 if(o.type==0){
  /* Woody hooked thorns sprout directly from the vine, with shaded broad bases. */
  for(int j=0;j<3;j++){
   int tx=x+(j-1)*10,height=j==1?19:14;
   for(int k=0;k<height;k++){
    int center=tx+k*k/95,width=(height-k)*4/height;
    for(int q=-width-1;q<=width+1;q++)pixel(px,center+q,vine_y(center+q+w->scroll)+s*(7+k),abs(q)>width?1:q<0?12:q<2?11:10);
   }
  }
 }else if(o.type==1){
  /* Two inchworm poses: extended / arched. Face follows its actual patrol direction. */
  int phase=(w->t/12+i)%2,dir=o.dir;
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
 int bx=o.anchor-w->scroll-80,by=vine_y(o.anchor-80),taken=1;
 for(int p=0;p<w->players;p++)if(w->h[p].state==HS_RUN||w->h[p].state==HS_READY)taken &= w->h[p].berry==i;
 if(taken)continue;
 line(px,bx,by,bx,by+s*16,0,2);
 oval(px,bx,by+s*21,6,6,9,8,7);
 line(px,bx-3,by+s*16,bx,by+s*18,0,2);line(px,bx+3,by+s*16,bx,by+s*18,0,2);
 pixel(px,bx-2,by+s*21-2,13);
 }
}
static void foreground(const world*w){
 uint8_t px[320*240];scenery_pixels(w,px);
 int next=1;uint8_t empty[64]={0};rs_tiles_load8(VR_BG2,empty,1);
 for(int r=0;r<30;r++)for(int c=0;c<40;c++){
  uint8_t tile[64];int ink=0;for(int y=0;y<8;y++)for(int x=0;x<8;x++){tile[y*8+x]=px[(r*8+y)*320+c*8+x];ink|=tile[y*8+x];}
  int id=0;if(ink){id=next++;rs_tiles_load8(VR_BG2+id,tile,1);}rs_bg_put(RS_BG2,c,r,RS_MAP(id,PAL_GROUND,0,0,0));
 }
}
static int player_pal(int p){return p?hu_player_pal(p):0;}
static void spr(int id,int x,int y,int prio,int pal){
 const gm_sprite_def*d=&gm_spr[id];rs_spr(x,y,d->tile,d->w,d->h,pal<0?d->pal:pal,prio,0);}
static void draw_heroes(const world*w,int t,int sx,int sy){
 for(int p=0;p<w->players;p++){const hero*h=&w->h[p];if(h->state==HS_OFF)continue;
 int id=h->state==HS_HIT||h->state==HS_DOWN?SPR_HERO_HIT:h->flip?SPR_HERO_SQUASH:SPR_HERO_IDLE+(t/10)%2;
 const gm_sprite_def*d=&gm_spr[id];
 int flip=h->flip?(h->flip<11?h->side:h->from):h->side;
 int cx=h->x+(h->flip?(curve_sine((22-h->flip)*512/22)*9>>15):0);
 /* The same full-size snail grips each surface; the eye stalks hang below on the underside. */
 rs_spr(cx-16+sx,(h->y>>16)-16+sy,d->tile,d->w,d->h,player_pal(p),2,flip?RS_SPR_VFLIP:0);
 }}
static void slot_icon(int p,int cx,int cy,int t,void*u){(void)t;(void)u;spr(SPR_HERO_ICON,cx-8,cy-8,2,player_pal(p));}
static void standing(const world*w,hu_standing*s){int a[4];for(int p=0;p<w->players;p++)a[p]=w->h[p].score;hu_rank(s,w->players,a,a);}
static void screen_text(const world *w, int state, int st_t, int best, int new_best, const hu_standing *rs)
{
    int screen = state == ST_DEAD ? ST_PLAY : state;
    if (screen != shown_state || (state == ST_OVER && st_t == RETRY_LOCK) || best != shown_best) {
        hu_clear();
        if (state == ST_TITLE) hu_logo("SNAIL SWITCH", title_ramps, 2, 2, 0);
        if (screen == ST_PLAY && w->players >= 3) hu_score_tags(w->players, 0);     /* P1..P4 by the corner chips */
        if (state == ST_OVER) {
            if (w->players == 1) {
                hu_banner(4, "GAME OVER");
                hu_gameover_panel(1, new_best, w->h[0].score, 0);
            } else {
                hu_results_panel(rs, NULL);                                     /* P2 WINS! and the ranking */
            }
        }
        shown_state = screen;
        shown_best = best;
    }
    /* the title, every frame: the prompt (blinking), the player slots, BEST, the join line, the credits */
    if (state == ST_TITLE) {hu_title_draw(st_t, best);hu_text(hu_center("UP / DOWN: VINE SIDE",0),9,"UP / DOWN: VINE SIDE");hu_text(hu_center("CATCH EVERY FRUIT",0),10,"CATCH EVERY FRUIT");}
    if (state == ST_OVER) {
        if (w->players == 1) hu_retry_line(st_t, RETRY_LOCK, "A: TITLE");
        else hu_retry_line_at(hu_results_retry_row(rs), st_t, RETRY_LOCK, "A: TITLE");
    }
}


void draw_frame(const world*w,int state,int st_t,int best,int new_best,int paused){
 static const int medals[4]=MEDAL_SCORES;
 int sx=hu_shake_x(),sy=hu_shake_y();hu_standing rank;standing(w,&rank);
 foreground(w);rs_bg_scroll(RS_BG2,-sx,-sy);rs_bg_scroll(RS_BG3,w->scroll/4,0);
 screen_text(w,state,st_t,best,new_best,&rank);
 int slide=state==ST_OVER?hu_slide_in(st_t,20,200):0;
 rs_bg_scroll(RS_BG1,0,-slide);hu_pause(paused,13);rs_oam_clear();
 if(state==ST_PLAY||state==ST_DEAD){int sc[4];for(int p=0;p<w->players;p++)sc[p]=w->h[p].score;hu_score_chips(w->players,sc,0);}
 if(state==ST_TITLE)hu_title_sprites(st_t,slot_icon,NULL);
 if(state!=ST_OVER)draw_heroes(w,w->t,sx,sy);
 if(state==ST_OVER){
 if(w->players==1)hu_gameover_sprites(1,w->h[0].score,0,best,hu_medal_of(w->h[0].score,medals),st_t,slide);
 else{hu_results_sprites(&rank,st_t,slide);for(int i=0;i<rank.n;i++){int x,y;hu_results_icon_pos(&rank,i,&x,&y);spr(SPR_HERO_ICON,x-8,y-8+slide,3,player_pal(rank.order[i]));}}
 }}
#define S(v) rs_state_var("draw." #v,&(v),sizeof(v))
void draw_state(void){S(line_col);S(shown_state);S(shown_best);RS_STATE_RASTER(raster);}




