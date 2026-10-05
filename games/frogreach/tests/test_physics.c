#include "game.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static world run(int seed,int policy,int players) {
 world w;world_init(&w,players,(uint32_t)seed);int held[4]={1,1,1,1};world_step(&w,held);
 for(int t=0;t<3600 && !world_all_down(&w);t++) {
  for(int p=0;p<players;p++)held[p]=policy==2?bot_decide(&w,p):policy;
  world_step(&w,held);
 }
 return w;
}
int main(void) {
 world start;world_init(&start,1,4);int tap[4]={1,0,0,0},none[4]={0};
 int64_t before_x=start.h[0].wx;int32_t before_y=start.h[0].y;world_step(&start,tap);world_step(&start,tap);world_step(&start,none);
 assert(start.h[0].mode==MODE_WAIT && start.h[0].length==0 && start.h[0].wx==before_x && start.h[0].y==before_y);
 int widths=0;for(int id=2;id<30;id++){int x,width;perch_pos(&start,id,0,&x,&width);assert(width>=24 && width<=64);widths|=1<<((width-24)/8);}
 int sizes=0;for(int i=0;i<6;i++)sizes+=(widths>>i)&1;assert(sizes>=4);
 start.h[0].anchor=4;start.h[0].pad_offset=3;int origin,width;perch_pos(&start,4,start.t,&origin,&width);int moved=0;
 for(int t=0;t<80;t++){world_step(&start,none);int x;perch_pos(&start,4,start.t,&x,&width);assert(start.h[0].wx==(int64_t)(x+3)*Q16_ONE);if(x!=origin)moved=1;}
 assert(moved);
 for(int seed=1;seed<=12;seed++) {
  world idle=run(seed,0,1),hold=run(seed,1,1),skill=run(seed,2,1);
  printf("seed %d: short=%d hold=%d skill=%d pads=%d flies=%d\n",seed,idle.h[0].score,hold.h[0].score,skill.h[0].score,skill.h[0].anchor,skill.h[0].apples);
  assert(idle.h[0].state==HS_DOWN && hold.h[0].state==HS_DOWN);
  assert(skill.h[0].score>300 && skill.h[0].launches>20 && skill.h[0].apples>10);
  world again=run(seed,2,1);assert(!memcmp(&skill,&again,sizeof skill));
 }
 world w;world_init(&w,1,4);int held[4]={1,0,0,0};world_step(&w,held);
 while(w.h[0].length<260)world_step(&w,held);
 held[0]=0;world_step(&w,held);assert(!w.h[0].good);
 for(int t=0;t<100;t++)world_step(&w,held);
 assert(w.h[0].state==HS_DOWN); /* An overshoot must splash, even on a wide pad. */
 world four=run(4,2,4);for(int p=0;p<4;p++)assert(four.h[p].score>300);
 puts("physics: safe short start, varied pad sizes, platform carry, misses and seeded multiplayer play passed");return 0;
}
