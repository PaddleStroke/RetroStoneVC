#!/usr/bin/env python3
"""Cosmo Corgi music: "Star Fetch", an upbeat space tune: a 4-channel ProTracker MOD with the house instrument set and
the kit's drums (games/common/tools/house_music.py; docs/art-direction.md "Audio").

A major, speed 5 (150 BPM: a row is 0.1 s, a bar 16 rows), five patterns (A A' B A' C: about 32 s, then again).
Channel 0 the plucked arpeggio up high (the stars streaming by), 1 the bass (bouncing octaves on the off-beats: the
rocket's engine), 2 the drums (kick, snare, hats: a happy gallop), 3 the breathy lead with the tune: a bright call
that leaps up a sixth like a corgi jumping for a ball, and lands on the beat. A and A' (A, F#m, D, E): the call; A'
answers it an octave up with the pluck doubling; B (D, E, C#m, F#m) climbs; C (D, E, A, A) brings it home with a
snare roll.

    make_music.py OUTDIR       -> OUTDIR/tune.mod

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/cosmocorgi/LICENSE.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note  # noqa: E402

KEY, SCALE = 9, hm.MAJOR                 # A major
PAD, BASS, DROP, LEAD, KICK, SNARE, HAT = 1, 2, 3, 4, 5, 6, 7

_ = None
# the tune, one bar a line, eight 8th notes a bar (scale degrees from A; None: hold)
TUNE_A = [[4, _, 2, 4, 7, _, 5, _], [5, _, 4, 2, 0, _, _, _], [3, _, 5, 7, 9, _, 7, 5], [4, _, _, 6, 8, _, _, _]]
TUNE_A2 = [[7, _, 9, 11, 9, _, 7, _], [9, _, 7, 5, 4, _, 2, _], [3, 5, 7, _, 10, _, 9, 7], [8, _, 6, _, 4, _, _, _]]
TUNE_B = [[3, _, 5, _, 7, 5, 3, _], [4, _, 6, _, 8, 6, 4, _], [2, _, 4, 6, 7, _, 6, 4], [5, _, 7, _, 9, _, _, _]]
TUNE_C = [[3, _, 5, 7, 10, _, 9, _], [8, _, 6, _, 4, _, 6, _], [7, _, _, 4, 2, _, 4, _], [0, _, _, _, 7, _, _, _]]
CHORDS_A = [0, 5, 3, 4]                  # A F#m D E
CHORDS_B = [3, 4, 2, 5]                  # D E C#m F#m
CHORDS_C = [3, 4, 0, 0]                  # D E A A


def deg(d, octave):
    return hm.deg(d, octave, KEY, SCALE)


def pattern(tune, chords, sparkle=False, roll=False):
    rows = []
    for r in range(64):
        bar, step = r // 16, r % 16
        ch = chords[bar]
        c = [cell(), cell(), cell(), cell()]
        if step % 2 == 0:                                   # the arpeggio, high and quick
            a = ch + [0, 2, 4, 7, 4, 2, 4, 7][step // 2]
            c[0] = cell(DROP, note(deg(a, 2)), 0xC, 20 if step % 4 else 28)
        if step % 4 == 2 or step == 0:                      # the bass: the root, then octaves on the off-beats
            o = 0 if step in (0, 8) else 1
            c[1] = cell(BASS, note(deg(ch, o)), 0xC, 40 if step == 0 else 30)
        if step in (0, 8):                                  # kick on 1 and 3, snare on 2 and 4, hats between
            c[2] = cell(KICK, note(12), 0xC, 34)
        elif step in (4, 12):
            c[2] = cell(SNARE, note(24), 0xC, 24)
        elif step % 2 == 0:
            c[2] = cell(HAT, note(30), 0xC, 14)
        if roll and bar == 3 and step >= 8 and step % 2:
            c[2] = cell(SNARE, note(24), 0xC, 14 + step)
        if step % 2 == 0:
            d = tune[bar][step // 2]
            if d is not None:
                c[3] = cell(LEAD, note(deg(d, 1)), 0xC, 40)
                if sparkle and step % 4 == 0:               # the pluck doubles the tune an octave up
                    c[0] = cell(DROP, note(deg(d + 7, 1)), 0xC, 26)
        rows.append(b"".join(c))
    return b"".join(rows)


def build():
    smp = hm.instruments(seed=9) + [("kick", hm.kick(), 34, 0, 0), ("snare", hm.snare(), 24, 0, 0),
                                    ("hat", hm.hat(), 14, 0, 0)]
    pats = [pattern(TUNE_A, CHORDS_A), pattern(TUNE_A2, CHORDS_A, sparkle=True), pattern(TUNE_B, CHORDS_B),
            pattern(TUNE_C, CHORDS_C, roll=True)]
    return hm.build_mod("cosmocorgi", smp, pats, [0, 1, 2, 1, 3], speed=5)


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)
    with open(os.path.join(outdir, "tune.mod"), "wb") as f:
        f.write(build())


if __name__ == "__main__":
    main()
