#!/usr/bin/env python3
"""Cheerful crab paddle, coconuts and shell fans; original house-style pixels."""
import argparse,os,sys,math
from PIL import Image
HERE=os.path.dirname(os.path.abspath(__file__));GAME=os.path.dirname(HERE);ROOT=os.path.abspath(os.path.join(GAME,'..','..'))
sys.path.insert(0,os.path.join(ROOT,'games','common','tools'))
import house_style as hs
Canvas=hs.Canvas;SHEET_W,SHEET_H=256,96
SPRITES=[('hero_idle',0,0,32,32,2,'hero'),('hero_squash',64,0,32,32,1,'hero'),('hero_stretch',96,0,32,32,1,'hero'),
 ('hero_air',128,0,32,32,1,'hero'),('hero_hit',160,0,32,32,1,'hero'),('hero_icon',0,32,16,16,2,'hero'),
 ('coconut',16,32,16,16,1,'props'),('shell',32,32,24,16,3,'props'),
 ('coconut_small',0,64,8,8,1,'props'),('shell_small',8,64,16,8,3,'props')]
PANORAMAS={'ground':('tiles/ground.png',512,48),'hills':('tiles/hills.png',512,192),'hills_wide':('tiles/hills_wide.png',1024,192)}
OUT=(27,31,58);CREAM=(255,246,220)
CORAL=[(255,186,139),(235,112,82),(169,68,60),(169,68,60)]
WHITE=[CREAM,(212,232,218),(151,183,189),(151,183,189)]
BROWN=[(222,172,112),(166,113,74),(105,74,62),(105,74,62)]
PINK=[(255,196,199),(226,141,158),(168,92,125),(168,92,125)]
BLUE=[(186,235,226),(109,182,195),(65,127,164),(65,127,164)]
GOLD=[(255,222,148),(234,176,103),(179,121,71),(179,121,71)]
def hero(pose='idle',frame=0,small=False):
 cv=Canvas(16 if small else 32,16 if small else 32)
 if small:
  hs.ellipse(cv,8,10,5,3,CORAL)
  for x in (5,10):cv.rect(x,5,x,8,CORAL[1]);hs.ellipse(cv,x,5,2,2,WHITE);cv.set(x,5,OUT)
  for x in (2,13):hs.ellipse(cv,x,8,2,3,CORAL);cv.set(x,6,None)
  for x in (4,7,10,12):cv.set(x,14,CORAL[2])
 else:
  for side in (-1,1):
   for yy in (22,25):hs.capsule(cv,16+side*7,yy,16+side*(11+frame),yy+3,1,CORAL)
   hs.capsule(cv,16+side*8,19,16+side*11,13,2,CORAL)
   hs.ellipse(cv,16+side*12,10,3,5,CORAL)
   cv.rect(16+side*12,5,16+side*12,8,None)
  hs.ellipse(cv,16,22,9,6,CORAL)
  for ex in (11,21):
   hs.capsule(cv,ex,18,ex,11,1,CORAL);hs.ellipse(cv,ex,10,4,4,WHITE)
   if pose=='hit':cv.rect(ex-1,10,ex+1,10,OUT)
   else:hs.eye(cv,ex-1,8,look=(0,-1 if pose=='air' else 0),white=CREAM,pupil=OUT,big=True)
  cv.rect(13,23,19,23,OUT);cv.set(12,22,OUT);cv.set(20,22,OUT);cv.set(15,20,CORAL[0])
  if pose=='squash':cv=hs.squash(cv,1.09)
 cv.outline(OUT);return cv
