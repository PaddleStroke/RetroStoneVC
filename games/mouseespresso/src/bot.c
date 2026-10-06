/*
 * Mouse Espresso: the test bot (--opt bot=N), a first-run player made of code. It plays from the SCREEN only:
 *   - its eyes: the playfield picture the game draws (paint.c, the same code that draws the screen), only the
 *     320-px view a 4:3 screen shows (so it plays the same at every width), now and as it looked over the last
 *     MEMORY frames (it remembers where a jet fired or the ladle swung, and sees a jet's rising wisps); it never
 *     reads the course's list, the director or the seed's future;
 *   - its own mouse: the sprite on screen (OAM), plus the feel of the steam (it knows when it holds A);
 *   - the plan: a search over "hold / release" (each at least BOT_DWELL frames) through what it sees, with a
 *     safety margin and a few frames of slack it relaxes only when it must;
 *   - its hand: a decided change of input happens on each frame with chance BOT_ACT / 100, never more than
 *     BOT_LATE frames late (a human's spread: two frames late on average), from a hash of the run's frame.
 * It is a measure of the course (DESIGN.md "The bot"): a decent first run, not a champion.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/mouseespresso/LICENSE.
 */
#include "game.h"
#include "paint.h"
#include "house_ui.h"
#include <string.h>

#define VIEW_W 320              /* the view it reads: what a 320-px screen shows, from the camera */
#define BOT_H 96                /* frames looked ahead */
#define BOT_DWELL 4
#define BOT_ACT 33              /* % chance per frame that a decided change happens now */
#define BOT_LATE 8              /* frames: the hand is never later than this */
#define MEMORY 64               /* frames of remembered motion */
#define WORDS 8                 /* 240 rows of bits */
static const int MARGINS[3]={6,3,1};   /* px around its mouse */
/* its caution, relaxed step by step when it must: (margin, frames it wants to be able to wait before acting) */
static const int LEVELS[4][2]={{0,8},{1,5},{2,2},{2,0}};
static int lock;                     /* the search may not change the input before this frame */

typedef struct bot_t { int32_t y,vy; int held,dwell,synced,wait; } bot_t;
static bot_t B[MAX_PLAYERS];
/* scratch (state_audit.txt): rebuilt from the picture every frame */
static uint8_t eye[VIEW_W*240];
static uint32_t solid[VIEW_W][WORDS],tall[VIEW_W][WORDS],blk[3][VIEW_W][WORDS];
static int seen_t=-1,seen_left,blk_ok[3];static uint32_t seen_seed;
static uint16_t memo[BOT_H+1][120][32][2],stamp;
static int view_left,plan_x[BOT_H+2];

void bot_state(void){rs_state_var("bot.B",B,sizeof B);}

