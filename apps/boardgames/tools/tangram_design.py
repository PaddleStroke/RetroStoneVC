"""
Tangram's designer: lays the seven tans over rough sketches (tangram_sketches.py) with tangram_design.c, a beam
search, and draws the results over their sketches, to pick figures from.

    python3 apps/boardgames/tools/tangram_design.py out.png [names...]    # all sketches, or the ones named

It builds tangram_design.c (gcc) into the system's temporary folder when needed. It writes out.png (each result in
black over its sketch in grey, its name and the checks' complaints in red) and out.png.txt, a line a result:
"name.i" then seven times "kind eighths flip ax bx ay by". Add the one you like to tangram_figures.py with
from_design(line, chapter, name). Needs Pillow.
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import math
import os
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from tangram_kit import Fig, N, Fr, check, fpts, inside  # noqa: E402
from tangram_sketches import SK  # noqa: E402

SRC = os.path.join(HERE, "tangram_design.c")
EXE = os.path.join(tempfile.gettempdir(), "tangram_design")


def build():
    if not os.path.exists(EXE) or os.path.getmtime(EXE) < os.path.getmtime(SRC):
        subprocess.run(["gcc", "-O2", "-Wall", "-o", EXE, SRC, "-lm"], check=True)


def run(sketch):
    p = subprocess.run([EXE], input=sketch, capture_output=True, text=True)
    res, res_sk = [], None
    for line in p.stdout.strip().split("\n"):
        if not line.strip():
            continue
        if line.startswith("#sk"):
            res_sk = [float(v) for v in line.split()[1:]]
            continue
        t = line.split()
        sc, rest = float(t[0]), t[1:]
        pcs = []
        for i in range(0, len(rest), 7):
            k, rot, flip, ax, bx, ay, by = rest[i:i + 7]
            pcs.append((k, int(rot), int(flip), N(Fr(int(ax), 4), Fr(int(bx), 4)), N(Fr(int(ay), 4), Fr(int(by), 4))))
        res.append((sc, pcs))
    return res, p.stderr, res_sk


def build_fig(name, pcs, chapter=0):
    f = Fig(name, chapter)
    for k, rot, flip, x, y in pcs:
        f.vat(k, rot, 0, (x, y), flip)
    return f


def parse_sketch(text):
    shapes, hole = [], False
    for line in text.split("\n"):
        t = line.split()
        if not t:
            continue
        if t[0] == "hole":
            hole = True
            continue
        if t[0] in ("poly", "seg", "disc"):
            shapes.append((t[0], [float(v) for v in t[1:]], hole))
            hole = False
    return shapes


def in_sketch(shapes, x, y):
    inn = False
    for kind, v, hole in shapes:
        if kind == "poly":
            hit = inside([(v[i], v[i + 1]) for i in range(0, len(v), 2)], x, y)
        elif kind == "seg":
            x0, y0, x1, y1, w = v
            dx, dy = x1 - x0, y1 - y0
            t = max(0, min(1, ((x - x0) * dx + (y - y0) * dy) / (dx * dx + dy * dy)))
            hit = math.hypot(x - x0 - t * dx, y - y0 - t * dy) <= w / 2
        else:
            hit = math.hypot(x - v[0], y - v[1]) <= v[2]
        if hit:
            inn = not hole
    return inn


def design_sheet(cells, path, scale=15, cols=8):
    """cells: (name, fig, shapes, sk) with the figure in the designer's canvas units"""
    from PIL import Image, ImageDraw
    cw = 12 * scale
    chh = cw + 14
    rows = (len(cells) + cols - 1) // cols
    img = Image.new("RGB", (cols * cw, rows * chh), (238, 228, 205))
    d = ImageDraw.Draw(img)
    for idx, (name, f, shapes, sk) in enumerate(cells):
        cx, cy = idx % cols * cw, idx // cols * chh
        allp = [p for _, pts in f.pieces for p in fpts(pts)]
        mx = (min(p[0] for p in allp) + max(p[0] for p in allp)) / 2
        my = (min(p[1] for p in allp) + max(p[1] for p in allp)) / 2
        X0, Y0 = mx - 6, my - 6
        if sk:
            ox, oy, sc, bx0, by0 = sk
            for py in range(0, cw, 2):
                for px in range(0, cw, 2):
                    X, Y = X0 + px / scale, Y0 + py / scale
                    if in_sketch(shapes, (X - ox) / sc + bx0, (Y - oy) / sc + by0):
                        d.rectangle([cx + px, cy + py, cx + px + 1, cy + py + 1], fill=(205, 190, 165))
        errs = check(f)
        for k, pts in f.pieces:
            poly = [(cx + (x - X0) * scale, cy + (y - Y0) * scale) for x, y in fpts(pts)]
            d.polygon(poly, fill=(28, 26, 30))
        d.rectangle([cx, cy, cx + cw - 1, cy + chh - 1], outline=(200, 185, 160))
        label = name + (" !" + " ".join(errs) if errs else "")
        d.text((cx + 3, cy + cw), label[:40], fill=(160, 0, 0) if errs else (40, 30, 20))
    img.save(path)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    build()
    out = sys.argv[1]
    names = sys.argv[2:] or list(SK.keys())
    with ThreadPoolExecutor(8) as ex:
        outs = list(ex.map(lambda n: run(SK[n]), names))
    cells = []
    for n, (res, err, sk) in zip(names, outs):
        shapes = parse_sketch(SK[n])
        for i, (sc, pcs) in enumerate(res):
            if len(pcs) == 7:
                cells.append(("%s.%d" % (n, i), build_fig("%s.%d" % (n, i), pcs), shapes, sk))
        if not res:
            print(n, "FAILED", err[-300:])
    design_sheet(cells, out)
    with open(out + ".txt", "w") as fo:
        for n, (res, err, sk) in zip(names, outs):
            for i, (sc, pcs) in enumerate(res):
                fo.write("%s.%d %s\n" % (n, i, " ".join("%s %d %d %s %s %s %s" % (k, r_, fl, x.p * 4, x.q * 4, y.p * 4,
                                                                                    y.q * 4) for k, r_, fl, x, y in pcs)))
    print(len(cells), "designs")
