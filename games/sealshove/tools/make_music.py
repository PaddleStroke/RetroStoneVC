#!/usr/bin/env python3
"""Seal Shove music: "Floe Polka", a bouncy waddling tune: a 4-channel ProTracker MOD with the house instrument set
(games/common/tools/house_music.py; docs/art-direction.md "Audio").

G major, speed 5 (150 BPM: a row is 0.1 s, a bar 16 rows), five patterns (A A' B A' C: about 32 s, then again).
Channel 0 the water-drop pluck on the off-beats (the polka's "pah", the chord), 1 the round bass on the beats, root
and fifth (the "oom": a seal's waddle), 2 a light kit (a soft kick on the beats, a snare on 2 and 4, little hats), 3
the soft square lead with the tune: a hop up the chord and a tumble back down, answered. A and A' (G, Em, D7, G);
A' sings it an octave up and the pluck doubles; B (C, D, Bm, Em) slides off sideways; C (C, D, G, G) waddles home.

    make_music.py OUTDIR       -> OUTDIR/tune.mod

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/sealshove/LICENSE.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note  # noqa: E402

KEY, SCALE = 7, hm.MAJOR                 # G major
DROP, BASS, SQ, KICK, SNARE, HAT = 1, 2, 3, 4, 5, 6

_ = None
# the tune, one bar a line, eight 8th notes a bar (scale degrees from G; None: hold)
TUNE_A = [[0, 2, 4, _, 4, _, 4, 5], [7, _, 5, 4, 2, _, _, _], [1, 4, 6, _, 6, _, 6, 4], [4, _, 2, _, 0, _, _, _]]
TUNE_A2 = [[7, 9, 11, _, 11, _, 11, 12], [14, _, 12, 11, 9, _, _, _], [8, 11, 13, _, 13, _, 13, 11],
           [11, _, 9, _, 7, _, _, _]]
TUNE_B = [[3, _, 5, _, 7, _, 5, 3], [4, _, 6, _, 8, _, 6, 4], [2, _, 4, _, 6, _, 4, 2], [5, _, _, 4, 1, _, _, _]]
TUNE_C = [[3, 5, 7, _, 8, _, 7, 5], [4, 6, 8, _, 8, 7, 6, 4], [7, _, 4, _, 2, _, 4, _], [7, _, _, _, _, _, _, _]]
CHORDS_A = [0, 5, 4, 0]                  # G Em D7 G
CHORDS_B = [3, 4, 2, 5]                  # C D Bm Em
CHORDS_C = [3, 4, 0, 0]                  # C D G G


def deg(d, octave):
    return hm.deg(d, octave, KEY, SCALE)


def pattern(tune, chords, sparkle=False, fill=False):
    rows = []
    for r in range(64):
        bar, step = r // 16, r % 16
        ch = chords[bar]
        c = [cell(), cell(), cell(), cell()]
        if step % 4 == 2:                                   # the "pah": the chord's third and fifth, plucked
            c[0] = cell(DROP, note(deg(ch + (2 if step % 8 == 2 else 4), 2)), 0xC, 24)
        if step % 8 == 0:                                   # the "oom": root, then fifth
            c[1] = cell(BASS, note(deg(ch + (4 if step == 8 else 0), 0)), 0xC, 40 if step == 0 else 32)
        elif step == 14 and bar % 2:
            c[1] = cell(BASS, note(deg(ch + 2, 0)), 0xC, 26)
        if step % 8 == 0:
            c[2] = cell(KICK, note(12), 0xC, 30)
        elif step % 8 == 4:
            c[2] = cell(SNARE, note(24), 0xC, 18)
        elif step % 2 == 0:
            c[2] = cell(HAT, note(30), 0xC, 10)
        if fill and bar == 3 and step >= 8 and step % 2 == 0:
            c[2] = cell(SNARE, note(24), 0xC, 10 + step)
        if step % 2 == 0:
            d = tune[bar][step // 2]
            if d is not None:
                c[3] = cell(SQ, note(deg(d, 1)), 0xC, 30)
                if sparkle and step % 4 == 0:               # the pluck doubles the tune
                    c[0] = cell(DROP, note(deg(d + 7, 1)), 0xC, 20)
            elif step % 4 == 2 and tune[bar][step // 2 - 1] is not None:
                c[3] = cell(SQ, 0, 0xC, 0)                  # staccato: a hop, not a slur
        rows.append(b"".join(c))
    return b"".join(rows)


def samples():
    return [("drop", hm.drop(), 32, 0, 0), ("bass", hm.bass(), 40, 0, 64), ("square", hm.square_lead(0.25), 28, 0, 32),
            ("kick", hm.kick(), 32, 0, 0), ("snare", hm.snare(), 20, 0, 0), ("hat", hm.hat(), 12, 0, 0)]


def build():
    pats = [pattern(TUNE_A, CHORDS_A), pattern(TUNE_A2, CHORDS_A, sparkle=True), pattern(TUNE_B, CHORDS_B, fill=True),
            pattern(TUNE_C, CHORDS_C)]
    return hm.build_mod("sealshove", samples(), pats, [0, 1, 2, 1, 3], speed=5)


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)
    with open(os.path.join(outdir, "tune.mod"), "wb") as f:
        f.write(build())


if __name__ == "__main__":
    main()
