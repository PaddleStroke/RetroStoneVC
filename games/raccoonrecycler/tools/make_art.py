#!/usr/bin/env python3
"""Raccoon Recycler: masked raccoons and three readable animal toys in three colours."""
import os,sys,math,random
from PIL import Image
HERE=os.path.dirname(os.path.abspath(__file__));GAME=os.path.dirname(HERE);ROOT=os.path.abspath(os.path.join(GAME,'..','..'))
sys.path.insert(0,os.path.join(ROOT,'games','common','tools'))
import house_style as hs
Canvas=hs.Canvas
def polygon(c,pts,col):
 for y in range(c.h):
  cuts=[]
  for (ax,ay),(bx,by) in zip(pts,pts[1:]+pts[:1]):
   if (ay<=y<by) or (by<=y<ay):cuts.append(ax+(y-ay)*(bx-ax)/(by-ay))
  cuts.sort()
  for a,b in zip(cuts[::2],cuts[1::2]):c.rect(math.ceil(a),y,math.floor(b),y,col)
SHEET_W,SHEET_H=320,128
SPRITES=[('hero_idle',0,0,32,32,2,'hero'),('hero_left',64,0,32,32,1,'hero'),('hero_right',96,0,32,32,1,'hero'),('hero_hit',128,0,32,32,1,'hero'),('hero_icon',160,0,16,16,1,'hero'),('belt_mark',176,0,8,8,1,'props'),('gate_left',192,0,32,32,1,'hero'),('gate_right',224,0,32,32,1,'hero'),('gate_center',256,0,32,32,1,'hero')]+[('item_%d'%i,i*32,32,32,32,1,'props') for i in range(9)]+[(n,i*32,64,32 if i<6 else 8,32 if i<6 else 8,1,'props') for i,n in enumerate(['badge_duck','badge_star','badge_snail','badge_yellow','badge_white','badge_blue','heart','heart_empty','lamp'])]
PANORAMAS={'ground':('tiles/ground.png',512,240),'hills':('tiles/hills.png',512,240),'ground_wide':('tiles/ground_wide.png',1024,240)}
O=(30,30,52);CREAM=(255,246,220)
FUR=[(224,208,170),(192,169,131),(143,123,102),(88,73,69)];MASK=[(74,73,89),(51,49,68),(37,36,56),(30,30,52)]
APRON=[(198,220,108),(146,182,70),(92,134,46),(52,86,30)]
def ell(c,x,y,rx,ry,r):hs.ellipse(c,x,y,rx,ry,r)
def line(c,ax,ay,bx,by,col,width=1):hs.capsule(c,ax,ay,bx,by,width,[col]*4)
def hero(pose='idle',frame=0):
 c=Canvas(32,32)
 # Striped fluffy tail, behind the apron and legs.
 ell(c,24,23,5,7,FUR)
 c.rect(22,18,28,20,MASK[1]);c.rect(22,24,28,26,MASK[1])
 ell(c,11,29,3,2,MASK);ell(c,20,29,3,2,MASK)
 ell(c,15,23,7,7,APRON);c.rect(11,19,19,23,APRON[0]);c.rect(14,24,18,27,APRON[2]);c.set(14,24,CREAM)
 ell(c,9,8,4,4,FUR);ell(c,22,8,4,4,FUR);ell(c,9,8,2,2,MASK);ell(c,22,8,2,2,MASK)
 ell(c,15,13,10,8,FUR);ell(c,15,14,9,4,MASK)
 if pose=='hit':
  for ex in (10,18):
   line(c,ex,12,ex+3,15,CREAM,.6);line(c,ex+3,12,ex,15,CREAM,.6)
 else:
  hs.eye(c,10,11,look=(-1 if pose=='left' else 1 if pose=='right' else 0,0),big=True);hs.eye(c,18,11,look=(-1 if pose=='left' else 1 if pose=='right' else 0,0),big=True)
 ell(c,15,18,5,3,[CREAM,FUR[0],FUR[1],FUR[2]]);ell(c,15,16,2,1.5,MASK);c.rect(14,20,16,20,MASK[1])
 if pose=='left':line(c,9,22,2,18,FUR[1],2)
 elif pose=='right':line(c,21,22,29,18,FUR[1],2)
 else:ell(c,9,25,2,3,FUR);ell(c,21,25,2,3,FUR)
 c.outline(O);return c
