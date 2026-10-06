/*
 * Mouse Espresso: the input-search solver (tests only).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/mouseespresso/LICENSE.
 *
 * The input model is a player's: A is held or released, and each choice lasts at least SV_MIN_DWELL frames.
 * alive(): a depth-first search over "keep / change" on every frame (keep first), on the game's own flight and
 * course. Its memo remembers dead states on a fine grid (1/4 px, 1/8 px per frame): a pruning that can only make the
 * solver more pessimistic. Every path it finds is flown exactly, so a passable stream is proven; a window it
 * reports can only be narrower than the true one.
 * sv_careful(): the careful player. Keep the input while keeping it does not crash within the horizon; when it
 * would, the frames on which changing it still leads on alive form runs (the windows): take the latest comfortable
 * run (>= SV_COMFORT frames; else the widest) and change in its middle. Its windows, changes and heights are what
 * the validator reports and what the difficulty score is made of.
 */
#include "solver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEMO_BITS 22
#define MEMO_N (1u<<MEMO_BITS)
#define HORIZON 150             /* frames looked ahead for a crash of the kept input */
#define BEYOND 110              /* frames a change must survive past that crash */

typedef struct sst { int32_t y,vy; int held,dwell; } sst;
typedef struct slot { int32_t f,qy,qvy; uint8_t held,dwell; uint32_t gen; int32_t dead_goal; } slot;
static slot *memo;
static uint32_t gen=1,used;
static int64_t trav[SV_MAX_FRAMES+2];
static int slot_x;
static course C;
static uint64_t nodes;
static long budget;
long sv_exhausted;
#define BUDGET 100000           /* nodes per question "does this change still work?" */

int64_t sv_travel(int t){return trav[t<0?0:t>SV_MAX_FRAMES?SV_MAX_FRAMES:t];}
const course *sv_course(void){return &C;}
void sv_init(void){
 if(!memo)memo=calloc(MEMO_N,sizeof *memo);
 if(!memo){fprintf(stderr,"solver: out of memory\n");exit(2);}
 trav[0]=0;for(int t=1;t<=SV_MAX_FRAMES+1;t++)trav[t]=trav[t-1]+course_speed((int)(trav[t-1]>>16));
}
static void reset(void){gen++;used=0;}
static slot *lookup(const sst *s,int32_t f,int create){
 int32_t qy=s->y>>14,qvy=s->vy>>13;uint32_t h=2166136261u;
 h=(h^(uint32_t)f)*16777619u;h=(h^(uint32_t)qy)*16777619u;h=(h^(uint32_t)qvy)*16777619u;
 h=(h^(uint32_t)(s->held|s->dwell<<1))*16777619u;
 for(uint32_t i=h&(MEMO_N-1);;i=(i+1)&(MEMO_N-1)){
  slot *z=&memo[i];
  if(z->gen!=gen){
   if(!create)return NULL;
   if(used>MEMO_N/4*3){reset();return lookup(s,f,create);}
   z->gen=gen;z->f=f;z->qy=qy;z->qvy=qvy;z->held=(uint8_t)s->held;z->dwell=(uint8_t)s->dwell;
   z->dead_goal=0x7fffffff;used++;return z;
  }
  if(z->f==f && z->qy==qy && z->qvy==qvy && z->held==s->held && z->dwell==s->dwell)return z;
 }
}
/* step frame f -> f + 1 with input `held`: 0 = crashed */
static int step(sst *s,int32_t f,int held){
 int32_t oy=s->y;int g;
 if(held!=s->held){s->held=held;s->dwell=1;}else if(s->dwell<SV_MIN_DWELL)s->dwell++;
 hero_fly(&s->y,&s->vy,held,&g);nodes++;
 int x0=slot_x+(int)(trav[f]>>16),x1=slot_x+(int)(trav[f+1]>>16);
 return !course_hits(&C,f+1,x0,x1,oy>>16,s->y>>16);
}
static int alive(const sst *s,int32_t f,int32_t goal){
 if(f>=goal)return 1;
 if(budget<=0)return 0;            /* out of budget: "no" (only ever more pessimistic) */
 budget--;
 slot *z=lookup(s,f,0);
 if(z && z->dead_goal<=goal)return 0;
 for(int c=0;c<2;c++){
  if(c && s->dwell<SV_MIN_DWELL)break;
  sst n=*s;
  if(step(&n,f,c?!s->held:s->held) && alive(&n,f+1,goal))return 1;
 }
 if(budget>0){z=lookup(s,f,1);if(z->dead_goal>goal)z->dead_goal=goal;}   /* proven dead (not just out of budget) */
 return 0;
}
/* every pattern of the run, with its items counted (the ring keeps only the recent ones) */
static sv_pattern all[SV_MAX_PATTERNS];
static int nall;
static uint32_t seen_n;
const sv_pattern *sv_patterns(int *n){*n=nall;return all;}
/* the game's own window of the course around frame f (GEN_AHEAD covers the search's horizon) */
static void keep_course(int32_t f){
 int32_t x=(int32_t)(trav[f]>>16);
 course_fill(&C,x+GEN_AHEAD);course_prune(&C,x-KEEP_BEHIND-720);   /* + the careful player's look back */
 if(C.gen_x<x+GEN_AHEAD){fprintf(stderr,"solver: the item ring is full at x %d\n",(int)x);exit(2);}
 while((uint32_t)nall<C.npat && nall<SV_MAX_PATTERNS){all[nall].p=*course_pattern(&C,(uint32_t)nall);nall++;}
 for(;seen_n<C.n;seen_n++){
  const item *it=course_item(&C,seen_n);
  int idx=nall-1;while(idx>=0 && idx%PATTERN_N!=it->pat)idx--;
  if(idx<0)continue;
  sv_pattern *p=&all[idx];p->items++;
  if(it->kind==IT_BISCUIT)p->biscuits++;
  else if(it->kind>=IT_JET || (it->kind==IT_FILTER && it->period))p->dynamic++;
 }
}
/* when the depth-first search runs out of budget: a breadth-first sweep, frame by frame, keeping one exact state per
 * cell of 1 px x 1/4 px per frame (x input x hold time). A state that reaches the goal is an exact path. */
