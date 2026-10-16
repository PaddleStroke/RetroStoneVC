#!/usr/bin/env python3
"""Kitty Kibble music: "Kibble Swing", a playful little jazz tune: a 4-channel ProTracker MOD with the house
instrument set (games/common/tools/house_music.py; docs/art-direction.md "Audio") and a ride cymbal.

F major, swung eighths (each beat is 8 rows at speed 3, 125 BPM: the off-beat eighth falls on row 5, a 5:3 swing),
A A B A (16 bars a section's half: about 61 s, then again). Channel 0 the house pluck comping in a Charleston rhythm
(the chord's third on beat 1, its sixth or seventh on the "and" of 2), 1 a walking bass (root, third, fifth and a
chromatic step into the next chord), 2 the ride cymbal (ding, ding-ga), 3 the breathy lead with the tune: a curious
kitten tiptoeing (A), a chromatic slide down (D7), blue notes on the C7, and a bridge (B) that climbs through Bb6,
B diminished, F and D7 before it turns home. The harmony is an old-style turnaround (I6 VI7 ii7 V7).

    make_music.py OUTDIR       -> OUTDIR/tune.mod

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/kittykibble/LICENSE.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note  # noqa: E402

KEY = 5                                  # F (semitone 5 from C)
PAD, BASS, DROP, LEAD, RIDE = 1, 2, 3, 4, 5
_ = None
# chords: (root in semitones above F, intervals)
CH = {"F6": (0, [0, 4, 7, 9]), "D7": (9, [0, 4, 7, 10]), "Gm7": (2, [0, 3, 7, 10]), "C7": (7, [0, 4, 7, 10]),
      "Bb6": (5, [0, 4, 7, 9]), "Bo7": (6, [0, 3, 6, 9]), "G7": (2, [0, 4, 7, 10])}
# one bar a line: its chords (one or two), eight swung eighths of the tune (semitones above F, octave 1; _: hold)
A = [(["F6"], [4, _, 2, 4, 7, _, 4, _]),
     (["D7"], [9, 8, 7, _, 4, _, 1, _]),
     (["Gm7"], [2, _, 5, 9, 12, _, 9, 5]),
     (["C7"], [7, _, 11, 10, 9, _, 7, _]),
     (["F6"], [4, _, 2, 4, 9, _, 7, _]),
     (["D7"], [13, _, 12, 9, 7, _, 4, _]),
     (["Gm7", "C7"], [5, 4, 2, _, 11, _, 7, _]),
     (["F6"], [0, _, _, _, _, _, 7, 9])]
A_END = [(["Gm7", "C7"], [5, 4, 2, _, 11, _, 10, _]),
         (["F6"], [12, _, 9, _, 0, _, _, _])]
B = [(["Bb6"], [9, _, 7, 5, 2, _, 5, _]),
     (["Bo7"], [6, _, 9, 12, 15, _, 12, _]),
     (["F6"], [16, _, 14, 12, 9, _, 7, _]),
     (["D7"], [9, _, 13, _, 16, _, 15, _]),
     (["G7"], [14, _, 12, 9, 6, _, 2, _]),
     (["C7"], [7, _, 11, 14, 17, _, 16, 14]),
     (["F6", "D7"], [12, _, 9, 7, 9, _, 13, _]),
     (["Gm7", "C7"], [14, _, 17, 14, 11, _, 7, _])]


def semi(off, octave):
    return KEY + 12 * octave + off


def eighth_row(e):
    """The row of swung eighth e (0..7) in a 32-row bar: on the beat, then 5 rows later."""
    return (e // 2) * 8 + (5 if e % 2 else 0)


def bar_rows(chords, tune, next_root):
    rows = [[cell(), cell(), cell(), cell()] for _r in range(32)]
    halves = chords if len(chords) == 2 else chords * 2
    for beat in range(4):
        r0 = beat * 8
        root, iv = CH[halves[beat // 2]]
        # the walking bass: root, third, fifth, a chromatic step into the next chord (two chords: root, fifth each)
        third = iv[2] if len(chords) == 2 else iv[1]
        b = [root, root + third, root + iv[2], next_root - 1][beat] if len(chords) == 1 else \
            [root, root + iv[2], root, next_root - 1][beat]
        rows[r0][1] = cell(BASS, note(semi(b % 12, 0)), 0xC, 40 if beat == 0 else 32)
        # the ride: ding on every beat, "ga" on the swung off-beat of 2 and 4
        rows[r0][2] = cell(RIDE, note(semi(7, 2)), 0xC, 16 if beat % 2 == 0 else 22)
        if beat % 2 == 1:
            rows[r0 + 5][2] = cell(RIDE, note(semi(7, 2)), 0xC, 12)
    # the comping (a Charleston): the third on the beat, the sixth / seventh on the swung "and" after it
    if len(chords) == 2:
        for half, chord in enumerate(chords):
            root, iv = CH[chord]
            rows[16 * half][0] = cell(DROP, note(semi((root + iv[1]) % 12, 1)), 0xC, 28)
            rows[16 * half + 13][0] = cell(DROP, note(semi((root + iv[3]) % 12, 1)), 0xC, 22)
    else:
        root, iv = CH[chords[0]]
        rows[0][0] = cell(DROP, note(semi((root + iv[1]) % 12, 1)), 0xC, 28)
        rows[13][0] = cell(DROP, note(semi((root + iv[3]) % 12, 1)), 0xC, 22)
        rows[29][0] = cell(DROP, note(semi((root + iv[2]) % 12, 1)), 0xC, 16)
    for e, n in enumerate(tune):
        if n is not None:
            rows[eighth_row(e)][3] = cell(LEAD, note(semi(n, 1)), 0xC, 40 if e % 2 == 0 else 32)
    return rows


def pattern(bars, next_roots):
    rows = []
    for (chords, tune), nr in zip(bars, next_roots):
        rows += bar_rows(chords, tune, nr)
    return b"".join(b"".join(r) for r in rows)


def section(bars, after):
    """Patterns of two bars; after: the root of the chord after the section (the bass's last step)."""
    roots = [CH[b[0][0]][0] for b in bars[1:]] + [after]
    return [pattern(bars[i:i + 2], roots[i:i + 2]) for i in range(0, len(bars), 2)]


def build():
    smp = hm.instruments(seed=9) + [("ride", [int(v * 0.6) for v in hm.hat(seed=21)], 30, 0, 0)]
    a1 = section(A, CH["F6"][0])                      # A (ends on F, back to A)
    a2 = section(A[:6] + A_END, CH["Bb6"][0])          # A, ending into the bridge
    b = section(B, CH["F6"][0])
    a3 = section(A[:6] + A_END, CH["F6"][0])
    pats = a1 + a2[3:] + b + a3[3:]                   # a2 and a3 share A's first 6 bars (patterns 0-2)
    # patterns: 0-3 A, 4 A's ending (into B), 5-8 B, 9 A's ending (home)
    orders = [0, 1, 2, 3, 0, 1, 2, 4, 5, 6, 7, 8, 0, 1, 2, 9]
    return hm.build_mod("kittykibble", smp, pats, orders, speed=3)


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)
    with open(os.path.join(outdir, "tune.mod"), "wb") as f:
        f.write(build())


if __name__ == "__main__":
    main()
