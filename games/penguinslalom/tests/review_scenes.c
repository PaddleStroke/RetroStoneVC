/* Reproducible screenshots of the preserved flags and new snow hazards. */
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
 world w;world_init(&w,1,4);w.started=1;w.scroll=1600;w.travel=(int64_t)w.scroll*Q16_ONE;
 w.h[0].state=HS_RUN;w.h[0].distance=w.scroll;w.h[0].wx=w.travel;
 w.bears[0]=(sliding_bear){1,0,248,w.travel-240*Q16_ONE,w.travel-240*Q16_ONE};
 for(int t=0;t<=160;t++){
  if(t%40==0){
   draw_frame(&w,ST_PLAY,t,0,0,0);rs_host_render();char path[180];
   snprintf(path,sizeof path,"build/penguinslalom-review/bear-slide-%03d.png",t);assert(!rsd_write_png(path,rs_host_framebuffer(),3));
  }
  int speed=course_speed(w.scroll);w.t++;w.travel+=speed;w.scroll=(int)(w.travel>>16);
  w.h[0].distance=w.scroll;w.h[0].wx=w.travel;w.bears[0].previous=w.bears[0].distance;w.bears[0].distance+=speed+3*Q16_ONE;
 }
 for(int k=0;k<RS_WARN_KIND_COUNT;k++)assert(!rs_warn_count(k));
 rs_host_shutdown();return 0;
}
