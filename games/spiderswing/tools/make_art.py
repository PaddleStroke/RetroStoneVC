#!/usr/bin/env python3
"""Original code-drawn Spider Swing art using the 8BCraft pixel-art kit."""
import argparse,math,os,sys
from PIL import Image
HERE=os.path.dirname(os.path.abspath(__file__)); GAME=os.path.dirname(HERE)
ROOT=os.path.abspath(os.path.join(GAME,'..','..'))
sys.path.insert(0,os.path.join(ROOT,'games','common','tools'))
import house_style as hs
Canvas=hs.Canvas
SHEET_W,SHEET_H=384,96
SPRITES=[('hero_idle',0,0,32,32,2,'hero'),('hero_squash',64,0,32,32,1,'hero'),
 ('hero_stretch',96,0,32,32,1,'hero'),('hero_air',128,0,32,32,1,'hero'),
 ('hero_hit',160,0,32,32,1,'hero'),('hero_icon',0,32,16,16,1,'hero'),
 ('flower',32,32,24,24,2,'props'),('dew',80,32,8,8,1,'props'),
 ('bee',96,32,24,16,2,'props'),('trap',144,32,24,24,2,'props'),('flower_small',0,64,16,16,2,'props'),('bee_small',32,64,16,8,2,'props')]
FLOWER_NAMES=('fuchsia','bell','star','rosette','orchid')
for kind,name in enumerate(FLOWER_NAMES[1:]):
 SPRITES += [('flower_'+name,192+kind*48,32,24,24,2,'props'),
             ('flower_'+name+'_small',64+kind*32,64,16,16,2,'props')]
PANORAMAS={'ground':('tiles/ground.png',512,48),'hills':('tiles/hills.png',512,192)}
PURPLE=[(230,180,250),(179,115,223),(124,65,171),(76,37,109)]
OUT=(42,29,60)
GREEN=[(192,222,144),(121,173,96),(73,125,84),(40,78,64)]
PINK=[(255,188,196),(229,120,158),(163,72,125),(163,72,125)]
GOLD=[(255,230,151),(241,185,92),(173,111,68),(173,111,68)]
BLUE=[(207,245,246),(126,205,223),(74,141,171),(74,141,171)]
CREAM=(255,246,220)
def hero(pose='idle',frame=0):
 cv=Canvas(32,32); bob=frame if pose=='idle' else 0
 for side in (-1,1):
  for k in range(4):
   a=(15+side*4,15+k*2+bob); b=(15+side*(11+k%2),10+k*5+bob)
   end=(15+side*13,12+k*5+bob+(frame%2))
   hs.capsule(cv,*a,*b,1.0,PURPLE);hs.capsule(cv,*b,*end,.75,PURPLE)
 hs.ellipse(cv,15,15+bob,7,9,PURPLE)
 hs.ellipse(cv,17,23+bob,6,5,PURPLE)
 for ex in (14,20):
  if pose=='hit':
   for d in range(3): cv.set(ex+d,20+d,OUT);cv.set(ex+2-d,20+d,OUT)
  else: hs.eye(cv,ex,19+bob,look=(1,0),big=True)
 cv.set(11,11+bob,PURPLE[0]);cv.set(12,12+bob,PURPLE[0])
 if pose=='squash': cv=hs.squash(cv,1.15)
 if pose=='stretch': cv=hs.squash(cv,.87)
 cv.outline(OUT);return cv
def hero_icon():
 cv=Canvas(16,16)
 for side in (-1,1):
  for k in range(3): hs.capsule(cv,7+side*3,6+k*2,7+side*6,3+k*4,.7,PURPLE)
 hs.ellipse(cv,8,7,4,5,PURPLE);hs.ellipse(cv,8,11,4,3,PURPLE)
 hs.eye(cv,6,8,look=(1,0));hs.eye(cv,10,8,look=(1,0))
 cv.outline(OUT);return cv
