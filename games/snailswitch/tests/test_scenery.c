#include "game.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* This renderer fixture never starts a console; it inspects the real foreground pixels, at 4:3 and at the widescreen
 * widths (DESIGN.md "Widescreen": the canvas is the screen's tile columns: 320, 432 for 427, 520, 560). */
const rs_game *rs_game_main(void){return NULL;}
static uint8_t before[RS_SCREEN_W_MAX*240],after[RS_SCREEN_W_MAX*240],narrow[RS_SCREEN_W*240];
/* the foreground's tiles (draw.c foreground(): one tile per inked 8x8 cell, after the empty tile) must fit its
 * 512-tile VRAM range (VR_BG2 .. VR_BG3) */
static int inked_tiles(const uint8_t*px,int cw){
 int n=0;
 for(int r=0;r<30;r++)for(int c=0;c<cw/8;c++){
  int ink=0;
  for(int y=0;y<8;y++)for(int x=0;x<8;x++)ink|=px[(r*8+y)*cw+c*8+x];
  n+=ink!=0;
 }
 return n;
}
int main(void){
 static const int widths[]={320,432,520,560};
 world w;world_init(&w,1,4);w.t=37;
 for(int k=0;k<4;k++){
 int cw=widths[k],most=0;
 for(int phase=0;phase<4;phase++){
 /* four courses (seeds), at the start of a run and (phases 2, 3) further on, where the harder segments come */
 world_init(&w,1,4+phase*3);w.t=phase*60;
 int from=phase<2?0:9000*(phase-1);
 for(int scroll=from;scroll<from+1500;scroll++){
  w.scroll=scroll+1;course_fill(&w);         /* the course buffer as world_step keeps it (both pictures in it) */
  w.scroll=scroll;scenery_pixels(&w,before,cw);
  w.scroll=scroll+1;scenery_pixels(&w,after,cw);
  for(int y=0;y<240;y++)for(int x=0;x<cw-1;x++)
   assert(before[y*cw+x+1]==after[y*cw+x]);
  int n=inked_tiles(after,cw);if(n>most)most=n;
  if(cw>RS_SCREEN_W){      /* a wider screen shows MORE of the same world: its first 320 columns are the 4:3 picture */
   scenery_pixels(&w,narrow,RS_SCREEN_W);
   for(int y=0;y<240;y++)assert(!memcmp(narrow+y*RS_SCREEN_W,after+y*cw,RS_SCREEN_W));
  }
 }
 }
 assert(1+most<=512);
 printf("width %d: foreground enters/leaves by clipping, no leaf or fruit pop-in across 6000 camera positions and patrol "
        "phases%s; at most %d foreground tiles (512 free): passed\n",cw,cw>RS_SCREEN_W?", the 4:3 picture extended":"",1+most);
 }
 int thorns=0,inclined=0,done=-1000;
 world_init(&w,1,4);w.t=37;
 while(thorns<30&&w.scroll<200000){
  obstacle o;int found=0;
  for(int i=0;i<w.nitems&&!found;i++)if(w.it[i].kind==IT_THORN&&w.it[i].x>done){item_at(&w.it[i],w.t,&o);found=1;}
  if(!found){w.scroll+=400;course_fill(&w);continue;}
  done=o.x;thorns++;
  w.scroll=o.x-160;course_fill(&w);scenery_pixels(&w,after,320);int side=o.side?1:-1;
  for(int j=-1;j<=1;j++)for(int q=-3;q<=3;q++){
   int x=160+j*10+q,y=vine_y(w.scroll+x);
   assert(after[(y+side*7)*320+x]!=0);
   assert(after[(y+side*8)*320+x]!=0);
   assert(after[(y+side*9)*320+x]!=0);
  }
  inclined+=vine_y(o.x-10)!=vine_y(o.x+10);
 }
 assert(thorns>3&&inclined>0);
 puts("Thorn bases connected on inclined vine surfaces: passed");
 /* The start of a run (DESIGN.md "Widescreen", the owner's rule for scrollers): the first thing on the vine (a hazard
  * or a fruit: the first segment's) sits at the same distance from the leading snail at every width, 334..394 px
  * (world 450..510, the snail at 116): just past a 4:3 screen's edge, already in view from the first frame on a wider
  * screen. The course never reads the width (and the first 320 columns of every wider picture above, from scroll 0,
  * are the 4:3 picture). */
 for(uint32_t s=1;s<=200;s++){
  world a;world_init(&a,1,s);
  int d=a.it[0].x-a.scroll-a.h[0].x;
  assert(a.scroll==0&&a.nitems>0&&d>=334&&d<=394);
 }
 puts("The first thing on the vine 334..394 px ahead of the snails at the start of a run, at every width: passed");
 /* Widescreen: a third of the extra width is shown BEHIND the snails, on the left (draw.c draws the foreground from
  * scroll - WIDE_LEFT(width - 320), the stage origin): behind the start of a run the vine goes on, no empty column. */
 static const int real[]={427,520,560};
 for(int k=0;k<3;k++){
  int cw=(real[k]+7)/8*8,left=WIDE_LEFT(real[k]-RS_SCREEN_W);
  assert(left==(real[k]==427?36:real[k]==520?67:80));
  for(uint32_t s=1;s<=20;s++){
   world a;world_init(&a,1,s);a.scroll=-left;scenery_pixels(&a,after,cw);
   for(int x=0;x<left;x++){int ink=0;for(int y=0;y<240;y++)ink|=after[y*cw+x];assert(ink);}
  }
 }
 puts("Widescreen: a third of the extra width on the left (36, 67, 80 px), the vine drawn behind the start: passed");
 return 0;
}
