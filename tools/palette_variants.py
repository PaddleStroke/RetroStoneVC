"""Palette-swap enemy variants: one sprite, several 16-colour palettes.

The base palette of a sprite (ferret or cat) is split into ramps:
  - outline: very dark colours (kept);
  - fur: the dominant colour family by pixel count (if no saturated family
    covers enough of the sprite, the mid-value greys are the fur);
  - light: light neutral colours not in the fur ramp (muzzle, belly: kept or tinted);
  - accents: other saturated colours (nose, eyes: kept).
A variant maps the fur ramp onto its own ramp by relative brightness, so the
shading of the art survives. It works the same on the placeholders and on
imported AI art. Used by the game's asset build and the variant preview.

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import colorsys

# name, base palette group, fur target (hue in degrees, saturation, darkest value, lightest value),
# light tint (None = keep, else (hue, saturation, value scale))
VARIANTS = [
    ("sleepy_ferret", "ferret", (45, 0.18, 0.50, 1.00), None),
    ("brown_ferret", "ferret", (28, 0.55, 0.22, 0.82), None),
    ("polecat", "ferret", (25, 0.35, 0.07, 0.45), (40, 0.25, 0.95)),
    ("stoat", "ferret", (210, 0.05, 0.60, 1.00), None),
    ("ginger_cat", "cat", (28, 0.78, 0.40, 1.00), None),
    ("grey_cat", "cat", (220, 0.08, 0.28, 0.85), None),
    ("black_cat", "cat", (245, 0.18, 0.06, 0.33), None),
    ("siamese_cat", "cat", (35, 0.22, 0.30, 0.97), (35, 0.12, 1.0)),
]


def to_rgb(c555):
    r, g, b = c555 & 31, (c555 >> 5) & 31, (c555 >> 10) & 31
    return (r * 255 // 31, g * 255 // 31, b * 255 // 31)


def to555(rgb):
    r, g, b = (max(0, min(255, int(round(v)))) for v in rgb)
    return ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3)


def _hsv(c555):
    r, g, b = (v / 255.0 for v in to_rgb(c555))
    h, s, v = colorsys.rgb_to_hsv(r, g, b)
    return h * 360.0, s, v


def _hue_dist(a, b):
    d = abs(a - b) % 360
    return min(d, 360 - d)


def ramps(palette, counts):
    """palette: list of up to 15 RGB555 colours; counts: pixels per colour.
    Returns {"outline": [...], "fur": [...], "light": [...], "accent": [...]} (indices)."""
    info = [_hsv(c) for c in palette]
    total = float(sum(counts)) or 1.0
    outline = [i for i, (h, s, v) in enumerate(info) if v < 0.18]
    rest = [i for i in range(len(palette)) if i not in outline]
    sat = [i for i in rest if info[i][1] >= 0.12]
    fur = []
    best, best_share = None, 0.0
    for i in sat:                       # the dominant saturated hue family
        fam = [j for j in sat if _hue_dist(info[i][0], info[j][0]) <= 28 and abs(info[i][1] - info[j][1]) < 0.4]
        share = sum(counts[j] for j in fam) / total
        if share > best_share:
            best, best_share = fam, share
    if best and best_share >= 0.25:
        fur = best
        light = [i for i in rest if i not in fur and info[i][1] < 0.12 and info[i][2] >= 0.72]
        # richer (AI) art: the cream belly and muzzle share the fur's hue; bright, pale colours
        # that are a minority of the coat are the light ramp (a mostly cream coat stays fur)
        pale = [i for i in fur if info[i][2] >= 0.85 and info[i][1] <= 0.40]
        if pale and len(pale) < len(fur) and \
                sum(counts[i] for i in pale) < 0.45 * sum(counts[i] for i in fur):
            fur = [i for i in fur if i not in pale]
            light += pale
    else:                                # a grey sprite: the mid greys are the fur
        fur = [i for i in rest if info[i][1] < 0.12 and info[i][2] < 0.80]
        light = [i for i in rest if info[i][1] < 0.12 and info[i][2] >= 0.80]
    accent = [i for i in rest if i not in fur and i not in light]
    return {"outline": outline, "fur": fur, "light": light, "accent": accent}


def variant_palette(palette, counts, target, light_tint=None):
    """The base palette recoloured: fur mapped onto the target ramp by brightness."""
    r = ramps(palette, counts)
    out = list(palette)
    info = [_hsv(c) for c in palette]
    if r["fur"]:
        vs = [info[i][2] for i in r["fur"]]
        lo, hi = min(vs), max(vs)
        hue, sat, v0, v1 = target
        for i in r["fur"]:
            t = 0.5 if hi - lo < 1e-6 else (info[i][2] - lo) / (hi - lo)
            v = v0 + (v1 - v0) * t
            s = sat * (0.75 + 0.5 * (1 - t))          # slightly richer in the shadows
            rgb = colorsys.hsv_to_rgb((hue % 360) / 360.0, min(1.0, s), min(1.0, v))
            out[i] = to555(tuple(c * 255 for c in rgb))
    if light_tint:
        hue, sat, vs = light_tint
        for i in r["light"]:
            v = min(1.0, info[i][2] * vs)
            rgb = colorsys.hsv_to_rgb(hue / 360.0, sat, v)
            out[i] = to555(tuple(c * 255 for c in rgb))
    return out


def counts_for(palette, cells):
    """Pixels per palette colour in a list of cells (rows of RGB555 or None)."""
    idx = {c: i for i, c in enumerate(palette)}
    counts = [0] * len(palette)
    for cell in cells:
        for row in cell:
            for v in row:
                if v is not None and v in idx:
                    counts[idx[v]] += 1
    return counts
