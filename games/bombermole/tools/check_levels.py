#!/usr/bin/env python3
"""Check every Bomber Mole level file (format: games/bombermole/DESIGN.md).

Same rules as the game's loader (grid sizes, legend, one mole, one exit on
the surface, holes that line up, grubs) plus a solvability check: every grub
and the exit must be reachable from the mole when breakable cells count as
passable (dig/bomb), following holes, ladders, thin floors, pipes and vents.

    check_levels.py [files...]      (default: all of games/bombermole/levels)

All rights reserved, 8BCraft.
"""
import glob
import os
import sys

GW, GH = 20, 14
HERE = os.path.dirname(os.path.abspath(__file__))
LEVELS = os.path.join(os.path.dirname(HERE), "levels")

DEFAULT = {
    '.': "floor", '#': "stone", 'd': "soft_dirt", 'r': "hard_rock", 't': "roots", 'f': "frozen_dirt",
    'l': "leaves", '~': "water", 'p': "puddle", '_': "thin_floor", 'v': "hole_down", '^': "hole_up",
    'H': "ladder", 'E': "exit", 'M': "floor+mole", '2': "floor+p2", '3': "floor+p3", '4': "floor+p4",
    'F': "floor+ferret", 'C': "floor+cat", 'K': "floor+boss", 'z': "floor+ferret+asleep",
    'D': "floor+dog+asleep", 'g': "floor+grub", 'G': "soft_dirt+grub", 'L': "leaves+grub",
    'Q': "hard_rock+grub", 'b': "floor+bomb", 'B': "soft_dirt+bomb", 'x': "floor+fire",
    'X': "soft_dirt+fire", 's': "floor+speed", 'S': "soft_dirt+speed", 'o': "floor+remote",
    'O': "soft_dirt+remote", 'h': "floor+heart", 'Y': "soft_dirt+heart", '=': "bridge", '&': "water+log",
    'i': "ice", 'j': "thin_ice", 'm': "mud", 'w': "tall_grass", 'P': "plate+chan:1", '|': "gate+chan:1",
    '/': "lever+chan:1", 'V': "steam_vent", '@': "pipe+chan:1", 'c': "crate", 'k': "sprinkler",
    'W': "windmill", '<': "floor+push_left", '>': "floor+push_right", 'n': "floor+push_up",
    'u': "floor+push_down", '{': "water+flow_left", '}': "water+flow_right",
}
TERRAIN = {"floor", "stone", "soft_dirt", "dirt", "hard_rock", "rock", "roots", "frozen_dirt", "leaves", "water",
           "puddle", "thin_floor", "hole_down", "hole_up", "ladder", "exit", "bridge", "ice", "thin_ice", "mud",
           "tall_grass", "corn", "cover", "burnt", "gate", "plate", "lever", "steam_vent", "vent", "pipe", "crate",
           "sprinkler", "windmill"}
ITEMS = {"grub", "bomb", "fire", "speed", "remote", "heart"}
ACTORS = {"mole", "p2", "p3", "p4", "ferret", "cat", "boss", "dog"}
SOLID_FOREVER = {"stone", "windmill", "sprinkler", "lever"}
SEASONS = ["spring", "summer", "autumn", "winter"]


# enemy types (games/bombermole/src/world.c ENEMY_TYPES): name -> (kind, tier)
ENEMY_TYPES = {"sleepy_ferret": ("ferret", 1), "brown_ferret": ("ferret", 2), "polecat": ("ferret", 3),
               "stoat": ("ferret", 4), "ginger_cat": ("cat", 1), "grey_cat": ("cat", 2), "black_cat": ("cat", 3),
               "siamese_cat": ("cat", 4)}
TYPE_OF = {(k, t): n for n, (k, t) in ENEMY_TYPES.items()}


def default_tier(head):
    """The difficulty curve: spring 1-4 tier 1, spring 5-8 and summer 2, autumn 3, winter 4."""
    if head.get("tier"):
        return int(head["tier"])
    s = head["season"]
    if s == "spring":
        return 2 if int(head.get("level", 1)) >= 5 else 1
    return {"summer": 2, "autumn": 3, "winter": 4}[s]


def max_tier(head):
    s = head["season"]
    if s == "spring":
        return 2 if int(head.get("level", 1)) >= 5 else 1
    return {"summer": 2, "autumn": 3, "winter": 4}[s]


