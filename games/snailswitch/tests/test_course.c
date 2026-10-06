/*
 * Snail Switch: the course's proofs and measures (make snailswitch-check; DESIGN.md "The course"). It runs the
 * director (src/course.c) and the game's own rules (hero_step, through src/plan.c), and:
 *   1. the solver: every stream is completable (1000 seeds x 3 minutes for the leading snail, 250 seeds for each of
 *      the three behind it), by a careful player who keeps its side until it must change and then presses in the
 *      middle of the frames that still work (its WINDOW), never before the thing it reacts to is on a 4:3 screen;
 *      every window >= 3 frames (the tightest are printed, with the segment they are in);
 *   2. the measured difficulty: presses per 10 s and the windows, per 30 s of a run (it rises with the distance);
 *   3. the variety: the distinct segment sequences in the first 30 / 60 / 120 s over 1000 seeds, the pairs of kinds
 *      (none repeats within the director's memory), and how fruits and hazards follow each other;
 *   4. the course's form: things in x order, fruit windows apart, the first thing 450..510 (334..394 px ahead).
 *   test_course [--seeds N] [--quick]
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/snailswitch/LICENSE.
 */
#include "plan.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SECS        180
#define MAXF        (SECS*60)
#define MAXI        6000
#define MAXSEG      1200
#define WIN_MIN     3                   /* frames: the tightest a window may be */
static int fails;
#define FAIL(...) do{printf("  FAIL ");printf(__VA_ARGS__);printf("\n");fails++;}while(0)

static item IT[MAXI];static int NI;static int32_t NEXT_ID;
static int BIG_SEG;
static int ITSEG[MAXI],SEGKIND[MAXSEG],NSEG;static int32_t SEGX[MAXSEG];static uint32_t SEGSIG[MAXSEG];
static int32_t SC[MAXF+1],LOW[MAXF+1];static int TT[MAXF+1],FIRST[MAXF+1];
static uint8_t R[MAXF+1][PLAN_STATES],A[MAXF+1][PLAN_STATES];

static uint32_t seed_of(int i){return 0x5eedu^(uint32_t)(90+i*37)*2654435761u;}

/* the course of a seed, to world x `upto` */
static void build(uint32_t seed,int32_t upto){
 director d;director_start(&d,seed);NI=0;NSEG=0;
 while(d.x<upto&&NSEG<MAXSEG&&NI+SEG_MAX_ITEMS<=MAXI){
  int32_t x0=d.x;int n=director_segment(&d,IT+NI,SEG_MAX_ITEMS);
  if(n>=SEG_MAX_ITEMS)BIG_SEG++;
  for(int i=0;i<n;i++)ITSEG[NI+i]=NSEG;
  SEGKIND[NSEG]=d.prev;SEGX[NSEG]=x0;SEGSIG[NSEG]=d.sig;NSEG++;NI+=n;
 }
 NEXT_ID=d.fruits;
}
static void frames(void){
 int64_t tr=0;SC[0]=0;TT[0]=0;
 for(int f=1;f<=MAXF;f++){TT[f]=f;tr+=course_speed(SC[f-1]);SC[f]=(int32_t)(tr>>16);}
}

