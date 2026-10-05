/* Hedgehog Hills video, using the shared 8BCraft kit. MIT, (c) 2026 8BCraft. */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include <stdio.h>
#include <string.h>
static const rs_color title_ramps[2][3]={
    {RS_RGB8(250,210,150),RS_RGB8(198,136,76),RS_RGB8(122,72,44)},
    {RS_RGB8(214,240,148),RS_RGB8(142,190,84),RS_RGB8(70,116,52)}};
static rs_color line_col[RS_SCREEN_H];
static int shown_state=-1,shown_best=-1,cam_y;
static void raster(int line,void *user) {
    (void)user;
    if(!draw_split_raster(line)) rs_pal_set(0,line_col[line]);
}
void draw_scenery_colors(int scroll,int height,rs_color *sky,rs_color *soil,rs_color *hills) {
    int season=(scroll/14000)%3;
    const uint32_t tops[3]={0x579ed6,0x7975b4,0x414c7c};
    const uint32_t horizons[3]={0xf4e5b0,0xffcb98,0xccd8ed};
    rs_color top=RS_HEX(tops[season]),bottom=RS_HEX(horizons[season]);
    for(int y=0;y<height;y++) sky[y]=hu_lerp_color(top,bottom,y*256/height);
    static const uint32_t ground[3][16]={
        {0,0x294c30,0xe0ee97,0x9acc66,0x65a54a,0x497837,0xba8356,0xa66e48,0x8b593f,0xd2a16c,0xe8bb88,0x744831,0x88b154,0xf5e5b0,0xcde794,0x354e35},
        {0,0x603b32,0xffdb8b,0xe6a34e,0xc27b38,0x875038,0xb88360,0x9f6e52,0x84583f,0xd8a578,0xf0c898,0x69432f,0xd89845,0xffe7b1,0xe8b368,0x624630},
        {0,0x344768,0xeaf3ff,0xc1dbf0,0x8db4d4,0x6386ac,0xa2b4ca,0x899db8,0x6f86a5,0xb9c9dc,0xdbe6f1,0x536b8d,0xabcce8,0xf8faff,0xc7dfee,0x465d7a}};
    soil[0]=hills[0]=0;
    for(int i=1;i<16;i++) soil[i]=RS_HEX(ground[season][i]);
    /* The upper palette slots are the same materials under snow. Ordered pixel
     * fades mix these with grass and soil across the advancing frost band. */
    static const uint32_t ice[6]={0x536f93,0xf2f8ff,0xd4e8f6,0xa8cde3,0x7fa8c2,0x9dbbd0};
    for(int i=0;i<6;i++) soil[10+i]=RS_HEX(ice[i]);
    for(int i=1;i<16;i++) {
        rs_color c=gm_hills_pal[i];
        if(season) c=hu_lerp_color(c,RS_HEX(season==1?0xebba8c:0xb4c8e4),season==1?70:120);
        hills[i]=c;
    }
}
static void scenery(const world *w) {
    rs_color soil[16],hills[16];
    draw_scenery_colors(w->scroll,RS_SCREEN_H,line_col,soil,hills);
    rs_pal_load(RS_PAL_BG(PAL_GROUND),soil,16);
    rs_pal_load(RS_PAL_BG(PAL_HILLS),hills,16);
}
/* Stream rows from world height so negative camera offsets reveal sky instead
 * of wrapping the panorama's forest floor into the top of the screen. */
