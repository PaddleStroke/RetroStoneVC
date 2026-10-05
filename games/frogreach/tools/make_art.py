#!/usr/bin/env python3
"""Original code-drawn frogs, lily pads and swamp scenery in the house style."""
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
 ('hero_hit',160,0,32,32,1,'hero'),('hero_icon',0,32,16,16,1,'hero'),
 ('pad',32,32,32,16,1,'props'),('fly',64,32,16,16,2,'props'),
 ('tip',96,32,8,8,1,'props'),('splash',112,32,24,16,3,'props'),('fly_small',0,64,8,8,2,'props')]
PANORAMAS={'ground':('tiles/ground.png',512,48),'hills':('tiles/hills.png',512,96)}
GREEN=[(217,239,124),(135,200,79),(76,143,70),(42,88,63)]
BELLY=[(255,241,177),(226,213,123),(226,213,123),(168,167,93)]
PINK=[(255,188,180),(231,120,148),(155,76,115),(155,76,115)]
OUT=(35,42,58)
WATER=[(187,230,223),(118,190,185),(62,137,151),(62,137,151)]
GOLD=[(255,230,151),(241,185,92),(173,111,68),(173,111,68)]
CREAM=(255,246,220)
def hero(pose='idle',frame=0):
 # A compact crouching frog: broad head, round haunches, folded legs and webbed feet.
 cv=Canvas(32,32);breath=frame if pose=='idle' else 0
 hs.ellipse(cv,15,23+breath,11,6,GREEN)
 hs.ellipse(cv,7,25,5,4,GREEN)
 hs.capsule(cv,5,28,12,29,1.4,GREEN)
 for x in (4,7,10):hs.capsule(cv,x,28,x-1,30,.6,GREEN)
 hs.ellipse(cv,17,25+breath,7,3,BELLY)
 hs.capsule(cv,23,22,24,28,1.4,GREEN);hs.capsule(cv,23,29,28,29,1.4,GREEN)
 for x in (25,28):cv.set(x,30,GREEN[1])
 hs.ellipse(cv,16,17+breath,11,7,GREEN)
 hs.ellipse(cv,11,10+breath,5,5,GREEN);hs.ellipse(cv,22,10+breath,4,5,GREEN)
 if pose=='hit':
  for ex in (10,21):
   for d in range(3):cv.set(ex+d,9+d,OUT);cv.set(ex+2-d,9+d,OUT)
 else:
  hs.eye(cv,10,8+breath,look=(1,0),big=True,white=CREAM,pupil=OUT)
  hs.eye(cv,21,8+breath,look=(1,0),big=True,white=CREAM,pupil=OUT)
 cv.rect(13,20+breath,25,20+breath,OUT);cv.set(12,19+breath,OUT);cv.set(26,19+breath,OUT)
 cv.set(26,16+breath,OUT);cv.set(9,17+breath,GREEN[0])
 if pose=='squash':cv=hs.squash(cv,1.12)
 if pose=='stretch':cv=hs.squash(cv,.89)
 if pose=='air':cv=hs.squash(cv,1.05)
 cv.outline(OUT);return cv
def hero_icon():
 cv=Canvas(16,16)
 hs.ellipse(cv,7,11,6,3,GREEN);hs.ellipse(cv,10,9,5,3,GREEN)
 hs.ellipse(cv,6,5,3,3,GREEN);hs.ellipse(cv,12,5,3,3,GREEN)
 hs.eye(cv,5,3,look=(1,0),white=CREAM,pupil=OUT);hs.eye(cv,11,3,look=(1,0),white=CREAM,pupil=OUT)
 cv.rect(9,10,14,10,OUT);cv.rect(3,14,7,14,GREEN[2]);cv.rect(10,14,14,14,GREEN[1]);cv.outline(OUT);return cv
def fly_small(frame):
 cv=Canvas(8,8);hs.ellipse(cv,4,5,2,2,GOLD)
 cv.set(4,5,OUT);hs.ellipse(cv,2,2+frame,2,1,[CREAM]*4);hs.ellipse(cv,6,2-frame,1,1,[CREAM]*4);return cv

def pad():
 cv=Canvas(32,16);hs.ellipse(cv,15,7,14,5,GREEN)
 for x in range(15,29):cv.set(x,8,OUT)
 cv.set(26,7,None);cv.set(27,6,None);cv.set(28,5,None)
 cv.outline(OUT);return cv
def fly(frame):
 cv=Canvas(16,16)
 hs.ellipse(cv,5,5+frame,4,2,[CREAM]*4);hs.ellipse(cv,11,5-frame,3,2,[CREAM]*4)
 hs.ellipse(cv,8,9,3,4,GOLD);cv.rect(6,10,10,10,OUT)
 hs.eye(cv,8,6,look=(-1,0),white=CREAM,pupil=OUT);cv.outline(OUT);return cv
def tip():
 cv=Canvas(8,8);hs.ellipse(cv,3,3,3,2,PINK)
 cv.set(1,2,PINK[0]);cv.outline(OUT);return cv
def splash(frame):
 cv=Canvas(24,16)
 for x in range(24):
  y=12-int(math.sin(x*math.pi/23)*4)
  if frame<2 or hs.checker(x,y):cv.set(x,y,WATER[1])
 for k in range(5):
  x=3+k*4;y=4+abs(2-k)*2+frame*2
  hs.ellipse(cv,x,y,1.5,2,WATER)
 return cv
def sprite_frame(name,i):
 if name=='hero_icon':return hero_icon()
 if name.startswith('hero_'):return hero(name[5:],i)
 return {'pad':pad,'fly':lambda:fly(i),'fly_small':lambda:fly_small(i),'tip':tip,'splash':lambda:splash(i)}[name]()
def ground():
 cv=Canvas(512,48)
 for y in range(48):
  for x in range(512):cv.set(x,y,[(92,153,162),(119,178,178),(157,202,193)][(x//12+y//8)%3])
 return cv
def hills():
 cv=Canvas(512,96)
 for x in range(512):
  top=15+int(7*math.sin(x*math.tau/256)+5*math.sin(x*math.tau/512))
  for y in range(top,65):cv.set(x,y,(130,166,139) if y<top+2 else (104,147,129))
  top=43+int(8*math.sin(x*math.tau/256+1.6))
  for y in range(top,66):cv.set(x,y,(82,124,119))
 for x in range(18,512,39):
  for y in range(31,67):cv.set(x,y,(64,106,110))
  hs.ellipse(cv,x,33,2,7,[(115,145,112)]*4)
 for y in range(64,96):
  for x in range(512):
   c=(79,143,156) if y<67 else (93,158,166)
   if (y%9==0 and (x+y*7)%53<15):c=(142,192,188)
   if (y%13==2 and (x-y*11)%97<20):c=(114,175,176)
   cv.set(x,y,c)
 for x,y in ((36,2),(204,10),(416,0)):
  for k in range(3):hs.ellipse(cv,x+k*9,y+4-k%2*2,11,3,[(219,226,199)]*4)
 return cv
def write_all(out,preview=False):
 os.makedirs(os.path.join(out,'tiles'),exist_ok=True)
 sheet=Image.new('RGB',(SHEET_W,SHEET_H),hs.MAGENTA)
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
