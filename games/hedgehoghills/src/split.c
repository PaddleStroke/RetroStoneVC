/* Independent multiplayer cameras, using SDK viewports and shared tile atlases.
 * Widescreen (DESIGN.md "Widescreen"): the views split the whole width (rs_viewport_layout) and each shows more of its
 * hills, a third of its extra width behind its hedgehog (draw_wide_left), two thirds ahead; a view is compact (its hedgehog 48 px from its left edge) when it is narrower than 3/4 of the screen,
 * as at 320; the right-hand views use map columns 64 on (128-tile maps); each view's text sits in the UI columns of
 * its place on the screen.
 * MIT, (c) 2026 Pierre-Louis Boyer (8BCraft). */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include <stdio.h>
#include <string.h>

typedef struct split_view {
    int cam_y,scroll,map_x,map_y;
    rs_color sky[RS_SCREEN_H],soil[16],hills[16];
} split_view;
static split_view V[MAX_PLAYERS];
static rs_viewport views[MAX_PLAYERS];
static rs_viewport views_all[MAX_PLAYERS+2];   /* per frame: the views and a tall screen's two bands (not saved) */
static int nviews;

void draw_split_init(void) {
    memset(V,0,sizeof V); memset(views,0,sizeof views); nviews=0;
    rs_viewports(0,NULL,0);
}
void draw_split_off(void) { nviews=0; rs_viewports(0,NULL,0); }
int draw_split_raster(int line) {
    int p=rs_viewport_current();
    if(p<0 || p>=nviews) return 0;
    const split_view *v=&V[p];
    rs_pal_set(0,v->sky[line-views[p].y]);
    rs_pal_load(RS_PAL_BG(PAL_GROUND),v->soil,16);
    rs_pal_load(RS_PAL_BG(PAL_HILLS),v->hills,16);
    return 1;
}
static int floor8(int y) { return y>=0?y/8:(y-7)/8; }
/* a view narrower than 3/4 of the screen is compact (at 320: narrower than 240 columns) */
static int compact(const rs_viewport *vp) { return vp->w*RS_SCREEN_W<240*rs_screen_width(); }
/* the UI layer's columns of a split view: a left-hand view from column 0, a right-hand view right after the left one's
 * (at 320: 159 px, 20 columns, so at 20), where the kit allows 19 or 20 columns of text at every width (its text may not
 * go left of column 0 or past the screen's right edge, -margin .. 320 + margin in its centred UI coordinates) */