static void distant_hills(const world *w) {
    int offset=cam_y/4;
    int first=offset>=0?offset/8:(offset-7)/8;
    for(int y=0;y<32;y++) for(int x=0;x<64;x++) {
        int yy=first+y-HILLS_Y/8;
        uint16_t tile=yy<0?RS_MAP(gm_bg3_tile_count,PAL_HILLS,0,0,0):
            gm_hills_map[(yy<HILLS_H?yy:HILLS_H-1)*64+x];
        rs_bg_put(RS_BG3,x,y,tile);
    }
    rs_bg_scroll(RS_BG3,(w->scroll/4)&511,offset-first*8);
}
void draw_init(void) {
    hu_config hc=hu_defaults(); hc.obj_tile=KIT_OBJ_TILE; hc.obj_vram=VR_OBJ; hc.obj_pal=3;
    hu_init(&hc);
    rs_bg_setup(RS_BG1,64,32,VR_BG1); rs_bg_setup(RS_BG2,64,32,VR_BG2);
    rs_bg_setup(RS_BG3,64,32,VR_BG3);
    rs_tiles_load(VR_BG3,gm_bg3_tiles,gm_bg3_tile_count);
    uint8_t sky_tile[64]={0};
    rs_tiles_load8(VR_BG3+gm_bg3_tile_count,sky_tile,1);
    rs_pal_load(RS_PAL_BG(PAL_HILLS),gm_hills_pal,16);
    uint8_t pixels[7*64]={0};
    for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
        pixels[64+y*8+x]=6;
        pixels[128+y*8+x]=(y==0||y==1)?7:6;
        pixels[192+y*8+x]=((x*7+y*3)%23==0)?9:6;
        pixels[256+y*8+x]=15;
        pixels[320+y*8+x]=(y==0||y==1)?14:15;
        pixels[384+y*8+x]=((x*7+y*3)%23==0)?11:15;
    }
    rs_tiles_load8(VR_BG2,pixels,7);
    rs_obj_base(VR_OBJ); rs_tiles_load(VR_OBJ,gm_obj_tiles,gm_obj_tile_count);
    rs_pal_load(RS_PAL_OBJ(0),gm_obj_pals,48); rs_pal_load(RS_PAL_OBJ(4),gm_obj_pals+64,32);
    for(int l=0;l<3;l++) rs_bg_enable(l,1);
    draw_split_init();
    rs_raster(raster,NULL);
}
int draw_frozen_pixel(const world *w,int wx,int wy) {
    static const uint8_t bayer[4][4]={{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
    int phase=wy&63, wave=(phase<32?phase:63-phase)/2-8;
    int amount=((int)(w->frost/Q16_ONE)-wx+48+wave)*256/96;
    return amount>bayer[wy&3][wx&3]*16+8;
}
int draw_iced(int color) {
    static const uint8_t map[16]={0,10,11,12,13,14,15,14,14,11,11,11,12,13,14,15};
    return map[color&15];
}
/* Only the curved grass fringe needs unique tiles: solid earth reuses three texture tiles.
 * Each screen column gets eight edge tiles. This stays within the console's VRAM budget. */
static void terrain(const world *w) {
    int frost_tile=0,front=(int)(w->frost/Q16_ONE);
    for(int c=0;c<40;c++) {
        int floors[8],lo=1000,hi=-1000;
        for(int x=0;x<8;x++) {
            floors[x]=(hill_y(w,w->scroll+c*8+x)>>16)-cam_y;
            if(floors[x]<lo) lo=floors[x];
            if(floors[x]>hi) hi=floors[x];
        }
        int first=lo/8; if(lo<0) first=(lo-7)/8;
        int last=(hi+6)/8;
        uint8_t edge[8*64]={0};
        for(int r=first;r<=last && r-first<8;r++) for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
            int yy=r*8+y,d=yy-floors[x],val=0;
            if(d>=0) val=d==0?1:d==1?2:d<4?3:d<6?4:d<8?5:6;
            if(d>=8 && ((w->scroll+c*8+x)*13+yy*7)%41==0) val=9;
            if(val && draw_frozen_pixel(w,w->scroll+c*8+x,yy+cam_y)) val=draw_iced(val);
            edge[(r-first)*64+y*8+x]=(uint8_t)val;
        }
        rs_tiles_load8(VR_BG2+16+c*8,edge,8);
        for(int r=0;r<30;r++) {
            int tile=r<first?0:r<=last?16+c*8+r-first:((r+c/3)%5==0?2:(r+c)%3==0?3:1);
            if(r>last) {
                int wx=w->scroll+c*8;
                if(front-wx-7+48-8>=96) tile+=3;
                else if(front-wx+48+8>0 && frost_tile<256) {
                    uint8_t pix[64];
                    for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
                        int color=tile==2 && y<2?7:tile==3 && (x*7+y*3)%23==0?9:6;
                        pix[y*8+x]=(uint8_t)(draw_frozen_pixel(w,wx+x,r*8+y+cam_y)?draw_iced(color):color);
                    }
                    tile=352+frost_tile++;
                    rs_tiles_load8(VR_BG2+tile,pix,1);
                }
            }
            rs_bg_put(RS_BG2,c,r,RS_MAP(tile,PAL_GROUND,0,0,0));
        }
    }
}
static void spr(int id,int x,int y,int prio,int pal) {
    const gm_sprite_def *d=&gm_spr[id];
    if(x<=-(int)d->w||x>=320||y<=-(int)d->h||y>=240) return;
    rs_spr(x,y,d->tile,d->w,d->h,pal>=0?pal:d->pal,prio,0);
}
int draw_player_pal(int p) { return p?hu_player_pal(p):0; }
int draw_hero_sprite(const hero *h,int t) {
    if(h->state==HS_HIT||h->state==HS_DOWN) return SPR_HERO_HIT;
    if(h->held) return SPR_HERO_ROLL+(t/4)%4;
    if(h->land_t<5) return SPR_HERO_SQUASH;
    if(!h->grounded) return h->vy<0?SPR_HERO_STRETCH:SPR_HERO_AIR;
    return SPR_HERO_IDLE+(t/6)%2;
}
static void slot_icon(int p,int cx,int cy,int t,void *user) {
    (void)user; spr(SPR_HERO_ICON,cx-8,cy-8+hu_bob(t+p*16,64,1),2,draw_player_pal(p));
}
static void standing(const world *w,hu_standing *s) {
    int score[4]; for(int p=0;p<w->players;p++) score[p]=w->h[p].score;
    hu_rank(s,w->players,score,score);
}
static void screen_text(const world *w,int state,int st_t,int best,int new_best,const hu_standing *rank) {
    int screen=state==ST_DEAD?ST_PLAY:state;
    if(screen!=shown_state||(state==ST_OVER && st_t==RETRY_LOCK)||best!=shown_best) {
        hu_clear();
        if(state==ST_TITLE) hu_logo("HEDGEHOG HILLS",title_ramps,2,2,0);
        if(screen==ST_PLAY && w->players>=3) hu_score_tags(w->players,0);
        if(state==ST_OVER) {
            if(w->players==1) { hu_banner(4,HTR(GAME_OVER)); hu_gameover_panel(1,new_best,w->h[0].score,0); }
            else hu_results_panel(rank,NULL);
        }
        shown_state=screen; shown_best=best;
    }
    if(state==ST_TITLE) {
        hu_title_draw(st_t,best);
        /* the hint, centred (a column to the left, as the English always was) */
        int hw=hu_width(TR(HINT_TITLE),0);
        hu_text(hw<38?(38-hw)/2:0,16,TR(HINT_TITLE));
    }
    if(state==ST_OVER) {
        if(w->players==1) hu_retry_line(st_t,RETRY_LOCK,HTR(BACK_TITLE));
        else hu_retry_line_at(hu_results_retry_row(rank),st_t,RETRY_LOCK,HTR(BACK_TITLE));
    }
    if(screen==ST_PLAY) {
        int focus=0;
        for(int p=0;p<w->players;p++)
            if(w->h[p].state==HS_RUN && (w->h[focus].state!=HS_RUN || w->h[p].wx>w->h[focus].wx)) focus=p;
        char txt[160]; int margin=(int)((w->h[focus].wx-w->frost)/Q16_ONE)/16;
        if(margin<0) margin=0;
        hu_clear_rows(4,3);
        if(w->players>1) rs_strf(txt,sizeof txt,TR(HUD_FROST_PLAYER),focus+1,margin);
        else rs_strf(txt,sizeof txt,TR(HUD_FROST),margin);
        hu_text(hu_center(txt,0),4,txt);
        const hero *h=&w->h[focus];
        if(h->smooth_t>0) rs_strf(txt,sizeof txt,TR(MSG_SMOOTH),h->combo);
        else if(h->smooth_t<0 && h->land_t<55) rs_strf(txt,sizeof txt,"%s",TR(MSG_OUCH));
        else if(w->t<360) rs_strf(txt,sizeof txt,"%s",h->held?TR(HINT_RELEASE):TR(HINT_HOLD));
        else rs_strf(txt,sizeof txt,TR(HUD_APPLES),h->apples);
        hu_text(hu_center(txt,0),6,txt);
    }
}
void draw_frame(const world *w,int state,int st_t,int best,int new_best,int paused) {
    static const int medals[4]=MEDAL_SCORES;
    if(w->players>1 && (state==ST_PLAY || state==ST_DEAD)) {
        draw_split_frame(w,st_t,paused);
        shown_state=shown_best=-1;
        return;
    }
    draw_split_off();
    int target=0;
    if(state!=ST_TITLE) for(int p=0;p<w->players;p++) if(w->h[p].state==HS_RUN) {
        int yy=(w->h[p].y>>16)-76;
        if(yy<target) target=yy;
    }
    if(!paused) cam_y+=(target-cam_y)/8;
    scenery(w); terrain(w); distant_hills(w);
    int sx=hu_shake_x(),sy=hu_shake_y();
    rs_bg_scroll(RS_BG2,-sx,-sy);
    hu_standing rank; standing(w,&rank);
    screen_text(w,state,st_t,best,new_best,&rank);
    int slide=state==ST_OVER?hu_slide_in(st_t,20,200):0;
    rs_bg_scroll(RS_BG1,0,-slide); hu_pause(paused,13);
    rs_oam_clear();
    if(state==ST_PLAY||state==ST_DEAD) {
        int scores[4]; for(int p=0;p<w->players;p++) scores[p]=w->h[p].score;
        hu_score_chips(w->players,scores,0);
    }
    if(state==ST_OVER) {
        if(w->players==1) hu_gameover_sprites(1,w->h[0].score,0,best,hu_medal_of(w->h[0].score,medals),st_t,slide);
        else {
            hu_results_sprites(&rank,st_t,slide);
            for(int i=0;i<rank.n;i++) {
                int cx,cy; hu_results_icon_pos(&rank,i,&cx,&cy);
                spr(SPR_HERO_ICON,cx-8,cy-8+slide,3,draw_player_pal(rank.order[i]));
            }
        }
    }
    if(state==ST_TITLE) {
        hu_title_sprites(st_t,slot_icon,NULL);
        spr(SPR_HERO_IDLE+(st_t/32)%2,144,91+hu_bob(st_t,64,4),2,-1);
    } else {
        for(int p=w->players-1;p>=0;p--) {
            const hero *h=&w->h[p];
            if(h->state==HS_OFF||h->x<-24) continue;
            int yy=(h->y>>16)-32+2-cam_y;
            spr(draw_hero_sprite(h,st_t),h->x-16+sx,yy+sy,2,draw_player_pal(p));
            if(h->state==HS_RUN && h->held && h->grounded && h->vx>6*Q16_ONE)
                spr(SPR_PUFF+(w->t/4)%3,h->x-25,yy+24,1,-1);
            if(h->smooth_t>0) hu_sparkle(h->x-20,yy+6,w->t,2);
            if(h->spill && h->smooth_t<0 && h->land_t<24) {
                int t=h->land_t;
                for(int k=0;k<h->spill;k++)
                    spr(SPR_APPLE,h->x-8-t*(k+1),yy+8-t*3+t*t/8-k*5,1,-1);
            }
        }
        int first=(w->scroll/HILL_SPAN)*3;
        for(int a=first;a<first+6;a++) {
            int ax,ay; apple_pos(w,a,&ax,&ay);
            int visible=0;
            for(int p=0;p<w->players;p++) if(w->h[p].state==HS_RUN && w->h[p].apple_id<a) visible=1;
            if(visible) spr(SPR_APPLE,ax-w->scroll-8,ay-cam_y-8+hu_bob(w->t+a*11,80,2),1,-1);
        }
        /* Wind-blown flakes and long wisps echo the feathered terrain edge. */
        int front=(int)(w->frost/Q16_ONE)-w->scroll;
        if(front>-100) for(int i=0;i<18;i++) {
            int x=front+38-((w->t*2+i*31)%155),y=(w->t+i*37)%220;
            spr(SPR_SNOW,x,y,2,-1);
        }
        if(front>-100) for(int i=0;i<5;i++) {
            int x=front+22-((w->t*3+i*39)%148),y=(w->t/3+i*47)%230;
            spr(SPR_FROST_WISP+(w->t/12+i)%2,x,y,1,-1);
        }
    }
}
#define S(v) rs_state_var("draw." #v,&(v),sizeof(v))
void draw_state(void) { S(line_col); S(shown_state); S(shown_best); S(cam_y); draw_split_state(); RS_STATE_RASTER(raster); }
#undef S
