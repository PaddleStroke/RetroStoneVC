#!/usr/bin/env python3
"""Original mouse with espresso jetpack, steam, biscuits and café scenery in the house style."""
import argparse,math,os,sys
from PIL import Image
HERE=os.path.dirname(os.path.abspath(__file__));GAME=os.path.dirname(HERE)
ROOT=os.path.abspath(os.path.join(GAME,'..','..'))
sys.path.insert(0,os.path.join(ROOT,'games','common','tools'))
import house_style as hs
Canvas=hs.Canvas
SHEET_W,SHEET_H=384,96
SPRITES=[('hero_idle',0,0,32,32,2,'hero'),('hero_squash',64,0,32,32,1,'hero'),
 ('hero_stretch',96,0,32,32,1,'hero'),('hero_air',128,0,32,32,1,'hero'),
 ('hero_hit',160,0,32,32,1,'hero'),('hero_icon',0,32,16,16,2,'hero'),
 ('biscuit',32,32,16,16,2,'props'),('toast',112,64,24,24,4,'props'),
 ('toaster',96,32,40,24,2,'props'),('steam',192,32,16,24,3,'props'),
 ('puff',240,32,16,16,3,'props'),('cup',288,32,16,16,1,'props'),
 ('toaster_small',0,64,24,16,2,'props'),('toast_small',48,64,16,8,1,'props'),
 ('biscuit_small',64,64,8,8,2,'props'),('steam_small',80,64,8,16,3,'props')]
PANORAMAS={'ground':('tiles/ground.png',512,48),'hills':('tiles/hills.png',512,192)}
FUR=[(222,218,207),(179,172,163),(128,121,123),(82,76,88)]
PINK=[(247,179,205),(217,128,163),(155,83,122),(155,83,122)]
METAL=[(211,230,232),(134,168,185),(72,100,128),(72,100,128)]
CORAL=[(255,135,94),(222,81,66),(143,64,47),(143,64,47)]
OUT=(35,31,51);CREAM=(255,246,220)
GOLD=[(255,229,161),(221,173,91),(154,102,55),(154,102,55)]
BAKE_OUT=(89,57,53)
STEAM=[CREAM,(218,232,232),(172,195,200),(172,195,200)]
def hero(pose='idle',frame=0,small=False):
 cv=Canvas(16 if small else 32,16 if small else 32)
 if small:
  # Purpose-drawn small mouse; the pack and big ear remain visible at 1x.
  cv.rect(1,5,6,12,CORAL[1]);cv.rect(2,5,3,11,CORAL[0]);cv.rect(1,4,6,4,METAL[0])
  hs.ellipse(cv,10,10,4,4,FUR);hs.ellipse(cv,10,4,3,3,FUR);hs.ellipse(cv,10,4,2,2,PINK)
  hs.ellipse(cv,12,7,3,2,FUR);cv.set(15,7,PINK[1]);hs.eye(cv,11,5,white=CREAM,pupil=OUT)
  cv.rect(8,10,12,10,CORAL[0]);cv.rect(9,13,13,13,PINK[1]);cv.set(2,13,METAL[1]);cv.outline(OUT);return cv
 airborne=pose in ('stretch','air')
 # Curved pink tail and little hind feet sit behind the heavy machine.
 for x,y in ((10,27),(8,28),(6,28),(4,27),(3,26),(3,24),(4,23)):
  cv.set(x,y,PINK[1]);cv.set(x,y+1,PINK[2])
 hs.ellipse(cv,19,22,7,7,FUR)
 hs.capsule(cv,19,25,23 if airborne else 22+frame,28,2,PINK)
 hs.capsule(cv,23,23,28,25 if airborne else 28,1.5,PINK)
 hs.ellipse(cv,20,14,7,6,FUR);hs.ellipse(cv,18,7,6,6,FUR);hs.ellipse(cv,18,7,4,4,PINK)
 hs.capsule(cv,23,16,28,16,2.5,FUR);cv.set(29,15,PINK[1]);cv.set(29,16,PINK[2])
 if pose=='hit':
  for k in range(3):cv.set(23+k,11+k,OUT);cv.set(25-k,11+k,OUT)
 else:hs.eye(cv,23,10,look=(1,0),white=CREAM,pupil=OUT,big=True)
 cv.rect(25,19,28,19,OUT);cv.set(25,20,CREAM)
 # Coral scarf trails backwards; the same material recolours for each player.
 cv.rect(17,19,23,21,CORAL[1]);cv.rect(17,19,23,19,CORAL[0]);cv.rect(13,20,17,22,CORAL[2])
 hs.capsule(cv,22,22,27,22,1.5,FUR)
 # Espresso pack: bright enamel tank, chrome lid, pressure gauge and steam nozzle.
 cv.rect(4,10,13,23,CORAL[1]);cv.rect(5,11,7,22,CORAL[0]);cv.rect(12,11,13,22,CORAL[2])
 cv.rect(4,8,13,10,METAL[1]);cv.rect(5,8,12,8,METAL[0]);cv.rect(4,23,13,25,METAL[2])
 cv.rect(5,23,11,23,METAL[1]);cv.rect(7,25,10,27,METAL[1]);cv.rect(7,27,10,27,METAL[0])
 hs.ellipse(cv,10,14,2,2,[CREAM]*4);cv.set(10,13,OUT);cv.set(11,14,OUT)
 cv.rect(2,12,3,19,METAL[2]);cv.set(2,12,METAL[0]);cv.rect(13,14,15,15,METAL[1])
 if pose=='squash':cv=hs.squash(cv,1.10)
 cv.outline(OUT);return cv
