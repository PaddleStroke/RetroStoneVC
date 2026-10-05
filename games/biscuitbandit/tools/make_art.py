#!/usr/bin/env python3
"""Code-drawn raccoon cafe: shaded sprites and tiled scenery in the house palette."""
import argparse, os, sys, math
from PIL import Image
HERE=os.path.dirname(os.path.abspath(__file__)); GAME=os.path.dirname(HERE); ROOT=os.path.abspath(os.path.join(GAME,'..','..'))
sys.path.insert(0,os.path.join(ROOT,'games','common','tools'))
import house_style as hs
Canvas=hs.Canvas
SHEET_W,SHEET_H=256,192
SPRITES=[('hero_idle',0,0,32,32,2,'hero'),('hero_squash',64,0,32,32,1,'hero'),('hero_stretch',96,0,32,32,1,'hero'),('hero_air',128,0,32,32,1,'hero'),('hero_hit',160,0,32,32,1,'hero'),('hero_icon',192,0,16,16,1,'hero'),('tray',0,80,48,16,3,'props'),('biscuit',0,56,16,8,3,'props'),('cup',64,56,16,16,1,'props'),('basket',96,40,32,32,1,'props'),('chute',144,40,32,24,1,'props'),('puff',192,40,8,8,3,'props'),('heart',224,40,8,8,1,'props'),('warning',224,56,16,16,1,'props'),('belt',0,104,32,8,4,'props'),('oven',0,128,48,64,1,'oven')]
PANORAMAS={'ground':('tiles/ground.png',512,32),'hills':('tiles/hills.png',512,208),'hills_wide':('tiles/hills_wide.png',1024,208)}
OUT=(32,28,48); FUR=[(214,221,213),(173,185,180),(113,128,135),(70,80,101)]; MASK=(46,44,62); CREAM=(255,246,220); APRON=hs.ramp('pink'); WOOD=hs.ramp('wood'); IRON=hs.ramp('iron'); IRON=IRON+[IRON[-1]] if len(IRON)==3 else IRON; SAND=hs.ramp('sand')
def hero(pose='idle',frame=0):
 c=Canvas(32,32); b=1 if frame else 0
 # Broad striped tail behind a small apron-clad body.
 hs.ellipse(c,7,23,5,7,FUR)
 for y in (19,24,28):c.rect(3,y,9,y+1,MASK)
 for x in (13,22):hs.ellipse(c,x,29,3,2,[FUR[2],FUR[3],MASK,MASK])
 hs.ellipse(c,18,23+b,7,7,FUR);c.rect(13,22+b,23,28,APRON[1]);c.rect(14,23+b,20,26,APRON[0]);c.rect(18,26,21,28,APRON[2])
 hs.ellipse(c,11,8+b,3,4,FUR);hs.ellipse(c,24,8+b,3,4,FUR);c.rect(11,7+b,12,9+b,APRON[2]);c.rect(23,7+b,24,9+b,APRON[2])
 hs.ellipse(c,18,14+b,9,8,FUR)
 hs.ellipse(c,14,13+b,4,3,[MASK]*4);hs.ellipse(c,23,13+b,4,3,[MASK]*4)
 if pose=='hit':
  for ex in (12,21):
   for k in range(3):c.set(ex+k,12+k+b,CREAM);c.set(ex+2-k,12+k+b,CREAM)
 else:
  hs.eye(c,12,11+b,look=(1,0),big=True);hs.eye(c,21,11+b,look=(0,0),big=True)
 hs.ellipse(c,19,18+b,4,2,[CREAM,FUR[0],FUR[1],FUR[2]]);c.rect(18,16+b,20,17+b,MASK);c.set(20,20+b,MASK)
 # Hands visibly support the wide tray above the head.
 hs.ellipse(c,6,4,3,2,FUR);hs.ellipse(c,28,4,3,2,FUR)
 c.outline(OUT);return c
def icon():
 c=Canvas(16,16);hs.ellipse(c,3,4,2,3,FUR);hs.ellipse(c,12,4,2,3,FUR);hs.ellipse(c,8,8,7,6,FUR);c.rect(3,5,12,8,MASK);hs.eye(c,4,5);hs.eye(c,10,5);c.rect(7,10,9,11,MASK);c.outline(OUT);return c
