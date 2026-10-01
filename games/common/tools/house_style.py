#!/usr/bin/env python3
"""The 8BCraft house style for RetroStone VC games: code-drawn pixel art helpers.

Extracted from Leady Squid's make_art.py (the reference look). The games' make_art.py
scripts import it; the C side of the same kit is games/common/src/house_ui.c, and the
rules are in docs/art-direction.md.

    import sys, os
    sys.path.insert(0, os.path.join(ROOT, "games", "common", "tools"))
    import house_style as hs

What is here:
  Canvas             a tiny pixel canvas (None = transparent), outline(), paste(), image()
  palettes           HOUSE (the master ramps), ACCENTS (per-game accent ramps), ramp()
  shading            LIGHT_DIR (top-left), shade_nd() (4-shade ramp by a normal), shade3() (cylinder)
  dither             checker(), bayer4(), dither_ok()
  shapes             ellipse(), capsule(), seg_dist(), squash() (squash and stretch), flip_h(), flip_v()
  the UI kit         digit(), medal(), button_glyph(), hint(), sparkle(), logo(), font_atlas(), panel()
  palette swap       hue_swap() (player 2 = the hero with its hue rotated)

    python3 games/common/tools/house_style.py --out DIR     writes the reference sheets (palette, font, kit)

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import colorsys
import math
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "sdk", "tools"))
from gen_font import G as FONT  # noqa: E402  (the SDK's 5x7 font: the house font)

MAGENTA = (255, 0, 255)          # sheet background (transparent)
MAG = MAGENTA

# ---- the master house palette -----------------------------------------------------------------------------
# Every ramp goes from the lightest to the darkest shade. Outlines are never black: a dark, slightly violet
# navy (or the darkest shade of the material, for objects that must melt into their scenery).
HOUSE = {
    # outlines
    "outline":    [(22, 18, 40)],               # UI sprites (digits, medals, glyphs)
    "outline_logo": [(26, 16, 40)],              # the title logo
    "ink":        [(18, 8, 26)],                # pupils, the darkest dot
    "white":      [(250, 250, 250)],
    # UI navy: text, panels, banners (the BG UI palette, house_ui.c HU_* entries)
    "cream":      [(255, 246, 220)],            # HU_CREAM: text ink
    "ui_navy":    [(120, 170, 210), (54, 84, 136), (42, 68, 118), (16, 24, 56), (12, 20, 44)],
    #              HU_LIGHT (rim)   HU_HILITE     HU_FILL        HU_OUTLINE   HU_DARK (shadow)
    "ui_gold":    [(250, 210, 90)],             # HU_GOLD: banner titles, "NEW BEST"
    "digit":      [(250, 250, 250), (168, 200, 232)],   # score digits: white, the bottom two rows blue-grey
    # medals (shells, coins...): light, dark
    "bronze":     [(210, 142, 82), (138, 76, 38)],
    "silver":     [(222, 230, 238), (128, 138, 156)],
    "gold":       [(252, 216, 72), (186, 128, 22)],
    "pearl":      [(255, 250, 238), (242, 202, 214), (200, 138, 170)],
    "spark":      [(255, 238, 150)],
    # materials shared by the family
    "skin":       [(255, 222, 196), (240, 184, 150), (206, 140, 108), (150, 92, 72)],
    "lead":       [(208, 214, 226), (160, 166, 180), (116, 122, 138)],      # metal grey (the LEADY logo)
    "iron":       [(172, 178, 192), (114, 120, 134), (60, 64, 76)],
    "wood":       [(212, 162, 102), (174, 122, 70), (130, 86, 46), (86, 52, 28)],
    "sand":       [(248, 232, 190), (232, 208, 152), (208, 176, 120), (168, 134, 86)],
    "leaf":       [(198, 220, 108), (146, 182, 70), (92, 134, 46), (52, 86, 30)],
    "water":      [(196, 238, 255), (112, 184, 226)],                        # bubbles, splashes
    "shadow_navy": [(20, 34, 70)],               # the logo's drop shadow
}

# per-game accent ramps (light -> dark); a new game adds its own here
ACCENTS = {
    "squid":   [(234, 200, 250), (194, 134, 230), (150, 82, 194), (100, 46, 138)],   # Leady Squid
    "squid_logo": [(236, 196, 252), (190, 120, 228), (138, 70, 180)],
    "coral":   [(255, 206, 176), (248, 150, 136), (222, 96, 106), (160, 50, 74)],
    "rust":    [(232, 172, 112), (198, 124, 74), (152, 84, 52), (100, 52, 36)],
    "pink":    [(255, 208, 226), (246, 150, 190), (214, 92, 146), (150, 46, 98)],     # Pogo Mamie
    "beaver":  [(226, 170, 110), (184, 120, 66), (136, 82, 42), (88, 50, 26)],        # Beaver Rush
    "duck":    [(255, 244, 150), (252, 214, 64), (226, 164, 28), (164, 104, 20)],      # Duck Parade
    "pancake": [(255, 226, 160), (240, 184, 96), (206, 136, 56), (146, 86, 34)],       # Pancake Tower
    "syrup":   [(214, 126, 62), (160, 80, 30), (104, 46, 18)],
}


def ramp(name):
    """A ramp by name, house or accent."""
    return HOUSE[name] if name in HOUSE else ACCENTS[name]


# ---- a tiny pixel canvas ------------------------------------------------------------------------------------
class Canvas:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.p = [[None] * w for _ in range(h)]

    def set(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.p[y][x] = c

    def get(self, x, y):
        return self.p[y][x] if 0 <= x < self.w and 0 <= y < self.h else None

    def rect(self, x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.set(x, y, c)

    def outline(self, c, diag=False):
        """1-px outline outside the drawing (transparent pixels touching a filled one)."""
        add = []
        for y in range(self.h):
            for x in range(self.w):
                if self.p[y][x] is not None:
                    continue
                nb = [(1, 0), (-1, 0), (0, 1), (0, -1)] + ([(1, 1), (-1, -1), (1, -1), (-1, 1)] if diag else [])
                if any(self.get(x + dx, y + dy) not in (None, c) for dx, dy in nb):
                    add.append((x, y))
        for x, y in add:
            self.p[y][x] = c

    def image(self):
        im = Image.new("RGB", (self.w, self.h), MAG)
        px = im.load()
        for y in range(self.h):
            for x in range(self.w):
                if self.p[y][x] is not None:
                    px[x, y] = self.p[y][x]
        return im

    def paste(self, other, ox, oy):
        for y in range(other.h):
            for x in range(other.w):
                if other.p[y][x] is not None:
                    self.set(ox + x, oy + y, other.p[y][x])

    def scaled(self, k):
        """Nearest-neighbour enlargement (previews only: never scale sprites in the game art)."""
        out = Canvas(self.w * k, self.h * k)
        for y in range(out.h):
            for x in range(out.w):
                out.p[y][x] = self.p[y // k][x // k]
        return out


# ---- shading ----------------------------------------------------------------------------------------------
LIGHT_DIR = (-0.62, -0.78)      # the light comes from the top-left (screen space, y down)


def shade_nd(nd, r4):
    """Pick one of 4 shades (light -> dark: highlight, light, mid, dark) from a normalised light dot product
    (nd in about -1..1): the thresholds of Leady Squid's body."""
    return r4[0] if nd > 0.62 else r4[1] if nd > 0.18 else r4[2] if nd > -0.45 else r4[3]


