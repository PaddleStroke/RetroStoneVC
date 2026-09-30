#!/usr/bin/env python3
"""Pancake Tower music: a cosy jazzy kitchen loop that evolves with the altitude, three 4-channel ProTracker
MODs with the house instruments (games/common/tools/house_music.py; docs/art-direction.md "Audio"):

    kitchen.mod   swung (speed 7/5 on alternate rows), a walking bass, arpeggiated seventh chords on the "and"
                  of 2 and 4 (Charleston comping), a vibraphone-like pluck melody, brushed hi-hats
    sky.mod       the same tune, airy: held pad chords, the breathy lead on the melody, a lighter bass, no drums
    space.mod     the same chords, spacey: slow (speed 8), a deep pad, an echoing pluck arpeggio (each note
                  repeated softer: a tracker echo), the bass on the roots only

The three share the key (F major), the chords (ii-V-I-VI: Gm7 C7 Fmaj7 D7) and the tempo grid, so the game
can crossfade from one to the next when the scenery changes.

    make_music.py OUTDIR       -> OUTDIR/kitchen.mod, sky.mod, space.mod

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pancaketower/LICENSE.
"""
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note  # noqa: E402

KEY = 5                                   # F
SCALE = hm.MAJOR
# the chords, 2 bars each (8 beats = 16 rows): root degree, and the arpeggio (semitones above the root) of the
# chord's 3rd and 7th: Gm7 (b3, b7), C7 (3, b7), Fmaj7 (3, 7), D7 (3, b7)
CHORDS = [(1, 0x3A), (4, 0x4A), (0, 0x4B), (5, 0x4A)]
CHORD_ROOTS = [KEY + 2, KEY + 7, KEY + 0, KEY + 9]         # semitones: G, C, F, D (octave 0)
SMP_PAD, SMP_BASS, SMP_DROP, SMP_LEAD, SMP_HAT = 1, 2, 3, 4, 5


def deg(d, octave):
    return hm.deg(d, octave, KEY, SCALE)


def walk(ch, beat, rng):
    """the walking bass: the root on beat 1 of the bar, chord tones and passing notes in between (semitones)"""
    root = CHORD_ROOTS[ch]
    third = 3 if ch == 0 else 4
    nxt = CHORD_ROOTS[(ch + 1) % 4]
    b = beat % 8
    if b == 0 or b == 4:
        return root
    if b == 7:                                             # a chromatic approach to the next root
        return nxt + (1 if rng.random() < 0.5 else -1)
    return root + [0, third, 7, 9, 7, third, 10][b % 7]


MELODY = [                                                   # scale degrees (None = rest), one per beat
    4, None, 5, 4, 2, None, 1, None,
    3, None, 4, 3, 1, None, 0, None,
    2, None, 4, 6, 7, None, 6, 4,
    5, None, 4, 2, 3, None, None, None,
]


