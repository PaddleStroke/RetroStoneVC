/* Shared-screen visibility, full-height flight and a continuous title transition. */
#include "game.h"
#include "rs_host.h"
#include "house_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint16_t picture[320*240];
static void check_heroes(int n,int pixels){
 for(int p=0;p<n;p++){
  int found=-1,pal=p?hu_player_pal(p):0;
  for(int i=0;i<rs_oam_next();i++){const rs_sprite *s=rs_oam(i);if(s->w==32 && s->h==32 && s->pal==pal){assert(found==-1);found=i;}}
  assert(found>=0);rs_sprite *s=rs_oam(found);
  assert(s->x==hero_start_x(n,p)-16 && s->y>=0 && s->y+s->h<=240);
  if(!pixels)continue;
  memcpy(picture,rs_host_framebuffer(),sizeof picture);s->flags|=RS_SPR_HIDE;rs_host_render();
  int changed=0;for(int i=0;i<320*240;i++)changed+=picture[i]!=rs_host_framebuffer()[i];
  assert(changed>30);s->flags&=~RS_SPR_HIDE;rs_host_render();
 }
}
int main(void){
 rs_host_set_option("seed","4");rs_host_set_option("ready","1");rs_host_set_option("strict","1");
 rs_host_set_option("music","0");rs_host_set_option("sound","0");
 for(int n=1;n<=4;n++){
  char count[4];snprintf(count,sizeof count,"%d",n);rs_host_set_option("bot",count);assert(!rs_host_init(rs_game_main()));
  for(int t=0;t<1500;t++){rs_host_frame();check_heroes(n,t%200==0);}
  world w;world_init(&w,n,4);int held[4]={1,1,1,1};world_step(&w,held);
  for(int p=0;p<n;p++)w.h[p].y=(p&1?FLIGHT_TOP:FLIGHT_BOTTOM)*Q16_ONE;
  draw_frame(&w,ST_PLAY,50,0,0,0);rs_host_render();check_heroes(n,1);
  for(int k=0;k<RS_WARN_KIND_COUNT;k++)assert(!rs_warn_count(k));
  rs_host_shutdown();
 }
 assert(!rs_host_init(rs_game_main()));world w;world_init(&w,1,4);
 draw_frame(&w,ST_TITLE,80,0,0,0);rs_sprite title={0};
 for(int i=0;i<rs_oam_next();i++){const rs_sprite *s=rs_oam(i);if(s->w==32 && s->h==32 && s->pal==0)title=*s;}
 int held[4]={1};world_step(&w,held);draw_frame(&w,ST_PLAY,0,0,0,0);int found=0;
 for(int i=0;i<rs_oam_next();i++){const rs_sprite *s=rs_oam(i);if(s->w==32 && s->h==32 && s->pal==0){assert(s->x==title.x && s->y==title.y && s->tile==title.tile);found++;}}
 assert(found==1);rs_host_shutdown();puts("shared-screen 1-4 player visibility, full-height bounds and title continuity passed");return 0;
}
