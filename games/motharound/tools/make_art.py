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
PANORAMAS={'ground':('tiles/ground.png',512,48),'hills':('tiles/hills.png',512,96),'hills_wide':('tiles/hills_wide.png',1024,160)}
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
# ---- widescreen (DESIGN.md "Widescreen") -----------------------------------------------------------------------
class WrapCanvas(Canvas):
 """a canvas whose x wraps round: stage x s is picture x s mod w (a panorama on a wrapping map)"""
 def set(self,x,y,c):Canvas.set(self,x%self.w,y,c)
def hills_wide():
 """The garden 1024 px wide and 160 high (screen y 80..239) for a screen wider than 320 (a 128-tile map, stage x s
 = picture x s & 1023): hills() pixel for pixel in its bottom 96 rows over the lantern's 320 columns, the hills, the
 fence and the hedge going on round in stage coordinates; above the hedge beyond the 320 columns, the garden's
 taller things: foxgloves and a rose arch on the left, a birdhouse on its pole and a sunflower on the right."""
 W=1024;cv=WrapCanvas(W,160);o=64
 for x in range(-W//2,W//2):
  top=30+int(11*math.sin(x/41)+7*math.sin(x/79))
  cv.rect(x,o+top,x,o+95,(45,63,87))
 for x in range(-24*21,W//2,24):
  cv.rect(x,o+52,x+3,o+90,(53,75,89));cv.rect(x,o+55,x+1,o+89,(63,86,96))
 cv.rect(0,o+65,W-1,o+67,(53,75,89))
 hedge=[]
 for x in range(-17*30,W//2,17):
  top=79+int(6*math.sin(x/29))
  hedge.append((x,top))
 WOOD=[(104,86,90),(74,62,72)];PINK=[(196,118,150),(138,78,112)];VIOLET=(156,124,186);SUN=[(214,178,98),(150,112,70)]
 STEM=(63,86,96);LEAFD=(51,80,86)
 # behind the hedge: the stems, posts and pole stand in it
 for sx in (-40,-30,-20):                                         # foxgloves
  h=[44,30,52][(sx+40)//10]
  cv.rect(sx,o+h,sx,o+82,STEM)
  for k,y in enumerate(range(o+h+2,o+h+34,4)):
   for dx in (-2,1):cv.rect(sx+dx,y,sx+dx+1,y+2,VIOLET if (k+dx)%3 else PINK[0]);cv.set(sx+dx+(1 if dx<0 else 0),y+2,PINK[1])
  for y in (o+70,o+76):cv.rect(sx-4,y,sx-1,y+1,LEAFD);cv.rect(sx+1,y-3,sx+4,y-2,LEAFD)
 for px in (-98,-68):                                             # the rose arch
  cv.rect(px,o+10,px+3,o+84,WOOD[1])
  cv.rect(px,o+10,px,o+84,WOOD[0])
 for a in range(0,181,3):
  ang=math.radians(a);x=-81.5+17*math.cos(ang);y=o+10-14*math.sin(ang)
  cv.rect(int(round(x))-1,int(round(y)),int(round(x))+2,int(round(y))+2,WOOD[1])
 for k,(x,y) in enumerate(((-98,o-2),(-91,o-6),(-84,o-4),(-75,o-5),(-68,o+1),(-99,o+14),(-67,o+18),(-98,o+32),(-66,o+40),(-97,o+52),(-67,o+60))):
  cv.rect(x-2,y,x+3,y+3,LEAFD);cv.rect(x-1,y+1,x+1,y+2,PINK[k%2]);cv.set(x,y+1,PINK[0])
 cv.rect(347,o-20,349,o+84,WOOD[1]);cv.rect(347,o-20,347,o+84,WOOD[0])   # the birdhouse on its pole
 cv.rect(338,o-40,358,o-20,WOOD[0]);cv.rect(352,o-40,358,o-20,WOOD[1])
 for y in range(o-52,o-38):
  half=(y-(o-52))*13//14
  cv.rect(348-half,y,348+half,y,PINK[1])
 cv.rect(346,o-34,350,o-30,(36,61,74));cv.rect(347,o-35,349,o-29,(36,61,74));cv.rect(344,o-26,352,o-25,WOOD[1])
 cv.rect(405,o-24,406,o+82,STEM)                                  # a sunflower, its head nodding
 for y,s in ((o+10,-1),(o+30,1),(o+52,-1)):hs.ellipse(cv,405.5+s*5,y,5,2,[LEAFD]*4)
 for a in range(0,360,24):
  ang=math.radians(a);c,s_=math.cos(ang),math.sin(ang)
  hs.capsule(cv,405.5+5*c,o-29.5+5*s_,405.5+9.5*c,o-29.5+9.5*s_,1.6,[SUN[0],SUN[0],SUN[1],SUN[1]])
 hs.ellipse(cv,405.5,o-29.5,5,5,[SUN[1],WOOD[1],WOOD[1],WOOD[1]])
 # the hedge in front of them all, as in hills()
 for x,top in hedge:
  hs.ellipse(cv,x,o+top+10,12,11,[(68,95,99),(51,80,86),(36,61,74),(36,61,74)])
  for k in range(3):hs.ellipse(cv,x+k*3-4,o+top+5-k*2,3,1.5,[(76,106,102)]*4)
 return cv
def write_all(out,preview=False):
 os.makedirs(os.path.join(out,'tiles'),exist_ok=True);sheet=Image.new('RGB',(SHEET_W,SHEET_H),hs.MAGENTA)
 for name,x,y,w,h,n,g in SPRITES:
  for i in range(n):sheet.paste(sprite_frame(name,i).image(),(x+i*w,y))
 for f,im in {'sprites.png':sheet,'tiles/ground.png':ground().image(),'tiles/hills.png':hills().image(),
              'tiles/hills_wide.png':hills_wide().image()}.items():
  im.save(os.path.join(out,f))
  if preview:
   os.makedirs(os.path.join(out,'preview'),exist_ok=True)
   im.resize((im.width*4,im.height*4),Image.Resampling.NEAREST).save(os.path.join(out,'preview',os.path.basename(f)))
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--out',default=os.path.join(GAME,'art'));ap.add_argument('--preview',action='store_true')
 a=ap.parse_args();write_all(a.out,a.preview)
