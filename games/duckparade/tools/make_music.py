#!/usr/bin/env python3
"""Duck Parade music: a jaunty marching-band loop in D major (a 4-channel ProTracker MOD with the house
music kit, games/common/tools/house_music.py) and the layers the game adds as the parade grows.

The module: channel 0 the fife (the tune), 1 the tuba (oom-pah), 2 the bass drum and the snare, 3 the hi-hat.
Speed 8 at 150 BPM: one tick per frame, a row is exactly 8 frames (a beat is 4 rows: 112.5 beats a minute),
so the game can play its layers in step with the module (src/sfx.c): the trumpet counter-melody (3 ducklings
or more) and the glockenspiel (8 or more), on their own voices, from layers() below.

    make_music.py OUTDIR       -> OUTDIR/march.mod

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/duckparade/LICENSE.
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note, s8  # noqa: E402

KEY, SCALE = 2, hm.MAJOR                  # D major
SPEED, TEMPO = 8, 150                     # one tick per 60-Hz frame: a row = 8 frames
ORDERS = [0, 1, 0, 2]
TRUMPET_HZ, GLOCK_HZ = 294, 1175          # the layer samples' own pitch (D4, D6), synthesised in src/sfx.c
TRUMPET_MIDI, GLOCK_MIDI = 62, 86
# chords per bar (scale degrees of the root) and the fife's tune, 16 rows a bar: a degree, '.' holds, '-' stops
SONG = [
    # A: I IV V I
    ([0, 3, 4, 0], ["4 . 4 2 4 . 7 . 6 . 4 . 2 . . -", "3 . 3 5 7 . 5 . 3 . 5 . 3 . . -",
                    "4 . 6 . 8 . 6 . 4 . 6 . 4 . 1 .", "2 . 4 . 7 . . 4 0 . . . - . . ."]),
    # B: vi ii V I
    ([5, 1, 4, 0], ["5 . 7 . 9 . 7 5 4 . 5 . 7 . . -", "1 . 3 . 5 . 3 1 0 . 1 . 3 . . -",
                    "4 . 4 4 6 . 4 . 8 . 6 . 4 . 6 .", "7 . 4 . 2 . 4 . 7 . . . - . . ."]),
    # C, the trio: IV I IV V
    ([3, 0, 3, 4], ["3 . . 5 7 . . 5 3 . 2 . 3 . . -", "4 . . 2 0 . . 2 4 . 7 . 4 . . -",
                    "3 . 5 . 7 . 8 . 9 . 7 . 5 . . .", "4 . 6 . 8 . 6 . 7 . . . - . . ."]),
]


def deg(d, octave):
    return hm.deg(d, octave, KEY, SCALE)


def midi(d, octave):
    """A scale degree -> MIDI note (octave 0 = around D4)."""
    return TRUMPET_MIDI + 12 * octave + SCALE[d % 7] + 12 * (d // 7)


def fife():
    """A bright fife: a soft square with a little breath (16-sample loop: an octave above the lead)."""
    return [s8(34 * (1 if i < 7 else -1) + 10 * math.sin(2 * math.pi * i / 16)) for i in range(16)]


def tuba():
    """A round tuba: the house bass with a buzzy third harmonic (64-sample loop)."""
    return [s8(64 * math.sin(2 * math.pi * i / 64) + 16 * math.sin(6 * math.pi * i / 64) + 8 * math.sin(4 * math.pi * i / 64))
            for i in range(64)]


def samples():
    return [("fife", fife(), 26, 0, 16), ("tuba", tuba(), 38, 0, 64), ("kick", hm.kick(), 34, 0, 0),
            ("snare", hm.snare(9), 26, 0, 0), ("hat", hm.hat(11), 12, 0, 0)]


def pattern(pi):
    chords, bars = SONG[pi]
    rows = []
    for b in range(4):
        root = chords[b]
        toks = bars[b].split()
        for r in range(16):
            c = [cell(), cell(), cell(), cell()]
            t = toks[r]
            if t == "-":
                c[0] = cell(0, 0, 0xC, 0)
            elif t != ".":
                c[0] = cell(1, note(deg(int(t), 1)), 0xC, 40 if r % 4 == 0 else 32)
            if r % 4 == 0:                                     # oom-pah: root, fifth, root, fifth
                d = root + (4 if r % 8 else 0)
                c[1] = cell(2, note(deg(d, 0)), 0xC, 44 if r % 8 == 0 else 34)
            if r in (0, 8):
                c[2] = cell(3, note(deg(0, 1)))
            elif r in (4, 12):
                c[2] = cell(4, note(deg(0, 1)), 0xC, 40)
            elif b == 3 and r in (13, 14, 15):                  # the snare roll at the end of the phrase
                c[2] = cell(4, note(deg(0, 1)), 0xC, 18 + (r - 13) * 8)
            if r % 4 == 2:
                c[3] = cell(5, note(deg(0, 2)), 0xC, 30 if r % 8 == 6 else 22)
            rows.append(c)
    return rows


def layers():
    """The game's two layers, one entry per row of the song (pitch for the SDK: 0x1000 = the sample's own pitch,
    0 = no note; volume 0..127): the trumpet counter-melody and the glockenspiel."""
    tr, gl = [], []
    for pi in ORDERS:
        chords, _bars = SONG[pi]
        for b in range(4):
            root = chords[b]
            for r in range(16):
                t = g = (0, 0)
                if r == 0:
                    t = (midi(root + 2, -1), 78)                # the chord's third, held
                elif r == 8:
                    t = (midi(root + 4, -1), 70)                # the fifth
                elif b == 3 and r in (12, 14):
                    t = (midi(root + (4 if r == 12 else 7), -1), 84)   # a little fanfare into the next phrase
                if r % 4 == 2:
                    g = (midi(root + [0, 2, 4, 7][(r // 4 + b) % 4], 2), 58 if r % 8 == 2 else 46)
                tr.append(t)
                gl.append(g)
    def pitch(m, base):
        return 0 if m == 0 else int(round(4096 * 2 ** ((m - base) / 12.0)))
    return [[(pitch(m, TRUMPET_MIDI), v) for m, v in tr], [(pitch(m, GLOCK_MIDI), v) for m, v in gl]]


def build():
    pats = []
    for pi in range(len(SONG)):
        rows = pattern(pi)
        if pi == ORDERS[0]:
            rows[0][3] = cell(0, 0, 0xF, SPEED)                  # speed 8 (ticks per row)
            k = rows[0][2]
            rows[0][2] = bytes([k[0], k[1], (k[2] & 0xF0) | 0xF, TEMPO])   # 150 BPM: a tick per frame
        pats.append(b"".join(b"".join(r) for r in rows))
    return hm.build_mod("duck parade", samples(), pats, ORDERS)


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)
    with open(os.path.join(outdir, "march.mod"), "wb") as f:
        f.write(build())


if __name__ == "__main__":
    main()
