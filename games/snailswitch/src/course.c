/*
 * Snail Switch: the director. It writes the course ahead of the snails as a chain of small segments, each ONE idea
 * with its own random parameters (counts, sides, gaps, hazard kinds): lone fruits, lone hazards, fruit trails, quick
 * zigzags, a fruit just after a hazard, slaloms, caterpillar clusters, sap stretches, a fruit and a hazard either side,
 * the classic fruit-then-hazard, rare greedy fruits, and breathers. It never repeats a kind twice in a row nor a pair of
 * kinds met in its last PAIR_MEMORY transitions; the gaps (in frames at the course speed) tighten and the harder kinds
 * come in as the distance grows (DESIGN.md "The course"). It is seeded and never looks at the screen: the same seed
 * gives the same vine at every width. tests/test_course.c proves its streams completable (tests/solver.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/snailswitch/LICENSE.
 */
#include "game.h"
#include <string.h>

const char*const seg_names[SEG_COUNT]={"breather","lone fruit","lone hazard","fruit trail","zigzag","fruit after",
 "slalom","caterpillars","sap stretch","greedy fruit","fruit then hazard","fork"};

/* the kinds' weights, easy and hard (the difficulty slides between them), and the difficulty they start at */
static const struct {uint8_t easy,hard,from;} kinds[SEG_COUNT]={
 [SEG_BREATHER]={5,3,0},[SEG_LONE]={10,4,0},[SEG_HAZARD]={10,5,0},[SEG_TRAIL]={8,7,0},[SEG_ZIGZAG]={7,10,0},
 [SEG_AFTER]={5,9,16},[SEG_SLALOM]={4,10,24},[SEG_CATERS]={3,8,48},[SEG_SAP]={4,6,32},[SEG_GREEDY]={2,4,96},
 [SEG_CLASSIC]={5,4,0},[SEG_FORK]={6,6,0}};

static uint32_t next(director*d){uint32_t x=d->rng;x^=x<<13;x^=x>>17;x^=x<<5;return d->rng=x;}
static int range(director*d,int lo,int hi){return hi<=lo?lo:lo+(int)(next(d)%(uint32_t)(hi-lo+1));}
static int coin(director*d,int per256){return (int)(next(d)&255)<per256;}
static uint32_t mix(uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;return x;}

int difficulty_at(int32_t x){if(x<=0)return 0;if(x>=DIFF_DIST)return 256;return (int)((int64_t)x*256/DIFF_DIST);}
static int lerp(int a,int b,int k){return a+(b-a)*k/256;}

/* ---- a segment being written ----------------------------------------------------------------------------------------- */
typedef struct seg {director*d;item*out;int n,max,diff;int32_t x;uint32_t sig;} seg;
/* px for a gap drawn from one of tuning.h's {easy lo, easy hi, hard lo, hard hi} rows, at the speed of this place */
static int gap(seg*s,const int g[4]){
 int lo=lerp(g[0],g[2],s->diff),hi=lerp(g[1],g[3],s->diff),f=range(s->d,lo,hi);
 s->sig=s->sig*31u+(uint32_t)f;
 return (int)(((int64_t)f*course_speed(s->x)+Q16_ONE/2)>>16);
}
static void param(seg*s,int v){s->sig=s->sig*31u+(uint32_t)v;}
static void put(seg*s,int kind,int side){
 if(s->n>=s->max)return;
 item*it=&s->out[s->n++];memset(it,0,sizeof *it);
 if(kind==IT_CATER&&(s->n>1||s->d->segs))s->x+=PATROL; /* its patrol keeps its distance from what came before */
 it->x=s->x;it->kind=(uint8_t)kind;it->side=(uint8_t)side;it->seg=(uint8_t)(s->d->segs&255);
 it->id=kind==IT_FRUIT?s->d->fruits++:-1;
 if(kind==IT_CATER){it->phase=(uint8_t)range(s->d,0,PATROL_T-1);s->x+=PATROL;}
 param(s,kind*2+side);
}
static void fruit(seg*s,int side){put(s,IT_FRUIT,side);}
/* a hazard of a kind drawn for the place: caterpillars come in with the difficulty */
static int hazard_kind(seg*s){int c=lerp(40,90,s->diff);int r=range(s->d,0,255);return r<c?IT_CATER:r<c+(256-c)/2?IT_THORN:IT_SAP;}
static void hazard(seg*s,int kind,int side){put(s,kind,side);}
static void step(seg*s,const int g[4]){s->x+=gap(s,g);}
/* a gap with (kind >= 0) something half-way along it */
static void halfway(seg*s,const int g[4],int kind,int side){
 int px=gap(s,g);s->x+=px/2;if(kind>=0)put(s,kind,side);s->x+=px-px/2;
}

