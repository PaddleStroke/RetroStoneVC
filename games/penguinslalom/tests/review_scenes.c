/* Reproducible screenshots of the preserved flags, the snow hazards and a bear's warning, landing and slide. */
#include "game.h"
#include "rs_host.h"
#include "rs_desktop.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 rs_host_set_option("seed","4");rs_host_set_option("strict","1");rs_host_set_option("music","0");rs_host_set_option("sound","0");
 assert(!rs_host_init(rs_game_main()));
 for(int id=0;id<12;id++)for(int frame=0;frame<(id==3?2:1);frame++){
  world w;world_init(&w,1,4);slope_obstacle o;w.t=frame*25;course_obstacle(&w,id,0,w.t,&o);
  w.scroll=o.distance-80;w.travel=(int64_t)w.scroll*Q16_ONE;w.h[0].distance=w.scroll;w.h[0].wx=w.travel;w.h[0].state=HS_RUN;
  w.h[0].score=w.scroll/12;w.h[0].anchor=id;
  draw_frame(&w,ST_PLAY,80,0,0,0);rs_host_render();char path[180];
  snprintf(path,sizeof path,"build/penguinslalom-review/obstacle-%02d-%d.png",id,frame);assert(!rsd_write_png(path,rs_host_framebuffer(),3));
 }
 /* The generator's own first bear (seed 4, 1 and 4 players, the bots steering): the marker as it grows, the
  * landing, the slide over the lineup. bear-<players>p-<frames since the warning>.png */
 for(int players=1;players<=4;players+=3){
  world w;world_init(&w,players,4);int start[4]={2,2,2,2};world_step(&w,start);
  int warned=-1;
  for(int t=0;t<2000;t++){
   for(int p=0;p<players;p++){w.h[p].state=HS_RUN;w.h[p].t=0;}
   int input[4]={0};for(int p=0;p<players;p++)input[p]=bot_decide(&w,p);
   world_step(&w,input);
   if(w.bear_warned && warned<0)warned=t;
   int age=warned<0?-1:t-warned;
   if(age==0 || age==20 || age==40 || age==55 || age==65 || age==80 || age==100){
    draw_frame(&w,ST_PLAY,t,0,0,0);rs_host_render();char path[180];
    snprintf(path,sizeof path,"build/penguinslalom-review/bear-%dp-%03d.png",players,age);assert(!rsd_write_png(path,rs_host_framebuffer(),3));
   }
   if(age>=100)break;
  }
  assert(warned>=0);
 }
 for(int k=0;k<RS_WARN_KIND_COUNT;k++)assert(!rs_warn_count(k));
 rs_host_shutdown();return 0;
}
