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
 w.t=phase*60;w.seed=4+phase*3;
 for(int scroll=0;scroll<1500;scroll++){
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
 int thorns=0,inclined=0;
 for(int id=0;id<30;id++){
  obstacle o;obstacle_at(&w,id,&o);if(o.type!=0)continue;thorns++;
  w.scroll=o.x-160;scenery_pixels(&w,after,320);int side=o.side?1:-1;
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
