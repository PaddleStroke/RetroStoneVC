/* Scattered hazards, both fork routes, mandatory fish and exact camera collisions. */
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c);return 1;}}while(0)
static void cap_start(world*w){w->scroll=24000;w->travel=(int64_t)w->scroll*Q16_ONE;for(int p=0;p<w->players;p++){int d=hero_distance(w,p);w->h[p].next_row=course_index(d);river_row r;course_row(w,w->h[p].next_row,&r);if(r.distance<d-14)w->h[p].next_row++;w->h[p].x=river_route(w,d,(p&1)?1:-1);w->h[p].xx=w->h[p].x*Q16_ONE;}}
int main(void){world w,a,b;int input[4]={0};
 CHECK(river_speed(0)==Q16_ONE*24/10&&river_speed(90000)==Q16_ONE*38/10);
 CHECK(river_current(700,80)>0&&river_current(1100,240)<0&&river_current(0,160)==0);
 int crocs=0,staggered=0,missing=0,fork_hazards=0;
 int min_length=9999,max_length=0,min_width=9999,max_width=0,min_bank=9999,max_bank=0,min_center=9999,max_center=0;
 for(unsigned seed=1;seed<=64;seed++){
  world_init(&w,4,seed);
  int length=0,island_width=0;
  for(int d=0;d<2400;d++){
   int left,right,center,width=river_island(&w,d,&center);river_banks(&w,d,&left,&right);
   CHECK(right-left>=218);if(left<min_bank)min_bank=left;if(left>max_bank)max_bank=left;
   if(width){length++;if(width>island_width)island_width=width;if(center<min_center)min_center=center;if(center>max_center)max_center=center;CHECK(!fish_allowed(&w,d));}
   for(int side=-1;side<=1;side+=2){int x=river_route(&w,d,side);CHECK(x>=left+12&&x<=right-12);CHECK(!width||abs(x-center)>=width+14);}
  }
  if(length<min_length)min_length=length;
  if(length>max_length)max_length=length;
  if(island_width<min_width)min_width=island_width;
  if(island_width>max_width)max_width=island_width;
  for(int id=0;id<200;id++){river_row r,n;course_row(&w,id,&r);course_row(&w,id+1,&n);CHECK(n.distance-r.distance>=200&&n.distance-r.distance<=280);
   CHECK((r.fish>=0)==fish_allowed(&w,r.distance));if(r.fish<0)missing++;
   for(int k=0;k<r.count;k++){
    if(r.kind[k]==CROCODILE)crocs++;
    if(k&&r.at[k]!=r.at[0])staggered++;
    int center,width=river_island(&w,r.at[k],&center);if(width>10)fork_hazards++;
    for(int off=-r.ry[k]-12;off<=r.ry[k]+12;off+=r.ry[k]+12){
     int d=r.at[k]+off,left,right;river_banks(&w,d,&left,&right);width=river_island(&w,d,&center);
     CHECK(r.x[k]-r.rx[k]-5>left&&r.x[k]+r.rx[k]+5<right);
     CHECK(!width||abs(r.x[k]-center)>=width+r.rx[k]+8);
     for(int side=-1;side<=1;side+=2)CHECK(abs(r.x[k]-river_route(&w,d,side))>=r.rx[k]+24);
    }
   }
  }
  for(int mode=0;mode<2;mode++){
   world_init(&a,4,seed);if(mode)cap_start(&a);b=a;
   for(int t=0;t<9000;t++){
    for(int p=0;p<4;p++)input[p]=bot_decide(&a,p);
    world_step(&a,input);world_step(&b,input);CHECK(!memcmp(&a,&b,sizeof a));
    for(int p=0;p<4;p++)if(a.h[p].state!=HS_RUN){fprintf(stderr,"seed %u cap%d t%d p%d x%d d%d next%d fish%d\n",seed,mode,t,p,a.h[p].x,hero_distance(&a,p),a.h[p].next_row,a.h[p].fish);return 1;}
   }
   for(int p=0;p<4;p++)CHECK(a.h[p].fish>20);
  }
  printf("seed %u: opening and cap, four players on both channels, every fish caught\n",seed);
 }
 CHECK(crocs>100&&staggered>100&&missing>100&&fork_hazards>100);
 CHECK(max_length-min_length>200&&max_width-min_width>15&&max_bank-min_bank>25&&max_center-min_center>25);
 printf("terrain variation: island lengths %d-%d, widths %d-%d, centres %d-%d, %d fork hazards\n",min_length,max_length,min_width*2,max_width*2,min_center,max_center,fork_hazards);
 // Fish are claimed independently; a single missed fish eliminates only that otter.
 world_init(&w,2,7);river_row r;course_row(&w,0,&r);
 w.scroll=r.distance-25;w.travel=(int64_t)w.scroll*Q16_ONE;
 for(int p=0;p<2;p++){w.h[p].x=r.fish;w.h[p].xx=r.fish*Q16_ONE;}
 memset(input,0,sizeof input);for(int i=0;i<25;i++)world_step(&w,input);
 CHECK(w.h[0].fish==1&&w.h[1].fish==1&&w.h[0].state==HS_RUN&&w.h[1].state==HS_RUN);
 world_init(&w,1,7);course_row(&w,0,&r);w.scroll=r.distance+13;w.travel=(int64_t)w.scroll*Q16_ONE;w.h[0].x=r.fish;w.h[0].xx=r.fish*Q16_ONE;w.h[0].collected=-1;
 w.h[0].xx=(r.fish+22)*Q16_ONE;world_step(&w,input);CHECK(w.h[0].state==HS_HIT&&w.events[0]==EV_HIT&&w.h[0].fish==0);
 // Touching a crocodile activates its surfacing/bite record at the exact drawn location.
 world_init(&w,1,4);int found=0;
 for(int id=0;id<80&&!found;id++){course_row(&w,id,&r);for(int k=0;k<r.count;k++)if(r.kind[k]==CROCODILE){
  w.scroll=r.at[k];w.travel=(int64_t)w.scroll*Q16_ONE;w.h[0].xx=r.x[k]*Q16_ONE;w.h[0].x=r.x[k];w.h[0].next_row=id+1;
  world_step(&w,input);CHECK(w.h[0].state==HS_HIT&&w.events[0]==EV_HIT&&w.bite[0].active&&w.bite[0].x==r.x[k]&&w.bite[0].distance==r.at[k]);
  world_step(&w,input);CHECK(w.bite[0].t==1);found=1;break;
 }}CHECK(found);
 // The land collision follows the same distance-to-screen conversion as all river objects.
 world_init(&w,4,9);w.scroll=0;int centre;
 while(river_island(&w,w.scroll,&centre)<22)w.scroll++;
 w.scroll+=50;w.travel=(int64_t)w.scroll*Q16_ONE;
 for(int p=0;p<4;p++){int center;river_island(&w,hero_distance(&w,p),&center);w.h[p].x=center;w.h[p].xx=center*Q16_ONE;}
 world_step(&w,input);for(int p=0;p<4;p++)CHECK(w.h[p].state==HS_HIT);
 puts("river rules: all passed");return 0;
}
