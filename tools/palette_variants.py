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

The moles of players 1-4 are told apart by their miner's helmet only (red,
blue, green, yellow): helmet_palette() finds the helmet's red ramp in the
mole's palette and moves only those entries to the player's colour, keeping
their shading (hue-shifted: cooler shadows, warmer lights). The fur, the pink
nose, the claws and the lamp keep the colours of the art.

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import colorsys

# name, base palette group, fur target (hue in degrees, saturation, darkest value, lightest value),
# light tint (None = keep, else (hue, saturation, value scale))
VARIANTS = [
    ("sleepy_ferret", "ferret", (45, 0.18, 0.50, 1.00), None),
    ("brown_ferret", "ferret", (28, 0.55, 0.22, 0.82), None),
    ("polecat", "ferret", (25, 0.35, 0.07, 0.45), (40, 0.25, 0.95)),
    ("stoat", "ferret", (214, 0.20, 0.28, 1.00), None),     # white coat, grey-blue shading, a dark face mask
    ("ginger_cat", "cat", (28, 0.78, 0.40, 1.00), None),
    ("grey_cat", "cat", (220, 0.08, 0.28, 0.85), None),
    ("black_cat", "cat", (245, 0.18, 0.06, 0.33), None),
    ("siamese_cat", "cat", (35, 0.22, 0.30, 0.97), (35, 0.12, 1.0)),
]


# outline recolour per variant (hue, saturation, value): a white coat needs a darker, cooler outline on snow
OUTLINES = {"stoat": (222, 0.45, 0.07)}

# the players' helmets: None = the art's own red, else the hue of the darkest and of the lightest shade,
# a saturation scale and a lift of the values towards white (yellow must stay bright to read as yellow)
HELMETS = [("RED", None), ("BLUE", (232, 208, 0.95, 0.08)), ("GREEN", (136, 100, 0.90, 0.10)),
           ("YELLOW", (34, 54, 1.00, 0.40))]


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


def variant_palette(palette, counts, target, light_tint=None, outline=None):
    """The base palette recoloured: fur mapped onto the target ramp by brightness."""
    r = ramps(palette, counts)
    out = list(palette)
    info = [_hsv(c) for c in palette]
    if outline:
        hue, sat, val = outline
        for i in r["outline"]:
            v = min(info[i][2], val)
            rgb = colorsys.hsv_to_rgb(hue / 360.0, sat, v)
            out[i] = to555(tuple(c * 255 for c in rgb))
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


def _is_red(c):
    """A true, saturated red (hue within 10 degrees of red), not the outline: the pink nose (hue about 345, or
    pale) and the brown fur (hue 20-30) stay out."""
    h, s, v = _hsv(c)
    return _hue_dist(h, 0) <= 10 and s >= 0.5 and v >= 0.18


def _nudge(c, used):
    """The same colour one 555 step apart (a separate palette entry that looks the same)."""
    for bit in (1, 32, 1024, 2, 64, 2048):
        if c ^ bit not in used:
            return c ^ bit
    return c


def _split_helmet(frames):
    reds = {v for f in frames for row in f for v in row if v is not None and _is_red(v)}
    core = {c for c in reds if _hsv(c)[2] >= 0.45}          # the helmet's mid and light reds
    masks = []
    for f in frames:                                         # the helmet: reds 4-connected to its core
        h, w = len(f), len(f[0])
        m = [[False] * w for _ in range(h)]
        stack = [(x, y) for y in range(h) for x in range(w) if f[y][x] in core]
        for x, y in stack:
            m[y][x] = True
        while stack:
            x, y = stack.pop()
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                X, Y = x + dx, y + dy
                if 0 <= X < w and 0 <= Y < h and not m[Y][X] and f[Y][X] in reds:
                    m[Y][X] = True
                    stack.append((X, Y))
        masks.append(m)
    inside, outside = set(), set()
    for f, m in zip(frames, masks):
        for y, row in enumerate(f):
            for x, v in enumerate(row):
                if v in reds:
                    (inside if m[y][x] else outside).add(v)
    used = {v for f in frames for row in f for v in row if v is not None}
    remap = {}
    for c in sorted(inside & outside):                       # drawn on the helmet and elsewhere: two entries
        remap[c] = _nudge(c, used)
        used.add(remap[c])
    out = [[[remap[v] if m[y][x] and v in remap else v for x, v in enumerate(row)] for y, row in enumerate(f)]
           for f, m in zip(frames, masks)]
    return out, {remap.get(c, c) for c in inside}


def helmet_frames(frames, quantize, limit=15):
    """The mole's frames (rows of RGB555 or None) with the helmet's colours its own. The helmet is the reds
    connected to its mid and light reds; a red also drawn elsewhere (a dark red shading the face, say) becomes
    two palette entries, so recolouring the helmet never touches the fur, nose, claws or lamp. The colours are
    reduced (quantize(frames, n)) to leave room for the split. Returns (frames, set of the helmet's colours)."""
    for n in range(limit, 1, -1):
        out, helmet = _split_helmet(quantize(frames, n))
        if len({v for f in out for row in f for v in row if v is not None}) <= limit:
            return out, helmet
    return frames, set()


def helmet_ramp(palette, helmet=None):
    """Indices of the helmet's colours in a mole palette: the set found by helmet_frames(), or (no set) every
    true red."""
    if helmet is not None:
        return [i for i, c in enumerate(palette) if c in helmet]
    return [i for i, c in enumerate(palette) if _is_red(c)]


def helmet_palette(palette, helmet, ramp=None):
    """The mole palette with the helmet ramp (indices; default: helmet_ramp()) moved to one player's colour
    (HELMETS[k][1])."""
    out = list(palette)
    if helmet is None:
        return out
    h0, h1, sscale, lift = helmet
    if ramp is None:
        ramp = helmet_ramp(palette)
    vs = [_hsv(palette[i])[2] for i in ramp]
    lo, hi = (min(vs), max(vs)) if vs else (0, 1)
    for i in ramp:
        _, s, v = _hsv(palette[i])
        t = 0.5 if hi - lo < 1e-6 else (v - lo) / (hi - lo)
        hue = h0 + (h1 - h0) * t                       # hue-shifted: the shadows cooler, the lights warmer
        v2 = min(1.0, v + lift * (1.0 - v))
        rgb = colorsys.hsv_to_rgb((hue % 360) / 360.0, min(1.0, s * sscale), v2)
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
