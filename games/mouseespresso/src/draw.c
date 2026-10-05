/* Mouse Espresso: the replaceable café theme. Flight/collision rules are in play.c.
 * Widescreen (DESIGN.md "Widescreen"): a side-scroller, so the café is revealed wider, to the right (the stage origin
 * stays 0): the mice fly at the same screen x, the shared camera shows the hazards, biscuits and counter further
 * ahead (the same course: obstacle_at, collectible_at). The maps are 128 tiles wide; the café's 512-px back wall goes
 * on with its mirror image (the same tiles h-flipped: nothing repeats on a 560-px screen). The UI is centred (the UI
 * layer scrolls by -hu_ui_x(), the game's UI sprites add it). */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include "game_text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct view {int cx,mx,my,zx,zy;rs_color sky[240];} view;
static view V[4];static rs_viewport VP[4];static int nviews,shown_state=-1,shown_best=-1;
static int laid_w;                     /* the width the maps were laid out for (a state of another width: relayout) */
/* the furniture layer's pixels (foreground(): per-frame scratch, state_audit.txt), as wide as the widest screen */
#define PIX_W RS_SCREEN_W_MAX
static uint8_t pixels[PIX_W*240];
static int wide(void) {return rs_screen_width()>RS_SCREEN_W;}
static const rs_color title_ramps[2][3]={
 {RS_HEX(0xffd8ba),RS_HEX(0xea9868),RS_HEX(0xa65951)},
 {RS_HEX(0xe1eddd),RS_HEX(0x9bc1bd),RS_HEX(0x567f91)}};
static void raster(int line,void *user) {
 (void)user;int p=rs_viewport_current();if(p<0 || p>=nviews)p=0;
 int y=line-VP[p].y;if(y>=0 && y<240)rs_pal_set(0,V[p].sky[y]);
}
/* the maps of this width: 64 tiles at 320, 128 wider */
static void maps_setup(void) {
 int mw=wide()?128:64;
 rs_bg_setup(RS_BG1,mw,32,VR_BG1);rs_bg_setup(RS_BG2,mw,32,VR_BG2);rs_bg_setup(RS_BG3,mw,32,VR_BG3);
 laid_w=rs_screen_width();
}
/* After a state was loaded (rs_game.state_loaded): one saved at another width brings that width's maps; set this
 * width's up (each frame draws the whole screen again). Same width: nothing. */
