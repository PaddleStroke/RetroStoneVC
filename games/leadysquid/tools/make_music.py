#!/usr/bin/env python3
"""Leady Squid music: a gentle underwater loop, a small 4-channel ProTracker MOD
(played by libxmp-lite in the SDK). Synthesised samples: a soft sine pad, a
round bass, a water-drop pluck and a breathy lead; slow tempo, D dorian.

    make_music.py OUTDIR       -> OUTDIR/tune.mod

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/leadysquid/LICENSE.
"""
import math
import os
import random
import struct
import sys

# ProTracker periods, C-1 .. B-3
PERIODS = [856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
           428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
           214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113]


def s8(v):
    return max(-128, min(127, int(round(v))))


def samples():
    rng = random.Random(5)
    pad = [s8(38 * math.sin(2 * math.pi * i / 64) + 12 * math.sin(4 * math.pi * i / 64 + 0.5)) for i in range(64)]
    bass = [s8(70 * math.sin(2 * math.pi * i / 64) + 14 * math.sin(4 * math.pi * i / 64)) for i in range(64)]
    drop, ph = [], 0.0
    for i in range(2400):                      # a pluck: sine, a small upward bend, fast decay
        ph += (1.0 / 16) * (1 + 0.25 * math.exp(-i / 120.0))
        drop.append(s8(100 * math.sin(2 * math.pi * ph) * math.exp(-i / 520.0)))
    lead = [s8(44 * math.sin(2 * math.pi * i / 32) + rng.uniform(-3, 3)) for i in range(32)]
    # name, data, volume, loop start, loop length (samples; 0 = no loop)
    return [("pad", pad, 22, 0, 64), ("bass", bass, 40, 0, 64), ("drop", drop, 34, 0, 0), ("lead", lead, 20, 0, 32)]


KEY, SCALE = 2, [0, 2, 3, 5, 7, 9, 10]          # D dorian
CHORDS = [0, 6, 5, 6]                            # Dm, C, Bb, C (scale degrees)


def note(n):
    while n < 0:
        n += 12
    while n >= len(PERIODS):
        n -= 12
    return PERIODS[n]


def deg(d, octave):
    return KEY + 12 * octave + SCALE[d % 7] + 12 * (d // 7)


def cell(smp=0, period=0, eff=0, par=0):
    return bytes([(smp & 0xF0) | (period >> 8), period & 0xFF, ((smp & 0x0F) << 4) | eff, par])


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
    smp = samples()
    out = bytearray()
    out += b"leady squid".ljust(20, b"\0")
    for i in range(31):
        if i < len(smp):
            n, data, vol, ls, ll = smp[i]
            if len(data) % 2:
                data = data + [0]
            out += n.encode()[:22].ljust(22, b"\0")
            out += struct.pack(">HBBHH", len(data) // 2, 0, vol, ls // 2, (ll // 2) if ll else 1)
        else:
            out += b"\0" * 22 + struct.pack(">HBBHH", 0, 0, 0, 0, 1)
    orders = [0, 1, 0, 2]
    out += bytes([len(orders), 127])
    out += bytes(orders + [0] * (128 - len(orders)))
    out += b"M.K."
    pats = [pattern(rng, 0), pattern(rng, 1), pattern(rng, 1)]
    p0 = bytearray(pats[0])                      # speed 8 on the first row (channel 4, effect F)
    p0[12:16] = bytes([p0[12], p0[13], (p0[14] & 0xF0) | 0xF, 8])
    pats[0] = bytes(p0)
    for p in pats:
        out += p
    for n, data, vol, ls, ll in smp:
        if len(data) % 2:
            data = data + [0]
        out += bytes((v + 256) % 256 for v in data)
    return bytes(out)


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)
    with open(os.path.join(outdir, "tune.mod"), "wb") as f:
        f.write(build())


if __name__ == "__main__":
    main()
