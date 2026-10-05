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
static void raster(int y,void*u){(void)u;if(y>=0 && y<240)rs_pal_set(0,water[y]);}
void draw_init(void){hu_config c=hu_defaults();c.obj_tile=KIT_OBJ_TILE;c.obj_vram=VR_OBJ;c.obj_pal=3;hu_init(&c);
 rs_bg_setup(RS_BG1,64,32,VR_BG1);rs_bg_setup(RS_BG2,64,32,VR_BG2);rs_bg_enable(RS_BG1,1);rs_bg_enable(RS_BG2,1);rs_bg_enable(RS_BG3,0);
 uint8_t empty[64]={0};rs_tiles_load8(VR_BG2,empty,1);rs_pal_load(RS_PAL_BG(1),river_pal,16);
 rs_obj_base(VR_OBJ);rs_tiles_load(VR_OBJ,gm_obj_tiles,gm_obj_tile_count);rs_pal_load(RS_PAL_OBJ(0),gm_obj_pals,48);rs_pal_load(RS_PAL_OBJ(4),gm_obj_pals+64,32);
 for(int y=0;y<240;y++){int d=(y&4)?3:0;water[y]=RS_RGB8(86+d+y/24,169+d+y/32,175+d+y/40);}rs_raster(raster,NULL);
}
static void dot(uint8_t*p,int x,int y,int c){if(x>=0 && x<320 && y>=0 && y<240)p[y*320+x]=(uint8_t)c;}
static void rect(uint8_t*p,int x0,int y0,int x1,int y1,int c){if(x0<0)x0=0;if(y0<0)y0=0;if(x1>319)x1=319;if(y1>239)y1=239;for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++)dot(p,x,y,c);}
static void line(uint8_t*p,int x0,int y0,int x1,int y1,int c){int n=abs(x1-x0)>abs(y1-y0)?abs(x1-x0):abs(y1-y0);if(n<1)n=1;for(int i=0;i<=n;i++)dot(p,x0+(x1-x0)*i/n,y0+(y1-y0)*i/n,c);}
static void oval(uint8_t*p,int x,int y,int rx,int ry,int c){for(int yy=-ry;yy<=ry;yy++)for(int xx=-rx;xx<=rx;xx++)if(xx*xx*ry*ry+yy*yy*rx*rx<=rx*rx*ry*ry)dot(p,x+xx,y+yy,c);}
static void poly(uint8_t*p,int x,int y,const int*xy,int n,int c){int top=50,bottom=-50;for(int i=0;i<n;i++){if(xy[i*2+1]<top)top=xy[i*2+1];if(xy[i*2+1]>bottom)bottom=xy[i*2+1];}for(int yy=top;yy<=bottom;yy++){int l=100,r=-100;for(int i=0;i<n;i++){int j=(i+1)%n,x0=xy[i*2],y0=xy[i*2+1],x1=xy[j*2],y1=xy[j*2+1];if(yy<(y0<y1?y0:y1)||yy>(y0>y1?y0:y1))continue;if(y0==y1){if(x0<l)l=x0;if(x1<l)l=x1;if(x0>r)r=x0;if(x1>r)r=x1;}else{int xx=x0+(x1-x0)*(yy-y0)/(y1-y0);if(xx<l)l=xx;if(xx>r)r=xx;}}if(l<=r)rect(p,x+l,y+yy,x+r,y+yy,c);}}
static int world_y(const world*w,int d){return HERO_LINE+w->scroll-d;}
static void crocodile(uint8_t*p,int x,int y,int age){
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
static void hazard(uint8_t*p,int x,int y,int kind,int t,int bite){
 if(y<-60 || y>300)return;
 if(kind==CROCODILE){crocodile(p,x,y,bite);return;}
 if(kind==ROCK){
  static const int edge[]={-17,3,-15,-7,-8,-14,5,-13,14,-6,17,4,10,13,-9,12};
  static const int body[]={-15,3,-13,-6,-7,-12,4,-11,12,-5,15,3,9,11,-8,10};
  static const int lit[]={-12,-5,-6,-11,0,-7,-4,3,-13,2};
  oval(p,x,y+12,21,6,1);oval(p,x+2,y+10,18,4,3);poly(p,x,y,edge,8,15);poly(p,x,y,body,8,8);poly(p,x,y,lit,5,7);
  line(p,x+3,y-7,x+9,y+6,9);line(p,x+9,y+6,x+2,y+10,9);line(p,x-6,y+6,x-1,y+4,7);
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
static void spr(int id,int x,int y,int pal){const gm_sprite_def*s=&gm_spr[id];if(x<=-(int)s->w||x>=320||y<=-(int)s->h||y>=240)return;rs_spr(x,y,s->tile,s->w,s->h,pal<0?s->pal:pal,2,0);}
static void foreground(const world*w){uint8_t p[320*240]={0};
 for(int y=0;y<240;y++){int d=HERO_LINE+w->scroll-y,left,right;river_banks(w,d,&left,&right);
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
 int island_base=w->scroll/85;
 for(int k=island_base-3;k<=island_base+5;k++){
  int d=k*85,center,width=river_island(w,d,&center);
  if(width<22)continue;
  int x=center+(k&1?8:-8),y=world_y(w,d);
  oval(p,x+2,y+4,12,10,12);oval(p,x,y,12,10,11);oval(p,x-3,y-3,9,7,10);
  for(int j=0;j<3;j++){line(p,x-3+j*3,y+13,x-5+j*3,y+6,10);line(p,x-3+j*3,y+13,x+j*3,y+7,11);}
 }
 // Reeds, grassy banks and rounded foliage move with the river, never with a separate camera.
 int base=w->scroll/70;
 for(int i=base-3;i<base+5;i++){int y=world_y(w,i*70),left,right;river_banks(w,i*70,&left,&right);int side=(i&1)?right+12:left-12;
  oval(p,side,y,13,12,12);oval(p,side-2,y-3,11,9,11);oval(p,side-4,y-6,7,5,10);
  for(int k=0;k<3;k++){int x=side+(i&1?-6:6)+k*2;line(p,x,y+24,x-3,y+12-k*2,12);line(p,x,y+24,x+2,y+14-k*2,10);}
 }
 // Surface ripples and current streaks. Contrast stays below rocks and players.
 for(int row=w->scroll/24-4;row<w->scroll/24+9;row++)for(int col=0;col<5;col++){
  int d=row*24,yy=world_y(w,d),x=45+col*51+((row*13+col*7)&15),center,left,right;
  river_banks(w,d,&left,&right);if(x<left+12||x>right-22)continue;
  int width=river_island(w,d,&center);if(width&&abs(x-center)<width+15)continue;
  int current=river_current(d,x),lean=current>0?9:current<0?-9:0;
  line(p,x,yy,x+8+lean,yy-1,1);line(p,x+4,yy+4,x+9+lean,yy+3,2);
  if(lean){int end=x+8+lean;line(p,end,yy-1,end-(lean>0?4:-4),yy-4,1);}
 }
 int first=course_index(w->scroll-180);
 for(int id=first>0?first-1:0;id<=first+4;id++){
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
 int next=1;for(int y=0;y<30;y++)for(int x=0;x<40;x++){uint8_t tile[64];int ink=0;for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++){tile[yy*8+xx]=p[(y*8+yy)*320+x*8+xx];ink|=tile[yy*8+xx];}int id=0;if(ink){id=next++;rs_tiles_load8(VR_BG2+id,tile,1);}rs_bg_put(RS_BG2,x,y,RS_MAP(id,1,0,0,0));}
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
void draw_frame(const world*w,int state,int t,int best,int fresh,int paused){hu_clear();rs_oam_clear();rs_brightness(paused?9:15);foreground(w);
 if(state==ST_TITLE){title(w,t,best);scene(w);}else if(state==ST_PLAY||state==ST_DEAD){if(w->players==1)hu_number(w->h[0].score,160,8,3);else for(int p=0;p<w->players;p++){char b[48],tag[16];rs_strf(tag,sizeof tag,HTR(PLAYER_TAG),p+1);snprintf(b,sizeof b,"%s %d",tag,w->h[p].score);hu_text(p*10,1,b);}scene(w);if(paused)hu_big(hu_center(HTR(PAUSED),1),13,HTR(PAUSED),HU_BIG_FREE);
 }else{int slide=hu_slide_in(t,20,200);if(w->players==1){static const int med[4]=MEDAL_SCORES;hu_banner(4,HTR(GAME_OVER));hu_gameover_panel(1,fresh,w->h[0].score,0);hu_gameover_sprites(1,w->h[0].score,0,best,hu_medal_of(w->h[0].score,med),t,slide);hu_retry_line(t,RETRY_LOCK,HTR(BACK_TITLE));}else{int scores[4];for(int p=0;p<w->players;p++)scores[p]=w->h[p].score;hu_standing s;hu_rank(&s,w->players,scores,scores);hu_results_panel(&s,NULL);hu_results_sprites(&s,t,slide);for(int i=0;i<s.n;i++){int x,y;hu_results_icon_pos(&s,i,&x,&y);spr(SPR_HERO_ICON,x-8,y-8+slide,hu_player_pal(s.order[i]));}hu_retry_line_at(hu_results_retry_row(&s),t,RETRY_LOCK,HTR(BACK_TITLE));}rs_bg_scroll(RS_BG1,0,-slide);}
 if(state!=ST_OVER)rs_bg_scroll(RS_BG1,0,0);
 rs_bg_scroll(RS_BG2,0,0);rs_viewports(0,NULL,0);
}
void draw_state(void){rs_state_var("draw.water",water,sizeof water);RS_STATE_RASTER(raster);}