def coconut(small=False):
 cv=Canvas(8 if small else 16,8 if small else 16);r=3 if small else 6
 hs.ellipse(cv,cv.w//2,cv.h//2,r,r,BROWN)
 for x,y in ((2,3),(4,2),(5,4)) if small else ((5,5),(8,4),(10,6)):cv.set(x,y,OUT)
 if not small:cv.rect(4,9,5,11,BROWN[0]);cv.set(6,12,CREAM)
 cv.outline(OUT);return cv
def shell(kind=0,small=False):
 cv=Canvas(16 if small else 24,8 if small else 16);ramp=(PINK,BLUE,GOLD)[kind];cx=cv.w//2
 hs.ellipse(cv,cx,cv.h//2,cv.w//2-2,cv.h//2-2,ramp)
 for angle in (-1.1,-.55,0,.55,1.1):
  ex=round(cx+math.sin(angle)*(cv.w//2-3));ey=1+round(abs(angle)*2)
  hs.capsule(cv,cx,cv.h-3,ex,ey,0.5,[ramp[2]]*4)
 hs.ellipse(cv,cx,cv.h-3,3 if not small else 2,1,ramp)
 if kind==2:hs.ellipse(cv,cx,cv.h//2,2,2,[CREAM]*4)
 cv.outline(OUT);return cv
def sprite_frame(name,i):
 if name=='hero_icon':return hero(frame=i,small=True)
 if name.startswith('hero_'):return hero(name[5:],i)
 return {'coconut':coconut,'coconut_small':lambda:coconut(True),'shell':lambda:shell(i),'shell_small':lambda:shell(i,True)}[name]()
def ground():
 cv=Canvas(512,48);cv.rect(0,0,511,47,(239,217,171));return cv
def hills():
 cv=Canvas(512,192)
 sand=[(249,231,192),(231,206,161),(205,170,124)]
 water=[(197,235,222),(145,207,207),(111,183,198)]
 leaf=[(123,177,143),(91,143,120)];trunk=[(201,178,137),(162,141,115)]
 for y in range(88,192):
  cv.rect(0,y,511,y,water[0] if y<96 else water[1] if y<120 else water[2])
 for x in range(512):
  top=159+(x%128-64)**2//430
  cv.rect(x,top, x,191,sand[0]);cv.set(x,top-1,water[0]);cv.set(x,top+5,sand[1])
 for y in (101,123,143):
  for x in range(512):
   if (x+y*7)%79<42:cv.set(x,y,water[0])
 for x in range(20,512,128):
  hs.ellipse(cv,x+20,88,32,8,[leaf[0]]*4)
  hs.capsule(cv,x+14,89,x+19,52,2,[trunk[0]]*4)
  for dx,dy in ((-17,3),(-12,-8),(8,-10),(22,1)):
   hs.capsule(cv,x+19,52,x+19+dx,52+dy,2,[leaf[1]]*4)
 return cv
# ---- widescreen (DESIGN.md "Widescreen") -----------------------------------------------------------------------
class WrapCanvas(Canvas):
 """a canvas whose x wraps round (a panorama on a wrapping map: stage x s is picture x s mod w)"""
 def set(self,x,y,c):Canvas.set(self,x%self.w,y,c)
def hills_wide():
 """The beach 1024 px wide for a screen wider than 320 (a 128-tile map, stage x s = picture x s & 1023): the same
 sea, sandbanks and palm islands as hills() (pixel for pixel over the 320-px court), every 128 px round the whole
 picture, and the sea's ripples in stage coordinates, so that nothing breaks where the picture wraps (stage x 0)."""
 W=1024;cv=WrapCanvas(W,192)
 sand=[(249,231,192),(231,206,161),(205,170,124)]
 water=[(197,235,222),(145,207,207),(111,183,198)]
 leaf=[(123,177,143),(91,143,120)];trunk=[(201,178,137),(162,141,115)]
 for y in range(88,192):
  cv.rect(0,y,W-1,y,water[0] if y<96 else water[1] if y<120 else water[2])
 for x in range(W):
  top=159+(x%128-64)**2//430
  cv.rect(x,top, x,191,sand[0]);cv.set(x,top-1,water[0]);cv.set(x,top+5,sand[1])
 for y in (101,123,143):
  for x in range(-W//2,W//2):
   if (x+y*7)%79<42:cv.set(x,y,water[0])
 for x in range(20,W,128):
  hs.ellipse(cv,x+20,88,32,8,[leaf[0]]*4)
  hs.capsule(cv,x+14,89,x+19,52,2,[trunk[0]]*4)
  for dx,dy in ((-17,3),(-12,-8),(8,-10),(22,1)):
   hs.capsule(cv,x+19,52,x+19+dx,52+dy,2,[leaf[1]]*4)
 return cv
def write_all(out,preview=False):
 os.makedirs(os.path.join(out,'tiles'),exist_ok=True);sheet=Image.new('RGB',(SHEET_W,SHEET_H),hs.MAGENTA)
 for name,x,y,w,h,n,g in SPRITES:
  for i in range(n):sheet.paste(sprite_frame(name,i).image(),(x+i*w,y))
 for f,im in {'sprites.png':sheet,'tiles/ground.png':ground().image(),'tiles/hills.png':hills().image(),
              'tiles/hills_wide.png':hills_wide().image()}.items():
  im.save(os.path.join(out,f))
  if preview:
   os.makedirs(os.path.join(out,'preview'),exist_ok=True);im.resize((im.width*4,im.height*4),Image.Resampling.NEAREST).save(os.path.join(out,'preview',os.path.basename(f)))
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--out',default=os.path.join(GAME,'art'));ap.add_argument('--preview',action='store_true');a=ap.parse_args();write_all(a.out,a.preview)
