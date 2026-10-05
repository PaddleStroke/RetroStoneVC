#!/usr/bin/env python3
"""Original pixel moth, brass garden lantern and moonlit garden, drawn with the house kit."""
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
 ('lantern',32,32,32,48,2,'props'),('leaf',96,32,16,16,2,'props'),
 ('moon',128,32,16,16,1,'props'),('star',144,32,8,8,1,'props')]
PANORAMAS={'ground':('tiles/ground.png',512,48),'hills':('tiles/hills.png',512,96)}
WING=[(236,197,252),(188,125,211),(123,77,163),(68,49,105)]
GOLD=[(255,239,181),(246,187,78),(189,116,61),(111,70,58)]
IRON=[(127,151,170),(76,88,115),(42,48,77),(42,48,77)]
LEAF=[(165,185,129),(97,134,112),(56,84,91),(56,84,91)]
OUT=(29,26,49);CREAM=(255,246,220)
def hero(pose='idle',frame=0,small=False):
 size=16 if small else 32;cv=Canvas(size,size)
 if small:
  spread=6 if not frame else 4
  hs.ellipse(cv,7-spread//2,7,spread//2+1,5,WING);hs.ellipse(cv,8+spread//2,7,spread//2+1,5,WING)
  hs.ellipse(cv,8,9,2,4,GOLD);hs.ellipse(cv,8,5,3,2,GOLD)
  hs.eye(cv,6,3);hs.eye(cv,9,3);cv.set(5,1,OUT);cv.set(10,1,OUT)
 else:
  spread=11 if pose in ('idle','squash') and not frame else 8 if pose!='air' else 5
  for sign in (-1,1):
   hs.ellipse(cv,16+sign*spread//2,13,spread//2+2,9,WING)
   hs.ellipse(cv,16+sign*6,23,5,5,WING)
   hs.ellipse(cv,16+sign*(spread-2),12,2,3,GOLD)
   hs.ellipse(cv,16+sign*6,23,2,1.5,GOLD)
   cv.set(16+sign*6,15,WING[0]);cv.set(16+sign*5,25,WING[0])
  hs.ellipse(cv,16,20,3,8,GOLD);hs.ellipse(cv,16,12,4,4,GOLD)
  for y in (19,23):cv.rect(14,y,18,y,GOLD[2])
  for sign in (-1,1):
   for k in range(5):cv.set(16+sign*(2+k//2),7-k,OUT)
   hs.ellipse(cv,16+sign*4,3,1,1,GOLD)
  if pose=='hit':
   for ex in (12,17):
    for k in range(3):cv.set(ex+k,10+k,OUT);cv.set(ex+2-k,10+k,OUT)
  else:hs.eye(cv,12,9,look=(0,1),big=True);hs.eye(cv,17,9,look=(0,1),big=True)
  cv.set(15,15,OUT);cv.set(16,16,OUT);cv.set(17,15,OUT)
 cv.outline(OUT);return cv
def lantern(frame):
 cv=Canvas(32,48)
 # Handle, roof and feet enclose luminous glass and a tiny flickering candle.
 for x,y in ((14,2),(15,1),(16,1),(17,2),(13,3),(18,3),(13,4),(18,4)):cv.set(x,y,IRON[1])
 for y in range(6,13):cv.rect(15-(y-6),y,16+(y-6),y,IRON[0 if y<9 else 1])
 cv.rect(6,12,25,14,IRON[1]);cv.rect(8,15,23,35,GOLD[2]);cv.rect(10,16,21,33,GOLD[1])
 cv.rect(11,17,13,31,GOLD[0]);cv.rect(14,21,20,32,GOLD[0])
 cv.rect(14,28,18,34,CREAM);hs.ellipse(cv,16+frame,24,2,4,[CREAM,GOLD[0],GOLD[1],GOLD[2]])
 cv.rect(7,15,9,36,IRON[0]);cv.rect(22,15,24,36,IRON[2]);cv.rect(14,15,15,18,IRON[1])
 cv.rect(6,35,25,38,IRON[1]);cv.rect(8,39,23,40,IRON[2]);cv.rect(10,41,12,43,IRON[1]);cv.rect(20,41,22,43,IRON[2])
 cv.outline(OUT);return cv
def leaf(frame):
 cv=Canvas(16,16);hs.ellipse(cv,8,7,6 if not frame else 4,4 if not frame else 6,LEAF)
 for k in range(9):cv.set(3+k,12-k,LEAF[2])
 cv.set(12,3,LEAF[0]);cv.outline(OUT);return cv
def moon():
 cv=Canvas(16,16);hs.ellipse(cv,7,7,6,6,[CREAM,GOLD[0],GOLD[0],GOLD[1]])
 for y in range(3,11):
  for x in range(7,14):
   if (x-11)**2+(y-5)**2<20:cv.set(x,y,None)
 return cv
def star():
 cv=Canvas(8,8);cv.rect(3,1,3,5,GOLD[0]);cv.rect(1,3,5,3,GOLD[0]);cv.set(3,3,CREAM);return cv
def sprite_frame(name,i):
 if name=='hero_icon':return hero(frame=i,small=True)
 if name.startswith('hero_'):return hero(name[5:],i)
 return {'lantern':lambda:lantern(i),'leaf':lambda:leaf(i),'moon':moon,'star':star}[name]()
def ground():
 cv=Canvas(512,48)
 for y in range(48):
  for x in range(512):cv.set(x,y,(40,63,76) if y%8 else (57,79,86))
 return cv
def hills():
 cv=Canvas(512,96)
 for x in range(512):
  top=30+int(11*math.sin(x/41)+7*math.sin(x/79))
  for y in range(top,96):cv.set(x,y,(45,63,87))
 for x in range(0,512,24):
  cv.rect(x,52,x+3,90,(53,75,89));cv.rect(x,55,x+1,89,(63,86,96))
 cv.rect(0,65,511,67,(53,75,89))
 for x in range(0,512,17):
  top=79+int(6*math.sin(x/29))
  hs.ellipse(cv,x,top+10,12,11,[(68,95,99),(51,80,86),(36,61,74),(36,61,74)])
  for k in range(3):hs.ellipse(cv,x+k*3-4,top+5-k*2,3,1.5,[(76,106,102)]*4)
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
