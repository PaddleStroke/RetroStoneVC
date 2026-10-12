"""
Tangram design kit (tangram_figures.py, tangram_design.py): exact numbers p + q r2, the seven tans turned and turned
over (the same shapes as src/games/tangram_rules.c), figures, the checks (the seven tans, no overlap, held together edge
to edge, no hole), contact sheets (Pillow), the C table, and a solver tiling a silhouette drawn in grid cells.
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
from fractions import Fraction as Fr
import math

S2 = math.sqrt(2)


class N:
    """p + q*sqrt2, p and q rational"""
    __slots__ = ("p", "q")

    def __init__(s, p=0, q=0):
        s.p = Fr(p)
        s.q = Fr(q)

    @staticmethod
    def of(v):
        return v if isinstance(v, N) else N(v, 0)

    def __add__(s, o):
        o = N.of(o)
        return N(s.p + o.p, s.q + o.q)

    __radd__ = __add__

    def __neg__(s):
        return N(-s.p, -s.q)

    def __sub__(s, o):
        return s + (-N.of(o))

    def __rsub__(s, o):
        return N.of(o) - s

    def __mul__(s, o):
        o = N.of(o)
        return N(s.p * o.p + 2 * s.q * o.q, s.p * o.q + s.q * o.p)

    __rmul__ = __mul__

    def __truediv__(s, k):
        return N(s.p / Fr(k), s.q / Fr(k))

    def __float__(s):
        return float(s.p) + float(s.q) * S2

    def __eq__(s, o):
        o = N.of(o)
        return s.p == o.p and s.q == o.q

    def __hash__(s):
        return hash((s.p, s.q))

    def __lt__(s, o):
        return float(s) < float(N.of(o)) - 1e-12

    def __repr__(s):
        return "%s%+s*r2" % (s.p, s.q)


h = N(0, Fr(1, 2))   # sqrt2 / 2
r = N(0, 1)          # sqrt2

KINDS = "LMSQP"
BASE = {
    "L": [(0, 0), (2, 0), (0, 2)],
    "M": [(0, 0), (r, 0), (0, r)],
    "S": [(0, 0), (1, 0), (0, 1)],
    "Q": [(0, 0), (1, 0), (1, 1), (0, 1)],
    "P": [(0, 0), (1, 0), (2, 1), (1, 1)],
}
AREA = {"L": 2, "M": 1, "S": Fr(1, 2), "Q": 1, "P": 1}
COUNT = {"L": 2, "M": 1, "S": 2, "Q": 1, "P": 1}


def rot45(pt):
    x, y = N.of(pt[0]), N.of(pt[1])
    return ((x - y) * h, (x + y) * h)


def variant(kind, rot, flip=0):
    pts = [(N.of(x), N.of(y)) for x, y in BASE[kind]]
    if flip:
        pts = [(-x, y) for x, y in pts]
    for _ in range(rot % 8):
        pts = [rot45(p) for p in pts]
    return pts


def bmin(pts):
    mx = min((p[0] for p in pts), key=float)
    my = min((p[1] for p in pts), key=float)
    return mx, my


def bmax(pts):
    return max((p[0] for p in pts), key=float), max((p[1] for p in pts), key=float)


def norm_set(pts):
    mx, my = bmin(pts)
    return sorted([(p[0] - mx, p[1] - my) for p in pts], key=lambda t: (float(t[0]), float(t[1])))


def identify(kind, pts):
    """the (rot, flip) giving this point set; flip only for P"""
    target = norm_set(pts)
    for flip in (0, 1):
        if flip and kind != "P":
            break
        for rot in range(8):
            if norm_set(variant(kind, rot, flip)) == target:
                return rot, flip
    raise ValueError("not a %s: %s" % (kind, pts))


class Fig:
    def __init__(s, name, chapter):
        s.name = name
        s.chapter = chapter
        s.pieces = []   # [kind, verts]

    def at(s, kind, rot, x, y, flip=0, corner="tl"):
        """a piece with its bounding box's corner (tl, tr, bl, br, tc, bc, lc, rc, c) at (x, y)"""
        pts = variant(kind, rot, flip)
        (x0, y0), (x1, y1) = bmin(pts), bmax(pts)
        ax = {"l": x0, "r": x1, "c": (x0 + x1) / 2}
        ay = {"t": y0, "b": y1, "c": (y0 + y1) / 2}
        if corner == "c":
            cx, cy = ax["c"], ay["c"]
        elif corner in ("tc", "bc"):
            cx, cy = ax["c"], ay[corner[0]]
        elif corner in ("lc", "rc"):
            cx, cy = ax[corner[0]], ay["c"]
        else:
            cx, cy = ax[corner[1]], ay[corner[0]]
        dx, dy = N.of(x) - cx, N.of(y) - cy
        s.pieces.append([kind, [(px + dx, py + dy) for px, py in pts]])
        return len(s.pieces) - 1

    def vat(s, kind, rot, i, pt, flip=0):
        """a piece with its vertex i at pt"""
        pts = variant(kind, rot, flip)
        dx, dy = N.of(pt[0]) - pts[i][0], N.of(pt[1]) - pts[i][1]
        s.pieces.append([kind, [(px + dx, py + dy) for px, py in pts]])
        return len(s.pieces) - 1

    def poly(s, kind, pts):
        pts = [(N.of(x), N.of(y)) for x, y in pts]
        identify(kind, pts)
        s.pieces.append([kind, pts])
        return len(s.pieces) - 1

    def V(s, k, i):
        return s.pieces[k][1][i]

    def transform(s, rot=0, mirror=0):
        for pc in s.pieces:
            pts = pc[1]
            if mirror:
                pts = [(-x, y) for x, y in pts]
            for _ in range(rot % 8):
                pts = [rot45(p) for p in pts]
            pc[1] = pts
        return s

    # ---- export ----
    def normalized(s):
        allp = [p for pc in s.pieces for p in pc[1]]
        mx, my = bmin(allp)
        return [[k, [(x - mx, y - my) for x, y in pts]] for k, pts in s.pieces]

    def size(s):
        allp = [p for pc in s.normalized() for p in pc[1]]
        X, Y = bmax(allp)
        return float(X), float(Y)


