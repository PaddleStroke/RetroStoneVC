#!/usr/bin/env python3
"""Nutty Branch music: three 4-channel ProTracker MODs with the house instrument set (games/common/tools/
house_music.py; docs/art-direction.md "Audio"), one per part of the day: the tune evolves over the distance (the game
switches module with the time-of-day band, src/main.c):

  tune.mod     spring and summer days: a playful woodland tune in G major, speed 5 (an action game), pluck
               arpeggios, a bouncy bass, the pad and a whistled lead; the second half adds a counter-melody
  dusk.mod     autumn afternoon and dusk: the same tune in E minor (the relative minor), a slower walk
  night.mod    night and winter dawn: E dorian, sparse plucks like drops of dew, the lead an octave down

    make_music.py OUTDIR       -> OUTDIR/tune.mod, dusk.mod, night.mod

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
"""
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note  # noqa: E402

# the tune: scale degrees (None = rest), 2 rows each, 32 per pattern: a skipping squirrel figure
MELODY = [
    [4, None, 5, 4, 2, None, 0, None, 1, 2, 4, None, 2, None, None, None,
     4, None, 5, 7, 6, None, 4, None, 5, 4, 2, None, 1, None, None, None],
    [7, None, 6, 7, 9, None, 7, None, 6, 4, 5, None, 4, None, 2, None,
     4, 5, 4, 2, 1, None, 2, None, 0, None, None, None, None, None, None, None],
]
COUNTER = [2, None, None, None, 0, None, None, None, 1, None, None, None, -1, None, None, None]


def pattern(rng, key, scale, chords, mel, counter=False, sparse=False, lead_oct=1):
    def deg(d, octave):
        return hm.deg(d, octave, key, scale)
    rows = []
    for r in range(64):
        ch = chords[r // 16]
        c = [cell(), cell(), cell(), cell()]
        step = 4 if sparse else 2
        if r % step == 0:                                        # plucked arpeggio (a drop of dew when sparse)
            c[0] = cell(3, note(deg(ch + [0, 2, 4, 7][(r // step) % 4], 2)), 0xC, 30 if r % 8 else 38)
        if r % 8 == 0:                                           # bass: the root, the fifth on the half bar
            c[1] = cell(2, note(deg(ch + (4 if r % 16 else 0), 0)))
        elif not sparse and r % 8 == 6:
            c[1] = cell(2, note(deg(ch + 2, 0)), 0xC, 24)      # a bounce
        if r % 32 == 0:                                          # pad: the chord's third
            c[2] = cell(1, note(deg(ch + 2, 1)))
        if counter and r % 4 == 2 and COUNTER[(r // 4) % 16] is not None:
            c[2] = cell(1, note(deg(ch + COUNTER[(r // 4) % 16], 1)), 0xC, 18)
        m = mel[r // 2] if r % 2 == 0 else None
        if m is not None:
            c[3] = cell(4, note(deg(m, lead_oct)), 0xC, 34)
        elif r % 2 == 1 and mel[r // 2] is not None and rng.random() < 0.5:
            c[3] = cell(0, 0, 0xC, 0)                            # staccato
        rows.append(b"".join(c))
    return b"".join(rows)


def song(name, key, scale, chords, speed, sparse=False, lead_oct=1):
    rng = random.Random(2026)
    smp = hm.instruments(seed=5)
    pats = [pattern(rng, key, scale, chords, MELODY[0], False, sparse, lead_oct),
            pattern(rng, key, scale, chords, MELODY[1], False, sparse, lead_oct),
            pattern(rng, key, scale, chords, MELODY[0], True, sparse, lead_oct),
            pattern(rng, key, scale, chords, MELODY[1], True, sparse, lead_oct)]
    return hm.build_mod(name, smp, pats, [0, 1, 2, 3], speed=speed)


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)
    tunes = {
        "tune.mod": song("nuttybranch day", 7, hm.MAJOR, [0, 3, 4, 0], 5),            # G: I IV V I
        "dusk.mod": song("nuttybranch dusk", 4, hm.MINOR, [0, 5, 3, 4], 6),           # E minor: i VI iv v
        "night.mod": song("nuttybranch night", 4, hm.DORIAN, [0, 3, 0, 6], 7, sparse=True, lead_oct=0),
    }
    for f, data in tunes.items():
        with open(os.path.join(outdir, f), "wb") as fh:
            fh.write(data)


if __name__ == "__main__":
    main()