class LevelError(Exception):
    pass


# BG palette budget of the terrain-like props (DESIGN.md, "BG palettes"): each prop has a colour
# family (tools/sheets.py PROP_PALETTES); a level may use two families (BG palettes 5 and 7).
PROP_SLOTS = 2


def prop_families(head, cells, has_boss):
    """Colour families of the props a level uses, or can create (same rules as draw.c)."""
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(HERE))), "tools"))
    import sheets
    names = set()
    for d in range(3):
        for row in cells[d]:
            for c in row:
                t = c["t"]
                if t in ("bridge", "ice", "thin_ice", "mud", "burnt", "gate", "plate", "lever", "steam_vent",
                         "pipe", "crate"):
                    names.add(t)
                elif t == "puddle":                 # puddles make mud on the depth below
                    names.add("mud")
                elif t == "tall_grass":             # corn in summer; cover burns
                    names.update(("corn" if head["season"] == "summer" else "tall_grass", "burnt"))
    boss = head.get("boss") or {"spring": "barncat", "summer": "farmer", "autumn": "fox", "winter": "owl"}[head["season"]]
    if has_boss and boss == "farmer":
        names.add("splat")                      # the farmer's tomatoes splat on the floor
    return {sheets.PROP_PALETTES[sheets.prop_palette(n)][0] for n in names}


def parse_spec(spec):
    out = {"t": "floor", "item": None, "actor": None, "chan": 0, "log": False, "push": None, "etype": None,
           "timed": 0, "blow": None, "asleep": False}
    for tok in spec.replace("+", " ").split():
        if tok in TERRAIN:
            out["t"] = {"dirt": "soft_dirt", "rock": "hard_rock", "vent": "steam_vent", "corn": "tall_grass",
                        "cover": "tall_grass"}.get(tok, tok)
        elif tok in ITEMS:
            out["item"] = tok
        elif tok in ACTORS:
            out["actor"] = tok
        elif tok in ENEMY_TYPES:
            out["actor"], out["etype"] = ENEMY_TYPES[tok][0], tok
        elif tok.startswith("ferret:") or tok.startswith("cat:"):
            k, t = tok.split(":")
            out["actor"], out["etype"] = k, TYPE_OF[(k, max(1, min(4, int(t))))]
        elif tok == "asleep":
            out["asleep"] = True
        elif tok[:5] == "blow_" and tok[5:] in ("up", "down", "left", "right"):
            out["blow"] = tok[5:]               # a windmill's wind lane (points away from the windmill)
        elif tok == "log":
            out["log"] = True
        elif tok.startswith("chan:"):
            out["chan"] = int(tok[5:])
        elif tok.startswith("timed:"):
            out["timed"] = int(tok[6:])
            pass
        elif tok[:5] in ("push_", "flow_") and tok[5:] in ("up", "down", "left", "right"):
            out["push"] = tok
        else:
            raise LevelError("unknown legend word %r" % tok)
    if out["t"] in ("gate", "plate", "lever", "pipe") and not out["chan"]:
        out["chan"] = 1
    return out


def parse(path):
    legend = {c: parse_spec(s) for c, s in DEFAULT.items()}
    head, grids, section, in_legend = {}, [[], [], []], None, False
    for n, raw in enumerate(open(path, encoding="utf-8").read().split("\n"), 1):
        line = raw.rstrip("\r").rstrip()
        if section is not None and len(grids[section]) < GH:
            if not line:
                continue
            if len(line) != GW:
                raise LevelError("%s:%d: grid line must be %d chars (got %d)" % (path, n, GW, len(line)))
            for c in line:
                if c not in legend:
                    raise LevelError("%s:%d: %r is not in the legend" % (path, n, c))
            grids[section].append(line)
            continue
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        if s in ("surface:", "under1:", "under2:"):
            section = {"surface:": 0, "under1:": 1, "under2:": 2}[s]
            in_legend = False
            continue
        if s == "legend:":
            in_legend = True
            continue
        if in_legend:
            if "=" not in s:
                raise LevelError("%s:%d: legend lines are 'C = spec'" % (path, n))
            legend[s[0]] = parse_spec(s.split("=", 1)[1])
            continue
        if ":" not in s:
            raise LevelError("%s:%d: expected 'key: value'" % (path, n))
        k, v = s.split(":", 1)
        head[k.strip()] = v.split("  #")[0].strip()
    if head.get("season") not in SEASONS:
        raise LevelError("%s: missing or bad season" % path)
    for d in range(3):
        if len(grids[d]) != GH:
            raise LevelError("%s: depth %d has %d rows (need %d)" % (path, d, len(grids[d]), GH))
    cells = [[[legend[c] for c in row] for row in grids[d]] for d in range(3)]
    return head, cells