# ---- geometry checks (floats) ----
def fpts(pts):
    return [(float(x), float(y)) for x, y in pts]


def area(poly):
    a = 0
    for i in range(len(poly)):
        x0, y0 = poly[i]
        x1, y1 = poly[(i + 1) % len(poly)]
        a += x0 * y1 - x1 * y0
    return a / 2


def clip(subject, clipper):
    """convex clipping (Sutherland-Hodgman); both convex, any winding"""
    if area(clipper) < 0:
        clipper = clipper[::-1]
    out = subject
    for i in range(len(clipper)):
        if not out:
            break
        ax, ay = clipper[i]
        bx, by = clipper[(i + 1) % len(clipper)]
        inp, out = out, []

        def side(p):
            return (bx - ax) * (p[1] - ay) - (by - ay) * (p[0] - ax)
        for j in range(len(inp)):
            P, Q = inp[j], inp[(j + 1) % len(inp)]
            sp, sq = side(P), side(Q)
            if sp >= 0:
                out.append(P)
            if (sp >= 0) != (sq >= 0):
                t = sp / (sp - sq)
                out.append((P[0] + t * (Q[0] - P[0]), P[1] + t * (Q[1] - P[1])))
    return out


def contact(a, b):
    """length of the boundary shared by two polygons"""
    tot = 0
    for i in range(len(a)):
        p0, p1 = a[i], a[(i + 1) % len(a)]
        for j in range(len(b)):
            q0, q1 = b[j], b[(j + 1) % len(b)]
            dx, dy = p1[0] - p0[0], p1[1] - p0[1]
            L = math.hypot(dx, dy)
            ux, uy = dx / L, dy / L
            # collinear: both q points on the line of p
            if abs(ux * (q0[1] - p0[1]) - uy * (q0[0] - p0[0])) > 1e-7:
                continue
            if abs(ux * (q1[1] - p0[1]) - uy * (q1[0] - p0[0])) > 1e-7:
                continue
            t0 = ux * (q0[0] - p0[0]) + uy * (q0[1] - p0[1])
            t1 = ux * (q1[0] - p0[0]) + uy * (q1[1] - p0[1])
            lo, hi = max(0, min(t0, t1)), min(L, max(t0, t1))
            if hi > lo + 1e-9:
                tot += hi - lo
    return tot


def inside(poly, x, y):
    c = False
    n = len(poly)
    for i in range(n):
        xi, yi = poly[i]
        xj, yj = poly[i - 1]
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi) + xi:
            c = not c
    return c


