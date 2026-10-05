#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "rs_desktop.h"
#include <stdio.h>
static world W;static int st=ST_PLAY;
static void init(void){draw_init();hu_title_cfg c={0,4,"PLAY",NULL,0};hu_title_setup(&c);hu_players_set(4);world_init(&W,4,7);}
static void draw(void){draw_frame(&W,st,W.t,0,0,0);}
const rs_game*rs_game_main(void){static const rs_game g={"Biscuit Bandit Review","biscuitbanditreview","1",init,NULL,draw,NULL,gm_assets,NULL,NULL,1};return &g;}
static void shot(const char*n){char path[256];snprintf(path,sizeof path,"games/biscuitbandit/docs/screenshots/%s.png",n);rs_host_frame();rsd_write_png(path,rs_host_framebuffer(),3);}
int main(void){rs_host_init(rs_game_main());st=ST_TITLE;shot("title-clean-4-players");st=ST_PLAY;world_step(&W,(int[4]){IN_START});for(int i=0;i<170;i++)world_step(&W,(int[4]){0});shot("all-four-alive");world_init(&W,1,7);hu_players_set(1);st=ST_TITLE;rs_host_frame();st=ST_PLAY;int loaded=0,tilt=0,delivered=0,warning=0;
for(int f=0;f<4000;f++){
 world_step(&W,(int[4]){bot_decide(&W,0)});
 if(!loaded&&W.h[0].load>=3){shot("loaded-tray");loaded=1;}
 if(!tilt&&W.h[0].load>=2&&W.h[0].tilt){shot("tilted-tray");tilt=1;}
 if(!delivered&&W.h[0].delivering==12){shot("basket-delivery");delivered=1;}
 for(int j=0;j<ITEM_MAX;j++)if(!warning&&W.items[j].active&&W.items[j].bad&&W.items[j].age==40){shot("cup-warning");warning=1;}
 if(loaded&&tilt&&delivered&&warning)break;
}rs_host_shutdown();return !(loaded&&tilt&&delivered&&warning);}

