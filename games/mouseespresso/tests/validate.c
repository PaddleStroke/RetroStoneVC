/*
 * Mouse Espresso: the validator (DESIGN.md "The validator"). The careful player (tests/solver.c) flies the real
 * director's courses over many seeds on the game's own flight and collisions; this proves and measures:
 *   1. every stream is passable to END_X (a score of about 3300) with inputs held or released >= 4 frames each;
 *   2. every timing window on the careful path is >= 3 frames (the tightest per pattern kind is printed);
 *   3. the same for the four-player slots (x 48, 76, 104, 132) on a share of the seeds;
 *   4. the measured difficulty of each pattern instance follows the rising target: the mean per distance band
 *      rises, the measured score follows the director's k, breathers are easier than their neighbours, the first
 *      EASY_PX are easy yet weave within seconds;
 *   5. the item ring never fills; no biscuit lies inside a solid item; the same seed flies the same path.
 *   validate [--seeds N] [--first S] [--quad N] [--end X] [--jobs J] [--report FILE] [--csv FILE]
 * The seeds are shared among J worker processes (each writes its lines to a file; the parent adds them up).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/mouseespresso/LICENSE.
 */
#define _POSIX_C_SOURCE 200809L
#include "solver.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static const char *KIND[PK_COUNT]={"breather","alternate","narrow","stairs","zigzag","cluster","corridor","jets","ladles",
 "bouncers"};
#define BAND_PX 3000
#define BANDS 14
static FILE *rep;
static int failures;
static void say(const char *fmt,...){
 va_list a;va_start(a,fmt);vprintf(fmt,a);va_end(a);fflush(stdout);
 if(rep){va_start(a,fmt);vfprintf(rep,fmt,a);va_end(a);}
}
static void fail(const char *fmt,...){
 va_list a;va_start(a,fmt);printf("  FAIL ");vprintf(fmt,a);va_end(a);fflush(stdout);
 if(rep){va_start(a,fmt);fprintf(rep,"  FAIL ");vfprintf(rep,fmt,a);va_end(a);}
 failures++;
}
static double clampd(double v){return v<0?0:v>1?1:v;}

/* ---- one seed (in a worker): its lines ------------------------------------------------------------------------------ *
 * S seed ok fail_x min_window peak early_changes early_tight biscuit_bad det_ok
 * W window kind              one per change of input
 * M kind k frames changes window dynamic score x0 breather_easier(-1: not a breather)
 * Q seed slot ok fail_x min_window */
typedef struct measure { int frames,toggles,window,dyn,k,kind,wsum; double vert,score; int32_t x0; } measure;
/* the measured difficulty (DESIGN.md "Measured difficulty"): the tightest and the mean window, the changes of input
 * per second, the vertical speed, the moving hazards and the travel speed, each normalised to 0..1 */
static double score_of(const measure *m){
 double secs=m->frames/60.0;if(secs<0.25)secs=0.25;
 double mean=m->toggles?(double)m->wsum/m->toggles:SV_WINDOW_CAP;
 double w=clampd((14.0-m->window)/11.0),a=clampd((14.0-mean)/10.0),d=clampd(m->toggles/secs/4.0),v=clampd(m->vert/m->frames/2.2),
  s=clampd(m->dyn/3.0),p=clampd((course_speed(m->x0-72)-SPEED_START)/(double)(SPEED_MAX-SPEED_START));
 return 100*(0.25*w+0.20*a+0.20*d+0.10*v+0.10*s+0.15*p);
}
static sv_toggle tog[SV_MAX_FRAMES],tog2[SV_MAX_FRAMES];
static int8_t held[SV_MAX_FRAMES+1],held2[SV_MAX_FRAMES+1];
static int16_t ys[SV_MAX_FRAMES+1],ys2[SV_MAX_FRAMES+1];
static measure ms[SV_MAX_PATTERNS];

