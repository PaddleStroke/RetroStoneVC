/* Every live character must remain visible in its own clipped, independent view. */
#include "game.h"
#include "rs_host.h"
#include "house_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static uint16_t picture[320*240];
static void check_heroes(int players,const rs_viewport *rect,int pixels) {
 for(int p=0;p<players;p++) {
  int found=-1,pal=p?hu_player_pal(p):0;
  for(int i=0;i<rs_oam_next();i++) {
   const rs_sprite *s=rs_oam(i);
   if(s->w==(rect[p].h<200?16:32) && s->h==(rect[p].h<200?16:32) && s->pal==pal){assert(found==-1);found=i;}
  }
  assert(found>=0);rs_sprite *s=rs_oam(found);
  assert(s->x>=0 && s->y>=0 && s->x+s->w<=rect[p].w && s->y+s->h<=rect[p].h);
  if(!pixels)continue;
  memcpy(picture,rs_host_framebuffer(),sizeof picture);s->flags|=RS_SPR_HIDE;rs_host_render();
  int changed=0;
  for(int y=0;y<240;y++)for(int x=0;x<320;x++)if(picture[y*320+x]!=rs_host_framebuffer()[y*320+x]) {
   changed++;assert(x>=rect[p].x && x<rect[p].x+rect[p].w && y>=rect[p].y && y<rect[p].y+rect[p].h);
  }
  assert(changed>30);s->flags&=~RS_SPR_HIDE;rs_host_render();
 }
}
static void place(world *w,int player,int anchor) {
 hero *h=&w->h[player];h->state=HS_RUN;h->anchor=anchor;h->score=anchor*10;
 int x,y;
#if GAME_FROG
 perch_pos(w,anchor,w->t,&x,&y);h->wx=(int64_t)x*Q16_ONE;h->y=192*Q16_ONE;
#else
 anchor_pos(w,anchor,&x,&y);h->rope=66;h->wx=(int64_t)x*Q16_ONE;h->y=(y+66)*Q16_ONE;
#endif
 h->x=x;
}
static int flower_zero(int *x,int *y){
 for(int i=0;i<rs_oam_next();i++){const rs_sprite *s=rs_oam(i);if((s->pal==2 || s->pal==6 || s->pal==7) && s->w==24 && s->h==24){*x=s->x+12;*y=s->y+12;return 1;}}
 return 0;
}
static int vine_ink(int x,int y){
 assert(x>=0 && x<320 && y>=0 && y<240);rs_color c=rs_host_framebuffer()[y*320+x];
 static const rs_color green[4]={RS_HEX(0x345951),RS_HEX(0x4d8064),RS_HEX(0xc3dc98),RS_HEX(0x83b475)};
 for(int i=0;i<4;i++){unsigned r=green[i]&31,g=(green[i]>>5)&31,b=(green[i]>>10)&31;uint16_t rgb=(uint16_t)((r<<11)|((g*2+g/16)<<5)|b);if(c==rgb)return i+1;}
 return 0;
}
static void fixed_vines(void){
 world w;world_init(&w,1,4);place(&w,0,0);int ax,ay,fx,fy,nx,ny;anchor_pos(&w,0,&ax,&ay);anchor_pos(&w,1,&nx,&ny);
 w.h[0].wx=(int64_t)(320-nx)*Q16_ONE; /* Keep both vines inside the initial camera. */
 draw_frame(&w,ST_TITLE,40,0,0,0);draw_frame(&w,ST_PLAY,40,0,0,0);rs_host_render();assert(flower_zero(&fx,&fy));
 int ink[2][80][40],count=0;
 for(int side=0;side<2;side++)for(int y=0;y<80;y++)for(int x=0;x<40;x++){
  ink[side][y][x]=vine_ink(fx-ax+(side?270:6)+x,fy-ay+112+y);count+=ink[side][y][x]>0;
 }
 assert(count>300);
 /* Move only the camera by moving the spider. Every stem and leaf must stay fixed relative to the flowers. */
 w.h[0].y-=28*Q16_ONE;w.h[0].wx+=24*Q16_ONE;
 for(int t=0;t<30;t++){
  draw_frame(&w,ST_PLAY,40,0,0,0);rs_host_render();assert(flower_zero(&fx,&fy));
  for(int side=0;side<2;side++)for(int y=0;y<80;y++)for(int x=0;x<40;x++)
   assert(ink[side][y][x]==vine_ink(fx-ax+(side?270:6)+x,fy-ay+112+y));
 }
}
int main(void) {
 rs_host_set_option("bot","4");rs_host_set_option("seed","4");rs_host_set_option("ready","1");
 rs_host_set_option("strict","1");rs_host_set_option("music","0");rs_host_set_option("sound","0");
 assert(!rs_host_init(rs_game_main()));rs_viewport rect[4];rs_viewport_layout(4,0,rect);
 for(int t=0;t<1200;t++) {
  rs_host_frame();assert(rs_viewport_count()==4);check_heroes(4,rect,t%100==0);
 }
 world w;world_init(&w,2,4);place(&w,0,0);place(&w,1,40);
 rs_viewport_layout(2,0,rect);draw_frame(&w,ST_PLAY,50,0,0,0);rs_host_render();
 check_heroes(2,rect,1);memcpy(picture,rs_host_framebuffer(),sizeof picture);
 place(&w,0,120);draw_frame(&w,ST_PLAY,50,0,0,0);rs_host_render();
 for(int y=rect[1].y;y<rect[1].y+rect[1].h;y++)for(int x=rect[1].x;x<rect[1].x+rect[1].w;x++)
  assert(picture[y*320+x]==rs_host_framebuffer()[y*320+x]);
 check_heroes(2,rect,1);
 /* Track real release/steering inputs through catches: neither the body nor its camera may jump. */
 for(int n=1;n<=4;n++){
  world_init(&w,n,4);rs_viewport_layout(n,0,rect);int lastx[4]={0},lasty[4]={0},catches=0;
  for(int t=0;t<2400;t++){
   int input[4];for(int p=0;p<n;p++)input[p]=bot_decide(&w,p);world_step(&w,input);
   if(w.events[0]&EV_SCORE)catches++;
   draw_frame(&w,ST_PLAY,t,0,0,0);rs_host_render();check_heroes(n,rect,t%200==0);
   for(int p=0;p<n;p++){int pal=p?hu_player_pal(p):0;for(int i=0;i<rs_oam_next();i++){const rs_sprite *sp=rs_oam(i);if(sp->pal==pal && sp->w==(rect[p].h<200?16:32)){
    if(t){assert(abs(sp->x-lastx[p])<=12);assert(abs(sp->y-lasty[p])<=12);}lastx[p]=sp->x;lasty[p]=sp->y;
   }}}
  }
  assert(catches>12);
 }
 fixed_vines();
 for(int k=0;k<RS_WARN_KIND_COUNT;k++)assert(!rs_warn_count(k));
 rs_host_shutdown();puts("view checks: world-fixed vines, visible/clipped heroes, independent cameras and smooth catches for 1-4 players");return 0;
}
