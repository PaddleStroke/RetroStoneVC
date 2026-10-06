/* Reproducible pictures of every pattern kind and of the moving items' phases (build/mouseespresso-review/). */
#include "game.h"
#include "rs_host.h"
#include "rs_desktop.h"
#include <assert.h>
#include <stdio.h>
static void shot(world *w,int x,int t,const char *name){
 w->t=t;w->scroll=x;w->travel=(int64_t)x*Q16_ONE;w->h[0].wx=(int64_t)(x+72)*Q16_ONE;w->h[0].y=120*Q16_ONE;w->h[0].state=HS_RUN;
 draw_frame(w,ST_PLAY,80,0,0,0);rs_host_render();char path[180];
 snprintf(path,sizeof path,"build/mouseespresso-review/%s.png",name);assert(!rsd_write_png(path,rs_host_framebuffer(),3));
}
int main(void){
 static const char *KIND[PK_COUNT]={"breather","alternate","narrow","stairs","zigzag","cluster","corridor","jets","ladles","bouncers"};
 rs_host_set_option("seed","4");rs_host_set_option("strict","1");rs_host_set_option("music","0");rs_host_set_option("sound","0");
 assert(!rs_host_init(rs_game_main()));
 static world w;int seen=0;
 for(uint32_t seed=1;seed<=8 && seen!=(1<<PK_COUNT)-1;seed++){
  world_init(&w,1,seed);
  for(int x=0;x<20000 && seen!=(1<<PK_COUNT)-1;x+=150){
   course_prune(&w.c,x-KEEP_BEHIND);uint32_t p0=w.c.npat;course_fill(&w.c,x+GEN_AHEAD);
   for(uint32_t i=p0;i<w.c.npat;i++){const pattern_rec *p=course_pattern(&w.c,i);if(seen>>p->kind&1)continue;
    seen|=1<<p->kind;char name[64];
    for(int j=0;j<3;j++){snprintf(name,sizeof name,"pattern-%s-%d",KIND[p->kind],j);shot(&w,p->x0-40,j*40,name);}}
  }
 }
 for(int k=0;k<RS_WARN_KIND_COUNT;k++)assert(!rs_warn_count(k));
 rs_host_shutdown();return 0;
}