def flower(frame,small=False,kind=0):
 # Pendant fuchsia blossoms: the calyx and vine are above the flower, with no ground stalk.
 cv=Canvas(16 if small else 24,16 if small else 24)
 if kind:
  # Draw directly at each native size; all flowers hang from the same top calyx.
  def u(n):return round(n*cv.w/24)
  def e(x,y,rx,ry,ramp=PINK):hs.ellipse(cv,u(x),u(y),max(1,u(rx)),max(1,u(ry)),ramp)
  def c(ax,ay,bx,by,r,ramp=PINK):hs.capsule(cv,u(ax),u(ay),u(bx),u(by),max(.6,r*cv.w/24),ramp)
  c(12,0,12,6,1,GREEN)
  droop=frame
  if kind==1: # Bell: narrow throat, deep cup and broad scalloped rim.
   e(12,11+droop,5,7);e(12,17+droop,8,3)
   for x in (7,12,17):e(x,18+droop,2,2)
   c(12,17,12,21,1,GOLD);e(12,21,2,1,GOLD)
   c(9,9,8,14,1,[PINK[0]]*4)
  elif kind==2: # Star: five long, separated petals around a bright centre.
   for petal in range(5):
    angle=math.tau*petal/5-math.pi/2
    c(12,13+droop,12+math.cos(angle)*8,13+droop+math.sin(angle)*8,2)
   e(12,13+droop,3,3,GOLD)
  elif kind==3: # Rosette: round, overlapping outer and inner whorls.
   for petal in range(7):
    angle=math.tau*petal/7
    e(12+math.cos(angle)*6,13+droop+math.sin(angle)*6,4,4)
   e(12,13+droop,5,5,[PINK[0],PINK[0],PINK[1],PINK[2]])
   for side in (-1,1):c(12+side*3,11+droop,12+side*2,15+droop,.7,[PINK[2]]*4)
   e(12,13+droop,2,2,GOLD)
  else: # Orchid: wing petals, upright sepals and a long pendant lower lip.
   e(6,12+droop,4,3);e(18,12+droop,4,3)
   c(12,12+droop,7,6+droop,2);c(12,12+droop,17,6+droop,2)
   e(12,18+droop,4,4);e(12,13+droop,3,2,GOLD)
   e(12,18+droop,2,2,[PINK[0]]*4)
  e(12,5,4,2,GREEN)
  cv.outline(OUT);return cv
 if small:
  hs.capsule(cv,8,0,8,3,1,GREEN);hs.ellipse(cv,8,4,4,2,GREEN)
  hs.ellipse(cv,8,8+frame,5,4,PINK);hs.ellipse(cv,8,11+frame,6,2,PINK)
  cv.rect(7,12,7,14,GOLD[1]);cv.rect(10,12,10,14,GOLD[1]);cv.set(6,7,PINK[0])
 else:
  hs.capsule(cv,12,0,12,4,1,GREEN)
  for side in (-1,1):
   hs.ellipse(cv,12+side*4,10+frame,4,5,PINK)
   hs.capsule(cv,12+side*6,13,12+side*9,17+frame,1.5,PINK)
  hs.ellipse(cv,12,12+frame,7,5,PINK)
  hs.ellipse(cv,12,16+frame,8,2,PINK)
  for x in (9,12,15):
   cv.rect(x,17,x,20+frame,GOLD[1]);hs.ellipse(cv,x,21,1,1,GOLD)
  hs.ellipse(cv,12,5,5,2,GREEN);cv.set(8,9,PINK[0]);cv.set(9,8,PINK[0])
 cv.outline(OUT);return cv
def bee_small(frame):
 cv=Canvas(16,8);hs.ellipse(cv,8,5,5,2,GOLD)
 cv.rect(6,3,6,6,OUT);cv.rect(9,3,9,6,OUT);cv.set(12,4,OUT)
 hs.ellipse(cv,7,1+frame,3,1,[CREAM]*4);cv.outline(OUT);return cv

def dew():
 cv=Canvas(8,8)
 for y in range(1,7):
  width=1 if y<3 else 2
  cv.rect(3-width,y,3+width,y,BLUE[min(2,y//3)])
 cv.set(2,3,CREAM);cv.outline(OUT);return cv
def bee(frame):
 cv=Canvas(24,16)
 hs.ellipse(cv,10,3+frame,4,2,[CREAM]*4);hs.ellipse(cv,16,3-frame,3,2,[CREAM]*4)
 hs.ellipse(cv,11,9,7,4,GOLD)
 for x in (8,12): cv.rect(x,6,x+1,11,OUT)
 hs.eye(cv,15,6,look=(-1,0),white=CREAM,pupil=OUT);cv.set(3,9,OUT)
 cv.outline(OUT);return cv
def trap(frame):
 cv=Canvas(24,24)
 hs.capsule(cv,12,15,12,23,1.5,GREEN)
 hs.ellipse(cv,12,11,10,7,GREEN)
 if not frame:
  cv.rect(4,8,20,12,OUT)
  for x in range(5,21,3): cv.set(x,8,CREAM);cv.set(x+1,12,CREAM)
 else:
  for x in range(4,21):cv.set(x,10,OUT)
 cv.outline(OUT);return cv
def sprite_frame(name,i):
 if name=='hero_icon':return hero_icon()
 if name.startswith('hero_'):return hero(name[5:],i)
 if name=='flower':return flower(i)
 if name=='flower_small':return flower(i,True)
 if name.startswith('flower_'):
  part=name[len('flower_'):];small=part.endswith('_small')
  if small:part=part[:-6]
  return flower(i,small,FLOWER_NAMES.index(part))
 if name=='bee_small':return bee_small(i)
 if name=='dew':return dew()
 if name=='bee':return bee(i)
 if name=='trap':return trap(i)
 raise KeyError(name)
def ground():
 cv=Canvas(512,48)
 for y in range(48):
  for x in range(512):cv.set(x,y,(120,147,107) if y<2 else (81,117,94))
 return cv
def hills():
 cv=Canvas(512,192)
 # Soft foliage silhouettes and broad twisting lianas, seamless over a vertical repeat.
 far=[(158,190,168),(174,201,175),(192,214,185)]
 for j,x in enumerate(range(20,512,88)):
  phase=j*.83
  for y in range(192):
   xx=x+round(math.sin(y*math.tau/192+phase)*16)
   cv.rect(xx-2,y,xx+1,y,far[0]);cv.set(xx-2,y,far[1])
  for y in (18,65,113,161):
   xx=x+round(math.sin(y*math.tau/192+phase)*16)
   for side in (-1,1):
    hs.capsule(cv,xx,y,xx+side*13,y-7,4,[far[1]]*4)
    hs.ellipse(cv,xx+side*13,y-7,10,5,[far[1]]*4)
    hs.capsule(cv,xx,y,xx+side*13,y-7,.5,[far[2]]*4)
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
