#include "game.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
/* The rules (play.c) and the course buffer, with the game's own test bot (plan.c: Up/Down from what a 4:3 screen
 * shows, the same interface as a player; no world overrides). */
static const item*first_of(const world*w,int kind){for(int i=0;i<w->nitems;i++)if(w->it[i].kind==kind)return &w->it[i];return NULL;}
int main(void){
 world fruit;world_init(&fruit,2,9);int start_fruit[4]={CMD_TOGGLE,0,0,0},no_fruit_input[4]={0};world_step(&fruit,start_fruit);
 const item*ff=first_of(&fruit,IT_FRUIT);assert(ff);item first_fruit=*ff;int bx=first_fruit.x;
 fruit.h[1].x=fruit.h[0].x;
 fruit.h[0].side=fruit.h[0].target=1-first_fruit.side;
 fruit.h[1].side=fruit.h[1].target=first_fruit.side;
 fruit.scroll=bx-15-fruit.h[0].x;fruit.travel=(int64_t)fruit.scroll*Q16_ONE;
 /* (nothing else in reach: the first fruit's own segment may hold a hazard; keep only the fruit) */
 fruit.it[0]=first_fruit;fruit.nitems=1;fruit.dir.x=1<<30;
 int claims=0;
 for(int t=0;t<30&&fruit.h[0].state==HS_RUN;t++){
  world_step(&fruit,no_fruit_input);
  if(fruit.events[1]&EV_SCORE)claims++;
 }
 assert(fruit.h[0].state==HS_HIT&&fruit.events[0]==EV_HIT&&fruit.h[0].berry==-1);
 assert(fruit.h[1].state==HS_RUN&&fruit.h[1].berry==first_fruit.id&&claims==1);
 printf("One missed fruit is immediately fatal; overlapping multiplayer claims remain independent and award once\n");
 world directional;world_init(&directional,4,9);int start_cmd[4]={1,0,0,0};world_step(&directional,start_cmd);
 int down[4]={CMD_DOWN,CMD_UP,CMD_DOWN,CMD_UP},up[4]={CMD_UP,CMD_DOWN,CMD_UP,CMD_DOWN};
 for(int t=0;t<48;t++)world_step(&directional,down);
 assert(directional.h[0].side==1&&!directional.h[0].flip);
 assert(directional.h[1].side==0&&!directional.h[1].flip);
 assert(directional.h[2].side==1&&directional.h[3].side==0);
 for(int t=0;t<48;t++)world_step(&directional,up);
 assert(directional.h[0].side==0&&!directional.h[0].flip);
 assert(directional.h[1].side==1&&!directional.h[1].flip);
 assert(directional.h[2].side==0&&directional.h[3].side==1);
 printf("Up/Down target a side, held input never toggles, four players independent\n");
 world queued;world_init(&queued,1,9);world_step(&queued,start_cmd);
 int to_bottom[4]={CMD_DOWN,0,0,0},to_top[4]={CMD_UP,0,0,0},released[4]={0};
 world_step(&queued,to_bottom);assert(queued.h[0].flip);
 world_step(&queued,to_top);
 for(int t=0;t<46;t++)world_step(&queued,released);
 assert(queued.h[0].side==0&&!queued.h[0].flip);
 printf("short directional tap during a curl queues the requested surface\n");
 for(int seed=1;seed<=16;seed++){
  world w,b;world_init(&w,4,seed);b=w;
  int none[4]={0};for(int t=0;t<120;t++)world_step(&w,none);assert(memcmp(&w,&b,sizeof w)==0);
  int start[4]={1,0,0,0};world_step(&w,start);assert(w.scroll==0&&w.h[0].side==0&&w.started);
  b=w;
  for(int t=0;t<18000;t++){
   int p[4];for(int j=0;j<4;j++)p[j]=bot_decide(&w,j);world_step(&w,p);world_step(&b,p);
   assert(!memcmp(&w,&b,sizeof w));
   for(int j=0;j<4;j++)assert(w.h[j].state==HS_RUN);
   /* the course buffer: written FILL_AHEAD past the scroll, in x order, never full */
   assert(w.dir.x>=w.scroll+FILL_AHEAD&&w.nitems<ITEM_MAX);
   for(int i=1;i<w.nitems;i++)assert(w.it[i].x>=w.it[i-1].x);
  }
  assert(w.h[0].score>1500);assert(course_speed(w.scroll)<=249036);
  printf("seed %d: four snails (the bot) survived %d pixels, score %d\n",seed,w.scroll,w.h[0].score);
 }
 /* a caterpillar: its patrol is slow, continuous, both ways, 18 px either side of its anchor */
 director d;director_start(&d,12);item seg[16],cat;int found=0;
 for(int k=0;k<2000&&!found;k++){int n=director_segment(&d,seg,16);
  for(int i=0;i<n;i++)if(seg[i].kind==IT_CATER){cat=seg[i];found=1;break;}}
 assert(found);obstacle first,previous;item_at(&cat,0,&first);previous=first;
 int lo=first.x,hi=first.x,forward=0,backward=0;
 for(int t=1;t<=PATROL_T;t++){obstacle o;item_at(&cat,t,&o);
  assert(o.anchor==first.anchor&&o.side==first.side);assert(o.x-previous.x<=1&&o.x-previous.x>=-1);
  forward|=o.x>previous.x;backward|=o.x<previous.x;if(o.x<lo)lo=o.x;if(o.x>hi)hi=o.x;previous=o;}
 assert(forward&&backward&&hi-lo==2*PATROL&&previous.x==first.x);
 printf("caterpillar patrol continuous, both directions, coherent world attachment\n");
 world w;world_init(&w,1,1);int a[4]={1,0,0,0},z[4]={0};world_step(&w,a);
 world_step(&w,a);int old=w.h[0].y>>16;assert(w.h[0].flip==21);
 for(int t=0;t<21;t++){world_step(&w,z);int yy=w.h[0].y>>16;assert(yy-old<7 && yy-old>-7);old=yy;}
 assert(w.h[0].side==1&&!w.h[0].flip);
 world_init(&w,1,1);world_step(&w,a);
 for(int t=0;t<3000&&w.h[0].state==HS_RUN;t++)world_step(&w,z);
 assert(w.h[0].state==HS_HIT);assert(w.events[0]==EV_HIT);
 printf("frozen title, continuous side change, collision and silent failure passed\n");
 return 0;
}
