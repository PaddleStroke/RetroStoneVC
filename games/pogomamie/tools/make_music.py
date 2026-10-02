#!/usr/bin/env python3
"""Pogo Mamie music: Paris musette waltzes, one per district and one for the night, as small 4-channel
ProTracker MODs (played by libxmp-lite in the SDK).

The samples are synthesised here:
  - the musette accordion: three detuned reeds (one in tune, one sharp, one flat) in one 2048-sample loop that
    holds 64, 65 and 63 cycles of a reedy wave: the "wet" beating (~8 Hz) of the musette tuning;
  - the bass reed (an octave lower, rounder);
  - the left hand's chord buttons: a just-tuned major (4:5:6) and minor (10:12:15) triad in one loop each;
  - a soft brush tick.
3/4 at 160 beats per minute: 6 rows per bar (eighth notes), speed 6 at 80 BPM ticks, 8 bars per pattern.
Oom (bass) on beat 1, pah-pah (chord stabs) on beats 2 and 3, the melody on top: arpeggios, chromatic
approach notes and turns, the musette's vocabulary, each district in its own key and progression.

    make_music.py OUTDIR       -> OUTDIR/tune_<district>.mod (montmartre, seine, haussmann, eiffel, night)

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pogomamie/LICENSE.
"""
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402  (the house MOD writer, periods and instruments)

PERIODS = hm.PERIODS
s8 = hm.s8


def reed(ph):
    """a reedy wave (odd harmonics strong, a little even), phase in cycles"""
    return (math.sin(2 * math.pi * ph) + 0.12 * math.sin(4 * math.pi * ph + 0.3) + 0.18 * math.sin(6 * math.pi * ph) +
            0.06 * math.sin(8 * math.pi * ph + 1.1) + 0.05 * math.sin(10 * math.pi * ph) + 0.02 * math.sin(14 * math.pi * ph))


def samples():
    acc = []
    for i in range(2048):
        v = reed(i * 64 / 2048.0) + 0.22 * reed(i * 65 / 2048.0 + 0.2) + 0.22 * reed(i * 63 / 2048.0 + 0.6)
        acc.append(s8(v * 24))
    bass = hm.bass()                         # the house round bass
    maj = [s8(15 * (reed(i * 4 / 256.0) + reed(i * 5 / 256.0 + 0.3) + reed(i * 6 / 256.0 + 0.7))) for i in range(256)]
    mnr = [s8(15 * (reed(i * 10 / 640.0) + reed(i * 12 / 640.0 + 0.3) + reed(i * 15 / 640.0 + 0.7))) for i in range(640)]
    rng = random.Random(3)
    tick, lp = [], 0.0
    for i in range(700):
        lp += (rng.uniform(-1, 1) - lp) * 0.55
        tick.append(s8(lp * 90 * math.exp(-i / 90.0)))
    # name, data, volume, loop start, loop length (0 = one-shot)
    return [("musette", acc, 26, 0, 2048), ("bass", bass, 30, 0, 64), ("major", maj, 18, 0, 256),
            ("minor", mnr, 18, 0, 640), ("brush", tick, 12, 0, 0)]


def cell(smp=0, note=None, eff=0, par=0):
    return hm.cell(smp, PERIODS[note] if note is not None else 0, eff, par)


MAJOR = [0, 2, 4, 5, 7, 9, 11]
MINOR = [0, 2, 3, 5, 7, 8, 11]              # harmonic minor: the musette's leading tone

# key (semitone of the tonic, 0 = C), mode, the 8-bar progressions of patterns A and C (degree, quality)
TUNES = {
    "montmartre": (0, MAJOR, [(0, "M"), (0, "M"), (4, "M"), (4, "M"), (4, "M"), (4, "M"), (0, "M"), (0, "M")],
                   [(3, "M"), (3, "M"), (0, "M"), (0, "M"), (1, "m"), (4, "M"), (0, "M"), (0, "M")], 11),
    "seine": (9, MINOR, [(0, "m"), (0, "m"), (4, "M"), (4, "M"), (4, "M"), (4, "M"), (0, "m"), (0, "m")],
              [(3, "m"), (3, "m"), (0, "m"), (0, "m"), (5, "M"), (4, "M"), (0, "m"), (0, "m")], 22),
    "haussmann": (5, MAJOR, [(0, "M"), (5, "m"), (1, "m"), (4, "M"), (0, "M"), (5, "m"), (1, "m"), (4, "M")],
                  [(3, "M"), (3, "M"), (2, "m"), (5, "m"), (1, "m"), (4, "M"), (0, "M"), (0, "M")], 33),
    "eiffel": (7, MAJOR, [(0, "M"), (0, "M"), (3, "M"), (3, "M"), (4, "M"), (4, "M"), (0, "M"), (4, "M")],
               [(5, "m"), (5, "m"), (3, "M"), (0, "M"), (1, "m"), (4, "M"), (0, "M"), (0, "M")], 44),
    "night": (2, MINOR, [(0, "m"), (3, "m"), (4, "M"), (0, "m"), (5, "M"), (3, "m"), (4, "M"), (4, "M")],
              [(3, "m"), (0, "m"), (5, "M"), (4, "M"), (3, "m"), (0, "m"), (4, "M"), (0, "m")], 55),
}


