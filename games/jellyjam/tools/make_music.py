#!/usr/bin/env python3
"""Jelly Jam music: "Jam Session", a bouncy kitchen tune: a 4-channel ProTracker MOD with the house instrument set
(games/common/tools/house_music.py; docs/art-direction.md "Audio").

G major, speed 6 (125 BPM: a row is 0.12 s, a bar 16 rows), five patterns (A A' B A' C: about 38 s, then again).
Channel 0 the water-drop pluck bouncing on the off-beats like jelly (the chord's notes, up and down), 1 the round bass
hopping root-fifth-octave, 2 a soft kick on the beat and a hat on the "and" (the busy cook's knife), 3 the breathy lead
with the tune: a cheeky phrase that wobbles up and plops down, then answers itself. A and A' (G, Em, C, D): the phrase;
A' sings it an octave up with the pluck doubling; B (C, D, Bm, Em) stirs the pot; C (C, D, G, G) serves it up.

    make_music.py OUTDIR       -> OUTDIR/tune.mod

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/jellyjam/LICENSE.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note  # noqa: E402

KEY, SCALE = 7, hm.MAJOR                 # G major
PAD, BASS, DROP, LEAD, KICK, HAT = 1, 2, 3, 4, 5, 6

_ = None
# the tune, one bar a line, eight 8th notes a bar (scale degrees from G; None: hold)
TUNE_A = [[4, _, 4, 5, 4, _, 2, _], [0, _, 2, _, 4, _, _, _], [5, _, 4, 2, 3, _, 2, 0], [1, _, _, _, 4, _, _, _]]
TUNE_A2 = [[7, _, 7, 8, 7, _, 4, _], [2, _, 4, _, 7, _, _, _], [8, _, 7, 5, 6, _, 5, 3], [4, _, _, _, 1, _, _, _]]
TUNE_B = [[2, _, 3, 4, 5, _, 4, _], [3, _, 4, 5, 6, _, _, _], [4, _, 3, 2, 1, _, 2, _], [0, _, _, _, _, _, _, _]]
TUNE_C = [[4, _, 5, _, 7, _, 5, 4], [3, _, 4, _, 6, _, 4, _], [2, 4, 7, _, 9, _, 7, _], [7, _, _, _, _, _, _, _]]
CHORDS_A = [0, 5, 3, 4]                  # G Em C D
CHORDS_B = [3, 4, 2, 5]                  # C D Bm Em
CHORDS_C = [3, 4, 0, 0]                  # C D G G


def deg(d, octave):
    return hm.deg(d, octave, KEY, SCALE)


def pattern(tune, chords, sparkle=False, serve=False):
    rows = []
    for r in range(64):
        bar, step = r // 16, r % 16
        ch = chords[bar]
        c = [cell(), cell(), cell(), cell()]
        if step % 4 == 2:                                   # the pluck on the off-beats: the chord, up and down
            a = ch + [0, 2, 4, 2][(step // 4) % 4]
            c[0] = cell(DROP, note(deg(a + 7, 1)), 0xC, 22)
        if step in (0, 6, 8, 12):                           # the bass hops: root, fifth, octave, fifth
            c[1] = cell(BASS, note(deg(ch + {0: 0, 6: 4, 8: 7, 12: 4}[step], 0)), 0xC, 38 if step == 0 else 28)
        if step % 4 == 0:                                   # a soft kick on the beat, the hat between
            c[2] = cell(KICK, note(12), 0xC, 30 if step % 8 == 0 else 22)
        elif step % 4 == 2:
            c[2] = cell(HAT, note(30), 0xC, 10)
        if bar == 0 and step == 0:
            c[2] = cell(PAD, note(deg(ch + 2, 1)), 0xC, 16)
        if serve and bar == 3 and step >= 8 and step % 2 == 0:   # the last bar: the pluck climbs to serve
            c[0] = cell(DROP, note(deg([0, 2, 4, 7][(step - 8) // 2], 2)), 0xC, 24)
        if step % 2 == 0:
            d = tune[bar][step // 2]
            if d is not None:
                c[3] = cell(LEAD, note(deg(d, 1)), 0xC, 40)
                if sparkle and step % 4 == 0:               # the pluck doubles the tune an octave up
                    c[0] = cell(DROP, note(deg(d + 7, 1)), 0xC, 20)
        rows.append(b"".join(c))
    return b"".join(rows)


def build():
    smp = hm.instruments(seed=17) + [("kick", hm.kick(), 26, 0, 0), ("hat", hm.hat(), 10, 0, 0)]
    pats = [pattern(TUNE_A, CHORDS_A), pattern(TUNE_A2, CHORDS_A, sparkle=True), pattern(TUNE_B, CHORDS_B),
            pattern(TUNE_C, CHORDS_C, serve=True)]
    return hm.build_mod("jellyjam", smp, pats, [0, 1, 2, 1, 3], speed=6)


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)
    with open(os.path.join(outdir, "tune.mod"), "wb") as f:
        f.write(build())


if __name__ == "__main__":
    main()