def shade3(P, x, y, w, x0=0):
    """Cylinder shading across a column: light on the left, dark on the right (P: h, l, m, d keys)."""
    f = (x - x0) / float(max(1, w - 1))
    return P["h"] if f < 0.12 else P["l"] if f < 0.4 else P["m"] if f < 0.78 else P["d"]


def ramp_dict(r4):
    """A 4-shade ramp as the h/l/m/d dict of shade3()."""
    return dict(h=r4[0], l=r4[1], m=r4[2], d=r4[3])


# ---- dither -------------------------------------------------------------------------------------------------
BAYER4 = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]


def checker(x, y, phase=0):
    """The house 50% dither: only for fades (smoke, rays' ends, ghosts), never inside a sprite's body."""
    return (x + y + phase) % 2 == 0


def bayer4(x, y):
    return BAYER4[y & 3][x & 3]


def dither_ok(x, y, level):
    """level 0..16: keep the pixel when the ordered dither says so (16 = all)."""
    return bayer4(x, y) < level


# ---- shapes ---------------------------------------------------------------------------------------------------
def seg_dist(px, py, ax, ay, bx, by):
    vx, vy = bx - ax, by - ay
    L = vx * vx + vy * vy
    t = 0 if L == 0 else max(0.0, min(1.0, ((px - ax) * vx + (py - ay) * vy) / L))
    dx, dy = px - (ax + t * vx), py - (ay + t * vy)
    return math.hypot(dx, dy), t


