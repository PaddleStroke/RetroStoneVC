#!/usr/bin/env python3
"""Four expressive garden beds: original code-drawn house sprites."""
import argparse,os,sys,random
from PIL import Image
HERE=os.path.dirname(os.path.abspath(__file__));GAME=os.path.dirname(HERE);ROOT=os.path.abspath(os.path.join(GAME,'..','..'))
sys.path.insert(0,os.path.join(ROOT,'games','common','tools'))
import house_style as hs
Canvas=hs.Canvas;SHEET_W,SHEET_H=480,160
SPRITES=[('hero_idle',0,0,48,48,2,'hero'),
 ('hero_air',192,0,48,48,1,'hero'),('hero_hit',240,0,48,48,1,'hero'),
 ('farmer_front',288,0,48,48,1,'hero'),('farmer_front_strike',336,0,48,48,1,'hero'),
 ('farmer_back',384,0,48,48,1,'hero'),('farmer_back_strike',432,0,48,48,1,'hero'),('hero_icon',0,48,16,16,2,'hero'),
 ('farmer_small',32,48,24,32,2,'hero'),('farmer_small_strike',80,48,24,32,1,'hero'),
 ('farmer_small_front',104,48,24,32,1,'hero'),('farmer_small_front_strike',128,48,24,32,1,'hero'),
 ('farmer_small_back',152,48,24,32,1,'hero'),('farmer_small_back_strike',176,48,24,32,1,'hero'),
 ('rabbit',0,96,32,32,2,'props'),('hedgehog',64,96,24,16,1,'props'),('radish',96,96,16,16,1,'props'),
 ('rabbit_small',128,96,16,16,2,'props'),('hedgehog_small',160,96,16,8,1,'props'),('radish_small',176,96,8,8,1,'props')]
PANORAMAS={'ground':('tiles/ground.png',512,48),'hills':('tiles/hills.png',512,240)}
OUT=(27,31,58);CREAM=(255,246,220)
HAT=[(255,223,145),(223,175,100),(169,112,66),(169,112,66)]
SKIN=[(255,222,196),(240,184,150),(206,140,108),(206,140,108)]
APRON=[(209,168,236),(158,111,197),(100,63,143),(100,63,143)]
LEAF=[(193,223,125),(127,176,85),(73,114,64),(73,114,64)]
PINK=[(255,190,201),(227,111,145),(164,72,110),(164,72,110)]
FUR=[CREAM,(218,208,183),(170,152,132),(170,152,132)]
QUILL=[(211,166,107),(153,105,71),(98,66,56),(98,66,56)]
def hero(pose='idle',frame=0,small=False):
 cv=Canvas(48,48)
 # Side-facing gardener: grounded boots, curved apron, straw hat and profile face.
 hs.capsule(cv,15,36,13,42,3,APRON);hs.capsule(cv,25,36,27,42,3,APRON)
 cv.rect(8,42,16,45,HAT[2]);cv.rect(24,42,33,45,HAT[2]);cv.rect(9,42,15,42,HAT[0])
 hs.ellipse(cv,20,31,10,11,APRON);cv.rect(20,28,27,38,APRON[0]);cv.rect(20,33,26,38,APRON[1]);cv.set(23,29,CREAM);cv.set(23,31,CREAM)
 hs.capsule(cv,12,27,12,35+frame%2,3,SKIN)
 hand=38 if pose=='air' else 31
 hs.capsule(cv,26,27,hand,31+frame%2,3,SKIN);hs.ellipse(cv,hand,31,3,3,SKIN)
 hs.ellipse(cv,22,17,8,9,SKIN);hs.ellipse(cv,31,18,3,2,SKIN)
 hs.ellipse(cv,19,19,2,3,SKIN);cv.set(19,18,SKIN[2])
 if pose=='hit':
  cv.rect(25,14,29,14,OUT);cv.set(28,13,OUT);cv.set(27,15,OUT)
 else:hs.eye(cv,25,12,white=CREAM,pupil=OUT,big=True,look=(1,0))
 cv.rect(25,22,30,22,OUT);cv.rect(20,23,25,24,HAT[2]);cv.set(29,21,CREAM)
 hs.ellipse(cv,22,10,16,3,HAT);hs.ellipse(cv,22,6,9,5,HAT);cv.rect(13,8,29,9,APRON[2]);cv.set(17,3,HAT[0]);cv.set(20,2,HAT[0])
 cv.outline(OUT)
 if small:
  out=Canvas(16,16);im=cv.image().resize((16,16),Image.Resampling.NEAREST)
  for y in range(16):
   for x in range(16):
    col=im.getpixel((x,y));out.set(x,y,None if col==hs.MAGENTA else col)
  return out
 return cv
