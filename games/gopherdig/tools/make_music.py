#!/usr/bin/env python3
"""Gopher Dig music: "Burrow Boogie", a bouncy digging tune: a 4-channel ProTracker MOD with the house instruments
(games/common/tools/house_music.py; docs/art-direction.md "Audio").

G major, speed 5 (150 BPM: a bar is 16 rows), four patterns (A A' B C: about 26 s, then again).
Channel 0 the soft square lead with the tune: a perky "dig-dig-dig, pop!" figure that hops up and drops back, answered
an octave up in A'; 1 a hopping octave bass (the shovel's rhythm); 2 the drums: kick on the beat, snare on 2 and 4,
hats between, a little roll at the end of C; 3 the water-drop pluck on the off-beats like clinks of a drill, climbing
in C. A and A' (G, Em, C, D); B (C, D, Bm, Em) wanders down a tunnel; C (C, D, G, G) pops back up.

    make_music.py OUTDIR       -> OUTDIR/tune.mod

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/gopherdig/LICENSE.
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
TUNE_A = [[4, 4, 4, _, 7, _, 4, _], [2, _, 2, 4, 5, 4, 2, _], [3, 3, 3, _, 5, _, 3, 1], [4, _, _, 2, 1, _, _, _]]
TUNE_B = [[3, _, 5, 3, 0, _, 3, _], [4, _, 6, 4, 1, _, 4, _], [2, 4, 6, _, 5, 4, 2, _], [1, _, 2, _, 4, _, _, _]]
TUNE_C = [[3, 3, 5, _, 7, _, 5, 3], [4, 4, 6, _, 8, _, 6, 4], [7, _, 4, _, 2, 4, 7, _], [7, _, _, _, _, _, _, _]]
CHORDS_A = [0, 5, 3, 4]                  # G Em C D
CHORDS_B = [3, 4, 2, 5]                  # C D Bm Em
CHORDS_C = [3, 4, 0, 0]                  # C D G G


def deg(d, octave):
    return hm.deg(d, octave, KEY, SCALE)


def pattern(tune, chords, up=0, climb=False, roll=False):
    rows = []
    for r in range(64):
        bar, st = r // 16, r % 16
        ch = chords[bar]
        c = [cell(), cell(), cell(), cell()]
        if st % 2 == 0:
            d = tune[bar][st // 2]
            if d is not None:
                c[0] = cell(SQ, note(deg(d + up, 1)), 0xC, 30 if st % 4 == 0 else 24)
        if st % 4 == 0 or st % 4 == 3:                  # the hopping octave bass
            c[1] = cell(BASS, note(deg(ch, 0 if st % 8 == 0 else 1)), 0xC, 38 if st % 8 == 0 else 26)
        if st in (0, 8):
            c[2] = cell(KICK, note(12), 0xC, 34)
        elif st in (4, 12):
            c[2] = cell(SNARE, note(24), 0xC, 22)
        elif st % 2 == 0:
            c[2] = cell(HAT, note(30), 0xC, 12)
        if roll and bar == 3 and st >= 10 and st % 2:
            c[2] = cell(SNARE, note(24), 0xC, 10 + st)
        if st % 4 == 2:                                 # the drill's clinks on the off-beats
            k = (st // 4 + bar) % 3 if climb else 0
            c[3] = cell(DROP, note(deg(ch + 7 + 2 * k, 1)), 0xC, 16)
        rows.append(b"".join(c))
    return b"".join(rows)


def build():
    smp = [("square", hm.square_lead(0.25), 28, 0, 32), ("bass", hm.bass(), 38, 0, 64), ("kick", hm.kick(), 34, 0, 0),
           ("hat", hm.hat(), 12, 0, 0), ("snare", hm.snare(), 22, 0, 0), ("drop", hm.drop(), 30, 0, 0)]
    pats = [pattern(TUNE_A, CHORDS_A), pattern(TUNE_A, CHORDS_A, up=7), pattern(TUNE_B, CHORDS_B),
            pattern(TUNE_C, CHORDS_C, climb=True, roll=True)]
    return hm.build_mod("gopherdig", smp, pats, [0, 1, 2, 3], speed=5)


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)
    with open(os.path.join(outdir, "tune.mod"), "wb") as f:
        f.write(build())


if __name__ == "__main__":
    main()