static int view_ux(const rs_viewport *vp) { return vp->x?(vp->x-RS_VIEW_DIVIDER+7)/8:0; }
static void spr(const rs_viewport *vp,int id,int x,int y,int prio,int pal) {
    const gm_sprite_def *s=&gm_spr[id];
    if(x<=-(int)s->w || x>=vp->w || y<=-(int)s->h || y>=vp->h) return;
    rs_spr(x,y,s->tile,s->w,s->h,pal<0?s->pal:pal,prio,0);
}
static int tile_pixels(const uint8_t *pixels,int *next) {
    if(*next>=1024) {
        rs_warn(RS_WARN_VRAM,"split-screen terrain exceeded its 1024-tile bank");
        return 1;
    }
    int tile=(*next)++;
    rs_tiles_load8(VR_BG2+tile,pixels,1);
    return tile;
}
static void terrain(const world *w,const split_view *v,const rs_viewport *vp,int *next) {
    int cols=(vp->w+7)/8,rows=(vp->h+7)/8;
    int front=(int)(w->frost/Q16_ONE);
    for(int c=0;c<cols;c++) {
        int floors[8],lo=100000,hi=-100000,wx=v->scroll+c*8;
        for(int x=0;x<8;x++) {
            floors[x]=(hill_y(w,wx+x)>>16)-v->cam_y;
            if(floors[x]<lo) lo=floors[x];
            if(floors[x]>hi) hi=floors[x];
        }
        int first=floor8(lo),last=floor8(hi+7);
        for(int r=0;r<rows;r++) {
            int texture=(r+c/3)%5==0?2:(r+c)%3==0?3:1;
            int tile=r<first?0:texture;
            int edge=r>=first && r<=last;
            int mixed=r>last && front-wx+56>0 && front-wx+33<96;
            if(edge || mixed) {
                uint8_t pixels[64];
                for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
                    int yy=r*8+y,d=yy-floors[x],color=0;
                    if(edge) {
                        if(d>=0) color=d==0?1:d==1?2:d<4?3:d<6?4:d<8?5:6;
                        if(d>=8 && ((wx+x)*13+yy*7)%41==0) color=9;
                    } else color=texture==2 && y<2?7:texture==3 && (x*7+y*3)%23==0?9:6;
                    if(color && draw_frozen_pixel(w,wx+x,yy+v->cam_y)) color=draw_iced(color);
                    pixels[y*8+x]=(uint8_t)color;
                }
                tile=tile_pixels(pixels,next);
            } else if(r>last && front-wx+33>=96) tile+=3;
            rs_bg_put(RS_BG2,v->map_x+c,v->map_y+r,RS_MAP(tile,PAL_GROUND,0,0,0));
        }
    }
}
static void hills(const split_view *v,rs_viewport *vp) {
    /* Every atlas slice has its own world rows; no vertical wrapping into ground. */
    int yoff=v->cam_y/4+(RS_SCREEN_H-vp->h)/2;
    int first=floor8(yoff),frac=yoff-first*8;
    int xoff=v->scroll>>2,first_x=xoff>>3;      /* floor: a scroll < 0 behind the start (widescreen) */
    int cols=(vp->w+(xoff&7)+7)/8,rows=(vp->h+frac+7)/8;
    for(int y=0;y<rows;y++) for(int x=0;x<cols;x++) {
        int yy=first+y-HILLS_Y/8;
        uint16_t tile=yy<0?RS_MAP(gm_bg3_tile_count,PAL_HILLS,0,0,0):
            draw_hills_entry(yy<HILLS_H?yy:HILLS_H-1,first_x+x);
        rs_bg_put(RS_BG3,v->map_x+x,v->map_y+y,tile);
    }
    vp->sx[RS_BG3]=(int16_t)(v->map_x*8+(xoff&7));
    vp->sy[RS_BG3]=(int16_t)(v->map_y*8+frac);
}
static void objects(const world *w,int p,int t,const split_view *v,const rs_viewport *vp) {
    const hero *h=&w->h[p];
    int x=(int)(h->wx>>16)-v->scroll,y=(h->y>>16)-30-v->cam_y;
    int n=rs_oam_next();
    spr(vp,draw_hero_sprite(h,t),x-16,y,2,draw_player_pal(p));
    if(p==0) draw_p1_x=rs_oam_next()>n?vp->x+rs_screen_origin_x()+rs_oam(n)->x+16:-999;
    if(h->state==HS_RUN && h->held && h->grounded && h->vx>6*Q16_ONE)
        spr(vp,SPR_PUFF+(w->t/4)%3,x-25,y+24,1,-1);
    if(h->smooth_t>0) hu_sparkle(x-20-hu_ui_x(),y+6,w->t,2);   /* kit sprites: UI coordinates */
    if(h->spill && h->smooth_t<0 && h->land_t<24) {
        int age=h->land_t;
        for(int k=0;k<h->spill;k++) spr(vp,SPR_APPLE,x-8-age*(k+1),y+8-age*3+age*age/8-k*5,1,-1);
    }
    int first=v->scroll/HILL_SPAN*3,hills_seen=vp->w>RS_SCREEN_W?(vp->w+HILL_SPAN-1)/HILL_SPAN+1:2;
    if(h->state==HS_RUN) for(int a=first;a<first+3*hills_seen;a++) if(h->apple_id<a) {
        int ax,ay; apple_pos(w,a,&ax,&ay);
        spr(vp,SPR_APPLE,ax-v->scroll-8,ay-v->cam_y-8+hu_bob(w->t+a*11,80,2),1,-1);
    }
    int front=(int)(w->frost/Q16_ONE)-v->scroll;
    if(front>-100) {
        for(int i=0;i<9;i++) spr(vp,SPR_SNOW,front+38-((w->t*2+i*31)%155),(w->t+i*37)%vp->h,2,-1);
        for(int i=0;i<3;i++) spr(vp,SPR_FROST_WISP+(w->t/12+i)%2,
            front+22-((w->t*3+i*39)%148),(w->t/3+i*47)%vp->h,1,-1);
    }
}
/* plain ASCII: no accent, no tall glyph (nothing in the row above the text) */
static int plain(const char *s) {
    for(;*s;s++) if((unsigned char)*s>=128) return 0;
    return 1;
}
/* a word in the panel font on its own fill, with no frame (PAUSED): the kit's box text goes on a panel's fill, so
 * a panel is laid under it and everything but the word's cells is blanked again */
