#!/usr/bin/env python3
"""Unicorn Hop alternative music: two 4-channel ProTracker MODs written with the house music kit
(games/common/tools/house_music.py: the MOD writer and periods; docs/art-direction.md "Audio"), the instruments
synthesised here, at the house mix (sample volumes <= 40). The owner picks; the game plays tune.mod (make_music.py).

  alt1  "Cotton Candy Waltz": a dreamy music-box waltz in 3/4, B flat major, speed 6. A music box (a bell with
        bright partials) sings the tune, a celesta answers on the 2nd and 3rd beats (oom-pa-pa), a pizzicato bass on
        the 1st, a soft pad holds the chords. A (I vi IV V twice), B (IV V iii vi: floating up), A' (home).
  alt2  "Rainbow Gallop": a bouncy chiptune gallop in 4/4, G major, speed 5. A square lead gallops (da-da-DUM), the
        house pluck sparkles in arpeggios, an eighth-note bass, kick, snare and hats. A (I V vi IV), A' (the lead up
        an octave), B (IV I V V with a snare roll), then again.

    make_music_alt.py              -> ../music-alt/alt1.mod, alt2.mod (previews; the game is unchanged)
    make_music_alt.py OUTDIR N     -> OUTDIR/tune.mod: alternative N (1 or 2), for tools/build_assets.py

To switch, in tools/build_assets.py call make_music_alt.py with OUTDIR and N instead of make_music.py.

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/unicornhop/LICENSE.
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note, s8  # noqa: E402

_ = None


# ---- instruments (8-bit signed samples) -----------------------------------------------------------------------------
def music_box(n=3600):
    """A music-box tine: a sine with two bright inharmonic partials, a fast attack, a long decay (one-shot)."""
    out = []
    for i in range(n):
        t = i / 16.0
        env = math.exp(-i / 1100.0)
        v = math.sin(2 * math.pi * t) + 0.35 * math.sin(2 * math.pi * t * 2.76) * math.exp(-i / 400.0) \
            + 0.18 * math.sin(2 * math.pi * t * 5.4) * math.exp(-i / 200.0)
        out.append(s8(124 * v * env / 1.3))
    return out


def celesta(n=2400):
    out = []
    for i in range(n):
        t = i / 16.0
        out.append(s8(100 * (math.sin(2 * math.pi * t) + 0.25 * math.sin(4 * math.pi * t)) * math.exp(-i / 900.0)))
    return out


def pizz(n=1800):
    """A plucked bass: a rounded triangle with a quick decay."""
    out = []
    for i in range(n):
        ph = (i / 32.0) % 1.0
        tri = 4 * ph - 1 if ph < 0.5 else 3 - 4 * ph
        out.append(s8(124 * tri * math.exp(-i / 600.0)))
    return out


def soft_pad():
    return [s8(90 * math.sin(2 * math.pi * i / 64) + 24 * math.sin(6 * math.pi * i / 64)) for i in range(64)]


def bass8():
    return [s8(60 * math.sin(2 * math.pi * i / 64) + 30 * (1 if i < 32 else -1) * 0.4) for i in range(64)]


# ---- a small score helper: notes as (row, pitch) per channel ---------------------------------------------------------
def deg(d, octave, key, scale=hm.MAJOR):
    return hm.deg(d, octave, key, scale)


def rows_to_pattern(chans, length=64):
    """chans: 4 dicts row -> cell bytes. A pattern shorter than 64 rows ends with a pattern break (D00)."""
    out = []
    for r in range(64):
        cs = [chans[k].get(r, cell()) for k in range(4)]
        if length < 64 and r == length - 1 and cs[3][2] & 0x0F == 0 and cs[3][3] == 0:
            b = bytearray(cs[3])
            b[2] = (b[2] & 0xF0) | 0xD
            b[3] = 0
            cs[3] = bytes(b)
        out.append(b"".join(cs))
    return b"".join(out)


# ---- alt1: Cotton Candy Waltz ---------------------------------------------------------------------------------------
def waltz():
    KEY = 10                                         # B flat
    BOX, CEL, PIZ, PAD = 1, 2, 3, 4
    smp = [("musicbox", music_box(), 40, 0, 0), ("celesta", celesta(), 32, 0, 0), ("pizz", pizz(), 40, 0, 0),
           ("pad", soft_pad(), 26, 0, 64)]
    A = [[(0, 4), (8, 5)], [(0, 4), (4, 2), (8, 0)], [(0, 3), (8, 5)], [(0, 4)]]
    A2 = [[(0, 7), (8, 6)], [(0, 5), (4, 4), (8, 2)], [(0, 3), (6, 4), (8, 5)], [(0, 4), (8, 1)]]
    B = [[(0, 5), (8, 7)], [(0, 6), (4, 4), (8, 2)], [(0, 4), (8, 6)], [(0, 5), (4, 3), (8, 1)]]
    END = [[(0, 4), (8, 5)], [(0, 4), (4, 2), (8, 0)], [(0, 3), (4, 5), (8, 4)], [(0, 0)]]

    def pat(tune, chords):
        ch = [{}, {}, {}, {}]
        for bar in range(4):
            c0, r0 = chords[bar], bar * 12
            for at, d in tune[bar]:
                ch[0][r0 + at] = cell(BOX, note(deg(d, 1, KEY)))
            ch[1][r0] = cell(PIZ, note(deg(c0, 0, KEY)))
            ch[2][r0 + 4] = cell(CEL, note(deg(c0 + 2, 1, KEY)))
            ch[2][r0 + 8] = cell(CEL, note(deg(c0 + 4, 1, KEY)), 0xC, 28)
            ch[3][r0] = cell(PAD, note(deg(c0, 1, KEY)))
        return rows_to_pattern(ch, 48)

    pats = [pat(A, [0, 5, 3, 4]), pat(A2, [0, 2, 3, 4]), pat(B, [3, 4, 2, 5]), pat(END, [0, 5, 3, 0])]
    return hm.build_mod("unicornhop waltz", smp, pats, [0, 1, 2, 3], speed=6)


# ---- alt2: Rainbow Gallop -------------------------------------------------------------------------------------------
def gallop():
    KEY = 7                                          # G
    SQ, PLK, BAS, KICK, SNR, HAT = 1, 2, 3, 4, 5, 6
    smp = [("square", hm.square_lead(0.25), 20, 0, 32), ("pluck", hm.drop(), 26, 0, 0), ("bass", bass8(), 36, 0, 64),
           ("kick", hm.kick(), 36, 0, 0), ("snare", hm.snare(), 26, 0, 0), ("hat", hm.hat(), 16, 0, 0)]
    # the gallop: da-da-DUM on rows 0, 1, 2 of each beat (4 rows), a melody note per beat
    A = [[0, 2, 4, 4], [4, 3, 1, _], [5, 4, 2, 0], [3, 2, 1, _]]
    B = [[3, 5, 7, 5], [4, 2, 0, 2], [4, 6, 8, 6], [4, 3, 1, _]]

    def pat(tune, chords, up=0, roll=False):
        ch = [{}, {}, {}, {}]
        for bar in range(4):
            c0, r0 = chords[bar], bar * 16
            for beat in range(4):
                d = tune[bar][beat]
                r = r0 + beat * 4
                if d is not None:
                    ch[0][r] = cell(SQ, note(deg(d, 1 + up, KEY)), 0xC, 30)
                    ch[0][r + 2] = cell(SQ, note(deg(d + 2, 1 + up, KEY)), 0xC, 36)
                ch[1][r] = cell(BAS, note(deg(c0, 0, KEY)), 0xC, 40)
                ch[1][r + 2] = cell(BAS, note(deg(c0 + (4 if beat % 2 else 0), 0, KEY)), 0xC, 28)
                ch[2][r] = cell(KICK, note(12)) if beat % 2 == 0 else cell(SNR, note(24))
                ch[2][r + 2] = cell(HAT, note(30), 0xC, 24)
                if roll and bar == 3 and beat >= 2:
                    ch[2][r + 1] = cell(SNR, note(24), 0xC, 20)
                    ch[2][r + 3] = cell(SNR, note(24), 0xC, 28)
                ch[3][r + 1] = cell(PLK, note(deg(c0 + [0, 2, 4, 7][beat], 1, KEY)), 0xC, 26)
                ch[3][r + 3] = cell(PLK, note(deg(c0 + [4, 7, 2, 4][beat], 1, KEY)), 0xC, 20)
        return rows_to_pattern(ch)

    pats = [pat(A, [0, 4, 5, 3]), pat(A, [0, 4, 5, 3], up=1), pat(B, [3, 0, 4, 4], roll=True)]
    return hm.build_mod("unicornhop gallop", smp, pats, [0, 1, 0, 2], speed=5)


ALTS = {1: waltz, 2: gallop}


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