def _shade_at(sx, sy, r4, scale):
    nd = (sx * LIGHT_DIR[0] + sy * LIGHT_DIR[1]) / float(scale)
    return shade_nd(nd, r4)


def ellipse(cv, cx, cy, rx, ry, r4, flat=None):
    """A filled ellipse shaded by the house light (4-shade ramp r4), or flat colour `flat`.
    Centre and radii in pixels (floats allowed: cx = 7.5 centres on a 16-px cell)."""
    for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
        for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
            dx, dy = (x + 0.5 - cx) / rx, (y + 0.5 - cy) / ry
            if dx * dx + dy * dy <= 1.0:
                cv.set(x, y, flat if flat else _shade_at(dx, dy, r4, 1.0))


def capsule(cv, ax, ay, bx, by, r, r4, flat=None):
    """A filled capsule (a segment with round ends, radius r), shaded across its width."""
    x0, x1 = int(min(ax, bx) - r) - 1, int(max(ax, bx) + r) + 2
    y0, y1 = int(min(ay, by) - r) - 1, int(max(ay, by) + r) + 2
    for y in range(y0, y1):
        for x in range(x0, x1):
            d, t = seg_dist(x + 0.5, y + 0.5, ax, ay, bx, by)
            if d <= r:
                px, py = ax + (bx - ax) * t, ay + (by - ay) * t
                cv.set(x, y, flat if flat else _shade_at((x + 0.5 - px) / r, (y + 0.5 - py) / r, r4, 1.0))


def squash(cv, sx, sy=None, anchor="bottom"):
    """Squash and stretch: scale a drawing by sx horizontally and sy vertically around its anchor
    ('bottom' = the feet stay on the ground, 'center'). sy defaults to 1/sx (the area is kept).
    Nearest-neighbour resampling, then redraw the outline yourself (draw the body, squash, outline)."""
    if sy is None:
        sy = 1.0 / sx
    out = Canvas(cv.w, cv.h)
    ax = cv.w / 2.0
    ay = float(cv.h) if anchor == "bottom" else cv.h / 2.0
    for y in range(cv.h):
        for x in range(cv.w):
            u = (x + 0.5 - ax) / sx + ax
            v = (y + 0.5 - ay) / sy + ay
            c = cv.get(int(math.floor(u)), int(math.floor(v)))
            if c is not None:
                out.p[y][x] = c
    return out


def flip_h(cv):
    out = Canvas(cv.w, cv.h)
    for y in range(cv.h):
        out.p[y] = list(reversed(cv.p[y]))
    return out


def flip_v(cv):
    out = Canvas(cv.w, cv.h)
    out.p = [list(r) for r in reversed(cv.p)]
    return out


def eye(cv, x, y, look=(1, 0), white=None, pupil=None, big=False):
    """The house eye: a 2x3 (or 3x4 when big) white with a 1x2 pupil on the side it looks at."""
    white = white or HOUSE["white"][0]
    pupil = pupil or HOUSE["ink"][0]
    w, h = (3, 4) if big else (2, 3)
    cv.rect(x, y, x + w - 1, y + h - 1, white)
    px = x + (w - 1 if look[0] > 0 else 0)
    py = y + (1 if look[1] >= 0 else 0) + (1 if big else 0)
    cv.set(px, py, pupil), cv.set(px, py + 1, pupil)


