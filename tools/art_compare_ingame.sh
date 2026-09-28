#!/bin/sh
# In-game comparison of the character sizes (16, 24, 32 px) with the owner's
# generated AI art: real builds, headless screenshots at 1x, one row per size.
#   sh tools/art_compare_ingame.sh [out dir]
set -e
OUT=${1:-docs/art-preview}
mkdir -p "$OUT" build/compare
for n in 16 24 32; do
    python3 tools/art_sync.py sync --include-generated --char-size $n --out build/art-$n \
        --report build/art-$n/REPORT.md > /dev/null
    rm -f build/gen/bombermole/assets.c
    make -s build/host/bombermole_headless ART=../../build/art-$n CHAR_SIZE=$n > /dev/null
    ./build/host/bombermole_headless --frames 110 --opt level=spring-3 --opt nointro=1 \
        --shot 100:build/compare/s3_$n.png > /dev/null
    ./build/host/bombermole_headless --frames 110 --opt level=spring-8 --opt nointro=1 \
        --shot 100:build/compare/s8_$n.png > /dev/null
    ./build/host/bombermole_headless --frames 110 --opt level=spring-2 --opt nointro=1 --opt view=1 \
        --shot 100:build/compare/s2u_$n.png > /dev/null
done
python3 - "$OUT" <<'PY'
import sys
from PIL import Image, ImageDraw
out = sys.argv[1]
rows = []
for n in (16, 24, 32):
    shots = [Image.open("build/compare/%s_%d.png" % (s, n)) for s in ("s3", "s8", "s2u")]
    row = Image.new("RGB", (3 * 320 + 4 * 6, 240 + 18), (24, 24, 32))
    ImageDraw.Draw(row).text((6, 3), "characters %d px (boss %d px), in game at 1x: spring 3, spring 8 (boss), spring 2 underground" % (n, 2 * n), fill=(255, 255, 255))
    for i, s in enumerate(shots):
        row.paste(s, (6 + i * 326, 18))
    rows.append(row)
im = Image.new("RGB", (rows[0].width, sum(r.height for r in rows)), (24, 24, 32))
y = 0
for r in rows:
    im.paste(r, (0, y))
    y += r.height
im.save(out + "/character_size_ingame_1x.png")
im.resize((im.width * 2, im.height * 2), Image.NEAREST).save(out + "/character_size_ingame_2x.png")
print("wrote", out + "/character_size_ingame_1x.png and _2x.png")
PY
# back to the normal build (validated art only, 16 px)
rm -f build/gen/bombermole/assets.c
make -s host > /dev/null
