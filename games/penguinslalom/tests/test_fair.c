/*
 * Penguin Slalom: the bears are fair (DESIGN.md "Polar bears"), proved on the exact steering.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/penguinslalom/LICENSE.
 *
 * For every bear the generator warns (seeded courses, 1 to 4 players, runs from the start and from far down the
 * course), and for every penguin: from BEAR_REACT frames after the marker shows, every "comfortable" state (x, its
 * lateral speed), one from which the course and the bears already warned can be followed never faster than 3 px per
 * frame sideways, keeps a path through the next gates and past the new bear at any speed, ending comfortable again
 * once the bear has passed every penguin. Steering is Left / Right / nothing held for 4 frames at a time (15
 * decisions a second), with CLEAR px of clearance from every pole, gate edge, obstacle and bear and BEAR_CLEAR more
 * from the new bear. The motion and the collisions are world_step's (the same integers, the swept ellipses, the
 * gates' frames), the course and the bears are the world's own frames; states are kept on a grid of 1 px and 1/4 px
 * per frame between decisions.
 * It also checks the in-game check (play.c bear_fair) against this proof on random spots (all it accepts must be
 * proved fair), and that the bears land across the whole course, more often on the racing line as the course ramps
 * up, warned 40+ frames ahead.   test_fair [--quick | --long]
 */
#include "game.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XS 273                      /* x 24..296 */
#define VS 33                       /* vx -4..4 px per frame, by 1/4 */
#define VQ 16384
#define BLOCK 4
#define MAXF 300
#define MAXB ((MAXF-BEAR_REACT)/BLOCK)
#define EASY_CLEAR 6                /* px of clearance a comfortable skier keeps from everything */
#define CLEAR 2                     /* px of clearance the dodge keeps from everything */
#define BEAR_CLEAR 8                /* ... and from the new bear on the dodge: room a human can use */

static world trace[MAXF+1];         /* the world frame by frame from the warning (no more bears spawn) */
static uint8_t hits[MAXF+1][XS][9]; /* [frame][x before][x after - x before + 4]: bit 0 the course and the bears already
                                       warned, bit 1 with the new bear too (CLEAR px wider, the new bear BEAR_CLEAR more) */
static uint8_t easy[MAXB+1][XS][VS],safe[MAXB+1][XS][VS];

static int show_trapped=1;           /* print each trapped state (not for the random spots) */
static void keep_running(world *w){for(int p=0;p<w->players;p++){w->h[p].state=HS_RUN;w->h[p].t=0;}}
/* one frame's hazards for a penguin `o` px from the camera, as world_step checks them (a: the frame before, b: this one) */
typedef struct frame_hazards {int d0,d1,n,rows,slot[8];slope_obstacle p[8],c[8];row r[3];} frame_hazards;
static void gather(const world *a,const world *b,int o,frame_hazards *h){
 h->d0=(int)(a->travel>>16)+o;h->d1=(int)(b->travel>>16)+o;h->n=h->rows=0;int base=course_index(b,h->d1);
 for(int id=base>0?base-1:0;id<=base+1;id++){
  slope_obstacle p,c;course_obstacle(b,id,a->scroll,b->t-1,&p);course_obstacle(b,id,b->scroll,b->t,&c);
  if(abs(c.distance-h->d1)<=c.ry+SKI_FORWARD_RADIUS+12){h->p[h->n]=p;h->c[h->n]=c;h->slot[h->n++]=-1;}
  course_row(b,id,&h->r[h->rows++]);
 }
 for(int i=0;i<MAX_BEARS;i++)if(b->bears[i].active && !b->bears[i].warn){
  slope_obstacle c;bear_obstacle(b,i,0,&c);if(abs(c.distance-h->d1)>c.ry+SKI_FORWARD_RADIUS+12)continue;
  h->c[h->n]=c;h->p[h->n]=c;h->p[h->n].distance=(int)(b->bears[i].previous>>16);h->slot[h->n++]=i;
 }
}
/* skip_slot: a bear left out; wide_slot: a bear given BEAR_CLEAR px more */
static int hit_at(const frame_hazards *h,int x0,int x1,int grow,int skip_slot,int wide_slot){
 for(int i=0;i<h->n;i++)if(h->slot[i]<0 || h->slot[i]!=skip_slot){
  int g=grow+(h->slot[i]>=0 && h->slot[i]==wide_slot?BEAR_CLEAR:0);
  slope_obstacle p=h->p[i],c=h->c[i];p.rx+=g;c.rx+=g;if(obstacle_hits(&p,&c,x0,x1,h->d0,h->d1))return 1;
 }
 for(int i=0;i<h->rows;i++){
  const row *r=&h->r[i];int dy=r->distance-h->d1,reach=r->rx+SKI_RADIUS+grow;
  if(abs(dy)<=r->ry+SKI_RADIUS && (abs(x1-r->left)<reach || abs(x1-r->right)<reach))return 1;
  if(h->d0<r->distance && h->d1>=r->distance && (x1<=r->left+reach || x1>=r->right-reach))return 1;
 }
 return 0;
}
/* one frame of world_step's steering (the same integers) */
static void steer(int32_t *xx,int32_t *vx,int dir){
 *vx=dir?*vx+dir*39322:*vx*4/5;if(*vx>262144)*vx=262144;if(*vx<-262144)*vx=-262144;
 *xx+=*vx;if(*xx<24*Q16_ONE){*xx=24*Q16_ONE;*vx=0;}if(*xx>296*Q16_ONE){*xx=296*Q16_ONE;*vx=0;}
}
/* backward over blocks b = blocks-1..0 (frames BEAR_REACT+4b+1..+4): the viable states, never faster than `top`
 * (Q16 px per frame) on any frame; the last block's end states are `last` (every state without it). bit: which
 * hazards */