/* ---- 1. the solver ----------------------------------------------------------------------------------------------- */
typedef struct tight {int win,seed,frame,kind,hx;int32_t x;} tight;
static tight TIGHT[8];static int NTIGHT;
static long BUCKET_PRESS[SECS/30],BUCKET_WIN[SECS/30];static int BUCKET_MIN[SECS/30];
static int MIN_WIN=1<<30,MIN_REACT=1<<30;static long PRESSES;
static int KIND_MIN[SEG_COUNT];static long KIND_N[SEG_COUNT],KIND_SUM[SEG_COUNT];
static void note_tight(int win,int seed,int frame,int kind,int hx,int32_t x){
 int i;
 if(NTIGHT==8){if(TIGHT[7].win<=win)return;i=7;}else i=NTIGHT++;
 TIGHT[i]=(tight){win,seed,frame,kind,hx,x};
 while(i>0&&TIGHT[i].win<TIGHT[i-1].win){tight t=TIGHT[i];TIGHT[i]=TIGHT[i-1];TIGHT[i-1]=t;i--;}
}
/* the thing a snail holding its side from state c at frame f runs into: its index (-1: none before the end) */
static int threat(const plan*p,int f,int c){
 int side=plan_side(c);
 for(int g=f+1;g<=p->nf;g++){
  int a=plan_next(p,g,c,0);
  if(a<0){
   int32_t wx=SC[g]+p->hx;
   for(int i=p->first[g];i<NI&&IT[i].x<=wx+64;i++){
    if(IT[i].kind==IT_FRUIT){if(IT[i].id>p->low[g-1]+(c>=PLAN_K)&&wx>IT[i].x+FRUIT_HALF)return i;}
    else{obstacle o;item_at(&IT[i],TT[g],&o);if(o.side==side&&abs(o.x-wx)<HAZ_HALF)return i;}
   }
   return -1;
  }
  c=a;
 }
 return -1;
}
static int solve(int seedi,int hx,int nf,int stats){
 int32_t lowp=0;int fp=0;
 for(int f=0;f<=nf;f++){
  int32_t wx=SC[f]+hx;
  while(fp<NI&&IT[fp].x<wx-64)fp++;
  FIRST[f]=fp;
  while(lowp<NI&&!(IT[lowp].kind==IT_FRUIT&&IT[lowp].x+FRUIT_HALF>=wx))lowp++;
  LOW[f]=lowp<NI?IT[lowp].id-1:NEXT_ID-1;
 }
 plan p={IT,NI,hx,nf,SC,TT,LOW,FIRST};
 memset(R,0,sizeof R[0]*(size_t)(nf+1));
 R[0][plan_state(0,0,0)]=1;
 for(int f=1;f<=nf;f++){
  int any=0;
  for(int s=0;s<PLAN_STATES;s++)if(R[f-1][s]){
   int a=plan_next(&p,f,s,0);if(a>=0){R[f][a]=1;any=1;}
   if(!plan_flip(s)){a=plan_next(&p,f,s,1);if(a>=0){R[f][a]=1;any=1;}}
  }
  if(!any){
   int32_t wx=SC[f]+hx;int i=FIRST[f];
   FAIL("seed %d (snail at %d): no way past frame %d (x %d, %s)",seedi,hx,f,(int)wx,i<NI?seg_names[SEGKIND[ITSEG[i]]]:"?");
   return 0;
  }
 }
 memcpy(A[nf],R[nf],PLAN_STATES);
 for(int f=nf-1;f>=0;f--)for(int s=0;s<PLAN_STATES;s++){
  A[f][s]=0;if(!R[f][s])continue;
  int a=plan_next(&p,f+1,s,0),b=plan_flip(s)?-1:plan_next(&p,f+1,s,1);
  A[f][s]=(a>=0&&A[f+1][a])||(b>=0&&A[f+1][b]);
 }
 /* the careful player */
 int f=0,s=plan_state(0,0,0);
 while(f<nf){
  if(plan_flip(s)){s=plan_next(&p,f+1,s,0);f++;if(s<0||!A[f][s]){FAIL("seed %d: careful path lost in a curl at %d",seedi,f);return 0;}continue;}
  int c=s,lo=-1,hi=-1,prev=0,doom=-1;
  for(int g=f+1;g<=nf;g++){
   int b=plan_next(&p,g,c,1),works=b>=0&&A[g][b];
   if(works){if(!prev)lo=g;hi=g;}
   prev=works;
   int a=plan_next(&p,g,c,0);
   if(a<0||!A[g][a]){doom=g;break;}
   c=a;
  }
  if(doom<0)break;                       /* it can hold its side to the end */
  if(hi<0){FAIL("seed %d: no press works before frame %d",seedi,doom);return 0;}
  /* the thing it reacts to: what holding the side runs into; a press only counts once it is on a 4:3 screen */
  int ti=threat(&p,f,s),vis=f+1;
  if(ti>=0){int a=0,b=nf;while(a<b){int m=(a+b)/2;if(IT[ti].x-SC[m]>RS_SCREEN_W)a=m+1;else b=m;}vis=a;}
  if(lo<vis)lo=vis;
  int win=hi-lo+1,kind=ti>=0?SEGKIND[ITSEG[ti]]:-1;
  if(win<WIN_MIN){FAIL("seed %d (snail at %d): a %d-frame window at frame %d (x %d, %s)",seedi,hx,win,hi,(int)(SC[hi]+hx),kind>=0?seg_names[kind]:"?");
   if(win<1)return 0;}
  if(win<MIN_WIN)MIN_WIN=win;
  if(ti>=0&&hi-vis<MIN_REACT)MIN_REACT=hi-vis;
  note_tight(win,seedi,hi,kind,hx,SC[hi]+hx);
  if(stats&&kind>=0){KIND_N[kind]++;KIND_SUM[kind]+=win;if(!KIND_MIN[kind]||win<KIND_MIN[kind])KIND_MIN[kind]=win;}
  int m=(lo+hi)/2;
  for(int g=f+1;g<m;g++)s=plan_next(&p,g,s,0);
  s=plan_next(&p,m,s,1);f=m;
  if(s<0||!A[f][s]){FAIL("seed %d: careful press at %d fails",seedi,m);return 0;}
  PRESSES++;
  if(stats&&m/1800<SECS/30){int b=m/1800;BUCKET_PRESS[b]++;BUCKET_WIN[b]+=win;if(win<BUCKET_MIN[b])BUCKET_MIN[b]=win;}
 }
 return 1;
}

