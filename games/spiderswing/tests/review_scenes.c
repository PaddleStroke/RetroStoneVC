/* Native-size flower combinations and the actual synthesised catch samples. */
#include "game.h"
#include "assets.h"
#include "rs_host.h"
#include "rs_desktop.h"
#include "house_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 rs_host_set_option("seed","4");rs_host_set_option("strict","1");
 rs_host_set_option("music","0");rs_host_set_option("sound","1");
 assert(!rs_host_init(rs_game_main()));
 world w;world_init(&w,1,4);draw_frame(&w,ST_TITLE,40,0,0,0);
 hu_clear();rs_oam_clear();rs_bg_enable(RS_BG2,0);
 static const char *names[FLOWER_TYPES]={"FUCHSIA","BELL","STAR","ROSETTE","ORCHID"};
 static const char *notes[FLOWER_TYPES]={"D5","E5","F#5","A5","B5"};
 static const int normal[FLOWER_TYPES]={SPR_FLOWER,SPR_FLOWER_BELL,SPR_FLOWER_STAR,SPR_FLOWER_ROSETTE,SPR_FLOWER_ORCHID};
 static const int small[FLOWER_TYPES]={SPR_FLOWER_SMALL,SPR_FLOWER_BELL_SMALL,SPR_FLOWER_STAR_SMALL,SPR_FLOWER_ROSETTE_SMALL,SPR_FLOWER_ORCHID_SMALL};
 hu_big(3,1,"FLOWER VARIETY",HU_BIG_FREE);
 for(int kind=0;kind<FLOWER_TYPES;kind++){
  int x=32+kind*64;
  hu_text((x-(int)strlen(names[kind])*4)/8,5,names[kind]);
  hu_text((x-(int)strlen(notes[kind])*4)/8,7,notes[kind]);
  for(int color=0;color<FLOWER_COLORS;color++){
   const gm_sprite_def *s=&gm_spr[normal[kind]];
   rs_spr(x-12,76+color*40,s->tile,24,24,color?5+color:2,2,0);
  }
  const gm_sprite_def *s=&gm_spr[small[kind]];
  rs_spr(x-8,202,s->tile,16,16,2,2,0);
 }
 hu_text(9,28,"5 SHAPES / 3 COLOURS");rs_host_render();
 assert(!rsd_write_png("build/spiderswing-review/flower-variety.png",rs_host_framebuffer(),3));
 for(int kind=0;kind<FLOWER_TYPES;kind++){
  char path[100];snprintf(path,sizeof path,"build/spiderswing-review/flower-note-%d.pcm",kind);
  FILE *out=fopen(path,"wb");assert(out);
  assert(rs_sfx(SFX_FLOWER+kind,88,64,0x1000)>=0);
  for(int t=0;t<24;t++){rs_host_frame();int n;const int16_t *data=rs_host_audio(&n);assert(fwrite(data,4,(size_t)n,out)==(size_t)n);}
  fclose(out);
 }
 for(int k=0;k<RS_WARN_KIND_COUNT;k++)assert(!rs_warn_count(k));
 rs_host_shutdown();return 0;
}
