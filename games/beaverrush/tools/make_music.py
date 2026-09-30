#!/usr/bin/env python3
"""Beaver Rush music: a lively banjo/folk loop in G major, written with the house music kit
(games/common/tools/house_music.py): 4 channels (0 banjo rolls, 1 double bass "boom-chick", 2 brushed drums,
3 fiddle), speed 6, 4 rows per beat.

The tune has two 8-bar sections (A: the fiddle tune over the banjo; B: the banjo leads, the fiddle answers),
each a module of its own, and each is rendered at four tempos (BPM 128, 144, 150, 160). The game plays A, B,
A, B... and picks the tempo of the next section from the timer's drain level, so the music speeds up
subtly as the game does (src/sfx.c music_update()).

    make_music.py OUTDIR      -> OUTDIR/a0.mod .. a3.mod, b0.mod .. b3.mod

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/beaverrush/LICENSE.
"""
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "games", "common", "tools"))
import house_music as hm  # noqa: E402

TEMPOS = [128, 144, 150, 160]           # 115200 / BPM frames per section: whole numbers (src/sfx.c)
KEY = 7                                   # G
S = hm.MAJOR


def banjo():
    """a plucked banjo string: Karplus-Strong on a 32-sample period (C-2 = middle C), bright attack, a drum-head
    twang (a fast pitch drop in the first few ms) and a quick decay (one-shot)."""
    rng = random.Random(21)
    n = 32
    buf = [rng.uniform(-1, 1) for _ in range(n)]
    out = []
    for i in range(2600):
        j = i % n
        v = buf[j]
        nxt = 0.5 * (buf[j] + buf[(j + 1) % n]) * 0.996
        buf[j] = nxt if i > 40 else buf[j]
        twang = 0.35 * math.sin(2 * math.pi * i / 16.0) * math.exp(-i / 60.0)
        out.append(hm.s8(105 * (v + twang) * math.exp(-i / 900.0)))
    return out


def dbass():
    """a plucked double bass: a round sine with its octave, a soft thump, a medium decay (one-shot)"""
    out = []
    for i in range(3000):
        ph = 2 * math.pi * i / 64.0
        v = 0.85 * math.sin(ph) + 0.25 * math.sin(2 * ph) + 0.1 * math.sin(3 * ph)
        out.append(hm.s8(100 * v * math.exp(-i / 1300.0) * min(1.0, i / 12.0)))
    return out


def fiddle():
    """a soft bowed tone: a band-limited saw (5 partials), looped"""
    return [hm.s8(34 * sum(math.sin(2 * math.pi * k * i / 32.0) / k for k in range(1, 6))) for i in range(32)]


def samples():
    return [("banjo", banjo(), 34, 0, 0), ("bass", dbass(), 40, 0, 0), ("kick", hm.kick(), 30, 0, 0),
            ("brush", hm.snare(13), 18, 0, 0), ("hat", hm.hat(17), 12, 0, 0), ("fiddle", fiddle(), 20, 0, 32)]


BANJO, BASS, KICK, BRUSH, HAT, FIDDLE = 1, 2, 3, 4, 5, 6


def n(d, octave):
    return hm.note(hm.deg(d, octave, KEY, S))


# chords per bar as scale degrees (G=0, Am=1, Bm=2, C=3, D=4, Em=5)
CHORDS_A = [0, 0, 3, 0, 5, 3, 4, 0]
CHORDS_B = [3, 0, 4, 5, 3, 0, 4, 0]
# the fiddle tune of section A: (bar, row in bar, degree, octave, length in rows); a pentatonic folk line
TUNE_A = [(0, 0, 4, 1, 4), (0, 4, 2, 1, 2), (0, 6, 4, 1, 2), (0, 8, 7, 1, 6), (0, 14, 5, 1, 2),
          (1, 0, 4, 1, 4), (1, 4, 2, 1, 4), (1, 8, 0, 1, 8),
          (2, 0, 3, 1, 4), (2, 4, 5, 1, 4), (2, 8, 7, 1, 4), (2, 12, 5, 1, 4),
          (3, 0, 4, 1, 12), (3, 12, 2, 1, 4),
          (4, 0, 5, 1, 4), (4, 4, 4, 1, 2), (4, 6, 2, 1, 2), (4, 8, 4, 1, 8),
          (5, 0, 3, 1, 4), (5, 4, 2, 1, 4), (5, 8, 0, 1, 8),
          (6, 0, 1, 1, 4), (6, 4, 4, 1, 4), (6, 8, 6, 1, 4), (6, 12, 4, 1, 4),
          (7, 0, 0, 1, 16)]
