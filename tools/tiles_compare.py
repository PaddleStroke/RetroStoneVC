#!/usr/bin/env python3
"""Side-by-side comparison of two terrain tilesets in the game (make preview):

    tiles_compare.py <headless A> <label A> <headless B> <label B> <out.png>

The same three scenes (spring surface, underground, winter) are captured with both headless
runners; each row shows A and B at 1x, then the middle of the screen of A and B at 2x.
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import os
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw

SCENES = [("spring surface (spring-3)", ["--opt", "level=spring-3"]),
          ("underground (spring-6, depth 1)", ["--opt", "level=spring-6", "--opt", "view=1"]),
          ("winter (winter-1)", ["--opt", "level=winter-1"])]


def shot(exe, args, path):
    subprocess.check_call([exe, "--frames", "200", "--opt", "nointro=1"] + args + ["--shot", "150:" + path],
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return Image.open(path).convert("RGB")


def main():
    ea, la, eb, lb, out = sys.argv[1:6]
    tmp = tempfile.mkdtemp()
    W, H, G, T = 256, 224, 8, 14
    CW, CH = 128, 112                           # 2x crop: the middle of the playfield
    width = 2 * W + 4 * CW + 4 * G
    img = Image.new("RGB", (width, len(SCENES) * (H + T + G) + T), (24, 24, 32))
    d = ImageDraw.Draw(img)
    heads = [(0, la + " 1x"), (W + G, lb + " 1x"), (2 * W + 2 * G, la + " 2x"), (2 * W + 3 * G + 2 * CW, lb + " 2x")]
    for x, t in heads:
        d.text((x + 2, 1), t, fill=(255, 255, 255))
    for i, (name, args) in enumerate(SCENES):
        y = T + i * (H + T + G)
        d.text((2, y), name, fill=(255, 220, 120))
        y += T
        shots = [shot(exe, args, os.path.join(tmp, "%d_%d.png" % (i, k))) for k, exe in enumerate((ea, eb))]
        for k, s in enumerate(shots):
            img.paste(s, (k * (W + G), y))
            box = ((W - CW) // 2, 16 + (H - 16 - CH) // 2)
            crop = s.crop((box[0], box[1], box[0] + CW, box[1] + CH)).resize((2 * CW, 2 * CH), Image.NEAREST)
            img.paste(crop, (2 * W + 2 * G + k * (2 * CW + G), y))
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    img.save(out)
    print("wrote", out)


if __name__ == "__main__":
    main()
