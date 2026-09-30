#!/usr/bin/env python3
"""docs/screenshots/hidden-grub-glow.png: the glow of blocks hiding a golden grub, at its dimmest and
brightest (the pulse lasts ~1 s), 2x, side by side.

    glow_shot.py <headless binary> <out.png> [level]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/bombermole/LICENSE.
"""
import os
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image, ImageDraw


def main():
    exe, out = sys.argv[1], sys.argv[2]
    level = sys.argv[3] if len(sys.argv) > 3 else "spring-3"
    tmp = tempfile.mkdtemp()
    frames = [150 + i * 2 for i in range(32)]            # one whole pulse (64 frames)
    args = [exe, "--scale", "2", "--frames", str(frames[-1] + 1), "--opt", "level=" + level, "--opt", "nointro=1"]
    for f in frames:
        args += ["--shot", "%d:%s" % (f, os.path.join(tmp, "%d.png" % f))]
    subprocess.check_call(args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    shots = [np.asarray(Image.open(os.path.join(tmp, "%d.png" % f)).convert("RGB")).astype(int) for f in frames]
    # the pulse phase of each shot: which colour of the glow ramp (ui_glow_pulse) is on screen
    def glow(k):
        c5 = (14 + k * 17 // 31, 9 + k * 16 // 31, 1 + k * 6 // 31)
        return np.array([(v << 3) | (v >> 2) for v in c5])
    phases = []
    for s in shots:
        counts = [int((np.abs(s - glow(k)).max(-1) <= 4).sum()) for k in range(32)]
        phases.append(int(np.argmax(counts)) if max(counts) else -1)
    if max(phases) < 0:
        sys.exit("glow_shot: no glow found in %s" % level)
    valid = [i for i, p in enumerate(phases) if p >= 0]
    lo = min(valid, key=lambda i: phases[i])
    hi = max(valid, key=lambda i: phases[i])
    w, h = shots[0].shape[1], shots[0].shape[0]
    img = Image.new("RGB", (2 * w + 8, h + 20), (24, 24, 32))
    d = ImageDraw.Draw(img)
    for k, (i, name) in enumerate(((lo, "dimmest"), (hi, "brightest"))):
        img.paste(Image.fromarray(shots[i].astype(np.uint8)), (k * (w + 8), 20))
        d.text((k * (w + 8) + 4, 4), "hidden-grub glow, %s (frame %d; the pulse lasts ~1 s)" % (name, frames[i]),
               fill=(255, 220, 120))
    img.save(out)
    print("wrote", out, "(pulse phases %d and %d of 31)" % (phases[lo], phases[hi]))


if __name__ == "__main__":
    main()