# section B: the fiddle answers the banjo with long notes
TUNE_B = [(0, 8, 7, 1, 8), (1, 8, 9, 1, 8), (2, 8, 8, 1, 8), (3, 0, 7, 1, 16),
          (4, 8, 5, 1, 8), (5, 8, 4, 1, 8), (6, 0, 6, 1, 8), (6, 8, 8, 1, 8), (7, 0, 7, 1, 16)]
ROLL = [0, 2, 4, 7, 2, 4, 7, 4]           # a forward roll over the chord (root, third, fifth, octave)
ROLL_B = [7, 4, 9, 7, 4, 7, 9, 11]        # higher, the banjo leads


def section(chords, tune, lead_banjo):
    """two 64-row patterns (8 bars of 16 rows)"""
    rng = random.Random(len(tune) * 7 + lead_banjo)
    rows = [[hm.cell(), hm.cell(), hm.cell(), hm.cell()] for _ in range(128)]
    for bar, ch in enumerate(chords):
        base = bar * 16
        for r in range(16):
            row = rows[base + r]
            # banjo: rolls on every 16th (a rest now and then), accents on the beat
            if r % 2 == 0 or lead_banjo or r % 4 == 3:
                roll = ROLL_B if lead_banjo else ROLL
                d = ch + roll[r % 8]
                vol = 44 if r % 4 == 0 else 30 if r % 2 == 0 else 22
                if not (not lead_banjo and rng.random() < 0.12 and r % 4):
                    row[0] = hm.cell(BANJO, n(d, 1), 0xC, vol)
            # bass: boom (root) on 1, chick... the fifth on 3, a walk up into the next bar
            if r == 0:
                row[1] = hm.cell(BASS, n(ch, 0))
            elif r == 8:
                row[1] = hm.cell(BASS, n(ch + 4, 0), 0xC, 36)
            elif r == 12 and bar % 2 == 1:
                nxt = chords[(bar + 1) % len(chords)]
                row[1] = hm.cell(BASS, n(nxt - 1, 0), 0xC, 30)
            # drums: kick 1 and 3, brush 2 and 4, a soft hat on the off-beats
            if r in (0, 8):
                row[2] = hm.cell(KICK, hm.note(12))
            elif r in (4, 12):
                row[2] = hm.cell(BRUSH, hm.note(24))
            elif r % 2 == 0:
                row[2] = hm.cell(HAT, hm.note(30), 0xC, 16 if r % 4 else 22)
    for (bar, r, d, octv, length) in tune:
        i = bar * 16 + r
        rows[i][3] = hm.cell(FIDDLE, n(d, octv), 0xC, 30)
        if length >= 6:
            rows[i + 2][3] = hm.cell(0, 0, 0x4, 0x34)            # vibrato on the long notes
        end = i + length
        if end < 128 and rows[end][3] == hm.cell():
            rows[end][3] = hm.cell(0, 0, 0xC, 0)                # note off
    data = b"".join(b"".join(row) for row in rows)
    return [data[:1024], data[1024:]]


def build(name, pats, tempo):
    return hm.build_mod("beaver rush " + name, samples(), pats, [0, 1], tempo=tempo)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(out, exist_ok=True)
    secs = {"a": section(CHORDS_A, TUNE_A, 0), "b": section(CHORDS_B, TUNE_B, 1)}
    for name, pats in secs.items():
        for k, t in enumerate(TEMPOS):
            with open(os.path.join(out, "%s%d.mod" % (name, k)), "wb") as f:
                f.write(build(name, pats, t))


if __name__ == "__main__":
    main()