def check(path):
    head, cells = parse(path)
    T = lambda d, x, y: cells[d][y][x]["t"]  # noqa: E731
    moles = [(d, x, y) for d in range(3) for y in range(GH) for x in range(GW) if cells[d][y][x]["actor"] == "mole"]
    if len(moles) != 1:
        raise LevelError("%s: needs exactly one mole start, found %d" % (path, len(moles)))
    exits = [(d, x, y) for d in range(3) for y in range(GH) for x in range(GW) if T(d, x, y) == "exit"]
    if len(exits) != 1 or exits[0][0] != 0:
        raise LevelError("%s: needs exactly one exit, on the surface" % path)
    grubs = [(d, x, y) for d in range(3) for y in range(GH) for x in range(GW) if cells[d][y][x]["item"] == "grub"]
    boss = [(d, x, y) for d in range(3) for y in range(GH) for x in range(GW) if cells[d][y][x]["actor"] == "boss"]
    # enemies: the tier curve and the sprite palette budget (one palette per variant)
    dt = default_tier(head)
    enemies = []
    for d in range(3):
        for y in range(GH):
            for x in range(GW):
                c = cells[d][y][x]
                if c["actor"] in ("ferret", "cat"):
                    enemies.append(c["etype"] or TYPE_OF[(c["actor"], dt)])
    mt = max_tier(head)
    for e in enemies:
        if ENEMY_TYPES[e][1] > mt:
            raise LevelError("%s: %s is tier %d; this level allows tier %d at most" % (path, e, ENEMY_TYPES[e][1], mt))
    if head.get("season") == "spring" and int(head.get("level", 0)) == 1 and len(enemies) < 2:
        raise LevelError("%s: spring 1 must teach bombing enemies (2 or more)" % path)
    has_dog = any(cells[d][y][x]["actor"] == "dog" for d in range(3) for y in range(GH) for x in range(GW))
    slots = 2 + (0 if boss else 1) + (0 if has_dog else 1)
    if len(set(enemies)) > slots:
        raise LevelError("%s: %d enemy variants but only %d free sprite palettes" % (path, len(set(enemies)), slots))
    head["_enemies"] = enemies
    fams = prop_families(head, cells, bool(boss))
    if len(fams) > PROP_SLOTS:
        raise LevelError("%s: terrain props of %d colour families (%s) but only %d BG palettes for props" %
                         (path, len(fams), ", ".join(sorted(fams)), PROP_SLOTS))
    head["_prop_families"] = fams
    if len(head.get("hint", "")) > 76:
        raise LevelError("%s: hint longer than 76 characters" % path)
    if not grubs and not boss:
        raise LevelError("%s: needs at least one grub" % path)
    for d in range(3):
        for y in range(GH):
            for x in range(GW):
                t = T(d, x, y)
                if t in ("hole_down", "thin_floor") and d == 2:
                    raise LevelError("%s: hole or thin floor on the deepest level at %d,%d" % (path, x, y))
                if t == "hole_down" and T(d + 1, x, y) not in ("hole_up", "ladder"):
                    raise LevelError("%s: hole down at %d,%d (depth %d) needs a hole up or ladder below" % (path, x, y, d))
                if t in ("hole_up", "ladder") and (d == 0 or T(d - 1, x, y) != "hole_down"):
                    raise LevelError("%s: hole up/ladder at %d,%d (depth %d) needs a hole down above" % (path, x, y, d))
    # Solvability AND softlocks. Abstraction of the game state:
    #  - breakable cells (dirt, rock, roots, leaves, frozen dirt, crates) count as passable: digging and
    #    bombing only ever open the way (items survive blasts);
    #  - holes, ladders, pipes and thin floors (once broken: a hole down and a hole up) are two-way;
    #    steam vents are one-way (up);
    #  - water is passable where the depth has a log (logs can be moved along their river);
    #  - lever gates stay as they are once opened, and only the lever's side can close them: they are
    #    two-way as soon as a lever of their channel can be reached;
    #  - plate gates and timed gates are open only while or shortly after a plate is pressed: a gate cell
    #    can be entered only from a side whose region (gates closed) holds a plate of its channel; leaving
    #    the gate cell is always possible. This is where one-way passages (and softlocks) come from.
    # Rule: from EVERY reachable cell, every grub and the exit must still be reachable (so whatever the
    # player did, the level can be finished).
    logs = {d for d in range(3) for y in range(GH) for x in range(GW) if cells[d][y][x]["log"]}
    DIRS = ((1, 0), (-1, 0), (0, 1), (0, -1))

    def inside(x, y):
        return 0 <= x < GW and 0 <= y < GH

    def walkable(d, x, y):
        t = T(d, x, y)
        if t in SOLID_FOREVER or t == "gate":
            return False
        if t == "water":
            return d in logs
        return True
    pipes = {}
    for d in range(3):
        for y in range(GH):
            for x in range(GW):
                if T(d, x, y) == "pipe":
                    pipes.setdefault(cells[d][y][x]["chan"], []).append((d, x, y))

    def links(d, x, y, two_way=False):
        """Moves from a walkable cell, gates excluded."""
        out = [(d, x + dx, y + dy) for dx, dy in DIRS if inside(x + dx, y + dy) and walkable(d, x + dx, y + dy)]
        t = T(d, x, y)
        if t in ("hole_down", "thin_floor") and d < 2:
            out.append((d + 1, x, y))
        if (t in ("hole_up", "ladder") or (t == "steam_vent" and not two_way)) and d > 0:
            out.append((d - 1, x, y))
        if d > 0 and T(d - 1, x, y) == "thin_floor":        # a broken thin floor above: a hole up
            out.append((d - 1, x, y))
        if t == "pipe":
            out += [p for p in pipes.get(cells[d][y][x]["chan"], []) if p != (d, x, y)]
        return out

    # regions with every gate closed, and the triggers each region can use
    region = {}
    n_regions = 0
    for d in range(3):
        for y in range(GH):
            for x in range(GW):
                if (d, x, y) in region or not walkable(d, x, y):
                    continue
                stack = [(d, x, y)]
                region[(d, x, y)] = n_regions
                while stack:
                    c = stack.pop()
                    for n in links(*c, two_way=True):
                        if n not in region:
                            region[n] = n_regions
                            stack.append(n)
                n_regions += 1
    plates, levers = {}, {}
    for d in range(3):
        for y in range(GH):
            for x in range(GW):
                c = cells[d][y][x]
                if c["t"] == "plate" and (d, x, y) in region:
                    plates.setdefault(region[(d, x, y)], set()).add(c["chan"])
                if c["t"] == "lever":                     # bumped from any side
                    for dx, dy in DIRS:
                        if (d, x + dx, y + dy) in region:
                            levers.setdefault(region[(d, x + dx, y + dy)], set()).add(c["chan"])

    def graph(lever_chans):
        """Directed moves between cells (gate cells included)."""
        g = {}
        for d in range(3):
            for y in range(GH):
                for x in range(GW):
                    t = T(d, x, y)
                    if walkable(d, x, y):
                        out = list(links(d, x, y))
                        r = region[(d, x, y)]
                        for dx, dy in DIRS:
                            nx, ny = x + dx, y + dy
                            if not inside(nx, ny) or T(d, nx, ny) != "gate":
                                continue
                            ch = cells[d][ny][nx]["chan"]
                            if ch in lever_chans or ch in plates.get(r, ()) or ch in levers.get(r, ()):
                                out.append((d, nx, ny))
                        g[(d, x, y)] = out
                    elif t == "gate":
                        out = []
                        for dx, dy in DIRS:
                            nx, ny = x + dx, y + dy
                            if inside(nx, ny) and (walkable(d, nx, ny) or T(d, nx, ny) == "gate"):
                                out.append((d, nx, ny))
                        g[(d, x, y)] = out
        return g

    def reach(g, src):
        seen, todo = {src}, [src]
        while todo:
            for n in g.get(todo.pop(), ()):
                if n not in seen:
                    seen.add(n)
                    todo.append(n)
        return seen
    start = moles[0]
    g = graph(set())
    first = reach(g, start)
    lever_chans = {ch for r, chans in levers.items() for ch in chans
                   if any(region.get(c) == r for c in first)}
    g = graph(lever_chans)
    seen = reach(g, start)
    for tgt in grubs + boss + exits:
        if tgt not in seen:
            raise LevelError("%s: %s at depth %d, %d,%d cannot be reached" %
                             (path, "exit" if tgt in exits else "grub", tgt[0], tgt[1], tgt[2]))
    needed = set(grubs + boss + exits)
    checked = {}
    for c in sorted(seen):
        r = region.get(c, ("gate", c))
        if r in checked:
            continue
        missing = needed - reach(g, c)
        checked[r] = True
        if missing:
            m = sorted(missing)[0]
            raise LevelError("%s: SOFTLOCK: from depth %d, %d,%d the %s at depth %d, %d,%d can no longer be "
                             "reached (a one-way gate?)" % (path, c[0], c[1], c[2],
                                                            "exit" if m in exits else "grub", m[0], m[1], m[2]))
    design_rules(path, head, cells, g, seen, start, grubs, boss, exits)
    return head, len(grubs) + len(boss)


