#!/usr/bin/env python3
"""Code-drawn hedgehogs and woodland scenery, in the 8BCraft house style."""
import argparse, math, os, sys, random
from PIL import Image
HERE=os.path.dirname(os.path.abspath(__file__))
GAME=os.path.dirname(HERE)
ROOT=os.path.abspath(os.path.join(GAME,'..','..'))
sys.path.insert(0,os.path.join(ROOT,'games','common','tools'))
import house_style as hs
from hedgehog_designs import candidate, poly
Canvas=hs.Canvas
SHEET_W,SHEET_H=384,64
SPRITES=[
 ('hero_idle',0,0,32,32,2,'hero'),('hero_squash',64,0,32,32,1,'hero'),
 ('hero_stretch',96,0,32,32,1,'hero'),('hero_air',128,0,32,32,1,'hero'),
 ('hero_hit',160,0,32,32,1,'hero'),('hero_roll',192,0,32,32,4,'hero'),
 ('hero_icon',0,32,16,16,1,'hero'),('apple',32,32,16,16,1,'props'),
 ('puff',48,32,8,8,3,'props'),('snow',80,32,8,8,1,'props'),
 ('frost_wisp',96,32,24,8,2,'props')]
PANORAMAS={'ground':('tiles/ground.png',512,48),'hills':('tiles/hills.png',512,96)}
SPINES=[(228,187,126),(183,133,78),(135,89,52),(78,47,35)]
FACE=[(255,225,170),(232,192,130),(194,146,87),(135,89,52)]
OUT=(46,28,42)
def hero(pose='idle',frame=0):
 return candidate(2,pose,frame)
def hero_icon():
 cv=Canvas(16,16)
 back=[(228,191,143),(183,134,88),(127,82,56),(77,48,42)]
 face=[(255,226,180),(231,189,131),(184,131,80),(126,77,51)]
 poly(cv,[(1,11),(0,8),(2,8),(1,5),(4,6),(4,2),(6,5),(7,1),(9,5),(11,4),(12,8),(12,12),(9,14),(4,14)],back)
 poly(cv,[(9,7),(11,7),(13,9),(14,11),(13,12),(11,13),(9,11)],face)
 hs.eye(cv,10,8,look=(1,0));cv.set(14,11,OUT)
 cv.set(3,8,back[0]);cv.set(6,6,back[0]);cv.set(6,10,back[0])
 cv.outline(OUT);return cv
def apple():
 cv=Canvas(16,16); red=[(255,174,101),(235,98,68),(191,58,49),(109,41,42)]
 hs.ellipse(cv,5.5,9,4,4.5,red);hs.ellipse(cv,9.5,9,4,4.5,red)
 cv.rect(7,2,8,5,(78,47,35));cv.rect(9,2,11,3,(145,191,81));cv.set(4,6,(255,225,170))
 cv.outline((56,33,40));return cv
def puff(i):
 cv=Canvas(8,8)
 for y in range(8):
  for x in range(8):
   if (x-3.5)**2+(y-3.5)**2<(2+i*.6)**2 and (i==0 or hs.checker(x,y,i)):
    cv.set(x,y,(228,205,161) if y<4 else (186,160,116))
 return cv
def snow():
 cv=Canvas(8,8)
 for d in range(1,7):cv.set(3,d,(218,238,251));cv.set(d,3,(218,238,251))
 cv.set(2,2,(157,195,219));cv.set(4,4,(157,195,219));return cv
def frost_wisp(frame):
 cv=Canvas(24,8)
 for x in range(24):
  y=3+int(round(math.sin((x+frame*4)/6)))
  if 3<x<20 or hs.checker(x,y,frame):cv.set(x,y,(157,195,219))
  if 8<x<18 and hs.checker(x,y+1,frame):cv.set(x,y+1,(218,238,251))
 return cv
def sprite_frame(name,i):
 if name=='hero_icon':return hero_icon()
 if name.startswith('hero_'):return hero(name[5:],i)
 if name=='apple':return apple()
 if name=='puff':return puff(i)
 if name=='snow':return snow()
 if name=='frost_wisp':return frost_wisp(i)
 raise KeyError(name)
def ground():
 cv=Canvas(512,48)
 for y in range(48):
  for x in range(512):cv.set(x,y,[(159,116,73),(181,135,86),(205,161,111)][(y//8)%3])
 return cv
def hills():
 cv=Canvas(512,96)
 # Soft hazy ridges, no outlines. Distant forest, farms, and cloud wisps.
 for x in range(512):
  top=33+int(14*math.sin(x*math.pi/170)+7*math.sin(x*math.pi/80))
  for y in range(top,96):cv.set(x,y,(169,190,163) if y<top+2 else (132,168,146))
  top=63+int(12*math.sin(x*math.pi/120+1.8))
  for y in range(top,96):cv.set(x,y,(102,146,121) if y<top+2 else (85,129,110))
 for x in range(15,512,43):
  base=75+int(8*math.sin(x/45))
  cv.rect(x+4,base-14,x+5,base,(85,129,110))
  for y in range(base-37,base-8):
   width=(y-(base-37))//3
   cv.rect(x+5-width,y,x+5+width,y,(102,146,121) if (x//43)%2 else (85,129,110))
 for x,y in ((34,9),(230,16),(403,5)):
  for k in range(3):hs.ellipse(cv,x+k*10,y+4-(k%2)*3,12,4,[(232,231,205)]*4)
 return cv
def build_sheet():
 im=Image.new('RGB',(SHEET_W,SHEET_H),hs.MAGENTA)
 for name,x,y,w,h,n,g in SPRITES:
  for i in range(n):im.paste(sprite_frame(name,i).image(),(x+i*w,y))
 return im
def write_all(out,preview=False):
 os.makedirs(os.path.join(out,'tiles'),exist_ok=True)
 files={'sprites.png':build_sheet(),PANORAMAS['ground'][0]:ground().image(),PANORAMAS['hills'][0]:hills().image()}
 for f,im in files.items():
  im.save(os.path.join(out,f))
  if preview:
   os.makedirs(os.path.join(out,'preview'),exist_ok=True)
   im.resize((im.width*4,im.height*4),Image.Resampling.NEAREST).save(os.path.join(out,'preview',os.path.basename(f)))
 return sorted(files)
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--out',default=os.path.join(GAME,'art'));ap.add_argument('--preview',action='store_true');a=ap.parse_args()
 for f in write_all(a.out,a.preview):print('wrote',f)
