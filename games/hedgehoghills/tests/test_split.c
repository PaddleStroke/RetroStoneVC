/* Rendered split-screen isolation, clipping and large-separation regressions. */
#include "game.h"
#include "assets.h"
#include "rs_host.h"
#include "rs_desktop.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint16_t picture[RS_SCREEN_W*RS_SCREEN_H];
static void render(const world *w,int paused) {
    draw_frame(w,ST_PLAY,50,0,0,paused);
    rs_host_render();
}
static void visible_players(const world *w,const rs_viewport *rect) {
    for(int p=0;p<w->players;p++) {
        int found=-1;
        for(int i=0;i<rs_oam_next();i++) {
            const rs_sprite *s=rs_oam(i);
            if(s->tile<gm_spr[SPR_HERO_ICON].tile && s->pal==draw_player_pal(p)) {
                assert(found==-1); found=i;
            }
        }
        assert(found>=0);
        rs_sprite *s=rs_oam(found);
        assert(s->x>=0 && s->x+s->w<=rect[p].w);
        assert(s->y>=24 && s->y+s->h<=rect[p].h);
        memcpy(picture,rs_host_framebuffer(),sizeof picture);
        s->flags|=RS_SPR_HIDE;
        rs_host_render();
        int changed=0;
        for(int y=0;y<RS_SCREEN_H;y++) for(int x=0;x<RS_SCREEN_W;x++) {
            int i=y*RS_SCREEN_W+x;
            if(picture[i]==rs_host_framebuffer()[i]) continue;
            changed++;
            assert(x>=rect[p].x && x<rect[p].x+rect[p].w);
            assert(y>=rect[p].y && y<rect[p].y+rect[p].h);
        }
        assert(changed>50); /* The character really appears in the final pixels. */
        s->flags&=~RS_SPR_HIDE;
        rs_host_render();
    }
}
int main(void) {
    rs_host_set_option("music","0"); rs_host_set_option("sound","0");
    rs_host_set_option("strict","1");
    assert(!rs_host_init(rs_game_main()));
    for(int players=2;players<=4;players++) {
        draw_split_off();
        world w; world_init(&w,players,4);
        for(int p=0;p<players;p++) {
            hero *h=&w.h[p]; h->state=HS_RUN;
            h->wx=(1000LL+p*18000LL)*Q16_ONE;
            h->y=hill_y(&w,(int)(h->wx>>16));
        }
        w.scroll=(int)(w.h[players-1].wx>>16)-HERO_X;
        rs_viewport rect[4]; rs_viewport_layout(players,RS_LAYOUT_HSPLIT,rect);
        render(&w,0); assert(rs_viewport_count()==players);
        visible_players(&w,rect);
        memcpy(picture,rs_host_framebuffer(),sizeof picture);
        /* P1 can move a hill and launch while the other panels remain identical. */
        w.h[0].wx+=11000LL*Q16_ONE; w.h[0].y=-1000*Q16_ONE;
        render(&w,0);
        for(int p=1;p<players;p++) for(int y=rect[p].y;y<rect[p].y+rect[p].h;y++)
            for(int x=rect[p].x;x<rect[p].x+rect[p].w;x++)
                assert(picture[y*RS_SCREEN_W+x]==rs_host_framebuffer()[y*RS_SCREEN_W+x]);
        visible_players(&w,rect);
        /* Stress overlapping frost transitions in every panel's tile allocation. */
        for(int p=0;p<players;p++) w.h[p].wx=(6000LL+p*14LL)*Q16_ONE;
        for(int t=0;t<90;t++) {
            for(int p=0;p<players;p++) {
                w.h[p].y=(t%2?-2048-p*100:220)*Q16_ONE;
                w.frost=w.h[p].wx-25LL*Q16_ONE;
            }
            render(&w,0); visible_players(&w,rect);
        }
        char path[120];
        snprintf(path,sizeof path,"build/hedgehoghills-review/separated-%d-players.png",players);
        assert(!rsd_write_png(path,rs_host_framebuffer(),3));
        render(&w,1); memcpy(picture,rs_host_framebuffer(),sizeof picture);
        for(int t=0;t<20;t++) render(&w,1);
        assert(!memcmp(picture,rs_host_framebuffer(),sizeof picture));
        draw_frame(&w,ST_TITLE,60,0,0,0); rs_host_render();
        assert(rs_viewport_count()==0); /* Shared lobby returns cleanly. */
        printf("split checks: %d players visible, isolated, clipped, paused; lobby restored\n",players);
    }
    for(int k=0;k<RS_WARN_KIND_COUNT;k++) assert(!rs_warn_count(k));
    rs_host_shutdown();
    return 0;
}
