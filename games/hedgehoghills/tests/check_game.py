#!/usr/bin/env python3
"""Flow, held controls, pressure, pause, lobby, SRAM, determinism and visual captures."""
import hashlib,re,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
EXE=ROOT/'build/host/hedgehoghills_headless'
OUT=ROOT/'build/hedgehoghills-review'
OUT.mkdir(parents=True,exist_ok=True)
def run(name,frames,script='',opts=(),shot=None,sram=None):
 p=OUT/(name+'.input');p.write_text(script)
 args=[str(EXE),'--frames',str(frames),'--input',str(p),'--opt','dump=1','--opt','seed=4','--opt','strict=1']
 for opt in opts:args+=['--opt',opt]
 if shot:args+=['--png',str(shot),'--scale','2']
 if sram:args+=['--sram',str(sram)]
 out=subprocess.run(args,cwd=ROOT,capture_output=True,text=True,check=True).stderr
 assert 'rs strict' not in out,out
 state=re.search(r'state: st=(\d+).*score=(\d+).*best=(\d+).*scroll=(\d+) paused=(\d+).*hash=([0-9a-f]+)',out)
 assert state,out
 return state.groups(),out
def expect(name,condition):
 assert condition,name
 print('  ok  '+name,flush=True)
title,_=run('title',100,shot=OUT/'title.png');expect('title waits for input',title[0]=='0' and title[1]=='0')
start,_=run('start',40,'30 A\n');expect('A starts immediately',start[0]=='1' and int(start[3])>0)
hold,_=run('hold',1200,'30 A\n',shot=OUT/'gameover.png');expect('holding uphill loses to frost',hold[0]=='3')
coast,_=run('coast',2000,'30 tap A\n');expect('coasting loses to frost',coast[0]=='3')
retry,_=run('retry',2020,'30 tap A\n2000 tap A\n');expect('results return to title',retry[0]=='0')
again,_=run('again',2050,'30 tap A\n2000 tap A\n2030 tap A\n');expect('title starts the next run',again[0]=='1')
pa='30 tap A\n90 tap SELECT\n'
a,_=run('pause-short',120,pa,shot=OUT/'pause.png');b,_=run('pause-long',500,pa)
expect('pause freezes world and diagnostics',a[3:]==b[3:] and a[4]=='1')
join,out=run('join4',90,'10 P2 tap A\n20 P3 tap A\n30 P4 tap A\n',shot=OUT/'title-4-players.png')
expect('four-pad title lobby','players=4' in out and join[0]=='0')
_,out=run('leave',90,'10 P4 tap A\n40 P4 tap B\n');expect('a remote pad joins and leaves','players=1' in out)
_,out=run('p2-no-start',90,'10 P2 tap A\n30 P2 tap A\n');expect('only player one starts','st=0 players=2' in out)
bot,_=run('bot',1600,opts=['bot=1'],shot=OUT/'play.png');expect('timed control survives and scores',bot[0]=='1' and int(bot[1])>500)
for players in (2,3):
 _,out=run('race'+str(players),1200,opts=['bot='+str(players)],shot=OUT/('race-'+str(players)+'-players.png'))
 expect(str(players)+'-player split race stays active','players='+str(players) in out and 'st=1' in out)
mp_pause='400 tap SELECT\n'
a,_=run('split-pause-short',450,mp_pause,opts=['bot=4'],shot=OUT/'pause-4-players.png')
b,_=run('split-pause-long',700,mp_pause,opts=['bot=4'])
expect('multiplayer pause freezes all cameras and world',a==b and a[4]=='1')
_,out=run('four',1200,opts=['bot=4'],shot=OUT/'race-4-players.png')
scores=re.search(r'scores=(\d+),(\d+),(\d+),(\d+)',out)
expect('four players all score',scores and all(int(x)>0 for x in scores.groups()))
run('results4',2600,opts=['bot=4','botstop=10'],shot=OUT/'results-4-players.png')
save=OUT/'test.srm'
if save.exists():save.unlink()
first,_=run('save-run',2000,'30 tap A\n',sram=save)
loaded,_=run('save-load',20,sram=save);expect('best score persists across processes',int(first[1])>0 and first[2]==loaded[2])
for players in (1,4):
 a,_=run('det-a',1400,opts=['bot='+str(players)],shot=OUT/'det-a.png')
 b,_=run('det-b',1400,opts=['bot='+str(players)],shot=OUT/'det-b.png')
 expect(str(players)+'-player deterministic input and pixels',a==b and (OUT/'det-a.png').read_bytes()==(OUT/'det-b.png').read_bytes())
if '--screenshots' in sys.argv:
 dest=ROOT/'games/hedgehoghills/docs/screenshots';dest.mkdir(parents=True,exist_ok=True)
 for name in ('title','title-4-players','play','race-2-players','race-3-players','race-4-players','results-4-players','pause','pause-4-players','gameover'):
  (dest/(name+'.png')).write_bytes((OUT/(name+'.png')).read_bytes())
print('game checks: all passed')