#define WIDE_KEYS (256*64*8)
static sst wide_a[WIDE_KEYS],wide_b[WIDE_KEYS];
static uint32_t wide_mark[WIDE_KEYS],wide_stamp;
static int alive_wide(const sst *s0,int32_t f0,int32_t goal){
 sst *cur=wide_a,*nxt=wide_b;int n=1;cur[0]=*s0;
 for(int32_t f=f0;f<goal;f++){
  int m=0;if(!++wide_stamp){memset(wide_mark,0,sizeof wide_mark);wide_stamp=1;}
  for(int i=0;i<n;i++)for(int c=0;c<2;c++){
   if(c && cur[i].dwell<SV_MIN_DWELL)break;
   sst t=cur[i];
   if(!step(&t,f,c?!t.held:t.held))continue;
   int vy=(t.vy>>14)+32;if(vy<0)vy=0;
   if(vy>63)vy=63;
   uint32_t key=(uint32_t)((((t.y>>16)&255)*64+vy)*8+t.held*4+(t.dwell-1));
   if(wide_mark[key]==wide_stamp)continue;
   wide_mark[key]=wide_stamp;nxt[m++]=t;
  }
  if(!m)return 0;
  sst *x=cur;cur=nxt;nxt=x;n=m;
 }
 return 1;
}
static int pattern_at(int32_t x){
 for(uint32_t i=C.npat>PATTERN_N?C.npat-PATTERN_N:0;i<C.npat;i++){const pattern_rec *p=course_pattern(&C,i);if(x>=p->x0 && x<p->x1)return (int)i;}
 return -1;
}
/* the toggle frames that work among f .. f + last - 1 (path[i]: the kept input's state at frame f + i), as runs; each
 * run is a candidate (its middle and its length). full = 0: stop at the latest comfortable run */
