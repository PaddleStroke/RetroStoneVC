/*
 * Mouse Espresso: the cafe items, code-drawn in the house style (dark outline, three-tone shading lit from the top
 * left) into an 8-bit canvas of the scenery palette (draw.c: the playfield layer; bot.c: the bot's eyes).
 * Palette (draw.c draw_init): 1 outline, 2-4 steel dark..light, 5 cream, 6-7 coffee browns, 8-9 tans, 10 gold,
 * 11 brick, 12 biscuit, 13 coral, 14 counter grey, 15 aubergine.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/mouseespresso/LICENSE.
 */
#include "paint.h"
#include "sine.h"
#include <stdlib.h>

static void pixel(const canvas *c,int x,int y,int color){
 if(x>=0 && x<c->w && y>=0 && y<c->h)c->p[y*c->stride+x]=(uint8_t)color;
}
static void rect(const canvas *c,int x0,int y0,int x1,int y1,int color){
 if(x0<0)x0=0;
 if(y0<0)y0=0;
 if(x1>=c->w)x1=c->w-1;
 if(y1>=c->h)y1=c->h-1;
 for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++)c->p[y*c->stride+x]=(uint8_t)color;
}
/* an outlined, shaded ellipse: light on the top third, mid on the left, dark on the right */
static void oval(const canvas *c,int cx,int cy,int rx,int ry,int light,int mid,int dark){
 for(int y=-ry-1;y<=ry+1;y++)for(int x=-rx-1;x<=rx+1;x++){
  int d=x*x*ry*ry+y*y*rx*rx;
  if(d>(rx+1)*(rx+1)*(ry+1)*(ry+1))continue;
  pixel(c,cx+x,cy+y,d>rx*rx*ry*ry?1:y<-ry/3?light:x<rx/3?mid:dark);
 }
}
static void line(const canvas *c,int x0,int y0,int x1,int y1,int width,int color){
 int dx=abs(x1-x0),dy=abs(y1-y0),steps=dx>dy?dx:dy;if(!steps)steps=1;
 for(int i=0;i<=steps;i++){int x=x0+(x1-x0)*i/steps,y=y0+(y1-y0)*i/steps;rect(c,x-width,y-width,x+width,y+width,color);}
}
/* an outlined box: light top edge, dark right edge */
static void box(const canvas *c,int x0,int y0,int x1,int y1,int light,int mid,int dark){
 rect(c,x0,y0,x1,y1,1);rect(c,x0+1,y0+1,x1-1,y1-1,mid);
 rect(c,x0+1,y0+1,x1-1,y0+1,light);rect(c,x1-2,y0+2,x1-1,y1-1,dark);
}
static void string(const canvas *c,int x,int bottom){rect(c,x,0,x,bottom,1);}

