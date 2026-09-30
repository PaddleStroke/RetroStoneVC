#!/usr/bin/env python3
"""The 8BCraft house music kit: small 4-channel ProTracker MODs with synthesised instruments.

Extracted from Leady Squid's make_music.py (the reference). A game's make_music.py writes its tune with it;
the SDK plays it with libxmp-lite (rs_music_play), at the house music volume (house_audio.h HA_MUSIC_VOL).

  PERIODS, note(), deg()         ProTracker periods C-1..B-3, a scale degree -> semitone
  instruments(seed)              the house instrument set: pad, bass, drop (a pluck), lead (breathy)
  extra instruments              square_lead(), kick(), snare(), hat() for livelier games
  cell(), Song                   a pattern cell; a song (samples, patterns, order) -> MOD bytes

Conventions (docs/art-direction.md "Audio"): 4 channels (0 melody/arpeggio, 1 bass, 2 pad or drums, 3 lead),
speed 6-8 at 125 BPM (Leady Squid: speed 8, a calm loop; action games: speed 5-6), 64 rows per pattern,
a loop of 3-6 patterns, instrument volumes <= 40 (pad 22, bass 40, drop 34, lead 20), effect C for accents.

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import math
import random
import struct

# ProTracker periods, C-1 .. B-3
PERIODS = [856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
           428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
           214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113]

MAJOR = [0, 2, 4, 5, 7, 9, 11]
MINOR = [0, 2, 3, 5, 7, 8, 10]
DORIAN = [0, 2, 3, 5, 7, 9, 10]
MIXOLYDIAN = [0, 2, 4, 5, 7, 9, 10]
PENTATONIC = [0, 2, 4, 7, 9]


def s8(v):
    return max(-128, min(127, int(round(v))))


def note(n):
    """Semitone n (0 = C-1) -> ProTracker period, folded into the 3 octaves."""
    while n < 0:
        n += 12
    while n >= len(PERIODS):
        n -= 12
    return PERIODS[n]


def deg(d, octave, key=2, scale=DORIAN):
    """Scale degree d (may exceed the scale: the next octave) -> semitone."""
    return key + 12 * octave + scale[d % len(scale)] + 12 * (d // len(scale))


def cell(smp=0, period=0, eff=0, par=0):
    return bytes([(smp & 0xF0) | (period >> 8), period & 0xFF, ((smp & 0x0F) << 4) | eff, par])


EMPTY_ROW = cell() * 4


# ---- the house instrument set ------------------------------------------------------------------------------
def pad():
    """A soft sine pad (64-sample loop)."""
    return [s8(38 * math.sin(2 * math.pi * i / 64) + 12 * math.sin(4 * math.pi * i / 64 + 0.5)) for i in range(64)]


def bass():
    """A round bass (64-sample loop)."""
    return [s8(70 * math.sin(2 * math.pi * i / 64) + 14 * math.sin(4 * math.pi * i / 64)) for i in range(64)]


def drop():
    """The water-drop pluck: a sine with a small upward bend and a fast decay (one-shot)."""
    out, ph = [], 0.0
    for i in range(2400):
        ph += (1.0 / 16) * (1 + 0.25 * math.exp(-i / 120.0))
        out.append(s8(100 * math.sin(2 * math.pi * ph) * math.exp(-i / 520.0)))
    return out


def lead(rng):
    """A breathy sine lead (32-sample loop with a little noise)."""
    return [s8(44 * math.sin(2 * math.pi * i / 32) + rng.uniform(-3, 3)) for i in range(32)]


def square_lead(duty=0.25):
    """A soft pulse lead for brighter games (32-sample loop)."""
    return [s8(40 if (i / 32.0) < duty else -40) for i in range(32)]


def kick():
    out, ph = [], 0.0
    for i in range(1600):
        f = 1.0 / 20 * (1 + 3 * math.exp(-i / 90.0))
        ph += f
        out.append(s8(110 * math.sin(2 * math.pi * ph) * math.exp(-i / 500.0)))
    return out


def snare(seed=9):
    rng = random.Random(seed)
    return [s8((rng.uniform(-90, 90) * 0.7 + 40 * math.sin(i / 3.0)) * math.exp(-i / 380.0)) for i in range(1400)]


def hat(seed=11):
    rng = random.Random(seed)
    return [s8(rng.uniform(-60, 60) * math.exp(-i / 90.0)) for i in range(500)]


def instruments(seed=5):
    """The house set as (name, data, volume, loop start, loop length) rows: samples 1..4 of a song.
    Volumes are the house mix: the pad under everything, the bass the loudest."""
    rng = random.Random(seed)
    return [("pad", pad(), 22, 0, 64), ("bass", bass(), 40, 0, 64), ("drop", drop(), 34, 0, 0),
            ("lead", lead(rng), 20, 0, 32)]


# ---- the MOD writer ----------------------------------------------------------------------------------------
def build_mod(title, samples, patterns, orders, speed=None, tempo=None):
    """samples: (name, data, volume, loop start, loop length) rows (at most 31); patterns: 1024-byte patterns
    (64 rows x 4 cells); orders: pattern indices. speed/tempo: an Fxx on channel 4 of the first row of the
    first pattern played (speed < 32, tempo >= 32). Returns the MOD bytes."""
    out = bytearray()
    out += title.encode()[:20].ljust(20, b"\0")
    for i in range(31):
        if i < len(samples):
            n, data, vol, ls, ll = samples[i]
            if len(data) % 2:
                data = data + [0]
            out += n.encode()[:22].ljust(22, b"\0")
            out += struct.pack(">HBBHH", len(data) // 2, 0, vol, ls // 2, (ll // 2) if ll else 1)
        else:
            out += b"\0" * 22 + struct.pack(">HBBHH", 0, 0, 0, 0, 1)
    out += bytes([len(orders), 127])
    out += bytes(list(orders) + [0] * (128 - len(orders)))
    out += b"M.K."
    pats = [bytes(p) for p in patterns]
    first = orders[0]
    if speed is not None or tempo is not None:
        p0 = bytearray(pats[first])
        p0[12:16] = bytes([p0[12], p0[13], (p0[14] & 0xF0) | 0xF, speed if speed is not None else tempo])
        pats[first] = bytes(p0)
    for p in pats:
        out += p
    for n, data, vol, ls, ll in samples:
        if len(data) % 2:
            data = data + [0]
        out += bytes((v + 256) % 256 for v in data)
    return bytes(out)