/* ---- 3. the variety ---------------------------------------------------------------------------------------------- */
static int cmpu(const void*a,const void*b){uint32_t x=*(const uint32_t*)a,y=*(const uint32_t*)b;return x<y?-1:x>y;}
static int distinct(uint32_t*v,int n){qsort(v,(size_t)n,4,cmpu);int d=n?1:0;for(int i=1;i<n;i++)d+=v[i]!=v[i-1];return d;}

int main(int argc,char**argv){
 int seeds=1000,quick=0;
 for(int i=1;i<argc;i++){if(!strcmp(argv[i],"--seeds")&&i+1<argc)seeds=atoi(argv[++i]);if(!strcmp(argv[i],"--quick"))quick=1;}
 if(quick&&seeds>200)seeds=200;
 frames();
 int32_t far=SC[MAXF]+116+400;
 /* 4. the form */
 int bad_order=0,bad_fruit=0,bad_first=0,overflow=0;
 for(int i=0;i<seeds;i++){
  build(seed_of(i),far);
  if(IT[0].x<FIRST_X_MIN||IT[0].x>FIRST_X_MAX+PATROL)bad_first++;
  int32_t lastf=-1000;
  for(int j=1;j<NI;j++)if(IT[j].x<IT[j-1].x)bad_order++;
  for(int j=0;j<NI;j++)if(IT[j].kind==IT_FRUIT){if(IT[j].x-lastf<2*FRUIT_HALF)bad_fruit++;lastf=IT[j].x;}
  /* the buffer (play.c course_fill): what lies between KEEP_BEHIND behind and the next segment past FILL_AHEAD */
  for(int f=0;f<=MAXF;f+=7){
   int32_t lo=SC[f]-KEEP_BEHIND,hi=SC[f]+FILL_AHEAD;int n=0,j=0;
   while(j<NI&&IT[j].x<lo)j++;
   int last=j;while(last<NI&&SEGX[ITSEG[last]]<hi)last++;
   n=last-j;if(n+SEG_MAX_ITEMS>ITEM_MAX)overflow++;
  }
 }
 printf("4. the course's form (%d seeds, %d s): ",seeds,SECS);
 if(bad_order)FAIL("%d things out of x order",bad_order);
 if(bad_fruit)FAIL("%d fruits closer than their catch windows",bad_fruit);
 if(bad_first)FAIL("%d courses whose first thing is not 450..510",bad_first);
 if(overflow)FAIL("the course buffer (%d) would overflow %d times",ITEM_MAX,overflow);
 if(BIG_SEG)FAIL("%d segments as big as SEG_MAX_ITEMS (%d)",BIG_SEG,SEG_MAX_ITEMS);
 if(!bad_order&&!bad_fruit&&!bad_first&&!overflow&&!BIG_SEG)printf("in x order, fruit windows apart, the first thing at 450..510 "
  "(334..394 px ahead), the buffer never full: passed\n");
 /* 1. + 2. the solver */
 printf("1. the solver: the careful player, every press >= %d frames of window, after its threat is on a 4:3 screen\n",WIN_MIN);
 for(int b=0;b<SECS/30;b++)BUCKET_MIN[b]=1<<30;
 int ok=0;
 for(int i=0;i<seeds;i++){build(seed_of(i),far);ok+=solve(i,116,MAXF,1);}
 printf("   the leading snail (x 116): %d / %d streams of %d s completable; %ld presses, the tightest window %d frames, "
        "the least reaction %d frames\n",ok,seeds,SECS,PRESSES,MIN_WIN,MIN_REACT);
 if(ok<seeds)FAIL("%d streams not completable",seeds-ok);
 int behind=quick?50:250,ok2=0;
 for(int i=0;i<behind;i++){build(seed_of(i),far);for(int q=1;q<4;q++)ok2+=solve(i,116-27*q,MAXF,0);}
 printf("   the snails behind (x 89, 62, 35): %d / %d streams completable\n",ok2,3*behind);
 if(ok2<3*behind)FAIL("%d streams not completable for the snails behind",3*behind-ok2);
 printf("   the tightest windows:\n");
 for(int i=0;i<NTIGHT;i++)printf("     %2d frames: seed %d, snail at %d, frame %d (%d s, x %d), %s\n",TIGHT[i].win,TIGHT[i].seed,
  TIGHT[i].hx,TIGHT[i].frame,TIGHT[i].frame/60,(int)TIGHT[i].x,TIGHT[i].kind>=0?seg_names[TIGHT[i].kind]:"?");
 printf("   the windows by the segment the threat is in (the leading snail): least / mean frames\n    ");
 for(int k=0;k<SEG_COUNT;k++)if(KIND_N[k])printf(" %s %d / %.0f%s",seg_names[k],KIND_MIN[k],(double)KIND_SUM[k]/KIND_N[k],k==6?"\n    ":";");
 printf("\n");
 printf("2. the measured difficulty (the leading snail, %d runs): per 30 s, the presses per 10 s, the mean and least window\n",seeds);
 for(int b=0;b<SECS/30;b++)
  printf("   %3d-%3d s (x %5d-%5d, %.1f-%.1f px/f): %.2f presses / 10 s, window mean %.1f, least %d frames\n",b*30,b*30+30,
   (int)SC[b*1800],(int)SC[b*1800+1800],course_speed(SC[b*1800])/65536.0,course_speed(SC[b*1800+1800])/65536.0,
   BUCKET_PRESS[b]*10.0/(30.0*seeds),BUCKET_PRESS[b]?(double)BUCKET_WIN[b]/BUCKET_PRESS[b]:0.0,BUCKET_MIN[b]);
 /* 3. the variety */
 static uint32_t hk[3][4000],hp[3][4000];
 int secs[3]={30,60,120};int32_t lim[3];
 for(int t=0;t<3;t++)lim[t]=SC[secs[t]*60]+116+(RS_SCREEN_W-116);
 long kind_count[SEG_COUNT]={0},pairs=0,pair_rep=0,fr=0,fr_then_same=0,fr_after_haz=0,fr_alone=0;
 static int gapseen[2048];memset(gapseen,0,sizeof gapseen);
 for(int i=0;i<seeds&&i<4000;i++){
  build(seed_of(i),lim[2]+800);
  for(int t=0;t<3;t++){
   uint32_t a=2166136261u,b=2166136261u;
   for(int g=0;g<NSEG&&SEGX[g]<lim[t];g++){a=(a^(uint32_t)(SEGKIND[g]+1))*16777619u;b=(b^SEGSIG[g])*16777619u;}
   hk[t][i]=a;hp[t][i]=b;
  }
  int prevk[PAIR_MEMORY+1][2],np=0,last=-1;
  for(int g=0;g<NSEG&&SEGX[g]<lim[2];g++){
   kind_count[SEGKIND[g]]++;
   if(SEGKIND[g]==SEG_BREATHER)continue;
   if(last>=0){
    pairs++;
    for(int q=0;q<np&&q<PAIR_MEMORY;q++)if(prevk[q][0]==last&&prevk[q][1]==SEGKIND[g]){pair_rep++;break;}
    memmove(prevk+1,prevk,sizeof prevk[0]*PAIR_MEMORY);prevk[0][0]=last;prevk[0][1]=SEGKIND[g];np++;
   }
   last=SEGKIND[g];
  }
  for(int j=0;j<NI&&IT[j].x<lim[2];j++){
   if(IT[j].kind!=IT_FRUIT)continue;
   fr++;
   int nh=-1,ph=-1;
   for(int q=j+1;q<NI;q++)if(IT[q].kind!=IT_FRUIT){nh=q;break;}
   for(int q=j-1;q>=0;q--)if(IT[q].kind!=IT_FRUIT){ph=q;break;}
   if(nh>=0){int gx=IT[nh].x-IT[j].x;if(gx<2048)gapseen[gx]=1;if(gx<=120&&IT[nh].side==IT[j].side)fr_then_same++;if(gx>200)fr_alone++;}
   if(ph>=0&&IT[j].x-IT[ph].x<=120&&IT[ph].side==IT[j].side)fr_after_haz++;
  }
 }
 int n=seeds<4000?seeds:4000,gaps=0;for(int g=0;g<2048;g++)gaps+=gapseen[g];
 printf("3. the variety (%d seeds): distinct segment sequences in the first 30 / 60 / 120 s (to the 4:3 screen's edge):\n",n);
 int d30=0;
 for(int t=0;t<3;t++){int a=distinct(hk[t],n),b=distinct(hp[t],n);if(!t)d30=a;
  printf("   %3d s (x %5d): %4d by segment kind (%.1f%%), %4d with their parameters (%.1f%%)\n",secs[t],(int)lim[t],a,100.0*a/n,b,100.0*b/n);}
 if(d30<n*95/100)FAIL("only %d of %d runs open with a different sequence of segments",d30,n);
 printf("   the kinds met (first 120 s):");
 long tot=0;for(int k=0;k<SEG_COUNT;k++)tot+=kind_count[k];
 for(int k=0;k<SEG_COUNT;k++)printf("%s %s %.1f%%",k?",":"",seg_names[k],100.0*kind_count[k]/(tot?tot:1));
 printf("\n   pairs of kinds: %ld, %ld (%.2f%%) met again within the last %d\n",pairs,pair_rep,100.0*pair_rep/(pairs?pairs:1),PAIR_MEMORY);
 if(pair_rep*100>pairs)FAIL("pairs of kinds repeat too often");
 printf("   fruits: %ld; followed within 120 px by a hazard on their side %.1f%%; just after a hazard on their side %.1f%%; "
        "with nothing for 200 px after %.1f%%; %d distinct fruit-to-next-hazard distances\n",fr,100.0*fr_then_same/fr,
        100.0*fr_after_haz/fr,100.0*fr_alone/fr,gaps);
 printf(fails?"test_course: %d FAILED\n":"test_course: all passed\n",fails);
 return fails?1:0;
}