def kitchen_pattern(rng, variant):
    rows = []
    for r in range(64):
        beat, half = r // 2, r % 2
        ch = (r // 16) % 4
        c = [cell(), cell(), cell(), cell()]
        speed = 7 if half == 0 else 5                        # the swing: a long and a short row
        # channel 1: the walking bass on each beat, the swing speed on every row
        if half == 0:
            c[1] = cell(SMP_BASS, note(walk(ch, beat, rng)), 0xF, speed)
        else:
            c[1] = cell(0, 0, 0xF, speed)
        # channel 2: comping: the chord (arpeggio effect) on the "and" of 2 and 4, short
        if (beat % 4 in (1, 3)) and half == 1:
            c[2] = cell(SMP_PAD, note(CHORD_ROOTS[ch] + 12), 0x0, CHORDS[ch][1])
        elif (beat % 4 in (2, 0)) and half == 0 and r > 0:
            c[2] = cell(0, 0, 0xC, 0)                        # cut the chord (a stab)
        # channel 0: the vibraphone melody (the pluck), in variant 1 with a little fill
        m = MELODY[beat % 32] if variant == 0 else MELODY[(beat + 16) % 32]
        if half == 0 and m is not None:
            c[0] = cell(SMP_DROP, note(deg(m, 2)), 0xC, 40 if beat % 2 == 0 else 32)
        elif variant and half == 1 and beat % 8 == 5:
            c[0] = cell(SMP_DROP, note(deg(rng.choice([2, 4, 5]), 2)), 0xC, 24)
        # channel 3: the brushes: every row, beats 2 and 4 louder
        vol = 20 if (half == 0 and beat % 2 == 1) else 10 if half == 0 else 7
        c[3] = cell(SMP_HAT, note(KEY + 24), 0xC, vol)
        rows.append(b"".join(c))
    return b"".join(rows)


def sky_pattern(rng, variant):
    rows = []
    for r in range(64):
        beat, half = r // 2, r % 2
        ch = (r // 16) % 4
        c = [cell(), cell(), cell(), cell()]
        speed = 7 if half == 0 else 5
        if half == 0 and beat % 4 in (0, 2):                 # a lighter bass: two notes a bar
            c[1] = cell(SMP_BASS, note(walk(ch, beat, rng)), 0xF, speed)
        else:
            c[1] = cell(0, 0, 0xF, speed)
        if half == 0 and beat % 8 == 0:                      # held pad chords (slow arpeggio shimmer)
            c[2] = cell(SMP_PAD, note(CHORD_ROOTS[ch] + 12), 0x0, CHORDS[ch][1])
        m = MELODY[(beat + (16 if variant else 0)) % 32]
        if half == 0 and m is not None:                      # the breathy lead on the melody (a flute)
            c[3] = cell(SMP_LEAD, note(deg(m, 2)), 0xC, 30)
        if half == 1 and beat % 4 == 3:
            c[0] = cell(SMP_DROP, note(deg(rng.choice([4, 6, 7]), 2)), 0xC, 18)   # a sparkle
        rows.append(b"".join(c))
    return b"".join(rows)


def space_pattern(rng, variant):
    rows = []
    for r in range(64):
        beat, half = r // 2, r % 2
        ch = (r // 16) % 4
        c = [cell(), cell(), cell(), cell()]
        if r % 16 == 0:
            c[1] = cell(SMP_BASS, note(CHORD_ROOTS[ch]), 0xC, 30)
            c[2] = cell(SMP_PAD, note(CHORD_ROOTS[ch] + 12), 0x0, CHORDS[ch][1])
        elif r == 1:
            c[1] = cell(0, 0, 0xF, 8)
        # the echoing arpeggio: a chord tone every 4 rows, repeated 2 rows later softer
        tones = [0, 7, 12 + (3 if ch == 0 else 4), 12 + 10]
        if r % 4 == 0:
            c[0] = cell(SMP_DROP, note(CHORD_ROOTS[ch] + 12 + tones[(r // 4) % 4]), 0xC, 36)
        elif r % 4 == 2:
            c[0] = cell(SMP_DROP, note(CHORD_ROOTS[ch] + 12 + tones[(r // 4) % 4]), 0xC, 14)
        m = MELODY[(beat + 8) % 32]
        if variant and half == 0 and beat % 4 == 0 and m is not None:
            c[3] = cell(SMP_LEAD, note(deg(m, 1)), 0xC, 22)
        rows.append(b"".join(c))
    return b"".join(rows)


def samples():
    smp = hm.instruments(seed=5)                             # pad, bass, drop (a pluck), lead
    smp.append(("brush", hm.hat(seed=13), 24, 0, 0))
    return smp


def build(kind):
    rng = random.Random({"kitchen": 1, "sky": 2, "space": 3}[kind])
    fn = {"kitchen": kitchen_pattern, "sky": sky_pattern, "space": space_pattern}[kind]
    pats = [fn(rng, 0), fn(rng, 1)]
    speed = None if kind != "space" else 8
    return hm.build_mod("pancake " + kind, samples(), pats, [0, 1, 0, 1], speed=speed)


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)
    for kind in ("kitchen", "sky", "space"):
        with open(os.path.join(outdir, kind + ".mod"), "wb") as f:
            f.write(build(kind))


if __name__ == "__main__":
    main()
