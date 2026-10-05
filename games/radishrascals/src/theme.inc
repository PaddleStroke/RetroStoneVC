#define TITLE "RADISH RASCALS"
#define BG_START(vp) ((vp)->h<200?-32:0)
static const rs_color title_ramps[2][3]={{RS_HEX(0xffbfd0),RS_HEX(0xe774a0),RS_HEX(0x944871)},{RS_HEX(0xd8ed9e),RS_HEX(0x93bb6a),RS_HEX(0x527b51)}};
static const rs_color theme_pal[16]={0,RS_HEX(0xd2dda0),RS_HEX(0xacc780),RS_HEX(0x73975d),RS_HEX(0xb78b63),RS_HEX(0x89674f),RS_HEX(0x614e43),RS_HEX(0xdfb688),RS_HEX(0xeaf0b5),RS_HEX(0xf8cb85),RS_HEX(0xbc7789),RS_HEX(0x587c4d),RS_HEX(0xaabac4),RS_HEX(0xeef3dd),RS_HEX(0x25273b),RS_HEX(0xe36f91)};
static rs_color theme_sky(const world *w,int y,int height){(void)w;return RS_RGB8(220-y*40/height,236-y*29/height,190-y*36/height);}
static int theme_scroll(const hero *h){(void)h;return 0;}
static void oval(uint8_t *pixels,int cx,int cy,int rx,int ry,int color,const rs_viewport *vp){for(int y=-ry;y<=ry;y++)for(int x=-rx;x<=rx;x++)if(x*x*ry*ry+y*y*rx*rx<=rx*rx*ry*ry)pixel(pixels,cx+x,cy+y,color,vp);}
static void garden_line(uint8_t*p,int ax,int ay,int bx,int by,int color,int width,const rs_viewport*vp){int n=abs(bx-ax)>abs(by-ay)?abs(bx-ax):abs(by-ay);if(n<1)n=1;for(int k=0;k<=n;k++){int x=ax+(bx-ax)*k/n,y=ay+(by-ay)*k/n;rect(p,x-width,y-width,x+width,y+width,color,vp);}}
static void theme_field(const world *w,int p,const view *v,const rs_viewport *vp,int title,uint8_t *pixels){
 const hero *h=&w->h[p];(void)title;
 int cx=sx(v,160),cy=sy(v,146);
 // Soft worn paths radiate between the four raised beds.
 for(int i=0;i<4;i++){int xx,yy;bed_position(i,&xx,&yy);int x=sx(v,xx),y=sy(v,yy);garden_line(pixels,cx,cy+10,x,y+10,2,v->small?3:6,vp);garden_line(pixels,cx,cy+10,x,y+10,1,v->small?2:4,vp);}
 for(int i=0;i<4;i++){
  int xx,yy;bed_position(i,&xx,&yy);int x=sx(v,xx),y=sy(v,yy),rx=(v->w-16)*35/320,ry=v->small?5:10;
  oval(pixels,x+2,y+7,rx+3,ry+3,3,vp);
  // Perspective soil face, wooden side rails and a substantial front plank.
  for(int k=-ry;k<=ry/2;k++){int half=rx-3+(k+ry)*3/(ry+1);rect(pixels,x-half,y+k,x+half,y+k,6,vp);if(k>-ry+1)rect(pixels,x-half+2,y+k,x+half-2,y+k,5,vp);}
  garden_line(pixels,x-rx+3,y-ry,x+rx-3,y-ry,7,0,vp);
  garden_line(pixels,x-rx+3,y-ry,x-rx,y+ry/2,4,1,vp);garden_line(pixels,x+rx-3,y-ry,x+rx,y+ry/2,6,1,vp);
  rect(pixels,x-rx,y+ry/2,x+rx,y+ry/2+(v->small?4:7),6,vp);rect(pixels,x-rx+1,y+ry/2+1,x+rx-1,y+ry/2+(v->small?2:5),4,vp);rect(pixels,x-rx+1,y+ry/2,x+rx-1,y+ry/2,7,vp);
  for(int side=-1;side<=1;side+=2){pixel(pixels,x+side*(rx-3),y+ry/2+2,6,vp);pixel(pixels,x+side*(rx-2),y+ry/2+2,7,vp);}
  for(int k=-rx+5;k<rx-3;k+=7){pixel(pixels,x+k,y+1,4,vp);pixel(pixels,x+k+1,y+2,6,vp);}
  const bed *b=&h->beds[i];if(b->state==BED_WARN){int shake=(b->t/3)&1;oval(pixels,x,y-3-shake,rx/3,2,6,vp);garden_line(pixels,x-3,y-ry-2,x-5,y-ry-5,7,0,vp);garden_line(pixels,x+3,y-ry-2,x+5,y-ry-5,7,0,vp);}
  if(b->state==BED_MISS&&((b->t/3)&1))oval(pixels,x,y-2,rx-3,ry/2,10,vp);
 }
 // The fork thrust follows the pressed direction and returns to the gardener's hand.
 if(h->state<HS_HIT){
  int facing=h->aim==3?-1:1;
  int ax=cx+facing*(v->small?7:12),ay=cy+(v->small?4:7),tx=ax+facing*(v->small?9:15),ty=ay-(v->small?6:12),reach=256;
  if(h->aim==0||h->aim==2){ax=cx+(v->small?5:10);ay=cy+(h->aim==0?(v->small?-1:0):(v->small?6:11));tx=ax;ty=ay+(h->aim==0?-1:1)*(v->small?13:23);}
  if(h->smooth_t&&h->aim>=0){int bx,by;bed_position(h->aim,&bx,&by);tx=sx(v,bx);ty=sy(v,by)-(v->small?6:12);int phase=12-h->smooth_t;reach=phase<5?(phase+1)*256/5:(12-phase)*256/7;}
  int ex=ax+(tx-ax)*reach/256,ey=ay+(ty-ay)*reach/256,dx=tx-ax,dy=ty-ay;
  int n=abs(dx)>abs(dy)?abs(dx):abs(dy);if(n<1)n=1;
  int head=v->small?4:7,spread=v->small?2:4,ux=dx*head/n,uy=dy*head/n,nx=-dy*spread/n,ny=dx*spread/n;
  garden_line(pixels,ax,ay,ex-ux,ey-uy,14,1,vp);garden_line(pixels,ax,ay,ex-ux,ey-uy,7,0,vp);
  garden_line(pixels,ex-ux-nx,ey-uy-ny,ex-ux+nx,ey-uy+ny,12,1,vp);
  for(int side=-1;side<=1;side++)garden_line(pixels,ex-ux+side*nx,ey-uy+side*ny,ex+side*nx,ey+side*ny,13,0,vp);
 }
 // One harvest heart: the first mistake ends this player's run.
 if(!title){int x=cx,y=v->small?23:35,col=h->hearts?15:3;oval(pixels,x-2,y,2,2,col,vp);oval(pixels,x+2,y,2,2,col,vp);for(int k=0;k<4;k++)rect(pixels,x-3+k,y+1+k,x+3-k,y+1+k,col,vp);}
}
static void farmer(int id,int x,int y,int pal,int left,const rs_viewport*vp){const gm_sprite_def*s=&gm_spr[id];rs_spr(x-s->w/2,y-s->h/2,s->tile,s->w,s->h,pal,2,left?RS_SPR_HFLIP:0);(void)vp;}
static void theme_scene(const world *w,int p,int t,const view *v,const rs_viewport *vp){
 const hero *h=&w->h[p];int x=sx(v,h->x),y=sy(v,h->y/Q16_ONE);
 if(h->state!=HS_OFF){
  int id=v->small?(h->smooth_t?SPR_FARMER_SMALL_STRIKE:SPR_FARMER_SMALL+(t/12)%2):h->state>=HS_HIT?SPR_HERO_HIT:h->smooth_t?SPR_HERO_AIR:SPR_HERO_IDLE+(t/12)%2;
  if(h->state<HS_HIT&&(h->aim==0||h->aim==2)){
   id=v->small?(h->aim==0?SPR_FARMER_SMALL_BACK:SPR_FARMER_SMALL_FRONT):(h->aim==0?SPR_FARMER_BACK:SPR_FARMER_FRONT);
   if(h->smooth_t)id++;
  }
  farmer(id,x,y,ppal(p),h->aim==3,vp);
 }
 for(int i=0;i<4;i++){
  int bx,by;bed_position(i,&bx,&by);int xx=sx(v,bx),yy=sy(v,by);const bed *b=&h->beds[i];
  int plant=v->small?SPR_RADISH_SMALL:SPR_RADISH,ps=v->small?8:16,side=(v->w-16)*22/320;
  if(b->state!=BED_MISS){spr(plant,xx-side-ps/2,yy-ps,2,-1,vp);spr(plant,xx+side-ps/2,yy-ps,2,-1,vp);spr(SPR_RADISH_SMALL,xx-side/2-4,yy-6,2,-1,vp);spr(SPR_RADISH_SMALL,xx+side/2-4,yy-6,2,-1,vp);}
  if(b->state==BED_ACTIVE||b->state==BED_SHOO||b->state==BED_SAFE){
   int id=b->friend?(v->small?SPR_HEDGEHOG_SMALL:SPR_HEDGEHOG):(v->small?SPR_RABBIT_SMALL:SPR_RABBIT)+(t/8)%2;
   const gm_sprite_def *s=&gm_spr[id];int jump=b->state==BED_SHOO?b->t*(20-b->t)/12:0;if(v->small)jump/=2;
   spr(id,xx-s->w/2,yy-s->h+2-jump,2,-1,vp);
   if(b->state==BED_SHOO||b->state==BED_SAFE)hu_sparkle(xx+s->w/2,yy-s->h,t,2);
  }
 }
}
static void theme_title(const world*w,int t,const view*v,const rs_viewport*vp){theme_scene(w,0,t,v,vp);}
