/* The full cavern is shared. Runtime ball/rope tiles over code-drawn background and heroes. */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
static const rs_color ramps[2][3]={{RS_RGB8(255,225,157),RS_RGB8(227,174,85),RS_RGB8(161,101,60)},{RS_RGB8(170,247,221),RS_RGB8(82,210,174),RS_RGB8(42,141,147)}};
static const rs_color field_pal[16]={0,RS_RGB8(29,29,55),RS_RGB8(255,238,194),RS_RGB8(246,255,248),RS_RGB8(255,181,210),RS_RGB8(222,99,157),RS_RGB8(117,53,119),RS_RGB8(174,246,205),RS_RGB8(74,193,155),RS_RGB8(35,109,121),RS_RGB8(168,220,254),RS_RGB8(91,156,232),RS_RGB8(57,80,154),RS_RGB8(255,226,121),RS_RGB8(199,140,67),RS_RGB8(33,34,57)};
static rs_color lines[240];
static void raster(int y,void*user){(void)user;rs_pal_set(0,lines[y]);}
static void pixel(uint8_t*p,int x,int y,int c){if(x>=0&&x<320&&y>=0&&y<240)p[y*320+x]=(uint8_t)c;}
static void rect(uint8_t*p,int x0,int y0,int x1,int y1,int c){if(x0<0)x0=0;if(y0<0)y0=0;if(x1>319)x1=319;if(y1>239)y1=239;for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++)pixel(p,x,y,c);}
static void ellipse(uint8_t*p,int cx,int cy,int rx,int ry,int c){for(int y=-ry;y<=ry;y++)for(int x=-rx;x<=rx;x++)if(x*x*ry*ry+y*y*rx*rx<=rx*rx*ry*ry)pixel(p,cx+x,cy+y,c);}
static void spr(int id,int x,int y,int pal,int flip){const gm_sprite_def*d=&gm_spr[id];if(x<=-(int)d->w||x>=320||y<=-(int)d->h||y>=240)return;rs_spr(x,y,d->tile,d->w,d->h,pal,2,flip?RS_SPR_HFLIP:0);}
void draw_init(void){
 hu_config c=hu_defaults();c.obj_tile=KIT_OBJ_TILE;c.obj_vram=VR_OBJ;c.obj_pal=3;hu_init(&c);
 rs_bg_setup(RS_BG1,64,32,VR_BG1);rs_bg_setup(RS_BG2,64,32,VR_BG2);rs_bg_setup(RS_BG3,64,32,VR_BG3);
 rs_tiles_load(VR_BG3,gm_bg3_tiles,gm_bg3_tile_count);rs_tiles_load(VR_BG3+gm_bg3_tile_count,gm_bg2_tiles,gm_bg2_tile_count);
 rs_pal_load(RS_PAL_BG(PAL_HILLS),gm_hills_pal,16);rs_pal_load(RS_PAL_BG(PAL_GROUND),gm_ground_pal,16);rs_pal_load(RS_PAL_BG(4),field_pal,16);
 for(int y=0;y<26;y++)for(int x=0;x<40;x++)rs_bg_put(RS_BG3,x,y,gm_hills_map[y*64+x]);
 for(int y=0;y<4;y++)for(int x=0;x<40;x++)rs_bg_put(RS_BG3,x,y+26,(uint16_t)(gm_ground_map[y*64+x]+gm_bg3_tile_count));
 uint8_t zero[64]={0};rs_tiles_load8(VR_BG2,zero,1);
 for(int i=0;i<3;i++)rs_bg_enable(i,1);
 rs_obj_base(VR_OBJ);rs_tiles_load(VR_OBJ,gm_obj_tiles,gm_obj_tile_count);rs_pal_load(RS_PAL_OBJ(0),gm_obj_pals,48);rs_pal_load(RS_PAL_OBJ(4),gm_obj_pals+64,32);
 for(int y=0;y<240;y++)lines[y]=RS_RGB8(26+y/40,30+y/40,50+y/30);
 rs_raster(raster,NULL);
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
 uint8_t pixels[320*240]={0};
 for(int p=0;p<w->players;p++){
  const hero*h=&w->h[p];if(h->state==HS_RUN&&h->shot){int c=p==0?13:p==1?10:p==2?4:7,x=h->shot_x,y=h->shot_y;
   for(int yy=y+5;yy<GROUND_Y-10;yy++){pixel(pixels,x-1,yy,1);pixel(pixels,x,yy,c);pixel(pixels,x+1,yy,yy%4?2:c);}
   for(int yy=0;yy<6;yy++)rect(pixels,x-yy/2,y+yy,x+yy/2,y+yy,yy<2?3:14);
   pixel(pixels,x-3,y+6,1);pixel(pixels,x+3,y+6,1);
  }
 }
 for(int i=0;i<MAX_BALLS;i++)if(w->balls[i].active)ball_draw(pixels,&w->balls[i]);
 for(int i=0;i<MAX_FX;i++)if(w->fx[i].t){const fragment*f=&w->fx[i];if(f->t>8||w->t%2)rect(pixels,f->x,f->y,f->x+1,f->y+1,4+3*f->hue);}
 if(w->warning)for(int n=0;n<w->warning_count;n++){int x=w->incoming[n].x;ellipse(pixels,x,30,ball_radius(w->incoming[n].size)+3,3,14);for(int d=0;d<7;d++){pixel(pixels,x-d,38+d,13);pixel(pixels,x+d,38+d,13);}rect(pixels,x-1,35,x+1,44,3);}
 int next=1;
 for(int row=0;row<30;row++)for(int col=0;col<40;col++){
  uint8_t tile[64];int ink=0;for(int y=0;y<8;y++)for(int x=0;x<8;x++){tile[y*8+x]=pixels[(row*8+y)*320+col*8+x];ink|=tile[y*8+x];}
  int id=0;if(ink){id=next++;rs_tiles_load8(VR_BG2+id,tile,1);}rs_bg_put(RS_BG2,col,row,RS_MAP(id,4,0,0,0));
 }
}
static void icon(int p,int x,int y,int t,void*user){(void)t;(void)user;spr(SPR_HERO_ICON,x-8,y-8,hu_player_pal(p),0);}
void draw_frame(const world*w,int state,int t,int best,int new_best,int paused){
 hu_clear();rs_oam_clear();rs_brightness(paused?9:15);field(w);
 for(int p=0;p<w->players;p++){const hero*h=&w->h[p];if(h->state==HS_DOWN&&h->t>70)continue;int id=h->state==HS_HIT||h->state==HS_DOWN?SPR_HERO_HIT:SPR_HERO_IDLE+(h->state==HS_RUN&&((w->t/8)%2));
  spr(id,h->x-16,GROUND_Y-31,hu_player_pal(p),h->facing<0);
 }
 int scores[4];for(int p=0;p<w->players;p++)scores[p]=w->h[p].score;hu_standing rank;hu_rank(&rank,w->players,scores,scores);int slide=state==ST_OVER?hu_slide_in(t,20,200):0;
 if(state==ST_TITLE){
  hu_logo("MOLE MARBLES",ramps,2,2,0);hu_title_draw(t,best);hu_title_sprites(t,icon,NULL);
  hu_text(hu_center("{ } MOVE   A: HARPOON",0),16,"{ } MOVE   A: HARPOON");
 }else if(state==ST_PLAY||state==ST_DEAD){
  if(w->players==1)hu_number(scores[0],160,8,3);
  else for(int p=0;p<w->players;p++){char s[24];snprintf(s,sizeof s,"P%d %d",p+1,scores[p]);hu_text(p*10,1,s);}
  if(paused)hu_big(hu_center("PAUSED",1),13,"PAUSED",HU_BIG_FREE);
 }else{
  if(w->players==1){static const int medals[4]=MEDAL_SCORES;hu_banner(4,"GAME OVER");hu_gameover_panel(1,new_best,scores[0],0);hu_gameover_sprites(1,scores[0],0,best,hu_medal_of(scores[0],medals),t,slide);hu_retry_line(t,RETRY_LOCK,"A: TITLE");}
  else{hu_results_panel(&rank,NULL);hu_results_sprites(&rank,t,slide);for(int i=0;i<rank.n;i++){int x,y;hu_results_icon_pos(&rank,i,&x,&y);spr(SPR_HERO_ICON,x-8,y-8+slide,hu_player_pal(rank.order[i]),0);}hu_retry_line_at(hu_results_retry_row(&rank),t,RETRY_LOCK,"A: TITLE");}
 }
 rs_bg_scroll(RS_BG1,0,-slide);rs_bg_scroll(RS_BG2,0,0);rs_bg_scroll(RS_BG3,0,0);
}
void draw_state(void){rs_state_var("draw.lines",lines,sizeof lines);RS_STATE_RASTER(raster);}