# ---- design rules (DESIGN.md, "Level ideas") ------------------------------------------------------------
ENEMY_START_DIST = 6        # no enemy closer than this (Manhattan, same depth) to the mole's start
MAX_EMPTY_AREA = 20         # largest rectangle of plain floor (both sides 3 or more) on a playable depth
SECTOR_W, SECTOR_H = 6, 6   # the 18x12 inside of a depth, cut into 3x2 sectors
SECTOR_MIN_OPEN = 12        # a sector with at least this many open cells...
SECTOR_MIN_FEATURES = 0.25  # ...needs this fraction of its cells to be something other than plain floor
GIMMICK_POINT_DIST = 2      # a point gimmick lies within this many cells (Chebyshev) of a required path
GIMMICK_AREA_DIST = 1       # an area gimmick (a patch, lane or sheet) touches a required path
POINT_GIMMICKS = {"plate", "lever", "gate", "pipe", "steam_vent", "bridge", "sprinkler", "crate", "windmill",
                  "thin_floor"}
AREA_GIMMICKS = {"ice", "thin_ice", "tall_grass", "puddle", "mud"}      # plus pushed floors, currents, logs
OPEN = {"floor", "puddle", "thin_floor", "exit", "bridge", "ice", "thin_ice", "mud", "tall_grass", "burnt",
        "plate", "steam_vent", "hole_up", "ladder", "hole_down", "pipe"}
