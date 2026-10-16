#!/usr/bin/env python3
"""Snowball Siege music: "Frosty Fort", a cheerful winter tune: a 4-channel ProTracker MOD with the house instrument
set and a little sleigh-bell jingle (games/common/tools/house_music.py; docs/art-direction.md "Audio").

G major, speed 6 (125 BPM: a row is 0.12 s, a bar 16 rows), five patterns (A A' B A' C: about 38 s, then again).
Channel 0 the water-drop pluck in skipping arpeggios (snowflakes), 1 a round bass bouncing root-fifth (boots in the
snow), 2 the warm pad on each chord with the sleigh bells shaking on the off-beats, 3 the breathy lead with the tune:
a bright phrase that hops up the chord and slides home, like a snowball lobbed over a fort. A and A' (G, Em, C, D);
A' sings it an octave up with the pluck doubling; B (C, D, Bm, Em) winds up; C (C, D, G, G) comes home.

    make_music.py OUTDIR       -> OUTDIR/tune.mod

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/snowballsiege/LICENSE.
"""
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note  # noqa: E402

KEY, SCALE = 7, hm.MAJOR                 # G major
PAD, BASS, DROP, LEAD, BELLS = 1, 2, 3, 4, 5

_ = None
# the tune, one bar a line, eight 8th notes a bar (scale degrees from G; None: hold)
TUNE_A = [[4, _, 2, _, 4, _, 7, _], [6, _, 5, 4, 2, _, _, _], [3, _, 5, _, 7, _, 6, 5], [4, _, _, _, 1, _, _, _]]
TUNE_A2 = [[7, _, 9, _, 11, _, 9, 7], [8, _, 7, 6, 4, _, _, _], [5, _, 7, _, 10, _, 9, 7], [8, _, _, _, 6, _, _, _]]
TUNE_B = [[5, _, _, 4, 3, _, 2, _], [6, _, _, 5, 4, _, 3, _], [4, _, 6, _, 8, _, 6, _], [5, _, _, _, _, _, _, _]]
TUNE_C = [[3, _, 4, _, 5, _, 6, _], [7, _, 6, _, 8, _, _, _], [7, _, 4, _, 2, _, 4, _], [0, _, _, _, _, _, _, _]]
CHORDS_A = [0, 5, 3, 4]                  # G Em C D
CHORDS_B = [3, 4, 2, 5]                  # C D Bm Em
CHORDS_C = [3, 4, 0, 0]                  # C D G G


def bells():
    """Sleigh bells: two high detuned rings and a little shaken noise, decaying fast (one-shot)."""
    rng = random.Random(3)
    out = []
    for i in range(1400):
        v = 34 * math.sin(2 * math.pi * i / 7.1) + 26 * math.sin(2 * math.pi * i / 5.3) \
            + rng.uniform(-30, 30) * math.exp(-i / 70.0)
        out.append(hm.s8(v * math.exp(-i / 240.0)))
    return out


def deg(d, octave):
    return hm.deg(d, octave, KEY, SCALE)


def pattern(tune, chords, sparkle=False, home=False):
    rows = []
    for r in range(64):
        bar, step = r // 16, r % 16
        ch = chords[bar]
        c = [cell(), cell(), cell(), cell()]
        if step % 2 == 0:                                   # snowflakes: the chord skipping up and down
            a = ch + [0, 4, 2, 4, 7, 4, 2, 4][step // 2]
            c[0] = cell(DROP, note(deg(a, 2)), 0xC, 18 if step % 4 else 26)
        if step in (0, 6, 8, 14):                           # the bass: root, a skip, the fifth, a skip
            b = {0: 0, 6: 0, 8: 4, 14: 4}[step]
            c[1] = cell(BASS, note(deg(ch + b, 0)), 0xC, 40 if step in (0, 8) else 26)
        if step == 0:                                       # the pad holds the chord's third
            c[2] = cell(PAD, note(deg(ch + 2, 1)), 0xC, 22)
        elif step % 4 == 2:                                 # the sleigh bells on the off-beats
            c[2] = cell(BELLS, note(deg(0, 2)), 0xC, 16 if step % 8 == 2 else 22)
        if home and bar == 3 and step % 2 == 0 and step >= 6:   # the flakes settle home
            c[0] = cell(DROP, note(deg([0, 2, 4, 7, 9][min(4, (step - 6) // 2)], 2)), 0xC, 22)
        if step % 2 == 0:
            d = tune[bar][step // 2]
            if d is not None:
                c[3] = cell(LEAD, note(deg(d, 1)), 0xC, 40)
                if sparkle and step % 4 == 0:               # the pluck doubles the tune an octave up
                    c[0] = cell(DROP, note(deg(d + 7, 1)), 0xC, 24)
        rows.append(b"".join(c))
    return b"".join(rows)


def build():
    smp = hm.instruments(seed=13) + [("bells", bells(), 24, 0, 0)]
    pats = [pattern(TUNE_A, CHORDS_A), pattern(TUNE_A2, CHORDS_A, sparkle=True), pattern(TUNE_B, CHORDS_B),
            pattern(TUNE_C, CHORDS_C, home=True)]
    return hm.build_mod("snowballsiege", smp, pats, [0, 1, 2, 1, 3], speed=6)


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)
    with open(os.path.join(outdir, "tune.mod"), "wb") as f:
        f.write(build())


if __name__ == "__main__":
    main()
