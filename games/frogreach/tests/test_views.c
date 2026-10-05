/* Every live character must remain visible in its own clipped, independent view. */
#include "game.h"
#include "rs_host.h"
#include "house_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint16_t picture[320*240];
static void check_heroes(int players,const rs_viewport *rect,int pixels) {
 for(int p=0;p<players;p++) {
  int found=-1,pal=p?hu_player_pal(p):0;
  for(int i=0;i<rs_oam_next();i++) {
   const rs_sprite *s=rs_oam(i);
   if(s->w==(rect[p].w<240?16:32) && s->h==(rect[p].w<240?16:32) && s->pal==pal){assert(found==-1);found=i;}
  }
  assert(found>=0);rs_sprite *s=rs_oam(found);
  assert(s->x>=0 && s->y>=0 && s->x+s->w<=rect[p].w && s->y+s->h<=rect[p].h);
  if(!pixels)continue;
  memcpy(picture,rs_host_framebuffer(),sizeof picture);s->flags|=RS_SPR_HIDE;rs_host_render();
  int changed=0;
  for(int y=0;y<240;y++)for(int x=0;x<320;x++)if(picture[y*320+x]!=rs_host_framebuffer()[y*320+x]) {
   changed++;assert(x>=rect[p].x && x<rect[p].x+rect[p].w && y>=rect[p].y && y<rect[p].y+rect[p].h);
  }
  assert(changed>30);s->flags&=~RS_SPR_HIDE;rs_host_render();
 }
}
static void place(world *w,int player,int anchor) {
 hero *h=&w->h[player];h->state=HS_RUN;h->anchor=anchor;h->score=anchor*10;
 int x,y;
#if GAME_FROG
 perch_pos(w,anchor,w->t,&x,&y);h->wx=(int64_t)x*Q16_ONE;h->y=192*Q16_ONE;
#else
 anchor_pos(w,anchor,&x,&y);h->rope=66;h->wx=(int64_t)x*Q16_ONE;h->y=(y+66)*Q16_ONE;
#endif
 h->x=x;
}
int main(void) {
 rs_host_set_option("bot","4");rs_host_set_option("seed","4");rs_host_set_option("ready","1");
 rs_host_set_option("strict","1");rs_host_set_option("music","0");rs_host_set_option("sound","0");
 assert(!rs_host_init(rs_game_main()));rs_viewport rect[4];rs_viewport_layout(4,0,rect);
 for(int t=0;t<1200;t++) {
  rs_host_frame();assert(rs_viewport_count()==4);check_heroes(4,rect,t%100==0);
 }
 world w;world_init(&w,2,4);place(&w,0,0);place(&w,1,40);
 rs_viewport_layout(2,RS_LAYOUT_HSPLIT,rect);draw_frame(&w,ST_PLAY,50,0,0,0);rs_host_render();
 assert(rect[0].w==320 && rect[1].w==320 && rect[0].y==0 && rect[1].y>rect[0].h);
 check_heroes(2,rect,1);memcpy(picture,rs_host_framebuffer(),sizeof picture);
 place(&w,0,120);draw_frame(&w,ST_PLAY,50,0,0,0);rs_host_render();
 for(int y=rect[1].y;y<rect[1].y+rect[1].h;y++)for(int x=rect[1].x;x<rect[1].x+rect[1].w;x++)
  assert(picture[y*320+x]==rs_host_framebuffer()[y*320+x]);
 check_heroes(2,rect,1);
 /* The real title scene and the first started frame have identical frog and pad positions. */
 world_init(&w,1,4);draw_frame(&w,ST_TITLE,80,0,0,0);rs_host_render();
 rs_sprite title_frog={0};int found=0;
 for(int i=0;i<rs_oam_next();i++){const rs_sprite *sp=rs_oam(i);if(sp->pal==0 && sp->w==32 && sp->h==32){title_frog=*sp;found++;}}
 assert(found==1);int input[4]={1};world_step(&w,input);assert(w.h[0].mode==MODE_WAIT);
 draw_frame(&w,ST_PLAY,0,0,0,0);rs_host_render();found=0;
 for(int i=0;i<rs_oam_next();i++){const rs_sprite *sp=rs_oam(i);if(sp->pal==0 && sp->w==32 && sp->h==32){assert(sp->x==title_frog.x && sp->y==title_frog.y && sp->tile==title_frog.tile);found++;}}
 assert(found==1);
 for(int k=0;k<RS_WARN_KIND_COUNT;k++)assert(!rs_warn_count(k));
 rs_host_shutdown();puts("view checks: horizontal two-player split, visible/clipped frogs, independent cameras and identical title/start position");return 0;
}
