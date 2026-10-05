#include "game.h"
#include "rs_host.h"
#include "house_ui.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint16_t picture[320*240];
static void visible(int n,const rs_viewport *rect,int pixels){
 for(int p=0;p<n;p++){
  int size=32,pal=p?hu_player_pal(p):0,found=-1;
  for(int i=0;i<rs_oam_next();i++){const rs_sprite *s=rs_oam(i);if(s->w==size && s->h==size && s->pal==pal){assert(found<0);found=i;}}
  assert(found>=0);rs_sprite *s=rs_oam(found);assert(s->x>=0 && s->y>=32 && s->x+s->w<=rect[p].w && s->y+s->h<=rect[p].h);
  if(!pixels)continue;
  unsigned flags[128];int oam_count=rs_oam_next();assert(oam_count<=128);
  for(int i=0;i<oam_count;i++){rs_sprite *other=rs_oam(i);flags[i]=other->flags;if(i!=found && other->w==32 && other->h==32)other->flags|=RS_SPR_HIDE;}
  rs_host_render();memcpy(picture,rs_host_framebuffer(),sizeof picture);s->flags|=RS_SPR_HIDE;rs_host_render();int changed=0;
  for(int y=0;y<240;y++)for(int x=0;x<320;x++)if(picture[y*320+x]!=rs_host_framebuffer()[y*320+x]){changed++;assert(x>=rect[p].x && x<rect[p].x+rect[p].w && y>=rect[p].y && y<rect[p].y+rect[p].h);}
  assert(changed>30);for(int i=0;i<oam_count;i++)rs_oam(i)->flags=flags[i];rs_host_render();
 }
}
int main(void){
 rs_host_set_option("seed","4");rs_host_set_option("ready","1");rs_host_set_option("strict","1");rs_host_set_option("music","0");rs_host_set_option("sound","0");
 for(int n=1;n<=4;n++){
  char count[4];snprintf(count,sizeof count,"%d",n);rs_host_set_option("bot",count);assert(!rs_host_init(rs_game_main()));rs_viewport rect[4];for(int p=0;p<4;p++){memset(&rect[p],0,sizeof rect[p]);rect[p].w=320;rect[p].h=240;}
  for(int t=0;t<1200;t++){rs_host_frame();visible(n,rect,t%200==0);}
  if(n==4){world w;world_init(&w,4,4);int start[4]={2,2,2,2};world_step(&w,start);
   for(int p=0;p<4;p++){w.h[p].bx=(p&1?BALL_RIGHT:BALL_LEFT)*Q16_ONE;w.h[p].by=(p<2?BALL_TOP:PADDLE_Y)*Q16_ONE;}
   draw_frame(&w,ST_PLAY,50,0,0,0);rs_host_render();visible(4,rect,1);
   int balls=0,shells=0;for(int i=0;i<rs_oam_next();i++){const rs_sprite *s=rs_oam(i);if(s->pal==2){balls+=s->w==16 && s->h==16;shells+=s->w==24 && s->h==16;}}
   int count=0;for(int i=0;i<SHELLS;i++)count+=w.shells[i].hp>0;
   assert(balls==4 && shells==count); /* One shared scattered board, not four copies. */
   for(int p=0;p<4;p++)w.h[p].x=160;
   draw_frame(&w,ST_PLAY,50,0,0,0);rs_host_render();visible(4,rect,1);
  }
  for(int k=0;k<RS_WARN_KIND_COUNT;k++)assert(!rs_warn_count(k));
  rs_host_shutdown();
 }
 assert(!rs_host_init(rs_game_main()));world w;world_init(&w,1,4);memset(w.shells,0,sizeof w.shells);
 w.h[0].state=HS_HIT;w.shells[0]=(shell){90,1,0,0,Q16_ONE/20};
 const int heights[]={180,190,206,230,238};
 for(int k=0;k<5;k++){
  w.shells[0].y=heights[k]*Q16_ONE;draw_frame(&w,ST_DEAD,10,0,0,0);rs_host_render();
  int found=-1;for(int i=0;i<rs_oam_next();i++){const rs_sprite *s=rs_oam(i);if(s->w==24 && s->h==16 && s->pal==2){assert(found<0);found=i;}}
  assert(found>=0);memcpy(picture,rs_host_framebuffer(),sizeof picture);rs_oam(found)->flags|=RS_SPR_HIDE;rs_host_render();
  int changed=0;for(int i=0;i<320*240;i++)changed+=picture[i]!=rs_host_framebuffer()[i];assert(changed>30);
 }
 /* The results transition also retains a shell until it leaves the screen. */
 w.shells[0].y=232*Q16_ONE;draw_frame(&w,ST_OVER,60,0,0,0);rs_host_render();int found=0;
 for(int i=0;i<rs_oam_next();i++){const rs_sprite *s=rs_oam(i);found+=s->w==24 && s->h==16 && s->pal==2;}
 assert(found==1);rs_host_shutdown();
 puts("shells visible over sand through the bottom edge and results transition, full-screen 1-4 player court, independent coconuts and one shared shell board passed");return 0;
}