# A shared prop palette keeps all nine toys saturated and readable.
Y=[(255,237,128),(245,201,77),(194,133,52),(124,83,49)]
R=[(242,138,112),(202,77,81),(124,83,49),(30,30,52)]
G=[(198,220,108),(146,182,70),(92,134,46),(30,30,52)]
B=[(187,232,240),(104,182,209),(67,115,161),(30,30,52)]
W=[CREAM,(224,208,170),(124,83,49),(30,30,52)]
# Gold, white and blue toys, orange bills and neutral rule signs.
WHITE=[(255,255,243),(214,225,220),(166,184,181),O]
ORANGE=(235,139,61)
GREY=[WHITE[0],WHITE[1],B[2],O]
def animal(species,color=0,neutral=False):
 c=Canvas(32,32);r=GREY if neutral else [Y,WHITE,B][color]
 if species==0:
  # Low round bath-duck body, upright head, flat projecting bill, short tail.
  ell(c,13,22,10,6,r);ell(c,20,12,6,7,r)
  c.rect(25,13,30,16,GREY[2] if neutral else ORANGE);c.set(29,17,O)
  polygon(c,[(3,22),(2,16),(9,19),(10,24)],r[1]);ell(c,12,22,5,3,[r[1]]*4)
  line(c,10,20,15,20,r[0],.6);c.rect(20,10,21,12,O);c.set(20,10,WHITE[0])
 elif species==1:
  # Five rounded arms and a plump centre make a recognisable sea star.
  pts=[]
  for k in range(10):
   a=-math.pi/2+k*math.pi/5;rad=13 if k%2==0 else 6
   pts.append((16+math.cos(a)*rad,16+math.sin(a)*rad))
  polygon(c,pts,r[2]);polygon(c,[(x,y-1) for x,y in pts],r[1])
  polygon(c,[(16,5),(18,13),(24,11),(20,16),(25,23),(17,20),(9,25),(12,17),(5,12),(13,13)],r[0])
  ell(c,16,16,5,5,[r[1]]*4)
  for x,y in [(16,8),(8,13),(24,13),(11,24),(22,24)]:c.set(x,y,r[2])
  c.set(13,15,O);c.set(18,15,O);line(c,14,19,17,19,r[2],.5)
 else:
  # The neutral body and eye stalks stay constant; the spiral shell changes colour.
  body=GREY if neutral else [WHITE[1],WHITE[2],B[2],O]
  ell(c,16,25,13,4,body);ell(c,26,20,4,6,body)
  line(c,24,18,23,11,body[1],1.5);line(c,28,18,29,10,body[1],1.5)
  ell(c,23,10,2,2,[WHITE[0]]*4);ell(c,29,9,2,2,[WHITE[0]]*4);c.set(24,10,O);c.set(30,9,O)
  ell(c,13,16,10,10,r);ell(c,12,15,7,7,[r[0]]*4)
  for k in range(70):
   a=k*.13;rad=7-k*.085;c.set(round(13+math.cos(a)*rad),round(16+math.sin(a)*rad),r[2])
  line(c,7,27,20,27,body[2],.6)
 c.outline(O);return c
