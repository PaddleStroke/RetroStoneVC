#!/usr/bin/env python3
"""Caterpillar Crawl music: "Inchworm Picnic", a light, happy garden loop: a 4-channel ProTracker MOD with the house
instrument set (games/common/tools/house_music.py; docs/art-direction.md "Audio").

G major, speed 6 (125 BPM: a row is 0.12 s, a bar 16 rows), four patterns (A A' A B: about 31 s, then again).
Channel 0 the plucked arpeggio (the water-drop pluck, little feet pattering), 1 the bass (oom-pah: the root on the
beat, the fifth on the off-beat), 2 the soft pad, 3 the breathy lead with the tune: it inches up step by step, a
little hop, and wiggles back down, like a caterpillar on a stem. A' answers a fifth higher with the pluck doubling it
(a music box), B climbs (vi IV I V) and lands back home.

    make_music.py OUTDIR       -> OUTDIR/tune.mod

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/caterpillarcrawl/LICENSE.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note  # noqa: E402

KEY, SCALE = 7, hm.MAJOR                 # G major
PAD, BASS, DROP, LEAD = 1, 2, 3, 4      # the house set's sample numbers

# the tune, one bar a line, eight 8th notes a bar (scale degrees from G; None: hold)
_ = None
TUNE_A = [[0, 1, 2, _, 1, 2, 3, _], [2, 3, 4, _, 7, _, 4, _], [5, 4, 3, _, 4, 3, 2, _], [1, _, 2, 1, 0, _, _, _]]
TUNE_A2 = [[4, 5, 6, _, 5, 6, 7, _], [6, 7, 9, _, 7, _, 4, _], [5, 6, 7, 5, 4, _, 2, _], [3, 2, 1, _, 0, _, _, _]]
TUNE_B = [[7, _, 5, _, 7, _, 9, _], [8, _, 7, 5, 3, _, 5, _], [4, 5, 7, _, 4, _, 2, _], [1, 2, 3, _, 4, _, _, _]]
CHORDS_A = [0, 3, 1, 4]                  # I IV ii V
CHORDS_B = [5, 3, 0, 4]                  # vi IV I V (then home to I)


def deg(d, octave):
    return hm.deg(d, octave, KEY, SCALE)


def pattern(tune, chords, sparkle=False):
    rows = []
    for r in range(64):
        bar, step = r // 16, r % 16
        ch = chords[bar]
        c = [cell(), cell(), cell(), cell()]
        if step % 2 == 0:                                   # the arpeggio on the 8ths, accent on the beat
            a = ch + [0, 2, 4, 2, 4, 7, 4, 2][step // 2]
            c[0] = cell(DROP, note(deg(a, 1)), 0xC, 28 if step % 4 else 36)
        if step % 4 == 0:                                   # oom-pah
            c[1] = cell(BASS, note(deg(ch + (4 if step % 8 else 0), 0)), 0xC, 40 if step % 8 == 0 else 28)
        if step == 0:
            c[2] = cell(PAD, note(deg(ch + 2, 1)))
        elif step == 8:
            c[2] = cell(PAD, note(deg(ch + 4, 1)), 0xC, 16)
        if step % 2 == 0:
            d = tune[bar][step // 2]
            if d is not None:
                c[3] = cell(LEAD, note(deg(d, 1)), 0xC, 36)
                if sparkle and step % 4 == 0:               # the music box an octave up
                    c[0] = cell(DROP, note(deg(d + 7, 1)), 0xC, 24)
        rows.append(b"".join(c))
    return b"".join(rows)


def build():
    smp = hm.instruments(seed=5)
    pats = [pattern(TUNE_A, CHORDS_A), pattern(TUNE_A2, CHORDS_A, sparkle=True), pattern(TUNE_B, CHORDS_B)]
    return hm.build_mod("caterpillarcrawl", smp, pats, [0, 1, 0, 2], speed=6)


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)
    with open(os.path.join(outdir, "tune.mod"), "wb") as f:
        f.write(build())


if __name__ == "__main__":
    main()
