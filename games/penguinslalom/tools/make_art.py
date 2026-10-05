#!/usr/bin/env python3
"""Code-drawn penguin on sardine-tin skis and alpine props, in the house style."""
import argparse,os,sys,math
from PIL import Image
HERE=os.path.dirname(os.path.abspath(__file__));GAME=os.path.dirname(HERE);ROOT=os.path.abspath(os.path.join(GAME,'..','..'))
sys.path.insert(0,os.path.join(ROOT,'games','common','tools'))
import house_style as hs
Canvas=hs.Canvas;SHEET_W,SHEET_H=384,96
SPRITES=[('hero_idle',0,0,32,32,2,'hero'),('hero_squash',64,0,32,32,1,'hero'),('hero_stretch',96,0,32,32,1,'hero'),
 ('hero_air',128,0,32,32,1,'hero'),('hero_hit',160,0,32,32,1,'hero'),('hero_icon',0,32,16,16,2,'hero'),
 ('ice',32,32,32,32,1,'props'),('seal',64,32,40,24,2,'props'),('hole',144,32,32,16,1,'props'),
 ('bank',176,32,32,16,1,'props'),('fish',208,32,16,16,1,'props'),('snow',224,32,8,8,2,'props'),
 ('ice_small',0,64,16,16,1,'props'),('seal_small',16,64,24,16,1,'props'),('hole_small',40,64,16,8,1,'props'),('bank_small',56,64,16,8,1,'props')]
PANORAMAS={'ground':('tiles/ground.png',512,48),'hills':('tiles/hills.png',512,96)}
OUT=(27,31,58);NAVY=[(94,118,149),(61,82,121),(39,54,89),(39,54,89)]
WHITE=[(255,249,221),(208,229,224),(208,229,224),(208,229,224)]
METAL=[(208,229,224),(126,171,193),(67,111,149),(67,111,149)]
GOLD=[(255,215,126),(233,159,74),(164,98,61),(164,98,61)]
RED=[(255,145,111),(223,84,66),(152,66,59),(152,66,59)]
ICE=[(238,253,244),(167,220,229),(101,176,204),(65,127,175)]
SEAL=[(168,180,196),(113,137,166),(73,97,139),(73,97,139)]
def hero(pose='idle',frame=0,small=False):
 cv=Canvas(16 if small else 32,16 if small else 32)
 if small:
  hs.ellipse(cv,8,7,5,6,NAVY);hs.ellipse(cv,8,9,3,4,WHITE)
  hs.eye(cv,5,3,white=WHITE[0],pupil=OUT);hs.eye(cv,9,3,white=WHITE[0],pupil=OUT)
  cv.rect(7,7,8,8,GOLD[1]);cv.rect(3,14,6,14,METAL[0]);cv.rect(9,14,12,14,METAL[0]);cv.rect(5,10,10,10,RED[1])
 else:
  hs.capsule(cv,8,13,4,20+frame,2,NAVY);hs.capsule(cv,24,13,28,20-frame,2,NAVY)
  hs.ellipse(cv,16,14,10,12,NAVY);hs.ellipse(cv,16,19,7,8,WHITE)
  hs.ellipse(cv,11,11,4,4,WHITE);hs.ellipse(cv,21,11,4,4,WHITE)
  if pose=='hit':
   for ex in (10,20):
    for k in range(3):cv.set(ex+k,8+k,OUT);cv.set(ex+2-k,8+k,OUT)
  else:hs.eye(cv,10,8,look=(0,1),white=WHITE[0],pupil=OUT,big=True);hs.eye(cv,20,8,look=(0,1),white=WHITE[0],pupil=OUT,big=True)
  hs.ellipse(cv,16,14,4,2,GOLD);cv.rect(9,17,22,18,RED[1]);cv.rect(10,17,21,17,RED[0]);cv.rect(22,18,24,23,RED[2])
  hs.capsule(cv,10,25,10,28,2,GOLD);hs.capsule(cv,22,25,22,28,2,GOLD)
  for x in (5,18):
   cv.rect(x,28,x+9,30,METAL[2]);cv.rect(x,28,x+8,28,METAL[0]);cv.rect(x+2,29,x+6,29,METAL[1])
  if pose=='squash':cv=hs.squash(cv,1.10)
  if pose=='air':cv=hs.squash(cv,.90)
 cv.outline(OUT);return cv