ENTER_COST = {"soft_dirt": 3, "leaves": 2, "hard_rock": 5, "roots": 5, "frozen_dirt": 5, "crate": 5, "water": 2}
BLOW = {"up": (0, -1), "down": (0, 1), "left": (-1, 0), "right": (1, 0)}


def playable_depths(cells, seen, grubs, boss):
    """A depth is playable when it holds a grub (or the boss) or an open walkable cell (a floor, not a
    block to dig) the mole can reach; an all-solid depth that is never entered is not."""
    out = set(t[0] for t in grubs + boss)
    for (d, x, y) in seen:
        if cells[d][y][x]["t"] in OPEN:
            out.add(d)
    return out


def plain(c):
    return c["t"] == "floor" and not c["item"] and c["actor"] in (None, "mole") and not c["push"]


def empty_rect(cells, d):
    """Area and box of the largest rectangle of plain floor with both sides of 3 cells or more."""
    best = (0, None)
    for top in range(1, GH - 1):
        cols = [True] * GW
        for bot in range(top, GH - 1):
            for x in range(GW):
                cols[x] = cols[x] and plain(cells[d][bot][x])
            h = bot - top + 1
            if h < 3:
                continue
            run = 0
            for x in range(GW + 1):
                if x < GW and cols[x]:
                    run += 1
                    continue
                if run >= 3 and run * h > best[0]:
                    best = (run * h, (x - run, top, run, h))
                run = 0
    return best


def sector_density(cells, d):
    """(fraction of non-plain cells, open cells, x, y) of each sector of the depth's 18x12 inside."""
    out = []
    for sy in range(1, GH - 1, SECTOR_H):
        for sx in range(1, GW - 1, SECTOR_W):
            box = [cells[d][y][x] for y in range(sy, min(sy + SECTOR_H, GH - 1))
                   for x in range(sx, min(sx + SECTOR_W, GW - 1))]
            n_open = sum(1 for c in box if c["t"] in OPEN)
            feat = sum(1 for c in box if not plain(c))
            out.append((feat / float(len(box)), n_open, sx, sy))
    return out