def biscuit(frame):
 cv=Canvas(16,16);hs.ellipse(cv,8,8,6 if not frame else 5,6,GOLD)
 for x,y in ((5,5),(10,4),(8,8),(4,10),(11,10)):cv.set(x,y,BAKE_OUT)
 cv.set(5,4,GOLD[0]);cv.outline(BAKE_OUT);return cv
def toast(frame=0):
 cv=Canvas(24,16);hs.ellipse(cv,12,5,10,4,GOLD);cv.rect(3,5,20,13,GOLD[2]);cv.rect(5,5,18,12,GOLD[0])
 cv.rect(8,8,15,9,GOLD[1]);cv.set(7,6,GOLD[1]);cv.set(16,11,GOLD[1]);cv.outline(BAKE_OUT)
 # Four nearest-neighbour, code-drawn tilted poses for a gentle tumble in flight.
 out=Canvas(24,24);angle=math.radians((-18,-8,8,18)[frame]);co,si=math.cos(angle),math.sin(angle)
 for y in range(24):
  for x in range(24):
   px=round((x-11.5)*co+(y-11.5)*si+11.5);py=round(-(x-11.5)*si+(y-11.5)*co+7.5)
   if 0<=px<24 and 0<=py<16:out.set(x,y,cv.get(px,py))
 return out
def toaster(frame):
 cv=Canvas(40,24);hs.ellipse(cv,20,9,18,5,METAL);cv.rect(2,9,37,21,METAL[1]);cv.rect(3,10,10,19,METAL[0])
 cv.rect(29,10,37,20,METAL[2]);cv.rect(6,5,31,7,OUT);cv.rect(7,5,30,5,METAL[2])
 cv.rect(9,22,13,23,OUT);cv.rect(28,22,32,23,OUT);cv.rect(34,10,35,16,OUT)
 cv.rect(33,16 if frame else 10,36,17 if frame else 11,CREAM);cv.rect(28,16,30,18,CORAL[0 if frame else 1]);cv.outline(OUT);return cv