def check(f):
    errs = []
    kinds = [k for k, _ in f.pieces]
    for k in KINDS:
        if kinds.count(k) != COUNT[k]:
            errs.append("count %s=%d" % (k, kinds.count(k)))
    for k, pts in f.pieces:
        try:
            identify(k, pts)
        except ValueError:
            errs.append("bad shape " + k)
    polys = [fpts(pts) for _, pts in f.normalized()]
    n = len(polys)
    for i in range(n):
        for j in range(i + 1, n):
            c = clip(polys[i], polys[j])
            if len(c) >= 3 and abs(area(c)) > 1e-6:
                errs.append("overlap %d%s/%d%s %.3f" % (i, kinds[i], j, kinds[j], abs(area(c))))
    # edge connectivity
    adj = [[contact(polys[i], polys[j]) > 0.2 for j in range(n)] for i in range(n)]
    seen, st = {0}, [0]
    while st:
        a = st.pop()
        for b in range(n):
            if adj[a][b] and b not in seen:
                seen.add(b)
                st.append(b)
    if len(seen) != n:
        errs.append("loose " + ",".join("%d%s" % (i, kinds[i]) for i in range(n) if i not in seen))
    # holes: raster flood from outside
    W, H = f.size()
    R = 20
    gw, gh = int(W * R) + 4, int(H * R) + 4
    grid = [[0] * gw for _ in range(gh)]
    for y in range(gh):
        for x in range(gw):
            px, py = (x - 2 + 0.5) / R, (y - 2 + 0.5) / R
            for p in polys:
                if inside(p, px, py):
                    grid[y][x] = 1
                    break
    st = [(0, 0)]
    grid[0][0] = 2
    while st:
        x, y = st.pop()
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if 0 <= nx < gw and 0 <= ny < gh and grid[ny][nx] == 0:
                grid[ny][nx] = 2
                st.append((nx, ny))
    holes = sum(row.count(0) for row in grid)
    if holes > 3:
        errs.append("hole %.2f" % (holes / R / R))
    return errs


# ---- contact sheet ----
COLS = [(196, 70, 52), (60, 90, 160), (70, 140, 100), (222, 168, 66), (230, 200, 120), (120, 80, 50), (150, 110, 190)]


def sheet(figs, path, colour=False, scale=24, cols=8):
    from PIL import Image, ImageDraw
    cw, chh = 10 * scale, 10 * scale + 16
    rows = (len(figs) + cols - 1) // cols
    img = Image.new("RGB", (cols * cw, rows * chh), (238, 228, 205))
    d = ImageDraw.Draw(img)
    for idx, f in enumerate(figs):
        cx, cy = idx % cols * cw, idx // cols * chh
        W, H = f.size()
        ox, oy = cx + (cw - W * scale) / 2, cy + (cw - H * scale) / 2
        errs = check(f)
        for i, (k, pts) in enumerate(f.normalized()):
            poly = [(ox + x * scale, oy + y * scale) for x, y in fpts(pts)]
            if colour:
                d.polygon(poly, fill=COLS[i], outline=(30, 20, 10))
            else:
                d.polygon(poly, fill=(28, 26, 30), outline=(28, 26, 30))
        d.rectangle([cx, cy, cx + cw - 1, cy + chh - 1], outline=(200, 185, 160))
        label = "%d %s" % (idx + 1, f.name)
        if errs:
            label += " !" + " ".join(errs)
        d.text((cx + 4, cy + cw), label[:60], fill=(160, 0, 0) if errs else (40, 30, 20))
    img.save(path)


def export_c(figs):
    lines = []
    for f in figs:
        ent = []
        order = sorted(range(7), key=lambda i: "LMSQP".index(f.pieces[i][0]))
        norm = f.normalized()
        for i in order:
            k, pts = norm[i]
            rot, flip = identify(k, pts)
            base = variant(k, rot, flip)
            # anchor: the variant's vertex 0, placed
            (bx, by), (px, py) = bmin(base), bmin(pts)
            ax, ay = px + base[0][0] - bx, py + base[0][1] - by
            vals = []
            for c in (ax, ay):
                a, b = c.p * 4, c.q * 4
                assert a.denominator == 1 and b.denominator == 1, (f.name, c)
                vals += [int(a), int(b)]
            ent.append("{%d,%d,%d,%d,%d,%d,%d}" % (KINDS.index(k), rot, flip, *vals))
        lines.append("    {%d, {%s}}, /* %s */" % (f.chapter, ",".join(ent), f.name))
    return "\n".join(lines)


# ---- the A-grid solver: a silhouette drawn by cells ('#' full, '.' empty, a b c d: the halves top-left, top-right,
# bottom-right, bottom-left), tiled by the pieces left ----
HALF = {"#": (0, 1, 2, 3), ".": (), "a": (0, 3), "b": (0, 1), "c": (1, 2), "d": (2, 3)}
QC = [(0.5, 1 / 6), (5 / 6, 0.5), (0.5, 5 / 6), (1 / 6, 0.5)]


