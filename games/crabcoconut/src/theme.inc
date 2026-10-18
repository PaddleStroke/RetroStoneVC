#define TITLE "CRAB COCONUT"
#define BG_START(vp) 0
static const rs_color title_ramps[2][3]={{RS_HEX(0xffcd9e),RS_HEX(0xeb8269),RS_HEX(0xa34f54)},{RS_HEX(0xe2eed7),RS_HEX(0x91c6c4),RS_HEX(0x548ba1)}};
static const rs_color theme_pal[16]={0,RS_HEX(0xeff2ce),RS_HEX(0xf9e7c0),RS_HEX(0xe7cea1),RS_HEX(0xcdaa7c),RS_HEX(0xbadfd0),RS_HEX(0x8fc6ca),RS_HEX(0x577c93),RS_HEX(0xefe1bf),RS_HEX(0xf49482),RS_HEX(0x67bde1),RS_HEX(0x95d371),RS_HEX(0xd493e0),RS_HEX(0x8a5a44),RS_HEX(0xc45c5c),0};   /* 13, 14: widescreen decor */
static rs_color theme_sky(const world *w,int y,int height){(void)w;return RS_RGB8(183+y*44/height,219+y*18/height,218-y*24/height);}
static int theme_scroll(const hero *h){(void)h;return 0;}
/* widescreen: the beach beyond the walls (stage x < 0 and >= 320; the pixels' x is stage x + ox): a beach umbrella
 * over its shadow, a striped towel and a starfish on the left, a sandcastle with its flag, a bucket and a spade on
 * the right; the 427 screen (53 columns a side) shows the umbrella and the castle */
