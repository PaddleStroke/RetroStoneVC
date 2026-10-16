#!/usr/bin/env python3
"""Llama Drama music: "Huayno de la Llama", a cheerful Andean dance: a 4-channel ProTracker MOD with the house
instrument set and the kit's drums (games/common/tools/house_music.py; docs/art-direction.md "Audio").

A minor, speed 3 (125 BPM to the quarter: a row is 0.06 s, a 2/4 bar 16 rows), five patterns (A B A C B: about 19 s,
then again), in the huayno's skipping rhythm: long, short, short on every beat. Channel 0 the charango: the house pluck
strummed (the arpeggio effect rolls each chord's three notes, like the strings under the thumb) on the off-beats;
1 the bass (the root long, the fifth and the root short); 2 the bombo and a shaker (the kick, the hat); 3 the quena,
the breathy house lead: a call that tumbles down the pentatonic scale and lands on A, answered an octave up; C turns
to C major and climbs, then E major (the Andean dominant) brings it home.

    make_music.py OUTDIR       -> OUTDIR/tune.mod

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/llamadrama/LICENSE.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note  # noqa: E402

KEY, SCALE = 9, hm.MINOR                 # A minor (the pentatonic A C D E G, and B and F now and then)
PAD, BASS, DROP, LEAD, KICK, SNARE, HAT = 1, 2, 3, 4, 5, 6, 7

_ = None
# the quena's tune: one bar a line, eight 16th notes a bar (scale degrees from A; None: hold): long, short, short
TUNE_A = [[4, _, 4, 3, 2, _, 0, _], [2, _, 3, 4, 4, _, _, _], [6, _, 4, 3, 2, _, 3, _], [0, _, _, _, 0, _, _, _]]
TUNE_B = [[7, _, 7, 6, 4, _, 6, _], [7, _, 9, 7, 6, _, _, _], [4, _, 6, 4, 3, _, 2, _], [4, _, _, _, 4, _, _, _]]
TUNE_C = [[2, _, 4, 6, 7, _, 6, _], [4, _, 6, 4, 2, _, _, _], [5, _, 7, 5, 4, _, 2, _], [4, _, _, 3, 1, _, _, _]]
# chords: (root degree, arpeggio: 0x37 minor, 0x47 major)
AM, C, G, E, F = (0, 0x37), (2, 0x47), (6, 0x47), (4, 0x47), (5, 0x47)
CHORDS_A = [AM, C, G, AM]
CHORDS_B = [AM, G, C, E]
CHORDS_C = [C, G, F, E]


def deg(d, octave):
    return hm.deg(d, octave, KEY, SCALE)


def root(ch, octave):
    """The chord's root: E major's G# is not in A minor: the chord roots are scale degrees, the third comes from the
    arpeggio."""
    return deg(ch[0], octave)


def pattern(tune, chords, high=False, roll=False):
    rows = []
    for r in range(64):
        bar, step = r // 16, r % 16
        ch = chords[bar]
        beat_row = step % 8
        c = [cell(), cell(), cell(), cell()]
        if beat_row in (2, 5, 6) or (beat_row == 0 and step == 8):   # the charango strums (the arpeggio rolls)
            c[0] = cell(DROP, note(root(ch, 2)), 0x0, ch[1])
        if beat_row in (0, 4, 6):                           # the bass: root long, fifth and root short
            o = 0 if beat_row == 0 else 1
            d = ch[0] + (4 if beat_row == 4 else 0)
            c[1] = cell(BASS, note(deg(d, o)), 0xC, 40 if beat_row == 0 else 30)
        if beat_row == 0:                                   # the bombo: long, short, short
            c[2] = cell(KICK, note(12), 0xC, 36)
        elif beat_row in (4, 6):
            c[2] = cell(KICK, note(14), 0xC, 22)
        elif beat_row in (2, 7):
            c[2] = cell(HAT, note(30), 0xC, 12)
        if roll and bar == 3 and step >= 8 and step % 2:
            c[2] = cell(SNARE, note(24), 0xC, 12 + step)
        if step % 2 == 0:
            d = tune[bar][step // 2]
            if d is not None:
                c[3] = cell(LEAD, note(deg(d, 2 if high else 1)), 0xC, 44 if step % 8 == 0 else 36)
        rows.append(b"".join(c))
    return b"".join(rows)


def build():
    smp = hm.instruments(seed=7) + [("bombo", hm.kick(), 36, 0, 0), ("snare", hm.snare(), 22, 0, 0),
                                    ("shaker", hm.hat(), 12, 0, 0)]
    pats = [pattern(TUNE_A, CHORDS_A), pattern(TUNE_B, CHORDS_B), pattern(TUNE_C, CHORDS_C, roll=True),
            pattern(TUNE_B, CHORDS_B, high=True)]
    return hm.build_mod("llamadrama", smp, pats, [0, 1, 0, 2, 3], speed=3)


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)
    with open(os.path.join(outdir, "tune.mod"), "wb") as f:
        f.write(build())


if __name__ == "__main__":
    main()
