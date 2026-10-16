#!/usr/bin/env python3
"""Kitty Kibble alternative music: two 4-channel ProTracker MODs written with the house music kit
(games/common/tools/house_music.py: the MOD writer, periods and drums; docs/art-direction.md "Audio"), at the house
mix (sample volumes <= 40). The owner picks; the game plays tune.mod (make_music.py, "Kibble Swing").

  alt1  "Whisker Waltz": a lilting jazz waltz in Bb, speed 2 (3/4, about 190 beats a minute, one bar a breath).
        The bass on one, the pluck's chord on two and three (oom-pah-pah), a soft brush, the breathy lead singing
        long notes. A: Bbmaj7 Gm7 Cm7 F7 (twice), B: Ebmaj7 Edim7 Bb/D G7 Cm7 F7 Bb6 F7, then again.
  alt2  "Catnip Boogie": a bouncy 12-bar blues shuffle in C, speed 3 (swung eighths, 125 BPM). A boogie-woogie bass
        walking up and down the chord, kick and snare, the pluck's stabs on the off-beats, the square lead with a
        blues riff (the flat third and seventh) that answers itself; the last two bars turn round.

    make_music_alt.py              -> ../music-alt/alt1.mod, alt2.mod (previews; the game is unchanged)
    make_music_alt.py OUTDIR N     -> OUTDIR/tune.mod: alternative N (1 or 2), for tools/build_assets.py

To switch, in tools/build_assets.py call make_music_alt.py with OUTDIR and N instead of make_music.py.

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/kittykibble/LICENSE.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note  # noqa: E402

_ = None
PAD, BASS, DROP, LEAD, HAT, KICK, SNARE, SQUARE = 1, 2, 3, 4, 5, 6, 7, 8


def empty(rows):
    return [[cell(), cell(), cell(), cell()] for _r in range(rows)]


def flat(rows):
    rows = rows + empty(64 - len(rows))
    return b"".join(b"".join(r) for r in rows)


# ---- alt1: Whisker Waltz ------------------------------------------------------------------------------------------
def waltz():
    key = 10                                          # Bb
    ch = {"Bbmaj7": (0, [0, 4, 7, 11]), "Gm7": (9, [0, 3, 7, 10]), "Cm7": (2, [0, 3, 7, 10]),
          "F7": (7, [0, 4, 7, 10]), "Ebmaj7": (5, [0, 4, 7, 11]), "Edim7": (6, [0, 3, 6, 9]),
          "Bb/D": (4, [-4, -1, 3, 6]), "G7": (9, [0, 4, 7, 10]), "Bb6": (0, [0, 4, 7, 9])}
    a = [("Bbmaj7", [14, _, 12]), ("Gm7", [10, _, _]), ("Cm7", [12, 14, 15]), ("F7", [14, _, 9]),
         ("Bbmaj7", [14, _, 17]), ("Gm7", [16, 14, 10]), ("Cm7", [12, _, 9]), ("F7", [10, _, _])]
    b = [("Ebmaj7", [7, 9, 10]), ("Edim7", [12, _, 9]), ("Bb/D", [10, _, 7]), ("G7", [11, 14, 17]),
         ("Cm7", [15, _, 14]), ("F7", [12, _, 9]), ("Bb6", [7, _, _]), ("F7", [9, 10, 12])]

    def s(off, octave):
        return key + 12 * octave + off - 12

    def bar(chord, tune):
        root, iv = ch[chord]
        rows = empty(24)
        rows[0][1] = cell(BASS, note(s(root % 12, 1)), 0xC, 40)
        for beat in (1, 2):
            rows[beat * 8][0] = cell(DROP, note(s((root + iv[1 + beat % 2]) % 12, 2)), 0xC, 24)
            rows[beat * 8][2] = cell(HAT, note(s(7, 3)), 0xC, 14)
        rows[0][2] = cell(HAT, note(s(7, 3)), 0xC, 20)
        for beat, n in enumerate(tune):
            if n is not None:
                rows[beat * 8][3] = cell(LEAD, note(s(n, 1)), 0xC, 40)
        return rows

    def pat(bars):
        rows = []
        for c, t in bars:
            rows += bar(c, t)
        rows[47][2] = cell(0, 0, 0xD, 0)                 # 48 rows: two bars of 3/4
        return flat(rows)
    secs = a + a + b + a
    pats = [pat(secs[i:i + 2]) for i in range(0, len(secs), 2)]
    smp = hm.instruments(seed=11) + [("brush", [int(v * 0.5) for v in hm.snare(seed=13)], 26, 0, 0)]
    return hm.build_mod("whiskerwaltz", smp, pats, list(range(len(pats))), speed=2)


# ---- alt2: Catnip Boogie ---------------------------------------------------------------------------------------------
def boogie():
    key = 0                                           # C
    blues = [("C7", 0), ("C7", 0), ("C7", 0), ("C7", 0), ("F7", 5), ("F7", 5), ("C7", 0), ("C7", 0),
             ("G7", 7), ("F7", 5), ("C7", 0), ("G7", 7)]
    walk = [0, 4, 7, 9, 10, 9, 7, 4]                  # the boogie-woogie bass, up and down the chord
    riff1 = [7, _, 10, 7, 3, 4, 0, _]                 # G Bb G Eb E C: the blues riff ...
    riff2 = [12, 10, 7, _, 3, _, 0, _]                # ... and its answer
    turn = [7, 6, 5, 4, 2, _, _, _]

    def s(off, octave):
        return key + 12 * octave + off

    def row_of(e):
        return (e // 2) * 8 + (5 if e % 2 else 0)

    def bar(i):
        name, root = blues[i]
        rows = empty(32)
        for e in range(8):
            rows[row_of(e)][1] = cell(BASS, note(s((root + walk[e]) % 12, 0) + (12 if root + walk[e] >= 12 else 0)),
                                      0xC, 36 if e % 2 == 0 else 28)
        for beat in range(4):
            r = beat * 8
            rows[r][2] = cell(KICK if beat % 2 == 0 else SNARE, note(s(0, 1) if beat % 2 == 0 else s(7, 1)), 0xC,
                              34 if beat % 2 == 0 else 26)
        third = 4 if name != "G7" else 4
        for r in (13, 29):                            # stabs on the swung "and" of 2 and 4
            rows[r][0] = cell(DROP, note(s((root + third) % 12, 2)), 0xC, 24)
            rows[r + 1][0] = cell(DROP, note(s((root + 10) % 12, 2)), 0xC, 18)
        tune = turn if i == 11 else (riff1 if i % 2 == 0 else riff2)
        if i in (4, 5, 9):                            # over F7: the riff a fourth up
            tune = [None if n is None else n + 5 for n in tune]
        for e, n in enumerate(tune):
            if n is not None:
                rows[row_of(e)][3] = cell(SQUARE, note(s(n % 24, 1)), 0xC, 30 if e % 2 == 0 else 24)
        return rows

    pats = [flat(bar(i) + bar(i + 1)) for i in range(0, 12, 2)]
    smp = hm.instruments(seed=12) + [("hat", hm.hat(seed=17), 20, 0, 0), ("kick", hm.kick(), 38, 0, 0),
                                     ("snare", hm.snare(seed=19), 28, 0, 0),
                                     ("square", hm.square_lead(0.25), 22, 0, 32)]
    return hm.build_mod("catnipboogie", smp, pats, list(range(len(pats))), speed=3)


ALTS = {1: waltz, 2: boogie}


def main():
    if len(sys.argv) > 2:
        outdir, n = sys.argv[1], int(sys.argv[2])
        os.makedirs(outdir, exist_ok=True)
        with open(os.path.join(outdir, "tune.mod"), "wb") as f:
            f.write(ALTS[n]())
        return
    out = os.path.join(HERE, "..", "music-alt")
    os.makedirs(out, exist_ok=True)
    for n, fn in ALTS.items():
        with open(os.path.join(out, "alt%d.mod" % n), "wb") as f:
            f.write(fn())


if __name__ == "__main__":
    main()
