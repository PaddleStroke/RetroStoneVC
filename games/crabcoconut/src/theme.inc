#define TITLE "CRAB COCONUT"
#define HINT "LEFT / RIGHT: BOUNCE AND BREAK"
#define BG_START(vp) 0
static const rs_color title_ramps[2][3]={{RS_HEX(0xffcd9e),RS_HEX(0xeb8269),RS_HEX(0xa34f54)},{RS_HEX(0xe2eed7),RS_HEX(0x91c6c4),RS_HEX(0x548ba1)}};
static const rs_color theme_pal[16]={0,RS_HEX(0xeff2ce),RS_HEX(0xf9e7c0),RS_HEX(0xe7cea1),RS_HEX(0xcdaa7c),RS_HEX(0xbadfd0),RS_HEX(0x8fc6ca),RS_HEX(0x577c93),RS_HEX(0xefe1bf),RS_HEX(0xf49482),RS_HEX(0x67bde1),RS_HEX(0x95d371),RS_HEX(0xd493e0),0,0,0};
static rs_color theme_sky(const world *w,int y,int height){(void)w;return RS_RGB8(183+y*44/height,219+y*18/height,218-y*24/height);}
static int theme_scroll(const hero *h){(void)h;return 0;}
static void theme_field(const world *w,int p,const view *v,const rs_viewport *vp,int title,uint8_t *pixels){
 (void)p;(void)v;int sand=184;rect(pixels,0,sand,vp->w-1,vp->h-1,2,vp);
 for(int y=sand+4;y<vp->h;y++)for(int x=0;x<vp->w;x++)if((x*13+y*7)%157<2)pixel(pixels,x,y,3,vp);
 if(title)return;
 rect(pixels,2,3,3,239,7,vp);rect(pixels,316,3,317,239,7,vp);rect(pixels,2,2,317,3,7,vp);
 // A coloured rim links each coconut to its owner's crab palette.
 if(w->players>1)for(int player=0;player<w->players;player++){
  const hero *h=&w->h[player];if(h->state!=HS_RUN)continue;int bx=h->bx/Q16_ONE,by=h->by/Q16_ONE;
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
  if(h->smooth_t)hu_sparkle(h->bx/Q16_ONE+6,h->by/Q16_ONE-6,t,2);
 }
 theme_shells(w,vp);
}
static void theme_hud(const hero *h,char *text,size_t n){snprintf(text,n,"SHELLS %d",h->apples);}
static void theme_title(int t,const rs_viewport *vp){spr(SPR_COCONUT,152,75+hu_bob(t,64,3),2,-1,vp);spr(SPR_HERO_IDLE+(t/8)%2,144,96,2,0,vp);spr(SPR_SHELL,100,97,2,-1,vp);spr(SPR_SHELL+1,199,97,2,-1,vp);}
