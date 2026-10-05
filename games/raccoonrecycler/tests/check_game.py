#!/usr/bin/env python3
"""Integration checks for real input edges, actual frozen menu, pause, checked SRAM and determinism."""
import subprocess,sys,pathlib,re
from PIL import Image
H=sys.argv[1];O=pathlib.Path(sys.argv[2])/ 'raccoonrecycler-tests';O.mkdir(parents=True,exist_ok=True)
def run(frames=100,input=None,opts=(),sram=None,png=None):
 args=[H,'--frames',str(frames),'--opt','music=0','--opt','dump=1','--opt','strict=1']
 for opt in opts:args+=['--opt',opt]
 if input:
  p=O/'input.txt';p.write_text(input);args+=['--input',str(p)]
 if sram:args+=['--sram',str(sram)]
 if png:args+=['--png',str(png)]
 r=subprocess.run(args,capture_output=True,text=True);assert r.returncode==0,r.stderr
 text=r.stdout+r.stderr;assert 'rs strict' not in text,text
 line=[l for l in text.splitlines() if 'state:' in l][-1];return line,text
def field(line,key):return int(re.search(r'\b'+key+r'=(\d+)',line)[1])
a,_=run(40,opts=['seed=7']);b,_=run(400,opts=['seed=7']);assert field(a,'scroll')==field(b,'scroll')==0
s,_=run(40,'30 tap A\n',opts=['seed=7']);assert field(s,'st')==1 and field(s,'scroll')==10*256
s,_=run(60,'20 P4 tap A\n',opts=['seed=7']);assert field(s,'players')==2 and field(s,'st')==0
s,_=run(60,'20 P2 tap A\n25 P3 tap A\n30 P4 tap A\n40 P2 tap B\n');assert field(s,'players')==3
# Seed 7 starts with a yellow duck. Selection is judged only at the lower junction.
s,_=run(90,'30 tap A\n60 tap RIGHT\n',opts=['seed=7']);assert field(s,'score')==0,s
s,_=run(160,'30 tap A\n60 RIGHT\n',opts=['seed=7']);assert field(s,'score')==0,s
s,_=run(210,'30 tap A\n60 tap RIGHT\n',opts=['seed=7']);assert field(s,'st')==3 and field(s,'score')==0 and field(s,'runs')==1,s
s,_=run(160,'30 tap A\n60 tap RIGHT\n120 tap LEFT\n',opts=['seed=7']);assert field(s,'score')==1,s
# Seed 17 starts with a yellow sea star: Up and Down both select its centre bin.
for button in ['UP','DOWN']:
 s,_=run(160,f'30 tap A\n60 tap {button}\n',opts=['seed=17']);assert field(s,'score')==1,s
# Compare every opaque duck pixel with its asset: treads must never overwrite it.
layer_png=O/'item-layering.png'
run(90,'30 tap A\n',opts=['seed=7'],png=layer_png)
shot=Image.open(layer_png).convert('RGB')
sheet=Image.open(pathlib.Path(__file__).resolve().parents[1]/'art'/'sprites.png').convert('RGB')
for y in range(32):
 for x in range(32):
  rgb=sheet.getpixel((x,32+y))
  if rgb==(255,0,255):continue
  r,g,b=[c>>3 for c in rgb];g=(g<<1)|(g>>4)
  expected=((r<<3)|(r>>2),(g<<2)|(g>>4),(b<<3)|(b>>2))
  assert shot.getpixel((144+x,92+y))==expected,(x,y,shot.getpixel((144+x,92+y)),expected)
# Bin signs must stay intact while a routed toy drops behind the front panel.
run(166,'30 tap A\n',opts=['seed=7'],png=layer_png)
shot=Image.open(layer_png).convert('RGB')
for y in range(32):
 for x in range(32):
  rgb=sheet.getpixel((x,64+y))
  if rgb==(255,0,255):continue
  r,g,b=[c>>3 for c in rgb];g=(g<<1)|(g>>4)
  expected=((r<<3)|(r>>2),(g<<2)|(g>>4),(b<<3)|(b>>2))
  assert shot.getpixel((80+x,197+y))==expected,(x,y,'bin badge obscured')
s,_=run(6000,'30 tap A\n',opts=['seed=7']);assert field(s,'st')==3 and field(s,'runs')==1
s,_=run(6020,'30 tap A\n6000 tap A\n',opts=['seed=7']);assert field(s,'st')==0 and field(s,'players')==1
p='30 tap A\n90 tap SELECT\n';a,_=run(120,p,opts=['seed=7']);b,_=run(400,p,opts=['seed=7']);assert field(a,'paused')==field(b,'paused')==1 and field(a,'scroll')==field(b,'scroll')
sram=O/'save.srm';sram.unlink(missing_ok=True);run(2500,opts=['bot=1','botstop=7','seed=7'],sram=sram);s,_=run(10,sram=sram);assert field(s,'best')==7,s
bad=O/'bad.srm';bad.write_bytes(bytes([0xff])*8192);s,_=run(10,sram=bad);assert field(s,'best')==0
for players in [1,4]:
 a,_=run(5000,opts=[f'bot={players}','seed=9'],png=O/f'det{players}a.png');b,_=run(5000,opts=[f'bot={players}','seed=9'],png=O/f'det{players}b.png');assert a==b and (O/f'det{players}a.png').read_bytes()==(O/f'det{players}b.png').read_bytes()
 assert field(a,'score')>=20 and field(a,'st')==1,a
print('integration: frozen title, persistent junction, arrival scoring, single-miss elimination, lobby, pause, checked SRAM and determinism all passed')

