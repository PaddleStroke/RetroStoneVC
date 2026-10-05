"""32-pixel hedgehog designs. C (index 2) selected by the owner.
Code MIT, (c) 2026 Pierre-Louis Boyer (8BCraft); drawn assets follow the game LICENSE.
"""
import math
from PIL import Image, ImageDraw
import house_style as hs
INK=(46,28,42)
FUR=[(255,226,180),(231,189,131),(184,131,80),(126,77,51)]
BACKS=[[(225,187,124),(178,131,78),(125,81,50),(75,46,36)],
       [(241,195,130),(196,137,80),(143,85,51),(87,48,40)],
       [(228,191,143),(183,134,88),(127,82,56),(77,48,42)],
       [(197,180,153),(148,125,99),(101,79,65),(62,47,45)]]
LABELS=['A  WOODLAND','B  ROUND RASCAL','C  SPIKY SPRINTER','D  SCRUFFY']
NOTES=['Long snout, low body','Round belly, big eye','Bold quills, lean pose','Dark coat, pale tips']
def poly(cv,points,ramp):
 mask=Image.new('1',(cv.w,cv.h));ImageDraw.Draw(mask).polygon(points,fill=1)
 ys=[p[1] for p in points];top=min(ys);bottom=max(ys)
 for y in range(cv.h):
  for x in range(cv.w):
   if mask.getpixel((x,y)):
    shade=0 if y<top+3 else 1 if y<top+(bottom-top)*.48 else 2 if y<bottom-2 else 3
    cv.set(x,y,ramp[shade])
def candidate(v,pose='idle',frame=0):
 cv=hs.Canvas(32,32);back=BACKS[v]
 if pose=='roll':
  hs.ellipse(cv,15,18,10,10,back)
  # Broad triangular quills survive both 1x display and palette quantisation.
  for k in range(10):
   a=k*math.pi/5+frame*math.pi/6
   points=[(round(15+math.cos(a-.17)*8),round(18+math.sin(a-.17)*8)),
           (round(15+math.cos(a)*12),round(18+math.sin(a)*12)),
           (round(15+math.cos(a+.17)*8),round(18+math.sin(a+.17)*8))]
   poly(cv,points,back)
  a=frame*math.pi/2
  cx=round(15+math.cos(a)*5);cy=round(18+math.sin(a)*5)
  hs.ellipse(cv,cx,cy,4,3,FUR)
  cv.set(cx+2,cy,INK)
 else:
  # Feet first, behind the body, with a tiny alternating gait.
  for x in (8,21):
   hs.ellipse(cv,x+(frame%2),28.5,3,1.5,back)
  if v==0:
   points=[(3,24),(2,20),(3,16),(2,12),(6,14),(6,9),(9,12),(10,6),(12,11),(15,7),(17,12),(20,10),(22,17),(22,25),(17,27),(8,27)]
  elif v==1:
   points=[(3,24),(2,18),(3,13),(6,14),(7,7),(10,11),(13,5),(15,10),(18,7),(20,13),(23,19),(22,25),(18,28),(8,28)]
  elif v==2:
   points=[(2,23),(3,18),(1,13),(6,15),(4,8),(9,11),(9,4),(13,10),(16,5),(18,11),(21,8),(23,17),(22,24),(18,26),(8,26)]
  else:
   points=[(3,24),(2,19),(3,14),(5,15),(5,9),(8,12),(10,6),(12,11),(15,7),(17,12),(20,10),(22,17),(22,25),(17,28),(8,28)]
  poly(cv,points,back)
  # Pale quill strokes separated by dark fur: these are solid shaded forms, not dither.
  for x,y in [(5,17),(8,14),(11,12),(14,14),(6,21),(10,19),(14,18),(17,16),(12,23)]:
   cv.set(x,y,back[0]);cv.set(x+1,y+1,back[1]);cv.set(x+1,y+2,back[3])
  head=[(17,15),(22,16),(26,19),(29,22),(29,24),(25,26),(20,25),(17,22)]
  if v==1: head=[(16,13),(22,14),(26,18),(29,22),(29,24),(25,26),(19,26),(16,22)]
  if v==2: head=[(18,15),(23,16),(27,19),(30,22),(29,24),(25,25),(20,24),(17,21)]
  poly(cv,head,FUR)
  hs.ellipse(cv,18 if v!=1 else 17,14 if v!=1 else 12,2.3,3,FUR)
  cv.set(18 if v!=1 else 17,13 if v!=1 else 11,FUR[3])
  ex,ey=(21,16) if v!=1 else (20,15)
  if pose=='hit':
   for n in range(3):cv.set(ex+n,ey+n,INK);cv.set(ex+2-n,ey+n,INK)
  else:hs.eye(cv,ex,ey,look=(1,0),big=True)
  hs.ellipse(cv,29,22.5,1.5,1.5,[INK]*4)
  cv.set(28,22,FUR[0]);cv.set(26,24,FUR[3])
  if v==1:cv.set(24,23,FUR[0])
 if pose=='squash':cv=hs.squash(cv,1.18)
 if pose=='stretch':cv=hs.squash(cv,.88)
 cv.outline(INK)
 return cv