static void disc(uint8_t *p,int cx,int cy,int rx,int ry,int color,const rs_viewport *vp){
 for(int y=-ry;y<=ry;y++)for(int x=-rx;x<=rx;x++)if(x*x*ry*ry+y*y*rx*rx<=rx*rx*ry*ry)pixel(p,cx+x,cy+y,color,vp);
}
static void theme_decor(uint8_t *p,int ox,const rs_viewport *vp){
 int u=-32+ox,c=351+ox,b=386+ox;
 disc(p,u+3,224,20,4,4,vp);rect(p,u,140,u+1,223,13,vp);rect(p,u,134,u+1,137,13,vp);         /* the umbrella's shadow, pole */
 for(int y=137;y<=154;y++){int dy=154-y,half=0;while((half+1)*(half+1)*361<=484*(361-dy*dy))half++;
  for(int x=-half;x<=half;x++){int k=(x+44)/11;pixel(p,u+x,y,y>=152?14:(k&1)?1:9,vp);}}
 for(int x=-21;x<=21;x++)if((x+44)%11>=2 && (x+44)%11<=8)pixel(p,u+x,155,(((x+44)/11)&1)?8:14,vp);  /* scallops */
 rect(p,-96+ox,208,-62+ox,220,1,vp);                                                       /* the towel */
 for(int x=-96;x<=-64;x+=8)rect(p,x+2+ox,208,x+4+ox,220,10,vp);
 for(int y=209;y<=219;y+=2){pixel(p,-97+ox,y,1,vp);pixel(p,-61+ox,y,1,vp);}
 for(int k=-3;k<=3;k++){pixel(p,-14+k+ox,230,9,vp);pixel(p,-14+ox,230+k,9,vp);}                /* a starfish */
 for(int k=-2;k<=2;k++){pixel(p,-14+k+ox,230-(k<0?-k:k)/2+1,9,vp);}pixel(p,-14+ox,230,14,vp);
 disc(p,c+2,229,22,3,4,vp);                                                                  /* the castle's shadow */
 rect(p,c-15,204,c+15,228,3,vp);rect(p,c+9,204,c+15,228,4,vp);                               /* its wall, shaded */
 for(int x=c-15;x<=c+13;x+=6)rect(p,x,200,x+2,203,x>c+6?4:3,vp);
 rect(p,c-17,186,c-9,228,3,vp);rect(p,c+9,186,c+17,228,4,vp);rect(p,c+9,186,c+11,228,3,vp);  /* its towers */
 for(int x=c-17;x<=c-11;x+=3)rect(p,x,182,x+1,185,3,vp);
 for(int x=c+9;x<=c+15;x+=3)rect(p,x,182,x+1,185,x>c+11?4:3,vp);
 rect(p,c-14,192,c-13,195,13,vp);rect(p,c+13,192,c+14,195,13,vp);                            /* windows, the door */
 rect(p,c-4,216,c+3,228,13,vp);rect(p,c-3,215,c+2,215,13,vp);
 rect(p,c+13,168,c+13,181,13,vp);for(int y=168;y<=173;y++)rect(p,c+14,y,c+14+(y<171?y-167:174-y)*2,y,14,vp);
 for(int x=c-17;x<=c+17;x++)pixel(p,x,228,4,vp);
 for(int y=206;y<=228;y++){int half=7-(y-206)*2/22;rect(p,b-half,y,b+half,y,10,vp);pixel(p,b-half,y,5,vp);}  /* the bucket */
 rect(p,b-7,206,b+7,206,6,vp);for(int x=-6;x<=6;x++){int y=205-(36-x*x)/6;pixel(p,b+x,y,7,vp);}
 rect(p,b+14,225,b+26,226,13,vp);rect(p,b+27,223,b+31,228,6,vp);                             /* a spade */
}
static void theme_field(const world *w,int p,const view *v,const rs_viewport *vp,int title,uint8_t *pixels){
 (void)p;(void)v;int sand=184,ox=-vp->x;rect(pixels,0,sand,vp->w-1,vp->h-1,2,vp);
 /* the grains in stage coordinates (the same at every width): (x * 13) % 157 is periodic in 157 */
 for(int y=sand+4;y<vp->h;y++)for(int x=0;x<vp->w;x++)if(((x-ox+628)*13+y*7)%157<2)pixel(pixels,x,y,3,vp);
 if(vp->w>RS_SCREEN_W)theme_decor(pixels,ox,vp);
 if(title)return;
 rect(pixels,2+ox,3,3+ox,239,7,vp);rect(pixels,316+ox,3,317+ox,239,7,vp);rect(pixels,2+ox,2,317+ox,3,7,vp);
 // A coloured rim links each coconut to its owner's crab palette.
 if(w->players>1)for(int player=0;player<w->players;player++){
  const hero *h=&w->h[player];if(h->state!=HS_RUN)continue;int bx=h->bx/Q16_ONE+ox,by=h->by/Q16_ONE;
  for(int dy=-9;dy<=9;dy++)for(int dx=-9;dx<=9;dx++){int d=dx*dx+dy*dy;if(d>=64 && d<=81)pixel(pixels,bx+dx,by+dy,9+player,vp);}
 }
}
static void theme_shells(const world *w,const rs_viewport *vp){
 for(int i=0;i<SHELLS;i++)if(w->shells[i].hp){const shell *s=&w->shells[i];int sid=SPR_SHELL+(s->hp>1?2:s->kind);const gm_sprite_def *def=&gm_spr[sid];
  spr(sid,s->x-def->w/2,s->y/Q16_ONE-def->h/2,2,-1,vp);
 }
}
static void theme_scene(const world *w,int p,int t,const view *v,const rs_viewport *vp){
 (void)p;(void)v;
 for(int player=0;player<w->players;player++){
  const hero *h=&w->h[player];
  if(h->state==HS_RUN)spr(SPR_COCONUT,h->bx/Q16_ONE-8,h->by/Q16_ONE-8,2,-1,vp);
 }
 for(int player=w->players-1;player>=0;player--){
  const hero *h=&w->h[player];
  if(h->state!=HS_OFF){int id=h->state!=HS_RUN?SPR_HERO_HIT:h->launches && h->by>(paddle_y(w->players,player)-18)*Q16_ONE?SPR_HERO_AIR:SPR_HERO_IDLE+(t/8)%2;
   spr(id,h->x-16,crab_y(w->players,player)-16,2,ppal(player),vp);
  }
  if(h->smooth_t)hu_sparkle(h->bx/Q16_ONE+6+spr_dx,h->by/Q16_ONE-6,t,2);
 }
 theme_shells(w,vp);
}
static void theme_hud(const hero *h,char *text,size_t n){rs_strf(text,n,TR(SHELLS_N),h->apples);}
/* the title's crab under its coconut, two shells beside it; dy: moved down (a tall screen: between the slots and the
 * text under them), the coconut only from line top down */
static void theme_title(int t,const rs_viewport *vp,int dy,int top){if(75+dy-3>=top)spr(SPR_COCONUT,152,75+dy+hu_bob(t,64,3),2,-1,vp);spr(SPR_HERO_IDLE+(t/8)%2,144,96+dy,2,0,vp);spr(SPR_SHELL,100,97+dy,2,-1,vp);spr(SPR_SHELL+1,199,97+dy,2,-1,vp);}