def farmer_small(strike=False,frame=0):
 # A deliberately redrawn profile for the shorter four-player views.
 cv=Canvas(24,32)
 cv.rect(4,29,8,31,HAT[2]);cv.rect(13,29,19,31,HAT[2])
 hs.ellipse(cv,10,23,6,7,APRON);cv.rect(11,20,15,25,APRON[0])
 hs.capsule(cv,5,19,4,24,1.5,SKIN);hs.capsule(cv,14,19,21 if strike else 18,21+frame%2,1.5,SKIN)
 hs.ellipse(cv,12,12,5,6,SKIN);cv.rect(17,12,20,14,SKIN[1]);hs.eye(cv,15,9,white=CREAM,pupil=OUT,look=(1,0));cv.rect(15,16,18,16,OUT)
 hs.ellipse(cv,11,7,9,2,HAT);hs.ellipse(cv,11,4,5,3,HAT);cv.rect(6,5,15,6,APRON[2]);cv.outline(OUT);return cv

def farmer_vertical(back=False,strike=False,small=False):
 # Separate front/back drawings keep the face, hat and apron readable at each size.
 if small:
  cv=Canvas(24,32);cx=12
  cv.rect(5,29,9,31,HAT[2]);cv.rect(15,29,19,31,HAT[2]);cv.rect(5,29,8,29,HAT[0])
  hs.ellipse(cv,12,23,6,7,APRON)
  if back:
   cv.rect(10,19,14,26,APRON[1]);cv.rect(7,21,17,22,HAT[1]);cv.set(12,22,CREAM)
   hs.capsule(cv,7,18,5,24,1.5,SKIN);hs.capsule(cv,17,18,18,13 if strike else 19,1.5,SKIN)
   hs.ellipse(cv,12,13,5,6,HAT);cv.rect(8,11,16,16,HAT[2]);cv.rect(10,16,14,18,SKIN[1])
  else:
   cv.rect(9,21,15,26,APRON[0]);cv.rect(9,23,15,26,APRON[1]);cv.set(11,21,CREAM)
   hs.capsule(cv,7,18,5,24,1.5,SKIN);hs.capsule(cv,17,18,18,25 if strike else 23,1.5,SKIN)
   hs.ellipse(cv,12,13,5,6,SKIN);hs.eye(cv,8,11,white=CREAM,pupil=OUT);hs.eye(cv,14,11,white=CREAM,pupil=OUT)
   cv.rect(11,15,13,15,SKIN[2]);cv.rect(10,17,14,17,OUT)
  hs.ellipse(cv,12,7,10,2,HAT);hs.ellipse(cv,12,4,6,3,HAT);cv.rect(6,5,18,6,APRON[2]);cv.set(9,2,HAT[0])
 else:
  cv=Canvas(48,48);cx=24
  hs.capsule(cv,18,36,16,42,3,APRON);hs.capsule(cv,30,36,32,42,3,APRON)
  cv.rect(10,42,19,45,HAT[2]);cv.rect(29,42,38,45,HAT[2]);cv.rect(11,42,18,42,HAT[0]);cv.rect(30,42,37,42,HAT[0])
  hs.ellipse(cv,24,31,10,11,APRON)
  if back:
   cv.rect(19,26,29,38,APRON[1]);hs.capsule(cv,17,24,25,32,1,APRON);hs.capsule(cv,31,24,23,32,1,APRON)
   cv.rect(16,32,32,33,HAT[1]);cv.rect(23,33,25,35,CREAM)
   hs.capsule(cv,15,26,13,35,3,SKIN);hs.capsule(cv,33,26,34,17 if strike else 24,3,SKIN)
   hs.ellipse(cv,24,17,8,9,HAT);cv.rect(18,13,30,22,HAT[2]);cv.rect(21,22,27,25,SKIN[1])
  else:
   cv.rect(19,26,29,38,APRON[0]);cv.rect(19,32,29,38,APRON[1]);cv.set(22,27,CREAM);cv.set(25,27,CREAM)
   hs.capsule(cv,15,26,13,35,3,SKIN);hs.capsule(cv,33,26,34,38 if strike else 34,3,SKIN)
   hs.ellipse(cv,24,17,8,9,SKIN);hs.ellipse(cv,16,17,2,3,SKIN);hs.ellipse(cv,32,17,2,3,SKIN)
   hs.eye(cv,18,12,white=CREAM,pupil=OUT,big=True);hs.eye(cv,26,12,white=CREAM,pupil=OUT,big=True)
   hs.ellipse(cv,24,19,2,2,SKIN);cv.rect(21,23,27,23,OUT);cv.set(24,24,CREAM)
  hs.ellipse(cv,24,10,16,3,HAT);hs.ellipse(cv,24,6,9,5,HAT);cv.rect(15,8,33,9,APRON[2]);cv.set(19,3,HAT[0]);cv.set(22,2,HAT[0])
 cv.outline(OUT);return cv
