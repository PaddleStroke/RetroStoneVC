#include "game.h"
#include "rs_host.h"
#include "house_ui.h"
#include "assets.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Widescreen (DESIGN.md "Widescreen"): every check at 320, 427, 520 and 560 columns. The views split the whole width
 * (stage coordinates: screen x = stage x + origin); each view's farmer stands at its centre (one view: the stage's). */
static uint16_t picture[RS_SCREEN_W_MAX*240];
static int W,O;
static void visible(int n,const rs_viewport *rect,int pixels){
 for(int p=0;p<n;p++){
  int width=rect[p].h<200?24:48,height=rect[p].h<200?32:48,pal=p?hu_player_pal(p):0,found=-1;
  for(int i=0;i<rs_oam_next();i++){const rs_sprite *s=rs_oam(i);if(s->w==width && s->h==height && s->pal==pal){assert(found<0);found=i;}}
  assert(found>=0);rs_sprite *s=rs_oam(found);assert(s->x>=0 && s->y>=32 && s->x+s->w<=rect[p].w && s->y+s->h<=rect[p].h);
  assert(abs(s->x+s->w/2-(n==1?RS_SCREEN_W/2:rect[p].w/2))<=1);
  if(!pixels)continue;
  memcpy(picture,rs_host_framebuffer(),sizeof(uint16_t)*W*240);s->flags|=RS_SPR_HIDE;rs_host_render();int changed=0;
  for(int y=0;y<240;y++)for(int x=0;x<W;x++)if(picture[y*W+x]!=rs_host_framebuffer()[y*W+x]){changed++;assert(x>=rect[p].x+O && x<rect[p].x+O+rect[p].w && y>=rect[p].y && y<rect[p].y+rect[p].h);}
  assert(changed>30);s->flags&=~RS_SPR_HIDE;rs_host_render();
 }
}
/* the garden goes on beside the stage: the margins show the scenery, not a flat fill */
static void margins(void){
 if(W==RS_SCREEN_W)return;
 const uint16_t *f=rs_host_framebuffer();
 for(int side=0;side<2;side++){
  uint16_t seen[64];int k=0;
  for(int y=0;y<240;y++)for(int x=0;x<O;x++){uint16_t c=f[y*W+(side?W-1-x:x)];int i=0;while(i<k && seen[i]!=c)i++;if(i==k && k<64)seen[k++]=c;}
  assert(k>=12);
 }
}
int main(void){
 static const int widths[]={320,427,520,560};
 rs_host_set_option("seed","4");rs_host_set_option("ready","1");rs_host_set_option("strict","1");rs_host_set_option("music","0");rs_host_set_option("sound","0");
 for(int wi=0;wi<4;wi++)for(int n=1;n<=4;n++){
  char count[4];snprintf(count,sizeof count,"%d",n);rs_host_set_option("bot",count);rs_host_set_width(widths[wi]);assert(!rs_host_init(rs_game_main()));
  W=rs_screen_width();O=rs_screen_origin_x();assert(W==widths[wi]);rs_viewport rect[4];rs_viewport_layout(n,0,rect);
  for(int t=0;t<1200;t++){rs_host_frame();visible(n,rect,t%200==0);if(n==1 && t==600)margins();}
  // Actual OAM tiles must match all four orientations at both farmer sizes.
  world facing;world_init(&facing,n,4);
  for(int d=0;d<4;d++)for(int action=0;action<2;action++){
   for(int p=0;p<n;p++){facing.h[p].state=HS_RUN;facing.h[p].aim=d;facing.h[p].smooth_t=action?6:0;}
   draw_frame(&facing,ST_PLAY,0,0,0,0);rs_host_render();visible(n,rect,1);
   for(int p=0;p<n;p++){
    int small=rect[p].h<200,pal=p?hu_player_pal(p):0;
    int id=small?(action?SPR_FARMER_SMALL_STRIKE:SPR_FARMER_SMALL):(action?SPR_HERO_AIR:SPR_HERO_IDLE);
    if(d==0||d==2){id=small?(d==0?SPR_FARMER_SMALL_BACK:SPR_FARMER_SMALL_FRONT):(d==0?SPR_FARMER_BACK:SPR_FARMER_FRONT);id+=action;}
    int found=0;for(int i=0;i<rs_oam_next();i++){const rs_sprite*s=rs_oam(i);if(s->pal==pal&&s->w==(small?24:48)&&s->h==(small?32:48)){assert(s->tile==gm_spr[id].tile);assert(!!(s->flags&RS_SPR_HFLIP)==(d==3));found++;}}
    assert(found==1);
   }
  }
  if(n==4){world w;world_init(&w,4,4);int start[4]={16,16,16,16};world_step(&w,start);for(int p=0;p<4;p++){w.h[p].state=HS_RUN;w.h[p].x=160;}
   draw_frame(&w,ST_PLAY,50,0,0,0);rs_host_render();visible(4,rect,1);memcpy(picture,rs_host_framebuffer(),sizeof(uint16_t)*W*240);w.h[0].x=40;
   draw_frame(&w,ST_PLAY,50,0,0,0);rs_host_render();for(int p=1;p<4;p++)for(int y=rect[p].y;y<rect[p].y+rect[p].h;y++)for(int x=rect[p].x+O;x<rect[p].x+O+rect[p].w;x++)assert(picture[y*W+x]==rs_host_framebuffer()[y*W+x]);
  }
  for(int k=0;k<RS_WARN_KIND_COUNT;k++)assert(!rs_warn_count(k));
  rs_host_shutdown();
 }puts("1-4 player visibility, clipped sprites and independent player views passed at 320, 427, 520 and 560 columns");return 0;
}
