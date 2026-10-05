#!/usr/bin/env python3
"""Real frontend inputs and persistence tests, complementing standalone river physics."""
import subprocess,sys,re,pathlib
H=pathlib.Path(sys.argv[1]).resolve();O=pathlib.Path(sys.argv[2] if len(sys.argv)>2 else "build")/"otterrapids-tests";O.mkdir(parents=True,exist_ok=True)
seq=0
def run(frames,script=None,opts=(),extra=()):
 global seq
 cmd=[str(H),"--frames",str(frames),"--opt","dump=1","--opt","music=0","--opt","strict=1"]
 if script is not None:
  seq+=1;p=O/f"input-{seq}.txt";p.write_text(script);cmd += ["--input",str(p)]
 for o in opts:cmd += ["--opt",o]
 cmd += list(map(str,extra));out=subprocess.check_output(cmd,stderr=subprocess.STDOUT,text=True)
 assert "rs strict" not in out,out
 line=next(l for l in out.splitlines() if "state:" in l)
 state=dict(re.findall(r"(\w+)=([^ ]+)",line));return state,out

def check(name,condition):
 assert condition,name
 print("  ok  "+name)
a,_=run(60);b,_=run(300);check("title is the frozen actual start",a["scroll"]==b["scroll"]=="0" and a["x"]==b["x"] and b["st"]=="0")
c,_=run(40,"30 tap A\n");check("A starts immediately without regenerating title seed",c["st"]=="1" and int(c["scroll"])>0 and a["seed"]==c["seed"])
a,_=run(70,"30 tap A\n35 LEFT\n");b,_=run(70,"30 tap A\n35 RIGHT\n");check("held D-pad steers both directions",int(a["x"])<125 and int(b["x"])>195)
c,_=run(70,"30 RIGHT\n");check("natural steering input also starts",c["st"]=="1")
a,_=run(120,"30 tap A\n90 tap SELECT\n");b,_=run(300,"30 tap A\n90 tap SELECT\n");check("pause freezes travel and position",a["scroll"]==b["scroll"] and a["x"]==b["x"] and b["paused"]=="1")
c,_=run(50,"20 P4 tap A\n");check("any free pad joins without starting",c["st"]=="0" and c["players"]=="2")
c,_=run(80,"20 P2 tap A\n25 P3 tap A\n30 P4 tap A\n40 P2 tap B\n");check("lobby joins and leaves",c["players"]=="3")
c,_=run(70,"20 P2 tap A\n30 tap A\n35 RIGHT\n35 P2 LEFT\n");check("two real pads steer independently",int(c["x"])>195 and int(c["x2"])<125)
for n in (1,2,4):
 c,_=run(2500,opts=(f"bot={n}","seed=6"));check(f"{n} players survive and score",c["st"]=="1" and all(int(s)>200 for s in c["scores"].split(",")))
sram=O/"save.srm";sram.unlink(missing_ok=True)
c,out=run(3000,opts=("bot=1","seed=5","botstop=100"),extra=("--sram",sram));check("collision ends the run and stores best",c["st"]=="3" and c["runs"]=="1" and int(c["best"])>=100)
b,_=run(10,extra=("--sram",sram));check("SRAM best survives a fresh process",b["best"]==c["best"])
f=int(re.search(r"run 1 over at frame (\d+)",out).group(1));r=f+60
c,_=run(r+10,f"{r} tap A\n",opts=("bot=1","seed=5","botstop=100"));check("results A returns to title",c["st"]=="0")
for n in (1,4):
 snapshots=[]
 for k in (1,2):
  png=O/f"det-{n}-{k}.png";c,_=run(1400,opts=(f"bot={n}","seed=3"),extra=("--png",png));snapshots.append((c,png.read_bytes()))
 check(f"{n}-player state and picture determinism",snapshots[0]==snapshots[1])
print("frontend checks: all passed")