static const int g_link[4]=GAP_LINK,g_phrase[4]=GAP_PHRASE,g_breath[4]=GAP_BREATH,g_trail[4]=GAP_TRAIL,g_zig[4]=GAP_ZIGZAG,
 g_slalom[4]=GAP_SLALOM,g_after[4]=GAP_AFTER,g_classic[4]=GAP_CLASSIC,g_sap[4]=GAP_SAP,g_cater[4]=GAP_CATER,
 g_greedy[4]=GAP_GREEDY,g_fork[4]=GAP_FORK;

/* ---- the segments: one idea each ----------------------------------------------------------------------------------- */
static void write(seg*s,int kind){
 director*d=s->d;int side=range(d,0,1),k=s->diff;
 switch(kind){
 case SEG_BREATHER:step(s,g_breath);break;          /* nothing: the vine and its leaves */
 case SEG_LONE:fruit(s,side);break;                  /* a fruit, and nothing after it */
 case SEG_HAZARD:hazard(s,hazard_kind(s),side);break;/* a hazard, and no fruit */
 case SEG_TRAIL:{                                     /* fruits along one side; on harder ground a guard opposite */
  int n=range(d,3,3+(k>=96)+(k>=192)),guard=k>=64&&coin(d,128);param(s,n*2+guard);
  for(int i=0;i<n;i++){
   if(i)halfway(s,g_trail,guard&&i==n/2?(range(d,0,1)?IT_THORN:IT_SAP):-1,1-side);
   fruit(s,side);
  }
  break;}
 case SEG_ZIGZAG:{                                    /* two or three fruits alternating sides: quick double flips */
  int n=2+(k>=64&&coin(d,96+k/4));param(s,n);
  for(int i=0;i<n;i++){if(i)step(s,g_zig);fruit(s,side^(i&1));}
  break;}
 case SEG_AFTER:{                                     /* a hazard, then its fruit just after: flip back for it */
  int lead=k>=48&&coin(d,100);param(s,lead);
  if(lead){fruit(s,1-side);step(s,g_classic);}      /* (from below the hazard, the fruit opposite first) */
  hazard(s,hazard_kind(s),side);step(s,g_after);fruit(s,side);
  break;}
 case SEG_SLALOM:{                                    /* hazards on alternating sides */
  int n=range(d,3,3+(k>=80)+(k>=160));param(s,n);
  for(int i=0;i<n;i++){if(i)step(s,g_slalom);hazard(s,k>=64&&coin(d,70)?IT_CATER:range(d,0,1)?IT_THORN:IT_SAP,side^(i&1));}
  break;}
 case SEG_CATERS:{                                    /* a cluster of caterpillars; between two on one side, a fruit */
  int n=range(d,2,2+(k>=128)),same=coin(d,128);param(s,n*2+same);
  /* (on one side, a fruit half-way between two: the cluster's safe window, timed against their patrols) */
  for(int i=0;i<n;i++){
   if(i)halfway(s,g_cater,same?IT_FRUIT:-1,side);
   hazard(s,IT_CATER,same?side:side^(i&1));
  }
  break;}
 case SEG_SAP:{                                       /* a stretch of sap on one side; fruits opposite, now and then */
  int n=range(d,3,4+(k>=128)),fr=coin(d,110);param(s,n*2+fr);
  for(int i=0;i<n;i++){if(i)halfway(s,g_sap,fr&&(i&1)?IT_FRUIT:-1,1-side);hazard(s,IT_SAP,side);}
  break;}
 case SEG_GREEDY:{                                    /* a fruit between two hazards on its side: in and out quickly */
  hazard(s,range(d,0,1)?IT_THORN:IT_SAP,side);step(s,g_greedy);fruit(s,side);step(s,g_greedy);
  hazard(s,range(d,0,1)?IT_THORN:IT_SAP,side);
  break;}
 case SEG_CLASSIC:                                    /* a fruit, then a hazard on its side, at a varying distance */
  fruit(s,side);step(s,g_classic);hazard(s,hazard_kind(s),side);break;
 case SEG_FORK:{                                      /* a hazard and a fruit on the other side, nearly level */
  int first=range(d,0,1);param(s,first);
  if(first){hazard(s,range(d,0,1)?IT_THORN:IT_SAP,side);step(s,g_fork);fruit(s,1-side);}
  else{fruit(s,1-side);step(s,g_fork);hazard(s,range(d,0,1)?IT_THORN:IT_SAP,side);}
  break;}
 }
}

