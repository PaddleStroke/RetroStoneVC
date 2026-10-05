/* Raccoon Recycler: fixed full-screen sorting station, all players share a visible item. */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include <stdio.h>
#include <string.h>
static const rs_color title_ramps[2][3]={{RS_RGB8(230,222,174),RS_RGB8(191,168,120),RS_RGB8(118,93,79)},{RS_RGB8(207,237,154),RS_RGB8(127,188,100),RS_RGB8(64,127,98)}};
void draw_init(void){
 hu_config hc=hu_defaults();hc.obj_tile=KIT_OBJ_TILE;hc.obj_vram=VR_OBJ;hc.obj_pal=3;hu_init(&hc);
 rs_bg_setup(RS_BG1,64,32,VR_BG1);rs_bg_setup(RS_BG2,64,32,VR_BG3);rs_bg_setup(RS_BG3,64,32,VR_BG2);
 rs_tiles_load(VR_BG2,gm_bg2_tiles,gm_bg2_tile_count);rs_tiles_load(VR_BG3,gm_bg3_tiles,gm_bg3_tile_count);
 rs_pal_load(RS_PAL_BG(PAL_GROUND),gm_ground_pal,16);rs_pal_load(RS_PAL_BG(PAL_HILLS),gm_hills_pal,16);
 for(int y=0;y<GROUND_H;y++)for(int x=0;x<64;x++)rs_bg_put(RS_BG3,x,y,gm_ground_map[y*64+x]);
 for(int y=0;y<HILLS_H;y++)for(int x=0;x<64;x++)rs_bg_put(RS_BG2,x,y,gm_hills_map[y*64+x]|(1u<<13));
 for(int l=0;l<3;l++)rs_bg_enable(l,1);
 rs_obj_base(VR_OBJ);rs_tiles_load(VR_OBJ,gm_obj_tiles,gm_obj_tile_count);rs_pal_load(RS_PAL_OBJ(0),gm_obj_pals,48);rs_pal_load(RS_PAL_OBJ(4),gm_obj_pals+64,32);
}
static void spr(int id,int x,int y,int prio,int pal){const gm_sprite_def *d=&gm_spr[id];rs_spr(x,y,d->tile,d->w,d->h,pal<0?d->pal:pal,prio,0);}
static int ppal(int p){return p?hu_player_pal(p):0;}
static void object(int item,int cx,int cy,int priority){spr(SPR_ITEM_0+item,cx-16,cy-16,priority,-1);}
void draw_frame(const world *w,int state,int st_t,int best,int new_best,int paused){
 hu_clear();rs_oam_clear();rs_brightness(paused?9:15);rs_bg_scroll(RS_BG2,0,0);rs_bg_scroll(RS_BG3,0,0);
 int score[4];for(int p=0;p<w->players;p++)score[p]=w->h[p].score;hu_standing rank;hu_rank(&rank,w->players,score,score);
 int slide=state==ST_OVER?hu_slide_in(st_t,20,200):0;rs_bg_scroll(RS_BG1,0,-slide);
 /* Foreground signs and HUD get the first OAM slots, above all moving objects. */
 if(state==ST_PLAY||state==ST_DEAD)for(int p=0;p<w->players;p++){
  char label[32];const hero*h=&w->h[p];int cx=(p*2+1)*160/w->players;
  if(w->players>1){rs_strf(label,sizeof label,HTR(PLAYER_TAG),p+1);hu_text((cx-hu_width(label,0)*4)/8,0,label);}
  hu_number(h->score,cx,w->players>1?10:2,3);
 }
 /* Bin badges are the complete rule: neutral animal silhouettes or plain colour swatches. */
 for(int b=0;b<3;b++){
  int badge=w->mode==RULE_SPECIES?SPR_BADGE_DUCK+w->bin_order[b]:SPR_BADGE_YELLOW+w->bin_order[b];
  spr(badge,80+b*64,197,3,-1);
  if(w->switch_t>0 && ((w->switch_t/6)&1))spr(SPR_LAMP,92+b*64,178,3,-1);
 }
 // Same real workstations on title and during play; full-size raccoons stand on the platform.
 for(int p=0;p<w->players;p++){const hero *h=&w->h[p];int id=SPR_HERO_IDLE;
  if(h->feedback<0||!h->alive)id=SPR_HERO_HIT;else if(h->drop_t&&h->drop_side!=ROUTE_CENTER)id=h->drop_side<0?SPR_HERO_LEFT:SPR_HERO_RIGHT;
  spr(id,h->x-16,126,2,ppal(p));
 }
 /* A single fixed conveyor; each player's gate chooses a continuous outgoing route. */
 for(int i=0;i<ITEM_MAX;i++)if(w->items[i].active)object(w->items[i].item,160,w->items[i].y/256,2);
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
 for(int yy=-12+(w->scroll/256%12);yy<BELT_JOIN;yy+=12)spr(SPR_BELT_MARK,146,yy,1,-1);
 for(int yy=-12+(w->scroll/256%12);yy<BELT_JOIN;yy+=12)spr(SPR_BELT_MARK,166,yy,1,-1);
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
void draw_state(void){}