static void label(int x,int y,const char *s) {
    int n=hu_width(s,0);
    char blank[48];
    if(n>(int)sizeof blank-3) n=(int)sizeof blank-3;
    memset(blank,' ',(size_t)n+2); blank[n+2]=0;
    hu_panel(x-1,y-1,n+2,3);
    hu_text(x-1,y-1,blank); hu_text(x-1,y+1,blank);
    hu_text(x-1,y," "); hu_text(x+n,y," ");
    hu_box_text(x,y,s);
}
void draw_split_frame(const world *w,int st_t,int paused) {
    int fresh=nviews!=w->players;
    nviews=rs_viewport_layout(w->players,RS_LAYOUT_HSPLIT,views);
    /* a tall screen (DESIGN.md "Tall screen"): the views split the stage's 240 lines (the layout splits the screen's
     * whole height), the lines above and below them the raster's sky and earth (draw.c raster_out) */
    if(rs_screen_height()>RS_SCREEN_H)
        for(int p=0;p<nviews;p++) {
            int y0=views[p].y-rs_screen_top(),y1=y0+views[p].h,hh=rs_screen_height();
            views[p].y=(int16_t)(y0*RS_SCREEN_H/hh);
            views[p].h=(int16_t)(y1*RS_SCREEN_H/hh-views[p].y);
        }
    hu_clear(); rs_oam_clear();
    int next_tile=16;
    for(int p=0;p<nviews;p++) {
        split_view *v=&V[p]; rs_viewport *vp=&views[p];
        const hero *h=&w->h[p];
        v->map_x=vp->x?(draw_wide()?64:32):0; v->map_y=vp->y?16:0;
        /* the UI layer's columns of the view (view_ux; the scenery layers need up to 71 columns per view: the
         * right-hand views' at 64 on a wide screen, 32 at 320) */
        int ux=view_ux(vp);
        int foot=h->y>>16,target=foot-vp->h*3/4;
        if(fresh) v->cam_y=target;
        else if(!paused) v->cam_y+=(target-v->cam_y)/8;
        /* Keep the complete sprite below its HUD even during a fast launch. */
        if(!paused || fresh) {
            if(foot-v->cam_y<60) v->cam_y=foot-60;
            if(foot-v->cam_y>vp->h-10) v->cam_y=foot-vp->h+10;
        }
        /* widescreen: a third of the view's extra width behind the hedgehog (0 at 320); behind the start, the hills
         * behind the starting line (down to -that third, as at 320 down to 0) */
        int behind=draw_wide_left(vp->w);
        v->scroll=(int)(h->wx>>16)-(compact(vp)?48:HERO_X)-behind;
        if(v->scroll<-behind) v->scroll=-behind;
        draw_scenery_colors(v->scroll,vp->h,v->sky,v->soil,v->hills);
        terrain(w,v,vp,&next_tile); hills(v,vp);
        vp->layers=7; vp->objs=1;
        vp->sx[RS_BG1]=(int16_t)(ux*8); vp->sx[RS_BG2]=(int16_t)(v->map_x*8);
        vp->sy[RS_BG1]=vp->sy[RS_BG2]=(int16_t)(v->map_y*8);
        char text[128];
        rs_strf(text,sizeof text,TR(SPLIT_SCORE),p+1,h->score,h->apples);
        hu_text(ux+1,v->map_y+1,text);
        int margin=(int)((h->wx-w->frost)/Q16_ONE)/16;
        if(margin<0) margin=0;
        if(h->state==HS_HIT || h->state==HS_DOWN) rs_strf(text,sizeof text,"%s",TR(SPLIT_FROZEN));
        else rs_strf(text,sizeof text,TR(SPLIT_FROST),margin);
        /* the second line right under the first; one row lower when its words have accents (the row above) */
        hu_text(ux+1,v->map_y+(plain(TR(SPLIT_FROST)) && plain(TR(SPLIT_FROZEN))?2:3),text);
        if(paused) {
            /* centred on the view (a right-hand view on a wide screen: within the kit's columns, the panel's edge too) */
            int n=hu_width(HTR(PAUSED),0),x=ux+(vp->w/8-n)/2,lim=(hu_ui_right()+7)/8;
            if(x+n+1>lim) x=lim-n-1;
            label(x,v->map_y+6,HTR(PAUSED));
        }
        else if(h->smooth_t>0) {
            rs_strf(text,sizeof text,TR(SPLIT_SMOOTH),h->combo);
            hu_text(ux+1,v->map_y+vp->h/8-1,text);
        }
        hu_view_oam_begin(vp); objects(w,p,st_t,v,vp); hu_view_oam_end(vp);
    }
    /* a tall screen: two more views with no layer, the sky above the stage and the earth below it (raster_out's colours) */
    int n=nviews;
    memcpy(views_all,views,sizeof views);
    if(rs_screen_height()>RS_SCREEN_H && n<=MAX_PLAYERS) {
        rs_viewport *a=&views_all[n],*b=&views_all[n+1];
        memset(a,0,2*sizeof *a);
        a->x=b->x=(int16_t)rs_screen_left(); a->w=b->w=(int16_t)rs_screen_width();
        a->y=(int16_t)rs_screen_top(); a->h=(int16_t)-rs_screen_top();
        b->y=RS_SCREEN_H; b->h=(int16_t)(rs_screen_bottom()-RS_SCREEN_H);
        n+=2;
    }
    rs_viewports(n,views_all,RS_HEX(0x294c30));
}
/* tests (draw_view_log): P1's view, if the split screen shows (its stage x, else -9999), its width, its scroll */
int draw_split_view_x(int p) { return p<nviews?views[p].x:-9999; }
int draw_split_view_w(int p) { return p<nviews?views[p].w:0; }
int draw_split_scroll(int p) { return p<nviews?V[p].scroll:0; }
#define S(v) rs_state_var("split." #v,&(v),sizeof(v))
void draw_split_state(void) { S(V); S(views); S(nviews); }
#undef S
