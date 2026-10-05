#include "game.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* This renderer fixture never starts a console; it inspects the real foreground pixels. */
const rs_game *rs_game_main(void){return NULL;}
static uint8_t before[320*240],after[320*240];
int main(void){
 world w;world_init(&w,1,4);w.t=37;
 for(int phase=0;phase<4;phase++){
 w.t=phase*60;w.seed=4+phase*3;
 for(int scroll=0;scroll<1500;scroll++){
  w.scroll=scroll;scenery_pixels(&w,before);
  w.scroll=scroll+1;scenery_pixels(&w,after);
  for(int y=0;y<240;y++)for(int x=0;x<319;x++)
   assert(before[y*320+x+1]==after[y*320+x]);
 }
 }
 puts("Foreground enters/leaves by clipping, no leaf or fruit pop-in across 6000 camera positions and patrol phases: passed");
 int thorns=0,inclined=0;
 for(int id=0;id<30;id++){
  obstacle o;obstacle_at(&w,id,&o);if(o.type!=0)continue;thorns++;
  w.scroll=o.x-160;scenery_pixels(&w,after);int side=o.side?1:-1;
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
 return 0;
}
