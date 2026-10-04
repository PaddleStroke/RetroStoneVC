#!/usr/bin/env python3
"""Nutty Branch: a tiny 3D renderer for the code-drawn props (make_art.py), so that every object sits ON the curved
bark, seen from the game's own camera, at each of its pre-drawn sizes.

The game looks along the branch from behind and above (the camera 72 units over the bark, 120 behind the squirrel:
an elevation of about 31 degrees). A prop is built in its local frame on the bark (x to the right, y up the bark's
normal, z along the branch, away from the camera) from ellipsoids (spheres, capsules as chains of spheres, flat
discs for leaves), each with a material ramp. It is ray-cast orthographically (numpy, one ellipsoid at a time over
the whole pixel grid, a depth buffer), shaded by the house light (from the top-left, a little from the front: 4 flat
shades per material), given a dithered contact shadow on the bark plane and the house 1-px outline. A prop sitting
at an angle round the branch is rolled by that angle about the branch axis (the view axis), so it leans with the
surface: the same mapping as the bark (x = R sin(angle)).

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
"""
import math

import numpy as np

ELEV = math.atan2(72.0, 120.0)            # the camera's look-down angle at the leader
CE, SE = math.cos(ELEV), math.sin(ELEV)
LIGHT = np.array([-0.45, 0.80, -0.40])    # from the top-left, a little toward the camera
LIGHT = LIGHT / np.linalg.norm(LIGHT)
TH = (0.62, 0.18, -0.45)                  # house_style.shade_nd thresholds


def rot_x(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[1, 0, 0], [0, c, -s], [0, s, c]])


def rot_y(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])


def rot_z(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])


class Ell:
    """An ellipsoid: centre (x, y, z), radii (a, b, c) along its own axes, a rotation (3x3, its axes in the prop's
    frame), a material (a 4-colour ramp light..dark), an optional texture (fn(local point, normal) -> shade shift)."""

    def __init__(self, c, r, mat, R=None, tex=None):
        self.c = np.array(c, dtype=float)
        self.r = np.array(r, dtype=float)
        self.R = np.eye(3) if R is None else np.array(R, dtype=float)
        self.mat = mat
        self.tex = tex


def capsule(p0, p1, r0, r1, mat, n=None, tex=None):
    """A tapered limb from p0 (radius r0) to p1 (radius r1): a chain of spheres."""
    p0, p1 = np.array(p0, float), np.array(p1, float)
    L = np.linalg.norm(p1 - p0)
    n = n or max(2, int(L / max(0.4, min(r0, r1) * 0.6)) + 1)
    out = []
    for i in range(n):
        t = i / (n - 1.0)
        r = r0 + (r1 - r0) * t
        out.append(Ell(p0 + (p1 - p0) * t, (r, r, r), mat, tex=tex))
    return out