static int measure_run(const sv_run *r,int np,const sv_pattern *pp){
 for(int i=0;i<np;i++){memset(&ms[i],0,sizeof ms[i]);ms[i].window=SV_WINDOW_CAP;ms[i].dyn=pp[i].dynamic;ms[i].k=pp[i].p.k;
  ms[i].kind=pp[i].p.kind;ms[i].x0=pp[i].p.x0;}
 int pi=0;
 for(int f=1;f<=r->frames;f++){
  int32_t x=72+(int32_t)(sv_travel(f)>>16);
  while(pi<np && x>=pp[pi].p.x1)pi++;
  if(pi>=np)break;
  if(x<pp[pi].p.x0)continue;
  ms[pi].frames++;ms[pi].vert+=abs(r->ys[f]-r->ys[f-1]);
 }
 for(int i=0;i<r->ntog;i++){int p=r->tog[i].pattern;if(p<0 || p>=np)continue;ms[p].toggles++;ms[p].wsum+=r->tog[i].window;if(r->tog[i].window<ms[p].window)ms[p].window=r->tog[i].window;}
 int last=-1;
 for(int i=0;i<np;i++){
  if(pp[i].p.x1>72+(sv_travel(r->frames)>>16))break;
  if(ms[i].frames)ms[i].score=score_of(&ms[i]);
  last=i;
 }
 return last;
}
/* no biscuit inside a solid item: the mouse at the biscuit, still (moving items: never at their rest pose) */
static int biscuits_bad(const course *c,uint32_t from){
 int bad=0;
 for(uint32_t i=from;i<c->n;i++){
  const item *b=course_item(c,i);if(b->kind!=IT_BISCUIT)continue;
  for(uint32_t j=(uint32_t)course_first_near(c,b->x-8);j<c->n;j++){
   const item *it=course_item(c,j);if(it->x-100>b->x+8)break;
   if(it->kind==IT_BISCUIT || it->kind>=IT_JET)continue;
   item_pose p;item_pose_at(it,0,&p);p.drops=0;
   if(item_hits(it,&p,b->x,b->x,b->y,b->y)){bad++;break;}
  }
 }
 return bad;
}
static void run_seed(FILE *out,uint32_t seed,int32_t end_x,int quad){
 sv_run r;r.tog=tog;r.held=held;r.ys=ys;
 sv_careful(seed,72,end_x,&r);
 int np;const sv_pattern *pp=sv_patterns(&np);
 /* biscuits: over the ring as the run left it, and over a fresh course to the end */
 static course c;course_init(&c,seed);int bad=0;
 for(int32_t x=0;x<end_x;x+=200){course_prune(&c,x-KEEP_BEHIND);uint32_t n0=c.n;course_fill(&c,x+GEN_AHEAD);bad+=biscuits_bad(&c,n0);}
 /* the same seed flies the same path */
 sv_run r2;r2.tog=tog2;r2.held=held2;r2.ys=ys2;int det=1;
 if(seed%16==1){sv_careful(seed,72,end_x,&r2);det=r2.frames==r.frames && r2.ntog==r.ntog && !memcmp(held,held2,(size_t)r.frames+1) &&
  !memcmp(ys,ys2,sizeof(int16_t)*((size_t)r.frames+1));
  sv_careful(seed,72,end_x,&r);pp=sv_patterns(&np);}
 int early=0,tight=0;
 for(int i=0;i<r.ntog;i++){if(r.tog[i].frame<=300)early++;if(r.tog[i].x<HAZARD_FIRST+EASY_PX && r.tog[i].window<6)tight++;}
 fprintf(out,"S %u %d %d %d %d %d %d %d %d %ld\n",seed,r.ok,(int)r.fail_x,r.min_window,r.peak_items,early,tight,bad,det,
  sv_exhausted);
 sv_exhausted=0;
 for(int i=0;i<r.ntog;i++)fprintf(out,"W %d %d %d %u\n",r.tog[i].window,r.tog[i].pattern>=0 && r.tog[i].pattern<np?pp[r.tog[i].pattern].p.kind:-1,(int)r.tog[i].x,seed);
 if(r.ok){
  int last=measure_run(&r,np,pp);
  for(int i=0;i<=last;i++){
   const measure *m=&ms[i];if(!m->frames)continue;
   int easier=-1;
   if(m->kind==PK_BREATHER && i>0 && i<last)easier=m->score<(ms[i-1].score+ms[i+1].score)/2;
   fprintf(out,"M %d %d %d %d %d %d %.2f %d %d\n",m->kind,m->k,m->frames,m->toggles,m->window,m->dyn,m->score,(int)m->x0,easier);
  }
 }
 if(quad)for(int p=0;p<4;p++){
  sv_careful(seed,48+28*p,end_x,&r2);
  fprintf(out,"Q %u %d %d %d %d\n",seed,48+28*p,r2.ok,(int)r2.fail_x,r2.min_window);
 }
}

/* ---- the parent: the sums -------------------------------------------------------------------------------------------- */
static int kn[PK_COUNT],kwin[PK_COUNT],ktog[PK_COUNT],kw3[PK_COUNT];
static double kscore[PK_COUNT],kk[PK_COUNT],ksec[PK_COUNT];
static double bscore[BANDS],bk[BANDS];static int bn[BANDS];
static int breath_ok,breath_n,narrow_windows,windows_total,win_hist[16];
static double sxy,sx,sy,sxx,syy;static int sn;

