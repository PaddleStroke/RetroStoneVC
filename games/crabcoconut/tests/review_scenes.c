/* Full-size crabs and the scattered, continuously replenished shell rain. */
#include "game.h"
#include "rs_host.h"
#include "rs_desktop.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 rs_host_set_option("seed","4");rs_host_set_option("strict","1");rs_host_set_option("music","0");rs_host_set_option("sound","0");
 assert(!rs_host_init(rs_game_main()));
 for(int n=1;n<=4;n++){
  world w;world_init(&w,n,4);int input[4]={2,2,2,2};world_step(&w,input);
  for(int t=0;t<=1800;t++){
   if(t==0 || t==600 || t==1800){
    draw_frame(&w,ST_PLAY,t,0,0,0);rs_host_render();char path[180];
    snprintf(path,sizeof path,"build/crabcoconut-review/rain-%d-players-%d.png",t,n);assert(!rsd_write_png(path,rs_host_framebuffer(),3));
   }
   for(int p=0;p<n;p++)input[p]=bot_decide(&w,p);
   world_step(&w,input);
  }
 }
 world w;world_init(&w,1,4);int input[4]={2};world_step(&w,input);
 for(int i=1;i<SHELLS;i++)w.shells[i].hp=0;
 w.shells[0].x=90;w.shells[0].y=SHELL_LIMIT*Q16_ONE;input[0]=0;world_step(&w,input);
 for(int t=0;t<=25;t++){
  if(t==0 || t==10 || t==15 || t==20 || t==25){
   draw_frame(&w,ST_DEAD,t,0,0,0);rs_host_render();char path[180];
   snprintf(path,sizeof path,"build/crabcoconut-review/fallout-%02d.png",t);assert(!rsd_write_png(path,rs_host_framebuffer(),3));
  }
  world_step(&w,input);
 }
 for(int k=0;k<RS_WARN_KIND_COUNT;k++)assert(!rs_warn_count(k));
 rs_host_shutdown();return 0;
}