def biscuit(i):
 c=Canvas(16,8);hs.ellipse(c,8,4,6.7,2.4,SAND)
 for x,y in ((4,3),(9,2),(11,4),(6,5)):c.set(x,y,WOOD[2])
 if i==1:c.rect(4,3,11,3,WOOD[1])
 if i==2:c.set(7,3,APRON[1]);c.set(8,4,APRON[2])
 c.outline(OUT);return c
def sprite(name,i):
 if name=='hero_icon':return icon()
 if name.startswith('hero_'):return hero(name[5:],i)
 if name=='biscuit':return biscuit(i)
 if name=='tray':
  c=Canvas(48,16)
  for x in range(1,47):
   y=4+(x-24)*(i-1)//10;c.rect(x,y,x,y+3,IRON[1]);c.set(x,y,IRON[0]);c.set(x,y+3,IRON[3])
  c.outline(OUT);return c
 if name=='basket':
  c=Canvas(32,32);hs.ellipse(c,16,12,14,5,WOOD);c.rect(3,12,28,25,WOOD[1]);c.rect(5,17,26,25,WOOD[2]);
  for x in range(5,28,4):c.rect(x,14,x,24,WOOD[0]);
  for y in (16,21,25):c.rect(3,y,28,y,WOOD[3])
  c.rect(10,17,21,22,CREAM);c.rect(13,19,18,20,WOOD[2]);c.outline(OUT);return c
 if name=='cup':
  c=Canvas(16,16);hs.ellipse(c,12,7,3,4,IRON);c.rect(10,5,12,8,OUT);c.rect(2,4,10,11,IRON[0]);c.rect(7,5,10,11,IRON[1]);hs.ellipse(c,6,4,4,1,IRON);c.rect(3,2,8,2,WOOD[2]);c.rect(2,12,11,13,IRON[2]);c.outline(OUT);return c
 if name=='chute':
  c=Canvas(32,24);c.rect(1,1,30,10,IRON[2]);c.rect(2,2,28,4,IRON[0]);c.rect(10,9,22,17,IRON[3]);c.rect(11,10,21,14,OUT);c.rect(10,17,22,19,IRON[1]);c.rect(3,7,6,9,APRON[1]);c.outline(OUT);return c
 if name=='warning':
  c=Canvas(16,16)
  for y in range(2,14):c.rect(8-y//2,y,8+y//2,y,SAND[0])
  c.rect(7,5,8,9,WOOD[3]);c.rect(7,11,8,12,WOOD[3]);c.outline(OUT);return c
 if name=='belt':
  c=Canvas(32,8);c.rect(0,0,31,7,IRON[2]);c.rect(0,0,31,0,IRON[0]);c.rect(0,7,31,7,OUT)
  for y in range(1,7):
   for x in range(32):
    if (x+i*2+y//2)%8<2:c.set(x,y,IRON[0] if y<4 else IRON[1])
  return c
 if name=='oven':return oven()
 if name=='heart':
  c=Canvas(8,8);hs.ellipse(c,2.5,2.5,2,2,APRON);hs.ellipse(c,5,2.5,2,2,APRON);c.rect(2,3,5,5,APRON[1]);c.rect(3,6,4,6,APRON[2]);c.outline(OUT);return c
 c=Canvas(8,8)
 for x,y in ((1,2),(4,0),(6,4),(2,6)):c.set(x,y,CREAM)
 return c
def ground():
 c=Canvas(512,32)
 for y in range(32):
  for x in range(512):
   color=WOOD[0] if y==0 else WOOD[3] if y==1 else WOOD[1] if (y//8+x//32)%2==0 else WOOD[2]
   if y%8==7 or (x+(y//8%2)*16)%32==0:color=WOOD[3]
   c.set(x,y,color)
 return c
def hills():
 c=Canvas(512,208); wall=[(224,205,177),(208,184,161),(180,154,135),(139,115,112)]; teal=[(177,206,195),(144,181,178),(101,145,151),(76,112,134)]
 c.rect(0,0,511,207,wall[0]);c.rect(0,110,511,207,wall[1]);c.rect(0,110,511,113,wall[3]);c.rect(0,114,511,117,wall[2])
 # Two tall cafe windows, muted garden silhouettes, wood frames.
 for bx in (18,240,386):
  c.rect(bx,60,bx+62,146,wall[3]);c.rect(bx+3,63,bx+59,142,teal[0]);c.rect(bx+3,110,bx+59,142,teal[1])
  for x in range(bx+5,bx+57,13):c.rect(x,116,x+5,141,teal[2]);c.rect(x+2,106,x+4,124,teal[2])
  c.rect(bx+30,63,bx+32,142,wall[3]);c.rect(bx+3,100,bx+59,102,wall[3]);c.rect(bx-3,146,bx+66,150,wall[2]);c.rect(bx-3,146,bx+66,147,wall[0])
 # Bakery cabinet and coffee machine. Shapes sit on their shelves.
 c.rect(99,78,208,83,wall[3]);c.rect(99,78,208,79,wall[0]);c.rect(108,84,112,96,wall[2]);c.rect(196,84,200,96,wall[2])
 for bx in (113,136,159):c.rect(bx,63,bx+13,76,teal[2]);c.rect(bx+2,64,bx+11,65,teal[0]);c.rect(bx+4,70,bx+9,74,wall[0])
 c.rect(185,61,199,77,wall[2]);c.rect(187,63,197,67,wall[0]);c.rect(190,68,193,72,wall[3])
 c.rect(96,137,211,141,wall[3]);c.rect(100,142,207,188,teal[2]);c.rect(106,148,146,180,teal[3]);c.rect(154,148,201,180,teal[3]);c.rect(109,150,143,177,teal[2]);c.rect(157,150,198,177,teal[2]);c.rect(136,159,140,160,wall[0]);c.rect(159,159,163,160,wall[0]);c.rect(103,189,107,207,wall[3]);c.rect(200,189,204,207,wall[3])
 c.rect(121,111,158,136,wall[2]);c.rect(123,112,155,116,wall[0]);c.rect(127,118,152,127,wall[3]);c.rect(130,120,149,124,teal[1]);c.rect(131,128,134,134,wall[3]);c.rect(147,128,150,134,wall[3]);c.rect(127,135,152,136,teal[3])
 # Fixed ceiling rail; animated belt slats are drawn in front at runtime.
 c.rect(0,38,511,44,teal[3]);c.rect(0,38,511,39,teal[0]);c.rect(0,45,511,50,wall[3]);
 for x in range(0,512,12):c.rect(x,45,x+4,46,wall[0]);c.rect(x+6,48,x+10,49,wall[2])
 c.rect(0,202,511,207,wall[3]);c.rect(0,202,511,203,wall[0]);return c

# ---- widescreen (DESIGN.md "Widescreen") ----------------------------------------------------------------------
# The cookie oven at the belt's right end: a screen wider than 320 shows where the cookies come from. A sprite on
# its own palette (OBJ 6), drawn in front of the cookies at stage x 320 (just right of the 320-px stage), its
# rubber curtain over the mouth they come out of; its flue goes up through the top of the screen.
def oven():
 c=Canvas(48,64); I=IRON; P=APRON; S=SAND; o=16
 c.rect(29,0,36,o+4,I[1]);c.rect(30,0,31,o+4,I[0]);c.rect(35,0,36,o+4,I[2])   # the flue
 c.rect(27,8,38,10,I[2]);c.rect(27,8,38,8,I[0])
 hs.ellipse(c,26,o+26,21,20,[I[0],I[1],I[1],I[2]])                 # the round chrome body
 c.rect(5,o+14,47,o+47,I[1]);c.rect(5,o+14,47,o+15,I[0]);c.rect(5,o+44,47,o+47,I[2])
 c.rect(3,o+10,47,o+13,P[1]);c.rect(3,o+10,47,o+10,P[0]);c.rect(3,o+13,47,o+13,P[2])   # the pink awning
 for x in range(5,47,8):c.rect(x,o+11,x+3,o+12,P[0])
 c.rect(0,o+16,13,o+39,OUT)                                        # the mouth, its rubber curtain
 for x in range(1,13,3):c.rect(x,o+17,x+1,o+38,I[2])
 c.rect(0,o+40,13,o+41,I[2]);c.rect(0,o+15,13,o+15,I[2])
 hs.ellipse(c,31,o+27,9,9,[I[0],I[1],I[2],OUT])                     # the porthole: cookies baking inside
 hs.ellipse(c,31,o+27,6,6,[S[0],S[1],S[2],WOOD[1]])
 for x,y in ((28,25),(33,24),(30,30),(35,29)):c.set(x,o+y,WOOD[2])
 c.set(29,o+24,CREAM);c.set(28,o+25,CREAM)
 hs.ellipse(c,43,o+22,2,2,[S[0],WOOD[0],WOOD[1],WOOD[2]])          # the dial and the warm lamp
 c.rect(17,o+42,44,o+43,WOOD[2]);c.rect(17,o+42,44,o+42,WOOD[0])
 for x in (19,42):c.set(x,o+45,I[0])
 c.outline(OUT)
 for x in range(48):c.p[0][x]=c.p[1][x]                            # the flue runs on through the screen's top
 return c

WALL=[(224,205,177),(208,184,161),(180,154,135),(139,115,112)]; TEAL=[(177,206,195),(144,181,178),(101,145,151),(76,112,134)]
def hills_wide():
 """1024x208: the classic room over stage x 0..327 (pixel for pixel), then the room goes on to the right (stage
 320..440: a bread rack under the oven, a tall fern) and to the left (image x 904..1023 = stage -120..-1: a hanging
 lamp over a fern, the cafe's door). Stage x s is image x s & 1023; the rails keep their 12-px rhythm."""
 W=1024; c=Canvas(W,208); wall,teal=WALL,TEAL; S=SAND; base=hills()
 c.rect(0,0,W-1,207,wall[0]);c.rect(0,110,W-1,207,wall[1]);c.rect(0,110,W-1,113,wall[3]);c.rect(0,114,W-1,117,wall[2])
 c.rect(0,38,W-1,44,teal[3]);c.rect(0,38,W-1,39,teal[0]);c.rect(0,45,W-1,50,wall[3])
 for x in range(W):
  s=x if x<512 else x-W
  if s%12<=4:c.set(x,45,wall[0]);c.set(x,46,wall[0])
  if 6<=s%12<=10:c.set(x,48,wall[2]);c.set(x,49,wall[2])
 c.rect(0,202,W-1,207,wall[3]);c.rect(0,202,W-1,203,wall[0])
 for y in range(208):
  for x in range(328):c.p[y][x]=base.p[y][x]
 X=lambda s:s&(W-1)
 def wood_rect(x0,y0,x1,y1):c.rect(X(x0),y0,X(x1),y1,WOOD[1]);c.rect(X(x0),y0,X(x1),y0,WOOD[0]);c.rect(X(x0),y1,X(x1),y1,WOOD[3])
 # right: the bread rack (under the oven sprite), loaves and baguettes on three shelves
 for x0 in (326,368):c.rect(x0,66,x0+3,201,WOOD[2]);c.rect(x0,66,x0,201,WOOD[1])
 for sy in (98,132,166,198):wood_rect(324,sy,373,sy+3)
 for i,lx in enumerate((334,349,362)):
  hs.ellipse(c,lx,92,6,5,[S[0],S[1],S[2],WOOD[1]]);c.rect(lx-3,89,lx-2,89,WOOD[1]);c.rect(lx+1,90,lx+2,90,WOOD[1])
 for y in (122,126):hs.capsule(c,331,y,365,y-2,3,[S[0],S[1],S[2],WOOD[1]])
 for x in range(334,366,8):c.set(x,121,WOOD[1]);c.set(x+3,125,WOOD[1])
 for lx in (336,357):hs.ellipse(c,lx,159,9,6,[S[0],S[1],WOOD[0],WOOD[1]]);c.rect(lx-4,156,lx+4,156,S[0])
 for lx in (333,346,359):hs.ellipse(c,lx,193,5,4,[S[0],S[1],S[2],WOOD[1]])
 def fern(cx,by,fronds):
  """a pot plant: long pointed leaves from (cx, by), each (angle from the vertical in degrees, length, half-width,
  bend per px: outward and down), lit from the top-left, the midrib darker, outlined in the darkest teal"""
  p=Canvas(96,96); ox,oy=48,95
  for a,L,wd,k in fronds:
   x,y,th=float(ox),float(oy),math.radians(a)
   for i in range(L):
    th+=k if a>=0 else -k; x+=math.sin(th); y-=math.cos(th)
    w=wd*math.sin(math.pi*min(1.0,(i+2)/(L+1)))
    nx,ny=math.cos(th),math.sin(th)
    for j in range(-int(w*4),int(w*4)+1):
     s=j/4.0
     col=teal[2] if abs(s)<0.6 else teal[0] if s<0 else teal[1]
     p.set(int(round(x+nx*s)),int(round(y+ny*s)),col)
  p.outline(teal[3])
  for yy in range(96):
   for xx in range(96):
    if p.p[yy][xx] is not None:c.set(X(cx-ox+xx),by-oy+yy,p.p[yy][xx])
 def pot(x0,x1):
  c.rect(X(x0+2),178,X(x1-2),201,WOOD[2]);c.rect(X(x0),176,X(x1),180,WOOD[1]);c.rect(X(x0),176,X(x1),176,WOOD[0])
  c.rect(X(x0+4),182,X(x0+5),199,WOOD[1])
 # right: a tall fern in a wooden pot
 fern(397,177,((-30,28,3,0.025),(-14,42,3.5,0.012),(0,52,3,0.004),(14,44,3.5,0.012),(30,30,3,0.025)))
 pot(384,410)
 # left: a hanging lamp over a low, spreading fern
 c.rect(X(-28),51,X(-27),74,wall[3])
 for x in range(-40,-14):
  dx=x+27.5; top=74+int(abs(dx)*0.25)
  for y in range(top,84):c.set(X(x),y,S[0] if dx<-6 else S[1] if dx<5 else S[2])
 c.rect(X(-40),84,X(-15),85,WOOD[2])
 c.rect(X(-31),86,X(-24),88,S[0])
 fern(-28,177,((-62,24,3,0.03),(-40,30,3.5,0.03),(-18,36,4,0.02),(4,34,3.5,0.01),(26,32,4,0.022),(48,26,3.5,0.03),(68,20,3,0.03)))
 pot(-44,-12)
 # left: the cafe's door (glass over a wooden panel), its handle
 c.rect(X(-104),60,X(-60),201,WOOD[3]);c.rect(X(-101),63,X(-63),201,WOOD[2])
 c.rect(X(-98),66,X(-66),126,teal[0]);c.rect(X(-98),104,X(-66),126,teal[1])
 for x in range(-96,-67,9):c.rect(X(x),110,X(x+4),126,teal[2]);c.rect(X(x+1),100,X(x+3),114,teal[2])
 c.rect(X(-83),66,X(-81),126,WOOD[2])
 c.rect(X(-98),132,X(-66),196,WOOD[1]);c.rect(X(-94),136,X(-70),162,WOOD[2]);c.rect(X(-94),168,X(-70),192,WOOD[2])
 c.rect(X(-93),137,X(-71),137,WOOD[0]);c.rect(X(-93),169,X(-71),169,WOOD[0])
 c.rect(X(-70),146,X(-67),148,S[0]);c.rect(X(-68),149,X(-67),152,WOOD[3])
 c.rect(X(-108),56,X(-56),59,WOOD[2]);c.rect(X(-108),56,X(-56),56,WOOD[0])
 return c

def write_all(out,preview=False):
 os.makedirs(os.path.join(out,'tiles'),exist_ok=True); im=Image.new('RGB',(SHEET_W,SHEET_H),hs.MAGENTA)
 for name,x,y,w,h,n,g in SPRITES:
  for i in range(n):im.paste(sprite(name,i).image(),(x+i*w,y))
 files={'sprites.png':im,'tiles/ground.png':ground().image(),'tiles/hills.png':hills().image(),
        'tiles/hills_wide.png':hills_wide().image()}
 for f,img in files.items():img.save(os.path.join(out,f))
 return list(files)
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('--out',default=os.path.join(GAME,'art'));a.add_argument('--preview',action='store_true');o=a.parse_args();write_all(o.out,o.preview)