def ice(small=False):
 cv=Canvas(16 if small else 32,16 if small else 32);n=cv.w
 hs.ellipse(cv,n/2,n-5,n/2-3,4,ICE)
 for y in range(2,n-5):
  width=3+(y*3//n);cv.rect(n//2-width,y,n//2+width,y,ICE[2]);cv.rect(n//2-width,y,n//2-1,y,ICE[1]);cv.set(n//2-width,y,ICE[0])
 cv.outline(OUT);return cv
def seal(frame=0,small=False):
 cv=Canvas(24 if small else 40,16 if small else 24);n=cv.w;cy=cv.h-7
 hs.ellipse(cv,n/2-2,cy,n/2-3,6 if not small else 4,SEAL);hs.ellipse(cv,n-9,cy-3,6 if not small else 4,6 if not small else 4,SEAL)
 hs.eye(cv,n-8,cy-6,white=ICE[0],pupil=OUT);cv.set(n-3,cy-1,OUT)
 hs.capsule(cv,8,cy+2,4,cy+4,2,SEAL);cv.outline(OUT);return cv
def hole(small=False):
 cv=Canvas(16 if small else 32,8 if small else 16);hs.ellipse(cv,cv.w/2,cv.h/2,cv.w/2-1,cv.h/2-1,ICE)
 hs.ellipse(cv,cv.w/2,cv.h/2,cv.w/2-4,cv.h/2-3,[OUT]*4);return cv
def bank(small=False):
 cv=Canvas(16 if small else 32,8 if small else 16);hs.ellipse(cv,cv.w/2,cv.h-3,cv.w/2-1,cv.h-4,ICE);return cv
def fish():
 cv=Canvas(16,16);hs.ellipse(cv,8,8,5,3,GOLD);cv.rect(1,6,3,10,GOLD[1]);cv.set(10,7,OUT);cv.set(7,6,GOLD[0]);cv.outline(OUT);return cv
def snow(frame):
 cv=Canvas(8,8);hs.ellipse(cv,4,4,3,2,ICE)
 if frame:
  for y in range(8):
   for x in range(8):
    if not hs.checker(x,y):cv.set(x,y,None)
 return cv
def sprite_frame(name,i):
 if name=='hero_icon':return hero(frame=i,small=True)
 if name.startswith('hero_'):return hero(name[5:],i)
 return {'ice':ice,'seal':lambda:seal(i),'hole':hole,'bank':bank,'fish':fish,'snow':lambda:snow(i),
 'ice_small':lambda:ice(True),'seal_small':lambda:seal(small=True),'hole_small':lambda:hole(True),'bank_small':lambda:bank(True)}[name]()
def ground():
 cv=Canvas(512,48);cv.rect(0,0,511,47,ICE[1]);return cv
def hills():
 cv=Canvas(512,96)
 for x in range(512):
  top=14+abs((x%128)-64)//3
  for y in range(top,72):cv.set(x,y,(127,165,193) if y>top+8 else (217,235,225))
  top=35+abs(((x+64)%256)-128)//4
  for y in range(top,96):cv.set(x,y,(163,197,209) if y>top+6 else (238,247,232))
 return cv
def write_all(out,preview=False):
 os.makedirs(os.path.join(out,'tiles'),exist_ok=True);sheet=Image.new('RGB',(SHEET_W,SHEET_H),hs.MAGENTA)
 for name,x,y,w,h,n,g in SPRITES:
  for i in range(n):sheet.paste(sprite_frame(name,i).image(),(x+i*w,y))
 for f,im in {'sprites.png':sheet,'tiles/ground.png':ground().image(),'tiles/hills.png':hills().image()}.items():
  im.save(os.path.join(out,f))
  if preview:
   os.makedirs(os.path.join(out,'preview'),exist_ok=True);im.resize((im.width*4,im.height*4),Image.Resampling.NEAREST).save(os.path.join(out,'preview',os.path.basename(f)))
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--out',default=os.path.join(GAME,'art'));ap.add_argument('--preview',action='store_true');a=ap.parse_args();write_all(a.out,a.preview)