def render(prims, k, roll=0.0, shadow=None, pad=2, out_col=None, shadow_col=None, crease=True):
    """Ray-cast the prims at scale k (px per unit), rolled by `roll` (radians, clockwise on screen) about the
    anchor's branch axis. Returns (pixels {(x, y): colour}, anchor (ax, ay), size (w, h)) with the anchor = the
    prop's origin on the bark. shadow = (cx, cz, rx, rz) on the bark plane (units), drawn dithered."""
    Rr = rot_z(-roll)
    prims = [Ell(Rr @ p.c, p.r, p.mat, Rr @ p.R, p.tex) for p in prims]
    # the screen box (units): project every ellipsoid's bounding sphere
    us, ws = [], []
    for p in prims:
        rad = float(max(p.r))
        x, y, z = p.c
        u, w = x, y * CE + z * SE
        us += [u - rad, u + rad]
        ws += [w - rad, w + rad]
    if shadow:
        cx, cz, rx, rz = shadow
        us += [cx - rx, cx + rx]
        ws += [(cz - rz) * SE, (cz + rz) * SE]
    u0, u1, w0, w1 = min(us + [0]), max(us + [0]), min(ws + [0]), max(ws + [0])
    W = int(math.ceil((u1 - u0) * k)) + 2 * pad + 1
    H = int(math.ceil((w1 - w0) * k)) + 2 * pad + 1
    ax = (0 - u0) * k + pad
    ay = (w1 - 0) * k + pad
    # pixel centres -> (u, w) units
    xs = (np.arange(W) + 0.5 - ax) / k
    ys = (ay - (np.arange(H) + 0.5)) / k
    U, Wg = np.meshgrid(xs, ys)
    depth = np.full((H, W), np.inf)
    shade = np.zeros((H, W), dtype=int)
    owner = np.full((H, W), -1, dtype=int)
    D = np.array([0.0, -SE, CE])                   # the view ray (toward +depth)
    for i, p in enumerate(prims):
        # ray origin on the screen plane: x = u, y = w cos, z = w sin (t = 0), then P = O + t D
        O = np.stack([U, Wg * CE, Wg * SE], axis=-1) - p.c
        Rt = p.R.T
        Ol = O @ Rt.T / p.r
        Dl = (Rt @ D) / p.r
        a = float(Dl @ Dl)
        b = 2 * (Ol @ Dl)
        c = np.sum(Ol * Ol, axis=-1) - 1
        disc = b * b - 4 * a * c
        hit = disc >= 0
        t = np.where(hit, (-b - np.sqrt(np.maximum(disc, 0))) / (2 * a), np.inf)
        closer = hit & (t < depth)
        if not closer.any():
            continue
        Pl = Ol + np.where(np.isfinite(t), t, 0)[..., None] * Dl                     # the hit, in the unit sphere's frame
        n = (Pl / p.r) @ Rt                              # the normal in the prop's frame
        n = n / np.linalg.norm(n, axis=-1, keepdims=True)
        nd = n @ LIGHT
        s = np.where(nd > TH[0], 0, np.where(nd > TH[1], 1, np.where(nd > TH[2], 2, 3)))
        if p.tex is not None:
            with np.errstate(invalid="ignore"):
                s = np.clip(s + p.tex(Pl, n), 0, 3)
        depth = np.where(closer, t, depth)
        shade = np.where(closer, s, shade)
        owner = np.where(closer, i, owner)
    px = {}
    for y in range(H):
        for x in range(W):
            o = owner[y, x]
            if o >= 0:
                mat = prims[o].mat
                px[(x, y)] = mat[shade[y, x]]
    if crease:                                           # a 1-px crease where a nearer part covers a farther one
        for (x, y), col in list(px.items()):
            o = owner[y, x]
            for dx, dy in ((0, -1), (-1, 0), (1, 0), (0, 1)):
                q = owner[y + dy, x + dx] if 0 <= y + dy < H and 0 <= x + dx < W else -1
                if q >= 0 and q != o and prims[q].mat is not prims[o].mat and depth[y + dy, x + dx] < depth[y, x] - 1.5:
                    px[(x, y)] = prims[o].mat[3]
                    break
    if out_col is not None:                              # the house outline, outside the shape
        add = {}
        for (x, y) in px:
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                q = (x + dx, y + dy)
                if q not in px and 0 <= q[0] < W and 0 <= q[1] < H:
                    add[q] = out_col
        px.update(add)
    if shadow and shadow_col is not None:                # the contact shadow on the bark plane (dithered)
        cx, cz, rx, rz = shadow
        for y in range(H):
            for x in range(W):
                if (x, y) in px:
                    continue
                u = (x + 0.5 - ax) / k
                w = (ay - (y + 0.5)) / k
                z = w / SE                                # the ray meets the bark plane y = 0 at z = w / sin
                if ((u - cx) / rx) ** 2 + ((z - cz) / rz) ** 2 <= 1 and (x + y) % 2 == 0:
                    px[(x, y)] = shadow_col
    return px, (ax, ay), (W, H)
