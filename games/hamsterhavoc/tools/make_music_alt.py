#!/usr/bin/env python3
"""Hamster Havoc alternative music: two 4-channel ProTracker MODs written with the house music kit
(games/common/tools/house_music.py: the MOD writer and periods; docs/art-direction.md "Audio"), the instruments
synthesised here, at the house mix (sample volumes <= 40). The owner picks; the game plays tune.mod (make_music.py).

  alt1  "Orbit Bossa": a lazy space bossa nova in 4/4, A minor with a C major turn, speed 7. Soft nylon-guitar
        chords on the bossa rhythm (3+3+2), a round bass on 1 and the "and" of 2, a rim click on the clave, and a
        breathy flute-like lead floating over it. A (Am7 Dm7 G7 Cmaj7), A', B (Fmaj7 Em7 Am7 Em7).
  alt2  "Cheese Rocket": a bouncy chiptune march in 4/4, G major, speed 5. A square lead in marching dotted
        rhythms, the house pluck in arpeggios, an octave bass, kick, snare and hats. A (I IV V I), A' (the lead an
        octave up), B (vi IV I V with a snare roll), then again.

    make_music_alt.py              -> ../music-alt/alt1.mod, alt2.mod (previews; the game is unchanged)
    make_music_alt.py OUTDIR N     -> OUTDIR/tune.mod: alternative N (1 or 2), for tools/build_assets.py

To switch, in tools/build_assets.py call make_music_alt.py with OUTDIR and N instead of make_music.py.

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/hamsterhavoc/LICENSE.
"""
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note, s8  # noqa: E402

_ = None


# ---- instruments (8-bit signed samples) -----------------------------------------------------------------------------
def nylon(n=3000):
    """A soft nylon-string pluck: a few decaying harmonics, the upper ones fading first (one-shot)."""
    out = []
    for i in range(n):
        t = i / 16.0
        v = math.sin(2 * math.pi * t) * math.exp(-i / 1300.0) + 0.45 * math.sin(4 * math.pi * t) * math.exp(-i / 500.0) \
            + 0.2 * math.sin(6 * math.pi * t) * math.exp(-i / 260.0)
        out.append(s8(110 * v / 1.5))
    return out


def flute(seed=3):
    """A breathy flute-like loop: a sine, a little second harmonic, a breath of noise (64-sample loop)."""
    rng = random.Random(seed)
    return [s8(46 * math.sin(2 * math.pi * i / 64) + 6 * math.sin(4 * math.pi * i / 64) + rng.uniform(-4, 4))
            for i in range(64)]


def rim(seed=5):
    rng = random.Random(seed)
    return [s8((rng.uniform(-70, 70) * 0.5 + 60 * math.sin(i / 1.6)) * math.exp(-i / 60.0)) for i in range(300)]


def soft_bass():
    return [s8(80 * math.sin(2 * math.pi * i / 64) + 10 * math.sin(4 * math.pi * i / 64)) for i in range(64)]


def rows_to_pattern(chans):
    return b"".join(b"".join(chans[k].get(r, cell()) for k in range(4)) for r in range(64))


