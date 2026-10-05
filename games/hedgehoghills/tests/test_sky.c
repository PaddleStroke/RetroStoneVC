/* Camera-height regression: scenery must never wrap ground into the sky. */
#include "game.h"
#include "rs_host.h"
#include <stdio.h>

int main(void) {
    static const int heights[]={-20,-80,-160,-256,-768,-2048,-8192,GROUND_Y};
    rs_host_set_option("music","0");
    rs_host_set_option("sound","0");
    rs_host_set_option("strict","1");
    if(rs_host_init(rs_game_main())) return 1;
    world w;
    world_init(&w,1,4);
    w.h[0].state=HS_RUN;
    for(int season=0;season<3;season++) {
        w.scroll=season*14000;
        w.h[0].y=GROUND_Y*Q16_ONE;
        for(int t=0;t<160;t++) draw_frame(&w,ST_PLAY,t,0,0,0);
        rs_host_render();
        uint16_t sky=rs_host_framebuffer()[0];
        for(unsigned h=0;h<sizeof heights/sizeof heights[0];h++) {
            w.h[0].y=heights[h]*Q16_ONE;
            for(int t=0;t<160;t++) {
                draw_frame(&w,ST_PLAY,t,0,0,0);
                rs_host_render();
                const uint16_t *pixels=rs_host_framebuffer();
                /* The hedgehog can cross the top mid-jump; sample both corners. */
                for(int x=0;x<RS_SCREEN_W;x++) if((x<64 || x>=256) && pixels[x]!=sky) {
                    fprintf(stderr,"sky wrapped: season=%d height=%d frame=%d x=%d\n",season,heights[h],t,x);
                    rs_host_shutdown();
                    return 1;
                }
            }
        }
    }
    rs_host_shutdown();
    puts("sky checks: all seasons, camera ascent and descent passed");
    return 0;
}
