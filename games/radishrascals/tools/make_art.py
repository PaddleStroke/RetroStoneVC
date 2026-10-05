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
PANORAMAS={'ground':('tiles/ground.png',512,48),'hills':('tiles/hills.png',512,240),'hills_wide':('tiles/hills-wide.png',1024,240)}
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
# ---- widescreen (DESIGN.md "Widescreen"): the garden goes on beside the 320-px stage ------------------------------
# hills-wide.png is a 1024-px panorama (a 128-tile map) in stage coordinates: canvas x = stage x, and x - 1024 for the
# columns left of the stage (a centred 560-px screen shows stage x -120..439). Stage x 0..319 is hills.png pixel for
# pixel (the 320 picture), 320..439 too (it already goes on there) with more garden drawn over it; left of the stage
# the hedgerow and the fence go on (the same formulas) over the grass of hills.png 392..511. Decor colours outside the
# hills' 15 use one more BG palette (WIDE_PAL, build_assets.py).
WIDE_W=1024
HILL_P=[(220,236,202),(187,213,173),(153,189,136),(112,156,107),(78,124,87),(210,217,156),(179,199,130),(151,177,109),(122,151,91),(228,204,153),(184,149,104),(133,105,77),(238,235,188),(105,138,89),(201,216,142)]   # hills()'s P
SOIL=(98,74,56);ORANGE=(236,152,76);ORANGE_D=(186,98,60)
WIDE_PAL=[HILL_P[6],HILL_P[7],HILL_P[8],HILL_P[14],HILL_P[11],HILL_P[10],HILL_P[9],SOIL,HILL_P[4],LEAF[0],LEAF[1],PINK[0],PINK[1],ORANGE,ORANGE_D]
def hedge_band(s0,s1):
 # the hedgerow's and the fence's rows (y < 64) for stage x s0..s1-1, drawn by the formulas of hills()
 P=HILL_P;off=-s0+64;cv=Canvas(s1-s0+128,64)
 cv.rect(0,0,cv.w-1,63,P[0]);cv.rect(0,52,cv.w-1,63,P[6])
 for x in range(-20-43*((s0+20)//-43+2),s1+43,43):
  hs.ellipse(cv,x+off,25+(x//43%3)*5,31,24,[P[1],P[2],P[3],P[4]])
  cv.rect(x+6+off,34,x+10+off,59,P[11])
 cv.rect(0,42,cv.w-1,44,P[11]);cv.rect(0,51,cv.w-1,53,P[11])
 for x in range(s0-s0%14-28,s1+14,14):
  top=37+(x//14%2);c=x+off;cv.rect(c+3,top,c+9,62,P[10]);cv.rect(c+3,top,c+5,61,P[9]);cv.set(c+6,top-2,P[9]);cv.rect(c+4,top-1,c+8,top-1,P[9]);cv.rect(c+4,61,c+11,63,P[8])
 return cv,off
class Stage:
 # a transparent decor layer in stage coordinates (stage x -128..447)
 def __init__(self):self.off=128;self.cv=Canvas(576,240)
 def set(self,x,y,c):self.cv.set(x+self.off,y,c)
 def rect(self,x0,y0,x1,y1,c):self.cv.rect(x0+self.off,y0,x1+self.off,y1,c)
 def ellipse(self,cx,cy,rx,ry,r4,flat=None):hs.ellipse(self.cv,cx+self.off,cy,rx,ry,r4,flat)
 def line(self,ax,ay,bx,by,c):
  n=max(abs(bx-ax),abs(by-ay),1)
  for k in range(n+1):self.set(ax+(bx-ax)*k//n,ay+(by-ay)*k//n,c)
def furrow(d,s0,s1,y):
 # a row of the kitchen garden: a strip of dug soil, darker at its top edge
 d.rect(s0,y,s1,y+5,SOIL);d.rect(s0+1,y,s1-1,y,HILL_P[11]);d.rect(s0,y+6,s1,y+6,HILL_P[8])
 for x in range(s0+3,s1-2,5):d.set(x,y+3,HILL_P[11])
def cabbage(d,x,y):
 d.ellipse(x+1,y+2,6,4,None,HILL_P[8]);d.ellipse(x,y,6,4.5,[LEAF[0],LEAF[1],HILL_P[8],HILL_P[4]])
 d.ellipse(x-1,y-1,3,2.5,[LEAF[0],LEAF[0],LEAF[1],HILL_P[8]]);d.set(x-2,y-2,HILL_P[14]);d.line(x+2,y-2,x+4,y+1,HILL_P[8])
def carrot(d,x,y):
 for dx,h in ((-2,5),(0,7),(2,5)):d.line(x,y,x+dx,y-h,LEAF[1]);d.set(x+dx,y-h,LEAF[0]);d.set(x+dx-(1 if dx<=0 else -1),y-h+2,LEAF[0])
 d.rect(x-1,y+1,x+1,y+2,ORANGE);d.set(x+1,y+2,ORANGE_D)
def kitchen_garden(d):
 # left of the stage: three rows of vegetables (cabbages, carrots, cabbages) in the ground, unlike the raised beds
 for i,y in enumerate((118,140,162)):
  furrow(d,-118,-60,y)
  for x in range(-112,-62,14 if i!=1 else 7):
   if i==1:carrot(d,x+2,y+1)
   else:cabbage(d,x+(7 if i==2 else 1),y)
def cosmos(d,x,y,heads):
 # a clump of pink garden flowers
 d.ellipse(x+1,y+10,7,3,None,HILL_P[8])
 for hx,hy in heads:d.line(x,y+10,x+hx,y+hy,LEAF[1])
 for lx,ly in ((-4,7),(3,6),(-2,4)):d.rect(x+lx,y+ly,x+lx+1,y+ly,HILL_P[4])
 for hx,hy in heads:
  for ex,ey in ((0,-1),(-1,0),(1,0),(0,1)):d.set(x+hx+ex,y+hy+ey,PINK[1])
  d.set(x+hx-1,y+hy-1,PINK[0]);d.set(x+hx,y+hy,ORANGE)
def wheelbarrow(d,x,y):
 # a wooden wheelbarrow with a heap of soil and two pulled radishes; x, y: the wheel's centre
 W=HILL_P;d.ellipse(x+15,y+5,17,3,None,W[8])
 for k in range(11):
  l,r=x+3+k*4//10,x+29-k*4//10;d.rect(l,y-13+k,r,y-13+k,W[10]);d.set(l,y-13+k,W[11]);d.set(r,y-13+k,W[11])
 d.ellipse(x+16,y-13,11,3.5,None,SOIL);d.rect(x+3,y-13,x+29,y-13,W[9]);d.rect(x+4,y-12,x+28,y-12,W[11])
 d.rect(x+4,y-7,x+28,y-7,W[11])
 for rx in (x+12,x+20):d.ellipse(rx,y-16,2.2,2.2,[PINK[0],PINK[1],PINK[1],PINK[1]]);d.line(rx,y-18,rx-2+(rx-x-12)//2,y-22,LEAF[1]);d.set(rx+1,y-20,LEAF[0])
 d.line(x+28,y-6,x+38,y-2,W[11]);d.line(x+28,y-7,x+38,y-3,W[10]);d.line(x+23,y-3,x+24,y+3,W[11])
 d.ellipse(x,y,5,5,None,W[11]);d.ellipse(x,y,3.2,3.2,None,W[10]);d.rect(x,y,x,y,W[9]);d.set(x-1,y-1,W[9])
def pot(d,x,y,w,h):
 # a terracotta flower pot; x, y: the top of its rim (its centre)
 d.ellipse(x+1,y+h+1,w//2+2,2,None,HILL_P[8])
 for k in range(h):
  half=w//2-k*2//max(1,h-1)
  for xx in range(x-half,x+half+1):d.set(xx,y+k,ORANGE_D if xx>=x+half-2 or k<2 and xx>=x+half-3 else ORANGE)
 d.rect(x-w//2-1,y,x+w//2+1,y+2,ORANGE);d.rect(x+w//2-1,y,x+w//2+1,y+2,ORANGE_D);d.rect(x-w//2-1,y,x+w//2+1,y,HILL_P[9])
 d.rect(x-w//2,y+3,x+w//2,y+3,ORANGE_D)
def pumpkin(d,x,y,rx,ry):
 d.ellipse(x+1,y+ry-1,rx+2,2.5,None,HILL_P[8]);d.ellipse(x,y,rx,ry,[HILL_P[9],ORANGE,ORANGE_D,HILL_P[11]])
 for k in (-rx//2,rx//2):d.line(x+k,y-ry+2,x+k,y+ry-2,ORANGE_D)
 d.rect(x-1,y-ry-2,x,y-ry,HILL_P[11]);d.set(x+1,y-ry-2,LEAF[1])
def pumpkin_patch(d):
 # right of the stage, by the far corner: a vine with leaves and three pumpkins
 pts=[(378,214),(388,209),(398,215),(410,210),(422,216),(434,211),(444,214)]
 for (ax,ay),(bx,by) in zip(pts,pts[1:]):d.line(ax,ay,bx,by,LEAF[1]);d.line(ax,ay+1,bx,by+1,HILL_P[4])
 for lx,ly in ((384,205),(404,206),(428,206),(440,219),(394,221)):d.ellipse(lx,ly,4,3,[LEAF[0],LEAF[1],HILL_P[8],HILL_P[4]])
 pumpkin(d,392,214,8,6);pumpkin(d,416,212,10,7);pumpkin(d,433,222,6,5)
def hills_wide():
 base=hills();P=HILL_P;cv=Canvas(WIDE_W,240)
 for y in range(240):
  for x in range(WIDE_W):cv.p[y][x]=base.p[y][x%512]
 band,off=hedge_band(-128,16)
 for y in range(64):
  assert all(band.p[y][s+off]==base.p[y][s] for s in range(16)),'the hedgerow must go on seamlessly'
  for s in range(-128,0):cv.p[y][s+WIDE_W]=band.p[y][s+off]
 d=Stage()
 kitchen_garden(d)
 cosmos(d,-30,90,((-5,-3),(0,-6),(4,-2),(-2,1),(6,3)))
 wheelbarrow(d,-46,192)
 cosmos(d,338,100,((-4,-2),(1,-5),(5,-1),(-1,2)))
 pot(d,333,178,13,11);cosmos(d,333,166,((-3,-1),(1,-4),(3,1)))
 pot(d,349,186,9,8);d.line(349,185,349,181,LEAF[1]);d.set(348,180,LEAF[0]);d.set(350,181,LEAF[0])
 pumpkin_patch(d)
 for y in range(240):
  for s in range(-128,448):
   c=d.cv.p[y][s+d.off]
   if c is not None:cv.p[y][s%WIDE_W]=c
 for y in range(240):
  assert all(cv.p[y][x]==base.p[y][x] for x in range(320)),'the 320-px stage must stay the 4:3 picture'
 return cv
def write_all(out,preview=False):
 os.makedirs(os.path.join(out,'tiles'),exist_ok=True);sheet=Image.new('RGB',(SHEET_W,SHEET_H),hs.MAGENTA)
 for name,x,y,w,h,n,g in SPRITES:
  for i in range(n):sheet.paste(sprite_frame(name,i).image(),(x+i*w,y))
 for f,im in {'sprites.png':sheet,'tiles/ground.png':ground().image(),'tiles/hills.png':hills().image(),'tiles/hills-wide.png':hills_wide().image()}.items():
  im.save(os.path.join(out,f))
  if preview:
   os.makedirs(os.path.join(out,'preview'),exist_ok=True);im.resize((im.width*4,im.height*4),Image.Resampling.NEAREST).save(os.path.join(out,'preview',os.path.basename(f)))
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--out',default=os.path.join(GAME,'art'));ap.add_argument('--preview',action='store_true');a=ap.parse_args();write_all(a.out,a.preview)
