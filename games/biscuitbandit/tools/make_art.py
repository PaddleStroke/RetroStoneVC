#!/usr/bin/env python3
"""Code-drawn raccoon cafe: shaded sprites and tiled scenery in the house palette."""
import argparse, os, sys, math
from PIL import Image
HERE=os.path.dirname(os.path.abspath(__file__)); GAME=os.path.dirname(HERE); ROOT=os.path.abspath(os.path.join(GAME,'..','..'))
sys.path.insert(0,os.path.join(ROOT,'games','common','tools'))
import house_style as hs
Canvas=hs.Canvas
SHEET_W,SHEET_H=256,128
SPRITES=[('hero_idle',0,0,32,32,2,'hero'),('hero_squash',64,0,32,32,1,'hero'),('hero_stretch',96,0,32,32,1,'hero'),('hero_air',128,0,32,32,1,'hero'),('hero_hit',160,0,32,32,1,'hero'),('hero_icon',192,0,16,16,1,'hero'),('tray',0,80,48,16,3,'props'),('biscuit',0,56,16,8,3,'props'),('cup',64,56,16,16,1,'props'),('basket',96,40,32,32,1,'props'),('chute',144,40,32,24,1,'props'),('puff',192,40,8,8,3,'props'),('heart',224,40,8,8,1,'props'),('warning',224,56,16,16,1,'props'),('belt',0,104,32,8,4,'props')]
PANORAMAS={'ground':('tiles/ground.png',512,32),'hills':('tiles/hills.png',512,208)}
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

def write_all(out,preview=False):
 os.makedirs(os.path.join(out,'tiles'),exist_ok=True); im=Image.new('RGB',(SHEET_W,SHEET_H),hs.MAGENTA)
 for name,x,y,w,h,n,g in SPRITES:
  for i in range(n):im.paste(sprite(name,i).image(),(x+i*w,y))
 files={'sprites.png':im,'tiles/ground.png':ground().image(),'tiles/hills.png':hills().image()}
 for f,img in files.items():img.save(os.path.join(out,f))
 return list(files)
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('--out',default=os.path.join(GAME,'art'));a.add_argument('--preview',action='store_true');o=a.parse_args();write_all(o.out,o.preview)