static void viable(uint8_t (*out)[XS][VS],int blocks,int bit,int32_t top,uint8_t (*last)[XS][VS]){
 for(int x=0;x<XS;x++)for(int v=0;v<VS;v++)out[blocks][x][v]=(uint8_t)((last?last[blocks][x][v]:1) && abs(v-VS/2)*VQ<=top);
 for(int b=blocks-1;b>=0;b--)for(int x=0;x<XS;x++)for(int v=0;v<VS;v++){
  int ok=0;
  if(abs(v-VS/2)*VQ<=top)for(int dir=-1;dir<=1 && !ok;dir++){
   int32_t xx=(24+x)*Q16_ONE+Q16_ONE/2,vx=(v-VS/2)*VQ;int dead=0;
   for(int k=1;k<=BLOCK && !dead;k++){
    int f=BEAR_REACT+b*BLOCK+k,x0=xx>>16;steer(&xx,&vx,dir);
    dead=(hits[f][x0-24][(xx>>16)-x0+4]>>bit&1) || vx>top || vx<-top;
   }
   if(dead)continue;
   int nv=(vx+(vx>=0?VQ/2:-VQ/2))/VQ+VS/2;if(nv<0)nv=0;if(nv>VS-1)nv=VS-1;
   ok=out[b+1][(xx>>16)-24][nv];
  }
  out[b][x][v]=(uint8_t)ok;
 }
}
/* the proof for the bear in `slot` of w (just warned): the number of comfortable states without a dodge, over all
 * penguins; *checked: the comfortable states */
