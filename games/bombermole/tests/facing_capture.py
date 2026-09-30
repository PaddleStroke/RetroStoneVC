#!/usr/bin/env python3
"""In-game facing test: captures the walk cycles (--opt spritetest=1) with the headless runner and
checks, per frame, that every left frame faces the same way as the first one and that every right
frame is the exact mirror of its left frame (right strips are mirrored in the build).

    facing_capture.py <headless binary> [out.png]

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/bombermole/LICENSE.
"""
import os
import subprocess
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import art_consistency as ac  # noqa: E402

ROWS = [("ferret", 2), ("cat", 2), ("mole", 3), ("dog", 2)]
BG = (0x20, 0x30, 0x40)


def crop(img, r, col):
    x0, y0 = 8 + col * 48, 8 + r * 48
    return np.asarray(img.crop((x0, y0, x0 + 40, y0 + 40)).convert("RGB")).astype(int)


def shape(a):
    mask = np.any(np.abs(a - np.array([(v >> 3) * 255 // 31 for v in BG])) > 12, -1)
    ys, xs = np.nonzero(mask)
    if not len(xs):
        return None, mask
    c = a[ys.min():ys.max() + 1, xs.min():xs.max() + 1].astype(np.float32)
    m = mask[ys.min():ys.max() + 1, xs.min():xs.max() + 1].astype(np.float32)
    lum = (0.299 * c[..., 0] + 0.587 * c[..., 1] + 0.114 * c[..., 2]) * m
    mm = np.asarray(Image.fromarray((m * 255).astype(np.uint8)).resize((16, 12), Image.BILINEAR), np.float32) / 255
    ll = np.asarray(Image.fromarray(lum).resize((16, 12), Image.BILINEAR), np.float32)
    return (mm, ll), mask


def main():
    exe = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "build", "facing_capture.png")
    subprocess.check_call([exe, "--frames", "3", "--opt", "spritetest=1", "--png", out],
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    img = Image.open(out)
    fails = 0
    for r, (name, n) in enumerate(ROWS):
        lefts = [crop(img, r, f) for f in range(n)]
        rights = [crop(img, r, n + f) for f in range(n)]
        ref, _ = shape(lefts[0])
        for f in range(n):
            s, mask = shape(lefts[f])
            flip, _ = shape(lefts[f][:, ::-1])
            same, other = ac._similar(s, ref), ac._similar(flip, ref)
            ok = f == 0 or same > other
            print(("  ok   " if ok else "  FAIL ") + "%s walk left frame %d faces left (%.2f vs mirrored %.2f)" %
                  (name, f + 1, same, other))
            fails += not ok
            mirrored = np.array_equal(rights[f], lefts[f][:, ::-1])
            print(("  ok   " if mirrored else "  FAIL ") + "%s walk right frame %d = mirrored left frame %d" %
                  (name, f + 1, f + 1))
            fails += not mirrored
    print("facing capture: %s (%s)" % ("all passed" if not fails else "%d FAILED" % fails, out))
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
