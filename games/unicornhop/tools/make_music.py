#!/usr/bin/env python3
"""Unicorn Hop music: "Cloud Hopper", a light, happy loop: a 4-channel ProTracker MOD with the house instrument set
(games/common/tools/house_music.py; docs/art-direction.md "Audio").

F major, speed 6 (125 BPM: a row is 0.12 s, a bar 16 rows), four patterns (A A' A B: about 31 s, then again).
Channel 0 the plucked arpeggio (the water-drop pluck, like little hooves on clouds), 1 the bass (oom-pah: the root on
the beat, the fifth on the off-beat), 2 the soft pad, 3 the breathy lead with the tune: it hops up by thirds and
fourths and floats down again, like the unicorn. A' doubles the tune an octave up on the pluck (a music box sparkle);
B climbs (IV V iii vi) and lands back home.

    make_music.py OUTDIR       -> OUTDIR/tune.mod

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/unicornhop/LICENSE.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note  # noqa: E402

KEY, SCALE = 5, hm.MAJOR                 # F major
PAD, BASS, DROP, LEAD = 1, 2, 3, 4      # the house set's sample numbers

# the tune, one bar a line, eight 8th notes a bar (scale degrees from F; None: hold)
_ = None
TUNE_A = [[0, 2, 4, _, 7, _, 4, _], [5, _, 4, 2, _, 0, 2, _], [3, _, 5, _, 7, _, 5, 3], [4, _, _, 2, 4, _, _, _]]
TUNE_A2 = [[7, _, 9, _, 7, 4, _, 2], [5, 4, 2, _, 0, _, _, _], [3, 5, 7, _, 8, 7, 5, 3], [4, 6, 4, 1, 2, _, _, _]]
TUNE_B = [[8, _, 7, _, 5, _, 3, _], [6, _, 4, _, 2, _, 4, _], [2, 4, 6, _, 4, _, 2, _], [1, _, 2, _, 4, _, _, _]]
CHORDS_A = [0, 5, 3, 4]                  # I vi IV V
CHORDS_B = [3, 4, 2, 5]                  # IV V iii vi (then home to I)


def deg(d, octave):
    return hm.deg(d, octave, KEY, SCALE)


def pattern(tune, chords, sparkle=False):
    rows = []
    for r in range(64):
        bar, step = r // 16, r % 16
        ch = chords[bar]
        c = [cell(), cell(), cell(), cell()]
        if step % 2 == 0:                                   # the arpeggio on the 8ths, accent on the beat
            a = ch + [0, 2, 4, 7, 4, 2, 0, 2][step // 2]
            c[0] = cell(DROP, note(deg(a, 1)), 0xC, 30 if step % 4 else 38)
        if step % 4 == 0:                                   # oom-pah
            c[1] = cell(BASS, note(deg(ch + (4 if step % 8 else 0), 0)), 0xC, 40 if step % 8 == 0 else 30)
        if step == 0:
            c[2] = cell(PAD, note(deg(ch + 2, 1)))
        elif step == 8:
            c[2] = cell(PAD, note(deg(ch + 4, 1)), 0xC, 16)
        if step % 2 == 0:
            d = tune[bar][step // 2]
            if d is not None:
                c[3] = cell(LEAD, note(deg(d, 1)), 0xC, 36)
                if sparkle and step % 4 == 0:               # the music box an octave up
                    c[0] = cell(DROP, note(deg(d + 7, 1)), 0xC, 26)
        rows.append(b"".join(c))
    return b"".join(rows)


def build():
    smp = hm.instruments(seed=5)
    pats = [pattern(TUNE_A, CHORDS_A), pattern(TUNE_A2, CHORDS_A, sparkle=True), pattern(TUNE_B, CHORDS_B)]
    return hm.build_mod("unicornhop", smp, pats, [0, 1, 0, 2], speed=6)


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)
    with open(os.path.join(outdir, "tune.mod"), "wb") as f:
        f.write(build())


if __name__ == "__main__":
    main()