static long prove(const world *w,int slot,long *checked){
 const sliding_bear *bear=&w->bears[slot];
 int lowest=player_offset(w->players,w->players-1);
 int pass=bear->warn+(lowest-bear->land+24+SKI_FORWARD_RADIUS+CLEAR+BEAR_SLIDE-1)/BEAR_SLIDE+1;
 int dodge=(pass-BEAR_REACT+BLOCK-1)/BLOCK,blocks=dodge+FAIR_AFTER/BLOCK;
 assert(BEAR_REACT+blocks*BLOCK<=MAXF);
 trace[0]=*w;trace[0].bear_wait=1<<30;
 for(int f=1;f<=BEAR_REACT+blocks*BLOCK;f++){trace[f]=trace[f-1];keep_running(&trace[f]);int idle[4]={0};world_step(&trace[f],idle);}
 long bad=0;
 for(int p=0;p<w->players;p++){
  int o=player_offset(w->players,p);
  for(int f=BEAR_REACT+1;f<=BEAR_REACT+blocks*BLOCK;f++){
   frame_hazards fh;gather(&trace[f-1],&trace[f],o,&fh);
   for(int x0=24;x0<=296;x0++)for(int dx=-4;dx<=4;dx++){
    int x1=x0+dx;if(x1<24 || x1>296){hits[f][x0-24][dx+4]=3;continue;}
    hits[f][x0-24][dx+4]=(uint8_t)(hit_at(&fh,x0,x1,EASY_CLEAR,slot,-1)|hit_at(&fh,x0,x1,CLEAR,-1,slot)<<1);
   }
  }
  viable(easy,blocks,0,262144*3/4,NULL);
  viable(safe,dodge,1,262144,easy);
  for(int x=0;x<XS;x++)for(int v=0;v<VS;v++)if(easy[0][x][v]){
   (*checked)++;
   if(!safe[0][x][v]){bad++;if(show_trapped)printf("  trapped: player %d at x %d, %d/4 px per frame\n",p+1,x+24,v-VS/2);}
  }
 }
 return bad;
}
/* a world at `distance` down the slope, running, its first bear due at once (distance < 0: a run from its start) */
static void start_at(world *w,int players,uint32_t seed,int distance){
 world_init(w,players,seed);int go[4]={2,2,2,2};world_step(w,go);
 if(distance>=0){w->travel=(int64_t)distance*Q16_ONE;w->scroll=distance;w->bear_wait=1;}
}
static int line_at(const world *w,int d){
 int id=course_index(w,d);row a,b;course_row(w,id,&a);course_row(w,id+1,&b);
 return d<=a.distance?a.center:a.center+(b.center-a.center)*(d-a.distance)/(b.distance-a.distance);
}
int main(int argc,char **argv){
 int quick=argc>1 && !strcmp(argv[1],"--quick"),more=argc>1 && !strcmp(argv[1],"--long")?5:1;setvbuf(stdout,NULL,_IONBF,0);
 static const int distances[]={-1,9000,18000,30000};
 long bears=0,checked=0,bad=0;int quarters[4]={0},aimed_early=0,early=0,aimed_late=0,late=0,shortest=1000;long off_early=0,off_late=0;
 for(int players=1;players<=4;players++)for(unsigned seed=1;seed<=(quick?1u:players==1?6u*more:2u*more);seed++)for(int k=0;k<4;k++){
  world w;start_at(&w,players,seed*7919u,distances[k]);
  for(int t=0;t<(quick?700:1400);t++){
   keep_running(&w);int idle[4]={0};world_step(&w,idle);if(!w.bear_warned)continue;
   int slot=-1;for(int i=0;i<MAX_BEARS;i++)if(w.bears[i].active && w.bears[i].warn==w.bears[i].warn_len && w.bears[i].x==w.bear_warned)slot=i;
   assert(slot>=0);const sliding_bear *b=&w.bears[slot];
   assert(b->warn>=BEAR_WARN_MIN && b->warn<=BEAR_WARN_EARLY && b->x>=BEAR_X_MIN && b->x<=BEAR_X_MAX);
   if(b->warn<shortest)shortest=b->warn;
   long c=0,n=prove(&w,slot,&c);bears++;checked+=c;bad+=n;
   if(n)printf("UNFAIR: %d players, seed %u, distance %d, x %d, warn %d: %ld of %ld comfortable states trapped\n",players,seed,w.scroll,b->x,b->warn,n,c);
   quarters[(b->x-24)*4/273]++;
   int pass=w.scroll+(int)((int64_t)course_speed(w.scroll)*(b->warn-b->land/BEAR_SLIDE)>>16),off=abs(b->x-line_at(&w,pass)),near=off<=24;
   if(w.scroll<9000){early++;aimed_early+=near;off_early+=off;}
   if(w.scroll>=BEAR_RAMP){late++;aimed_late+=near;off_late+=off;}
  }
 }
 printf("%ld bears, %ld comfortable states checked, %ld trapped; landing spots by quarter of the course %d %d %d %d; "
  "within 24 px of the racing line %d/%d early (%ld px off on average), %d/%d late (%ld px); shortest warning %d frames\n",
  bears,checked,bad,quarters[0],quarters[1],quarters[2],quarters[3],aimed_early,early,early?off_early/early:0,aimed_late,late,
  late?off_late/late:0,shortest);
 assert(!bad && bears>(quick?8:60) && checked>0);
 for(int q=0;q<4;q++)assert(quarters[q]>0);
 /* the same frames early and late: more bears late, nearer the line */
 assert(late>early && off_late*early<off_early*late);
 /* the in-game check never accepts a spot the proof rejects (random spots, times and player counts) */
 int accepted=0,rejected=0,both=0,wrong=0;show_trapped=0;
 for(int n=0;n<(quick?20:120*more);n++){
  uint32_t r=(uint32_t)n*2654435761u^0x5eedu;int players=1+(int)(r%4);
  world w;start_at(&w,players,r>>8,(int)((r>>4)%32000));w.bear_wait=1<<30;
  for(int t=(int)(r>>20)%40;t>0;t--){keep_running(&w);int idle[4]={0};world_step(&w,idle);}
  int x=BEAR_X_MIN+(int)((r>>12)%(BEAR_X_MAX-BEAR_X_MIN+1)),warn=bear_warn_time(w.scroll),land=bear_land(players);
  int fair=bear_fair(&w,x,warn,land),slot=0;
  w.bears[slot]=(sliding_bear){1,999,x,warn,warn,land,0,w.travel+(int64_t)land*Q16_ONE,w.travel+(int64_t)land*Q16_ONE};
  w.bears[1].active=0;long c=0,bad_states=prove(&w,slot,&c);
  if(fair){accepted++;if(bad_states){wrong++;printf("ACCEPTED UNFAIR (spot %d): %d players, distance %d, x %d: %ld trapped\n",n,players,w.scroll,x,bad_states);}}
  else{rejected++;both+=bad_states>0;}
 }
 printf("random spots: %d accepted (%d of them unfair), %d rejected (%d of them proved unfair)\n",accepted,wrong,rejected,both);
 assert(accepted>0 && !wrong);
 puts("fair bears: every comfortable state keeps a dodge on the exact steering, spots across the course, warned 40+ frames ahead");
 return 0;
}