def item(i):return animal(i//3,i%3)
def badge(name):
 if name in ('badge_duck','badge_star','badge_snail'):return animal(['badge_duck','badge_star','badge_snail'].index(name),neutral=True)
 if name.startswith('badge'):
  c=Canvas(32,32);r={'badge_white':WHITE,'badge_yellow':Y,'badge_blue':B}[name]
  c.rect(5,7,26,24,r[2]);c.rect(6,8,25,22,r[1]);c.rect(7,9,24,20,r[0]);c.outline(O);return c
 c=Canvas(8,8)
 if name=='lamp':ell(c,4,4,3,3,Y)
 else:
  col=ORANGE if name=='heart' else GREY[2]
  polygon(c,[(0,2),(2,0),(4,2),(6,0),(7,2),(7,4),(4,7),(0,4)],col)
 c.outline(O);return c
def gate(side):
 c=Canvas(32,32)
 # Side blades block the opposite chute; the centre opens a straight run.
 if side:
  line(c,16,16,16-side*13,3,O,1.5);line(c,16,15,16-side*13,2,APRON[1],.8)
  line(c,16,14,16-side*12,2,APRON[0],.5)
 else:
  for s in [-1,1]:
   line(c,16+s*9,3,16+s*5,16,O,1.4);line(c,16+s*9,3,16+s*5,16,APRON[0],.7)
 ex,ey=16+side*12,29 if not side else 25
 line(c,16,17,ex,ey,O,1.5);line(c,16,17,ex,ey,APRON[1],.8)
 if side:polygon(c,[(ex,ey),(ex-side*6,ey-7),(ex-side*7,ey+1)],APRON[0])
 else:polygon(c,[(16,30),(11,24),(21,24)],APRON[0])
 ell(c,16,16,3,3,[FUR[0],FUR[1],MASK[1],O]);c.set(15,15,CREAM)
 return c
# Scenery palette: 15 deliberately muted colors; warm wood and shaded green/blue machines.
P=[(222,236,216),(189,212,192),(145,178,160),(101,146,131),(63,109,96),(55,64,77),(99,112,122),(155,172,172),(211,190,147),(173,145,103),(123,101,78),(83,68,60),(164,190,115),(88,143,116),(93,145,177)]
# Widescreen (DESIGN.md "Widescreen"): a screen W columns wide centres the 320-px stage and shows stage x -m .. 320+m
# (m = (W-320)/2, at most 120). The wide ground is a 1024-px (128-tile) map: map x = stage x & 1023, drawn for stage
# x WIDE_L .. WIDE_R (the rest stays blank: no tiles). Stage x 0..319 are the classic picture pixel for pixel; the
# margins go on with the yard: the fence and its trees, the workshop shed, crates, a barrel and sacks on the left,
# the recycling skip, cardboard and a hand cart on the right. Only the classic ground's colours (the same palette).
WIDE_L,WIDE_R,WIDE_W=-120,440,1024
class Stage(Canvas):
 """a canvas addressed in stage x (x0 .. x1)"""
 def __init__(s,x0,x1,h):Canvas.__init__(s,x1-x0,h);s.ox=-x0
 def set(s,x,y,c):Canvas.set(s,x+s.ox,y,c)
 def get(s,x,y):return Canvas.get(s,x+s.ox,y)
def crate(c,x,y,w,h):
 c.rect(x,y,x+w-1,y+h-1,P[10]);c.rect(x+1,y+1,x+w-2,y+h-2,P[9]);c.rect(x+1,y+1,x+w-2,y+1,P[8])
 for yy in range(y+6,y+h-2,5):c.rect(x+1,yy,x+w-2,yy,P[10])
 c.rect(x+2,y+2,x+2,y+h-2,P[8]);c.rect(x+w-3,y+2,x+w-3,y+h-2,P[10])
def barrel(c,x,y,w,h):
 c.rect(x,y+1,x+w-1,y+h-1,P[5]);c.rect(x+1,y,x+w-2,y,P[5]);c.rect(x+1,y+1,x+w-2,y+h-2,P[14]);c.rect(x+2,y+2,x+3,y+h-3,P[7])
 c.rect(x+w-3,y+2,x+w-2,y+h-2,P[6]);c.rect(x+1,y+1,x+w-2,y+2,P[6]);c.rect(x+1,y+1,x+w-2,y+1,P[7])
 for yy in (y+5,y+h-5):c.rect(x+1,yy,x+w-2,yy,P[5])
def recycle_sign(c,cx,cy,r,col):
 # three chasing arrows, as the triangle of the recycling mark
 pts=[(cx+r*math.cos(a),cy+r*math.sin(a)) for a in (-math.pi/2,math.pi/6,5*math.pi/6)]
 for (ax,ay),(bx,by) in zip(pts,pts[1:]+pts[:1]):
  mx,my=ax+(bx-ax)*.62,ay+(by-ay)*.62;line(c,ax,ay,mx,my,col,.7)
  polygon(c,[(mx+(by-ay)*.2,my-(bx-ax)*.2),(mx-(by-ay)*.2,my+(bx-ax)*.2),(ax+(bx-ax)*.85,ay+(by-ay)*.85)],col)
def sack(c,x,y,rx,ry):
 # a burlap sack of toys: a round body, its neck tied with string and the cloth flaring above
 polygon(c,[(x-5,y-ry-5),(x+5,y-ry-5),(x+2,y-ry+1),(x-2,y-ry+1)],P[5]);polygon(c,[(x-4,y-ry-4),(x+4,y-ry-4),(x+1,y-ry+1),(x-1,y-ry+1)],P[9])
 ell(c,x,y,rx+1,ry+1,[P[5]]*4);ell(c,x,y,rx,ry,[P[8],P[8],P[9],P[10]]);c.rect(x-2,y-ry-1,x+2,y-ry,P[10])
 c.rect(x-rx//2,y+1,x-rx//2+1,y+2,P[10]);c.rect(x+2,y-2,x+3,y-1,P[10])
def toy(c,x,y,col):ell(c,x,y,3.5,3,[P[0],col,col,P[5]])
def wide_yard(c):
 """the margins' props (stage x < 0 and >= 320; drawn after the classic scene, never inside the stage)"""
 # left: the workshop shed with its open door (shelves of toys waiting), its window and the recycling sign
 c.rect(-124,61,-58,152,P[9])
 for x in range(-120,-58,7):c.rect(x,63,x,151,P[10])
 c.rect(-124,61,-58,63,P[10]);c.rect(-59,61,-58,152,P[10]);c.rect(-124,149,-58,152,P[5])
 c.rect(-128,50,-54,52,P[5]);c.rect(-127,53,-55,58,P[6]);c.rect(-127,53,-55,53,P[7]);c.rect(-128,59,-54,60,P[5])
 c.rect(-110,94,-86,152,P[5]);c.rect(-108,96,-88,152,P[10])
 for yy,cols in ((112,(P[14],P[13],P[0])),(130,(P[13],P[0],P[14]))):
  c.rect(-108,yy,-88,yy+1,P[9]);c.rect(-108,yy+2,-88,yy+2,P[5])
  for k,col in enumerate(cols):toy(c,-103+k*7,yy-3,col)
 c.rect(-80,76,-64,92,P[5]);c.rect(-78,78,-66,90,P[14]);c.rect(-78,78,-66,79,P[7]);c.rect(-73,78,-72,90,P[5]);c.rect(-78,84,-66,85,P[5]);c.set(-77,81,P[0]);c.set(-76,80,P[0])
 c.rect(-107,70,-89,88,P[5]);c.rect(-106,71,-90,87,P[13]);recycle_sign(c,-98,80,6,P[0])
 # a blue barrel and the crate stack by the fence (the top crate full of toys to sort)
 barrel(c,-66,128,13,25)
 crate(c,-46,134,18,19);crate(c,-27,134,18,19)
 toy(c,-33,115,P[14]);toy(c,-26,116,P[13]);toy(c,-30,113,P[0]);crate(c,-37,116,18,18)
 # left, in the yard: a pallet of sacks
 c.rect(-106,216,-56,219,P[9]);c.rect(-106,216,-56,216,P[8])
 for x in range(-106,-56,10):c.rect(x,220,x+3,223,P[10])
 c.rect(-104,224,-58,225,P[9])
 sack(c,-93,205,11,10);sack(c,-70,206,10,9);sack(c,-82,191,9,8)
 # right: the recycling skip (green, its lid, ribs and mark), cardboard bales behind it on the right
 for (x,y,w,h) in ((418,104,24,22),(414,126,30,27)):
  c.rect(x,y,x+w-1,y+h-1,P[10]);c.rect(x+1,y+1,x+w-2,y+h-2,P[8]);c.rect(x+w-6,y+1,x+w-2,y+h-2,P[9]);c.rect(x+1,y+h//2,x+w-2,y+h//2+1,P[9])
 polygon(c,[(342,111),(415,111),(410,150),(347,150)],P[5]);polygon(c,[(344,112),(413,112),(409,149),(348,149)],P[13])
 c.rect(346,140,411,149,P[3]);c.rect(347,149,410,150,P[5])
 for x in range(382,408,12):c.rect(x,114,x+1,147,P[3])
 c.rect(340,104,417,110,P[5]);c.rect(341,105,416,108,P[4]);c.rect(341,105,416,105,P[3])
 recycle_sign(c,363,127,9,P[0])
 for x in (356,400):ell(c,x,151,4,4,[P[7],P[6],P[5],P[5]]);c.set(x,151,P[7])
 # right, in the yard: a hand cart with a blue barrel
 line(c,381,178,377,226,P[5],1.2);line(c,381,178,386,174,P[5],1.2);c.rect(344,225,378,227,P[5]);c.rect(345,225,377,225,P[6])
 barrel(c,349,196,24,29)
 ell(c,374,229,5,5,[P[7],P[6],P[5],P[5]]);c.set(374,229,P[7])
def ground(x0=0,x1=512,wide=False):
 c=Stage(x0,x1,240)
 # Back garden and workshop fence.
 for y in range(240):c.rect(x0,y,x1-1,y,P[0] if y<44 else P[1] if y<83 else P[2] if y<147 else P[8])
 for x in range(x0//20*20,x1,20):
  c.rect(x,53,x+15,133,P[9]);c.rect(x,53,x+2,133,P[8]);c.rect(x+15,53,x+16,133,P[10]);c.rect(x+6,70,x+6,95,P[10])
 c.rect(x0,64,x1-1,68,P[10]);c.rect(x0,65,x1-1,65,P[8])
 for x in ([-70] if wide else [])+[10,46,290,330,402,480]:
  c.rect(x+2,38,x+6,150,P[10]);ell(c,x,41,24,31,[P[1],P[2],P[3],P[4]])
 c.rect(x0,153,x1-1,154,P[10])
 rng=random.Random(3)
 for _ in range(170):
  x=rng.randrange(512);y=rng.randrange(158,238);c.rect(x,y,x+2,y,P[9])
 if wide:
  rng=random.Random(5)
  for _ in range(40):
   x=rng.randrange(WIDE_L,-3);y=rng.randrange(158,238);c.rect(x,y,x+2,y,P[9])
  wide_yard(c)
 # Longer main conveyor, a compact three-way junction and nearby bins.
 c.rect(110,122,210,179,P[5]);c.rect(112,124,208,176,P[6]);c.rect(113,124,207,128,P[7])
 c.rect(137,0,183,161,P[5]);c.rect(141,0,179,159,P[6]);c.rect(138,0,140,157,P[7]);c.rect(180,0,182,157,P[7])
 for side in [-1,1]:
  line(c,160,160,160+side*64,185,P[5],9)
  line(c,160,160,160+side*64,185,P[7],7)
  line(c,160,162,160+side*64,187,P[6],5)
 c.rect(149,157,171,190,P[5]);c.rect(151,158,169,189,P[7]);c.rect(153,158,167,189,P[6])
 # Individual workstations under machine.
 for x in [32,64,224,256]:c.rect(x,158,x+30,163,P[10]);c.rect(x,157,x+30,158,P[8])
 # Sorting bins, deep opening and 3-shade side panels.
 for x,col in [(70,P[13]),(134,P[9]),(198,P[14])]:
  c.rect(x+3,181,x+49,228,P[5]);c.rect(x+5,187,x+47,226,col);c.rect(x+5,187,x+9,225,P[7]);c.rect(x+42,187,x+47,226,P[6]);c.rect(x,181,x+52,186,P[5]);c.rect(x+2,181,x+50,182,P[7]);c.rect(x+8,228,x+16,232,P[5]);c.rect(x+37,228,x+45,232,P[5])
 return c
def ground_wide():
 """the 128-tile ground map's picture (1024 px, map x = stage x & 1023; blank where no screen can look)"""
 s=ground(WIDE_L,WIDE_R,True);c=Canvas(WIDE_W,240)
 for y in range(240):
  for x in range(WIDE_L,WIDE_R):c.p[y][x%WIDE_W]=s.get(x,y)
 return c
def hills():
 # Foreground rims occlude descending sprites once they actually enter the bins.
 c=Canvas(512,240)
 # Only the physical front lips mask a bird entering a bin; no invisible centre cover.
 for x,col in [(70,P[13]),(134,P[9]),(198,P[14])]:
  c.rect(x,192,x+52,194,P[5]);c.rect(x+2,192,x+50,192,P[7]);c.rect(x+3,195,x+49,228,P[5]);c.rect(x+5,195,x+47,226,col);c.rect(x+5,195,x+9,225,P[7]);c.rect(x+42,195,x+47,226,P[6]);c.rect(x+12,198,x+40,226,P[0])
 return c
def write_all(out,preview=False):
 os.makedirs(os.path.join(out,'tiles'),exist_ok=True);im=Image.new('RGB',(SHEET_W,SHEET_H),hs.MAGENTA)
 for name,x,y,w,h,n,g in SPRITES:
  for f in range(n):
   if name=='belt_mark':
    cv=Canvas(8,8);cv.rect(0,1,7,2,GREY[2]);cv.rect(0,0,7,0,GREY[1]);im2=cv.image()
   elif name.startswith('gate_'):im2=gate(-1 if name=='gate_left' else 1 if name=='gate_right' else 0).image()
   elif name.startswith('badge') or name in ('heart','heart_empty','lamp'):im2=badge(name).image()
   elif name=='hero_icon':im2=hero().image().resize((16,16),Image.Resampling.NEAREST)
   elif name.startswith('hero'):im2=hero(name[5:],f).image()
   else:im2=item(int(name[5:])).image()
   im.paste(im2,(x+f*w,y))
 im.save(os.path.join(out,'sprites.png'));ground().image().save(os.path.join(out,'tiles/ground.png'));hills().image().save(os.path.join(out,'tiles/hills.png'));ground_wide().image().save(os.path.join(out,'tiles/ground_wide.png'))
 return ['sprites.png','tiles/ground.png','tiles/hills.png','tiles/ground_wide.png']
if __name__=='__main__':write_all(os.path.join(GAME,'art'))

