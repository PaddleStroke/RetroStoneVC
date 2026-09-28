#!/usr/bin/env python3
"""Placeholder music for Bomber Mole: small 4-channel ProTracker MODs.

One loop per season plus the title, synthesised samples (square lead,
triangle bass, pad, kick, snare, hat) and a deterministic tune built from a
chord progression. Played by libxmp-lite in the SDK.

    make_music.py OUTDIR       -> OUTDIR/<name>.mod

All rights reserved, 8BCraft.
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
    rng = random.Random(7)
    sq = [s8(90 if i < 8 else -90) for i in range(32)]                      # 25% square, 32-sample loop
    tri = [s8(110 * (1 - abs((i / 32.0) - 1))) - 55 for i in range(64)]      # triangle, 64-sample loop
    pad = [s8(45 * math.sin(2 * math.pi * i / 32) + 25 * math.sin(6 * math.pi * i / 32)) for i in range(32)]
    kick, ph = [], 0.0
    for i in range(1400):                      # sine with a falling pitch
        ph += 0.06 * (1 - i / 1400.0) + 0.008
        kick.append(s8(120 * math.sin(2 * math.pi * ph) * (1 - i / 1400.0)))
    snare = [s8(rng.uniform(-100, 100) * (1 - i / 1800.0) ** 2) for i in range(1800)]
    hat = [s8(rng.uniform(-70, 70) * (1 - i / 500.0) ** 3) for i in range(500)]
    # name, data, volume, loop start, loop length (in samples; 0 = no loop)
    return [("lead", sq, 40, 0, 32), ("bass", tri, 56, 0, 64), ("pad", pad, 30, 0, 32),
            ("kick", kick, 64, 0, 0), ("snare", snare, 44, 0, 0), ("hat", hat, 30, 0, 0)]


SCALES = {"major": [0, 2, 4, 5, 7, 9, 11], "minor": [0, 2, 3, 5, 7, 8, 10], "dorian": [0, 2, 3, 5, 7, 9, 10]}
SONGS = {
    #          key  scale     speed chords (scale degrees)   seed
    "title":  (0,  "major",  5, [0, 5, 3, 4], 11),
    "spring": (2,  "major",  5, [0, 3, 4, 0], 21),
    "summer": (7,  "major",  4, [0, 4, 5, 3], 31),
    "autumn": (9,  "dorian", 6, [0, 3, 6, 4], 41),
    "winter": (4,  "minor",  7, [0, 5, 2, 6], 51),
    "boss":   (5,  "minor",  4, [0, 0, 5, 6], 61),
}


def note(n):
    """Semitone index from C-1 -> period (clamped to the 3 ProTracker octaves)."""
    while n < 0:
        n += 12
    while n >= len(PERIODS):
        n -= 12
    return PERIODS[n]


def cell(smp=0, period=0, eff=0, par=0):
    return bytes([(smp & 0xF0) | (period >> 8), period & 0xFF, ((smp & 0x0F) << 4) | eff, par])


def pattern(key, scale, chords, rng, variant):
    sc = SCALES[scale]

    def deg(d, octave):
        return key + 12 * octave + sc[d % 7] + 12 * (d // 7)
    rows = []
    melody_deg = 7
    for r in range(64):
        chord = chords[r // 16]
        ch = [cell(), cell(), cell(), cell()]
        # lead: a note every 2 rows, chord tones on strong beats, steps otherwise
        if r % 2 == 0 and not (variant == 1 and r % 16 >= 12 and r % 4 == 2):
            if r % 4 == 0:
                melody_deg = chord + rng.choice([0, 2, 4]) + 7
            else:
                melody_deg += rng.choice([-1, 1, 1, 2, -2])
            ch[0] = cell(1, note(deg(melody_deg, 1)))
        # bass: root on beats, fifth on off-beats
        if r % 4 == 0:
            ch[1] = cell(2, note(deg(chord, 0)))
        elif r % 4 == 2 and variant == 1:
            ch[1] = cell(2, note(deg(chord + 4, 0)))
        # pad / arpeggio
        if r % 8 == 0:
            ch[2] = cell(3, note(deg(chord + 2 * ((r // 8) % 3), 1)), 0xC, 24)
        # drums
        if r % 8 == 0:
            ch[3] = cell(4, 428)
        elif r % 8 == 4:
            ch[3] = cell(5, 428)
        elif r % 2 == 1 and variant == 1:
            ch[3] = cell(6, 428, 0xC, 20)
        rows.append(b"".join(ch))
    return b"".join(rows)


def build(name):
    key, scale, speed, chords, seed = SONGS[name]
    rng = random.Random(seed)
    smp = samples()
    out = bytearray()
    out += ("bombermole " + name).encode()[:20].ljust(20, b"\0")
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
    pats = [pattern(key, scale, chords, rng, 0), pattern(key, scale, chords, rng, 1),
            pattern(key, scale, chords[::-1] if name != "boss" else chords, rng, 1)]
    # set the speed on the first row of pattern 0 (channel 3 effect F)
    p0 = bytearray(pats[0])
    p0[12:16] = bytes([p0[12], p0[13], (p0[14] & 0xF0) | 0xF, speed])
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
    for name in SONGS:
        p = os.path.join(outdir, name + ".mod")
        with open(p, "wb") as f:
            f.write(build(name))


if __name__ == "__main__":
    main()