def dijkstra(g, src, cells, reverse=False):
    import heapq
    if reverse:
        rg = {}
        for a, outs in g.items():
            for b in outs:
                rg.setdefault(b, []).append(a)
        g = rg
    dist = {src: 0}
    q = [(0, src)]
    while q:
        k, c = heapq.heappop(q)
        if k > dist.get(c, 1e9):
            continue
        for n in g.get(c, ()):
            # the cost of a move is the cost of entering its destination (reverse: of entering c)
            dst = c if reverse else n
            w = ENTER_COST.get(cells[dst[0]][dst[2]][dst[1]]["t"], 1)
            if k + w < dist.get(n, 1e9):
                dist[n] = k + w
                heapq.heappush(q, (k + w, n))
    return dist


def required_cells(g, cells, start, grubs, boss, exits, extra_targets=()):
    """Cells on a required path: a shortest path (digging costs more than walking) from the start to
    each grub, the boss and the exit, and from each grub to the exit (the player's last leg)."""
    req = set()
    targets = grubs + boss + exits + list(extra_targets)
    legs = [(start, t) for t in targets] + [(s, exits[0]) for s in grubs + list(extra_targets)]
    fwd, back = {}, {}
    for s, t in legs:
        if s not in fwd:
            fwd[s] = dijkstra(g, s, cells)
        if t not in back:
            back[t] = dijkstra(g, t, cells, reverse=True)
        ds, dt = fwd[s], back[t]
        if t not in ds:
            continue
        total = ds[t]
        req.update(c for c, k in ds.items() if c in dt and k + dt[c] == total)
    return req


