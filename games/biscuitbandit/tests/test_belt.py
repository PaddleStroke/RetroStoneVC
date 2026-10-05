from pathlib import Path
import subprocess
from PIL import Image
root=Path(__file__).resolve().parents[3]
h=root/'build/host/biscuitbandit_headless';o=root/'build/belt-review';o.mkdir(exist_ok=True)
script=o/'motion.input';script.write_text('30 tap A\n90 tap SELECT\n')
subprocess.run([str(h),'--frames','301','--input',str(script),'--opt','music=0','--opt','strict=1','--shot',f'40:{o}/moving-a.png','--shot',f'43:{o}/moving-b.png','--shot',f'120:{o}/paused-a.png','--shot',f'300:{o}/paused-b.png'],check=True,cwd=root)
def belt(name):return Image.open(o/name).crop((0,45,320,51)).tobytes()
assert belt('moving-a.png')!=belt('moving-b.png'),'Conveyor is visually static during play'
assert belt('paused-a.png')==belt('paused-b.png'),'Conveyor moves during pause'
print('Rendered conveyor moves during play and freezes during pause: passed')

