/*
 * Snail Switch: the planner. A snail's whole state, for planning, is its surface, its curl's frame and whether it has
 * caught the fruit it is passing: 88 states (plan.h). plan_next() steps one of them through the game's own rules
 * (hero_step), so the test bot below and the tests' solver (tests/solver.c) play exactly the game.
 * The bot (--opt bot=N) sees what a 4:3 screen shows: the things on the vine up to the screen's right edge (x 320),
 * never the director nor what lies beyond; it plans PLAN_H frames ahead (optimistic past the edge) and presses a few
 * frames before the last moment that still works.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/snailswitch/LICENSE.
 */
#include "plan.h"
#include <string.h>

int plan_state(int side,int flip,int k){return side+2*flip+2*FLIP_FRAMES*k;}
int plan_flip(int s){return (s>>1)%FLIP_FRAMES;}
int plan_side(int s){return s&1;}

/* the id of the last fruit a snail at world x must have caught (passed: x > fruit + 15) */
int32_t plan_low(const item*it,int n,int32_t wx,int32_t next_id){
 for(int i=0;i<n;i++)if(it[i].kind==IT_FRUIT&&it[i].x+FRUIT_HALF>=wx)return it[i].id-1;
 return next_id-1;
}

int plan_next(const plan*p,int f,int s,int press){
 hero h;memset(&h,0,sizeof h);
 int side=plan_side(s),flip=plan_flip(s);
 if(press&&flip)return -1;
 h.state=HS_RUN;h.x=p->hx;h.side=h.target=side;h.flip=flip;h.from=flip?1-side:side;
 h.berry=p->low[f-1]+(s>=PLAN_K);
 course_view c={p->it+p->first[f],p->n-p->first[f],p->t[f],p->scroll[f]};
 hero_step(&h,press?(side?CMD_UP:CMD_DOWN):0,&c);
 if(h.state!=HS_RUN)return -1;
 int k=h.berry-p->low[f];
 if(k<0||k>1)return -1;
 return plan_state(h.side,h.flip,k);
}

/* ---- the bot ------------------------------------------------------------------------------------------------------- */
#define PLAN_H 60               /* frames looked ahead (the 4:3 screen shows 204 px ahead: 54..85 frames) */
#define BOT_SLACK 4             /* it presses at least this many frames before the last moment that works ... */
#define BOT_LEAD  20            /* ... or once holding its side would stop working within this many frames */
int bot_decide(const world*w,int pn){
 const hero*me=&w->h[pn];
 if(!w->started)return CMD_TOGGLE;
 if(me->flip)return 0;
 /* what the screen shows: the things up to its right edge (the 4:3 screen's, at every width) */
 item vis[ITEM_MAX];int n=0;
 for(int i=0;i<w->nitems&&w->it[i].x<w->scroll+RS_SCREEN_W+8;i++)vis[n++]=w->it[i];
 int32_t scroll[PLAN_H+1],low[PLAN_H+1];int t[PLAN_H+1],first[PLAN_H+1];
 int64_t travel=w->travel;scroll[0]=w->scroll;t[0]=w->t;
 for(int f=0;f<=PLAN_H;f++){
  if(f){t[f]=t[f-1]+1;travel+=course_speed(scroll[f-1]);scroll[f]=(int32_t)(travel>>16);}
  int32_t wx=scroll[f]+me->x;int i=f?first[f-1]:0;
  while(i<n&&vis[i].x<wx-64)i++;
  first[f]=i;low[f]=plan_low(w->it,w->nitems,wx,w->dir.fruits);
 }
 plan p={vis,n,me->x,PLAN_H,scroll,t,low,first};
 int k=me->berry-low[0];
 if(k<0||k>1)return 0;
 /* the states it can reach (and their moves), then alive[f]: those from which it lasts to the horizon (past the
  * screen's edge: nothing, optimistic) */
 int8_t move[PLAN_H][PLAN_STATES][2];uint8_t alive[PLAN_H+1][PLAN_STATES];
 int c=plan_state(me->side,0,k);
 memset(alive,0,sizeof alive);alive[0][c]=1;
 for(int f=0;f<PLAN_H;f++)for(int s=0;s<PLAN_STATES;s++){
  move[f][s][0]=move[f][s][1]=-1;
  if(!alive[f][s])continue;
  for(int press=0;press<=(plan_flip(s)?0:1);press++){
   int a=plan_next(&p,f+1,s,press);move[f][s][press]=(int8_t)a;if(a>=0)alive[f+1][a]=1;
  }
 }
 for(int f=PLAN_H-1;f>=0;f--)for(int s=0;s<PLAN_STATES;s++)if(alive[f][s]){
  int a=move[f][s][0],b=move[f][s][1];
  alive[f][s]=(a>=0&&alive[f+1][a])||(b>=0&&alive[f+1][b]);
 }
 /* holding its side: when would that stop working, and which presses work before it */
 int lo=-1,hi=-1,prev=0;
 for(int f=1;f<=PLAN_H;f++){
  int b=move[f-1][c][1],works=b>=0&&alive[f][b];
  if(works){if(!prev)lo=f;hi=f;}
  prev=works;
  int a=move[f-1][c][0];
  if(a<0||!alive[f][a]){
   if(hi>=1&&lo==1&&(hi<=1+BOT_SLACK||f<=1+BOT_LEAD))return me->side?CMD_UP:CMD_DOWN;
   return 0;
  }
  c=a;
 }
 return 0;
}
