/* Reproducible visual checks for every hazard family and toast launch frame. */
#include "game.h"
#include "rs_host.h"
#include "rs_desktop.h"
#include <assert.h>
#include <stdio.h>
static void snapshot(int id,int t,const char *name){
 world w;world_init(&w,1,4);obstacle o;obstacle_at(&w,id,t,&o);w.t=t;
 w.scroll=o.x-200;w.h[0].wx=(int64_t)(w.scroll+72)*Q16_ONE;w.h[0].y=174*Q16_ONE;w.h[0].state=HS_RUN;
 draw_frame(&w,ST_PLAY,80,0,0,0);rs_host_render();char path[180];
 snprintf(path,sizeof path,"build/mouseespresso-review/%s.png",name);assert(!rsd_write_png(path,rs_host_framebuffer(),3));
}
int main(void){
 rs_host_set_option("seed","4");rs_host_set_option("strict","1");rs_host_set_option("music","0");rs_host_set_option("sound","0");
 assert(!rs_host_init(rs_game_main()));
 for(int id=0;id<30;id++){
  world w;world_init(&w,1,4);obstacle o;obstacle_at(&w,id,0,&o);
  if(id>=7 && o.type!=OB_APPLIANCE && o.type!=OB_STOOL && o.type!=OB_SHELF && o.type!=OB_TABLE)continue;
  w.scroll=o.x-230;w.h[0].wx=(int64_t)(w.scroll+72)*Q16_ONE;w.h[0].y=180*Q16_ONE;w.h[0].state=HS_RUN;
  if(o.type==OB_TRAP)w.t=(o.period-id*31%o.period)%o.period+6;
  draw_frame(&w,ST_PLAY,80,0,0,0);rs_host_render();char path[160];
  snprintf(path,sizeof path,"build/mouseespresso-review/hazard-%02d-%d.png",id,o.type);
  assert(!rsd_write_png(path,rs_host_framebuffer(),3));
 }
 int paw_seen=0,appliance_seen=0;
 for(int id=0;id<100;id++){
  world w;world_init(&w,1,4);obstacle o;obstacle_at(&w,id,0,&o);
  if(o.type==OB_PAW && !(paw_seen&(1<<o.side))){
   int warning=o.period-PAW_ATTACK_FRAMES-PAW_WARNING_FRAMES;
   paw_seen|=1<<o.side;int phase[8]={0,warning+11,warning+22,warning+32,o.period-46,o.period-40,o.period-18,o.period-4};
   const char *name[8]={"rest","peek","tentative-out","tentative-in","extend","swat","retract","hidden"};
   for(int j=0;j<8;j++){char label[96];snprintf(label,sizeof label,"paw-%s-%s",o.side?"lower":"upper",name[j]);snapshot(id,(phase[j]-id*47%o.period+o.period)%o.period,label);}
  }
  if(o.type==OB_APPLIANCE && !(appliance_seen&(1<<o.variant))){
   appliance_seen|=1<<o.variant;
   if(o.variant==1)for(int j=0;j<8;j++){char label[80];snprintf(label,sizeof label,"mixer-spin-%d",j);snapshot(id,j*12,label);}
   else{int phase[3]={0,o.period-72,o.period-52};const char *name[3]={"off","warning","burst"};
    for(int j=0;j<3;j++){char label[80];snprintf(label,sizeof label,"%s-steam-%s",o.variant==0?"espresso":"kettle",name[j]);snapshot(id,(phase[j]-id*41%o.period+o.period)%o.period,label);}
   }
  }
 }
 for(int age=0;age<=84;age++){
  world w;world_init(&w,1,4);obstacle o;obstacle_at(&w,1,0,&o);w.t=(o.period-37%o.period)%o.period+age;
  w.scroll=o.x-180;w.h[0].wx=(int64_t)(w.scroll+72)*Q16_ONE;w.h[0].y=42*Q16_ONE;w.h[0].state=HS_RUN;
  draw_frame(&w,ST_PLAY,80,0,0,0);rs_host_render();char path[160];
  snprintf(path,sizeof path,"build/mouseespresso-review/toast-%02d.png",age);assert(!rsd_write_png(path,rs_host_framebuffer(),3));
 }
 for(int k=0;k<RS_WARN_KIND_COUNT;k++)assert(!rs_warn_count(k));
 rs_host_shutdown();return 0;
}
