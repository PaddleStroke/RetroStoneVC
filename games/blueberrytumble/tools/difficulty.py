#!/usr/bin/env python3
"""Blueberry Tumble: the difficulty report (make blueberrytumble-difficulty).

  difficulty.py CURVES.csv HEADLESS OUT.png [--seeds N]

1. The chart: the measured difficulty of the generated pattern instances over distance for a few seeds (the CSV
   written by tests/validate.c --difficulty --csv), the director's target (the ramp and its waves), a rolling
   mean over 300 m (about 30 s), the biome gates; drawn with PIL into OUT.png (docs/difficulty.png).
2. The sanity check: the screen-reading bot plays N seeds (--opt bot=1 --opt evlog=1); every pattern instance it
   meets and every splat are logged; the failure rate per pattern (splats / instances met) is printed next to the
   pattern's mean measured score, with their rank correlation (Spearman).
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/blueberrytumble/LICENSE.
"""
import argparse
import collections
import csv
import re
import subprocess

from PIL import Image, ImageDraw, ImageFont

W, H = 960, 420
ML, MR, MT, MB = 56, 16, 30, 40
BIOME_M = 384
COLORS = [(88, 104, 226), (206, 96, 186), (84, 170, 110), (232, 140, 40), (120, 120, 140)]


def load(path):
    rows = collections.defaultdict(list)
    with open(path) as f:
        for r in csv.DictReader(f):
            rows[int(r["seed"])].append(dict(col=int(r["col"]), len=int(r["len"]), score=int(r["score"]),
                                             target=int(r["target"]), pat=r["pattern"]))
    return rows


def chart(rows, out):
    im = Image.new("RGB", (W, H), (252, 250, 244))
    d = ImageDraw.Draw(im)
    font = ImageFont.load_default()
    maxm = max(r["col"] + r["len"] for s in rows.values() for r in s)
    X = lambda m: ML + (W - ML - MR) * m / float(maxm)
    Y = lambda v: H - MB - (H - MT - MB) * v / 100.0
    for v in range(0, 101, 20):
        d.line([(ML, Y(v)), (W - MR, Y(v))], fill=(226, 222, 212))
        d.text((8, Y(v) - 6), "%3d" % v, fill=(90, 90, 90), font=font)
    names = ["summit", "forest", "meadows", "village", "night"]
    for b in range(0, maxm // BIOME_M + 1):
        x = X(b * BIOME_M)
        d.line([(x, MT), (x, H - MB)], fill=(200, 196, 186))
        d.text((x + 4, MT - 16), names[b % 4] + ("" if b < 4 else " (night)"), fill=(110, 110, 110), font=font)
    for m in range(0, maxm + 1, 500):
        d.text((X(m) - 10, H - MB + 6), "%d m" % m, fill=(90, 90, 90), font=font)
    # the target (the director's, with the waves), from the first seed
    first = sorted(rows)[0]
    pts = [(X(r["col"]), Y(r["target"])) for r in rows[first] if r["pat"] not in ("gate", "start", "bridge")]
    for a, b in zip(pts, pts[1:]):
        d.line([a, b], fill=(40, 40, 40), width=1)
    for i, s in enumerate(sorted(rows)):
        col = COLORS[i % len(COLORS)]
        segs = [r for r in rows[s] if r["score"] >= 0 and r["pat"] not in ("gate", "start", "bridge")]
        for r in segs:
            d.line([(X(r["col"]), Y(r["score"])), (X(r["col"] + r["len"]), Y(r["score"]))], fill=col, width=2)
        # the rolling mean over 300 m
        roll = []
        for m in range(150, maxm - 150, 25):
            inside = [r["score"] for r in segs if m - 150 <= r["col"] < m + 150]
            if inside:
                roll.append((X(m), Y(sum(inside) / float(len(inside)))))
        for a, b in zip(roll, roll[1:]):
            d.line([a, b], fill=tuple(max(0, c - 60) for c in col), width=3)
    d.text((ML, 8), "Blueberry Tumble: measured difficulty of the generated patterns (thin: instances, thick: 300-m "
           "rolling mean, black: the director's target with its waves), %d seeds" % len(rows), fill=(30, 30, 30), font=font)
    im.save(out)
    print("wrote %s" % out)


def spearman(xs, ys):
    def ranks(v):
        order = sorted(range(len(v)), key=lambda i: v[i])
        r = [0.0] * len(v)
        i = 0
        while i < len(order):
            j = i
            while j + 1 < len(order) and v[order[j + 1]] == v[order[i]]:
                j += 1
            for k in range(i, j + 1):
                r[order[k]] = (i + j) / 2.0
            i = j + 1
        return r
    rx, ry = ranks(xs), ranks(ys)
    n = len(xs)
    if n < 2:
        return 0.0
    mx, my = sum(rx) / n, sum(ry) / n
    cov = sum((a - mx) * (b - my) for a, b in zip(rx, ry))
    vx = sum((a - mx) ** 2 for a in rx) ** 0.5
    vy = sum((b - my) ** 2 for b in ry) ** 0.5
    return cov / (vx * vy) if vx and vy else 0.0


def bot_rates(headless, seeds):
    met, died, score = collections.Counter(), collections.Counter(), collections.defaultdict(list)
    dist = []
    for s in range(1, seeds + 1):
        out = subprocess.run([headless, "--frames", "60000", "--opt", "music=0", "--opt", "sound=0", "--opt", "bot=1",
                              "--opt", "ready=1", "--opt", "evlog=1", "--opt", "seed=%d" % (1000 + s)],
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True).stdout
        for line in out.splitlines():
            m = re.search(r"seg (\S+) score (\d+)", line)
            if m and m.group(1) not in ("gate", "start", "bridge"):
                met[m.group(1)] += 1
                score[m.group(1)].append(int(m.group(2)))
            m = re.search(r"splat at (\d+) m .* in (\S+)\)", line)
            if m:
                died[m.group(2)] += 1
                dist.append(int(m.group(1)))
    return met, died, score, dist


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv")
    ap.add_argument("headless")
    ap.add_argument("out")
    ap.add_argument("--seeds", type=int, default=20)
    a = ap.parse_args()
    chart(load(a.csv), a.out)
    met, died, score, dist = bot_rates(a.headless, a.seeds)
    print("\nthe screen bot over %d seeds: distances %s (mean %.0f m)" % (a.seeds, sorted(dist), sum(dist) / max(1, len(dist))))
    print("   pattern       met  splats  failure   mean score (of the instances met)")
    xs, ys = [], []
    for p in sorted(met, key=lambda p: -sum(score[p]) / len(score[p])):
        rate = died[p] / float(met[p])
        ms = sum(score[p]) / float(len(score[p]))
        xs.append(ms)
        ys.append(rate)
        print("   %-11s %5d  %6d   %5.1f%%   %5.1f" % (p, met[p], died[p], 100 * rate, ms))
    others = sum(v for k, v in died.items() if k not in met)
    if others:
        print("   (%d splats in bridges or gates)" % others)
    print("rank correlation (Spearman) between the measured score and the bot's failure rate: %.2f" % spearman(xs, ys))


if __name__ == "__main__":
    main()