# ---- alt1: Orbit Bossa ------------------------------------------------------------------------------------------------
def bossa():
    KEY, SC = 9, hm.MINOR                            # A minor (natural)
    NYL, FLU, BAS, RIM = 1, 2, 3, 4
    smp = [("nylon", nylon(), 34, 0, 0), ("flute", flute(), 22, 0, 64), ("bass", soft_bass(), 38, 0, 64),
           ("rim", rim(), 26, 0, 0)]

    def dg(d, o):
        return hm.deg(d, o, KEY, SC)
    # chords as scale-degree stacks (root, third, fifth, seventh), one bar each
    A = [0, 3, 6, 2]                                  # Am7 Dm7 G7 Cmaj7 (the third and seventh of each)
    B = [5, 4, 0, 4]                                  # Fmaj7 Em7 Am7 Em7
    TA = [[4, _, _, 2, _, _, 4, _], [3, _, 2, _, 0, _, _, _], [1, _, _, 3, _, _, 6, _], [4, _, _, _, 2, _, _, _]]
    TA2 = [[7, _, _, 6, _, _, 4, _], [5, _, 4, _, 3, _, _, _], [6, _, _, 4, _, _, 1, _], [2, _, _, _, 4, _, _, _]]
    TB = [[2, _, 4, _, 7, _, 6, _], [4, _, _, 3, _, _, 1, _], [2, _, 4, _, 6, 7, _, _], [6, _, 4, _, _, _, _, _]]

    def pat(tune, chords):
        ch = [{}, {}, {}, {}]
        for bar in range(4):
            c0, r0 = chords[bar], bar * 16
            for i, step in enumerate((0, 6, 12)):        # the bossa's 3+3+2: chord stabs
                ch[0][r0 + step] = cell(NYL, note(dg(c0 + 2, 1)), 0xC, 30 if i == 0 else 24)
            for step in (3, 10):                          # the higher chord tones answer
                ch[0][r0 + step] = cell(NYL, note(dg(c0 + 6, 1)), 0xC, 18)
            ch[1][r0] = cell(BAS, note(dg(c0, 0)), 0xC, 40)
            ch[1][r0 + 6] = cell(BAS, note(dg(c0 + 4, 0)), 0xC, 30)
            ch[1][r0 + 8] = cell(BAS, note(dg(c0 + 4, 0)), 0xC, 0)
            ch[1][r0 + 12] = cell(BAS, note(dg(c0, 0)), 0xC, 32)
            for step in (0, 6, 12, 4 if bar % 2 else 10):  # the clave on the rim
                ch[2][r0 + step] = cell(RIM, note(24), 0xC, 22 if step else 28)
            for j, d in enumerate(tune[bar]):
                if d is not None:
                    ch[3][r0 + j * 2] = cell(FLU, note(dg(d, 1)), 0xC, 34)
        return rows_to_pattern(ch)

    pats = [pat(TA, A), pat(TA2, A), pat(TB, B)]
    return hm.build_mod("hamsterhavoc bossa", smp, pats, [0, 1, 2, 1], speed=7)


# ---- alt2: Cheese Rocket ----------------------------------------------------------------------------------------------
def rocket():
    KEY = 7                                          # G
    SQ, PLK, BAS, KICK, SNR, HAT = 1, 2, 3, 4, 5, 6
    smp = [("square", hm.square_lead(0.25), 20, 0, 32), ("pluck", hm.drop(), 26, 0, 0), ("bass", soft_bass(), 36, 0, 64),
           ("kick", hm.kick(), 36, 0, 0), ("snare", hm.snare(), 26, 0, 0), ("hat", hm.hat(), 16, 0, 0)]

    def dg(d, o):
        return hm.deg(d, o, KEY, hm.MAJOR)
    # the march: a dotted rhythm per beat (rows 0 and 3 of each 4), a melody note per beat
    A = [[4, 4, 5, 7], [6, 5, 3, _], [4, 2, 1, 4], [0, _, 0, _]]
    B = [[5, 4, 2, 5], [3, 5, 7, 5], [4, 2, 0, 2], [1, 3, 4, _]]

    def pat(tune, chords, up=0, roll=False):
        ch = [{}, {}, {}, {}]
        for bar in range(4):
            c0, r0 = chords[bar], bar * 16
            for beat in range(4):
                d = tune[bar][beat]
                r = r0 + beat * 4
                if d is not None:
                    ch[0][r] = cell(SQ, note(dg(d, 1 + up)), 0xC, 32)
                    ch[0][r + 3] = cell(SQ, note(dg(d + 2, 1 + up)), 0xC, 22)
                ch[1][r] = cell(BAS, note(dg(c0, 0)), 0xC, 40)
                ch[1][r + 2] = cell(BAS, note(dg(c0, 1)), 0xC, 26)
                ch[2][r] = cell(KICK, note(12)) if beat % 2 == 0 else cell(SNR, note(24))
                ch[2][r + 2] = cell(HAT, note(30), 0xC, 22)
                if roll and bar == 3 and beat >= 2:
                    ch[2][r + 1] = cell(SNR, note(24), 0xC, 20)
                    ch[2][r + 3] = cell(SNR, note(24), 0xC, 28)
                ch[3][r + 1] = cell(PLK, note(dg(c0 + [0, 2, 4, 7][beat], 1)), 0xC, 24)
                ch[3][r + 3] = cell(PLK, note(dg(c0 + [4, 7, 2, 4][beat], 1)), 0xC, 18)
        return rows_to_pattern(ch)

    pats = [pat(A, [0, 3, 4, 0]), pat(A, [0, 3, 4, 0], up=1), pat(B, [5, 3, 0, 4], roll=True)]
    return hm.build_mod("hamsterhavoc rocket", smp, pats, [0, 1, 0, 2], speed=5)


ALTS = {1: bossa, 2: rocket}


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
        print("wrote %s" % os.path.normpath(os.path.join(out, "alt%d.mod" % n)))


if __name__ == "__main__":
    main()
