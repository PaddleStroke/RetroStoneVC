#!/usr/bin/env python3
"""Real input flow, split play, pause, score persistence and pixel determinism.

Widescreen (DESIGN.md "Widescreen"): at the width of RS_WIDTH (320 by default) the same checks, the pictures as wide
as the screen (their PNG width is checked) and, wider than 320, the garden going on in the margins beside the stage;
the shots go to build/radishrascals-review-<width>. --screenshots at 320 writes the classic screenshots, at another
width title-<width>, play-<width> and gameover-<width>."""
import os,re,struct,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3];ID=Path(__file__).resolve().parents[1].name
EXE=ROOT/('build/host/'+ID+'_headless');WIDTH=int(os.environ.get('RS_WIDTH') or 320)
OUT=ROOT/('build/'+ID+'-review'+('' if WIDTH==320 else '-%d'%WIDTH))
OUT.mkdir(parents=True,exist_ok=True)
def run(name,frames,script='',opts=(),shot=False,sram=None):
 p=OUT/(name+'.input');p.write_text(script)
 args=[str(EXE),'--frames',str(frames),'--input',str(p),'--opt','seed=4','--opt','strict=1','--opt','dump=1','--opt','music=0']
 for opt in opts:args+=['--opt',opt]
 if WIDTH!=320:args+=['--width',str(WIDTH)]
 if shot:args+=['--png',str(OUT/(name+'.png')),'--scale','3']
 if sram:args+=['--sram',str(sram)]
 r=subprocess.run(args,cwd=ROOT,capture_output=True,text=True,check=True)
 assert 'rs strict' not in r.stderr,r.stderr
 if shot:
  png=(OUT/(name+'.png')).read_bytes();assert struct.unpack('>II',png[16:24])==(WIDTH*3,720),name+': the picture is the screen'
 s=re.search(r'state: st=(\d+).*score=(\d+) score2=.*best=(\d+).*paused=(\d+) hash=([0-9a-f]+)',r.stderr)
 assert s,r.stderr
 return s.groups(),r.stderr
def ok(name,condition):assert condition,name;print('  ok  '+name,flush=True)
def margins(name):
 # wider than 320: the stage is centred and the garden goes on beside it (many colours, not a flat fill or a bar)
 if WIDTH==320:return
 from PIL import Image
 im=Image.open(OUT/(name+'.png')).convert('RGB');m=(WIDTH-320)//2*3
 for x0 in (0,im.width-m):
  n=len(set(im.crop((x0,0,x0+m,720)).getdata()))
  ok('%s at %d: the garden goes on in the %s margin (%d colours)'%(name,WIDTH,'left' if x0==0 else 'right',n),n>=12)
s,_=run('title',80,shot=True);ok('title waits for input',s[0]=='0');margins('title')
s,_=run('start',40,'30 tap A\n');ok('A starts directly',s[0]=='1')
a,_=run('correct-bed',120,'30 tap A\n80 tap UP\n')
b,_=run('wrong-bed',120,'30 tap A\n80 tap RIGHT\n')
ok('D-pad matches the warned bed',int(a[1])>int(b[1]) and a[0]=='1')
s,_=run('first-miss',190,'30 tap A\n');ok('the first missed rabbit ends the game',s[0]=='3' and s[1]=='0')
s,_=run('first-wrong',140,'30 tap A\n80 tap RIGHT\n');ok('the first wrong direction ends the game',s[0]=='3' and s[1]=='0')
s,_=run('gameover',900,'30 tap A\n',shot=True);ok('mistimed or idle control ends a run',s[0]=='3');margins('gameover')
s,_=run('retry',920,'30 tap A\n900 tap A\n');ok('results return to the title',s[0]=='0')
s,_=run('again',945,'30 tap A\n900 tap A\n930 tap A\n');ok('another press starts another run',s[0]=='1')
for n in (1,2,3,4):
 s,out=run('play-'+str(n),1000,opts=['bot='+str(n)],shot=True)
 scores=re.search(r'scores=([0-9,]+)',out).group(1).split(',')
 ok(str(n)+' players survive and score with ordinary controls',s[0]=='1' and all(int(x)>50 for x in scores))
 if n==1:margins('play-1')
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
if '--screenshots' in sys.argv:
 dest=ROOT/('games/'+ID+'/docs/screenshots');dest.mkdir(parents=True,exist_ok=True)
 names=('title','title-4','play-1','play-2','play-3','play-4','pause-1','pause-4','gameover','results-4')
 if WIDTH!=320:names=(('title','title-%d'%WIDTH),('play-1','play-%d'%WIDTH),('gameover','gameover-%d'%WIDTH))
 for name in names:
  src,dst=name if isinstance(name,tuple) else (name,name)
  (dest/(dst+'.png')).write_bytes((OUT/(src+'.png')).read_bytes())
print('game checks: all passed')
