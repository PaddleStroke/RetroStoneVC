#!/usr/bin/env python3
"""Hamster Havoc music: "Moon Patrol Picnic", a light, spacey loop: a 4-channel ProTracker MOD with the house
instrument set (games/common/tools/house_music.py; docs/art-direction.md "Audio").

D lydian (the raised fourth gives the major key its floating, starry colour), speed 6 (125 BPM: a row is 0.12 s, a
bar 16 rows), four patterns (A A' A B: about 31 s, then again). Channel 0 the plucked arpeggio up high (twinkling
stars), 1 the bass (bouncing octaves, like a hamster wheel), 2 the soft pad (long chords: space), 3 the breathy
lead with the tune: it leaps by fifths and floats down, curious and cheerful. The A sections sway between D and E
major (the lydian vamp); A' lifts the tune an octave and doubles it on the pluck; B (B minor, A, D, E) turns home.
The march beat of the mice (the game's own sound, faster as they thin out) plays over it: the tune stays light.

    make_music.py OUTDIR       -> OUTDIR/tune.mod

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/hamsterhavoc/LICENSE.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note  # noqa: E402

LYDIAN = [0, 2, 4, 6, 7, 9, 11]
KEY, SCALE = 2, LYDIAN                   # D lydian
PAD, BASS, DROP, LEAD = 1, 2, 3, 4      # the house set's sample numbers

# the tune, one bar a line, eight 8th notes a bar (scale degrees from D; None: hold)
_ = None
TUNE_A = [[4, _, 7, _, 6, 4, _, _], [5, _, 8, _, 7, _, 5, _], [4, 6, 7, _, 9, _, 7, _], [8, _, 7, 5, 3, _, _, _]]
TUNE_A2 = [[7, _, 9, _, 11, _, 9, 7], [8, _, 10, _, 8, 7, 5, _], [4, 6, 7, 9, 11, _, 9, _], [8, _, _, 7, 8, _, _, _]]
TUNE_B = [[5, _, 7, _, 9, _, 7, _], [4, _, 6, _, 8, _, 6, _], [7, _, 9, 11, 9, 7, 4, _], [1, _, 3, _, 5, _, _, _]]
CHORDS_A = [0, 1, 0, 1]                  # D E D E (the lydian sway)
CHORDS_B = [5, 4, 0, 1]                  # Bm A D E


def deg(d, octave):
    return hm.deg(d, octave, KEY, SCALE)


def pattern(tune, chords, sparkle=False):
    rows = []
    for r in range(64):
        bar, step = r // 16, r % 16
        ch = chords[bar]
        c = [cell(), cell(), cell(), cell()]
        if step % 2 == 0:                                   # the twinkling arpeggio, high and soft
            a = ch + [0, 4, 7, 9, 7, 4, 2, 4][step // 2]
            c[0] = cell(DROP, note(deg(a, 2)), 0xC, 22 if step % 4 else 30)
        if step % 4 == 0:                                   # the bass: root, octave, fifth, octave
            b = [(0, 0), (0, 1), (4, 0), (0, 1)][step // 4]
            c[1] = cell(BASS, note(deg(ch + b[0], b[1])), 0xC, 40 if step == 0 else 30)
        if step == 0:
            c[2] = cell(PAD, note(deg(ch + 2, 1)))
        elif step == 8:
            c[2] = cell(PAD, note(deg(ch + 4, 1)), 0xC, 16)
        if step % 2 == 0:
            d = tune[bar][step // 2]
            if d is not None:
                c[3] = cell(LEAD, note(deg(d, 1)), 0xC, 36)
                if sparkle and step % 4 == 0:               # the pluck doubles the tune an octave up
                    c[0] = cell(DROP, note(deg(d + 7, 1)), 0xC, 26)
        rows.append(b"".join(c))
    return b"".join(rows)


def build():
    smp = hm.instruments(seed=7)
    pats = [pattern(TUNE_A, CHORDS_A), pattern(TUNE_A2, CHORDS_A, sparkle=True), pattern(TUNE_B, CHORDS_B)]
    return hm.build_mod("hamsterhavoc", smp, pats, [0, 1, 0, 2], speed=6)


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)
    with open(os.path.join(outdir, "tune.mod"), "wb") as f:
        f.write(build())


if __name__ == "__main__":
    main()
