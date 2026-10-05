#include "game.h"
#include "rs_host.h"
#include "house_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint16_t picture[320*240];
static void visible(int n,int pixels){
 for(int p=0;p<n;p++){
  int pal=p?hu_player_pal(p):0,found=-1;
  for(int i=0;i<rs_oam_next();i++){const rs_sprite *s=rs_oam(i);if(s->w==32 && s->h==32 && s->pal==pal){assert(found<0);found=i;}}
  assert(found>=0);rs_sprite *s=rs_oam(found);assert(s->x>=0 && s->x+s->w<=320 && s->y==player_y(n,p)-16);
  if(!pixels)continue;
  memcpy(picture,rs_host_framebuffer(),sizeof picture);s->flags|=RS_SPR_HIDE;rs_host_render();int changed=0;
  for(int i=0;i<320*240;i++)changed+=picture[i]!=rs_host_framebuffer()[i];
  assert(changed>30);s->flags&=~RS_SPR_HIDE;rs_host_render();
 }
}
int main(void){
 rs_host_set_option("seed","4");rs_host_set_option("ready","1");rs_host_set_option("strict","1");rs_host_set_option("music","0");rs_host_set_option("sound","0");
 for(int n=1;n<=4;n++){
  char count[4];snprintf(count,sizeof count,"%d",n);rs_host_set_option("bot",count);assert(!rs_host_init(rs_game_main()));
  for(int t=0;t<1200;t++){rs_host_frame();visible(n,t%200==0);}
  world w;world_init(&w,n,4);int start[4]={2,2,2,2};world_step(&w,start);
  for(int p=0;p<n;p++){w.h[p].x=160;w.h[p].xx=160*Q16_ONE;}
  draw_frame(&w,ST_PLAY,50,0,0,0);rs_host_render();visible(n,1); /* All penguins remain identifiable at the same lateral position. */
  for(int k=0;k<RS_WARN_KIND_COUNT;k++)assert(!rs_warn_count(k));
  rs_host_shutdown();
 }
 assert(!rs_host_init(rs_game_main()));world w;world_init(&w,1,4);draw_frame(&w,ST_TITLE,80,0,0,0);rs_sprite title={0};
 for(int i=0;i<rs_oam_next();i++){const rs_sprite *s=rs_oam(i);if(s->w==32 && s->h==32 && s->pal==0)title=*s;}
 int start[4]={2};world_step(&w,start);draw_frame(&w,ST_PLAY,0,0,0,0);int found=0;
 for(int i=0;i<rs_oam_next();i++){const rs_sprite *s=rs_oam(i);if(s->w==32 && s->h==32 && s->pal==0){assert(s->x==title.x && s->y==title.y && s->tile==title.tile);found++;}}
 assert(found==1);rs_host_shutdown();puts("shared-course visibility, overlapping lateral positions and title continuity passed");return 0;
}
