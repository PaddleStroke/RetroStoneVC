/* Small moths share one orbit; identical angles may overlap. */
#include "game.h"
#include "rs_host.h"
#include "house_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint16_t picture[320*240];
static void check_heroes(int n,const rs_viewport *rect,int pixels) {
 int lanterns=0;for(int i=0;i<rs_oam_next();i++){const rs_sprite *s=rs_oam(i);lanterns+=s->w==32 && s->h==48;}
 assert(lanterns==1);
 for(int y=5;y<=7;y++)for(int x=0;x<40;x++)assert(rs_bg_get(RS_BG1,x,y)==rs_bg_get(RS_BG1,0,7));
 for(int p=0;p<n;p++) {
  int size=16,found=-1,pal=p?hu_player_pal(p):0;
  for(int i=0;i<rs_oam_next();i++) {
   const rs_sprite *s=rs_oam(i);
   if(s->w==size && s->h==size && s->pal==pal){assert(found==-1);found=i;}
  }
  assert(found>=0);rs_sprite *s=rs_oam(found);
  assert(s->x>=0 && s->y>=32 && s->x+s->w<=rect[p].w && s->y+s->h<=rect[p].h);
  if(!pixels)continue;
  /* Check each submitted sprite in isolation: another moth may cover it at
     exactly the same position, which is intentional on the shared orbit. */
  unsigned flags[128];int oam_count=rs_oam_next();assert(oam_count<=128);
  for(int i=0;i<oam_count;i++){rs_sprite *other=rs_oam(i);flags[i]=other->flags;if(i!=found && other->w==16 && other->h==16)other->flags|=RS_SPR_HIDE;}
  rs_host_render();memcpy(picture,rs_host_framebuffer(),sizeof picture);s->flags|=RS_SPR_HIDE;rs_host_render();
  int changed=0;
  for(int y=0;y<240;y++)for(int x=0;x<320;x++)if(picture[y*320+x]!=rs_host_framebuffer()[y*320+x]) {
   changed++;assert(x>=rect[p].x && x<rect[p].x+rect[p].w && y>=rect[p].y && y<rect[p].y+rect[p].h);
  }
  assert(changed>30);for(int i=0;i<oam_count;i++)rs_oam(i)->flags=flags[i];rs_host_render();
 }
}
int main(void) {
 rs_host_set_option("seed","4");rs_host_set_option("ready","1");
 rs_host_set_option("strict","1");rs_host_set_option("music","0");rs_host_set_option("sound","0");
 for(int n=1;n<=4;n++) {
  char count[4];snprintf(count,sizeof count,"%d",n);rs_host_set_option("bot",count);
  assert(!rs_host_init(rs_game_main()));rs_viewport rect[4];for(int p=0;p<4;p++){memset(&rect[p],0,sizeof rect[p]);rect[p].w=320;rect[p].h=240;}
  for(int t=0;t<1000;t++){rs_host_frame();check_heroes(n,rect,t%200==0);}
  if(n==4) {
   world w;world_init(&w,4,4);int start[4]={2,2,2,2};world_step(&w,start);
   for(int a=0;a<256;a++) {
    for(int p=0;p<4;p++)w.h[p].angle=a;
    draw_frame(&w,ST_PLAY,50,0,0,0);rs_host_render();check_heroes(4,rect,a%64==0);
    int x=-1,y=-1;for(int i=0;i<rs_oam_next();i++){const rs_sprite *s=rs_oam(i);if(s->w==16 && s->h==16){if(x<0){x=s->x;y=s->y;}else assert(s->x==x && s->y==y);}}
   }
  }
  for(int k=0;k<RS_WARN_KIND_COUNT;k++)assert(!rs_warn_count(k));
  rs_host_shutdown();
 }
 puts("one lantern, small moths in solo and multiplayer, no gameplay instructions, complete shared orbit and intentional overlap passed");return 0;
}