static void look(const world *w){
 if(seen_t==w->t && seen_left==w->scroll && seen_seed==w->seed)return;
 seen_t=w->t;seen_left=w->scroll;seen_seed=w->seed;view_left=w->scroll;
 memset(eye,0,sizeof eye);canvas c={eye,VIEW_W,240,VIEW_W};
 for(int back=MEMORY;back>=0;back-=8)paint_course(&c,&w->c,view_left,w->t-back);
 memset(solid,0,sizeof solid);
 for(int y=0;y<COUNTER_Y;y++)for(int x=0;x<VIEW_W;x++){int v=eye[y*VIEW_W+x];if(v && v!=14)solid[x][y>>5]|=1u<<(y&31);}
 blk_ok[0]=blk_ok[1]=blk_ok[2]=0;
}
static void shift_or(uint32_t *d,const uint32_t *s,int up){   /* d |= s shifted by one row (up: towards row 0) */
 for(int i=0;i<WORDS;i++){
  uint32_t v=up?(s[i]>>1)|(i+1<WORDS?s[i+1]<<31:0):(s[i]<<1)|(i?s[i-1]>>31:0);d[i]|=v;
 }
}
/* the rows where the mouse's centre would touch something seen, with a margin of m px */
static void dilate(int mi){
 if(blk_ok[mi])return;
 int m=MARGINS[mi],ry=HERO_RADIUS_Y+m,rx=HERO_RADIUS_X+m;
 for(int x=0;x<VIEW_W;x++){
  uint32_t a[WORDS],b[WORDS];memcpy(a,solid[x],sizeof a);
  for(int k=0;k<ry;k++){memcpy(b,a,sizeof b);shift_or(a,b,1);shift_or(a,b,0);}
  memcpy(tall[x],a,sizeof a);
 }
 for(int x=0;x<VIEW_W;x++){
  uint32_t *d=blk[mi][x];memset(d,0,sizeof blk[mi][x]);
  for(int dx=-rx;dx<=rx;dx++){int c=x+dx;if(c<0 || c>=VIEW_W)continue;for(int i=0;i<WORDS;i++)d[i]|=tall[c][i];}
 }
 blk_ok[mi]=1;
}
static int blocked(int mi,int x,int y){
 int c=x-view_left;if(c<0 || c>=VIEW_W)return 0;
 if(y<0 || y>=240)return 1;
 return (int)(blk[mi][c][y>>5]>>(y&31)&1);
}
typedef struct pst { int32_t y,vy; int held,dwell; } pst;
static int search(int mi,const pst *s,int f){
 if(f>=BOT_H || plan_x[f]+HERO_RADIUS_X>=view_left+VIEW_W)return 1;   /* the end of what it can see */
 int yb=(s->y>>16)/2,vb=(s->vy>>14)+16;if(yb<0)yb=0;
 if(yb>119)yb=119;
 if(vb<0)vb=0;
 if(vb>31)vb=31;
 uint16_t *z=&memo[f][yb][vb][s->held];
 if(*z==stamp)return 0;
 for(int c=0;c<2;c++){
  if(c && (s->dwell<BOT_DWELL || f<lock))break;
  pst n=*s;int held=c?!s->held:s->held,g;
  if(held!=n.held){n.held=held;n.dwell=1;}else if(n.dwell<BOT_DWELL)n.dwell++;
  hero_fly(&n.y,&n.vy,held,&g);
  if(blocked(mi,plan_x[f+1],n.y>>16))continue;
  if(search(mi,&n,f+1))return 1;
 }
 *z=stamp;return 0;
}
static int try_first(int mi,const bot_t *b,int change,int late){
 lock=change?0:late;
 if(change && b->dwell<BOT_DWELL)return 0;
 if(!++stamp){memset(memo,0,sizeof memo);stamp=1;}
 pst s={b->y,b->vy,b->held,b->dwell},n=s;int held=change?!s.held:s.held,g;
 if(held!=n.held){n.held=held;n.dwell=1;}else if(n.dwell<BOT_DWELL)n.dwell++;
 hero_fly(&n.y,&n.vy,held,&g);
 if(blocked(mi,plan_x[1],n.y>>16))return 0;
 return search(mi,&n,1);
}
static uint32_t hand(const world *w,int p){
 uint32_t v=w->seed*0x85ebca6bu+(uint32_t)p*0xc2b2ae35u;v^=v>>16;v*=0x7feb352du;v^=v>>15;
 v+=(uint32_t)w->t*0x9e3779b9u;
 v^=v>>16;v*=0x7feb352du;v^=v>>15;v*=0x846ca68bu;return v^(v>>16);
}
/* its mouse on the screen: the 32x32 sprite in its colours (draw.c), the centre 16 px down */
static int sprite_y(int p,int *y){
 int pal=p?hu_player_pal(p):0;
 for(int i=0;i<rs_oam_next();i++){const rs_sprite *s=rs_oam(i);if(s->w==32 && s->h==32 && s->pal==pal){*y=s->y+16;return 1;}}
 return 0;
}
int bot_decide(const world *w,int p){
 if(!w->started)return 1;
 const hero *h=&w->h[p];bot_t *b=&B[p];
 if(h->state!=HS_RUN){b->synced=0;return 0;}
 int sy;
 if(!b->synced){memset(b,0,sizeof *b);b->y=FLIGHT_BOTTOM*Q16_ONE;b->dwell=BOT_DWELL;b->synced=1;}
 if(sprite_y(p,&sy) && (b->y>>16)!=sy && (b->y>>16)+1!=sy && (b->y>>16)-1!=sy){b->y=sy*Q16_ONE;b->vy=0;}
 look(w);
 /* where its mouse will be, frame by frame: the screen scrolls at the speed it sees */
 int64_t x=(int64_t)(hero_start_x(w->players,p)+w->scroll)*Q16_ONE+(w->travel&0xffff);
 for(int f=0;f<=BOT_H+1;f++){plan_x[f]=(int)(x>>16);x+=course_speed(w->scroll);}
 int want=b->held;
 for(int l=0;l<4;l++){int mi=LEVELS[l][0];
  dilate(mi);
  if(try_first(mi,b,0,LEVELS[l][1])){want=b->held;break;}
  if(try_first(mi,b,1,0)){want=!b->held;break;}
 }
 /* not quite yet: the hand is a little late (never more than BOT_LATE frames) */
 if(want!=b->held && hand(w,p)%100>=BOT_ACT && b->wait<BOT_LATE){b->wait++;want=b->held;}else b->wait=0;
 if(want!=b->held){b->held=want;b->dwell=1;}else if(b->dwell<BOT_DWELL)b->dwell++;
 int g;hero_fly(&b->y,&b->vy,want,&g);
 return want;
}