/* ---- choosing ------------------------------------------------------------------------------------------------------ */
static int recent(const director*d,int k){for(int i=0;i<RECENT_MEMORY;i++)if(d->recent[i]==k)return 1;return 0;}
static int pair_seen(const director*d,int a,int b){
 int n=d->npairs<PAIR_MEMORY?d->npairs:PAIR_MEMORY;
 for(int i=0;i<n;i++)if(d->pairs[i][0]==a&&d->pairs[i][1]==b)return 1;
 return 0;
}
/* the next kind: by weight for the difficulty, never the last one again, no pair met lately; a breather (a rest,
 * outside the pair memory) when enough has come since the last one */
static int choose(director*d,int diff,int first){
 if(!first&&d->load>=d->load_max&&d->prev!=SEG_BREATHER)return SEG_BREATHER;
 int w[SEG_COUNT],tot=0;
 for(int pass=0;pass<2&&!tot;pass++)
  for(int k=0;k<SEG_COUNT;k++){
   w[k]=0;
   if(diff<kinds[k].from||k==d->last||k==d->prev)continue;
   if(first&&k!=SEG_LONE&&k!=SEG_HAZARD&&k!=SEG_TRAIL&&k!=SEG_CLASSIC&&k!=SEG_FORK)continue;
   if(k==SEG_GREEDY&&d->greedy_cool>0)continue;
   if(k==SEG_BREATHER&&d->load<3)continue;
   if(pass==0&&k!=SEG_BREATHER&&pair_seen(d,d->last,k))continue;   /* no pair of kinds twice within the memory */
   int x=lerp(kinds[k].easy,kinds[k].hard,diff)*16;
   if(recent(d,k))x/=4;
   w[k]=x>0?x:1;tot+=w[k];
  }
 if(!tot)return SEG_LONE;
 int r=range(d,0,tot-1),k=0;while(r>=w[k])r-=w[k++];
 return k;
}

void director_start(director*d,uint32_t seed){
 memset(d,0,sizeof *d);
 d->rng=mix(seed^0x9e3779b9u)|1;d->last=d->prev=-1;
 for(int i=0;i<RECENT_MEMORY;i++)d->recent[i]=-1;
 d->load_max=(int16_t)range(d,BREATH_LOAD_EASY-2,BREATH_LOAD_EASY+2);
 d->x=range(d,FIRST_X_MIN,FIRST_X_MAX);
 d->phrase=(int16_t)range(d,2,4);
}

int director_segment(director*d,item*out,int max){
 int first=d->segs==0,diff=difficulty_at(d->x);
 int kind=choose(d,diff,first);
 seg s={d,out,0,max,diff,d->x,(uint32_t)kind};
 write(&s,kind);
 /* remember it: the pairs and the recent kinds skip the breathers (rests) */
 if(kind!=SEG_BREATHER){
  if(d->last>=0){int i=d->npairs++%PAIR_MEMORY;d->pairs[i][0]=d->last;d->pairs[i][1]=(int8_t)kind;}
  memmove(d->recent+1,d->recent,sizeof d->recent-sizeof d->recent[0]);d->recent[0]=(int8_t)kind;
  d->last=(int8_t)kind;
  d->load=(int16_t)(d->load+s.n);
  if(d->greedy_cool>0)d->greedy_cool--;
  if(kind==SEG_GREEDY)d->greedy_cool=GREEDY_COOL;
  /* the link to the next segment, at this place's speed: close within a phrase, longer after it */
  if(--d->phrase>0)step(&s,g_link);
  else{step(&s,g_phrase);d->phrase=(int16_t)range(d,2,4);}
 }else{
  int m=lerp(BREATH_LOAD_EASY,BREATH_LOAD_HARD,diff);
  d->load=0;d->load_max=(int16_t)range(d,m-3,m+3);d->phrase=(int16_t)range(d,2,4);
 }
 d->prev=(int8_t)kind;d->sig=s.sig;d->segs++;d->x=s.x;
 return s.n;
}
