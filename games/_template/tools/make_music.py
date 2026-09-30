#!/usr/bin/env python3
"""@NAME@ music: a 4-channel ProTracker MOD with the house instrument set
(games/common/tools/house_music.py; docs/art-direction.md "Audio"). REPLACE the tune, keep the kit.

    make_music.py OUTDIR       -> OUTDIR/tune.mod

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/@ID@/LICENSE.
"""
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note  # noqa: E402

KEY, SCALE = 0, hm.MAJOR                 # C major: a bright, bouncy loop
CHORDS = [0, 4, 5, 3]                    # I V vi IV (scale degrees)


def deg(d, octave):
    return hm.deg(d, octave, KEY, SCALE)


def pattern(rng, variant):
    rows = []
    mel = 7
    for r in range(64):
        ch = CHORDS[r // 16]
        c = [cell(), cell(), cell(), cell()]
        if r % 2 == 0:                                      # plucked arpeggio
            c[0] = cell(3, note(deg(ch + [0, 2, 4, 2][(r // 2) % 4], 2)), 0xC, 32 if r % 4 else 40)
        if r % 4 == 0:                                      # bass on the beat, the fifth on the offbeat
            c[1] = cell(2, note(deg(ch + (4 if r % 8 else 0), 0)))
        if r % 16 == 0:                                     # pad: the chord's third
            c[2] = cell(1, note(deg(ch + 2, 1)))
        if variant and r % 4 == 2 and rng.random() > 0.3:  # a little lead tune
            mel = max(5, min(11, mel + rng.choice([-2, -1, 1, 2])))
            c[3] = cell(4, note(deg(mel, 1)), 0xC, 36)
        elif variant and r % 4 == 3:
            c[3] = cell(0, 0, 0xC, 0)
        rows.append(b"".join(c))
    return b"".join(rows)


def build():
    rng = random.Random(2026)
    smp = hm.instruments(seed=5)
    pats = [pattern(rng, 0), pattern(rng, 1), pattern(rng, 1)]
    return hm.build_mod("@ID@", smp, pats, [0, 1, 0, 2], speed=5)


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)
    with open(os.path.join(outdir, "tune.mod"), "wb") as f:
        f.write(build())


if __name__ == "__main__":
    main()