def a_variants(lat="A"):
    out = []
    for k in KINDS:
        for flip in ((0, 1) if k == "P" else (0,)):
            for rot in range(8):
                pts = variant(k, rot, flip)
                if lat == "B":
                    pts = [(x * r, y * r) for x, y in pts]
                if not all(x.q == 0 and y.q == 0 and x.p.denominator == 1 and y.p.denominator == 1 for x, y in pts):
                    continue
                key = norm_set(pts)
                if any(o[0] == k and norm_set(variant(k, o[1], o[2])) == key for o in out):
                    continue
                fp = fpts(pts)
                x0 = int(math.floor(min(p[0] for p in fp)))
                y0 = int(math.floor(min(p[1] for p in fp)))
                x1 = int(math.ceil(max(p[0] for p in fp)))
                y1 = int(math.ceil(max(p[1] for p in fp)))
                qs = []
                for cy in range(y0, y1):
                    for cx in range(x0, x1):
                        for q in range(4):
                            if inside(fp, cx + QC[q][0], cy + QC[q][1]):
                                qs.append((cy, cx, q))
                out.append((k, rot, flip, qs))
    return out


AV = {}


def solve_cells(rows, pieces=None, limit=1, lat="A"):
    """tile the drawn cells with the pieces (a dict kind->count); a list of solutions [(kind, rot, flip, dx, dy)];
    lat B: the cells are h wide, the pieces turned an eighth from the A ones"""
    if lat not in AV:
        AV[lat] = a_variants(lat)
    av = AV[lat]
    pieces = dict(pieces or COUNT)
    target = set()
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            for q in HALF[ch]:
                target.add((y, x, q))
    need = sum(AREA[k] * n for k, n in pieces.items()) * 4 * (2 if lat == "B" else 1)
    if len(target) != need:
        raise ValueError("cells hold %s quarters, the pieces %s" % (len(target), need))
    sols, cur = [], []

    def go(free):
        if len(sols) >= limit:
            return
        if not free:
            sols.append(list(cur))
            return
        first = min(free)
        for k, rot, flip, qs in av:
            if not pieces.get(k):
                continue
            for (cy, cx, q) in qs:
                if q != first[2]:
                    continue
                dy, dx = first[0] - cy, first[1] - cx
                moved = {(a + dy, b + dx, c) for a, b, c in qs}
                if moved <= free:
                    pieces[k] -= 1
                    cur.append((k, rot, flip, dx, dy))
                    go(free - moved)
                    cur.pop()
                    pieces[k] += 1

    go(frozenset(target))
    return sols


def add_cells(f, rows, ox=0, oy=0, pieces=None, pick=0, lat="A"):
    if isinstance(pieces, str):
        d = {}
        for c in pieces:
            d[c] = d.get(c, 0) + 1
        pieces = d
    sols = solve_cells(rows, pieces, limit=pick + 1, lat=lat)
    if len(sols) <= pick:
        raise ValueError("%s: no tiling" % f.name)
    sc = h if lat == "B" else 1
    for k, rot, flip, dx, dy in sols[pick]:
        pts = variant(k, rot, flip)
        f.pieces.append([k, [(x + dx * sc + ox, y + dy * sc + oy) for x, y in pts]])
    return f


def ref_sheet(path, scale=40):
    from PIL import Image, ImageDraw
    img = Image.new("RGB", (16 * 3 * scale, 5 * 4 * scale), (238, 228, 205))
    d = ImageDraw.Draw(img)
    for ki, k in enumerate(KINDS):
        for flip in (0, 1):
            for rot in range(8):
                col = flip * 8 + rot
                pts = fpts(variant(k, rot, flip))
                mx, my = min(p[0] for p in pts), min(p[1] for p in pts)
                ox, oy = col * 3 * scale + 10, ki * 4 * scale + 20
                poly = [(ox + (x - mx) * scale, oy + (y - my) * scale) for x, y in pts]
                d.polygon(poly, fill=(120, 80, 50), outline=(0, 0, 0))
                d.ellipse([poly[0][0] - 4, poly[0][1] - 4, poly[0][0] + 4, poly[0][1] + 4], fill=(220, 30, 30))
                d.text((ox, oy - 16), "%s r%d%s" % (k, rot, "f" if flip else ""), fill=(0, 0, 0))
    img.save(path)
