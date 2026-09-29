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


# Two layouts, alternated along the arc (mirrored for levels 5-7), using all three depths with a loop:
# the surface's two holes lead to depth 1 (one comes back up by a ladder), depth 1 drops to depth 2 and
# a ladder climbs back. Every depth has an enemy (the level's tier), a grub hidden in a block (only its
# glow shows it), a riskier grub near the enemies, and the season's feature (%) near the paths.
LAYOUT_A = {
    "surface": [
        "####################",
        "#M..d...rr...%%%..g#",
        "#...d.g.rr...%.%...#",
        "#dd.dd......ddd..v.#",
        "#..........r.dGd...#",
        "#.rr.%%.d..r.ddd...#",
        "#.rQ.%%.d......C...#",
        "#.rr....ddd.rr.....#",
        "#.....v.....rr.%%..#",
        "#.ddd......d....%..#",
        "#.dGd..%%..d..rr...#",
        "#.ddd..%%.....rg...#",
        "#.......E......d...#",
        "####################"],
    "under1": [
        "####################",
        "#dddddddddddddddddd#",
        "#dd.g....ddd...dddd#",
        "#dd.dddd.rdd.d..d^d#",
        "#dd.d..d..d..F.dd.d#",
        "#dd...dd.dddd.dd..d#",
        "#dddd.dd.v..d..dddd#",
        "#dd.....tt..dd.Gddd#",
        "#dd.ddHdd.ddd.ddddd#",
        "#dd.d...d.....ddddd#",
        "#dd...d.dddd.dddddd#",
        "#dd.ddd......r.gddd#",
        "#dddddddddddddddddd#",
        "####################"],
    "under2": [
        "####################",
        "#dddddddddddddddddd#",
        "#dddddddddddddddddd#",
        "#ddddd.....dddddddd#",
        "#dddd..rQr..ddddddd#",
        "#dddd..r.r..F.ddddd#",
        "#dddd.b..^.....dddd#",
        "#dddddd..ddd.g.dddd#",
        "#dddddddddddddddddd#",
        "#dddddddddddddddddd#",
        "#dddddddddddddddddd#",
        "#dddddddddddddddddd#",
        "#dddddddddddddddddd#",
        "####################"],
}
LAYOUT_B = {
    "surface": [
        "####################",
        "#M..rr....%%%.....g#",
        "#...rQ.dd.%.%.ddd..#",
        "#.d.rr.dd.....dGd..#",
        "#.d......r..v.ddd..#",
        "#.dd.%%..r.........#",
        "#....%%.....rrr.C..#",
        "#.rrr...dd..r.r....#",
        "#.r.r...dd......%%.#",
        "#.rgr.v.....ddd.%%.#",
        "#......%%...dGd....#",
        "#.dd...%%...ddd.x..#",
        "#.dd......E........#",
        "####################"],
    "under1": [
        "####################",
        "#dddddddddddddddddd#",
        "#ddd.g..ddd..dddddd#",
        "#ddd.dd.ddd.d.ddddd#",
        "#ddd.dd.....^..dddd#",
        "#dd..ddd.rr.dd.dddd#",
        "#dd.dd...rG.dd..ddd#",
        "#dd.dd.F......d.ddd#",
        "#dd.ddd.dd.v.dd.ddd#",
        "#dd.ddH.dd...dd.ddd#",
        "#dd.......tt....ddd#",
        "#ddddd.ddd.dd.g.ddd#",
        "#dddddddddddddddddd#",
        "####################"],
    "under2": [
        "####################",
        "#dddddddddddddddddd#",
        "#dddddddddddddddddd#",
        "#dddddddddddddddddd#",
        "#dddddddddddddddddd#",
        "#dddddd.......ddddd#",
        "#dddddd.rQr...ddddd#",
        "#dddddd.r.r.F.ddddd#",
        "#dddddd....^..ddddd#",
        "#ddddddd.ddd.dddddd#",
        "#dddddd..o...g.dddd#",
        "#dddddddddddddddddd#",
        "#dddddddddddddddddd#",
        "####################"],
}
FEATURE_CHAR = {"spring": "p", "summer": "w", "autumn": "l", "winter": "f"}


def mirror(rows):
    """Left-right mirror of a depth (the border stays)."""
    return [r[0] + r[1:-1][::-1] + r[-1] for r in rows]


def stub(season, arc, n):
    note = PLANNED.get(season, [""] * 7)[n - 2]
    lay = LAYOUT_A if n % 2 == 0 else LAYOUT_B
    f = FEATURE_CHAR[season]
    depths = {}
    for k, rows in lay.items():
        rows = [r.replace("%", f) for r in rows]
        if 5 <= n <= 7:
            rows = mirror(rows)
        depths[k] = rows
    return "\n".join([
        "# STUB: a generated level (tools/make_stub_levels.py). Replace it with a real design (DESIGN.md).",
        "# Planned: %s" % note,
        "# Layout %s%s: three depths with a loop (two holes down, a ladder back), an enemy on each depth, a grub"
        % ("A" if lay is LAYOUT_A else "B", ", mirrored" if 5 <= n <= 7 else ""),
        "# hidden in a block on each depth (only its glow shows it), the season's feature near the paths.",
        "name: %s %d (stub)" % (season.capitalize(), n),
        "season: %s" % season,
        "arc: %d" % arc,
        "level: %d" % n,
        "status: stub",
        "hint: A generated level: three depths, a grub hidden on each.",
        "",
        "surface:", *depths["surface"], "under1:", *depths["under1"], "under2:", *depths["under2"], ""])


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
