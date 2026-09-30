#!/usr/bin/env python3
"""Bomber Mole: writes the 6 battle arenas (games/bombermole/arenas/*.txt). Each depth is drawn as its
top-left quarter (9 x 6 cells), mirrored left-right and top-bottom, so the 4 starts are exactly fair; S = the
start of the quarter's mole (M, 2, 3, 4 by quarter). A hole down v puts a ladder H below it.

    make_arenas.py [out dir]     (default: games/bombermole/arenas)

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/bombermole/LICENSE.
"""
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else "games/bombermole/arenas"
FLIP_X = {'<': '>', '>': '<'}
FLIP_Y = {'n': 'u', 'u': 'n'}


def mirror(q):
    q = [r.replace("H", ".") for r in q]         # ladders come from the holes above
    assert len(q) == 6 and all(len(r) == 9 for r in q), q
    g = []
    for y in range(12):
        qy = y if y < 6 else 11 - y
        row = ""
        for x in range(18):
            qx = x if x < 9 else 17 - x
            c = q[qy][qx]
            if x >= 9: c = FLIP_X.get(c, c)
            if y >= 6: c = FLIP_Y.get(c, c)
            if c == 'S':
                c = {(0, 0): 'M', (1, 0): '2', (0, 1): '3', (1, 1): '4'}[(x >= 9, y >= 6)]
            row += c
        g.append(row)
    return g


def arena(name, title, season, comment, head, legend, quarters, edits=()):
    grids = [mirror(q) for q in quarters]
    grids = [[list(r) for r in g] for g in grids]
    for d in range(2):                          # a hole down lands on a ladder
        for y in range(12):
            for x in range(18):
                if grids[d][y][x] == 'v':
                    grids[d + 1][y][x] = 'H'
    for (d, x, y, c) in edits:                  # single cells (x, y: 1..18, 1..12)
        grids[d][y - 1][x - 1] = c
    out = ["# Bomber Mole battle arena: " + title + ". " + comment[0]] + ["# " + c for c in comment[1:]]
    out += ["name: " + title, "season: " + season, "arc: 0", "level: 0", "mode: battle"] + head + ["status: done", ""]
    if legend:
        out += ["legend:"] + legend + [""]
    for nm, g in zip(("surface", "under1", "under2"), grids):
        out += [nm + ":", "#" * 20] + ["#" + "".join(r) + "#" for r in g] + ["#" * 20]
    open("%s/%s.txt" % (OUT, name), "w", encoding="utf-8", newline="\n").write("\n".join(out) + "\n")


arena("molehill-maze", "Molehill Maze", "spring",
      ["The classic: a grid of stone posts and soft dirt, holes", "everywhere between three depths."],
      ["bombs: 1", "range: 2"], [],
      [["S.dddddd.",
        ".#d#d#d#d",
        "dd.dvdddd",
        "d#d#d#.#d",
        "dddvdd.d.",
        "d#d#d#d#d"],
       ["dddd.dddd",
        "dr.rdr.rd",
        "d..d...v.",
        "dr.r.rdrd",
        ".d.....dd",
        "drdrdrdr."],
       ["ffffffddd",
        "f..d.f.d.",
        "f.dd.f...",
        "f.d....d.",
        "f...d.d..",
        "ff.d....d"]])

arena("river-duel", "River Duel", "summer",
      ["A river splits the field: two bridges on each side and the", "crocodile in the middle; tunnels run under the riverbed."],
      ["bombs: 1", "range: 2"], [],
      [["S.dd.dd..",
        ".#d#.#d#d",
        "dd.vd.d.d",
        "d#d#.#v#d",
        ".dd.ddd.d",
        "~~=~~~~=~"],
       ["ddd.ddddd",
        "dr...r.vd",
        "d.rv..d.d",
        "d.r..rv.d",
        "d.v.....d",
        "dddd.dd.."],
       ["fffffffff",
        "ff.d...ff",
        "f..H.d.ff",
        "f.d..fH..",
        "f...d....",
        "ff.fff.f."]],
      edits=[(0, 9, 6, '%')])

arena("windmill-wars", "Windmill Wars", "spring",
      ["Gale lanes blow towards the middle: a bomb dropped in a lane", "drifts into the other half. Below, lanes of their own."],
      ["bombs: 1", "range: 2", "gust: 200 110 8"], [],
      [["S.dd.ddd.",
        ".#d#.#d#d",
        ">>>>>>>>>",
        "d#v#d#d#.",
        "dd.d.vd.d",
        "d#d#d#d#d"],
       ["dddd.dddd",
        "dr.r.r.rd",
        "d.....dd.",
        "d.rv>>>>>",
        ".d.r.H.dd",
        "drdrdr.r."],
       ["fffffffff",
        "f.d...ff.",
        "f.d.dd...",
        "f..Hd.ff.",
        "f.....f..",
        "ff.f.fff."]])

arena("ice-rink", "Ice Rink", "winter",
      ["A frozen rink ringed by snowdrifts: slide, and kick bombs across", "the ice; rocks are the only brakes."],
      ["bombs: 1", "range: 3"], [],
      [["S.,,d,,,.",
        ".r.iiiiii",
        ",.iiirii.",
        "d.iivii.d",
        ",.iiiiv.,",
        "d.r.iiii."],
       ["ffff.ffff",
        "f.v...d.f",
        "f.iiiH.d.",
        "f.i.H...f",
        ".d.i..v.f",
        "fddf.f.f."],
       ["fffffffff",
        "fff.d.fff",
        "ff.....ff",
        "f.d.d....",
        "f.....H..",
        "ff.fff.f."]])

arena("mine-cart-mayhem", "Mine Cart Mayhem", "autumn",
      ["Runaway carts race round loops on two depths and flatten any", "mole on the rails: cross between them."],
      ["bombs: 1", "range: 2"], ["R = rails + runaway"],
      [["S.dd.d.d.",
        ".#d#d#d#d",
        "d.+R+++++",
        "d#+#v#d#d",
        "dd+d.d.vd",
        "d#+#d#d#d"],
       ["dddd.dddd",
        "d.d.H.d.d",
        "d.d.v.d..",
        "d.+R++++H",
        ".d+.d.d.d",
        "dd+dd.d.d"],
       ["fffffffff",
        "fff.d.f..",
        "ff..H....",
        "f.d.d.ff.",
        "f....f.fH",
        "ff.f.f.f."]])

arena("pumpkin-fort", "Pumpkin Fort", "autumn",
      ["Each mole starts in a fort of pumpkins: push them out as", "cover, or into a hole to seal a way down."],
      ["bombs: 1", "range: 2"], [],
      [["S.0.d.dd.",
        ".00d#d#d.",
        "0.d.v.d.d",
        ".d#d#d#.d",
        "dd.v.d..0",
        "d#d.d#d.."],
       ["dddd.dddd",
        "d.d.d.d.d",
        "d..dH..v.",
        "d.dddd.dd",
        ".d.H.d.d.",
        "dd.d.d.d."],
       ["fffffffff",
        "f.d.f.d.f",
        "f...d..H.",
        "f.dff.f..",
        "f.d..d...",
        "ff.f.f.f."]])
print("arenas written")