# ---- palette swap (player 2) ----------------------------------------------------------------------------------
def hue_swap(rgbs, h_lo, h_hi, shift, sat_min=0.25, sat_mul=1.05):
    """Rotate the hue of the colours whose hue is in (h_lo, h_hi) (0..1) by `shift`: player 2 is the hero with
    its main material recoloured (Leady Squid: the purples 0.70..0.86 turned pink by +0.13)."""
    out = []
    for c in rgbs:
        r, g, b = [v / 255.0 for v in c]
        h, l, s = colorsys.rgb_to_hls(r, g, b)
        if s > sat_min and h_lo < h < h_hi:
            h = (h + shift) % 1.0
            s = min(1.0, s * sat_mul)
        r, g, b = colorsys.hls_to_rgb(h, l, s)
        out.append((int(r * 255), int(g * 255), int(b * 255)))
    return out


# ---- four players: the house palette swaps ------------------------------------------------------------------------
# P1 is the hero's own palette (OBJ 0); P2 the game's hue_swap (OBJ 1); P3 and P4 (OBJ 4 and 5, house_ui.h
# hu_player_pal) the same material rotated to the two hues farthest from P1's and P2's, so the four main colours
# stay far apart on the colour wheel. Only the hero's main material turns (h_lo..h_hi): the outline, the eyes,
# skin, metal and the white highlights never change.
def main_hue(rgbs, h_lo, h_hi, sat_min=0.25):
    """The mean hue (0..1) of the colours a hue_swap(rgbs, h_lo, h_hi, ...) would turn (None if none)."""
    xs = ys = 0.0
    n = 0
    for c in rgbs:
        h, _l, s = colorsys.rgb_to_hls(*[v / 255.0 for v in c])
        if s > sat_min and h_lo < h < h_hi:
            xs += math.cos(h * 2 * math.pi)
            ys += math.sin(h * 2 * math.pi)
            n += 1
    if not n:
        return None
    return (math.atan2(ys, xs) / (2 * math.pi)) % 1.0


def player_shifts(rgbs, h_lo, h_hi, p2_shift, sat_min=0.25):
    """The hue shifts of players 1..4: [0, p2_shift, s3, s4]; s3 and s4 put P3's and P4's main hue in the largest
    gaps left by P1, P2 (and P3), on a 1/72 grid (deterministic)."""
    h1 = main_hue(rgbs, h_lo, h_hi, sat_min)
    if h1 is None:
        return [0.0, p2_shift, 1 / 3.0, 2 / 3.0]
    taken = [h1, (h1 + p2_shift) % 1.0]
    shifts = [0.0, p2_shift]
    for _ in range(2):
        best, best_d = 0.0, -1.0
        for k in range(72):
            h = (h1 + k / 72.0) % 1.0
            d = min(min(abs(h - t), 1 - abs(h - t)) for t in taken)
            if d > best_d + 1e-9:
                best, best_d = k / 72.0, d
        taken.append((h1 + best) % 1.0)
        shifts.append(best)
    return shifts


def player_palettes(rgbs, h_lo, h_hi, p2_shift, sat_min=0.25, sat_mul=1.05):
    """The four players' palettes of a hero palette (a list of RGB): [P1 (as is), P2, P3, P4] (house rule above)."""
    out = [list(rgbs)]
    for s in player_shifts(rgbs, h_lo, h_hi, p2_shift, sat_min)[1:]:
        out.append(hue_swap(rgbs, h_lo, h_hi, s, sat_min, sat_mul))
    return out


# ---- the UI kit (sprites) ------------------------------------------------------------------------------------
UI = dict(out=(22, 18, 40), white=(250, 250, 250), shade=(168, 200, 232), bd=(138, 76, 38), bl=(210, 142, 82),
          sd=(128, 138, 156), sl=(222, 230, 238), gd=(186, 128, 22), gl=(252, 216, 72), pd=(200, 138, 170),
          pl=(242, 202, 214), pearl=(255, 250, 238))
SPARK = dict(spark=(255, 238, 150), white=(255, 255, 255))