def degree_note(key, mode, deg, octave):
    return key + 12 * octave + mode[deg % 7] + 12 * (deg // 7)


def chord_tones(key, mode, deg):
    return [degree_note(key, mode, deg + k, 0) for k in (0, 2, 4)]


def fit(n, lo, hi):
    while n < lo:
        n += 12
    while n > hi:
        n -= 12
    return n


def melody_bar(rng, key, mode, deg, motif, prev):
    """6 eighth notes (None = hold / rest marker 'R') over the chord of this bar"""
    tones = sorted(fit(t, 14, 30) for t in chord_tones(key, mode, deg))
    hi = [t + 12 for t in tones if t + 12 <= 33]
    pool = sorted(set(tones + hi))
    near = min(pool, key=lambda t: abs(t - prev))
    i = pool.index(near)
    up = lambda k: pool[min(len(pool) - 1, i + k)]  # noqa: E731
    dn = lambda k: pool[max(0, i - k)]  # noqa: E731
    if motif == "arp":                  # an arpeggio up and back
        return [up(0), up(1), up(2), up(3), up(2), up(1)]
    if motif == "arpdown":
        return [up(2), up(1), up(0), dn(1), up(0), None]
    if motif == "chrom":                # chromatic approach from below to a chord tone, held
        t = up(1)
        return [t - 2, t - 1, t, None, up(0), up(2)]
    if motif == "turn":                 # a turn around a chord tone
        t = up(1)
        return [t, t + 1, t, t - 1, t, None]
    if motif == "long":
        return [up(1), None, None, up(0), up(2), None]
    return [up(2), None, up(1), None, up(0), None]      # "call": three long-ish notes


MOTIFS_A = ["long", "call", "arpdown", "long", "call", "long", "arpdown", "call"]
MOTIFS_B = ["call", "long", "arpdown", "call", "long", "arpdown", "long", "call"]


def pattern(name, key, mode, prog, rng, variant, tempo):
    rows = [[cell() for _ in range(4)] for _ in range(64)]
    prev = 24
    motifs = MOTIFS_A if variant == 0 else MOTIFS_B
    if variant == 2:
        motifs = [rng.choice(["call", "long", "arpdown"]) for _ in range(7)] + ["call"]
    for bar, (deg, q) in enumerate(prog):
        r0 = bar * 6
        # oom: the bass on beat 1 (the root, the fifth on even bars)
        root = degree_note(key, mode, deg, 0) % 12
        bass = root if bar % 2 == 0 else (root + 7) % 12
        rows[r0][1] = cell(2, bass, 0xE, 0xC4)
        # pah pah: the chord on beats 2 and 3, short
        chord = fit(root + 12, 19, 30)
        for b in (2, 4):
            rows[r0 + b][2] = cell(3 if q == "M" else 4, chord, 0xE, 0xC3)
        # the brush on 2 and 3, lighter on 3
        rows[r0 + 2][3] = cell(5, 24, 0xC, 24)
        rows[r0 + 4][3] = cell(5, 24, 0xC, 14)
        # the melody
        notes = melody_bar(rng, key, mode, deg, motifs[bar], prev)
        if variant == 1 and bar % 4 == 3:
            notes = [notes[0], None, None, None, None, None]
        for k, n in enumerate(notes):
            if n is not None:
                n = max(0, min(35, n))
                rows[r0 + k][0] = cell(1, n)
                prev = n
    # speed 6 and 80 BPM on the first row (channels 1 and 2 carry them: their notes keep sample defaults)
    rows[0][2] = cell(0, None, 0xF, 6)
    rows[1][2] = cell(0, None, 0xF, tempo)
    rows[47][0] = bytes(rows[47][0][:2]) + bytes([rows[47][0][2] & 0xF0 | 0xD, 0])   # pattern break: 8 bars
    return b"".join(b"".join(r) for r in rows)


def build(name):
    key, mode, prog_a, prog_c, seed = TUNES[name]
    rng = random.Random(seed)
    smp = samples()
    tempo = 60 if name == "night" else 66
    pats = [pattern(name, key, mode, prog_a if i < 4 else prog_c, rng, i % 3, tempo) for i in range(8)]
    return hm.build_mod("pogo mamie " + name[:9], smp, pats, list(range(8)))


NAMES = ["montmartre", "seine", "haussmann", "eiffel", "night"]


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)
    for name in NAMES:
        with open(os.path.join(outdir, "tune_%s.mod" % name), "wb") as f:
            f.write(build(name))


if __name__ == "__main__":
    main()