def design_rules(path, head, cells, g, seen, start, grubs, boss, exits):
    """The design rules (DESIGN.md, "Level ideas"); raises one LevelError listing every problem."""
    T = lambda d, x, y: cells[d][y][x]["t"]  # noqa: E731
    errs = []
    play = playable_depths(cells, seen, grubs, boss)
    head["_playable"] = play
    # 1. an enemy on every playable depth, none next to the start
    for d in sorted(play):
        foes = [(x, y) for y in range(GH) for x in range(GW)
                if cells[d][y][x]["actor"] in ("ferret", "cat", "boss") and (d, x, y) in seen]
        if not foes:
            errs.append("NO ENEMY on playable depth %d (every depth with grubs or reachable floor needs one)" % d)
        if d == start[0]:
            for x, y in foes:
                if abs(x - start[1]) + abs(y - start[2]) < ENEMY_START_DIST:
                    errs.append("ENEMY AT START: the enemy at %d,%d is closer than %d cells to the mole" %
                                (x, y, ENEMY_START_DIST))
    # 2. no big empty areas
    stats = []
    for d in sorted(play):
        area, box = empty_rect(cells, d)
        dens = [s for s in sector_density(cells, d) if s[1] >= SECTOR_MIN_OPEN]
        stats.append((d, area, min([s[0] for s in dens] or [1.0])))
        if area > MAX_EMPTY_AREA:
            errs.append("EMPTY AREA on depth %d: %dx%d cells of plain floor at %d,%d (max %d cells)" %
                        (d, box[2], box[3], box[0], box[1], MAX_EMPTY_AREA))
        for frac, n_open, sx, sy in dens:
            if frac < SECTOR_MIN_FEATURES:
                errs.append("EMPTY AREA on depth %d: the sector at %d,%d is %d%% plain floor (at most %d%%)" %
                            (d, sx, sy, round(100 * (1 - frac)), round(100 * (1 - SECTOR_MIN_FEATURES))))
    head["_density"] = stats
    if "--stats" in sys.argv:
        print("  %-12s %s" % (os.path.basename(path)[:-4], "  ".join("d%d: empty %2d, sectors %2d%%" % (d, a, round(100 * f))
                                                                for d, a, f in stats)))
    # 3. every gimmick matters: it lies on or near a required path
    boss_kind = head.get("boss") or {"spring": "barncat", "summer": "farmer", "autumn": "fox",
                                     "winter": "owl"}[head["season"]]
    extra = []
    if boss and boss_kind == "farmer":          # his crates shield him: blowing them up is required
        extra = [(d, x, y) for d in range(3) for y in range(GH) for x in range(GW) if T(d, x, y) == "crate"]
    req = required_cells(g, cells, start, grubs, boss, exits, extra)
    head["_required"] = req

    def near(d, x, y, r):
        return any((d, x + dx, y + dy) in req for dx in range(-r, r + 1) for dy in range(-r, r + 1))
    # a gate on a required path makes the plates and levers of its channel matter (wherever they are)
    gates_ok = {cells[d][y][x]["chan"] for d in range(3) for y in range(GH) for x in range(GW)
                if T(d, x, y) == "gate" and near(d, x, y, 1)}
    for d in range(3):
        for y in range(GH):
            for x in range(GW):
                c = cells[d][y][x]
                t = c["t"]
                if t in ("plate", "lever", "gate") and c["chan"] in gates_ok:
                    continue
                if t == "windmill" and c["blow"]:
                    dx, dy = BLOW[c["blow"]]
                    lx, ly, lane = x + dx, y + dy, []
                    while 0 <= lx < GW and 0 <= ly < GH and (T(d, lx, ly) in OPEN or T(d, lx, ly) == "water"):
                        lane.append((lx, ly))
                        lx, ly = lx + dx, ly + dy
                    if not near(d, x, y, GIMMICK_POINT_DIST) and \
                            not any(near(d, a, b, GIMMICK_AREA_DIST) for a, b in lane):
                        errs.append("USELESS GIMMICK: the windmill at depth %d, %d,%d blows a lane that no required "
                                    "path crosses" % (d, x, y))
                    continue
                if t in POINT_GIMMICKS and not near(d, x, y, GIMMICK_POINT_DIST):
                    errs.append("USELESS GIMMICK: %s at depth %d, %d,%d is more than %d cells from every required "
                                "path (start to a grub or the exit)" % (t, d, x, y, GIMMICK_POINT_DIST))
    # area gimmicks: each connected patch of one kind touches a required path
    kind = lambda c: "wind/current" if c["push"] or c["log"] else (c["t"] if c["t"] in AREA_GIMMICKS else None)  # noqa: E731
    done = set()
    for d in range(3):
        for y in range(GH):
            for x in range(GW):
                k = kind(cells[d][y][x])
                if not k or (d, x, y) in done:
                    continue
                patch, todo = [], [(x, y)]
                done.add((d, x, y))
                while todo:
                    a, b = todo.pop()
                    patch.append((a, b))
                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        na, nb = a + dx, b + dy
                        if 0 <= na < GW and 0 <= nb < GH and (d, na, nb) not in done and kind(cells[d][nb][na]) == k:
                            done.add((d, na, nb))
                            todo.append((na, nb))
                if not any(near(d, a, b, GIMMICK_AREA_DIST) for a, b in patch):
                    errs.append("USELESS GIMMICK: the %s patch at depth %d, %d,%d (%d cells) does not touch a "
                                "required path" % (k, d, x, y, len(patch)))
    if errs:
        raise LevelError("%s: %s" % (path, "\n        ".join(errs)))


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    files = args or sorted(glob.glob(os.path.join(LEVELS, "*.txt")))
    bad = 0
    stubs = 0
    rows = []
    for f in files:
        try:
            head, g = check(f)
            stubs += head.get("status") == "stub"
            en = head["_enemies"]
            rows.append((os.path.basename(f)[:-4], head.get("status", ""), g, len(en),
                         ", ".join("%dx %s" % (en.count(e), e) for e in sorted(set(en), key=lambda e: ENEMY_TYPES[e][1]))))
        except LevelError as e:
            print("  FAIL", e)
            bad += 1
    if "--table" in sys.argv:
        print("| Level | Status | Grubs | Enemies | Mix |\n|---|---|---|---|---|")
        order = {"spring": 0, "summer": 1, "autumn": 2, "winter": 3}
        for r in sorted(rows, key=lambda r: (order[r[0].split("-")[0]], int(r[0].split("-")[1]))):
            print("| %s | %s | %d | %d | %s |" % r)
    print("levels: %d files, %d stubs, %s" % (len(files), stubs, "all valid" if not bad else "%d INVALID" % bad))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