def digit(n):
    """A 16x16 score digit: the font at 2x, white with the bottom two rows blue-grey, a diagonal outline."""
    cv = Canvas(16, 16)
    g = FONT[str(n)]
    for y, row in enumerate(g[:7]):
        for x, c in enumerate(row):
            if c == "#":
                for sy in range(2):
                    for sx in range(2):
                        cv.set(2 + x * 2 + sx, 1 + y * 2 + sy, UI["shade"] if y >= 5 else UI["white"])
    cv.outline(UI["out"], diag=True)
    return cv


MEDAL_TIERS = ["bronze", "silver", "gold", "pearl"]


def medal(kind):
    """A 24x24 medal, tier 0..3 (bronze, silver, gold, pearl): Leady Squid's scallop shell. Games may draw
    their own icon but keep the four tiers and their colours."""
    cv = Canvas(24, 24)
    dark, light = [(UI["bd"], UI["bl"]), (UI["sd"], UI["sl"]), (UI["gd"], UI["gl"]), (UI["pd"], UI["pl"])][kind]
    cx, cy = 12, 17
    for y in range(24):
        for x in range(24):
            dx, dy = x + 0.5 - cx, y + 0.5 - cy
            r = math.hypot(dx, dy)
            ang = math.atan2(-dy, dx)
            if 0.08 < ang < math.pi - 0.08 and r < 10.5 - 0.8 * abs(math.cos(ang * 7)):
                rib = int((ang / math.pi) * 7 + 0.5)
                edge = abs((ang / math.pi) * 7 - rib) < 0.16
                col = dark if edge or r > 9.3 else light
                if dx + dy < -6 and not edge:
                    col = UI["white"] if r < 7 else light
                cv.set(x, y, col)
            # the hinge
            if 16 <= y <= 19 and abs(dx) < 3.2:
                cv.set(x, y, dark)
    if kind == 3:                            # the pearl
        for y in range(24):
            for x in range(24):
                if math.hypot(x + 0.5 - 12, y + 0.5 - 13) < 3.3:
                    cv.set(x, y, UI["pearl"] if (x + y) > 22 else UI["white"])
        cv.set(11, 12, UI["white"])
    cv.outline(UI["out"])
    return cv


def button_glyph(label="A", frame=0):
    """The 16x16 pad-button glyph: a round silver button with its letter, frame 1 = pressed (1 px down,
    no rim shadow). Shown left of every 'PRESS A' prompt, blinking with it."""
    cv = Canvas(16, 16)
    oy = 1 if frame else 0
    for y in range(16):
        for x in range(16):
            d = math.hypot(x + 0.5 - 8, y + 0.5 - 7.5 - oy)
            if d < 6.2:
                cv.set(x, y, UI["sl"] if (x + y) < 14 else UI["sd"])
            elif d < 7.2 and y > 8 and not frame:
                cv.set(x, y, UI["sd"])
    g = FONT[label]
    for y, row in enumerate(g[:7]):
        for x, c in enumerate(row):
            if c == "#":
                cv.set(6 + x, 4 + y + oy, UI["out"])
    cv.outline(UI["out"])
    return cv


def dpad_glyph(frame=0):
    """The 16x16 D-pad glyph (house_ui.c make_dpad): a silver cross with a dark centre, frame 1 = pressed. Shown
    left of 'PRESS ANY ARROW' and 'PRESS <-/->' prompts."""
    cv = Canvas(16, 16)
    oy = 1 if frame else 0
    for y in range(16):
        for x in range(16):
            yy = y - oy
            inside = (6 <= x <= 9 and 2 <= yy <= 13) or (6 <= yy <= 9 and 2 <= x <= 13)
            if inside:
                cv.set(x, y, UI["sl"] if (x + yy) < 15 else UI["sd"])
            elif not frame and yy == 14 and 6 <= x <= 9:
                cv.set(x, y, UI["sd"])
            elif not frame and yy == 10 and (2 <= x <= 5 or 10 <= x <= 13):
                cv.set(x, y, UI["sd"])
    for y in (7, 8):
        for x in (7, 8):
            cv.set(x, y + oy, UI["sd"])
    cv.outline(UI["out"])
    return cv


def hint(frame):
    """Leady Squid's name for the A glyph."""
    return button_glyph("A", frame)


