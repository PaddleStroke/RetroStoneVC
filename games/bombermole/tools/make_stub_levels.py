#!/usr/bin/env python3
"""Write the missing Bomber Mole levels as small playable STUBS (status: stub).

Existing files are never overwritten: replace a stub by editing its file.
Each stub shows its season's twist so the arc stays playable end to end.

All rights reserved, 8BCraft.
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
LEVELS = os.path.join(os.path.dirname(HERE), "levels")
SEASONS = ["spring", "summer", "autumn", "winter"]
FEATURE = {"spring": "p", "summer": "w", "autumn": "l", "winter": "f"}
PLANNED = {  # design notes for each stub (DESIGN.md, "Level gimmicks")
    "summer": ["corn maze and beehives", "harvester sweeping a row", "badger mini-boss (underground)",
               "steam-vent lifts through three depths", "mine carts and levers", "harvest night (dark)",
               "the farmer (boss)"],
    "autumn": ["pumpkins to push (Sokoban)", "apple trees", "bouncy mushrooms", "fog", "leaf storms",
               "drain pipes", "the fox (boss)"],
    "winter": ["thin-ice lake", "snowdrifts and snowballs", "icicles in the tunnels", "night (helmet lamp)",
               "frozen well bucket", "ice caves and glow-worms", "the snowy owl (boss)"],
}


# Stubs are GENERATED, each from its own random seed (so no two depths look alike: check_levels.py rejects
# a depth sharing more than 60% of its open cells with another level's), and kept only when they pass
# check_levels.py (solvable, no softlock, the design rules). The surface is a lawn broken up by rock beds,
# dirt hedges and the season's feature; two holes lead down to a carved tunnel network on depth 1 (a ladder
# back up beside each), which drops to a den on depth 2 (a ladder back). Every depth has enemies, a grub in
# the open, a grub hidden in a block (only its glow shows it) and something to find.
import random
import sys
import tempfile

sys.path.insert(0, HERE)
import check_levels  # noqa: E402

W_, H_ = 18, 12
FEATURE_CHAR = {"spring": "t", "summer": "w", "autumn": "l", "winter": "f"}


def _grid(ch):
    return [[ch] * W_ for _ in range(H_)]


def _free(g, x, y):
    return 0 <= x < W_ and 0 <= y < H_ and g[y][x] == "."


def _surface(rng, season):
    g = _grid(".")
    feat = FEATURE_CHAR[season]
    corner = rng.randrange(4)
    sx, sy = (0 if corner % 2 == 0 else W_ - 1), (0 if corner < 2 else H_ - 1)
    keep = {(sx + dx, sy + dy) for dx in range(-2, 3) for dy in range(-2, 3)}
    ex, ey = W_ - 1 - sx + (2 if sx else -2), H_ - 1 - sy + (1 if sy else -1)
    keep |= {(ex + dx, ey + dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1)}
    ox, oy = rng.randrange(2), rng.randrange(2)
    for y in range(H_):
        for x in range(W_):
            if (x, y) in keep or (x + ox) % 2 or (y + oy) % 2 or rng.random() > 0.8:
                continue
            g[y][x] = rng.choice("rrrddd" + feat)
    for _ in range(rng.randint(4, 7)):                   # hedges and rock beds
        x, y = rng.randrange(W_), rng.randrange(H_)
        horiz = rng.random() < 0.5
        ch = rng.choice(["d", "d", "r", feat])
        for i in range(rng.randint(2, 4)):
            cx, cy = (x + i, y) if horiz else (x, y + i)
            if _free(g, cx, cy) and (cx, cy) not in keep:
                g[cy][cx] = ch
    g[sy][sx] = "M"
    g[ey][ex] = "E"
    return g, (sx, sy), keep


def _place(rng, g, ch, n, far_from=None, dist=0, cells=None):
    spots = [(x, y) for y in range(H_) for x in range(W_) if g[y][x] == "." and
             (far_from is None or abs(x - far_from[0]) + abs(y - far_from[1]) >= dist) and
             (cells is None or (x, y) in cells)]
    rng.shuffle(spots)
    out = []
    for x, y in spots:
        if len(out) >= n:
            break
        if all(abs(x - a) + abs(y - b) >= 3 for a, b in out):
            g[y][x] = ch
            out.append((x, y))
    return out


def _carve(g, a, b, rng):
    """An L-shaped tunnel with a jog from a to b."""
    (x0, y0), (x1, y1) = a, b
    mid = rng.randint(min(x0, x1), max(x0, x1))
    for x in range(min(x0, mid), max(x0, mid) + 1):
        g[y0][x] = "." if g[y0][x] == "d" else g[y0][x]
    for y in range(min(y0, y1), max(y0, y1) + 1):
        g[y][mid] = "." if g[y][mid] == "d" else g[y][mid]
    for x in range(min(mid, x1), max(mid, x1) + 1):
        g[y1][x] = "." if g[y1][x] == "d" else g[y1][x]


def _room(g, rng, cx, cy, w, h):
    cells = []
    for y in range(max(0, cy - h // 2), min(H_, cy - h // 2 + h)):
        for x in range(max(0, cx - w // 2), min(W_, cx - w // 2 + w)):
            if g[y][x] == "d":
                g[y][x] = "."
            cells.append((x, y))
    return cells


def _level(season, arc, n, seed):
    rng = random.Random(seed)
    surf, start, keep = _surface(rng, season)
    # two links between the surface and depth 1 (a hole down, a ladder up on the same cell below)
    links = _place(rng, surf, "v", 2, far_from=start, dist=5)
    u1 = _grid("d")
    for x, y in links:
        u1[y][x] = "H"
    rooms = []
    for (x, y) in links:
        rooms += _room(u1, rng, x, y, rng.randint(3, 5), rng.randint(3, 4))
    for _ in range(rng.randint(1, 2)):
        rx, ry = rng.randrange(2, W_ - 2), rng.randrange(2, H_ - 2)
        rooms += _room(u1, rng, rx, ry, rng.randint(3, 5), rng.randint(3, 4))
        _carve(u1, links[0], (rx, ry), rng)
    _carve(u1, links[0], links[1], rng)
    for x, y in links:
        u1[y][x] = "H"
    for (x, y) in rooms:                                  # rock and roots to break up the rooms
        if u1[y][x] == "." and rng.random() < 0.12:
            u1[y][x] = rng.choice("rt")
    down = _place(rng, u1, "v", 1, cells=set(rooms))
    u2 = _grid("d")
    for x, y in down:
        den = _room(u2, rng, x + rng.choice((-1, 0, 1)), y + rng.choice((-1, 0, 1)), rng.randint(4, 6), rng.randint(3, 4))
        u2[y][x] = "H"
        tail = (rng.randrange(1, W_ - 1), rng.randrange(1, H_ - 1))
        _carve(u2, (x, y), tail, rng)
        u2[y][x] = "H"
        edge = [(a, b) for a, b in den if u2[b][a] == "."]
        if edge:
            qa, qb = rng.choice(edge)
            u2[qb][qa] = "Q"
    # the inhabitants and the grubs
    _place(rng, surf, "C", 3, far_from=start, dist=7)
    _place(rng, surf, "g", 2, far_from=start, dist=4)
    dirt = [(x, y) for y in range(H_) for x in range(W_) if surf[y][x] == "d"]
    if dirt:
        x, y = rng.choice(dirt)
        surf[y][x] = "G"
    _place(rng, surf, rng.choice("bxs"), 1)
    _place(rng, u1, "F", 2, cells=set(rooms))
    _place(rng, u1, "g", 2, cells=set(rooms))
    near = [(x, y) for y in range(H_) for x in range(W_) if u1[y][x] == "d" and
            any(0 <= x + dx < W_ and 0 <= y + dy < H_ and u1[y + dy][x + dx] == "." for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))]
    if near:
        x, y = rng.choice(near)
        u1[y][x] = "G"
    _place(rng, u2, "z", 1)
    _place(rng, u2, "g", 1)
    note = PLANNED.get(season, [""] * 7)[n - 2]
    body = ["# STUB: a generated level (tools/make_stub_levels.py, seed %d). Replace it with a real design." % seed,
            "# Planned: %s" % note,
            "name: %s %d (stub)" % (season.capitalize(), n), "season: %s" % season, "arc: %d" % arc,
            "level: %d" % n, "status: stub", "hint: A generated level: three depths, a grub hidden on each.", ""]
    for name, grid in (("surface", surf), ("under1", u1), ("under2", u2)):
        body += [name + ":", "#" * 20] + ["#" + "".join(r) + "#" for r in grid] + ["#" * 20]
    return "\n".join(body) + "\n"


_OPEN = {}


def _others(season, n):
    """The open cells of every other level's depths (the new stub must not look like any of them)."""
    me = "%s-%d.txt" % (season, n)
    out = []
    for f in sorted(os.listdir(LEVELS)):
        if not f.endswith(".txt") or f == me:
            continue
        p = os.path.join(LEVELS, f)
        key = (p, os.path.getmtime(p))
        if key not in _OPEN:
            try:
                _OPEN[key] = list(check_levels.check(p)[0].get("_open", {}).values())
            except Exception:   # noqa: BLE001
                _OPEN[key] = []
        out += _OPEN[key]
    return out