def rabbit(frame=0,small=False):
 cv=Canvas(16 if small else 32,16 if small else 32);s=1 if small else 2;cx=8*s
 for ex in (5*s,11*s):
  hs.ellipse(cv,ex,4*s,2*s,3*s,FUR);cv.rect(ex,2*s,ex,5*s,PINK[1])
 hs.ellipse(cv,cx,11*s,5*s,4*s,FUR)
 if small:
  cv.set(5,9,OUT);cv.set(10,9,OUT);cv.set(8,11,PINK[1]);cv.rect(6,14,10,14,FUR[1])
 else:
  hs.eye(cv,10,17,white=CREAM,pupil=OUT,big=True);hs.eye(cv,20,17,white=CREAM,pupil=OUT,big=True)
  hs.ellipse(cv,16,23,2,1,PINK);cv.rect(15,25,17,27,CREAM)
  hs.ellipse(cv,9,28,4,2,FUR);hs.ellipse(cv,23,28,4,2,FUR)
  cv.set(5,23+frame,FUR[2]);cv.set(27,23+frame,FUR[2])
 cv.outline(OUT);return cv
def hedgehog(small=False):
 cv=Canvas(16 if small else 24,8 if small else 16)
 hs.ellipse(cv,cv.w//2-2,cv.h-6,cv.w//2-4,cv.h//2-2,QUILL)
 for x in range(3,cv.w-7,3):
  top=2 if small else 2+(x//3)%2;cv.set(x,top,QUILL[2]);cv.set(x+1,top+1,QUILL[0])
 hs.ellipse(cv,cv.w-7,cv.h-5,4,3,FUR);cv.set(cv.w-2,cv.h-5,OUT);cv.set(cv.w-7,cv.h-7,OUT)
 cv.rect(4,cv.h-2,6,cv.h-2,QUILL[2]);cv.set(cv.w-7,cv.h-2,QUILL[2]);cv.outline(OUT);return cv
def radish(small=False):
 cv=Canvas(8 if small else 16,8 if small else 16);s=1 if small else 2
 hs.ellipse(cv,4*s,5*s,2*s,2*s,PINK)
 hs.capsule(cv,4*s,3*s,2*s,1*s,1*s,LEAF);hs.capsule(cv,4*s,3*s,6*s,1*s,1*s,LEAF)
 cv.set(4*s,7*s,CREAM);cv.outline(OUT);return cv
def sprite_frame(name,i):
 if name=='hero_icon':return hero(frame=i,small=True)
 if name.startswith('farmer_') and ('front' in name or 'back' in name):return farmer_vertical('back' in name,name.endswith('strike'),'small' in name)
 if name.startswith('farmer_small'):return farmer_small(name.endswith('strike'),i)
 if name.startswith('hero_'):return hero(name[5:],i)
 return {'rabbit':lambda:rabbit(i),'rabbit_small':lambda:rabbit(i,True),'hedgehog':hedgehog,'hedgehog_small':lambda:hedgehog(True),'radish':radish,'radish_small':lambda:radish(True)}[name]()
def ground():
 cv=Canvas(512,48);cv.rect(0,0,511,47,LEAF[1]);return cv
def hills():
 # Quiet garden beyond the beds: wooded hedgerow, picket fence and sunlit grass.
 cv=Canvas(512,240)
 P=[(220,236,202),(187,213,173),(153,189,136),(112,156,107),(78,124,87),(210,217,156),(179,199,130),(151,177,109),(122,151,91),(228,204,153),(184,149,104),(133,105,77),(238,235,188),(105,138,89),(201,216,142)]
 cv.rect(0,0,511,239,P[0]);cv.rect(0,52,511,239,P[6])
 for x in range(-20,530,43):
  hs.ellipse(cv,x,25+(x//43%3)*5,31,24,[P[1],P[2],P[3],P[4]])
  cv.rect(x+6,34,x+10,59,P[11])
 cv.rect(0,42,511,44,P[11]);cv.rect(0,51,511,53,P[11])
 for x in range(0,512,14):
  top=37+(x//14%2);cv.rect(x+3,top,x+9,62,P[10]);cv.rect(x+3,top,x+5,61,P[9]);cv.set(x+6,top-2,P[9]);cv.rect(x+4,top-1,x+8,top-1,P[9]);cv.rect(x+4,61,x+11,63,P[8])
 rng=random.Random(8)
 for _ in range(260):
  x=rng.randrange(512);y=rng.randrange(69,239);cv.rect(x,y,x+2,y,P[7] if y&1 else P[5]);cv.set(x+1,y-1,P[14])
 for x,y in [(28,78),(290,74),(17,202),(304,201),(367,135),(436,94)]:
  hs.ellipse(cv,x,y,13,9,[P[14],P[7],P[8],P[13]])
  for dx,dy in [(-6,-2),(0,-5),(5,0)]:hs.ellipse(cv,x+dx,y+dy,2,2,[P[12]]*4)
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
