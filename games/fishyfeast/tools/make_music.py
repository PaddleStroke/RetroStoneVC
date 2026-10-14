#!/usr/bin/env python3
"""Fishy Feast music: "Pond Picnic", a light underwater tune: a 4-channel ProTracker MOD with the house instrument set
(games/common/tools/house_music.py; docs/art-direction.md "Audio").

F major, speed 6 (125 BPM: a row is 0.12 s, a bar 16 rows), five patterns (A A' B A' C: about 38 s, then again).
Channel 0 the water-drop pluck in bubbly arpeggios (rising like bubbles, two notes per beat), 1 a soft round bass on
the beat and its fifth (the fish bobbing), 2 the warm sine pad on each chord with a tiny bubble "plip" up high on the
off-beats, 3 the breathy lead with the tune: a curious little phrase that swims up and turns back, then answers
itself. A and A' (F, Dm, Bb, C): the phrase; A' sings it an octave up with the pluck doubling; B (Bb, C, Am, Dm)
wanders off; C (Bb, C, F, F) swims home.

    make_music.py OUTDIR       -> OUTDIR/tune.mod

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/fishyfeast/LICENSE.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note  # noqa: E402

KEY, SCALE = 5, hm.MAJOR                 # F major
PAD, BASS, DROP, LEAD = 1, 2, 3, 4

_ = None
# the tune, one bar a line, eight 8th notes a bar (scale degrees from F; None: hold)
TUNE_A = [[4, _, 5, 4, 2, _, 0, _], [1, _, 2, _, 4, _, _, _], [3, _, 4, 5, 6, _, 5, 3], [4, _, _, _, 1, _, _, _]]
TUNE_A2 = [[7, _, 8, 7, 4, _, 2, _], [5, _, 4, _, 2, _, _, _], [3, _, 5, 6, 7, _, 8, 7], [6, _, 4, _, 4, _, _, _]]
TUNE_B = [[3, _, _, 5, 6, _, 5, _], [4, _, _, 6, 7, _, 6, _], [5, 4, 2, _, 4, _, 2, _], [1, _, _, _, _, _, _, _]]
TUNE_C = [[3, _, 5, _, 6, 5, 3, _], [4, _, 6, _, 8, _, 6, _], [7, _, _, 4, 2, _, 4, _], [0, _, _, _, _, _, _, _]]
CHORDS_A = [0, 5, 3, 4]                  # F Dm Bb C
CHORDS_B = [3, 4, 2, 5]                  # Bb C Am Dm
CHORDS_C = [3, 4, 0, 0]                  # Bb C F F


def deg(d, octave):
    return hm.deg(d, octave, KEY, SCALE)


def pattern(tune, chords, sparkle=False, home=False):
    rows = []
    for r in range(64):
        bar, step = r // 16, r % 16
        ch = chords[bar]
        c = [cell(), cell(), cell(), cell()]
        if step % 2 == 0:                                   # bubbles: the chord rising, two notes a beat
            a = ch + [0, 2, 4, 7, 2, 4, 7, 9][step // 2]
            c[0] = cell(DROP, note(deg(a, 2)), 0xC, 18 if step % 4 else 26)
        if step == 0 or step == 8:                          # the bass: root, then fifth
            c[1] = cell(BASS, note(deg(ch + (4 if step == 8 else 0), 0)), 0xC, 38 if step == 0 else 30)
        elif step == 14 and bar % 2:
            c[1] = cell(BASS, note(deg(ch + 2, 0)), 0xC, 24)
        if step == 0:                                       # the pad holds the chord's third
            c[2] = cell(PAD, note(deg(ch + 2, 1)), 0xC, 22)
        elif step in (6, 14):                               # a tiny bubble plip
            c[2] = cell(DROP, note(deg(ch + 9, 2)), 0xC, 10)
        if home and bar == 3 and step % 2 == 0 and step >= 6:   # the last bubbles climb home
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
    smp = hm.instruments(seed=11)
    pats = [pattern(TUNE_A, CHORDS_A), pattern(TUNE_A2, CHORDS_A, sparkle=True), pattern(TUNE_B, CHORDS_B),
            pattern(TUNE_C, CHORDS_C, home=True)]
    return hm.build_mod("fishyfeast", smp, pats, [0, 1, 2, 1, 3], speed=6)


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)
    with open(os.path.join(outdir, "tune.mod"), "wb") as f:
        f.write(build())


if __name__ == "__main__":
    main()