def steam(frame):
 cv=Canvas(16,24)
 hs.capsule(cv,7,1,6+frame,11,2,STEAM);hs.ellipse(cv,6+frame,13,4,5,STEAM)
 hs.ellipse(cv,9-frame,18,5,4,STEAM)
 for y in range(18,24):
  for x in range(16):
   if not hs.dither_ok(x,y,3-(y-18)//2):cv.set(x,y,None)
 return cv
def puff(frame):
 cv=Canvas(16,16)
 for k in range(3):hs.ellipse(cv,4+k*4,8+(k%2)*3,3+frame,3,STEAM)
 if frame==2:
  for y in range(16):
   for x in range(16):
    if not hs.checker(x,y):cv.set(x,y,None)
 return cv
def cup():
 cv=Canvas(16,16);cv.rect(3,5,11,11,CREAM);cv.rect(3,11,11,12,METAL[1]);cv.rect(12,6,14,10,METAL[0])
 cv.rect(0,13,14,13,METAL[0]);cv.rect(3,4,11,4,BAKE_OUT);cv.set(5,1,STEAM[1]);cv.set(6,2,STEAM[1]);cv.outline(OUT);return cv
def toaster_small(frame):
 cv=Canvas(24,16);hs.ellipse(cv,12,5,10,3,METAL);cv.rect(2,5,21,13,METAL[1]);cv.rect(3,6,7,12,METAL[0])
 cv.rect(18,6,21,13,METAL[2]);cv.rect(5,3,18,4,OUT);cv.set(19,10,CORAL[frame]);cv.outline(OUT);return cv
def toast_small():
 cv=Canvas(16,8);hs.ellipse(cv,8,3,7,3,GOLD);cv.rect(2,3,13,6,GOLD[1]);cv.rect(4,3,11,5,GOLD[0]);cv.outline(BAKE_OUT);return cv
def biscuit_small(frame):
 cv=Canvas(8,8);hs.ellipse(cv,4,4,3,3,GOLD);cv.set(3,3,BAKE_OUT);cv.set(5,5,BAKE_OUT);cv.outline(BAKE_OUT);return cv
def steam_small(frame):
 cv=Canvas(8,16);hs.capsule(cv,3,1,3+frame%2,7,1,STEAM);hs.ellipse(cv,4,11,3,4,STEAM)
 for y in range(13,16):
  for x in range(8):
   if not hs.checker(x,y):cv.set(x,y,None)
 return cv
def sprite_frame(name,i):
 if name=='hero_icon':return hero(frame=i,small=True)
 if name.startswith('hero_'):return hero(name[5:],i)
 return {'biscuit':lambda:biscuit(i),'toast':lambda:toast(i),'toaster':lambda:toaster(i),
  'steam':lambda:steam(i),'puff':lambda:puff(i),'cup':cup,'toaster_small':lambda:toaster_small(i),
  'toast_small':toast_small,'biscuit_small':lambda:biscuit_small(i),'steam_small':lambda:steam_small(i)}[name]()
def ground():
 cv=Canvas(512,48)
 for y in range(48):
  for x in range(512):
   color=(112,81,69) if y<4 else (91,68,66) if y<9 else (75,62,65)
   if y==0:color=(192,154,105)
   if y==3:color=(53,46,59)
   if y>10 and (x//37+y//12)%2:color=(84,65,64)
   if y>10 and y%12==0:color=(49,45,57)
   cv.set(x,y,color)
 return cv
def hills():
 cv=Canvas(512,192)
 WALL=(178,157,137);RIM=(120,113,113);LIGHT=(207,193,166);GLASS=(156,180,179);SHADOW=(146,135,125)
 for x in range(512):
  if x%64<2:cv.rect(x,0,x,191,SHADOW)
 # Repeating café windows, shelves, muted cups and a chalkboard; deliberately no tiny text.
 for x in (26,284):
  cv.rect(x,7,x+97,99,RIM);cv.rect(x+3,10,x+94,96,LIGHT);cv.rect(x+7,14,x+90,91,GLASS)
  cv.rect(x+46,14,x+49,91,LIGHT);cv.rect(x+7,48,x+90,51,LIGHT)
  for k in range(3):
   hs.ellipse(cv,x+20+k*31,86-k*3,22,7,[(145,166,165)]*4)
  cv.rect(x-5,3,x+102,9,(135,125,121));cv.rect(x-2,101,x+100,106,(123,116,113))
 for x in (152,406):
  cv.rect(x,17,x+61,66,(122,119,111));cv.rect(x+3,20,x+58,63,(83,111,106))
  for k in range(3):cv.rect(x+12,29+k*10,x+45-k*6,30+k*10,(171,181,153))
  cv.rect(x-7,92,x+73,97,(129,114,105))
  for j in range(3):
   cv.rect(x+j*24,79,x+13+j*24,90,LIGHT);cv.rect(x+14+j*24,82,x+17+j*24,87,RIM)
 cv.rect(0,123,511,126,(137,123,112));cv.rect(0,128,511,131,(199,178,147))
 for x in range(0,512,32):
  cv.rect(x,134,x+1,191,(163,142,128))
  cv.rect(x+4,147,x+27,148,(168,147,133))
 return cv
def write_all(out,preview=False):
 os.makedirs(os.path.join(out,'tiles'),exist_ok=True);sheet=Image.new('RGB',(SHEET_W,SHEET_H),hs.MAGENTA)
 for name,x,y,w,h,n,g in SPRITES:
  for i in range(n):sheet.paste(sprite_frame(name,i).image(),(x+i*w,y))
 for f,im in {'sprites.png':sheet,'tiles/ground.png':ground().image(),'tiles/hills.png':hills().image()}.items():
  im.save(os.path.join(out,f))
  if preview:
   os.makedirs(os.path.join(out,'preview'),exist_ok=True)
   im.resize((im.width*4,im.height*4),Image.Resampling.NEAREST).save(os.path.join(out,'preview',os.path.basename(f)))
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--out',default=os.path.join(GAME,'art'));ap.add_argument('--preview',action='store_true')
 a=ap.parse_args();write_all(a.out,a.preview)
