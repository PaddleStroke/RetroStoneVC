#!/usr/bin/env python3
"""Nutty Branch: measure the bark's furrows on screen lines near the squirrel (tests/furrow_test.sh).

The branch is a cylinder seen from above: a furrow at angle t round it sits at x = xc + hw sin t on a screen line, so
furrows evenly spaced in ANGLE must bunch up toward the silhouette (their screen spacing ~ cos t) and thin out
(their screen width ~ cos t too). For each screenshot line (with its branch centre and scale from --opt linedump),
the furrows are found as runs of pixels clearly darker than the bark around them, then:
  - their spacing in angle is about constant (the texture's 9 degrees),
  - their spacing on screen shrinks from the top to the sides, following cos t,
  - their width on screen shrinks from the top to the sides.
  furrow_measure.py <shot.png> <line> <centre> <scale> [<shot.png> <line> <centre> <scale> ...]
Prints the furrows of the first line and the statistics; exits 1 when a check fails.
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
"""
import math
import statistics as st
import sys

from PIL import Image

BRANCH_R = 64
FURROW_DEG = 9.0
MAX_DEG = 66.0


def furrows(path, y, xc, a):
    im = Image.open(path).convert("RGB")
    xc += (im.width - 320) // 2          # widescreen: the centre is a stage x, the 320-px stage centred on the screen
    hw = BRANCH_R * 256.0 / a
    lum = [0.3 * r + 0.59 * g + 0.11 * b for r, g, b in (im.getpixel((x, y)) for x in range(im.width))]
    x0, x1 = int(xc - hw * 0.97), int(xc + hw * 0.97)
    dark = {}
    for x in range(max(1, x0), min(im.width - 1, x1)):
        t = math.asin(max(-1.0, min(1.0, (x + 0.5 - xc) / hw)))
        win = max(3, int(hw * math.radians(FURROW_DEG) * math.cos(t) * 0.7))
        around = sorted(lum[max(0, x - win):x + win + 1])
        ref = around[int(len(around) * 0.75)]
        dark[x] = lum[x] < 0.8 * ref
    runs, cur = [], []
    for x in range(max(1, x0), min(im.width - 1, x1)):
        if dark.get(x):
            cur.append(x)
        elif cur:
            runs.append(cur)
            cur = []
    if cur:
        runs.append(cur)
    out = []
    for r in runs:
        c = (r[0] + r[-1] + 1) / 2.0
        t = math.degrees(math.asin(max(-1.0, min(1.0, (c - xc) / hw))))
        expect = 2 * math.radians(1.75) * hw * math.cos(math.radians(t))     # a furrow's width there (px)
        if abs(t) < MAX_DEG and (len(r) > 1 or expect < 2.5):               # (not a speck)
            out.append((c, t, len(r)))
    return out, hw


def main(argv):
    groups = [argv[i:i + 4] for i in range(0, len(argv), 4)]
    pairs, widths = [], []
    first = None
    for g in groups:
        path, y, xc, a = g[0], int(g[1]), int(g[2]), int(g[3])
        fs, hw = furrows(path, y, xc, a)
        if first is None:
            first = (path, y, xc, hw, fs)
        for (c0, t0, w0), (c1, t1, w1) in zip(fs, fs[1:]):
            if t1 - t0 < FURROW_DEG * 1.6:                # neighbours (not across a furrow missed)
                pairs.append((abs((t0 + t1) / 2.0), t1 - t0, c1 - c0))
        for c, t, w in fs:
            widths.append((abs(t), w))
    path, y, xc, hw, fs = first
    print("furrows on line %d of %s (centre %d, half width %.1f px):" % (y, path.split("/")[-1], xc, hw))
    print("      x   angle  width")
    for c, t, w in fs:
        print("  %5.1f  %+6.1f  %3d" % (c, t, w))
    bins = [(0, 22), (22, 44), (44, MAX_DEG)]
    mid = [math.radians((lo + hi) / 2.0) for lo, hi in bins]
    sp = [[p[2] for p in pairs if lo <= p[0] < hi] for lo, hi in bins]
    da = [[p[1] for p in pairs if lo <= p[0] < hi] for lo, hi in bins]
    wd = [[w for t, w in widths if lo <= t < hi] for lo, hi in bins]
    if not all(sp) or not all(wd):
        print("FAIL: too few furrows found")
        return 1
    msp = [st.median(v) for v in sp]
    mda = [st.median(v) for v in da]
    mwd = [st.mean(v) for v in wd]
    alld = st.median([p[1] for p in pairs])
    print("%d neighbouring furrows over %d lines; their spacing in angle: median %.1f deg (the texture: %.0f)" %
          (len(pairs), len(groups), alld, FURROW_DEG))
    for (lo, hi), m, d, w, n, tm in zip(bins, msp, mda, mwd, sp, mid):
        print("  |angle| %2d-%2d: %3d pairs, %4.1f deg apart, %4.1f px apart (cos law: %4.1f), %.1f px wide" %
              (lo, hi, len(n), d, m, msp[0] * math.cos(tm) / math.cos(mid[0]), w))
    ok = True
    if not 7.0 <= alld <= 11.0 or max(mda) > 1.3 * min(mda):
        print("FAIL: the furrows are not evenly spaced in angle (round the cylinder)")
        ok = False
    if not msp[0] > msp[1] > msp[2]:
        print("FAIL: their screen spacing does not shrink toward the sides")
        ok = False
    want = math.cos(mid[2]) / math.cos(mid[0])
    if abs(msp[2] / msp[0] - want) > 0.25 * want:
        print("FAIL: their screen spacing does not follow cos(angle): %.2f, not %.2f" % (msp[2] / msp[0], want))
        ok = False
    if not mwd[0] > mwd[1] > mwd[2]:
        print("FAIL: they do not thin out toward the sides")
        ok = False
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
