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


def stub(season, arc, n):
    f = FEATURE[season]
    note = PLANNED.get(season, [""] * 7)[n - 2]
    surface = [
        "####################",
        "#M.................#",
        "#..................#",
        "#....ddd....%s%s%s....#" % (f, f, f),
        "#....dgd....%s.%s....#" % (f, f),
        "#....ddd....%s%s%s....#" % (f, f, f),
        "#..................#",
        "#..................#",
        "#.........v........#",
        "#..................#",
        "#..................#",
        "#.......E..........#",
        "#..................#",
        "####################",
    ]
    under1 = ["####################"] + ["#dddddddddddddddddd#"] * 6 + [
        "#dddddd.......ddddd#",
        "#dddddd...^.g.ddddd#",
        "#dddddd.......ddddd#",
    ] + ["#dddddddddddddddddd#"] * 3 + ["####################"]
    under2 = ["####################"] + ["#dddddddddddddddddd#"] * 12 + ["####################"]
    return "\n".join([
        "# STUB: a placeholder level. Replace it with a real design (see DESIGN.md).",
        "# Planned: %s" % note,
        "name: %s %d (stub)" % (season.capitalize(), n),
        "season: %s" % season,
        "arc: %d" % arc,
        "level: %d" % n,
        "status: stub",
        "hint: A stub level: grab the grubs and reach the exit.",
        "",
        "surface:", *surface, "under1:", *under1, "under2:", *under2, ""])


def main():
    made = 0
    for a, season in enumerate(SEASONS):
        for n in range(1, 9):
            p = os.path.join(LEVELS, "%s-%d.txt" % (season, n))
            if os.path.exists(p):
                continue
            with open(p, "w", newline="\n") as fh:
                fh.write(stub(season, a + 1, n))
            made += 1
    print("wrote %d stub levels" % made)


if __name__ == "__main__":
    main()
