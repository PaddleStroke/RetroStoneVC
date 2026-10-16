#!/usr/bin/env python3
"""Ladybug Laps music: "Garden Grand Prix", an upbeat racing tune: a 4-channel ProTracker MOD with the house music kit
(games/common/tools/house_music.py; docs/art-direction.md "Audio").

G major, speed 5 (150 BPM: a row is 0.1 s, a bar 16 rows), four patterns (A A' B C: about 26 s, then again).
Channel 0 the water-drop pluck in bright off-beat stabs (the chord's fifth and third, a hop up on the last beat), 1 a
driving bass in eighths (root, root, octave, root: the engines purring), 2 the drums (kick on the beats, the snare on
two and four, hats between, a snare roll into the next lap), 3 the soft square lead with the tune: a cheeky call that
zips up the chord and slides home. A and A' (G, D, Em, C): the call; A' sings it an octave up; B (C, D, Bm, Em) races
along; C (Am, D, G, G) brakes into the corner and accelerates back to the start.

    make_music.py OUTDIR       -> OUTDIR/tune.mod

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/ladybuglaps/LICENSE.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note  # noqa: E402

KEY, SCALE = 7, hm.MAJOR                 # G major
SQ, BASS, KICK, HAT, SNARE, DROP = 1, 2, 3, 4, 5, 6

_ = None
# the tune, one bar a line, eight 8th notes a bar (scale degrees from G; None: hold)
TUNE_A = [[0, _, 2, 4, 7, _, 4, _], [6, _, 4, _, 1, _, _, _], [5, _, 4, 2, 4, _, 2, _], [3, _, 2, _, 0, _, _, _]]
TUNE_B = [[3, _, 5, _, 7, _, 5, 3], [4, _, 6, _, 8, _, 6, 4], [2, _, 4, _, 6, _, 4, 2], [5, _, 4, _, 5, _, _, _]]
TUNE_C = [[1, _, 3, _, 5, _, 3, _], [4, _, 5, _, 6, _, 8, _], [7, _, _, _, 4, _, _, _], [7, 6, 5, 4, 3, 2, 1, _]]
CHORDS_A = [0, 4, 5, 3]                  # G D Em C
CHORDS_B = [3, 4, 2, 5]                  # C D Bm Em
CHORDS_C = [1, 4, 0, 0]                  # Am D G G


def deg(d, octave):
    return hm.deg(d, octave, KEY, SCALE)


def pattern(tune, chords, up=0, roll=False):
    rows = []
    for r in range(64):
        bar, step = r // 16, r % 16
        ch = chords[bar]
        c = [cell(), cell(), cell(), cell()]
        if step % 4 == 2:                                   # off-beat stabs
            c[0] = cell(DROP, note(deg(ch + (4 if step % 8 == 2 else 2), 1)), 0xC, 20)
        elif step == 14:
            c[0] = cell(DROP, note(deg(ch + 7, 1)), 0xC, 24)
        if step % 2 == 0:                                   # the driving bass
            o = 1 if step % 8 == 4 else 0
            c[1] = cell(BASS, note(deg(ch, o)), 0xC, 38 if step % 8 == 0 else 28)
        if step in (0, 8):                                  # the drums
            c[2] = cell(KICK, note(12), 0xC, 36)
        elif step in (4, 12):
            c[2] = cell(SNARE, note(24), 0xC, 24)
        elif step % 2 == 0:
            c[2] = cell(HAT, note(30), 0xC, 12)
        if roll and bar == 3 and step >= 8 and step % 2:
            c[2] = cell(SNARE, note(24), 0xC, 10 + step)
        if step % 2 == 0:
            d = tune[bar][step // 2]
            if d is not None:
                c[3] = cell(SQ, note(deg(d + up, 1)), 0xC, 30)
        rows.append(b"".join(c))
    return b"".join(rows)


def build():
    smp = [("square", hm.square_lead(0.25), 30, 0, 32), ("bass", hm.bass(), 38, 0, 64), ("kick", hm.kick(), 34, 0, 0),
           ("hat", hm.hat(), 12, 0, 0), ("snare", hm.snare(), 22, 0, 0), ("drop", hm.drop(), 30, 0, 0)]
    pats = [pattern(TUNE_A, CHORDS_A), pattern(TUNE_A, CHORDS_A, up=7), pattern(TUNE_B, CHORDS_B),
            pattern(TUNE_C, CHORDS_C, roll=True)]
    return hm.build_mod("ladybuglaps", smp, pats, [0, 1, 2, 3], speed=5)


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)
    with open(os.path.join(outdir, "tune.mod"), "wb") as f:
        f.write(build())


if __name__ == "__main__":
    main()
