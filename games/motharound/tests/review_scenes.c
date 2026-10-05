/* Visual checks of all opening counts, both materials, and the speed cap. */
#include "game.h"
#include "rs_host.h"
#include "rs_desktop.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 rs_host_set_option("seed","4");rs_host_set_option("strict","1");rs_host_set_option("music","0");rs_host_set_option("sound","0");
 assert(!rs_host_init(rs_game_main()));
 for(int n=1;n<=4;n+=3)for(int count=1;count<=3;count++)for(int kind=0;kind<2;kind++){
  world w;world_init(&w,n,4);w.started=1;w.t=2400;ring *r=&w.rings[0];
  r->active=1;r->openings=count;r->kind=kind;r->radius=112*Q16_ONE;r->angle=20*256;
  for(int k=0;k<count;k++){r->offset[k]=256*k/count;r->width[k]=38+k*10;}
  for(int p=0;p<n;p++){w.h[p].state=HS_RUN;w.h[p].angle=26+p*60;}
  draw_frame(&w,ST_PLAY,80,0,0,0);rs_host_render();char path[180];
  snprintf(path,sizeof path,"build/motharound-review/openings-%d-material-%d-players-%d.png",count,kind,n);
  assert(!rsd_write_png(path,rs_host_framebuffer(),3));
 }
 world w;world_init(&w,4,4);
 for(int t=0;t<18000;t++){
  int input[4];for(int p=0;p<4;p++)input[p]=bot_decide(&w,p);world_step(&w,input);
  if(t==120 || t==500 || t==15000){
   draw_frame(&w,ST_PLAY,t,0,0,0);rs_host_render();char path[180];
   snprintf(path,sizeof path,"build/motharound-review/live-%d.png",t);assert(!rsd_write_png(path,rs_host_framebuffer(),3));
  }
 }
 for(int k=0;k<RS_WARN_KIND_COUNT;k++)assert(!rs_warn_count(k));
 rs_host_shutdown();return 0;
}
