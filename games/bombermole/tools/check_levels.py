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


class LevelError(Exception):
    pass


def parse_spec(spec):
    out = {"t": "floor", "item": None, "actor": None, "chan": 0, "log": False, "push": None}
    for tok in spec.replace("+", " ").split():
        if tok in TERRAIN:
            out["t"] = {"dirt": "soft_dirt", "rock": "hard_rock", "vent": "steam_vent", "corn": "tall_grass",
                        "cover": "tall_grass"}.get(tok, tok)
        elif tok in ITEMS:
            out["item"] = tok
        elif tok in ACTORS:
            out["actor"] = tok
        elif tok == "asleep":
            pass
        elif tok == "log":
            out["log"] = True
        elif tok.startswith("chan:"):
            out["chan"] = int(tok[5:])
        elif tok.startswith("timed:"):
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
    # solvability (optimistic: anything breakable can be broken, logs can be moved along their river)
    logs = {d for d in range(3) for y in range(GH) for x in range(GW) if cells[d][y][x]["log"]}

    def passable(d, x, y):
        c = cells[d][y][x]
        t = c["t"]
        if t in SOLID_FOREVER:
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
    start = moles[0]
    seen, todo = {start}, [start]
    while todo:
        d, x, y = todo.pop()
        nxt = []
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if 0 <= nx < GW and 0 <= ny < GH and passable(d, nx, ny):
                nxt.append((d, nx, ny))
        t = T(d, x, y)
        if t in ("hole_down", "thin_floor") and d < 2:
            nxt.append((d + 1, x, y))
        if t in ("hole_up", "ladder", "steam_vent") and d > 0:
            nxt.append((d - 1, x, y))
        if t == "pipe":
            nxt += pipes.get(cells[d][y][x]["chan"], [])
        for n in nxt:
            if n not in seen:
                seen.add(n)
                todo.append(n)
    for g in grubs + boss + exits:
        if g not in seen:
            raise LevelError("%s: %s at depth %d, %d,%d cannot be reached" %
                             (path, "exit" if g in exits else "grub", g[0], g[1], g[2]))
    return head, len(grubs) + len(boss)


def main():
    files = sys.argv[1:] or sorted(glob.glob(os.path.join(LEVELS, "*.txt")))
    bad = 0
    stubs = 0
    for f in files:
        try:
            head, g = check(f)
            stubs += head.get("status") == "stub"
        except LevelError as e:
            print("  FAIL", e)
            bad += 1
    print("levels: %d files, %d stubs, %s" % (len(files), stubs, "all valid" if not bad else "%d INVALID" % bad))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