typedef struct cand { int mid,window; } cand;
#define MAX_CANDS 24
static int scan_runs(const sst *path,int32_t f,int last,int32_t goal,int full,cand *out){
 int n=0,run_hi=-1;
 for(int i=last-1;i>=0;i--){
  int ok=0;
  if(path[i].dwell>=SV_MIN_DWELL){
   sst t=path[i];budget=BUDGET;ok=step(&t,f+i,!path[i].held) && alive(&t,f+i+1,goal);
   if(!ok && budget<=0){ok=alive_wide(&t,f+i+1,goal);if(!ok)sv_exhausted++;}
  }
  if(ok && run_hi<0)run_hi=i;
  int capped=run_hi>=0 && run_hi-i+1>=SV_WINDOW_CAP;
  if(run_hi>=0 && (!ok || i==0 || capped)){
   int lo=ok?i:i+1,len=run_hi-lo+1;
   if(n<MAX_CANDS){out[n].mid=(lo+run_hi)/2;out[n].window=len>SV_WINDOW_CAP?SV_WINDOW_CAP:len;n++;}
   run_hi=-1;
   if(!full && len>=SV_COMFORT)break;
  }
 }
 return n;
}
/* the careful player's preference: the latest comfortable run, else the widest */
static int preferred(const cand *c,int n){
 int best=-1;
 for(int i=0;i<n;i++){if(c[i].window>=SV_COMFORT)return i;if(best<0 || c[i].window>c[best].window)best=i;}
 return best;
}
/* a decision of the path, kept to come back to it (a later squeeze may need another choice here) */
typedef struct decision { int32_t f,death; sst s; int ntog,full,ncand,tried; cand c[MAX_CANDS]; } decision;
#define MAX_DECISIONS 4096
#define BACK_FRAMES 120         /* how far back the careful player reconsiders (the course ring keeps that much) */
#define BACK_DEPTH 6
static decision hist[MAX_DECISIONS];