void draw_relayout(void) {if(laid_w!=rs_screen_width())maps_setup();}
void draw_init(void) {
 hu_config cfg=hu_defaults();cfg.obj_tile=KIT_OBJ_TILE;cfg.obj_vram=VR_OBJ;cfg.obj_pal=3;hu_init(&cfg);
 maps_setup();
 rs_tiles_load(VR_BG3,gm_bg3_tiles,gm_bg3_tile_count);
 uint8_t blank[64]={0};rs_tiles_load8(VR_BG2,blank,1);rs_tiles_load8(VR_BG3+gm_bg3_tile_count,blank,1);
 rs_pal_load(RS_PAL_BG(PAL_HILLS),gm_hills_pal,16);
 static const rs_color scenery[16]={0,RS_HEX(0x23233b),RS_HEX(0x486580),RS_HEX(0x8baabb),RS_HEX(0xd4e5e5),
  RS_HEX(0xfff2da),RS_HEX(0x584449),RS_HEX(0x906b56),RS_HEX(0xc39568),RS_HEX(0xe0b780),
  RS_HEX(0xf4ce7e),RS_HEX(0x956450),RS_HEX(0xd5a77a),RS_HEX(0xf49568),RS_HEX(0x5a4d53),RS_HEX(0x423a4b)};
 rs_pal_load(RS_PAL_BG(PAL_GROUND),scenery,16);
 rs_obj_base(VR_OBJ);rs_tiles_load(VR_OBJ,gm_obj_tiles,gm_obj_tile_count);
 rs_pal_load(RS_PAL_OBJ(0),gm_obj_pals,48);rs_pal_load(RS_PAL_OBJ(4),gm_obj_pals+64,32);
 for(int l=0;l<3;l++)rs_bg_enable(l,1);
 memset(V,0,sizeof V);memset(VP,0,sizeof VP);nviews=0;rs_raster(raster,NULL);
}
static int ppal(int p) {return p?hu_player_pal(p):0;}
static int floor8(int x) {return x>=0?x/8:(x-7)/8;}
static int sx(const view *v,int x) {return (x-v->cx)*v->zx/256;}
static void spr(int id,int x,int y,int priority,int pal,const rs_viewport *vp) {
 const gm_sprite_def *s=&gm_spr[id];
 if(x<=-(int)s->w || x>=vp->w || y<=-(int)s->h || y>=vp->h)return;
 rs_spr(x,y,s->tile,s->w,s->h,pal<0?s->pal:pal,priority,0);
}
static void background(const view *v,rs_viewport *vp) {
 int offset=v->cx/4,fx=floor8(offset),dx=offset-fx*8;
 int cols=(vp->w+dx+7)/8,rows=(vp->h+7)/8;
 for(int r=0;r<rows;r++)for(int c=0;c<cols;c++) {
  int world_y=v->zy==256?r*8:(r*8-12)*2;
  int yy=floor8(world_y),xx=(fx+c*(v->zx==128?2:1))&(wide()?127:63);
  /* widescreen: a 1024-px period, the back wall then its mirror image (h-flipped tiles: seamless at both joins) */
  uint16_t tile=yy<0 || yy>=HILLS_H?RS_MAP(gm_bg3_tile_count,PAL_HILLS,0,0,0):xx<64?gm_hills_map[yy*64+xx]:
   (uint16_t)(gm_hills_map[yy*64+127-xx]^RS_MAP_HFLIP);
  rs_bg_put(RS_BG3,v->mx+c,v->my+r,tile);
 }
 vp->sx[2]=(int16_t)(v->mx*8+dx);vp->sy[2]=(int16_t)(v->my*8);
}
static void pixel(uint8_t *p,int x,int y,int color,const rs_viewport *vp) {
 if(x>=0 && x<vp->w && y>=0 && y<vp->h)p[y*PIX_W+x]=(uint8_t)color;
}
static void rect(uint8_t *p,int x0,int y0,int x1,int y1,int color,const rs_viewport *vp) {
 if(x0<0)x0=0;
 if(y0<0)y0=0;
 if(x1>=vp->w)x1=vp->w-1;
 if(y1>=vp->h)y1=vp->h-1;
 for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++)p[y*PIX_W+x]=(uint8_t)color;
}
static void oval(uint8_t *p,int cx,int cy,int rx,int ry,int light,int mid,int dark,const rs_viewport *vp){
 for(int y=-ry-1;y<=ry+1;y++)for(int x=-rx-1;x<=rx+1;x++){
  int d=x*x*ry*ry+y*y*rx*rx;
  if(d>(rx+1)*(rx+1)*(ry+1)*(ry+1))continue;
  pixel(p,cx+x,cy+y,d>rx*rx*ry*ry?1:y<-ry/3?light:x<0?mid:dark,vp);
 }
}
static void line(uint8_t *p,int x0,int y0,int x1,int y1,int width,int color,const rs_viewport *vp){
 int dx=abs(x1-x0),dy=abs(y1-y0),steps=dx>dy?dx:dy;if(!steps)steps=1;
 for(int i=0;i<=steps;i++)rect(p,x0+(x1-x0)*i/steps-width,y0+(y1-y0)*i/steps-width,
  x0+(x1-x0)*i/steps+width,y0+(y1-y0)*i/steps+width,color,vp);
}
static void box(uint8_t *p,int x0,int y0,int x1,int y1,int light,int mid,int dark,const rs_viewport *vp){
 rect(p,x0,y0,x1,y1,1,vp);rect(p,x0+1,y0+1,x1-1,y1-1,mid,vp);
 rect(p,x0+1,y0+1,x1-1,y0+2,light,vp);rect(p,x1-3,y0+3,x1-1,y1-1,dark,vp);
}
static void cat_paw(uint8_t *p,const obstacle *o,const view *v,const rs_viewport *vp){
 if(!o->reach)return;
 int root=sx(v,o->x),x=sx(v,o->tip_x),tip=o->tip,cy=o->side?tip+13:tip-13,edge=o->side?232:0;
 int length=abs(cy-edge);if(length<1)length=1;
 /* A tapered, shaded foreleg with anchored tabby markings, rather than a rectangular arm. */
 for(int k=0;k<=length;k++){
  int yy=edge+(cy-edge)*k/length,cx=root+(x-root)*k/length,radius=12-3*k/length;
  for(int dx=-radius;dx<=radius;dx++){
   int ink=abs(dx)==radius?1:dx<-radius/2?5:dx<radius/2?12:11;
   if(k%29>=12 && k%29<=16 && (dx<-2 || dx>5))ink=11;
   pixel(p,cx+dx,yy,ink,vp);
  }
 }
 oval(p,x,cy,17,12,5,12,11,vp);
 /* Four uneven, separated toes make the tip read as a feline paw. */
 for(int j=0;j<4;j++){
  int toe=x-12+j*8,yy=o->side?tip+6:tip-6;yy+=(j==0 || j==3)?(o->side?2:-2):0;
  oval(p,toe,yy,5,5,5,12,11,vp);
  if(o->active && o->reach>80){int claw=o->side?tip+2:tip-2;pixel(p,toe-1,claw,4,vp);pixel(p,toe,claw,5,vp);}
 }
 if(o->side){
  oval(p,x,cy-1,7,5,13,13,11,vp);
  for(int j=0;j<4;j++)oval(p,x-12+j*8,tip+7+(j==0 || j==3?2:0),2,2,13,13,11,vp);
 }else{
  line(p,x-9,cy-1,x-2,cy+3,1,11,vp);line(p,x+10,cy+3,x+5,cy+6,1,11,vp);
 }
}
static void steam_jet(uint8_t *p,const obstacle *o,const view *v,const rs_viewport *vp){
 if(!o->effect)return;
 int x=sx(v,o->jet_x);
 for(int d=0;d<=o->effect;d+=4){int cx=x+(d?((d+o->phase*3)%13-6)/2:0),cy=o->jet_y-d,rx=3+d/18;
  for(int yy=-6;yy<=6;yy++)for(int xx=-rx;xx<=rx;xx++)if(xx*xx*36+yy*yy*rx*rx<=rx*rx*36){
   if(d>o->effect-8 && ((cx+xx+cy+yy)&3)==0)continue;
   pixel(p,cx+xx,cy+yy,xx<-rx/3?5:xx<rx/2?4:3,vp);
  }
 }
}
static void furniture(uint8_t *p,const obstacle *o,const view *v,const rs_viewport *vp){
 int x=sx(v,o->x),y=o->tip;
 if(o->type==OB_STOOL){
  // A wooden bar stool, padded diner stool, or a chair with a rounded back.
  line(p,x-15,y+8,x-22,231,3,1,vp);line(p,x+15,y+8,x+22,231,3,1,vp);
  line(p,x-15,y+8,x-22,231,2,8,vp);line(p,x+15,y+8,x+22,231,2,7,vp);
  box(p,x-18,206,x+18,210,9,8,7,vp);
  if(o->variant==2){box(p,x-20,y,x+20,y+26,9,8,7,vp);box(p,x-15,y+5,x+15,y+17,9,7,6,vp);y+=25;}
  oval(p,x,y+6,24,6,o->variant==1?13:9,o->variant==1?11:8,7,vp);
 }else if(o->type==OB_SHELF){
  line(p,x-26,0,x-26,y+5,1,3,vp);line(p,x+26,0,x+26,y+5,1,3,vp);
  box(p,x-34,y,x+34,y+10,9,8,7,vp);
  for(int j=0;j<3;j++){
   int cx=x-23+j*23,top=y-19-(j==1?6:0);
   if(o->variant==1){box(p,cx-7,top,cx+7,y-1,4,3,2,vp);box(p,cx-8,top-3,cx+8,top,9,7,6,vp);}
   else{box(p,cx-7,top,cx+6,y-1,5,o->variant==2?13:4,3,vp);box(p,cx+6,top+4,cx+11,top+11,5,3,2,vp);rect(p,cx+7,top+6,cx+9,top+9,0,vp);}
  }
 }else if(o->type==OB_TABLE){
  /* Rear supports sit behind the flight lane; the broad tabletop is the solid obstacle. */
  for(int side=-1;side<=1;side+=2){int leg=x+side*(o->width-24);
   line(p,leg,y+12,leg+side*8,231,3,14,vp);line(p,leg-1,y+14,leg+side*8-1,229,1,7,vp);
  }
  box(p,x-o->width,y,x+o->width,y+13,9,8,7,vp);
  rect(p,x-o->width+2,y+9,x+o->width-2,y+10,6,vp);
  for(int grain=-o->width+16;grain<o->width-12;grain+=32)rect(p,x+grain,y+5,x+grain+17,y+5,9,vp);
 }else if(o->type==OB_APPLIANCE){
  // Espresso machine, windmill mixer and steaming kettle.
  box(p,x-23,223,x+23,231,4,3,2,vp);
  if(o->variant==0){
   box(p,x-20,y,x+22,219,4,13,11,vp);box(p,x-16,y+7,x+17,y+25,4,3,2,vp);
   oval(p,x+9,y+15,5,5,5,4,2,vp);line(p,x+9,y+15,x+11,y+12,0,o->warning?13:1,vp);
   box(p,x-12,y+32,x+13,218,4,2,1,vp);box(p,x-8,y+39,x+8,y+43,4,3,2,vp);
   line(p,x,y+43,x,y+49,1,4,vp);box(p,x-9,208,x+8,219,5,4,3,vp);box(p,x+8,210,x+12,215,4,3,2,vp);
   line(p,x-12,y+24,sx(v,o->jet_x),o->jet_y,1,4,vp);pixel(p,sx(v,o->jet_x),o->jet_y,13,vp);
   rect(p,x-17,221,x+17,222,2,vp);pixel(p,x+16,y+32,o->warning?13:10,vp);
  }else if(o->variant==1){
   box(p,x+7,y+9,x+21,224,4,13,11,vp);oval(p,x,y+11,20,10,4,13,11,vp);
   oval(p,x-4,212,18,11,4,3,2,vp);rect(p,x-22,201,x+14,204,4,vp);
   for(int j=0;j<4;j++){const moving_blade *b=&o->blade[j];int bx=sx(v,b->x),tx=sx(v,b->tip_x);
    line(p,bx,b->y,tx,b->tip_y,5,1,vp);line(p,bx,b->y,tx,b->tip_y,3,3,vp);
    line(p,bx-1,b->y-1,tx-1,b->tip_y-1,1,4,vp);oval(p,tx,b->tip_y,5,5,4,3,2,vp);
   }
   oval(p,sx(v,o->blade[0].x),o->blade[0].y,7,7,4,3,2,vp);oval(p,sx(v,o->blade[0].x),o->blade[0].y,3,3,5,13,11,vp);
  }else{
   int cy=(y+12+220)/2,ry=(220-y-12)/2;
   oval(p,x,cy,22,ry,4,3,2,vp);box(p,x-10,y,x+8,y+11,4,3,2,vp);
   line(p,x-15,cy-8,sx(v,o->jet_x),o->jet_y,3,1,vp);line(p,x-15,cy-8,sx(v,o->jet_x),o->jet_y,2,4,vp);
   box(p,x+16,cy-18,x+23,cy+12,9,8,7,vp);rect(p,x+18,cy-15,x+22,cy+9,0,vp);
   box(p,x-9,213,x+11,220,5,o->warning?13:3,11,vp);
  }
  if(o->variant!=1)steam_jet(p,o,v,vp);
 }else if(o->type==OB_TRAP){
  box(p,x-26,219,x+26,231,9,8,7,vp);box(p,x-4,215,x+4,218,4,3,2,vp);
  for(int i=0;i<6;i++)oval(p,x-9+i*3,217,1,2,4,3,2,vp);
  for(int row=0;row<7;row++)rect(p,x+9,211+row,x+9+row,211+row,10,vp);
  pixel(p,x+11,215,7,vp);pixel(p,x+13,217,7,vp);
  int bar=o->tip;
  line(p,x-22,219,x-18,bar,1,4,vp);line(p,x+22,219,x+18,bar,1,3,vp);
  box(p,x-24,bar,x+24,bar+3,4,3,2,vp);
  if(o->warning){pixel(p,x-29,211,13,vp);pixel(p,x+29,211,13,vp);line(p,x-28,212,x-31,207,0,10,vp);line(p,x+28,212,x+31,207,0,10,vp);}
 }else if(o->type==OB_PAW)cat_paw(p,o,v,vp);
}
static void foreground(const world *w,const view *v,const rs_viewport *vp,int *next){
 memset(pixels,0,sizeof pixels);int r1=vp->w-1;      /* the counter: the whole width (widescreen) */
 rect(pixels,0,232,r1,239,6,vp);rect(pixels,0,232,r1,233,9,vp);rect(pixels,0,236,r1,237,15,vp);
 for(int x=0;x<vp->w;x++)if(((x+v->cx)/32)&1)rect(pixels,x,238,x,239,14,vp);
 int base=(v->cx-HAZARD_FIRST)/HAZARD_SPACING;if(base<0)base=0;
 for(int id=base>0?base-1:0;id<=base+3;id++){
  obstacle o;obstacle_at(w,id,w->t,&o);int x=sx(v,o.x);
  if(x<-140 || x>vp->w+140 || o.type==OB_POP)continue;
  furniture(pixels,&o,v,vp);
 }
 int cols=(vp->w+7)/8;
 for(int r=0;r<30;r++)for(int c=0;c<cols;c++){
  uint8_t tile[64];int ink=0;for(int y=0;y<8;y++)for(int x=0;x<8;x++){tile[y*8+x]=pixels[(r*8+y)*PIX_W+c*8+x];ink|=tile[y*8+x];}
  int id=0;if(ink){id=(*next)++;rs_tiles_load8(VR_BG2+id,tile,1);}rs_bg_put(RS_BG2,c,r,RS_MAP(id,PAL_GROUND,0,0,0));
 }
}
static void scene(const world *w,int t,const view *v,const rs_viewport *vp){
 // Heroes win OAM overlap against their steam and kitchen props.
 for(int p=0;p<w->players;p++){
  const hero *h=&w->h[p];int x=sx(v,(int)(h->wx>>16)),y=h->y>>16;
  if(h->state==HS_OFF)continue;
  int frame=h->state==HS_HIT || h->state==HS_DOWN?SPR_HERO_HIT:h->land_t<5?SPR_HERO_SQUASH:
   h->grounded?SPR_HERO_IDLE+(t/5)%2:h->held?SPR_HERO_STRETCH:SPR_HERO_AIR;
  spr(frame,x-16,y-16,2,ppal(p),vp);
  if(h->held && h->state==HS_RUN && !h->grounded)spr(SPR_STEAM+(t/4)%3,x-15,y+10,2,-1,vp);
  if(h->smooth_t>0)hu_sparkle(x+18-hu_ui_x(),y-10,w->t,2);   /* kit sprites: UI coordinates */
  if(h->state==HS_HIT && h->t<20)spr(SPR_PUFF+(h->t/7)%3,x-8,y+4,2,-1,vp);
 }
 int base=(v->cx-HAZARD_FIRST)/HAZARD_SPACING;if(base<0)base=0;
 for(int id=base>0?base-1:0;id<=base+3;id++){
  obstacle o;obstacle_at(w,id,w->t,&o);if(o.type!=OB_POP)continue;
  int x=sx(v,o.x);spr(SPR_TOASTER+(o.warning || o.phase<16),x-20,208,2,-1,vp);
  for(int i=0;i<2;i++)if(o.shot[i].active){
   const toast_shot *s=&o.shot[i];int frame=(s->age/9+i*2)%4;
   spr(SPR_TOAST+frame,sx(v,s->x)-12,s->y-12,2,-1,vp);
  }
  if(o.phase<18)spr(SPR_PUFF+(o.phase/6)%3,x-8,199,1,-1,vp);
 }
 int group=(v->cx-210)/HAZARD_SPACING;if(group<0)group=0;
 for(int g=group;g<=group+3;g++)for(int j=0;j<5;j++){
  int taken=1;for(int p=0;p<w->players;p++)if(w->h[p].state==HS_RUN || w->h[p].state==HS_READY)taken&=collectible_taken(&w->h[p],g,j);
  if(taken)continue;
  int x,y;collectible_at(w,g,j,&x,&y);spr(SPR_BISCUIT+(w->t/12+j)%2,sx(v,x)-8,y-8,2,-1,vp);
 }
}
static void icon(int p,int cx,int cy,int t,void *user) {
 (void)user;rs_viewport vp={0};vp.w=(int16_t)rs_screen_width();vp.h=240;
 spr(SPR_HERO_ICON,cx-8+hu_ui_x(),cy-8+hu_bob(t+p*16,64,1),2,ppal(p),&vp);   /* the kit's UI coordinates -> stage */
}
static void ranking(const world *w,hu_standing *rank) {
 int scores[4];for(int p=0;p<w->players;p++)scores[p]=w->h[p].score;hu_rank(rank,w->players,scores,scores);
}
static void title_ui(int t,int best){
 hu_logo("MOUSE ESPRESSO",title_ramps,2,2,0);hu_title_draw(t,best);
 // Move the join line above the action, leaving the starting counter unobscured.
 hu_clear_rows(26,1);hu_clear_rows(28,1);char text[160],who[64]="";int n=hu_players();
 for(int p=n;p<4;p++){size_t k=strlen(who);if(p>n)k+=(size_t)snprintf(who+k,sizeof who-k," / ");if(k<sizeof who)rs_strf(who+k,sizeof who-k,HTR(PLAYER_TAG),p+1);}
 if(n<4)rs_strf(text,sizeof text,n>1?TR(JOIN_LEAVE_SHORT):TR(JOIN_SHORT),who);
 else rs_strf(text,sizeof text,"%s",TR(FULL_LEAVE));
 hu_text(hu_center(text,0),12,text);hu_copyright(29);
 /* the hint, centred (half a column left of the true centre: the English keeps its column 4) */
 int hx=(38-hu_width(TR(HINT_TITLE),0))/2;hu_text(hx<0?0:hx,16,TR(HINT_TITLE));hu_title_sprites(t,icon,NULL);
}
void draw_frame(const world *w,int state,int st_t,int best,int new_best,int paused){
 nviews=rs_viewport_layout(1,0,VP);hu_clear();rs_oam_clear();rs_brightness(paused?9:15);
 view *v=&V[0];rs_viewport *vp=&VP[0];v->mx=v->my=0;v->zx=v->zy=256;v->cx=w->scroll;
 if(state==ST_OVER)v->cx=0;
 for(int y=0;y<240;y++){int k=y*256/240,d=(y&4)?4:0;v->sky[y]=RS_RGB8(192+14*k/256+d,172+16*k/256+d/2,150+12*k/256+d);}
 background(v,vp);int next=1;foreground(w,v,vp,&next);
 vp->sx[0]=vp->sx[1]=vp->sy[0]=vp->sy[1]=0;vp->layers=7;vp->objs=1;
 if(state==ST_TITLE)title_ui(st_t,best);
 if(state==ST_PLAY || state==ST_DEAD){
  if(w->players==1){hu_number(w->h[0].score,160,8,3);char text[128];rs_strf(text,sizeof text,TR(HUD_BISCUITS_DIST),w->h[0].apples,w->scroll/12);hu_text(hu_center(text,0),4,text);}
  else for(int p=0;p<w->players;p++){char text[48],tag[16];rs_strf(tag,sizeof tag,HTR(PLAYER_TAG),p+1);snprintf(text,sizeof text,"%s %d",tag,w->h[p].score);hu_text(p*10,1,text);}
  if(paused)hu_big(hu_center(HTR(PAUSED),1),13,HTR(PAUSED),HU_BIG_FREE);
 }
 if(state==ST_TITLE || state==ST_PLAY || state==ST_DEAD)scene(w,w->t,v,vp);
 if(state==ST_OVER){
  hu_standing rank;ranking(w,&rank);int slide=hu_slide_in(st_t,20,200);
  if(w->players==1){
   static const int medals[4]=MEDAL_SCORES;hu_banner(4,HTR(GAME_OVER));hu_gameover_panel(1,new_best,w->h[0].score,0);
   hu_gameover_sprites(1,w->h[0].score,0,best,hu_medal_of(w->h[0].score,medals),st_t,slide);hu_retry_line(st_t,RETRY_LOCK,HTR(BACK_TITLE));
  }else{
   hu_results_panel(&rank,NULL);hu_results_sprites(&rank,st_t,slide);
   for(int i=0;i<rank.n;i++){int x,y;hu_results_icon_pos(&rank,i,&x,&y);spr(SPR_HERO_ICON,x-8+hu_ui_x(),y-8+slide,3,ppal(rank.order[i]),vp);}
   hu_retry_line_at(hu_results_retry_row(&rank),st_t,RETRY_LOCK,HTR(BACK_TITLE));
  }
  rs_bg_scroll(RS_BG1,-hu_ui_x()&(rs_bg_map_w(RS_BG1)*8-1),-slide);   /* the UI centred (widescreen) */
 }else rs_bg_scroll(RS_BG1,-hu_ui_x()&(rs_bg_map_w(RS_BG1)*8-1),0);
 rs_bg_scroll(RS_BG2,0,0);rs_bg_scroll(RS_BG3,vp->sx[2],vp->sy[2]);rs_viewports(0,NULL,0);
 shown_state=state;shown_best=best;
}
#define S(v) rs_state_var("draw." #v,&(v),sizeof(v))
void draw_state(void) {S(V);S(VP);S(nviews);S(shown_state);S(shown_best);S(laid_w);RS_STATE_RASTER(raster);}
#undef S
