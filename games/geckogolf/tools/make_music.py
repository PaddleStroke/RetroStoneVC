#!/usr/bin/env python3
"""Gecko Golf music: "Fairway Stroll", a relaxed walk over the hills: a 4-channel ProTracker MOD with the house
instrument set (games/common/tools/house_music.py; docs/art-direction.md "Audio").

G major, speed 8 (94 BPM: a row is 0.16 s, a bar 16 rows), five patterns (A A' B A' C: about 51 s, then again).
Channel 0 the water-drop pluck in lazy broken chords (up, then back down, like a ball bouncing to rest), 1 a soft
round bass walking root, fifth, sixth, 2 the warm sine pad holding each chord with a high dewdrop now and then, 3 the
breathy lead with the tune: a sunny little phrase that strolls up the hill and sits down on the green. A and A'
(G, Em, C, D): the phrase; A' sings it an octave up with the pluck doubling; B (C, D, Bm, Em) looks at the view;
C (Am, D, G, G) walks home.

    make_music.py OUTDIR       -> OUTDIR/tune.mod

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/geckogolf/LICENSE.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note  # noqa: E402

KEY, SCALE = 7, hm.MAJOR                 # G major
PAD, BASS, DROP, LEAD = 1, 2, 3, 4

_ = None
# the tune, one bar a line, eight 8th notes a bar (scale degrees from G; None: hold)
TUNE_A = [[2, _, 4, _, 5, 4, 2, _], [1, _, _, 0, 1, _, _, _], [2, _, 4, 5, 7, _, 5, _], [4, _, _, _, _, _, 3, _]]
TUNE_A2 = [[2, _, 4, _, 5, 4, 2, _], [1, _, _, 0, 1, _, 2, _], [4, _, 5, _, 7, 8, 9, _], [7, _, _, _, _, _, _, _]]
TUNE_B = [[2, _, _, 4, 3, _, 2, _], [3, _, _, 5, 4, _, _, _], [1, _, 2, _, 4, _, 2, 1], [0, _, _, _, 1, _, _, _]]
TUNE_C = [[5, _, 4, _, 2, _, 0, _], [1, _, 3, _, 5, _, 4, _], [2, _, _, 1, 0, _, _, _], [0, _, _, _, _, _, _, _]]
CHORDS_A = [0, 5, 3, 4]                  # G Em C D
CHORDS_B = [3, 4, 2, 5]                  # C D Bm Em
CHORDS_C = [1, 4, 0, 0]                  # Am D G G


def deg(d, octave):
    return hm.deg(d, octave, KEY, SCALE)


def pattern(tune, chords, sparkle=False, home=False):
    rows = []
    for r in range(64):
        bar, step = r // 16, r % 16
        ch = chords[bar]
        c = [cell(), cell(), cell(), cell()]
        if step % 2 == 0:                                   # broken chords: up the hill, then back down
            a = ch + [0, 2, 4, 7, 4, 2, 4, 2][step // 2]
            c[0] = cell(DROP, note(deg(a, 1)), 0xC, 16 if step % 4 else 22)
        if step in (0, 6, 10):                              # the bass strolls: root, fifth, sixth
            c[1] = cell(BASS, note(deg(ch + {0: 0, 6: 4, 10: 5}[step], 0)), 0xC, 36 if step == 0 else 26)
        if step == 0:                                       # the pad holds the chord
            c[2] = cell(PAD, note(deg(ch + 2, 1)), 0xC, 22)
        elif step == 12 and bar % 2 == 0:                   # a dewdrop up high
            c[2] = cell(DROP, note(deg(ch + 9, 2)), 0xC, 9)
        if home and bar == 3 and step % 4 == 0 and step:    # the last steps home, down to the root
            c[0] = cell(DROP, note(deg([4, 2, 0][step // 4 - 1], 1)), 0xC, 20)
        if step % 2 == 0:
            d = tune[bar][step // 2]
            if d is not None:
                c[3] = cell(LEAD, note(deg(d, 1)), 0xC, 40)
                if sparkle and step % 4 == 0:               # the pluck doubles the tune an octave up
                    c[0] = cell(DROP, note(deg(d + 7, 1)), 0xC, 20)
        rows.append(b"".join(c))
    return b"".join(rows)


def build():
    smp = hm.instruments(seed=17)
    pats = [pattern(TUNE_A, CHORDS_A), pattern(TUNE_A2, CHORDS_A, sparkle=True), pattern(TUNE_B, CHORDS_B),
            pattern(TUNE_C, CHORDS_C, home=True)]
    return hm.build_mod("geckogolf", smp, pats, [0, 1, 2, 1, 3], speed=8)


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)
    with open(os.path.join(outdir, "tune.mod"), "wb") as f:
        f.write(build())


if __name__ == "__main__":
    main()
