#!/usr/bin/env python3
"""Real input flow, shared-course play, pause, score persistence and pixel determinism.

Widescreen: the width is the environment's RS_WIDTH (the headless reads it; 320 by default), the pictures go to
build/penguinslalom-review-<width>; at a wider width the 320 columns of the course are the 320 picture's, pixel for
pixel, and the forest fills the margins (DESIGN.md "Widescreen"). --screenshots: NAME.png at 320, NAME-<width>.png
(title, play, gameover) wider."""
import os,re,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3];ID=Path(__file__).resolve().parents[1].name
W=int(os.environ.get('RS_WIDTH') or 320)
EXE=ROOT/('build/host/'+ID+'_headless');OUT=ROOT/('build/'+ID+'-review'+('' if W==320 else '-'+str(W)))
OUT.mkdir(parents=True,exist_ok=True)
SHOTS={}
def run(name,frames,script='',opts=(),shot=False,sram=None):
 p=OUT/(name+'.input');p.write_text(script)
 args=[str(EXE),'--frames',str(frames),'--input',str(p),'--opt','seed=4','--opt','strict=1','--opt','dump=1','--opt','music=0']
 for opt in opts:args+=['--opt',opt]
 if shot:SHOTS[name]=list(args);args+=['--png',str(OUT/(name+'.png')),'--scale','3']
 if sram:args+=['--sram',str(sram)]
 r=subprocess.run(args,cwd=ROOT,capture_output=True,text=True,check=True)
 assert 'rs strict' not in r.stderr,r.stderr
 s=re.search(r'state: st=(\d+).*score=(\d+) score2=.*best=(\d+).*paused=(\d+) hash=([0-9a-f]+)',r.stderr)
 assert s,r.stderr
 return s.groups(),r.stderr
def ok(name,condition):assert condition,name;print('  ok  '+name,flush=True)
s,_=run('title',80,shot=True);ok('title waits for input',s[0]=='0')
s,_=run('start',40,'30 tap A\n');ok('A starts directly',s[0]=='1')
a,_=run('left',50,'30 tap A\n34 LEFT\n')
b,_=run('right',50,'30 tap A\n34 RIGHT\n')
ok('real Left and Right inputs steer in opposite directions',a[0]=='1' and b[0]=='1' and a[4]!=b[4])
a,_=run('both-directions',50,'30 tap A\n34 LEFT+RIGHT\n')
b,_=run('no-direction',50,'30 tap A\n')
ok('opposite directions together stop movement',a==b)
s,_=run('gameover',900,'30 tap A\n',shot=True);ok('mistimed or idle control ends a run',s[0]=='3')
s,_=run('retry',920,'30 tap A\n900 tap A\n');ok('results return to the title',s[0]=='0')
s,_=run('again',945,'30 tap A\n900 tap A\n930 tap A\n');ok('another press starts another run',s[0]=='1')
for n in (1,2,3,4):
 s,out=run('play-'+str(n),1000,opts=['bot='+str(n)],shot=True)
 scores=re.search(r'scores=([0-9,]+)',out).group(1).split(',')
 ok(str(n)+' players survive and score with ordinary controls',s[0]=='1' and all(int(x)>50 for x in scores))
 a,_=run('pause-'+str(n),450,'400 tap SELECT\n',opts=['bot='+str(n)],shot=True)
 b,_=run('pause-long-'+str(n),700,'400 tap SELECT\n',opts=['bot='+str(n)])
 ok(str(n)+' players pause together',a==b and a[3]=='1')
 a,_=run('det-a',750,opts=['bot='+str(n)],shot=True)
 b,_=run('det-b',750,opts=['bot='+str(n)],shot=True)
 ok(str(n)+'-player deterministic state and pixels',a==b and (OUT/'det-a.png').read_bytes()==(OUT/'det-b.png').read_bytes())
run('results-4',1600,opts=['bot=4','botstop=20'],shot=True)
_,out=run('title-4',80,'10 P2 tap A\n20 P3 tap A\n30 P4 tap A\n',shot=True)
ok('four-pad title lobby','st=0 players=4' in out)
_,out=run('leave',80,'10 P2 tap A\n30 P2 tap B\n');ok('joined pad can leave','st=0 players=1' in out)
_,out=run('p2-start',80,'10 P2 tap A\n30 P2 tap A\n');ok('only P1 starts','st=0 players=2' in out)
save=OUT/'verify.srm'
if save.exists():save.unlink()
a,_=run('save-run',1600,opts=['bot=1','botstop=20'],sram=save)
b,_=run('save-load',20,sram=save)
ok('best score persists in SRAM',int(a[2])>0 and a[2]==b[2])
if W>320:
 from PIL import Image
 M=(W-320)//2;REF=OUT/'320';REF.mkdir(exist_ok=True)
 PINE=((0x24,0x5c,0x5c),(0x3c,0x7c,0x70))
 def near(c,ref):return all(abs(c[i]-ref[i])<=10 for i in range(3))
 def pines(im,x0,x1):
  px=im.load();return sum(1 for y in range(0,im.height,3) for x in range(x0,x1,3) if any(near(px[x,y],c) for c in PINE))
 for name,args in SHOTS.items():
  if name.startswith('det-'):continue
  im=Image.open(OUT/(name+'.png')).convert('RGB')
  ok(name+': a %dx240 picture'%W,im.size==(W*3,720))
  subprocess.run(args+['--width','320','--png',str(REF/(name+'.png')),'--scale','3'],cwd=ROOT,capture_output=True,check=True)
  ref=Image.open(REF/(name+'.png')).convert('RGB')
  ok(name+': the course is the 320 picture, centred',im.crop((M*3,0,(M+320)*3,720)).tobytes()==ref.tobytes())
  left,right=pines(im,0,M*3),pines(im,(M+320)*3,W*3)
  if not name.startswith('pause'):ok(name+': the snowy forest goes on at both sides (%d, %d pine pixels)'%(left,right),left>40 and right>40)
if '--screenshots' in sys.argv:
 dest=ROOT/('games/'+ID+'/docs/screenshots');dest.mkdir(parents=True,exist_ok=True)
 names={'title':'title','play-1':'play','gameover':'gameover'} if W>320 else \
  {n:n for n in ('title','title-4','play-1','play-2','play-3','play-4','pause-1','pause-4','gameover','results-4')}
 for name,to in names.items():
  (dest/(to+('' if W==320 else '-'+str(W))+'.png')).write_bytes((OUT/(name+'.png')).read_bytes())
print('game checks: all passed')