static sst states[SV_MAX_FRAMES+1];       /* the path's state on every frame */
static void record(sv_run *r,int32_t f,const sst *s){r->held[f]=(int8_t)s->held;r->ys[f]=(int16_t)(s->y>>16);states[f]=*s;}
/* from decision d's state: keep the input to the candidate's middle frame, then change it */
static int take(sv_run *r,const decision *d,const cand *c,sst *s,int32_t *f){
 *s=d->s;*f=d->f;r->ntog=d->ntog;
 for(int i=0;i<c->mid;i++){step(s,*f,s->held);(*f)++;record(r,*f,s);}
 if(!step(s,*f,!s->held))return 0;
 (*f)++;record(r,*f,s);
 sv_toggle *tg=&r->tog[r->ntog++];
 tg->frame=*f;tg->x=slot_x+(int)(trav[*f]>>16);tg->window=c->window;
 tg->pattern=pattern_at(slot_x+(int)(trav[d->death]>>16));
 return 1;
}
static void keep_path(sst *path,int32_t f,const sst *s,int32_t *death){
 path[0]=*s;*death=0;
 for(int i=0;i<HORIZON;i++){path[i+1]=path[i];if(!step(&path[i+1],f+i,s->held)){*death=f+i+1;break;}}
}
void sv_careful(uint32_t seed,int slot,int32_t end_x,sv_run *r){
 static sst path[HORIZON+2];
 slot_x=slot;nodes=0;reset();nall=0;seen_n=0;
 course_init(&C,seed);keep_course(0);
 r->ok=0;r->ntog=0;r->min_window=99;r->peak_items=0;r->fail_frame=-1;r->fail_x=-1;
 sst s={FLIGHT_BOTTOM*Q16_ONE,0,0,SV_MIN_DWELL};
 int32_t f=0,accept=-1;int nh=0;record(r,0,&s);
 while(f<SV_MAX_FRAMES-HORIZON-BEYOND-2 && slot+(trav[f]>>16)<end_x){
  keep_course(f);
  if((int)(C.n-C.first)>r->peak_items)r->peak_items=(int)(C.n-C.first);
  /* keep the input: does it crash within the horizon? */
  int32_t death;keep_path(path,f,&s,&death);
  if(!death){
   if(!step(&s,f,s.held)){r->fail_frame=f;break;}
   f++;record(r,f,&s);continue;
  }
  /* a change is needed on one of the frames f .. death - 2 (the change applies to the step after it) */
  decision *d=&hist[nh<MAX_DECISIONS?nh:MAX_DECISIONS-1];
  d->f=f;d->death=death;d->s=s;d->ntog=r->ntog;d->full=0;d->tried=0;
  d->ncand=scan_runs(path,f,death-1-f,death+BEYOND,0,d->c);
  int pick=preferred(d->c,d->ncand);
  if((pick<0 || d->c[pick].window<3) && f>accept){
   /* squeezed: first, change earlier (the input was kept since the last change: the same crash ahead) */
   int moved=0,last_change=r->ntog?r->tog[r->ntog-1].frame:0,from=f-BACK_FRAMES;
   if(from<last_change+SV_MIN_DWELL)from=last_change+SV_MIN_DWELL;
   if(from<0)from=0;
   if(from<f){
    static sst ep[HORIZON+BACK_FRAMES+2];decision e;e.f=from;e.s=states[from];e.ntog=r->ntog;e.full=1;e.tried=0;
    ep[0]=e.s;e.death=0;
    for(int i=0;i<HORIZON+BACK_FRAMES;i++){ep[i+1]=ep[i];if(!step(&ep[i+1],from+i,e.s.held)){e.death=from+i+1;break;}}
    if(e.death){
     e.ncand=scan_runs(ep,from,e.death-1-from,e.death+BEYOND,1,e.c);
     int q=preferred(e.c,e.ncand);
     if(q>=0 && e.c[q].window>=3 && (pick<0 || e.c[q].window>d->c[pick].window)){
      if(nh<MAX_DECISIONS){hist[nh]=e;hist[nh].tried=1;}
      if(take(r,&e,&e.c[q],&s,&f)){if(nh<MAX_DECISIONS)nh++;continue;}
     }
    }
   }
   /* then reconsider the decisions just before (other runs of theirs), the nearest first */
   for(int h=nh-1;h>=0 && h>=nh-BACK_DEPTH && hist[h].f>=f-BACK_FRAMES && !moved;h--){
    decision *b=&hist[h];
    if(!b->full){
     /* all its runs, and earlier ones too: the input was kept since the change before it */
     int prev=b->ntog?r->tog[b->ntog-1].frame:0,from=b->f-BACK_FRAMES,old=b->f;
     if(from<prev+SV_MIN_DWELL)from=prev+SV_MIN_DWELL;
     if(from<0)from=0;
     if(from>b->f)from=b->f;
     static sst bp[HORIZON+BACK_FRAMES+2];int32_t bd=0;bp[0]=states[from];
     for(int i=0;i<HORIZON+BACK_FRAMES;i++){bp[i+1]=bp[i];if(!step(&bp[i+1],from+i,bp[0].held)){bd=from+i+1;break;}}
     int first=preferred(b->c,b->ncand);cand was=first>=0?b->c[first]:(cand){-1,0};was.mid+=old-from;
     b->f=from;b->s=states[from];if(bd)b->death=bd;
     b->ncand=scan_runs(bp,b->f,b->death-1-b->f,b->death+BEYOND,1,b->c);b->full=1;
     /* most robust first: the widest runs, the one taken already marked as tried */
     for(int i=1;i<b->ncand;i++){cand t=b->c[i];int j=i;while(j>0 && b->c[j-1].window<t.window){b->c[j]=b->c[j-1];j--;}b->c[j]=t;}
     for(int i=0;i<b->ncand;i++)if(b->c[i].mid==was.mid){cand t=b->c[i];for(int j=i;j>0;j--)b->c[j]=b->c[j-1];b->c[0]=t;}
     b->tried=1;
    }
    while(b->tried<b->ncand){
     cand c=b->c[b->tried++];
     if(c.window<3)continue;    /* no use trading one squeeze for another */
     if(take(r,b,&c,&s,&f)){nh=h+1;moved=1;break;}
    }
   }
   if(moved)continue;
   accept=f;                    /* nothing better nearby: take the squeeze (the validator reports it) */
   keep_path(path,f,&s,&death);d->f=f;d->death=death;d->s=s;d->ntog=r->ntog;
   d->ncand=scan_runs(path,f,death-1-f,death+BEYOND,0,d->c);pick=preferred(d->c,d->ncand);
  }
  if(pick<0){
   if(getenv("SV_DEBUG"))fprintf(stderr,"stuck: frame %d x %d y %d vy %d held %d dwell %d; keep crashes at frame %d x %d\n",
    (int)f,slot+(int)(trav[f]>>16),s.y>>16,s.vy,s.held,s.dwell,(int)death,slot+(int)(trav[death]>>16));
   r->fail_frame=f;r->fail_x=slot+(int)(trav[f]>>16);break;
  }
  if(nh<MAX_DECISIONS)nh++;
  else{memmove(hist,hist+1,sizeof hist[0]*(MAX_DECISIONS-1));d=&hist[MAX_DECISIONS-1];}
  d->tried=0;
  if(!take(r,d,&d->c[pick],&s,&f)){r->fail_frame=f;break;}
 }
 r->min_window=99;
 for(int i=0;i<r->ntog;i++)if(r->tog[i].window<r->min_window)r->min_window=r->tog[i].window;
 r->frames=f;r->nodes=nodes;
 if(r->fail_frame<0)r->ok=1;
 else if(r->fail_x<0)r->fail_x=slot+(int)(trav[r->fail_frame]>>16);
}