def stub(season, arc, n):
    """The first seed whose level passes check_levels.py."""
    base = (SEASONS.index(season) * 1000 + n * 37) * 7919
    tmp = os.path.join(tempfile.mkdtemp(), "%s-%d.txt" % (season, n))
    for k in range(400):
        text = _level(season, arc, n, base + k)
        with open(tmp, "w", newline="\n") as f:
            f.write(text)
        try:
            mine = check_levels.check(tmp)[0].get("_open", {})
        except (check_levels.LevelError, Exception):   # noqa: BLE001 - try the next seed
            continue
        if not any(check_levels.layout_similarity(A, B) > check_levels.SIMILAR_MAX
                   for B in _others(season, n) for A in mine.values() if len(A) >= 8 and len(B) >= 8):
            return text
    raise SystemExit("no valid stub found for %s-%d" % (season, n))


def main():
    import sys
    rewrite = "--rewrite-stubs" in sys.argv   # regenerate the files written by this script
    made = 0
    for a, season in enumerate(SEASONS):
        for n in range(1, 9):
            p = os.path.join(LEVELS, "%s-%d.txt" % (season, n))
            if os.path.exists(p) and not (rewrite and open(p).readline().startswith("# STUB:")):
                continue
            with open(p, "w", newline="\n") as fh:
                fh.write(stub(season, a + 1, n))
            made += 1
    print("wrote %d stub levels" % made)


if __name__ == "__main__":
    main()