/* ---- hanging ------------------------------------------------------------------------------------------------------ */
static void lamp(const canvas *c,const item *it,int x){
 int l=it->len,hw=it->size,coral=hw&1;
 string(c,x,l);rect(c,x-2,l-2,x+2,l,1);
 /* the shade: a dome, coral enamel or steel */
 for(int y=0;y<=9;y++){int w=hw*(4+y)/13;if(w<3)w=3;
  for(int dx=-w;dx<=w;dx++){int ink=abs(dx)==w || y==9?1:y<3?(coral?13:4):dx<w/3?(coral?13:3):(coral?11:2);
   if(y==1 && dx<0 && dx>-w+1)ink=coral?5:4;
   pixel(c,x+dx,l+y,ink);
  }
 }
 oval(c,x,l+10,3,2,5,10,10);
}
static void teabag(const canvas *c,const item *it,int x){
 int l=it->len;
 rect(c,x,0,x,l,4);if(l>12)box(c,x+1,l/2-4,x+6,l/2+1,5,13,11);   /* the string and its paper tag */
 box(c,x-5,l,x+5,l+13,9,8,7);rect(c,x-3,l+5,x+3,l+5,7);rect(c,x-3,l+9,x+2,l+9,7);
 rect(c,x-2,l,x+2,l+1,4);pixel(c,x,l+1,3);
}
static void cuphang(const canvas *c,const item *it,int x){
 int l=it->len;
 string(c,x,l);
 /* hung by its handle from a hook: the handle loop on top, the cup upside down below it */
 oval(c,x+1,l+2,3,2,4,3,2);pixel(c,x+1,l+2,0);
 box(c,x-7,l+3,x+7,l+11,5,5,3);rect(c,x-6,l+6,x+6,l+7,it->x&1?13:2);rect(c,x-8,l+3,x+8,l+4,1);rect(c,x-7,l+4,x+7,l+4,4);
}
static void filter(const canvas *c,const item *it,const item_pose *p,int x){
 int l=it->len;
 rect(c,x-1,0,x+1,l,1);rect(c,x,0,x,l,3);
 /* a steel holder ring and the paper cone with its coffee */
 box(c,x-10,l,x+10,l+3,4,3,2);
 for(int y=4;y<=13;y++){int w=9-(y-4)*7/9;
  for(int dx=-w;dx<=w;dx++)pixel(c,x+dx,l+y,abs(dx)==w || y==13?1:y==4?6:dx<0?5:4);}
 rect(c,x-5,l+4,x+5,l+5,6);
 for(int k=0;k<p->drops;k++){int y=p->dy[k];rect(c,x-1,y,x+1,y+4,1);rect(c,x,y+1,x,y+3,7);pixel(c,x,y,6);}
}
/* ---- standing on the counter ------------------------------------------------------------------------------------- */
static void cups(const canvas *c,const item *it,int x){
 int n=it->var,band=(it->x>>2)&1?13:2;
 box(c,x-9,COUNTER_Y-3,x+9,COUNTER_Y-1,4,4,3);
 for(int i=0;i<n;i++){
  int b=COUNTER_Y-3-i*11,dx=(i*5+it->x)%3-1;
  box(c,x-7+dx,b-10,x+7+dx,b,5,5,3);rect(c,x-6+dx,b-6,x+6+dx,b-5,band);
  oval(c,x+9+dx,b-5,2,2,4,4,3);pixel(c,x+9+dx,b-5,0);
  if(i==n-1){rect(c,x-6+dx,b-9,x+6+dx,b-9,6);pixel(c,x-3+dx,b-9,7);}
 }
}
static void sugar(const canvas *c,const item *it,int x){
 int cols=it->size,n=it->var;
 for(int i=0;i<n;i++)for(int j=0;j<cols;j++){
  int x0=x-4*cols+j*8+((i&1) && cols==1?((it->x>>3)&1):0),y1=COUNTER_Y-1-i*8;
  box(c,x0,y1-7,x0+8,y1,5,4,3);pixel(c,x0+2,y1-4,5);pixel(c,x0+5,y1-2,3);
 }
}
static void jug(const canvas *c,const item *it,int x){
 int h=it->len,top=COUNTER_Y-h;
 /* a steel milk jug: a handle on the right, a spout on the left, milk inside */
 line(c,x+11,top+7,x+15,top+9,1,1);line(c,x+15,top+9,x+14,COUNTER_Y-9,1,1);line(c,x+14,COUNTER_Y-9,x+10,COUNTER_Y-8,1,1);
 for(int y=top;y<COUNTER_Y;y++){int k=y-top,w=k<4?8+k/2:k<h/3?10:11;
  for(int dx=-w;dx<=w;dx++)pixel(c,x+dx,y,abs(dx)==w || y==COUNTER_Y-1?1:dx<-w/2?4:dx<w/2?3:2);}
 rect(c,x-8,top,x+8,top,1);rect(c,x-7,top+1,x+7,top+2,5);
 line(c,x-8,top,x-12,top-3,0,1);pixel(c,x-11,top-2,4);
 rect(c,x-6,top+h/2,x+6,top+h/2,4);
}
/* ---- floating -------------------------------------------------------------------------------------------------------- */
static void spoon(const canvas *c,const item *it,int x){
 int l=it->size,d=l*181/256,ax,ay,bx,by;
 if(it->var==0){ax=x-l;ay=it->y;bx=x+l;by=it->y;}
 else if(it->var==1){ax=x-d;ay=it->y+d;bx=x+d;by=it->y-d;}
 else{ax=x-d;ay=it->y-d;bx=x+d;by=it->y+d;}
 line(c,ax,ay,bx,by,1,1);line(c,ax,ay,bx,by,0,3);
 oval(c,bx,by,4,3,4,3,2);pixel(c,bx-1,by-1,5);
}
static void bean(const canvas *c,const item *it,int x){
 int y=it->y,r=it->size;
 oval(c,x,y,r,r*3/4>2?r*3/4:3,11,7,6);line(c,x-r+2,y+1,x+r-2,y-1,0,1);pixel(c,x-1,y-2,12);
}
static void cake(const canvas *c,const item *it,int x){
 int hw=it->size,y=it->y;
 /* a wedge pointing left: sponge layers, cream, a strawberry on top */
 for(int dx=-hw;dx<=hw;dx++){int top=y-6+(hw-dx)*0/1,h=6+(dx+hw)*7/(2*hw);top=y+7-h-6;
  for(int yy=top;yy<=y+7;yy++){int k=yy-top,ink=yy==y+7 || dx==-hw || dx==hw || yy==top?1:k<2?5:((yy-y)&3)==0?5:dx>hw/2?8:9;pixel(c,x+dx,yy,ink);}}
 oval(c,x+hw-4,y-8,3,3,13,13,11);pixel(c,x+hw-4,y-11,7);
}
static void croissant(const canvas *c,const item *it,int x){
 int hw=it->size,y=it->y;
 for(int k=-2;k<=2;k++){int cx=x+k*hw/3,cy=y+(abs(k)==2?1:abs(k)==1?-1:-2),rx=k?hw/4+1:hw/3,ry=abs(k)==2?3:5;
  oval(c,cx,cy,rx,ry,10,9,8);}
 for(int k=-1;k<=1;k++)line(c,x+k*hw/3+hw/6,y-4,x+k*hw/3+hw/6,y+2,0,8);
}
/* ---- moving -------------------------------------------------------------------------------------------------------- */
static void jet(const canvas *c,const item *it,const item_pose *p,int x,int t){
 int floor=it->var==0,base=floor?COUNTER_Y-8:8,dir=floor?-1:1;
 /* the steel nozzle and its pressure light: red while it warns and fires */
 if(floor){box(c,x-6,COUNTER_Y-8,x+6,COUNTER_Y-1,4,3,2);rect(c,x-3,COUNTER_Y-10,x+3,COUNTER_Y-8,1);}
 else{box(c,x-6,0,x+6,7,4,3,2);rect(c,x-3,8,x+3,10,1);}
 int light=p->h || (p->warn && (t&8))?13:6;pixel(c,x+3,floor?COUNTER_Y-5:3,light);pixel(c,x+4,floor?COUNTER_Y-5:3,light);
 if(p->warn){                     /* the telegraph: harmless puffs at the nozzle and wisps rising as high as it fires */
  int n=(t>>2)&3;for(int k=0;k<2;k++){int yy=base+dir*(4+k*6+n),r=2+k;oval(c,x+(k?2:-1),yy,r,r-1,5,4,3);}
  for(int d=14+((t>>1)&3);d<=it->len*p->rise/JET_TELEGRAPH;d+=6){int wx=x+((d/6+(t>>3))&1?1:-1);pixel(c,wx,base+dir*d,4);pixel(c,wx,base+dir*(d+1),3);}
 }
 if(!p->h)return;
 for(int d=0;d<=p->h;d+=4){int cx=x+(d?((d+t*3)%9-4)/2:0),cy=base+dir*d,rx=3+d/30;if(rx>5)rx=5;
  for(int yy=-3;yy<=3;yy++)for(int xx=-rx;xx<=rx;xx++)if(xx*xx*9+yy*yy*rx*rx<=rx*rx*9){
   if(d>p->h-6 && ((cx+xx+cy+yy)&3)==0)continue;
   pixel(c,cx+xx,cy+yy,xx<-rx/3?5:xx<rx/2?4:3);
  }
 }
}
static void ladle(const canvas *c,const item_pose *p,int x,int left){
 int tx=p->tx-left,ty=p->ty;
 rect(c,x-3,0,x+3,2,1);rect(c,x-2,0,x+2,1,3);
 line(c,x,2,tx,ty,1,1);line(c,x,2,tx,ty,0,3);
 oval(c,tx,ty+1,LADLE_BOWL,LADLE_BOWL-1,4,3,2);rect(c,tx-LADLE_BOWL+2,ty-2,tx+LADLE_BOWL-2,ty-1,1);rect(c,tx-4,ty-1,tx+4,ty-1,5);
}
static void bounce(const canvas *c,const item *it,const item_pose *p,int x){
 int y=p->ty,squash=y>COUNTER_Y-9?1:0;
 (void)it;
 rect(c,x-4,COUNTER_Y-2,x+4,COUNTER_Y-1,14);                    /* its shadow on the counter */
 box(c,x-5,y-5+squash,x+5,y+5,5,4,3);pixel(c,x-2,y-2+squash,5);pixel(c,x+2,y+2,3);
}
void paint_item(const canvas *c,const item *it,const item_pose *p,int left,int t){
 int x=it->x-left;
 switch(it->kind){
 case IT_LAMP:lamp(c,it,x);break;
 case IT_TEABAG:teabag(c,it,x);break;
 case IT_CUPHANG:cuphang(c,it,x);break;
 case IT_FILTER:filter(c,it,p,x);break;
 case IT_CUPS:cups(c,it,x);break;
 case IT_SUGAR:sugar(c,it,x);break;
 case IT_JUG:jug(c,it,x);break;
 case IT_SPOON:spoon(c,it,x);break;
 case IT_BEAN:bean(c,it,x);break;
 case IT_CAKE:cake(c,it,x);break;
 case IT_CROISSANT:croissant(c,it,x);break;
 case IT_JET:jet(c,it,p,x,t);break;
 case IT_LADLE:ladle(c,p,x,left);break;
 case IT_BOUNCE:bounce(c,it,p,x);break;
 default:break;
 }
}
/* every item of the course that reaches the canvas, at frame t */
void paint_course(const canvas *c,const course *cs,int left,int t){
 for(uint32_t i=(uint32_t)course_first_near(cs,left);i<cs->n;i++){
  const item *it=course_item(cs,i);
  if(it->x-100>left+c->w)break;
  if(it->kind==IT_BISCUIT)continue;
  int l,r;item_extent(it,&l,&r);if(r<left-2 || l>left+c->w+2)continue;
  item_pose p;item_pose_at(it,t,&p);paint_item(c,it,&p,left,t);
 }
}
