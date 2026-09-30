#!/usr/bin/env python3
"""Leady Squid music: a gentle underwater loop, a small 4-channel ProTracker MOD
(played by libxmp-lite in the SDK). The house instrument set (games/common/tools/house_music.py):
a soft sine pad, a round bass, a water-drop pluck and a breathy lead; slow tempo, D dorian.

    make_music.py OUTDIR       -> OUTDIR/tune.mod

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/leadysquid/LICENSE.
"""
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note  # noqa: E402

KEY, SCALE = 2, hm.DORIAN                        # D dorian
CHORDS = [0, 6, 5, 6]                            # Dm, C, Bb, C (scale degrees)


def deg(d, octave):
    return hm.deg(d, octave, KEY, SCALE)


def pattern(rng, variant):
    rows = []
    mel = 9
    for r in range(64):
        ch = CHORDS[r // 16]
        c = [cell(), cell(), cell(), cell()]
        # water drops: a slow arpeggio of the chord, every 2 rows, some skipped
        if r % 2 == 0 and rng.random() > (0.25 if variant else 0.4):
            c[0] = cell(3, note(deg(ch + [0, 2, 4, 7, 4, 2][(r // 2) % 6], 2)), 0xC, rng.choice([28, 36, 44]))
        # bass: the root every 8 rows, a soft fifth in between
        if r % 8 == 0:
            c[1] = cell(2, note(deg(ch, 0)))
        elif r % 8 == 6 and variant:
            c[1] = cell(2, note(deg(ch + 4, 0)), 0xC, 30)
        # pad: the chord's third, held
        if r % 16 == 0:
            c[2] = cell(1, note(deg(ch + 2, 1)))
        # lead: a few long notes in the variants
        if variant and r % 8 == 4 and rng.random() > 0.35:
            mel = max(7, min(13, mel + rng.choice([-2, -1, 1, 2])))
            c[3] = cell(4, note(deg(mel, 1)), 0xC, 34)
        elif variant and r % 8 == 7:
            c[3] = cell(0, 0, 0xC, 0)
        rows.append(b"".join(c))
    return b"".join(rows)


def build():
    rng = random.Random(1993)
    smp = hm.instruments(seed=5)
    pats = [pattern(rng, 0), pattern(rng, 1), pattern(rng, 1)]
    return hm.build_mod("leady squid", smp, pats, [0, 1, 0, 2], speed=8)


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)
    with open(os.path.join(outdir, "tune.mod"), "wb") as f:
        f.write(build())


if __name__ == "__main__":
    main()