int main(int argc,char **argv){
 int seeds=400,first=1,quad=40,jobs=1;int32_t end_x=40000;const char *csv=NULL;
 for(int i=1;i<argc;i++){
  if(!strcmp(argv[i],"--seeds") && i+1<argc)seeds=atoi(argv[++i]);
  else if(!strcmp(argv[i],"--first") && i+1<argc)first=atoi(argv[++i]);
  else if(!strcmp(argv[i],"--quad") && i+1<argc)quad=atoi(argv[++i]);
  else if(!strcmp(argv[i],"--end") && i+1<argc)end_x=atoi(argv[++i]);
  else if(!strcmp(argv[i],"--jobs") && i+1<argc)jobs=atoi(argv[++i]);
  else if(!strcmp(argv[i],"--report") && i+1<argc)rep=fopen(argv[++i],"w");
  else if(!strcmp(argv[i],"--csv") && i+1<argc)csv=argv[++i];
 }
 if(jobs<1)jobs=1;
 if(jobs>64)jobs=64;
 say("Mouse Espresso validator: %d seeds to x %d (score %d), 1 player; %d of them also in the 4 slots; %d jobs\n",
  seeds,(int)end_x,(int)end_x/SCORE_PX,quad,jobs);
 struct timespec t0,t1;clock_gettime(CLOCK_MONOTONIC,&t0);
 char tmpl[64];snprintf(tmpl,sizeof tmpl,"/tmp/me_validate_%d",(int)getpid());
 pid_t pid[64];
 for(int j=0;j<jobs;j++){
  pid[j]=fork();
  if(pid[j]==0){
   char path[96];snprintf(path,sizeof path,"%s.%d",tmpl,j);FILE *out=fopen(path,"w");if(!out)_exit(2);
   sv_init();
   for(int s=first+j;s<first+seeds;s+=jobs){run_seed(out,(uint32_t)s,end_x,s<first+quad);fflush(out);
    if(!j && (s-first)/jobs%25==24){printf("  .. %d seeds\n",s-first+1);fflush(stdout);}}
   fclose(out);_exit(0);
  }
 }
 int bad_exit=0;
 for(int j=0;j<jobs;j++){int st;waitpid(pid[j],&st,0);if(!WIFEXITED(st) || WEXITSTATUS(st))bad_exit++;}
 if(bad_exit)fail("%d workers failed\n",bad_exit);
 FILE *cf=csv?fopen(csv,"w"):NULL;
 if(cf)fprintf(cf,"kind,k,x0,frames,changes,window,dynamic,score\n");
 long exhausted=0;int nseed=0,stuck=0,min_window=99,peak=0,weave_fail=0,easy_fail=0,biscuit_bad=0,det_bad=0,qn=0,qbad=0;
 for(int j=0;j<jobs;j++){
  char path[96],line[256];snprintf(path,sizeof path,"%s.%d",tmpl,j);FILE *in=fopen(path,"r");if(!in)continue;
  while(fgets(line,sizeof line,in)){
   if(line[0]=='S'){unsigned s;int ok,fx,mw,pk,early,tight,bad,det;long ex=0;
    if(sscanf(line+2,"%u %d %d %d %d %d %d %d %d %ld",&s,&ok,&fx,&mw,&pk,&early,&tight,&bad,&det,&ex)!=10)continue;
    exhausted+=ex;
    nseed++;
    if(!ok){stuck++;fail("seed %u: stuck at x %d\n",s,fx);}
    if(mw<min_window)min_window=mw;
    if(pk>peak)peak=pk;
    if(early<2)weave_fail++;
    easy_fail+=tight;biscuit_bad+=bad;
    if(bad)fail("seed %u: %d biscuits inside items\n",s,bad);
    if(!det){det_bad++;fail("seed %u: not deterministic\n",s);}
   }else if(line[0]=='W'){int w,k,x;unsigned sd=0;if(sscanf(line+2,"%d %d %d %u",&w,&k,&x,&sd)!=4)continue;
    windows_total++;win_hist[w<15?w:15]++;if(w<=4)narrow_windows++;
    if(k>=0 && w<=3)kw3[k]++;
    if(w<3)fail("seed %u: a %d-frame window at x %d (%s)\n",sd,w,x,k>=0?KIND[k]:"?");
   }else if(line[0]=='M'){measure m;int easier,x0;memset(&m,0,sizeof m);
    if(sscanf(line+2,"%d %d %d %d %d %d %lf %d %d",&m.kind,&m.k,&m.frames,&m.toggles,&m.window,&m.dyn,&m.score,&x0,&easier)!=9)continue;
    kn[m.kind]++;kscore[m.kind]+=m.score;kk[m.kind]+=m.k;ksec[m.kind]+=m.frames/60.0;ktog[m.kind]+=m.toggles;
    if(!kwin[m.kind] || m.window<kwin[m.kind])kwin[m.kind]=m.window;
    int b=(x0-HAZARD_FIRST)/BAND_PX;if(b>=0 && b<BANDS){bscore[b]+=m.score;bk[b]+=m.k;bn[b]++;}
    if(m.kind!=PK_BREATHER){sx+=m.k;sy+=m.score;sxy+=m.k*m.score;sxx+=(double)m.k*m.k;syy+=m.score*m.score;sn++;}
    if(easier>=0){breath_n++;breath_ok+=easier;}
    if(cf)fprintf(cf,"%s,%d,%d,%d,%d,%d,%d,%.2f\n",KIND[m.kind],m.k,x0,m.frames,m.toggles,m.window,m.dyn,m.score);
   }else if(line[0]=='Q'){unsigned s;int slot,ok,fx,mw;if(sscanf(line+2,"%u %d %d %d %d",&s,&slot,&ok,&fx,&mw)!=5)continue;
    qn++;if(!ok || mw<3){qbad++;fail("seed %u, slot %d: %s (x %d, window %d)\n",s,slot,ok?"tight":"stuck",fx,mw);}
   }
  }
  fclose(in);remove(path);
 }
 if(cf)fclose(cf);
 clock_gettime(CLOCK_MONOTONIC,&t1);
 say("\npassable: %d of %d seeds to x %d; four-player slots: %d of %d runs\n",nseed-stuck,nseed,(int)end_x,qn-qbad,qn);
 say("windows: %d changes of input; the tightest %d frames; %.1f%% at 3-4 frames\n",windows_total,min_window,
  100.0*narrow_windows/(windows_total?windows_total:1));
 say("  histogram (frames: changes):");for(int i=0;i<16;i++)if(win_hist[i])say(" %d%s:%d",i,i==15?"+":"",win_hist[i]);say("\n");
 say("\n%-10s %7s %6s %7s %8s %8s %9s\n","pattern","count","mean k","score","tightest","3-frame","changes/s");
 for(int k=0;k<PK_COUNT;k++)if(kn[k])say("%-10s %7d %6.0f %7.1f %8d %8d %9.2f\n",KIND[k],kn[k],kk[k]/kn[k],kscore[k]/kn[k],kwin[k],kw3[k],ktog[k]/ksec[k]);
 say("\nthe curve (the mean measured score of the patterns per %d px; the score is the distance / %d):\n",BAND_PX,SCORE_PX);
 double prev=-1;int dips=0;
 for(int b=0;b<BANDS;b++)if(bn[b] && HAZARD_FIRST+(b+1)*BAND_PX<=end_x){double m=bscore[b]/bn[b];
  say("  x %5d-%5d (score %4d-%4d): target k %5.1f, measured %5.1f (%d patterns)\n",HAZARD_FIRST+b*BAND_PX,
   HAZARD_FIRST+(b+1)*BAND_PX,(HAZARD_FIRST+b*BAND_PX)/SCORE_PX,(HAZARD_FIRST+(b+1)*BAND_PX)/SCORE_PX,bk[b]/bn[b],m,bn[b]);
  if(prev>=0 && m<prev-2.0)dips++;
  prev=m;}
 double corr=(sn*sxy-sx*sy)/(1e-9+sqrt((sn*sxx-sx*sx)*(sn*syy-sy*sy)));
 say("  the director's k against the measured score: correlation %.2f over %d patterns\n",corr,sn);
 say("  breathers easier than their neighbours: %d of %d\n",breath_ok,breath_n);
 say("  the start: %d seeds without 2 changes of input in the first 5 s; %d windows under 6 frames before x %d\n",
  weave_fail,easy_fail,HAZARD_FIRST+EASY_PX);
 say("  the item ring's peak: %d of %d; biscuits inside items: %d; determinism failures: %d\n",peak,COURSE_N,biscuit_bad,det_bad);
 say("  the search gave up on %ld questions (each counted as \"no\")\n",exhausted);
 say("  %.1f s\n",(double)(t1.tv_sec-t0.tv_sec)+(t1.tv_nsec-t0.tv_nsec)/1e9);
 if(min_window<3)fail("windows under 3 frames\n");
 if(dips)fail("the measured curve falls in %d bands\n",dips);
 if(corr<0.5)fail("the measured score does not follow the target (%.2f)\n",corr);
 if(breath_n && breath_ok*10<breath_n*9)fail("breathers are not easier than their neighbours\n");
 if(weave_fail*20>nseed)fail("the start does not weave\n");
 if(easy_fail)fail("the start is not easy\n");
 if(peak>=COURSE_N-64)fail("the item ring is too full\n");
 if(nseed!=seeds)fail("%d of %d seeds reported\n",nseed,seeds);
 say(failures?"\nvalidator: %d FAILURES\n":"\nvalidator: all passed\n",failures);
 if(rep)fclose(rep);
 return failures?1:0;
}