def sparkle(frame):
    """An 8x8 twinkle (2 frames) next to a medal."""
    cv = Canvas(8, 8)
    r = 3 if frame == 0 else 2
    for i in range(-r, r + 1):
        cv.set(4 + i, 4, SPARK["spark"])
        cv.set(4, 4 + i, SPARK["spark"])
    cv.set(4, 4, SPARK["white"])
    if frame == 1:
        for d in (-1, 1):
            cv.set(4 + d, 4 + d, SPARK["spark"]), cv.set(4 + d, 4 - d, SPARK["spark"])
    return cv


# ---- the title logo -------------------------------------------------------------------------------------------
LOGO_OUTLINE, LOGO_SHADOW = (26, 16, 40), (20, 34, 70)


def logo(words, ramps, size=(256, 64), scale=4, y0=14, outline=LOGO_OUTLINE, shadow=LOGO_SHADOW, depth=(3, 4)):
    """The house title logo: the font at `scale`, each word in its own 3-shade ramp (light top band, mid,
    dark bottom band), a 1-px outline all round (8 neighbours) and a navy drop shadow `depth` px below
    (half as far to the right): the 3D extrusion. Returns (image, info) where info has the glyph mask
    {(x, y): colour}, x0, y0, the advance and the scale, for extras (Leady Squid's rivets and bubbles).
    house_ui.c hu_logo() draws the same logo at run time."""
    W, H = size
    im = Image.new("RGB", (W, H), MAG)
    px = im.load()
    sc = scale
    adv = 5 * sc + 2
    text = " ".join(words)
    total = len(text) * adv - 2
    x0 = (W - total) // 2
    mask = {}
    i = 0
    for wi, word in enumerate(words):
        pal = ramps[wi % len(ramps)]
        for ch in word:
            g = FONT[ch]
            for y, row in enumerate(g[:7]):
                for x, c in enumerate(row):
                    if c != "#":
                        continue
                    for sy in range(sc):
                        for sx in range(sc):
                            X, Y = x0 + i * adv + x * sc + sx, y0 + y * sc + sy
                            yy = y * sc + sy
                            k = 0 if yy < 2 * sc else 1 if yy < 4.5 * sc else 2
                            mask[(X, Y)] = pal[k]
            i += 1
        i += 1                                                # the space between words
    for (X, Y) in mask:                                      # drop shadow
        for d in depth:
            if (X + d // 2, Y + d) not in mask:
                px[X + d // 2, Y + d] = shadow
    for (X, Y), c in mask.items():
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                if (X + dx, Y + dy) not in mask:
                    px[X + dx, Y + dy] = outline
    for (X, Y), c in mask.items():
        px[X, Y] = c
    return im, dict(mask=mask, x0=x0, y0=y0, adv=adv, scale=sc)


def bubble_ring(px, bx, by, r, colour=(196, 238, 255)):
    """A 1-px ring (bubbles, sparkles around a logo)."""
    for y in range(by - r - 1, by + r + 2):
        for x in range(bx - r - 1, bx + r + 2):
            if abs(math.hypot(x - bx, y - by) - r) < 0.6:
                px[x, y] = colour


# ---- the font and panels (as house_ui.c draws them; reference pictures and art mock-ups) -------------------------
UI_BG = {1: (255, 246, 220), 2: (16, 24, 56), 3: (42, 68, 118), 4: (120, 170, 210), 5: (12, 20, 44),
         6: (250, 210, 90), 7: (54, 84, 136)}


def font_glyph(ch, style="free"):
    """One glyph as house_ui.c draws it. style 'small' (8x8, cream, shadow 1 px down-right), 'box' (small on
    the panel fill), 'free' (16x16: the font at 2x, cream, outlined) or 'banner' (2x, gold, outlined, drop
    shadow, on the panel fill). Returns a Canvas."""
    rows = FONT.get(ch, FONT[" "])
    # the SDK's font tiles put the 5 columns at x = 1..5 (rs_font.c: bit 7 = x 0)
    bits = [[c == "#" for c in (" " + r).ljust(8)] for r in rows[:7]] + [[False] * 8]
    if style in ("small", "box"):
        cv = Canvas(8, 8)
        for y in range(8):
            for x in range(8):
                on = bits[y][x]
                sh = x > 0 and y > 0 and bits[y - 1][x - 1]
                v = 1 if on else 2 if sh else (3 if style == "box" else 0)
                if v:
                    cv.set(x, y, UI_BG[v])
        return cv
    px = [[0] * 16 for _ in range(16)]
    for y in range(7):
        for x in range(6):
            if bits[y][x]:
                for k in range(4):
                    px[1 + y * 2 + (k >> 1)][1 + x * 2 + (k & 1)] = 1
    for y in range(16):
        for x in range(16):
            if px[y][x]:
                continue
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    yy, xx = y + dy, x + dx
                    if 0 <= yy < 16 and 0 <= xx < 16 and px[yy][xx] == 1:
                        px[y][x] = 2
    if style == "banner":
        for y in range(15, 0, -1):
            for x in range(15, 0, -1):
                if not px[y][x] and px[y - 1][x - 1] == 2:
                    px[y][x] = 3
    col = {"free": [0, 1, 2, 0], "banner": [3, 6, 2, 5]}[style]
    cv = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            v = col[px[y][x]]
            if v:
                cv.set(x, y, UI_BG[v])
    return cv


def font_atlas(path=None, chars=None):
    """The house font atlas: the 96 glyphs in the four styles (rows: small, box, free, banner)."""
    chars = chars or [chr(c) for c in range(32, 128) if chr(c) in FONT]
    cv = Canvas(16 * 32, 16 * 4 * ((len(chars) + 31) // 32))
    for i, ch in enumerate(chars):
        bx, by = (i % 32) * 16, (i // 32) * 64
        for k, st in enumerate(("small", "box", "free", "banner")):
            cv.paste(font_glyph(ch, st), bx, by + k * 16)
    if path:
        cv.image().save(path)
    return cv


def panel(wt, ht):
    """A wt x ht-tile panel as house_ui.c draws it (rounded corners, dark rim, light inner rim on the top and
    left, a highlight line on the top row)."""
    cv = Canvas(wt * 8, ht * 8)
    for ty in range(ht):
        for tx in range(wt):
            kx = 0 if tx == 0 else 2 if tx == wt - 1 else 1
            ky = 0 if ty == 0 else 2 if ty == ht - 1 else 1
            for y in range(8):
                for x in range(8):
                    ex = x if kx == 0 else 7 - x if kx == 2 else 8
                    ey = y if ky == 0 else 7 - y if ky == 2 else 8
                    v = 3
                    if kx != 1 and ky != 1 and ex + ey < 3:
                        v = 0
                    elif ex == 0 or ey == 0 or (kx != 1 and ky != 1 and ex + ey == 3):
                        v = 5
                    elif ex == 1 or ey == 1 or (kx != 1 and ky != 1 and ex + ey == 4):
                        v = 5 if (ky == 2 or kx == 2) else 4
                    elif ey == 2 and ky == 0:
                        v = 7
                    if v:
                        cv.set(tx * 8 + x, ty * 8 + y, UI_BG[v])
    return cv


def text_canvas(s, style="small"):
    step = 8 if style in ("small", "box") else 16
    cv = Canvas(max(1, len(s)) * step, step)
    for i, ch in enumerate(s):
        cv.paste(font_glyph(ch, style), i * step, 0)
    return cv


# ---- reference sheets ----------------------------------------------------------------------------------------
def palette_sheet():
    """The master palette and the accent ramps as swatches (16x16 each, one ramp per row)."""
    rows = list(HOUSE.items()) + list(ACCENTS.items())
    cv = Canvas(16 * 6 + 4, len(rows) * 18)
    for r, (name, cols) in enumerate(rows):
        for i, c in enumerate(cols):
            cv.rect(i * 18, r * 18, i * 18 + 15, r * 18 + 15, c)
    return cv, [n for n, _ in rows]


def kit_sheet():
    """The UI sprites: digits, button glyphs, medals, sparkles, the D-pad glyph."""
    cv = Canvas(16 * 10, 16 + 16 + 24 + 8 + 16)
    for n in range(10):
        cv.paste(digit(n), n * 16, 0)
    for i, lab in enumerate("ABXY"):
        cv.paste(button_glyph(lab, 0), i * 36, 18)
        cv.paste(button_glyph(lab, 1), i * 36 + 17, 18)
    for k in range(4):
        cv.paste(medal(k), k * 28, 36)
    cv.paste(sparkle(0), 116, 44), cv.paste(sparkle(1), 128, 44)
    cv.paste(dpad_glyph(0), 0, 62), cv.paste(dpad_glyph(1), 17, 62)
    return cv


def players_sheet():
    """The four players: a pink blob hero (the template's) in the P1..P4 palettes of the house rule."""
    body = ACCENTS["pink"]
    base = Canvas(24, 24)
    ellipse(base, 12.0, 13.5, 8.5, 8.0, body)
    eye(base, 11, 10, big=True), eye(base, 15, 10, big=True)
    base.outline((40, 16, 58))
    cols = sorted({c for row in base.p for c in row if c is not None})
    pals = player_palettes(cols, 0.80, 1.00, -0.45)
    cv = Canvas(4 * 28, 24)
    for p, pal in enumerate(pals):
        m = dict(zip(cols, pal))
        for y in range(24):
            for x in range(24):
                c = base.get(x, y)
                if c is not None:
                    cv.set(p * 28 + x, y, m[c])
    return cv


def shapes_sheet():
    """The drawing helpers: a shaded ellipse and capsule, squash and stretch, the dithers, a panel with text."""
    cv = Canvas(208, 80)
    body = ACCENTS["pink"]
    for i, k in enumerate((1.0, 1.25, 0.8)):
        c = Canvas(32, 32)
        ellipse(c, 16.0, 18.5, 9.5, 9.0, body)
        eye(c, 15, 15, big=True), eye(c, 20, 15, big=True)
        c = squash(c, k) if k != 1.0 else c
        c.outline((40, 16, 58))
        cv.paste(c, i * 34, 0)
    c = Canvas(40, 32)
    capsule(c, 8, 22, 32, 10, 5.5, HOUSE["wood"])
    c.outline((34, 20, 12))
    cv.paste(c, 104, 0)
    for lvl in range(0, 17, 2):                         # the ordered dither, 0..16
        for y in range(8):
            for x in range(8):
                if dither_ok(x, y, lvl):
                    cv.set(146 + (lvl // 2) * 7 + x % 6, 2 + y, HOUSE["cream"][0])
    for y in range(8):
        for x in range(24):
            if checker(x, y):
                cv.set(146 + x, 14 + y, HOUSE["water"][1])
    p = panel(16, 5)
    p.paste(text_canvas("SCORE", "box"), 16, 16)
    cv.paste(p, 0, 36)
    cv.paste(text_canvas("OK", "banner"), 136, 40)
    return cv


def main(argv=None):
    ap = argparse.ArgumentParser(description="write the house style reference sheets")
    ap.add_argument("--out", default=os.path.join(ROOT, "docs", "art-direction"))
    ap.add_argument("--scale", type=int, default=3)
    a = ap.parse_args(argv)
    os.makedirs(a.out, exist_ok=True)
    k = a.scale
    pal, _ = palette_sheet()
    lg, _info = logo(["HOUSE", "STYLE"], [ACCENTS["duck"][:3], ACCENTS["pink"][:3]])
    lcv = Canvas(lg.width, lg.height)
    lpx = lg.load()
    for y in range(lg.height):
        for x in range(lg.width):
            if lpx[x, y] != MAGENTA:
                lcv.set(x, y, lpx[x, y])
    out = [("palette.png", pal), ("font.png", font_atlas()), ("ui-sprites.png", kit_sheet()),
           ("shapes.png", shapes_sheet()), ("logo.png", lcv), ("players.png", players_sheet())]
    for name, cv in out:
        im = cv.image()
        im.resize((im.width * k, im.height * k), Image.NEAREST).save(os.path.join(a.out, name))
        print("wrote", os.path.join(a.out, name))


if __name__ == "__main__":
    main()
